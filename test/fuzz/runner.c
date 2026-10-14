/* runner.c - Run gifsicle command lines in-process and check the results.
   This file is part of gifsicle's fuzzing harness. See runner.h and
   README.md.

   fuzz_run() writes a command's input files into a private scratch
   directory and runs gifsicle_main() there. inprocess.h, included into
   gifsicle's sources, gives gifsicle an empty stdin, captures its stdout
   and stderr, turns exit() (only called when out of memory) into a
   longjmp, and stops --transform-colormap from running commands. Besides
   sanitizer errors, a run fails if:

   * gifsicle_main returns a status other than 0 or 1;
   * gifsicle writes a GIF (to a file or to stdout) that gifcheck.c, a
     strict decoder, rejects; that gifsicle's own reader decodes
     differently; or that gifsicle can't reread without complaint;
   * adding --careful or --conserve-memory to the command (unless it is
     lossy) changes its status or how its output GIFs look;
   * the command resizes, and adding -j2 (threads) changes its outputs;
   * gifsicle, given one of its own output GIFs and options that preserve
     appearance (-O, -U, --careful, ...), fails or writes a GIF that looks
     different;
   * with GIFSICLE_FUZZ_TWICE set, running the same command again gives a
     different status, output, or message, which means gifsicle_reset()
     missed some state.

   Appearance is judged by compositing frames as browsers do, in this file,
   not by gifsicle.

   Environment:
     GIFSICLE_FUZZ_VERBOSE=1    print each run's command and messages
     GIFSICLE_FUZZ_TWICE=1      run each command twice and compare
     GIFSICLE_FUZZ_REPRO=DIR    write each command's input files and a cmd.sh
                                that reruns it with the gifsicle binary */

#include <config.h>
#include "runner.h"
#include "gifcheck.h"
#include <lcdfgif/gif.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int gifsicle_main(int argc, char* argv[]);
void gifsicle_reset(void);

/* Resource budget for GIFs given to gifsicle */
#define MAX_IMAGES              64
#define MAX_FRAME_PIXELS        (1UL << 20)     /* also screens */
#define MAX_TOTAL_PIXELS        (1UL << 22)

static const char* prog = "gifsicle-fuzz";


/*****
 * Replacements for gifsicle's streams, exit(), and popen() (see
 * inprocess.h)
 **/

FILE* gifsicle_fuzz_stdin;
FILE* gifsicle_fuzz_stdout;
FILE* gifsicle_fuzz_stderr;
static jmp_buf fatal_jmp;
static int fatal_armed;

void gifsicle_fuzz_exit(int status) {
    if (fatal_armed)
        longjmp(fatal_jmp, 1);
    fprintf(stderr, "%s: gifsicle called exit(%d)\n", prog, status);
    abort();
}

FILE* gifsicle_fuzz_popen(const char* command, const char* mode) {
    (void) command, (void) mode;
    errno = EPERM;
    return NULL;
}


/*****
 * Commands
 **/

void spec_init(run_spec* spec) {
    memset(spec, 0, sizeof(*spec));
    spec->argv[spec->argc++] = strdup("gifsicle");
}

void spec_arg(run_spec* spec, const char* format, ...) {
    char buf[4096];
    va_list val;
    if (spec->argc >= RUN_MAX_ARGS)
        abort();
    va_start(val, format);
    vsnprintf(buf, sizeof(buf), format, val);
    va_end(val);
    spec->argv[spec->argc++] = strdup(buf);
}

void spec_file(run_spec* spec, const char* name, const void* data, size_t len) {
    if (spec->nfiles >= RUN_MAX_FILES)
        abort();
    spec->name[spec->nfiles] = strdup(name);
    spec->data[spec->nfiles] = (uint8_t*) malloc(len ? len : 1);
    memcpy(spec->data[spec->nfiles], data, len);
    spec->len[spec->nfiles] = len;
    ++spec->nfiles;
}

void spec_free(run_spec* spec) {
    int i;
    for (i = 0; i < spec->argc; ++i)
        free(spec->argv[i]);
    for (i = 0; i < spec->nfiles; ++i) {
        free(spec->name[i]);
        free(spec->data[i]);
    }
    memset(spec, 0, sizeof(*spec));
}

