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
  
  /* Don't actually set the screen width & height. This won't actually result
     in bad behavior when extracting a frame from an image, since `left' and
     `top' will still be available. */
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
  
  desti->image_data = Gif_NewArray(byte, desti->width * desti->height);
  desti->free_image_data = Gif_DeleteArrayFunc;
  Gif_MakeImg(desti, desti->image_data, desti->interlace);
  
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


/********************************************************
 * Optimizing images
 **/


#if 0
/* shrink_difference_border: Make the difference border as small as possible
   in the case that the transparent values from the last frame and this frame
   are different. This requires slightly slower code. Called from
   find_difference_border */

static void
shrink_difference_border(byte *last_data, byte *this_data,
			 int screen_width, int screen_height,
			 int *lf, int *rt, int *tp, int *bt,
			 int last_transparent, int this_transparent)
{
  int last_map[256], this_map[256];
  int flf = *lf, frt = *rt, ftp = *tp, fbt = *bt, x, y;
  frt--, fbt--;
  
  for (x = 0; x < 256; x++) {
    last_map[x] = (x == last_transparent ? -1 : x);
    this_map[x] = (x == this_transparent ? -1 : x);
  }

  /* shrink each side in turn */
  for (; ftp < fbt; ftp++) {
    byte *ld = last_data + screen_width * ftp;
    byte *td = this_data + screen_width * ftp;
    for (x = flf; x <= frt; x++)
      if (last_map[ld[x]] != this_map[td[x]])
	goto top_stop;
  }
 top_stop:
  
  for (; fbt > ftp; fbt--) {
    byte *ld = last_data + screen_width * fbt;
    byte *td = this_data + screen_width * fbt;
    for (x = flf; x <= frt; x++)
      if (last_map[ld[x]] != this_map[td[x]])
	goto bottom_stop;
  }
 bottom_stop:
  
  for (; flf < frt; flf++) {
    u_int32_t off = ftp * screen_width + flf;
    for (y = ftp; y <= fbt; y++, off += screen_width)
      if (last_map[last_data[off]] != this_map[this_data[off]])
	goto left_stop;
  }
 left_stop:
  
  for (; frt > flf; frt--) {
    u_int32_t off = ftp * screen_width + frt;
    for (y = ftp; y <= fbt; y++, off += screen_width)
      if (last_map[last_data[off]] != this_map[this_data[off]])
	goto right_stop;
  }
 right_stop:
  
  if (flf >= frt) flf = frt = 0;
  if (ftp >= fbt) ftp = fbt = 0;
  
  /* Don't allow a 0x0 image. */    
  *lf = flf;
  *rt = frt + 1;
  *tp = ftp;
  *bt = fbt + 1;
}


/* find_difference_border: Find the smallest rectangular area containing all
   the changes. Uses the helper shrink_difference_border for the slower
   uncommon case (different transparent pixels). */

static void
find_difference_border(byte *last_data, byte *this_data,
		       int screen_width, int screen_height,
		       int *lf, int *rt, int *tp, int *bt,
		       int last_transparent, int this_transparent)
{
  int flf, frt, ftp, fbt, x, y;
  
  for (ftp = 0; ftp < screen_height; ftp++)
    if (memcmp(last_data + screen_width * ftp, this_data + screen_width * ftp,
	       screen_width) != 0)
      break;
  for (fbt = screen_height - 1; fbt >= 0; fbt--)
    if (memcmp(last_data + screen_width * fbt, this_data + screen_width * fbt,
	       screen_width) != 0)
      break;
  
  flf = screen_width;
  frt = 0;
  for (y = ftp; y <= fbt; y++) {
    byte *ld = last_data + screen_width * y;
    byte *td = this_data + screen_width * y;
    for (x = 0; x < flf; x++)
      if (ld[x] != td[x])
	break;
    flf = x;
    
    for (x = screen_width - 1; x > frt; x--)
      if (ld[x] != td[x]) {
	if (x == 466) fprintf(stderr, "  %d >> %d\n", ld[x], td[x]);
	break;}
    frt = x;
  }
  
  if (flf >= frt) flf = frt = 0;
  if (ftp >= fbt) ftp = fbt = 0;
  
  /* Don't allow a 0x0 image. */    
  *lf = flf;
  *rt = frt + 1;
  *tp = ftp;
  *bt = fbt + 1;

  fprintf(stderr, "%d-%d\n", last_transparent, this_transparent);
  if (last_transparent != 0 && this_transparent != 0
      && last_transparent != this_transparent)
    shrink_difference_border(last_data, this_data, screen_width, screen_height,
			     lf, rt, tp, bt,
			     last_transparent, this_transparent);
}


