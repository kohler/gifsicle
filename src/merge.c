/* merge.c - Functions which actually combine and manipulate GIF image data.
   Copyright (C) 1997-8 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software; you can copy, distribute, or alter it at will, as
   long as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include "gifsicle.h"
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* First merging stage: Mark the used colors in all colormaps. */

void
unmark_colors(Gif_Colormap *gfcm)
{
  int i;
  if (gfcm)
    for (i = 0; i < gfcm->ncol; i++)
      gfcm->col[i].haspixel = 0;
}


void
unmark_colors_2(Gif_Colormap *gfcm)
{
  int i;
  for (i = 0; i < gfcm->ncol; i++)
    gfcm->col[i].pixel = 256;
}


void
mark_used_colors(Gif_Stream *gfs, Gif_Image *gfi)
{
  Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
  Gif_Color *col;
  int ncol;
  byte have[256];
  int i, j, total, old_total;
  
  if (!gfcm)
    fatal_error("no global or local colormap for source image");
  col = gfcm->col;
  ncol = gfcm->ncol;
  
  /* Only mark colors until we've seen all of them. The total variable keeps
     track of how many we've seen. have[i] is true if we've seen color i */
  total = 0;
  for (i = 0; i < ncol; i++)
    have[i] = 0;
  for (i = ncol; i < 256; i++)
    have[i] = 1;
  
  /* Make sure the transparent color is within bounds. */
  if (gfi->transparent >= ncol)
    gfi->transparent = -1;
  if (gfi->transparent >= 0 && !have[gfi->transparent]) {
    have[gfi->transparent] = 1;
    total++;
    col[gfi->transparent].haspixel = 2;
  }
  
  old_total = total;
  
  for (j = 0; j < gfi->height && total < ncol; j++) {
    byte *data = gfi->img[j];
    for (i = 0; i < gfi->width; i++, data++)
      if (!have[*data]) {
	have[*data] = 1;
	total++;
      }
  }
  
  /* If we found any new colors, mark them down. But don't mark the
     transparent color */
  if (old_total < total)
    for (i = 0; i < ncol; i++)
      if (have[i] && i != gfi->transparent)
	col[i].haspixel = 1;
}


int
find_color_index(Gif_Color *c, int nc, Gif_Color *color)
{
  int index;
  for (index = 0; index < nc; index++)
    if (GIF_COLOREQ(&c[index], color))
      return index;
  return -1;
}


int
merge_colormap_if_possible(Gif_Colormap *dest, Gif_Colormap *src)
{
  Gif_Color *srccol = src->col;
  Gif_Color *destcol = dest->col;
  int ndestcol = dest->ncol;
  int i, x;
  int trivial_map = 1;
  
  for (i = 0; i < src->ncol; i++)
    if (srccol[i].haspixel == 1) {
      int mapto = srccol[i].pixel >= ndestcol ? -1 : srccol[i].pixel;
      if (mapto == -1)
	mapto = find_color_index(destcol, ndestcol, &srccol[i]);
      if (mapto == -1 && ndestcol < 256) {
	/* add the color */
	mapto = ndestcol;
	destcol[mapto] = srccol[i];
	ndestcol++;
      }
      if (mapto == -1)
	/* check for a pure-transparent color */
	for (x = 0; x < ndestcol; x++)
	  if (destcol[x].haspixel == 2) {
	    mapto = x;
	    destcol[mapto] = srccol[i];
	    break;
	  }
      if (mapto == -1) {
	/* local colormap required */
	if (warn_local_colormaps == 1) {
	  warning("too many colors, had to use some local colormaps");
	  warning("  (you may want to try `--colors 256')");
	  warn_local_colormaps = 2;
	}
	return 0;
      }
      
      if (mapto != i)
	trivial_map = 0;
      assert(mapto >= 0 && mapto < ndestcol);
      srccol[i].pixel = mapto;
      destcol[mapto].haspixel = 1;
      
    } else if (srccol[i].haspixel == 2)
      /* a dedicated transparent color; if trivial_map & at end of colormap
         insert it with haspixel == 2. (strictly not necessary; we do it to
	 try to keep the map trivial.) */
      if (trivial_map && i == ndestcol) {
	destcol[ndestcol] = srccol[i];
	ndestcol++;
      }
  
  dest->ncol = ndestcol;
  return 1;
}


