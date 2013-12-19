/* xform.c - Image transformation functions for gifsicle.
   Copyright (C) 1997-2013 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include "kcolor.h"
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif


/******
 * color transforms
 **/

Gt_ColorTransform *
append_color_transform(Gt_ColorTransform *list,
		       color_transform_func func, void *data)
{
  Gt_ColorTransform *trav;
  Gt_ColorTransform *xform = Gif_New(Gt_ColorTransform);
  xform->next = 0;
  xform->func = func;
  xform->data = data;

  for (trav = list; trav && trav->next; trav = trav->next)
    ;
  if (trav) {
    trav->next = xform;
    return list;
  } else
    return xform;
}

Gt_ColorTransform *
delete_color_transforms(Gt_ColorTransform *list, color_transform_func func)
{
  Gt_ColorTransform *prev = 0, *trav = list;
  while (trav) {
    Gt_ColorTransform *next = trav->next;
    if (trav->func == func) {
      if (prev) prev->next = next;
      else list = next;
      Gif_Delete(trav);
    } else
      prev = trav;
    trav = next;
  }
  return list;
}

void
apply_color_transforms(Gt_ColorTransform *list, Gif_Stream *gfs)
{
  int i;
  Gt_ColorTransform *xform;
  for (xform = list; xform; xform = xform->next) {
    if (gfs->global)
      xform->func(gfs->global, xform->data);
    for (i = 0; i < gfs->nimages; i++)
      if (gfs->images[i]->local)
	xform->func(gfs->images[i]->local, xform->data);
  }
}


typedef struct Gt_ColorChange {
  struct Gt_ColorChange *next;
  Gif_Color old_color;
  Gif_Color new_color;
} Gt_ColorChange;

void
color_change_transformer(Gif_Colormap *gfcm, void *thunk)
{
  int i, have;
  Gt_ColorChange *first_change = (Gt_ColorChange *)thunk;
  Gt_ColorChange *change;

  /* change colors named by color */
  for (i = 0; i < gfcm->ncol; i++)
    for (change = first_change; change; change = change->next) {
      if (!change->old_color.haspixel)
	have = GIF_COLOREQ(&gfcm->col[i], &change->old_color);
      else
	have = (change->old_color.pixel == (uint32_t)i);

      if (have) {
	gfcm->col[i] = change->new_color;
	break;			/* ignore remaining color changes */
      }
    }
}

Gt_ColorTransform *
append_color_change(Gt_ColorTransform *list,
		    Gif_Color old_color, Gif_Color new_color)
{
  Gt_ColorTransform *xform;
  Gt_ColorChange *change = Gif_New(Gt_ColorChange);
  change->old_color = old_color;
  change->new_color = new_color;
  change->next = 0;

  for (xform = list; xform && xform->next; xform = xform->next)
    ;
  if (!xform || xform->func != &color_change_transformer)
    return append_color_transform(list, &color_change_transformer, change);
  else {
    Gt_ColorChange *prev = (Gt_ColorChange *)(xform->data);
    while (prev->next) prev = prev->next;
    prev->next = change;
    return list;
  }
}


void
pipe_color_transformer(Gif_Colormap *gfcm, void *thunk)
{
  int i, status;
  FILE *f;
  Gif_Color *col = gfcm->col;
  Gif_Colormap *new_cm = 0;
  char *command = (char *)thunk;
#ifdef HAVE_MKSTEMP
# ifdef P_tmpdir
  char tmp_file[] = P_tmpdir "/gifsicle.XXXXXX";
# else
  char tmp_file[] = "/tmp/gifsicle.XXXXXX";
# endif
#else
  char *tmp_file = tmpnam(0);
#endif
  char *new_command;

#ifdef HAVE_MKSTEMP
  if (mkstemp(tmp_file) < 0)
    fatal_error("can%,t create temporary file!");
#else
  if (!tmp_file)
    fatal_error("can%,t create temporary file!");
#endif

  new_command = Gif_NewArray(char, strlen(command) + strlen(tmp_file) + 4);
  sprintf(new_command, "%s  >%s", command, tmp_file);
  f = popen(new_command, "w");
  if (!f)
    fatal_error("can%,t run color transformation command: %s", strerror(errno));
  Gif_DeleteArray(new_command);

  for (i = 0; i < gfcm->ncol; i++)
    fprintf(f, "%d %d %d\n", col[i].gfc_red, col[i].gfc_green, col[i].gfc_blue);

  errno = 0;
  status = pclose(f);
  if (status < 0) {
    error(1, "color transformation error: %s", strerror(errno));
    goto done;
  } else if (status > 0) {
    error(1, "color transformation command failed");
    goto done;
  }

  f = fopen(tmp_file, "r");
  if (!f || feof(f)) {
    error(1, "color transformation command generated no output", command);
    if (f) fclose(f);
    goto done;
  }
  new_cm = read_colormap_file("<color transformation>", f);
  fclose(f);

  if (new_cm) {
    int nc = new_cm->ncol;
    if (nc < gfcm->ncol) {
      nc = gfcm->ncol;
      warning(1, "too few colors in color transformation results");
    } else if (nc > gfcm->ncol)
      warning(1, "too many colors in color transformation results");
    for (i = 0; i < nc; i++)
      col[i] = new_cm->col[i];
  }

 done:
  remove(tmp_file);
  Gif_DeleteColormap(new_cm);
}



