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

static void
scale_image(Gif_Stream *gfs, Gif_Image *gfi, uint16_t* xoff, uint16_t* yoff)
{
  uint8_t *new_data, *in_line, *out_data;
  int new_width, new_height;
  int was_compressed = (gfi->img == 0);
  int i, j, k;

  /* Fri 9 Jan 1999: Fix problem with resizing animated GIFs: we scaled from
     left edge of the *subimage* to right edge of the subimage, causing
     consistency problems when several subimages overlap. Solution: always use
     scale factors relating to the *whole image* (the screen size). */

  /* calculate new width and height based on the four edges (left, right, top,
     bottom). This is better than simply multiplying the width and height by
     the scale factors because it avoids roundoff inconsistencies between
     frames on animated GIFs. Don't allow 0-width or 0-height images; GIF
     doesn't support them well. */
  if (xoff[gfi->left + gfi->width] <= xoff[gfi->left]
      || yoff[gfi->top + gfi->height] <= yoff[gfi->top]) {
      new_width = new_height = 1;
      gfi->transparent = 0;
      gfi->disposal = GIF_DISPOSAL_ASIS;
      new_data = Gif_NewArray(uint8_t, 1);
      new_data[0] = 0;
      goto done;
  }

  xoff += gfi->left;
  yoff += gfi->top;
  new_width = xoff[gfi->width] - xoff[0];
  new_height = yoff[gfi->height] - yoff[0];

  if (was_compressed)
    Gif_UncompressImage(gfi);

  new_data = Gif_NewArray(uint8_t, new_width * new_height);
  out_data = new_data;
  for (j = 0; j < gfi->height; ++j)
      if (yoff[j] != yoff[j+1]) {
          in_line = gfi->img[j];
          for (i = 0; i < gfi->width; ++i, ++in_line)
              for (k = xoff[i]; k != xoff[i+1]; ++k, ++out_data)
                  *out_data = *in_line;
          for (i = yoff[j] + 1; i != yoff[j+1]; ++i, out_data += new_width)
              memcpy(out_data, out_data - new_width, new_width);
      }

 done:
  Gif_ReleaseUncompressedImage(gfi);
  Gif_ReleaseCompressedImage(gfi);
  gfi->left = xoff[0];
  gfi->top = yoff[0];
  gfi->width = new_width;
  gfi->height = new_height;
  Gif_SetUncompressedImage(gfi, new_data, Gif_DeleteArrayFunc, 0);
  if (was_compressed) {
    Gif_FullCompressImage(gfs, gfi, &gif_write_info);
    Gif_ReleaseUncompressedImage(gfi);
  }
}

void
resize_stream(Gif_Stream *gfs, double new_width, double new_height, int fit)
{
    double xfactor, yfactor;
    uint16_t* xarr;
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

    xarr = Gif_NewArray(uint16_t, gfs->screen_width + gfs->screen_height + 2);
    for (i = 0; i != gfs->screen_width + 1; ++i)
        xarr[i] = (int) (i * xfactor + 0.5);
    for (i = 0; i != gfs->screen_height + 1; ++i)
        xarr[gfs->screen_width + 1 + i] = (int) (i * yfactor + 0.5);

    for (i = 0; i < gfs->nimages; i++)
        scale_image(gfs, gfs->images[i],
                    &xarr[0], &xarr[gfs->screen_width + 1]);

    Gif_DeleteArray(xarr);
    gfs->screen_width = nw;
    gfs->screen_height = nh;
}
