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
    fatal_error("can't create temporary file!");
#else
  if (!tmp_file)
    fatal_error("can't create temporary file!");
#endif

  new_command = Gif_NewArray(char, strlen(command) + strlen(tmp_file) + 4);
  sprintf(new_command, "%s  >%s", command, tmp_file);
  f = popen(new_command, "w");
  if (!f)
    fatal_error("can't run color transformation command: %s", strerror(errno));
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

typedef struct {
    Gif_Stream* gfs;
    Gif_Image* gfi;
    const uint16_t* xoff;
    const uint16_t* yoff;
    kd3_tree global_kd3;
} scale_context;

static void scale_context_init(scale_context* sctx, Gif_Stream* gfs,
                               const uint16_t* xoff, const uint16_t* yoff) {
    sctx->gfs = gfs;
    sctx->gfi = NULL;
    sctx->xoff = xoff;
    sctx->yoff = yoff;
    sctx->global_kd3.ks = NULL;
}

static void scale_context_cleanup(scale_context* sctx) {
    if (sctx->global_kd3.ks)
        kd3_cleanup(&sctx->global_kd3);
}

static void scale_image_data_trivial(scale_context* sctx, Gif_Image* new_gfi) {
    uint8_t* data = new_gfi->image_data;
    Gif_Image* gfi = sctx->gfi;
    const uint16_t* xoff = &sctx->xoff[gfi->left];
    const uint16_t* yoff = &sctx->yoff[gfi->top];
    int new_width = new_gfi->width;
    int xi, yi, xo, yo;

    for (yi = 0; yi < gfi->height; ++yi)
        if (yoff[yi] != yoff[yi+1]) {
            const uint8_t* in_line = gfi->img[yi];
            for (xi = 0; xi < gfi->width; ++xi, ++in_line)
                for (xo = xoff[xi]; xo != xoff[xi+1]; ++xo, ++data)
                    *data = *in_line;
            for (yo = yoff[yi] + 1; yo != yoff[yi+1]; ++yo, data += new_width)
                memcpy(data, data - new_width, new_width);
        }
}

static void scale_image_data_interp(scale_context* sctx, Gif_Image* new_gfi) {
    uint8_t* data = new_gfi->image_data;
    Gif_Image* gfi = sctx->gfi;
    const uint16_t* xoff = &sctx->xoff[gfi->left];
    const uint16_t* yoff = &sctx->yoff[gfi->top];
    int x1, y1, x2, y2, xi, yi, n, i;
    double xc[3];
    uint8_t* in_data = data;
    kd3_tree local_kd3, *kd3;
    kcolor kc;
    (void) in_data;

    if (gfi->local) {
        kd3 = &local_kd3;
        kd3_init_build(kd3, NULL, gfi->local);
    } else {
        kd3 = &sctx->global_kd3;
        if (!kd3->ks)
            kd3_init_build(kd3, NULL, sctx->gfs->global);
        kd3_enable_all(kd3);
    }
    if (gfi->transparent >= 0 && gfi->transparent < kd3->nitems)
        kd3_disable(kd3, gfi->transparent);

    for (y1 = 0; y1 < gfi->height; y1 = y2) {
        for (y2 = y1 + 1; yoff[y2] != yoff[y1] + 1; ++y2)
            /* spin */;
        while (y2 < gfi->height && yoff[y2+1] == yoff[y2])
            ++y2;
        assert(y2 <= gfi->height);

        for (x1 = 0; x1 < gfi->width; x1 = x2) {
            for (x2 = x1 + 1; x2 < gfi->width && xoff[x2] != xoff[x1] + 1; ++x2)
                /* spin */;
            while (x2 < gfi->width && xoff[x2+1] == xoff[x2])
                ++x2;
            assert(x2 <= gfi->width);

            if (gfi->img[y1][x1] == gfi->transparent) {
                *data++ = gfi->transparent;
                continue;
            }

            n = 0;
            xc[0] = xc[1] = xc[2] = 0;
            for (yi = y1; yi != y2; ++yi)
                for (xi = x1; xi != x2; ++xi) {
                    uint8_t pixel = gfi->img[yi][xi];
                    if (pixel != gfi->transparent) {
                        for (i = 0; i != 3; ++i)
                            xc[i] += kd3->ks[pixel].a[i];
                        ++n;
                    }
                }

            for (i = 0; i != 3; ++i) {
                int v = (int) (xc[i] / n + 0.5);
                kc.a[i] = KC_CLAMPV(v);
            }
            *data++ = kd3_closest_transformed(kd3, &kc);
        }
    }

    if (gfi->local)
        kd3_cleanup(kd3);
    assert(data - in_data == (xoff[gfi->width] - xoff[0]) * (yoff[gfi->height] - yoff[0]));
}

static void
scale_image(scale_context* sctx)
{
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
    if (new_gfi.width == 0 || new_gfi.height == 0) {
        new_gfi.width = new_gfi.height = 1;
        Gif_CreateUncompressedImage(&new_gfi, 0);
        new_gfi.image_data[0] = 0;
        new_gfi.transparent = 0;
        new_gfi.disposal = GIF_DISPOSAL_ASIS;
    } else {
        if (was_compressed)
            Gif_UncompressImage(gfi);
        Gif_CreateUncompressedImage(&new_gfi, 0);
#if 0
        scale_image_data_trivial(sctx, &new_gfi);
#else
        if (new_gfi.width >= gfi->width && new_gfi.height >= gfi->height)
            scale_image_data_trivial(sctx, &new_gfi);
        else
            scale_image_data_interp(sctx, &new_gfi);
#endif
    }

    Gif_ReleaseUncompressedImage(gfi);
    Gif_ReleaseCompressedImage(gfi);
    *gfi = new_gfi;
    if (was_compressed) {
        Gif_FullCompressImage(sctx->gfs, gfi, &gif_write_info);
        Gif_ReleaseUncompressedImage(gfi);
    }
}

void
resize_stream(Gif_Stream *gfs, double new_width, double new_height, int fit)
{
    double xfactor, yfactor;
    uint16_t* xyarr;
    scale_context sctx;
    int i, nw, nh;

    Gif_CalculateScreenSize(gfs, 0);

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
    xfactor = (double) nw / gfs->screen_width;
    yfactor = (double) nh / gfs->screen_height;

    if (fit && nw >= gfs->screen_width && nh >= gfs->screen_height)
        return;
    else if (fit && xfactor < yfactor) {
        nh = (int) (gfs->screen_height * xfactor + 0.5);
        yfactor = (double) nh / gfs->screen_height;
    } else if (fit && yfactor < xfactor) {
        nw = (int) (gfs->screen_width * yfactor + 0.5);
        xfactor = (double) nw / gfs->screen_width;
    }

    xyarr = Gif_NewArray(uint16_t, gfs->screen_width + gfs->screen_height + 2);
    for (i = 0; i != gfs->screen_width; ++i)
        xyarr[i] = (int) (i * xfactor);
    xyarr[gfs->screen_width] = nw;
    for (i = 0; i != gfs->screen_height; ++i)
        xyarr[gfs->screen_width + 1 + i] = (int) (i * yfactor);
    xyarr[gfs->screen_width + 1 + gfs->screen_height] = nh;

    scale_context_init(&sctx, gfs, &xyarr[0], &xyarr[gfs->screen_width+1]);

    for (i = 0; i < gfs->nimages; i++) {
        sctx.gfi = gfs->images[i];
        scale_image(&sctx);
    }

    scale_context_cleanup(&sctx);
    Gif_DeleteArray(xyarr);
    gfs->screen_width = nw;
    gfs->screen_height = nh;
}