/*****
 * crop image; returns true if the image exists
 **/

void
combine_crop(Gt_Crop *dstcrop, const Gt_Crop *srccrop, const Gif_Image *gfi)
{
    dstcrop->x = srccrop->x - gfi->left;
    dstcrop->y = srccrop->y - gfi->top;
    dstcrop->w = srccrop->w;
    dstcrop->h = srccrop->h;

    /* Check that the rectangle actually intersects with the image. */
    if (dstcrop->x < 0)
	dstcrop->w += dstcrop->x, dstcrop->x = 0;
    if (dstcrop->y < 0)
	dstcrop->h += dstcrop->y, dstcrop->y = 0;
    if (dstcrop->w > 0 && dstcrop->x + dstcrop->w > gfi->width)
	dstcrop->w = gfi->width - dstcrop->x;
    if (dstcrop->h > 0 && dstcrop->y + dstcrop->h > gfi->height)
	dstcrop->h = gfi->height - dstcrop->y;
    if (dstcrop->w < 0)
        dstcrop->w = 0;
    if (dstcrop->h < 0)
        dstcrop->h = 0;
}

int
crop_image(Gif_Image *gfi, Gt_Crop *crop, int preserve_total_crop)
{
    Gt_Crop c;
    int j;
    uint8_t **img;

    combine_crop(&c, crop, gfi);

  if (c.w > 0 && c.h > 0) {
    img = Gif_NewArray(uint8_t *, c.h + 1);
    for (j = 0; j < c.h; j++)
      img[j] = gfi->img[c.y + j] + c.x;
    img[c.h] = 0;

    gfi->left += c.x - crop->left_offset;
    gfi->top += c.y - crop->top_offset;

  } else if (preserve_total_crop) {
    c.w = c.h = 1;
    img = Gif_NewArray(uint8_t *, c.h + 1);
    img[0] = gfi->img[0];
    img[1] = 0;
    gfi->transparent = img[0][0];

  } else {
    /* Empty image */
    c.w = c.h = 0;
    img = 0;
  }

  Gif_DeleteArray(gfi->img);
  gfi->img = img;
  gfi->width = c.w;
  gfi->height = c.h;
  return gfi->img != 0;
}


/*****
 * flip and rotate
 **/

void
flip_image(Gif_Image *gfi, int screen_width, int screen_height, int is_vert)
{
  int x, y;
  int width = gfi->width;
  int height = gfi->height;
  uint8_t **img = gfi->img;

  /* horizontal flips */
  if (!is_vert) {
    uint8_t *buffer = Gif_NewArray(uint8_t, width);
    uint8_t *trav;
    for (y = 0; y < height; y++) {
      memcpy(buffer, img[y], width);
      trav = img[y] + width - 1;
      for (x = 0; x < width; x++)
	*trav-- = buffer[x];
    }
    gfi->left = screen_width - (gfi->left + width);
    Gif_DeleteArray(buffer);
  }

  /* vertical flips */
  if (is_vert) {
    uint8_t **buffer = Gif_NewArray(uint8_t *, height);
    memcpy(buffer, img, height * sizeof(uint8_t *));
    for (y = 0; y < height; y++)
      img[y] = buffer[height - y - 1];
    gfi->top = screen_height - (gfi->top + height);
    Gif_DeleteArray(buffer);
  }
}