/* Initialize `dst` as `src` with `extra` inserted after argv[0]. */
static void spec_derive(run_spec* dst, const run_spec* src,
                        const char* const* extra, int nextra) {
    int i;
    spec_init(dst);
    for (i = 0; i < nextra; ++i)
        spec_arg(dst, "%s", extra[i]);
    for (i = 1; i < src->argc; ++i)
        spec_arg(dst, "%s", src->argv[i]);
    for (i = 0; i < src->nfiles; ++i)
        spec_file(dst, src->name[i], src->data[i], src->len[i]);
}

static void spec_print(FILE* f, const run_spec* spec) {
    int i;
    const char* s;
    for (i = 0; i < spec->argc; ++i) {
        fputc(i ? ' ' : '\n', f);
        if (strspn(spec->argv[i], "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                   "0123456789-_=.,+:%@") == strlen(spec->argv[i])
            && spec->argv[i][0])
            fputs(spec->argv[i], f);
        else {
            fputc('\'', f);
            for (s = spec->argv[i]; *s; ++s)
                if (*s == '\'')
                    fputs("'\\''", f);
                else
                    fputc(*s, f);
            fputc('\'', f);
        }
    }
    fputc('\n', f);
}

/* Return 1 if some argument starts with one of `prefixes` (which catches
   unambiguous abbreviations, too). */
static int spec_has_arg(const run_spec* spec, const char* const* prefixes) {
    int i, j;
    for (i = 1; i < spec->argc; ++i)
        for (j = 0; prefixes[j]; ++j)
            if (strncmp(spec->argv[i], prefixes[j], strlen(prefixes[j])) == 0)
                return 1;
    return 0;
}


/*****
 * Budget
 **/

uint64_t fuzz_hash(const uint8_t* data, size_t len) {
    uint64_t h = 14695981039346656037ULL;
    size_t i;
    for (i = 0; i < len; ++i)
        h = (h ^ data[i]) * 1099511628211ULL;
    return h;
}

int fuzz_is_gif(const char* data, size_t len) {
    return len >= 3 && memcmp(data, "GIF", 3) == 0;
}

/* Bytes past the end read as 0, as in gifsicle's reader. */
static unsigned byte_at(const uint8_t* p, size_t n, size_t i) {
    return i < n ? p[i] : 0;
}

static unsigned u16(const uint8_t* p, size_t n, size_t i) {
    return byte_at(p, n, i) | (byte_at(p, n, i + 1) << 8);
}

/* What scan_gif learns about one GIF. */
typedef struct {
    unsigned long sw, sh;       /* screen, grown to fit the first image as
                                   Gif_CalculateScreenSize does */
    unsigned long max_frame;    /* largest image, in pixels */
    unsigned long total;        /* total image pixels */
    int nimages;
} gif_scan;

/* Scan the GIF at the start of `p` as gifsicle's lenient reader would: skip
   up to 20 unknown block bytes, and stop at anything else. Returns the
   length through the trailer, or 0 if there is no trailer. */
static size_t scan_gif(const uint8_t* p, size_t n, gif_scan* s) {
    size_t pos = 13;
    int unknown = 0;
    memset(s, 0, sizeof(*s));
    if (n < 13 || memcmp(p, "GIF", 3) != 0)
        return 0;
    s->sw = u16(p, n, 6);
    s->sh = u16(p, n, 8);
    if (p[10] & 0x80)
        pos += 3 * (2 << (p[10] & 7));
    while (pos < n && p[pos] != ';') {
        if (p[pos] == ',') {
            unsigned long w = u16(p, n, pos + 5), h = u16(p, n, pos + 7);
            if (s->nimages == 0) {
                unsigned long r = u16(p, n, pos + 1) + w, b = u16(p, n, pos + 3) + h;
                s->sw = r > s->sw ? r : s->sw;
                s->sh = b > s->sh ? b : s->sh;
            }
            s->max_frame = w * h > s->max_frame ? w * h : s->max_frame;
            s->total += w * h;
            ++s->nimages;
            if (byte_at(p, n, pos + 9) & 0x80)
                pos += 3 * (2 << (p[pos+9] & 7));
            pos += 11;                  /* descriptor and min code size */
        } else if (p[pos] == '!')
            pos += 2;
        else if (++unknown > 20)
            return 0;
        else {
            ++pos;
            continue;
        }
        while (pos < n && p[pos])       /* data sub-blocks */
            pos += 1 + p[pos];
        ++pos;
    }
    return pos < n ? pos + 1 : 0;
}

int fuzz_screen_size(const uint8_t* p, size_t n, unsigned long* w, unsigned long* h) {
    gif_scan s;
    if (n < 13 || memcmp(p, "GIF", 3) != 0)
        return 0;
    scan_gif(p, n, &s);
    *w = s.sw;
    *h = s.sh;
    return 1;
}

/* Also handles concatenated GIFs, for --multifile and --nextfile. */
int fuzz_within_budget(const uint8_t* p, size_t n) {
    unsigned long total = 0;
    int nimages = 0;
    while (n >= 13 && memcmp(p, "GIF", 3) == 0) {
        gif_scan s;
        size_t len = scan_gif(p, n, &s);
        if (s.sw * s.sh > MAX_FRAME_PIXELS || s.max_frame > MAX_FRAME_PIXELS
            || (total += s.total) > MAX_TOTAL_PIXELS
            || (nimages += s.nimages) > MAX_IMAGES)
            return 0;
        if (!len)
            break;
        p += len;
        n -= len;
    }
    return 1;
}


/*****
 * Scratch directory
 **/

static char workdir[PATH_MAX];
static int home_fd = -1;        /* the directory to return to */
static int verbose, twice;
static const char* repro_dir;

static void clean_workdir(void) {
    DIR* d = opendir(".");
    struct dirent* de;
    if (!d)
        return;
    while ((de = readdir(d)))
        if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
            unlink(de->d_name);
    closedir(d);
}

static void remove_workdir(void) {
    if (workdir[0] && chdir(workdir) == 0) {
        clean_workdir();
        if (fchdir(home_fd) == 0)
            rmdir(workdir);
    }
}

static void write_file(const char* name, const uint8_t* data, size_t len) {
    FILE* f = fopen(name, "wb");
    if (!f || fwrite(data, 1, len, f) != len) {
        fprintf(stderr, "%s: %s: %s\n", prog, name, strerror(errno));
        abort();
    }
    fclose(f);
}

static char* read_file(const char* name, size_t* len) {
    FILE* f = fopen(name, "rb");
    char* buf = NULL;
    size_t cap = 0, n;
    *len = 0;
    if (!f)
        return NULL;
    do {
        if (*len == cap) {
            cap = cap ? cap * 2 : 4096;
            buf = (char*) realloc(buf, cap);
        }
        n = fread(buf + *len, 1, cap - *len, f);
        *len += n;
    } while (n > 0);
    fclose(f);
    return buf;
}

static int compare_entries(const void* a, const void* b) {
    return strcmp(((const dir_entry*) a)->name, ((const dir_entry*) b)->name);
}

static void read_workdir(snapshot* snap) {
    DIR* d = opendir(".");
    struct dirent* de;
    int cap = 0;
    snap->files = NULL;
    snap->nfiles = 0;
    if (!d)
        return;
    while ((de = readdir(d))) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (snap->nfiles == cap) {
            cap = cap ? cap * 2 : 8;
            snap->files = (dir_entry*) realloc(snap->files, cap * sizeof(dir_entry));
        }
        snap->files[snap->nfiles].name = strdup(de->d_name);
        snap->files[snap->nfiles].data =
            read_file(de->d_name, &snap->files[snap->nfiles].len);
        ++snap->nfiles;
    }
    closedir(d);
    qsort(snap->files, snap->nfiles, sizeof(dir_entry), compare_entries);
}

