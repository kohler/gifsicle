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
}


static int
histogram_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->pixel - b->pixel;
}


Gif_Color *
histogram(Gif_Stream *gfs, int *nhisto_store)
{
  Gif_Color *histo = Gif_NewArray(Gif_Color, 256);
  int histo_cap = 256;
  int nhisto = 0;
  int x, y, i;
  
  unmark_colors(gfs->global);
  for (i = 0; i < gfs->nimages; i++)
    unmark_colors(gfs->images[i]->local);
  
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    Gif_Color *col;
    if (!gfcm) continue;
    col = gfcm->col;
    for (y = 0; y < gfi->height; y++) {
      byte *data = gfi->img[y];
      for (x = 0; x < gfi->width; x++, data++) {
	if (!col[*data].haspixel)
	  add_histogram_color(&col[*data], &histo, &nhisto, &histo_cap);
	histo[ col[*data].pixel ].pixel++;
      }
    }
  }
  
  qsort(histo, nhisto, sizeof(Gif_Color), histogram_sort_compare);
  *nhisto_store = nhisto;
  return histo;
}


typedef struct adaptive_slot {
  int first;
  int count;
  u_int32_t pixel;
} adaptive_slot;


Gif_Color *
adaptive_palette(Gif_Color *histo, int nhisto, int adapt_size)
{
  adaptive_slot *slots = Gif_NewArray(adaptive_slot, adapt_size);
  int nadapt;
  int i;

  if (adapt_size < 2 || adapt_size > 256)
    fatal_error("adaptive palette size must be between 2 and 256");
  if (adapt_size > nhisto)
    warning("trivial adaptive palette (only %d colors in source)", nhisto);
  
  {
    u_int32_t total = 0;
    for (i = 0; i < nhisto; i++)
      total += histo[i].pixel;
    
    adapt[0].first = 0;
    adapt[0].count = nhisto;
    adapt[0].pixel = total;
  }
  
  for (nadapt = 1; nadapt < adapt_size; nadapt++) {
    int split_box = -1;
    int split_count = 1;
    u_int32_t pixel = 1;
    
    /* 1. pick the box to split. */
    for (i = 0; i < nadapt; i++)
      if (adapt[i].count > split_count) {
	split_box = i;
	split_count = adapt[i].count;
      }
    if (split_box < 0)
      break;
    
    /* 2. decide how to split it. */
  }
}