void
merge_stream(Gif_Stream *dest, Gif_Stream *src, int no_comments)
{
  assert(dest->global);
  
  if (src->global)
    unmark_colors_2(src->global);
  
  if (dest->loopcount < 0)
    dest->loopcount = src->loopcount;
  
  if (src->comment && !no_comments) {
    if (!dest->comment) dest->comment = Gif_NewComment();
    merge_comments(dest->comment, src->comment);
  }
}


void
merge_comments(Gif_Comment *destc, Gif_Comment *srcc)
{
  int i;
  for (i = 0; i < srcc->count; i++)
    Gif_AddComment(destc, srcc->str[i], srcc->len[i]);
}


Gif_Image *
merge_image(Gif_Stream *dest, Gif_Stream *src, Gif_Image *srci)
{
  Gif_Colormap *imagecm;
  Gif_Color *imagecol;
  int delete_imagecm = 0;
  int islocal;
  int i;
  Gif_Colormap *localcm = 0;
  Gif_Colormap *destcm = dest->global;
  
  byte map[256];
  byte have[256];
  
  Gif_Image *desti;
  
  /* mark colors that were actually used in this image */
  islocal = srci->local != 0;
  imagecm = islocal ? srci->local : src->global;
  if (!imagecm)
    fatal_error("no global or local colormap for source image");
  imagecol = imagecm->col;
  
  unmark_colors(imagecm);
  mark_used_colors(src, srci);
  
  for (i = 0; i < 256; i++)
    map[i] = have[i] = 0;
  
  if (!merge_colormap_if_possible(dest->global, imagecm)) {
    /* Need a local colormap. */
    int ncol = 0;
    destcm = localcm = Gif_NewFullColormap(0, 256);
    for (i = 0; i < imagecm->ncol; i++)
      if (imagecol[i].haspixel == 1) {
	map[i] = ncol;
	have[i] = 1;
	localcm->col[ ncol++ ] = imagecm->col[i];
      }
    localcm->ncol = ncol;
  } else
    for (i = 0; i < imagecm->ncol; i++)
      if (imagecol[i].haspixel == 1) {
	map[i] = imagecol[i].pixel;
	have[i] = 1;
      }
  
  if (srci->transparent > -1) {
    byte rev_have[256];
    int found_transparent = -1;
    for (i = 0; i < 256; i++)
      rev_have[i] = 0;
    for (i = 0; i < 256; i++)
      rev_have[map[i]] = 1;
    for (i = destcm->ncol - 1; i >= 0; i--)
      if (!rev_have[i]) {
	found_transparent = i;
	if (i == srci->transparent) break; /* prefer same transparent index */
      }
    if (found_transparent < 0) {
      Gif_Color *c;
      found_transparent = destcm->ncol++;
      c = &destcm->col[found_transparent];
      c->red = c->green = c->blue = 0;
      c->haspixel = 2;
    }
    map[srci->transparent] = found_transparent;
    have[srci->transparent] = 1;
  }

  assert(destcm->ncol <= 256);
  /* Make the new image. */
  desti = Gif_NewImage();
  
  desti->identifier = Gif_CopyString(srci->identifier);
  if (srci->transparent > -1)
    desti->transparent = map[srci->transparent];
  desti->delay = srci->delay;
  desti->disposal = srci->disposal;
  desti->left = srci->left;
  desti->top = srci->top;
  desti->interlace = srci->interlace;
  
  desti->width = srci->width;
  desti->height = srci->height;
  desti->local = localcm;
  
  if (srci->comment) {
    desti->comment = Gif_NewComment();
    merge_comments(desti->comment, srci->comment);
  }
  
  Gif_CreateUncompressedImage(desti);
  
  {
    int i, j, trivial_map = 1;
    /* check for a trivial map */
    for (i = 0; i < 256 && trivial_map; i++)
      if (map[i] != i && have[i])
	trivial_map = 0;

    if (trivial_map)
      for (j = 0; j < desti->height; j++)
	memcpy(desti->img[j], srci->img[j], desti->width);
    
    else
      for (j = 0; j < desti->height; j++) {
	byte *srcdata = srci->img[j];
	byte *destdata = desti->img[j];
	for (i = 0; i < desti->width; i++, srcdata++, destdata++)
	  *destdata = map[*srcdata];
      }
  }
  
  Gif_AddImage(dest, desti);
  if (delete_imagecm)
    Gif_DeleteColormap(imagecm);
  return desti;  
}


