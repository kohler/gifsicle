/* xform.c - Image transformation functions for gifsicle.
   Copyright (C) 1997-9 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2 or later; you can copy, distribute, or alter it at will, as long
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
    fprintf(f, "%d %d %d\n", col[i].red, col[i].green, col[i].blue);
  
  errno = 0;
  status = pclose(f);
  if (status < 0) {
    error("color transformation error: %s", strerror(errno));
    goto done;
  } else if (status > 0) {
    error("color transformation command failed");
    goto done;
  }
  
  f = fopen(tmp_file, "r");
  if (!f || feof(f)) {
    error("color transformation command generated no output", command);
    if (f) fclose(f);
    goto done;
  }
  new_cm = read_colormap_file("<color transformation>", f);
  fclose(f);
  
  if (new_cm) {
    int nc = new_cm->ncol;
    if (nc < gfcm->ncol) {
      nc = gfcm->ncol;
      warning("too few colors in color transformation results");
    } else if (nc > gfcm->ncol)
      warning("too many colors in color transformation results");
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

int
crop_image(Gif_Image *gfi, Gt_Crop *crop, int first_image)
{
  int x, y, w, h, j;
  uint8_t **img;
  
  x = crop->x - gfi->left;
  y = crop->y - gfi->top;
  w = crop->w;
  h = crop->h;
  
  /* Check that the rectangle actually intersects with the image. */
  if (x < 0)
      w += x, x = 0;
  if (y < 0)
      h += y, y = 0;
  if (x + w > gfi->width)
      w = gfi->width - x;
  if (y + h > gfi->height)
      h = gfi->height - y;

  /* Remove transparent edges if required. */
  if (w > 0 && h > 0 && crop->transparent_edges && gfi->transparent >= 0) {
    img = gfi->img;
    /* left edge */
    while (w > 0) {
      for (j = y; j < y + h; j++)
	if (img[j][x] != gfi->transparent)
	  goto found_left_edge;
      x++, w--;
    }
   found_left_edge:
    /* top edge */
    while (h > 0) {
      for (j = x; j < x + w; j++)
	if (img[y][j] != gfi->transparent)
	  goto found_top_edge;
      y++, h--;
    }
   found_top_edge:
    /* right edge */
    while (w > 0) {
      for (j = y; j < y + h; j++)
	if (img[j][x + w - 1] != gfi->transparent)
	  goto found_right_edge;
      w--;
    }
   found_right_edge:
    /* bottom edge */
    while (h > 0) {
      for (j = x; j < x + w; j++)
	if (img[y + h - 1][j] != gfi->transparent)
	  goto found_bottom_edge;
      h--;
    }
   found_bottom_edge: ;
  }
  
  if (w > 0 && h > 0) {
    img = Gif_NewArray(uint8_t *, h + 1);
    for (j = 0; j < h; j++)
      img[j] = gfi->img[y + j] + x;
    img[h] = 0;

    gfi->left += x - crop->left_offset;
    gfi->top += y - crop->top_offset;
    
  } else if (first_image) {
      w = h = 1;
      img = Gif_NewArray(uint8_t *, h + 1);
      img[0] = gfi->img[0];
      img[1] = 0;
      gfi->transparent = img[0][0];
      
  } else {
    /* Empty image */
    w = h = 0;
    img = 0;
  }
  
  Gif_DeleteArray(gfi->img);
  gfi->img = img;
  gfi->width = w;
  gfi->height = h;
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

#define SCALE(d)	((d) << 10)
#define UNSCALE(d)	((d) >> 10)
#define SCALE_FACTOR	SCALE(1)