void
rotate_image(Gif_Image *gfi, int screen_width, int screen_height, int rotation)
{
  int x, y;
  int width = gfi->width;
  int height = gfi->height;
  uint8_t **img = gfi->img;
  uint8_t *new_data = Gif_NewArray(uint8_t, width * height);
  uint8_t *trav = new_data;

  /* this function can only rotate by 90 or 270 degrees */
  assert(rotation == 1 || rotation == 3);

  if (rotation == 1) {
    for (x = 0; x < width; x++)
      for (y = height - 1; y >= 0; y--)
	*trav++ = img[y][x];
    x = gfi->left;
    gfi->left = screen_height - (gfi->top + height);
    gfi->top = x;

  } else {
    for (x = width - 1; x >= 0; x--)
      for (y = 0; y < height; y++)
	*trav++ = img[y][x];
    y = gfi->top;
    gfi->top = screen_width - (gfi->left + width);
    gfi->left = y;
  }

  Gif_ReleaseUncompressedImage(gfi);
  gfi->width = height;
  gfi->height = width;
  Gif_SetUncompressedImage(gfi, new_data, Gif_DeleteArrayFunc, 0);
}


/*****
 * scale
 **/

/* kcscreen: A frame buffer containing `kcolor`s (one per pixel).
   Kcolors with components < 0 represent transparency. */
typedef struct {
    kcolor* data;       /* data buffer: (x,y) in data[y*sw + x] */
    kcolor* scratch;    /* scratch buffer, used for previous disposal */
    unsigned sw;        /* screen width */
    unsigned sh;        /* screen height */
    kcolor bg;          /* background color */
} kcscreen;

/* initialize `kcs` to an empty state */
static void kcscreen_clear(kcscreen* kcs) {
    kcs->data = kcs->scratch = NULL;
}

/* initialize `kcs` for stream `gfs`. Screen size is `sw`x`sh`; if either
   component is <= 0, the value of `gfs->screen_*` is taken. */
static void kcscreen_init(kcscreen* kcs, Gif_Stream* gfs, int sw, int sh) {
    unsigned i, sz;
    assert(!kcs->data && !kcs->scratch);
    kcs->sw = sw <= 0 ? gfs->screen_width : sw;
    kcs->sh = sh <= 0 ? gfs->screen_height : sh;
    sz = (unsigned) kcs->sw * kcs->sh;
    kcs->data = Gif_NewArray(kcolor, sz);
    if (gfs->nimages > 0 && gfs->images[0]->transparent >= 0) {
        /* XXX assume memset(..., 255, ...) sets to -1 (pretty good ass'n) */
        memset(kcs->data, 255, sizeof(kcolor) * sz);
        kcs->bg.a[0] = kcs->bg.a[1] = kcs->bg.a[2] = -1;
    } else {
        kcs->bg = kc_makegfcg(&gfs->global->col[gfs->background]);
        for (i = 0; i != sz; ++i)
            kcs->data[i] = kcs->bg;
    }
}

/* free memory associated with `kcs` */
static void kcscreen_cleanup(kcscreen* kcs) {
    Gif_DeleteArray(kcs->data);
    Gif_DeleteArray(kcs->scratch);
}

/* apply image `gfi` to screen `kcs`, using the color array `ks` */
static void kcscreen_apply(kcscreen* kcs, const Gif_Image* gfi,
                           const kcolor* ks) {
    int x, y;
    assert((unsigned) gfi->left + gfi->width <= kcs->sw);
    assert((unsigned) gfi->top + gfi->height <= kcs->sh);

    if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
        if (!kcs->scratch)
            kcs->scratch = Gif_NewArray(kcolor, kcs->sw * kcs->sh);
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            memcpy(&kcs->scratch[y * kcs->sw + gfi->left],
                   &kcs->data[y * kcs->sw + gfi->left],
                   sizeof(kcolor) * gfi->width);
    }

    for (y = gfi->top; y != gfi->top + gfi->height; ++y) {
        const uint8_t* linein = gfi->img[y - gfi->top];
        kcolor* lineout = &kcs->data[y * kcs->sw + gfi->left];
        for (x = 0; x != gfi->width; ++x)
            if (linein[x] != gfi->transparent)
                lineout[x] = ks[linein[x]];
    }
}