static void free_snapshot(snapshot* snap) {
    int i;
    free(snap->out);
    free(snap->err);
    for (i = 0; i < snap->nfiles; ++i) {
        free(snap->files[i].name);
        free(snap->files[i].data);
    }
    free(snap->files);
}

int fuzz_unchanged_input(const run_spec* spec, const dir_entry* e) {
    int i;
    for (i = 0; i < spec->nfiles; ++i)
        if (strcmp(e->name, spec->name[i]) == 0)
            return e->len == spec->len[i]
                && (e->len == 0 || memcmp(e->data, spec->data[i], e->len) == 0);
    return 0;
}

static const dir_entry* find_entry(const snapshot* snap, const char* name) {
    int i;
    for (i = 0; i < snap->nfiles; ++i)
        if (strcmp(snap->files[i].name, name) == 0)
            return &snap->files[i];
    return NULL;
}


/*****
 * Running gifsicle
 **/

static void run_once(const run_spec* spec, snapshot* snap) {
    volatile int status = 0;
    char* argv[RUN_MAX_ARGS + 1];
    int i;

    if (chdir(workdir) != 0) {
        fprintf(stderr, "%s: %s: %s\n", prog, workdir, strerror(errno));
        abort();
    }
    clean_workdir();
    for (i = 0; i < spec->nfiles; ++i)
        write_file(spec->name[i], spec->data[i], spec->len[i]);

    /* gifsicle_main might modify its arguments */
    for (i = 0; i < spec->argc; ++i)
        argv[i] = strdup(spec->argv[i]);
    argv[spec->argc] = NULL;
    snap->out = snap->err = NULL;
    gifsicle_fuzz_stdout = open_memstream(&snap->out, &snap->out_len);
    gifsicle_fuzz_stderr = open_memstream(&snap->err, &snap->err_len);
    clearerr(gifsicle_fuzz_stdin);

    if (setjmp(fatal_jmp)) {
        /* exit() from gifsicle, after running out of memory */
        status = -1;
        gifsicle_reset();
    } else {
        fatal_armed = 1;
        status = gifsicle_main(spec->argc, argv);
    }
    fatal_armed = 0;

    fclose(gifsicle_fuzz_stdout);
    fclose(gifsicle_fuzz_stderr);
    gifsicle_fuzz_stdout = gifsicle_fuzz_stderr = NULL;
    for (i = 0; i < spec->argc; ++i)
        free(argv[i]);
    snap->status = status;
    read_workdir(snap);
    clean_workdir();
    if (fchdir(home_fd) != 0)
        abort();

    if (verbose) {
        fprintf(stderr, "%s: status %d:", prog, snap->status);
        spec_print(stderr, spec);
        fprintf(stderr, "%.*s", (int) snap->err_len, snap->err ? snap->err : "");
    }
}

