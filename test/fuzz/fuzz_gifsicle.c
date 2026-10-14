/* fuzz_gifsicle.c - Fuzz target that runs gifsicle with structured options.
   This file is part of gifsicle's fuzzing harness. See README.md.

   Each input is a CONFIG_SIZE-byte configuration followed by a GIF. The
   configuration decodes to gifsicle command-line options (see
   decode_options), and the target runs `gifsicle OPTIONS in.gif -o out.gif`
   in-process, checking the results (see runner.c). Unlike fuzz_main.c's
   free-form command lines, every configuration is a meaningful command,
   so this target reaches deep combinations of options quickly.

   Besides runner.c's checks, if every pixel of the input refers to a color
   in its colormap (or is transparent), every pixel of the output must,
   too. */

#include "runner.h"
#include "gifcheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_SIZE             16
#define MAX_OUTPUT_PIXELS       (1UL << 22)


/*****
 * Options
 **/

enum { CM_NONE, CM_WEB, CM_GRAY, CM_BW, CM_CUSTOM };

/* Options decoded from the configuration bytes. */
typedef struct {
    /* input and frame options */
    int unoptimize;
    int crop, crop_x, crop_y, crop_w, crop_h, crop_transparency;
    int flip_horizontal, flip_vertical, rotation;
    int interlace;              /* -1 means unset */
    int disposal;               /* -1 means unset */
    int delay;                  /* -1 means unset */
    int position, position_x, position_y;
    int twice;                  /* select all frames twice */
    int skip_first;             /* drop the first selected frame */
    /* output options */
    int optimize, keep_empty, conserve_memory, careful, lossy;
    int scaling;                /* 0 none, 1 --resize, 2 --scale,
                                   3 --resize-geometry */
    int resize_w, resize_h;
    double scale_x, scale_y;
    int geometry_down, geometry_up, geometry_min;
    int scale_method, scale_colors;
    int colormap;               /* CM_ value */
    int exact_colormap;
    int custom_size, custom_seed1, custom_seed2;
    int colors, color_method, dither;
    int gamma_type;             /* 0 sRGB, 1 numeric, 2 OKLab */
    double gamma;
    int logical_screen, screen_w, screen_h;
    int warnings;
} fuzz_options;

static const char* const dither_names[8] = {
    "none", "floyd-steinberg", "atkinson", "ro64", "o3", "o8",
    "diagonal", "halftone"
};
static const char* const method_names[7] = {
    "sample", "box", "mix", "catrom", "lanczos2", "lanczos3", "mitchell"
};
static const char* const color_method_names[3] = {
    "diversity", "blend-diversity", "median-cut"
};

/* Configuration byte layout (bit 0 is least significant):
   b[0]  0 --unoptimize, 1 --crop, 2 --crop-transparency,
         3 --flip-horizontal, 4 --flip-vertical, 5-6 rotation,
         7 --interlace or --no-interlace (from b[12] bit 0)
   b[1]  0-1 -O level, 2 --optimize=keep-empty, 3 --conserve-memory,
         4 --careful, 5 allow warnings, 7 --lossy=b[7]
   b[2]  0-1 scaling (none, --resize, --scale, --resize-geometry),
         2-4 --resize-method, 5 --resize-colors, 6-7 geometry '>' and '<'
   b[3], b[4]  resize width and height, or scale factors
   b[5]  0-2 colormap (none, --colors, web, gray, bw, custom,
         custom + --colors, web + --colors), 3-4 --color-method,
         5-7 --dither
   b[6]  --colors and --resize-colors count
   b[7]  lossiness
   b[8..11]  --crop X,Y+WxH (0 means "to the edge")
   b[12] 0 interlace value, 1-3 --disposal, 4 --delay=b[14],
         5 select all frames twice, 6 skip the first frame,
         7 --position=X,Y (from b[14], b[15])
   b[13] 0-1 --gamma, 2 --logical-screen (from b[14], b[15]),
         3 geometry '^', 4 use the exact colormap
   b[14], b[15]  values for delay, position, logical screen, gamma, and
         the custom palette */