static int
set_background_disposal(Gif_Stream *gfs, int imageno, byte *last_data,
			int old_transparent)
{
  Gif_Image *gfi = gfs->images[imageno - 1];
  int bottom = gfi->top + gfi->height;
  int y;

  /*if (gfi->disposal == GIF_DISPOSAL_BACKGROUND) return 0;*/
  
  gfi->disposal = GIF_DISPOSAL_BACKGROUND;
  for (y = gfi->top; y < bottom; y++) {
    byte *move = last_data + gfs->screen_width * y + gfi->left;
    memset(move, old_transparent, gfi->width);
  }
  return 1;
}


static int
look_for_transparent(Gif_Stream *gfs,
		     byte *this_data, byte *last_data,
		     int left, int right, int top, int bottom)
{
  int screen_width = gfs->screen_width;
  int ncol = gfs->global->ncol;
  int bad[256];
  int i, x, y;
  
  for (i = 0; i < 256; i++) bad[i] = 0;
  
  for (y = top; y < bottom; y++) {
    byte *td = this_data + screen_width * y + left;
    byte *ld = last_data + screen_width * y + left;
    for (x = left; x < right; x++, td++, ld++)
      if (*td != *ld)
	bad[*td] = 1;
  }
  
  for (i = 0; i < ncol; i++)
    if (!bad[i])
      return i;
  /* If no unused pixel works for transparency, add a pixel if we can. */
  if (ncol < 256) {
    gfs->global->ncol++;
    return ncol;
  }
  return -1;
}


static void
copy_trans_fragment_data(byte *data, byte *thisdata, byte *lastdata,
			 int left, int right, int top, int bottom,
			 int screen_width, int transparent)
{
  int x, y;
  int color_used[256];

  for (x = 0; x < 256; x++)
    color_used[x] = 0;
  
  /*
    Use transparency if possible to shrink the size of the written GIF.
    
    The written GIF will be small if patterns (sequences of pixel values)
    recur in the image.
    We could conceivably use transparency to produce THE OPTIMAL image,
    with the most recurring patterns of the best kinds; but this would
    be very hard (wouldn't it?). Instead, we settle for a heuristic:
    we try and create RUNS. (Since we *try* to create them, they will
    presumably recur!) A RUN is a series of adjacent pixels all with the
    same value.
    
    By & large, we just use the regular image's values. However, we might
    create a transparent run *not in* the regular image, if TWO OR MORE
    adjacent runs OF DIFFERENT COLORS *could* be made transparent.
    
    (An area can be made transparent if the corresponding area in the previous
    frame had the same colors as the area does now.)
    
    Why? If only one run (say of color C) could be transparent, we get no
    large immediate advantage from making it transparent (it'll be a run of
    the same length regardless). Also, we might LOSE: what if the run was
    adjacent to some more of color C, which couldn't be made transparent? If
    we use color C (instead of the transparent color), then we get a longer
    run.
    
    This simple heuristic does a little better than Gifwizard's (6/97)
    on some images, but does *worse than nothing at all* on others. FIXME. */
  
  for (y = top; y < bottom; y++) {
    byte *from = lastdata + screen_width * y + left;
    byte *into = thisdata + screen_width * y + left;
    int lastpixel = *from;
    int pixelcount;
    
    x = left;
    
   normal_runs:
    /* Copy pixels (no transparency); measure runs which could be
       made transparent. */
    pixelcount = 0;
    for (; x < right; x++, from++, into++, data++) {
      *data = *into;
      
      /* If this pixel could be transparent... */
      if (*into == *from) {
	if (color_used[*data] == 0)
	  /* always transparentize infrequently used colors */
	  goto combine_transparent_runs;
	else if (*into == lastpixel)
	  /* within a transparent run */
	  pixelcount++;
	else if (pixelcount > 0)
	  /* Ooo!! enough adjacent transparent runs -- go & combine them */
	  goto combine_transparent_runs;
	else {
	  /* starting a new transparent run */
	  lastpixel = *into;
	  pixelcount++;
	}
	
      } else {
	color_used[*data]++;
	pixelcount = 0;
      }
    }
    /* If we get here, we've finished up the row. */
    goto row_finished;
    
   combine_transparent_runs:
    /* Here's where we make transparent runs. */
    memset(data - pixelcount, transparent, pixelcount);
    for (; x < right; x++, from++, into++, data++) {
      if (*from == *into)
	*data = transparent;
      else
	/* If this pixel can't be made transparent, just go back to
	   copying normal runs. */
	goto normal_runs;
    }
    /* If we get here, we've finished up the row. */
    goto row_finished;
    
   row_finished: ;
  }
}