static void write_repro(const run_spec* spec) {
    char path[PATH_MAX];
    FILE* f;
    int i;
    mkdir(repro_dir, 0777);
    for (i = 0; i < spec->nfiles; ++i) {
        snprintf(path, sizeof(path), "%s/%s", repro_dir, spec->name[i]);
        write_file(path, spec->data[i], spec->len[i]);
    }
    snprintf(path, sizeof(path), "%s/cmd.sh", repro_dir);
    if ((f = fopen(path, "w"))) {
        fprintf(f, "#!/bin/sh\n# Run with gifsicle in PATH.\ncd \"$(dirname \"$0\")\" &&");
        spec_print(f, spec);
        fclose(f);
        chmod(path, 0755);
    }
    fprintf(stderr, "%s: wrote %s; command:", prog, path);
    spec_print(stderr, spec);
}


/*****
 * Checks
 **/

static const run_spec* cur_spec;        /* for failure reports */
static const run_spec* cur_check_spec;
static const snapshot* cur_snapshot;

void fuzz_failed(const char* format, ...) {
    va_list val;
    fprintf(stderr, "%s: CHECK FAILED: ", prog);
    va_start(val, format);
    vfprintf(stderr, format, val);
    va_end(val);
    fputc('\n', stderr);
    if (cur_spec) {
        fprintf(stderr, "command:");
        spec_print(stderr, cur_spec);
    }
    if (cur_check_spec) {
        fprintf(stderr, "checking command:");
        spec_print(stderr, cur_check_spec);
    }
    if (cur_snapshot && cur_snapshot->err_len)
        fprintf(stderr, "gifsicle's messages:\n%.*s", (int) cur_snapshot->err_len,
                cur_snapshot->err);
    abort();
}

static int reread_errors;

static void count_reread_errors(Gif_Stream* gfs, Gif_Image* gfi,
                                int is_error, const char* message) {
    (void) gfs, (void) gfi, (void) is_error;
    if (message)
        ++reread_errors;
}

/* A GIF gifsicle wrote must parse strictly, and gifsicle's reader must
   decode it the same way without complaint. */