static void decode_options(const uint8_t* b, fuzz_options* o) {
    memset(o, 0, sizeof(*o));
    o->unoptimize = b[0] & 1;
    o->crop = (b[0] >> 1) & 1;
    o->crop_x = b[8] % 32;
    o->crop_y = b[9] % 32;
    o->crop_w = b[10] % 64;
    o->crop_h = b[11] % 64;
    o->crop_transparency = (b[0] >> 2) & 1;
    o->flip_horizontal = (b[0] >> 3) & 1;
    o->flip_vertical = (b[0] >> 4) & 1;
    o->rotation = (b[0] >> 5) & 3;
    o->interlace = (b[0] & 0x80) ? (b[12] & 1) : -1;
    o->disposal = (b[12] & 0x02) ? (b[12] >> 2) & 3 : -1;
    o->delay = (b[12] & 0x10) ? b[14] : -1;
    o->twice = (b[12] >> 5) & 1;
    o->skip_first = (b[12] >> 6) & 1;
    o->position = (b[12] >> 7) & 1;
    o->position_x = b[14] % 64;
    o->position_y = b[15] % 64;

    o->optimize = b[1] & 3;
    o->keep_empty = (b[1] >> 2) & 1;
    o->conserve_memory = (b[1] >> 3) & 1;
    o->careful = (b[1] >> 4) & 1;
    o->warnings = (b[1] >> 5) & 1;
    o->lossy = (b[1] & 0x80) ? b[7] : 0;

    o->scaling = b[2] & 3;
    o->resize_w = b[3];
    o->resize_h = b[4];
    if ((o->scaling == 1 || o->scaling == 3) && !o->resize_w && !o->resize_h)
        o->scaling = 0;         /* gifsicle rejects 0x0 */
    o->scale_x = (b[3] + 1) / 64.0;
    o->scale_y = (b[4] + 1) / 64.0;
    o->geometry_down = (b[2] >> 6) & 1;
    o->geometry_up = (b[2] >> 7) & 1;
    o->geometry_min = (b[13] >> 3) & 1;
    o->scale_method = ((b[2] >> 2) & 7) % 7;
    o->scale_colors = (b[2] & 0x20) ? 2 + b[6] % 255 : 0;

    switch (b[5] & 7) {
    case 0: o->colormap = CM_NONE; break;
    case 1: o->colormap = CM_NONE; o->colors = 1; break;
    case 2: o->colormap = CM_WEB; break;
    case 3: o->colormap = CM_GRAY; break;
    case 4: o->colormap = CM_BW; break;
    case 5: o->colormap = CM_CUSTOM; break;
    case 6: o->colormap = CM_CUSTOM; o->colors = 1; break;
    case 7: o->colormap = CM_WEB; o->colors = 1; break;
    }
    if (o->colors)
        o->colors = 2 + b[6] % 255;
    o->exact_colormap = (b[13] >> 4) & 1;
    o->custom_size = 1 + b[15];
    o->custom_seed1 = b[14];
    o->custom_seed2 = b[15];
    o->color_method = ((b[5] >> 3) & 3) % 3;
    o->dither = b[5] >> 5;

    o->gamma_type = (b[13] & 3) % 3;
    o->gamma = 0.5 + (b[15] % 32) / 8.0;
    o->logical_screen = (b[13] >> 2) & 1;
    o->screen_w = 1 + b[14] % 128;
    o->screen_h = 1 + b[15] % 128;
}

/* The custom palette, as a gifsicle colormap file: arbitrary colors with
   some duplicates. */
static char* custom_palette(const fuzz_options* o, size_t* len) {
    size_t cap = (size_t) o->custom_size * 16 + 1;
    char* buf = (char*) malloc(cap);
    int i, r = 0, g = 0, b = 0, r0 = 0, g0 = 0, b0 = 0;
    *len = 0;
    for (i = 0; i < o->custom_size; ++i) {
        if (i > 0 && i % 7 == 3)
            r = r0, g = g0, b = b0;
        else {
            r = (i * 53 + o->custom_seed1 * 7) & 255;
            g = (i * 101 + o->custom_seed2 * 13) & 255;
            b = (i * 29) & 255;
        }
        if (i == 0)
            r0 = r, g0 = g, b0 = b;
        *len += snprintf(buf + *len, cap - *len, "%d %d %d\n", r, g, b);
    }
    return buf;
}

