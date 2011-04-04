/* merge.c - Functions which actually combine and manipulate GIF image data.
   Copyright (C) 1997-2011 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software; you can copy, distribute, or alter it at will, as
   long as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
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


static void
mark_used_colors(Gif_Image *gfi, Gif_Colormap *gfcm)
{
  Gif_Color *col = gfcm->col;
  int ncol = gfcm->ncol;
  uint8_t have[256];
  int i, j, total;

  /* Only mark colors until we've seen all of them. The total variable keeps
     track of how many we've seen. have[i] is true if we've seen color i */
  for (i = 0; i < ncol; i++)
    have[i] = 0;
  for (i = ncol; i < 256; i++)
    have[i] = 1;
  total = 256 - ncol;

  /* Loop over every pixel (until we've seen all colors) */
  for (j = 0; j < gfi->height && total < 256; j++) {
    uint8_t *data = gfi->img[j];
    for (i = 0; i < gfi->width; i++, data++)
      if (!have[*data]) {
	have[*data] = 1;
	total++;
      }
  }

  /* Mark the colors we've found */
  for (i = 0; i < ncol; i++)
    col[i].haspixel = have[i];

  /* Mark the transparent color specially */
  if (gfi->transparent >= 0 && gfi->transparent < ncol)
    col[gfi->transparent].haspixel = 2;
  /* 1.Aug.1999 - DO NOT clear transparent index if gfi->transparent isn't
     within bounds! Some smart GIF optimizers will write a small colormap, not
     including the transparent color. */
  /* 19.Aug.1999 - Don't need to set col[gfi->transparent].haspixel to 2 even
     if it's not within bounds! */
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
  int dest_userflags = dest->userflags;
  int i, x;
  int trivial_map = 1;

  for (i = 0; i < src->ncol; i++) {
    if (srccol[i].haspixel == 1) {
      /* Store an image color cell's mapping to the global colormap in its
	 'pixel' slot. This is useful caching: oftentimes many input frames
	 will share a colormap */
      int mapto = (srccol[i].pixel < 256 ? (int)srccol[i].pixel : -1);

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

      if (mapto == -1)
	/* give up and require a local colormap */
	goto local_colormap_required;

      assert(mapto >= 0 && mapto < ndestcol);
      assert(GIF_COLOREQ(&destcol[mapto], &srccol[i]));

      srccol[i].pixel = mapto;
      destcol[mapto].haspixel = 1;
      if (mapto != i)
	trivial_map = 0;

    } else if (srccol[i].haspixel == 2)
      /* a dedicated transparent color; if trivial_map & at end of colormap
         insert it with haspixel == 2. (strictly not necessary; we do it to
	 try to keep the map trivial.) */
      if (trivial_map && i == ndestcol) {
	destcol[ndestcol] = srccol[i];
	ndestcol++;
      }
  }

  /* success! save new number of colors */
  dest->ncol = ndestcol;
  dest->userflags = dest_userflags;
  return 1;

  /* failure: a local colormap is required */
 local_colormap_required:
  if (warn_local_colormaps == 1) {
    static int context = 0;
    warning(1, "so many colors that local colormaps were required");
    if (!context)
      warncontext(1, "(You may want to try '--colors 256'.)");
    warn_local_colormaps = 2;
    context = 1;
  }

  /* 9.Dec.1998 - This must have been a longstanding bug! We MUST clear
     the cached mappings of any pixels in the source colormap we
     assigned this time through, since we are throwing those colors
     away. We assigned it this time through if the cached mapping is >=
     dest->ncol. */
  for (x = 0; x < i; x++)
    if (srccol[x].haspixel == 1 && srccol[x].pixel >= (uint32_t)dest->ncol)
      srccol[x].pixel = 256;

  return 0;
}