static void check_gif(const char* name, const char* data, size_t len) {
    gc_file gf;
    const char* problem;
    Gif_Record rec;
    Gif_Stream* gfs;
    int i, y;
    if (!fuzz_is_gif(data, len))
        return;                 /* such as --info output */
    if ((problem = gc_parse((const uint8_t*) data, len, &gf)))
        fuzz_failed("gifsicle wrote a bad GIF to %s: %s", name, problem);

    rec.data = (const unsigned char*) data;
    rec.length = (uint32_t) len;
    reread_errors = 0;
    gfs = Gif_FullReadRecord(&rec, GIF_READ_UNCOMPRESSED, "output",
                             count_reread_errors);
    if (!gfs || reread_errors)
        fuzz_failed("gifsicle complains reading its %s", name);
    if (gfs->nimages != gf.nimages)
        fuzz_failed("%s: gifsicle reads %d images, gifcheck %d",
                    name, gfs->nimages, gf.nimages);
    for (i = 0; i < gf.nimages; ++i) {
        Gif_Image* gfi = gfs->images[i];
        gc_image* gi = &gf.images[i];
        if (gfi->left != gi->left || gfi->top != gi->top
            || gfi->width != gi->width || gfi->height != gi->height
            || gfi->transparent != gi->transparent)
            fuzz_failed("%s image #%d: gifsicle and gifcheck read different headers",
                        name, i);
        for (y = 0; y < gi->height; ++y)
            if (memcmp(gfi->img[y], gi->pixels + (size_t) y * gi->width, gi->width) != 0)
                fuzz_failed("%s image #%d (%dx%d%s): gifsicle and gifcheck read different pixels",
                            name, i, gi->width, gi->height,
                            gi->interlace ? ", interlaced" : "");
    }
    Gif_DeleteStream(gfs);
    gc_free(&gf);
}

static void check_snapshot(const run_spec* spec, const snapshot* snap) {
    int i;
    if (snap->status != 0 && snap->status != 1 && snap->status != -1)
        fuzz_failed("gifsicle_main returned %d", snap->status);
    check_gif("<stdout>", snap->out, snap->out_len);
    for (i = 0; i < snap->nfiles; ++i)
        if (!fuzz_unchanged_input(spec, &snap->files[i]))
            check_gif(snap->files[i].name, snap->files[i].data, snap->files[i].len);
}

static int same_bytes(const char* a, size_t alen, const char* b, size_t blen) {
    return alen == blen && (alen == 0 || memcmp(a, b, alen) == 0);
}

/* Fail if two runs' results differ. `what` describes the second run. */
static void check_same(const snapshot* a, const snapshot* b, const char* what,
                       int compare_messages) {
    int i;
    if (a->status != b->status)
        fuzz_failed("%s returned %d, not %d", what, b->status, a->status);
    if (!same_bytes(a->out, a->out_len, b->out, b->out_len))
        fuzz_failed("%s's stdout differs", what);
    if (compare_messages && !same_bytes(a->err, a->err_len, b->err, b->err_len))
        fuzz_failed("%s's messages differ:\n%.*s", what, (int) b->err_len, b->err);
    if (a->nfiles != b->nfiles)
        fuzz_failed("%s left %d files, not %d", what, b->nfiles, a->nfiles);
    for (i = 0; i < a->nfiles; ++i)
        if (strcmp(a->files[i].name, b->files[i].name) != 0
            || !same_bytes(a->files[i].data, a->files[i].len,
                           b->files[i].data, b->files[i].len))
            fuzz_failed("%s's %s differs", what, b->files[i].name);
}


/*****
 * Appearance
 **/

/* A composited frame, as a hash of its pixels, and how long it shows. */
typedef struct {
    uint64_t hash;
    unsigned long delay;
} shown_frame;

typedef struct {
    int width, height, loopcount;
    int nframes;
    shown_frame* frames;
} appearance;

/* Composite `gf`'s frames as a browser would: the background is
   transparent, and disposal 2 restores it. Consecutive identical frames
   merge, as in `gifdiff -w`. */