/* dispose of image `gfi`, which was previously applied by kcscreen_apply */
static void kcscreen_dispose(kcscreen* kcs, const Gif_Image* gfi) {
    int x, y;
    assert((unsigned) gfi->left + gfi->width <= kcs->sw);
    assert((unsigned) gfi->top + gfi->height <= kcs->sh);

    if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            memcpy(&kcs->data[y * kcs->sw + gfi->left],
                   &kcs->scratch[y * kcs->sw + gfi->left],
                   sizeof(kcolor) * gfi->width);
    } else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND && kcs->bg.a[0] < 0) {
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            memset(&kcs->data[y * kcs->sw + gfi->left], 255,
                   sizeof(kcolor) * gfi->width);
    } else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND) {
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            for (x = gfi->left; x != gfi->left + gfi->width; ++x)
                kcs->data[y * kcs->sw + x] = kcs->bg;
    }
}


typedef struct {
    Gif_Stream* gfs;
    Gif_Image* gfi;
    int imageno;
    const uint16_t* xoff;
    const uint16_t* yoff;
    kd3_tree global_kd3;
    kcscreen in;
    kcscreen out;
    double rxfactor;
    double ryfactor;
} scale_context;

static void scale_context_init(scale_context* sctx, Gif_Stream* gfs,
                               const uint16_t* xoff, const uint16_t* yoff) {
    sctx->gfs = gfs;
    sctx->gfi = NULL;
    sctx->xoff = xoff;
    sctx->yoff = yoff;
    sctx->global_kd3.ks = NULL;
    kcscreen_clear(&sctx->in);
    kcscreen_clear(&sctx->out);
}

static void scale_context_cleanup(scale_context* sctx) {
    if (sctx->global_kd3.ks)
        kd3_cleanup(&sctx->global_kd3);
    kcscreen_cleanup(&sctx->in);
    kcscreen_cleanup(&sctx->out);
}

static void scale_image_data_point(scale_context* sctx, Gif_Image* new_gfi) {
    uint8_t* data;
    Gif_Image* gfi = sctx->gfi;
    const uint16_t* xoff = &sctx->xoff[gfi->left];
    const uint16_t* yoff = &sctx->yoff[gfi->top];
    int xi, yi, xo, yo;

    if (new_gfi->width == 0 || new_gfi->height == 0) {
        /* don't create zero-dimensioned frames */
        new_gfi->width = new_gfi->height = 1;
        Gif_CreateUncompressedImage(new_gfi, 0);
        new_gfi->image_data[0] = new_gfi->transparent = 0;
        new_gfi->disposal = GIF_DISPOSAL_ASIS;
        return;
    }

    Gif_CreateUncompressedImage(new_gfi, 0);
    data = new_gfi->image_data;

    for (yi = 0; yi < gfi->height; ++yi)
        if (yoff[yi] != yoff[yi+1]) {
            const uint8_t* in_line = gfi->img[yi];
            for (xi = 0; xi < gfi->width; ++xi, ++in_line)
                for (xo = xoff[xi]; xo != xoff[xi+1]; ++xo, ++data)
                    *data = *in_line;
            for (yo = yoff[yi] + 1; yo != yoff[yi+1]; ++yo) {
                memcpy(data, data - new_gfi->width, new_gfi->width);
                data += new_gfi->width;
            }
        }
}

static kd3_tree* scale_image_prepare(scale_context* sctx, kd3_tree* local_kd3) {
    kd3_tree* kd3;
    int sw = sctx->gfs->screen_width;

    if (sctx->gfi->local) {
        kd3 = local_kd3;
        kd3_init_build(kd3, NULL, sctx->gfi->local);
    } else {
        kd3 = &sctx->global_kd3;
        if (!kd3->ks)
            kd3_init_build(kd3, NULL, sctx->gfs->global);
        kd3_enable_all(kd3);
    }
    if (sctx->gfi->transparent >= 0 && sctx->gfi->transparent < kd3->nitems)
        kd3_disable(kd3, sctx->gfi->transparent);

    /* apply image to sctx->in */
    if (!sctx->in.data) {
        /* first image, set up screens */
        kcscreen_init(&sctx->in, sctx->gfs, 0, 0);
        kcscreen_init(&sctx->out, sctx->gfs,
                      sctx->xoff[sw], sctx->yoff[sctx->gfs->screen_height]);
    }
    kcscreen_apply(&sctx->in, sctx->gfi, kd3->ks);

    return kd3;
}