void
merge_stream(Gif_Stream *dest, Gif_Stream *src, int no_comments)
{
  int i;
  assert(dest->global);

  /* unmark colors in global and local colormaps -- 12/9 */
  if (src->global)
    unmark_colors_2(src->global);
  for (i = 0; i < src->nimages; i++)
    if (src->images[i]->local)
      unmark_colors_2(src->images[i]->local);

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
merge_image(Gif_Stream *dest, Gif_Stream *src, Gif_Image *srci,
	    int same_compressed_ok)
{
  Gif_Colormap *imagecm;
  Gif_Color *imagecol;
  int islocal;
  int i;
  Gif_Colormap *localcm = 0;
  Gif_Colormap *destcm = dest->global;

  uint8_t map[256];		/* map[input pixel value] == output pixval */
  int trivial_map = 1;		/* does the map take input pixval --> the same
				   pixel value for all colors in the image? */
  uint8_t used[256];		/* used[output pixval K] == 1 iff K was used
				   in the image */

  Gif_Image *desti;

  /* mark colors that were actually used in this image */
  islocal = srci->local != 0;
  imagecm = islocal ? srci->local : src->global;
  if (!imagecm)
    fatal_error("no global or local colormap for source image");

  mark_used_colors(srci, imagecm);
  imagecol = imagecm->col;	/* may be changed by mark_used_colors */

  /* map[old_pixel_value] == new_pixel_value */
  for (i = 0; i < 256; i++)
    map[i] = used[i] = 0;

  /* Merge the colormap */
  if (merge_colormap_if_possible(dest->global, imagecm)) {
    /* Create 'map' and 'used' for global colormap. */
    for (i = 0; i < imagecm->ncol; i++)
      if (imagecol[i].haspixel == 1) {
	map[i] = imagecol[i].pixel;
	if (map[i] != i) trivial_map = 0;
	used[map[i]] = 1;
      }

  } else {
    /* Need a local colormap. */
    int ncol = 0;
    destcm = localcm = Gif_NewFullColormap(0, 256);
    for (i = 0; i < imagecm->ncol; i++)
      if (imagecol[i].haspixel) {
	map[i] = ncol;
	if (ncol != i) trivial_map = 0;
	/* 19.Aug.1999- BUGFIX! color is only used if it was opaque */
	if (imagecol[i].haspixel == 1)
	  used[ncol] = 1;
	localcm->col[ ncol++ ] = imagecol[i];
      }
    localcm->ncol = ncol;
  }

  /* Decide on a transparent index */
  if (srci->transparent >= 0) {
    int found_transparent = -1;

    /* try to keep the map trivial -- prefer same transparent index */
    if (trivial_map && !used[srci->transparent])
      found_transparent = srci->transparent;
    else
      for (i = destcm->ncol - 1; i >= 0; i--)
	if (!used[i])
	  found_transparent = i;

    /* 1.Aug.1999 - Allow for the case that the transparent index is bigger
       than the number of colors we've created thus far. */
    if (found_transparent < 0 || found_transparent >= destcm->ncol) {
      Gif_Color *c;
      found_transparent = destcm->ncol;
      /* 1.Aug.1999 - Don't update destcm->ncol -- we want the output colormap
         to be as small as possible. */
      c = &destcm->col[found_transparent];
      if (srci->transparent < imagecm->ncol)
	*c = imagecol[srci->transparent];
      else
	c->haspixel = 2;
      assert(c->haspixel == 2 && found_transparent < 256);
    }

    map[srci->transparent] = found_transparent;
    if (srci->transparent != found_transparent) trivial_map = 0;
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

  if (trivial_map && same_compressed_ok && srci->compressed) {
    desti->compressed_len = srci->compressed_len;
    desti->compressed = Gif_NewArray(uint8_t, srci->compressed_len);
    desti->free_compressed = Gif_DeleteArrayFunc;
    memcpy(desti->compressed, srci->compressed, srci->compressed_len);
  } else {
    int i, j;
    Gif_CreateUncompressedImage(desti);

    if (trivial_map)
      for (j = 0; j < desti->height; j++)
	memcpy(desti->img[j], srci->img[j], desti->width);

    else
      for (j = 0; j < desti->height; j++) {
	uint8_t *srcdata = srci->img[j];
	uint8_t *destdata = desti->img[j];
	for (i = 0; i < desti->width; i++, srcdata++, destdata++)
	  *destdata = map[*srcdata];
      }
  }

  Gif_AddImage(dest, desti);
  return desti;
}