static void composite(const gc_file* gf, appearance* ap) {
    size_t n = (size_t) gf->screen_width * gf->screen_height, k;
    uint32_t* canvas = (uint32_t*) calloc(n ? n : 1, sizeof(uint32_t));
    uint32_t* saved = (uint32_t*) calloc(n ? n : 1, sizeof(uint32_t));
    int i, x, y;
    ap->width = gf->screen_width;
    ap->height = gf->screen_height;
    ap->loopcount = gf->loopcount;
    ap->nframes = 0;
    ap->frames = (shown_frame*) calloc(gf->nimages + 1, sizeof(shown_frame));

    for (i = 0; i < gf->nimages; ++i) {
        const gc_image* gi = &gf->images[i];
        const uint8_t* cm = gi->local_colormap ? gi->local_colormap : gf->global_colormap;
        int ncol = gi->local_colormap ? gi->local_colormap_size : gf->global_colormap_size;
        uint64_t h = 14695981039346656037ULL;
        if (gi->disposal == 3)
            memcpy(saved, canvas, n * sizeof(uint32_t));
        for (y = 0; y < gi->height && gi->top + y < ap->height; ++y)
            for (x = 0; x < gi->width && gi->left + x < ap->width; ++x) {
                int c = gi->pixels[(size_t) y * gi->width + x];
                uint32_t* px = &canvas[(size_t) (gi->top + y) * ap->width + gi->left + x];
                if (c == gi->transparent)
                    continue;
                else if (c < ncol)
                    *px = 0xFF000000U | cm[3*c] << 16 | cm[3*c+1] << 8 | cm[3*c+2];
                else
                    *px = 0x01000000U | c;      /* no such color */
            }
        for (k = 0; k < n; ++k)
            h = (h ^ canvas[k]) * 1099511628211ULL;
        if (ap->nframes > 0 && ap->frames[ap->nframes - 1].hash == h)
            ap->frames[ap->nframes - 1].delay += gi->delay;
        else {
            ap->frames[ap->nframes].hash = h;
            ap->frames[ap->nframes].delay = gi->delay;
            ++ap->nframes;
        }
        if (gi->disposal == 2) {
            for (y = gi->top; y < gi->top + gi->height && y < ap->height; ++y)
                for (x = gi->left; x < gi->left + gi->width && x < ap->width; ++x)
                    canvas[(size_t) y * ap->width + x] = 0;
        } else if (gi->disposal == 3)
            memcpy(canvas, saved, n * sizeof(uint32_t));
    }
    free(canvas);
    free(saved);
}

static void parse_appearance(const char* data, size_t len, appearance* ap) {
    gc_file gf;
    if (gc_parse((const uint8_t*) data, len, &gf))
        abort();                /* check_gif already accepted it */
    composite(&gf, ap);
    gc_free(&gf);
}

static void check_same_appearance(const char* name, const char* a, size_t alen,
                                  const char* b, size_t blen) {
    appearance x, y;
    int i;
    parse_appearance(a, alen, &x);
    parse_appearance(b, blen, &y);
    if (x.width != y.width || x.height != y.height)
        fuzz_failed("%s: appearance changed: screen %dx%d became %dx%d",
                    name, x.width, x.height, y.width, y.height);
    if (x.loopcount != y.loopcount)
        fuzz_failed("%s: appearance changed: loop count %d became %d",
                    name, x.loopcount, y.loopcount);
    for (i = 0; i < x.nframes && i < y.nframes; ++i) {
        if (x.frames[i].hash != y.frames[i].hash)
            fuzz_failed("%s: appearance changed: displayed frame %d differs", name, i);
        if (x.frames[i].delay != y.frames[i].delay)
            fuzz_failed("%s: appearance changed: displayed frame %d's delay %lu became %lu",
                        name, i, x.frames[i].delay, y.frames[i].delay);
    }
    if (x.nframes != y.nframes)
        fuzz_failed("%s: appearance changed: %d displayed frames became %d",
                    name, x.nframes, y.nframes);
    free(x.frames);
    free(y.frames);
}

/* Compositing huge outputs (from explicit --logical-screen or --resize,
   say) would take too long and too much memory. */
static int both_within_budget(const char* a, size_t alen,
                              const char* b, size_t blen) {
    return fuzz_within_budget((const uint8_t*) a, alen)
        && fuzz_within_budget((const uint8_t*) b, blen);
}

/* Fail if `b`, a run of a variant of the command that made `a`, has a
   different status or output GIFs that look different. */