static void scale_image_complete(scale_context* sctx, Gif_Image* new_gfi,
                                 kd3_tree* kd3) {
    /* apply disposal to sctx->in and sctx->out */
    if (sctx->imageno != sctx->gfs->nimages - 1) {
        kcscreen_dispose(&sctx->in, sctx->gfi);

        if (sctx->gfi->disposal == GIF_DISPOSAL_BACKGROUND)
            kcscreen_dispose(&sctx->out, new_gfi);
        else if (sctx->gfi->disposal != GIF_DISPOSAL_PREVIOUS)
            kcscreen_apply(&sctx->out, new_gfi, kd3->ks);
    }

    if (sctx->gfi->local)
        kd3_cleanup(kd3);
}

static void scale_image_data_box(scale_context* sctx, Gif_Image* new_gfi) {
    uint8_t* data;
    Gif_Image* gfi = sctx->gfi;
    int sw = sctx->gfs->screen_width;
    int xo, yo, xi, yi, xi1, xd, yd, n, i, j, k;
    double xc[3];
    kd3_tree local_kd3, *kd3;
    kcolor kc;

    kd3 = scale_image_prepare(sctx, &local_kd3);

    /* create image data */
    /* might need to expand the output a bit; it must cover all pixels
       modified by the input image or we'll get "trail" type artifacts */
    if (sctx->xoff[gfi->left+gfi->width-1] == new_gfi->left+new_gfi->width)
        ++new_gfi->width;
    if (sctx->yoff[gfi->top+gfi->height-1] == new_gfi->top+new_gfi->height)
        ++new_gfi->height;
    Gif_CreateUncompressedImage(new_gfi, 0);
    data = new_gfi->image_data;

    /* for each output pixel, pick closest pixel to output screen */
    xi1 = gfi->left;
    while (xi1 > 0 && sctx->xoff[xi1-1] == sctx->xoff[xi1])
        --xi1;
    yi = gfi->top;
    while (yi > 0 && sctx->yoff[yi-1] == sctx->yoff[yi])
        --yi;

    for (yo = new_gfi->top; yo != new_gfi->top + new_gfi->height;
         ++yo) {
        /* find the range of input pixels that affect it */
        for (yd = 1; sctx->yoff[yi+yd] <= yo; ++yd)
            /* nada */;

        xi = xi1;
        for (xo = new_gfi->left; xo != new_gfi->left + new_gfi->width;
             ++xo, ++data) {
            for (xd = 1; sctx->xoff[xi+xd] <= xo; ++xd)
                /* nada */;

            /* combine the input pixels */
            n = 0;
            xc[0] = xc[1] = xc[2] = 0;
            for (j = 0; j != yd; ++j) {
                const kcolor* indata = &sctx->in.data[sw*(yi+j) + xi];
                for (i = 0; i != xd; ++i)
                    if (indata[i].a[0] >= 0) {
                        for (k = 0; k != 3; ++k)
                            xc[k] += indata[i].a[k];
                        ++n;
                    }
            }

            /* find the closest match */
            if (gfi->transparent >= 0 && n <= (xd * yd) / 4)
                *data = gfi->transparent;
            else {
                for (k = 0; k != 3; ++k) {
                    int v = (int) (xc[k] / n + 0.5);
                    kc.a[k] = KC_CLAMPV(v);
                }
                *data = kd3_closest_transformed(kd3, &kc);
                if (gfi->transparent >= 0) {
                    int outidx = sctx->out.sw*yo + xo;
                    if (sctx->out.data[outidx].a[0] >= 0
                        && kc_distance(&sctx->out.data[outidx], &kc)
                           <= kc_distance(&kd3->ks[*data], &kc))
                        *data = gfi->transparent;
                }
            }

            /* advance */
            if (sctx->xoff[xi+xd] == xo + 1)
                xi += xd;
        }

        if (sctx->yoff[yi+yd] == yo + 1)
            yi += yd;
    }

    scale_image_complete(sctx, new_gfi, kd3);
}

static double bounds_factor(int val, const double bounds[2]) {
    if (val < bounds[0] && bounds[1] < val + 1)
        return bounds[1] - bounds[0];
    else if (val < bounds[0])
        return 1 - (bounds[0] - val);
    else if (bounds[1] < val + 1)
        return bounds[1] - val;
    else
        return 1.0;
}