void
optimize_fragments(Gif_Stream *gfs, int optimize_trans)
{
  static int bad_optimize_warned = 0;
  int x, y;
  int imageno;
  int size;
  byte *last_data;
  byte *this_data;
  int screen_width;
  int screen_height;
  int transparent;
  int last_transparent;
  Gif_Image *gfi;
  
  if (gfs->nimages <= 1) return;
  
  /* Call Gif_Unoptimize. Besides unoptimizing the image, it has the pleasant
     side effect of creating all-new contiguous image data for each image.
     This new data is laid out uninterlaced -- i.e., image_data[width*y + x] ==
     img[y][x]. We use this fact below. */
  if (!Gif_Unoptimize(gfs)) {
    warning("could not optimize output");
    if (!bad_optimize_warned) {
      bad_optimize_warned = 1;
      warning("  (The reason was local color tables or odd transparency.");
      warning("  Try running the GIF through `gifsicle --colors=256'.)");
    }
    return;
  }
  
  screen_width = gfs->screen_width;
  screen_height = gfs->screen_height;

  /* Make space for possible new transparent pixels if necessary. */
  Gif_ReArray(gfs->global->col, Gif_Color, 256);
  if (!gfs->global->col) fatal_error("out of memory in optimizer");
  
  /* It's OK to just copy the image data in without worrying about
     interlacing (see above). */
  gfi = gfs->images[0];
  size = screen_width * screen_height;
  last_data = Gif_NewArray(byte, size);
  if (!last_data) fatal_error("out of memory in optimizer");
  memcpy(last_data, gfi->image_data, size);
  
  gfi->disposal = GIF_DISPOSAL_ASIS;
  last_transparent = gfi->transparent;
  
  for (imageno = 1; imageno < gfs->nimages; imageno++) {
    byte *data;
    int left, right, top, bottom;
    
    gfi = gfs->images[imageno];
    this_data = gfi->image_data;
    transparent = gfi->transparent;
    
    find_difference_border(last_data, this_data, screen_width, screen_height,
			   &left, &right, &top, &bottom,
			   last_transparent, transparent);
    
    /* If this image has transparency, see if we need to switch the last frame
       to disposal=background (this must happen if a pixel now is (globally)
       transparent but in the last frame it was opaque). */
    
    if (transparent >= 0)
      for (y = top; y < bottom; y++) {
	byte *lastd = last_data + screen_width * y + left;
	byte *thisd = this_data + screen_width * y + left;
	for (x = left; x < right; x++, thisd++, lastd++)
	  if (*thisd == transparent && *lastd != last_transparent) {
	    set_background_disposal(gfs, imageno, last_data, last_transparent);
	    find_difference_border(last_data, this_data,
				   screen_width, screen_height,
				   &left, &right, &top, &bottom,
				   last_transparent, transparent);
	    goto found_border;
	  }
      }
    
   found_border:
    
    gfi->left = left;
    gfi->top = top;
    gfi->width = right - left;
    gfi->height = bottom - top;
    gfi->interlace = 0;
    gfi->disposal = GIF_DISPOSAL_ASIS;
    
    gfi->image_data = Gif_NewArray(byte, gfi->width * gfi->height);
    data = gfi->image_data;
    if (!data) fatal_error("out of memory in optimizer");
    Gif_MakeImg(gfi, data, 0);
    
    if (optimize_trans && transparent < 0)
      gfi->transparent = transparent =
	look_for_transparent(gfs, this_data, last_data,
			     left, right, top, bottom);
    
    if (optimize_trans && transparent >= 0)
      copy_trans_fragment_data(data, this_data, last_data,
			       left, right, top, bottom,
			       screen_width, transparent);
    
    else
      for (y = top; y < bottom; y++) {
	byte *into = this_data + screen_width * y + left;
	memcpy(data, into, gfi->width);
	data += gfi->width;
      }
    
    Gif_DeleteArray(last_data);
    last_data = this_data;
    last_transparent = transparent;
  }
  
  Gif_DeleteArray(last_data);
}
#endif


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