static void check_same_outputs(const snapshot* a, const snapshot* b,
                               const run_spec* spec, const char* what) {
    int i;
    if (a->status != b->status)
        fuzz_failed("%s returned %d, not %d", what, b->status, a->status);
    if (fuzz_is_gif(a->out, a->out_len) || fuzz_is_gif(b->out, b->out_len)) {
        if (!fuzz_is_gif(a->out, a->out_len) || !fuzz_is_gif(b->out, b->out_len))
            fuzz_failed("%s's stdout differs", what);
        if (both_within_budget(a->out, a->out_len, b->out, b->out_len))
            check_same_appearance("<stdout>", a->out, a->out_len, b->out, b->out_len);
    }
    for (i = 0; i < a->nfiles; ++i) {
        const dir_entry* ea = &a->files[i];
        const dir_entry* eb = find_entry(b, ea->name);
        if (fuzz_unchanged_input(spec, ea) || !fuzz_is_gif(ea->data, ea->len))
            continue;
        if (!eb || !fuzz_is_gif(eb->data, eb->len))
            fuzz_failed("%s didn't write %s", what, ea->name);
        if (both_within_budget(ea->data, ea->len, eb->data, eb->len))
            check_same_appearance(ea->name, ea->data, ea->len, eb->data, eb->len);
    }
}

/* Options that must not change how a GIF looks when reprocessing it. */
static const char* const appearance_options[][2] = {
    {"-O1", 0}, {"-O2", 0}, {"-O3", 0}, {"-U", 0}, {"-U", "-O3"},
    {"--careful", 0}, {"--conserve-memory", 0}, {"--interlace", 0},
    {"--no-interlace", 0}, {"--no-comments", 0}, {"--no-names", 0}
};
#define NAPPEARANCE_OPTIONS \
    (sizeof(appearance_options) / sizeof(appearance_options[0]))

/* Run gifsicle with appearance-preserving options on the first GIF that
   `snap` holds, and check that the result looks the same. */
static void check_reprocessing(const run_spec* spec, const snapshot* snap,
                               uint64_t seed) {
    const char* gif = NULL;
    size_t len = 0;
    run_spec check;
    snapshot result;
    const dir_entry* out;
    int i, j;

    if (fuzz_is_gif(snap->out, snap->out_len))
        gif = snap->out, len = snap->out_len;
    for (i = 0; !gif && i < snap->nfiles; ++i)
        if (!fuzz_unchanged_input(spec, &snap->files[i])
            && fuzz_is_gif(snap->files[i].data, snap->files[i].len))
            gif = snap->files[i].data, len = snap->files[i].len;
    if (!gif || !fuzz_within_budget((const uint8_t*) gif, len))
        return;

    spec_init(&check);
    /* explicit options like --scale can exceed the default --max-screen;
       the budget bounds the size */
    spec_arg(&check, "--no-max-screen");
    for (i = (int) (seed % 3); i >= 0; --i) {      /* 1-3 options */
        seed /= 3;
        for (j = 0; j < 2 && appearance_options[seed % NAPPEARANCE_OPTIONS][j]; ++j)
            spec_arg(&check, "%s", appearance_options[seed % NAPPEARANCE_OPTIONS][j]);
        seed /= NAPPEARANCE_OPTIONS;
    }
    spec_arg(&check, "check.gif");
    spec_arg(&check, "-o");
    spec_arg(&check, "check-out.gif");
    spec_file(&check, "check.gif", gif, len);

    cur_check_spec = &check;
    run_once(&check, &result);
    cur_snapshot = &result;
    check_snapshot(&check, &result);
    if (result.status != 0)
        fuzz_failed("gifsicle failed on its own output");
    if (!(out = find_entry(&result, "check-out.gif")))
        fuzz_failed("gifsicle wrote nothing for its own output");
    check_same_appearance("check-out.gif", gif, len, out->data, out->len);
    cur_check_spec = NULL;
    free_snapshot(&result);
    spec_free(&check);
}

/* Run a variant of `spec` with `extra` options, and check its results
   against `first`'s: byte for byte if `exact`, otherwise by appearance. */
