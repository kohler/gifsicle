/* quantize.c - Histograms and quantization for gifsicle.
   Copyright (C) 1997-8 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software; you can copy, distribute, or alter it at will, as
   long as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include "gifsicle.h"

static void
add_histogram_color(Gif_Color *color, Gif_Color **histo_store,
		    int *nhisto_store, int *histo_cap_store)
{
  int i;
  Gif_Color *histo = *histo_store;
  int nhisto = *nhisto_store;
  
  for (i = 0; i < nhisto; i++)
    if (histo[i].red == color->red && histo[i].green == color->green
	&& histo[i].blue == color->blue) {
      color->haspixel = 1;
      color->pixel = i;
      return;
    }
  
  if (nhisto >= *histo_cap_store) {
    *histo_cap_store *= 2;
    *histo_store = histo = Gif_ReArray(histo, Gif_Color, *histo_cap_store);
  }
  histo[nhisto] = *color;
  histo[nhisto].pixel = 0;
  (*nhisto_store)++;
  
  color->haspixel = 1;
  color->pixel = nhisto;
  fprintf(stderr, "%d=#%02X%02X%02X\n", nhisto, color->red, color->green, color->blue);
}


static int
histogram_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->pixel - b->pixel;
}


void
histogram(Gif_Stream *gfs)
{
  Gif_Color *histo = Gif_NewArray(Gif_Color, 256);
  int histo_cap = 256;
  int nhisto = 0;
  int x, y, ic, i;
  u_int32_t total = 0;
  
  unmark_colors(gfs->global);
  for (ic = 0; ic < gfs->nimages; ic++)
    unmark_colors(gfs->images[ic]->local);
  
  for (ic = 0; ic < gfs->nimages; ic++) {
    Gif_Image *gfi = gfs->images[ic];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    Gif_Color *col;
    if (!gfcm) continue;
    col = gfcm->col;
    for (y = 0; y < gfi->height; y++) {
      byte *data = gfi->img[y];
      for (x = 0; x < gfi->width; x++, data++) {
	if (!col[*data].haspixel)
	  add_histogram_color(&col[*data], &histo, &nhisto, &histo_cap);
	fprintf(stderr, "%d,%d,%d ", col[*data].pixel, gfcm->ncol, *data);
	histo[ col[*data].pixel ].pixel++;
	total++;
      }
    }
  }
  
  qsort(histo, nhisto, sizeof(Gif_Color), histogram_sort_compare);
  
  for (i = 0; i < nhisto; i++)
    fprintf(stderr, "#%02X%02X%02X: %10lu\n", histo[i].red, histo[i].green,
	    histo[i].blue, (unsigned long)histo[i].pixel);
  fprintf(stderr, "         %10lu\n", (unsigned long)total);
}