/********************************************************/

/*****
 * crop image; returns true if the image exists
 **/

int
crop_image(Gif_Image *gfi, Gt_Crop *crop)
{
  int x, y, w, h, j;
  byte **img;
  
  if (!crop->ready) {
    crop->x = crop->spec_x + gfi->left;
    crop->y = crop->spec_y + gfi->top;
    crop->w = crop->spec_w <= 0 ? gfi->width + crop->spec_w : crop->spec_w;
    crop->h = crop->spec_h <= 0 ? gfi->height + crop->spec_h : crop->spec_h;
    if (crop->x < 0 || crop->y < 0 || crop->w <= 0 || crop->h <= 0
	|| crop->x + crop->w > gfi->width
	|| crop->y + crop->h > gfi->height) {
      error("cropping dimensions don't fit image");
      crop->ready = 2;
    } else
      crop->ready = 1;
  }
  if (crop->ready == 2)
    return 1;
  
  x = crop->x - gfi->left;
  y = crop->y - gfi->top;
  w = crop->w;
  h = crop->h;
  
  /* Check that the rectangle actually intersects with the image. */
  if (x < 0) w += x, x = 0;
  if (y < 0) h += y, y = 0;
  if (x + w > gfi->width) w = gfi->width - x;
  if (y + h > gfi->height) h = gfi->height - y;
  
  if (w > 0 && h > 0) {
    img = Gif_NewArray(byte *, h + 1);
    for (j = 0; j < h; j++)
      img[j] = gfi->img[y + j] + x;
    img[h] = 0;
    
    /* Change position of image appropriately */
    if (crop->whole_stream) {
      /* If cropping the whole stream, then this is the first frame. Position
	 it at (0,0). */
      crop->left_off = x + gfi->left;
      crop->right_off = y + gfi->top;
      gfi->left = 0;
      gfi->top = 0;
      crop->whole_stream = 0;
    } else {
      gfi->left += x - crop->left_off;
      gfi->top += y - crop->right_off;
    }
    
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


/******
 * color changes
 **/

static void
apply_color_change_to_cmap(Gif_Colormap *gfcm, Gt_ColorChange *cc)
{
  int i;
  for (i = 0; i < gfcm->ncol; i++)
    if (gfcm->col[i].red == cc->old_color.red
	&& gfcm->col[i].green == cc->old_color.green
	&& gfcm->col[i].blue == cc->old_color.blue)
      gfcm->col[i] = cc->new_color;
}


void
apply_color_changes(Gif_Stream *gfs, Gt_ColorChange *cc)
{
  int i;
  if (!cc) return;
  if (gfs->global)
    apply_color_change_to_cmap(gfs->global, cc);
  for (i = 0; i < gfs->nimages; i++)
    if (gfs->images[i]->local)
      apply_color_change_to_cmap(gfs->images[i]->local, cc);
  apply_color_changes(gfs, cc->next);
}