static void check_variant(const run_spec* spec, const snapshot* first,
                          const char* const* extra, int nextra, int exact) {
    run_spec variant;
    snapshot result;
    char what[128];
    snprintf(what, sizeof(what), "the run with %s", extra[0]);
    spec_derive(&variant, spec, extra, nextra);
    cur_check_spec = &variant;
    run_once(&variant, &result);
    cur_snapshot = &result;
    check_snapshot(&variant, &result);
    if (exact)
        /* messages may differ: -j2 can warn that it can't use threads */
        check_same(first, &result, what, 0);
    else
        check_same_outputs(first, &result, spec, what);
    cur_check_spec = NULL;
    free_snapshot(&result);
    spec_free(&variant);
}


/*****
 * Entry points
 **/

static char** fixed_argv;     /* kept for the life of the process */

/* Make -artifact_prefix absolute, adding one if necessary: crashes during a
   run happen in the scratch directory. Both libFuzzer and standalone.c parse
   their flags after calling LLVMFuzzerInitialize. */
static void fix_artifact_prefix(int* argc, char*** argv) {
    char cwd[PATH_MAX], **nargv;
    int i, found = 0;
    if (!getcwd(cwd, sizeof(cwd)))
        return;
    nargv = (char**) calloc(*argc + 2, sizeof(char*));
    for (i = 0; i < *argc; ++i) {
        const char* a = (*argv)[i];
        nargv[i] = (*argv)[i];
        if (strncmp(a, "-artifact_prefix=", 17) == 0
            || strncmp(a, "-exact_artifact_path=", 21) == 0) {
            const char* eq = strchr(a, '=') + 1;
            found = 1;
            if (*eq != '/') {
                size_t n = strlen(a) + strlen(cwd) + 2;
                nargv[i] = (char*) malloc(n);
                snprintf(nargv[i], n, "%.*s%s/%s", (int) (eq - a), a, cwd, eq);
            }
        }
    }
    if (!found) {
        size_t n = strlen(cwd) + 20;
        nargv[*argc] = (char*) malloc(n);
        snprintf(nargv[*argc], n, "-artifact_prefix=%s/", cwd);
        ++*argc;
    }
    nargv[*argc] = NULL;
    *argv = fixed_argv = nargv;
}

void fuzz_initialize(int* argc, char*** argv, const char* name) {
    const char* tmp = getenv("TMPDIR");
    prog = name;
    fix_artifact_prefix(argc, argv);
    verbose = getenv("GIFSICLE_FUZZ_VERBOSE") != NULL;
    twice = getenv("GIFSICLE_FUZZ_TWICE") != NULL;
    repro_dir = getenv("GIFSICLE_FUZZ_REPRO");
    snprintf(workdir, sizeof(workdir), "%s/%s.XXXXXX",
             tmp && *tmp ? tmp : "/tmp", name);
    if (!mkdtemp(workdir) || (home_fd = open(".", O_RDONLY)) < 0) {
        fprintf(stderr, "%s: %s: %s\n", prog, workdir, strerror(errno));
        abort();
    }
    atexit(remove_workdir);
    gifsicle_fuzz_stdin = fopen("/dev/null", "r");
}

void fuzz_run(const run_spec* spec, uint64_t seed, fuzz_check extra) {
    static const char* const resizing[] = {"--re", "--sc", 0};
    static const char* const lossy[] = {"--lo", 0};
    static const char* const threads[] = {"-j2"};
    static const char* const careful[] = {"--careful"};
    static const char* const conserve[] = {"--conserve-memory"};
    snapshot first;

    if (repro_dir)
        write_repro(spec);
    cur_spec = spec;
    run_once(spec, &first);
    cur_snapshot = &first;
    check_snapshot(spec, &first);
    if (extra)
        extra(spec, &first);

    if (twice) {
        snapshot again;
        run_once(spec, &again);
        cur_snapshot = &again;
        check_same(&first, &again, "a second run", 1);
        free_snapshot(&again);
    }
    if (spec_has_arg(spec, resizing))
        check_variant(spec, &first, threads, 1, 1);
    if (!spec_has_arg(spec, lossy))
        check_variant(spec, &first, (seed & 1) ? careful : conserve, 1, 0);
    cur_snapshot = &first;
    check_reprocessing(spec, &first, seed >> 1);

    cur_spec = NULL;
    cur_snapshot = NULL;
    free_snapshot(&first);
}