void
scale_image(Gif_Stream *gfs, Gif_Image *gfi, double xfactor, double yfactor)
{
  uint8_t *new_data;
  int new_left, new_top, new_right, new_bottom, new_width, new_height;
  int was_compressed = (gfi->img == 0);
  
  int i, j, new_x, new_y;
  int scaled_xstep, scaled_ystep, scaled_new_x, scaled_new_y;

  /* Fri 9 Jan 1999: Fix problem with resizing animated GIFs: we scaled from
     left edge of the *subimage* to right edge of the subimage, causing
     consistency problems when several subimages overlap. Solution: always use
     scale factors relating to the *whole image* (the screen size). */
  
  /* use fixed-point arithmetic */
  scaled_xstep = (int)(SCALE_FACTOR * xfactor + 0.5);
  scaled_ystep = (int)(SCALE_FACTOR * yfactor + 0.5);
  
  /* calculate new width and height based on the four edges (left, right, top,
     bottom). This is better than simply multiplying the width and height by
     the scale factors because it avoids roundoff inconsistencies between
     frames on animated GIFs. Don't allow 0-width or 0-height images; GIF
     doesn't support them well. */
  new_left = UNSCALE(scaled_xstep * gfi->left);
  new_top = UNSCALE(scaled_ystep * gfi->top);
  new_right = UNSCALE(scaled_xstep * (gfi->left + gfi->width));
  new_bottom = UNSCALE(scaled_ystep * (gfi->top + gfi->height));
  
  new_width = new_right - new_left;
  new_height = new_bottom - new_top;

  if (new_width <= 0) new_width = 1, new_right = new_left + 1;
  if (new_height <= 0) new_height = 1, new_bottom = new_top + 1;
  if (new_width > UNSCALE(INT_MAX) || new_height > UNSCALE(INT_MAX))
    fatal_error("new image size is too big for me to handle");
  
  if (was_compressed)
    Gif_UncompressImage(gfi);
  new_data = Gif_NewArray(uint8_t, new_width * new_height);
  
  new_y = new_top;
  scaled_new_y = scaled_ystep * gfi->top;
  
  for (j = 0; j < gfi->height; j++) {
    uint8_t *in_line = gfi->img[j];
    uint8_t *out_data;
    int x_delta, y_delta, yinc;
    
    scaled_new_y += scaled_ystep;
    /* account for images which should've had 0 height but don't */
    if (j == gfi->height - 1) scaled_new_y = SCALE(new_bottom);
    
    if (scaled_new_y < SCALE(new_y + 1)) continue;
    y_delta = UNSCALE(scaled_new_y - SCALE(new_y));
    
    new_x = new_left;
    scaled_new_x = scaled_xstep * gfi->left;
    out_data = &new_data[(new_y - new_top) * new_width + (new_x - new_left)];
    
    for (i = 0; i < gfi->width; i++) {
      scaled_new_x += scaled_xstep;
      /* account for images which should've had 0 width but don't */
      if (i == gfi->width - 1) scaled_new_x = SCALE(new_right);
      
      x_delta = UNSCALE(scaled_new_x - SCALE(new_x));
      
      for (; x_delta > 0; new_x++, x_delta--, out_data++)
	for (yinc = 0; yinc < y_delta; yinc++)
	  out_data[yinc * new_width] = in_line[i];
    }
    
    new_y += y_delta;
  }
  
  Gif_ReleaseUncompressedImage(gfi);
  Gif_ReleaseCompressedImage(gfi);
  gfi->width = new_width;
  gfi->height = new_height;
  gfi->left = UNSCALE(scaled_xstep * gfi->left);
  gfi->top = UNSCALE(scaled_ystep * gfi->top);
  Gif_SetUncompressedImage(gfi, new_data, Gif_DeleteArrayFunc, 0);
  if (was_compressed) {
    Gif_FullCompressImage(gfs, gfi, gif_write_flags);
    Gif_ReleaseUncompressedImage(gfi);
  }
}

void
resize_stream(Gif_Stream *gfs, int new_width, int new_height)
{
  double xfactor, yfactor;
  int i;

  if (new_width <= 0)
    new_width = (int)(((double)gfs->screen_width / gfs->screen_height) * new_height);
  if (new_height <= 0)
    new_height = (int)(((double)gfs->screen_height / gfs->screen_width) * new_width);
  
  Gif_CalculateScreenSize(gfs, 0);
  xfactor = (double)new_width / gfs->screen_width;
  yfactor = (double)new_height / gfs->screen_height;
  for (i = 0; i < gfs->nimages; i++)
    scale_image(gfs, gfs->images[i], xfactor, yfactor);
  
  gfs->screen_width = new_width;
  gfs->screen_height = new_height;
}