static void scale_image_data_mix(scale_context* sctx, Gif_Image* new_gfi) {
    uint8_t* data;
    Gif_Image* gfi = sctx->gfi;
    int sw = sctx->gfs->screen_width;
    int xo, yo, i, j, k;
    double xc[3], n, bounds[4];
    double transpbound = sctx->rxfactor * sctx->ryfactor / 4;
    kd3_tree local_kd3, *kd3;
    kcolor kc;

    kd3 = scale_image_prepare(sctx, &local_kd3);

    /* create image data */
    /* might need to expand the output a bit; it must cover all pixels
       modified by the input image or we'll get "trail" type artifacts */
    if (sctx->xoff[gfi->left+gfi->width-1] == new_gfi->left+new_gfi->width)
        ++new_gfi->width;
    if (sctx->yoff[gfi->top+gfi->height-1] == new_gfi->top+new_gfi->height)
        ++new_gfi->height;
    Gif_CreateUncompressedImage(new_gfi, 0);
    data = new_gfi->image_data;

    /* for each output pixel, pick closest pixel to output screen */
    for (yo = new_gfi->top; yo != new_gfi->top + new_gfi->height;
         ++yo) {
        for (xo = new_gfi->left; xo != new_gfi->left + new_gfi->width;
             ++xo, ++data) {
            /* combine the input pixels */
            n = 0;
            xc[0] = xc[1] = xc[2] = 0;

            /* create floating-point bounds */
            bounds[0] = xo * sctx->rxfactor;
            if (xo + 1 == (int) sctx->out.sw)
                bounds[1] = sctx->in.sw;
            else
                bounds[1] = (xo + 1) * sctx->rxfactor;
            bounds[2] = yo * sctx->ryfactor;
            if (yo + 1 == (int) sctx->out.sh)
                bounds[3] = sctx->in.sh;
            else
                bounds[3] = (yo + 1) * sctx->ryfactor;

            /* run over integer bounds */
            for (j = (int) bounds[2]; j < bounds[3]; ++j) {
                double jf = bounds_factor(j, &bounds[2]);
                const kcolor* indata = &sctx->in.data[sw*j];
                for (i = (int) bounds[0]; i < bounds[1]; ++i) {
                    double f = jf * bounds_factor(i, &bounds[0]);
                    if (indata[i].a[0] >= 0) {
                        for (k = 0; k != 3; ++k)
                            xc[k] += indata[i].a[k] * f;
                        n += f;
                    }
                }
            }

            /* find the closest match */
            if (gfi->transparent >= 0 && n <= transpbound)
                *data = gfi->transparent;
            else {
                for (k = 0; k != 3; ++k) {
                    int v = (int) (xc[k] / n + 0.5);
                    kc.a[k] = KC_CLAMPV(v);
                }
                *data = kd3_closest_transformed(kd3, &kc);
                if (gfi->transparent >= 0) {
                    int outidx = sctx->out.sw*yo + xo;
                    if (sctx->out.data[outidx].a[0] >= 0
                        && kc_distance(&sctx->out.data[outidx], &kc)
                           <= kc_distance(&kd3->ks[*data], &kc))
                        *data = gfi->transparent;
                }
            }
        }
    }

    scale_image_complete(sctx, new_gfi, kd3);
}

static void scale_image(scale_context* sctx, int method) {
    Gif_Image* gfi = sctx->gfi;
    Gif_Image new_gfi;
    int was_compressed = (gfi->img == 0);

    /* Fri 9 Jan 1999: Fix problem with resizing animated GIFs: we scaled
       from left edge of the *subimage* to right edge of the subimage,
       causing consistency problems when several subimages overlap.
       Solution: always use scale factors relating to the *whole image* (the
       screen size). */

    /* calculate new width and height based on the four edges (left, right,
       top, bottom). This is better than simply multiplying the width and
       height by the scale factors because it avoids roundoff
       inconsistencies between frames on animated GIFs. Don't allow 0-width
       or 0-height images; GIF doesn't support them well. */
    new_gfi = *gfi;
    new_gfi.left = sctx->xoff[gfi->left];
    new_gfi.top = sctx->yoff[gfi->top];
    new_gfi.width = sctx->xoff[gfi->left + gfi->width] - new_gfi.left;
    new_gfi.height = sctx->yoff[gfi->top + gfi->height] - new_gfi.top;
    new_gfi.img = NULL;
    new_gfi.image_data = NULL;
    new_gfi.compressed = NULL;

    if (was_compressed)
        Gif_UncompressImage(gfi);

    if (method == SCALE_METHOD_MIX)
        scale_image_data_mix(sctx, &new_gfi);
    else if (method == SCALE_METHOD_BOX)
        scale_image_data_box(sctx, &new_gfi);
    else
        scale_image_data_point(sctx, &new_gfi);

    Gif_ReleaseUncompressedImage(gfi);
    Gif_ReleaseCompressedImage(gfi);
    *gfi = new_gfi;
    if (was_compressed) {
        Gif_FullCompressImage(sctx->gfs, gfi, &gif_write_info);
        Gif_ReleaseUncompressedImage(gfi);
    }
}