/* Build the command for options `o` and input GIF `gif`. */
static void build_command(const fuzz_options* o, const uint8_t* gif, size_t size,
                          run_spec* spec) {
    spec_init(spec);
    if (!o->warnings)
        spec_arg(spec, "--no-warnings");
    if (o->unoptimize)
        spec_arg(spec, "--unoptimize");
    if (o->crop)
        spec_arg(spec, "--crop=%d,%d+%dx%d", o->crop_x, o->crop_y, o->crop_w, o->crop_h);
    if (o->crop_transparency)
        spec_arg(spec, "--crop-transparency");
    if (o->flip_horizontal)
        spec_arg(spec, "--flip-horizontal");
    if (o->flip_vertical)
        spec_arg(spec, "--flip-vertical");
    if (o->rotation)
        spec_arg(spec, "--rotate-%d", 90 * o->rotation);
    if (o->interlace >= 0)
        spec_arg(spec, o->interlace ? "--interlace" : "--no-interlace");
    if (o->disposal >= 0)
        spec_arg(spec, "--disposal=%d", o->disposal);
    if (o->delay >= 0)
        spec_arg(spec, "--delay=%d", o->delay);
    if (o->position)
        spec_arg(spec, "--position=%d,%d", o->position_x, o->position_y);

    if (o->optimize)
        spec_arg(spec, "-O%d", o->optimize);
    if (o->keep_empty)
        spec_arg(spec, "--optimize=keep-empty");
    if (o->conserve_memory)
        spec_arg(spec, "--conserve-memory");
    if (o->careful)
        spec_arg(spec, "--careful");
    if (o->lossy)
        spec_arg(spec, "--lossy=%d", o->lossy);
    if (o->scaling == 1 || o->scaling == 3) {
        char w[16], h[16];
        snprintf(w, sizeof(w), o->resize_w ? "%d" : "_", o->resize_w);
        snprintf(h, sizeof(h), o->resize_h ? "%d" : "_", o->resize_h);
        if (o->scaling == 1)
            spec_arg(spec, "--resize=%sx%s", w, h);
        else
            spec_arg(spec, "--resize-geometry=%sx%s%s%s%s", w, h,
                     o->geometry_down ? ">" : "", o->geometry_up ? "<" : "",
                     o->geometry_min ? "^" : "");
    } else if (o->scaling == 2)
        spec_arg(spec, "--scale=%.17gx%.17g", o->scale_x, o->scale_y);
    if (o->scaling)
        spec_arg(spec, "--resize-method=%s", method_names[o->scale_method]);
    if (o->scale_colors)
        spec_arg(spec, "--resize-colors=%d", o->scale_colors);
    if (o->colormap != CM_NONE)
        spec_arg(spec, "--use-%scolormap=%s", o->exact_colormap ? "exact-" : "",
                 o->colormap == CM_WEB ? "web" : o->colormap == CM_GRAY ? "gray"
                 : o->colormap == CM_BW ? "bw" : "palette.txt");
    if (o->colors) {
        spec_arg(spec, "--colors=%d", o->colors);
        spec_arg(spec, "--color-method=%s", color_method_names[o->color_method]);
    }
    if (o->colors || o->colormap != CM_NONE)
        spec_arg(spec, "--dither=%s", dither_names[o->dither]);
    if (o->gamma_type == 2)
        spec_arg(spec, "--gamma=oklab");
    else if (o->gamma_type == 1)
        spec_arg(spec, "--gamma=%.17g", o->gamma);
    if (o->logical_screen)
        spec_arg(spec, "--logical-screen=%dx%d", o->screen_w, o->screen_h);

    spec_arg(spec, "in.gif");
    if (o->twice) {
        spec_arg(spec, "#%d-", o->skip_first);
        spec_arg(spec, "#0-");
    } else if (o->skip_first)
        spec_arg(spec, "#1-");
    spec_arg(spec, "-o");
    spec_arg(spec, "out.gif");

    spec_file(spec, "in.gif", gif, size);
    if (o->colormap == CM_CUSTOM) {
        size_t len;
        char* palette = custom_palette(o, &len);
        spec_file(spec, "palette.txt", palette, len);
        free(palette);
    }
}


/*****
 * Colormap check
 **/

/* Return 1 if `data` is a strictly valid GIF in which every image has a
   colormap and every pixel is transparent or refers to a color in it. */
static int pixels_in_colormaps(const char* data, size_t len, int* bad_image) {
    gc_file gf;
    int i, ok = 1;
    size_t k;
    if (gc_parse((const uint8_t*) data, len, &gf)) {
        gc_free(&gf);
        return 0;
    }
    for (i = 0; ok && i < gf.nimages; ++i) {
        const gc_image* gi = &gf.images[i];
        int ncol = gi->local_colormap ? gi->local_colormap_size : gf.global_colormap_size;
        for (k = 0; k < (size_t) gi->width * gi->height; ++k)
            if (gi->pixels[k] >= ncol && gi->pixels[k] != gi->transparent) {
                ok = 0;
                *bad_image = i;
                break;
            }
    }
    gc_free(&gf);
    return ok;
}

static void check_colormaps(const run_spec* spec, const snapshot* snap) {
    int i, bad = -1;
    if (!pixels_in_colormaps((const char*) spec->data[0], spec->len[0], &bad))
        return;                 /* the input doesn't promise anything */
    for (i = 0; i < snap->nfiles; ++i) {
        const dir_entry* e = &snap->files[i];
        if (strcmp(e->name, "out.gif") == 0
            && !pixels_in_colormaps(e->data, e->len, &bad))
            fuzz_failed("out.gif image #%d has pixels outside its colormap", bad);
    }
}


/*****
 * libFuzzer interface
 **/

int LLVMFuzzerInitialize(int* argc, char*** argv) {
    fuzz_initialize(argc, argv, "gifsicle-fuzz");
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    fuzz_options o;
    run_spec spec;
    const uint8_t* gif = data + CONFIG_SIZE;
    size_t gif_size = size - CONFIG_SIZE;
    unsigned long sw, sh;
    if (size < CONFIG_SIZE || !fuzz_within_budget(gif, gif_size))
        return -1;
    decode_options(data, &o);
    /* --scale can make huge outputs */
    if (o.scaling == 2 && fuzz_screen_size(gif, gif_size, &sw, &sh)
        && sw * o.scale_x * sh * o.scale_y > MAX_OUTPUT_PIXELS)
        return -1;
    build_command(&o, gif, gif_size, &spec);
    fuzz_run(&spec, fuzz_hash(data, size), check_colormaps);
    spec_free(&spec);
    return 0;
}