void
resize_stream(Gif_Stream *gfs, double new_width, double new_height, int fit,
              int method)
{
    double xfactor, yfactor;
    uint16_t* xyarr;
    scale_context sctx;
    int i, nw, nh;

    Gif_CalculateScreenSize(gfs, 0);
    assert(gfs->nimages > 0);
    assert(gfs->images[0]->transparent >= 0
           || (int) gfs->background < gfs->global->ncol);

    if (new_width < 0.5 && new_height < 0.5)
        /* do nothing */
        return;
    else if (new_width < 0.5)
        new_width = (int)
            (gfs->screen_width * new_height / gfs->screen_height + 0.5);
    else if (new_height < 0.5)
        new_height = (int)
            (gfs->screen_height * new_width / gfs->screen_width + 0.5);

    if (new_width >= GIF_MAX_SCREEN_WIDTH + 0.5
        || new_height >= GIF_MAX_SCREEN_HEIGHT + 0.5)
        fatal_error("new image is too large (max size 65535x65535)");

    nw = (int) (new_width + 0.5);
    nh = (int) (new_height + 0.5);

    if (fit && nw >= gfs->screen_width && nh >= gfs->screen_height)
        return;
    else if (fit) {
        xfactor = (double) nw / gfs->screen_width;
        yfactor = (double) nh / gfs->screen_height;
        if (xfactor < yfactor)
            nh = (int) (gfs->screen_height * xfactor + 0.5);
        else if (yfactor < xfactor)
            nw = (int) (gfs->screen_width * yfactor + 0.5);
    }

    /* refuse to create 0-pixel dimensions */
    if (nw == 0)
        nw = 1;
    if (nh == 0)
        nh = 1;

    /* create real scale factors */
    xfactor = (double) nw / gfs->screen_width;
    sctx.rxfactor = gfs->screen_width / (double) nw;
    yfactor = (double) nh / gfs->screen_height;
    sctx.ryfactor = gfs->screen_height / (double) nh;

    xyarr = Gif_NewArray(uint16_t, gfs->screen_width + gfs->screen_height + 2);
    for (i = 0; i != gfs->screen_width; ++i)
        xyarr[i] = (int) (i * xfactor);
    xyarr[gfs->screen_width] = nw;
    for (i = 0; i != gfs->screen_height; ++i)
        xyarr[gfs->screen_width + 1 + i] = (int) (i * yfactor);
    xyarr[gfs->screen_width + 1 + gfs->screen_height] = nh;
    assert(xyarr[gfs->screen_width - 1] != nw);
    assert(xyarr[gfs->screen_width + gfs->screen_height] != nh);

    scale_context_init(&sctx, gfs, &xyarr[0], &xyarr[gfs->screen_width+1]);

    /* no point to MIX or BOX method if we're expanding the image in
       both dimens */
    if ((method == SCALE_METHOD_MIX || method == SCALE_METHOD_BOX)
        && gfs->screen_width <= nw && gfs->screen_height <= nh)
        method = SCALE_METHOD_POINT;

    for (sctx.imageno = 0; sctx.imageno < gfs->nimages; ++sctx.imageno) {
        sctx.gfi = gfs->images[sctx.imageno];
        scale_image(&sctx, method);
    }

    scale_context_cleanup(&sctx);
    Gif_DeleteArray(xyarr);
    gfs->screen_width = nw;
    gfs->screen_height = nh;
}
