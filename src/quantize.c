/* quantize.c - Histograms and quantization for gifsicle.
   Copyright (C) 1997-8 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software; you can copy, distribute, or alter it at will, as
   long as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include "gifsicle.h"

static void
add_histogram_color(Gif_Color *color, Gif_Color **hist_store,
		    int *nhist_store, int *hist_cap_store)
{
  int i;
  Gif_Color *hist = *hist_store;
  int nhist = *nhist_store;
  
  for (i = 0; i < nhist; i++)
    if (hist[i].red == color->red && hist[i].green == color->green
	&& hist[i].blue == color->blue) {
      color->haspixel = 1;
      color->pixel = i;
      return;
    }
  
  if (nhist >= *hist_cap_store) {
    *hist_cap_store *= 2;
    *hist_store = hist = Gif_ReArray(hist, Gif_Color, *hist_cap_store);
  }
  hist[nhist] = *color;
  hist[nhist].pixel = 0;
  (*nhist_store)++;
  
  color->haspixel = 1;
  color->pixel = nhist;
}


static int
popularity_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return b->pixel - a->pixel;
}

static int
pixel_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->pixel - b->pixel;
}


Gif_Color *
histogram(Gif_Stream *gfs, int *nhist_store)
{
  Gif_Color *hist = Gif_NewArray(Gif_Color, 256);
  int hist_cap = 256;
  int nhist = 0;
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
	  add_histogram_color(&col[*data], &hist, &nhist, &hist_cap);
	hist[ col[*data].pixel ].pixel++;
      }
    }
  }
  
  *nhist_store = nhist;
  return hist;
}


typedef struct adaptive_slot {
  u_int32_t first;
  u_int32_t count;
  u_int32_t pixel;
} adaptive_slot;


#undef min
#undef max
#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))

static int
red_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->red - b->red;
}

static int
green_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->green - b->green;
}

static int
blue_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->blue - b->blue;
}



Gif_Color *
adaptive_palette_median_cut(Gif_Color *hist, int nhist, int adapt_size,
			    int *nadapt_store)
{
  adaptive_slot *slots = Gif_NewArray(adaptive_slot, adapt_size);
  Gif_Color *adapt = Gif_NewArray(Gif_Color, adapt_size);
  int nadapt;
  int i, j;
  
  /* This code was written with reference to ppmquant by Jef Poskanzer, part
     of the pbmplus package. */
  
  if (adapt_size < 2 || adapt_size > 256)
    fatal_error("adaptive palette size must be between 2 and 256");
  if (adapt_size > nhist) {
    warning("trivial adaptive palette (only %d colors in source)", nhist);
    adapt_size = nhist;
  }
  
  /* 1. set up the first slot, containing all pixels. */
  {
    u_int32_t total = 0;
    for (i = 0; i < nhist; i++)
      total += hist[i].pixel;
    
    slots[0].first = 0;
    slots[0].count = nhist;
    slots[0].pixel = total;
    
    qsort(hist, nhist, sizeof(Gif_Color), pixel_sort_compare);
  }
  
  /* 2. split slots until we have enough. */
  for (nadapt = 1; nadapt < adapt_size; nadapt++) {
    adaptive_slot *split = 0;
    Gif_Color minc, maxc, *slice;
    
    /* 2.1. pick the slot to split. */
    {
      u_int32_t split_pixel = 0;
      for (i = 0; i < nadapt; i++)
	if (slots[i].count >= 2 && slots[i].pixel > split_pixel) {
	  split = &slots[i];
	  split_pixel = slots[i].pixel;
	}
      if (!split)
	break;
    }
    slice = &hist[split->first];
    
    /* 2.2. find its extent. */
    {
      Gif_Color *trav = slice;
      minc = maxc = *trav;
      for (i = 1, trav++; i < split->count; i++, trav++) {
	minc.red = min(minc.red, trav->red);
	maxc.red = max(maxc.red, trav->red);
	minc.green = min(minc.green, trav->green);
	maxc.green = max(maxc.green, trav->green);
	minc.blue = min(minc.blue, trav->blue);
	maxc.blue = max(maxc.blue, trav->blue);
      }
    }
    
    /* 2.3. decide how to split it. use the luminance method. also sort the
       colors. */
    {
      double red_diff = 0.299 * (maxc.red - minc.red);
      double green_diff = 0.587 * (maxc.green - minc.green);
      double blue_diff = 0.114 * (maxc.blue - minc.blue);
      if (red_diff >= green_diff && red_diff >= blue_diff)
	qsort(slice, split->count, sizeof(Gif_Color), red_sort_compare);
      else if (green_diff >= blue_diff)
	qsort(slice, split->count, sizeof(Gif_Color), green_sort_compare);
      else
	qsort(slice, split->count, sizeof(Gif_Color), blue_sort_compare);
    }
    
    /* 2.4. decide where to split the slot and split it there. */
    {
      u_int32_t half_pixels = split->pixel / 2;
      u_int32_t pixel_accum = slice[0].pixel;
      for (i = 1; i < split->count - 1 && pixel_accum < half_pixels; i++)
	pixel_accum += slice[i].pixel;
      
      slots[nadapt].first = split->first + i;
      slots[nadapt].count = split->count - i;
      slots[nadapt].pixel = split->pixel - pixel_accum;
      split->count = i;
      split->pixel = pixel_accum;
    }
  }
  
  /* 3. make the new palette by choosing one color from each slot. */
  for (i = 0; i < nadapt; i++) {
    double red_total = 0, green_total = 0, blue_total = 0;
    Gif_Color *slice = &hist[ slots[i].first ];
    for (j = 0; j < slots[i].count; j++) {
      red_total += slice[j].red * slice[j].pixel;
      green_total += slice[j].green * slice[j].pixel;
      blue_total += slice[j].blue * slice[j].pixel;
    }
    adapt[i].red = (byte)(red_total / slots[i].pixel);
    adapt[i].green = (byte)(green_total / slots[i].pixel);
    adapt[i].blue = (byte)(blue_total / slots[i].pixel);
  }
  
  Gif_DeleteArray(slots);
  *nadapt_store = nadapt;
  return adapt;
}



Gif_Color *
adaptive_palette_diversity(Gif_Color *hist, int nhist, int adapt_size,
			   int *nadapt_store)
{
  u_int32_t *min_dist = Gif_NewArray(u_int32_t, nhist);
  int *closest = Gif_NewArray(int, nhist);
  Gif_Color *adapt = Gif_NewArray(Gif_Color, adapt_size);
  int nadapt = 0;
  int i, j;
  
  /* This code was uses XV's modified diversity algorithm, and was written
     with reference to XV's implementation by that algorithm by John Bradley
     <bradley@cis.upenn.edu> and Tom Lane <Tom.Lane@g.gp.cs.cmu.edu>. */
  
  if (adapt_size < 2 || adapt_size > 256)
    fatal_error("adaptive palette size must be between 2 and 256");
  if (adapt_size > nhist) {
    warning("trivial adaptive palette (only %d colors in source)", nhist);
    adapt_size = nhist;
  }
  
  /* 1. initialize min_dist and sort the colors in order of popularity. */
  for (i = 0; i < nhist; i++)
    min_dist[i] = 0x7FFFFFFF;
  
  qsort(hist, nhist, sizeof(Gif_Color), popularity_sort_compare);
  
  /* 2. choose colors one at a time. */
  for (nadapt = 0; nadapt < adapt_size; nadapt++) {
    int chosen;
    
    /* 2.1. choose the color to be added */
    if (nadapt == 0 || (nadapt >= 10 && nadapt % 2 == 0)) {
      /* 2.1a. choose based on popularity from unchosen colors; we've sorted
	 them, so just choose the first in the list. */
      for (chosen = 0; chosen < nhist; chosen++)
	if (min_dist[chosen])
	  break;
      
    } else {
      /* 2.1b. choose based on diversity from unchosen colors */
      u_int32_t chosen_dist = 0;
      for (i = 0; i < nhist; i++)
	if (min_dist[i] > chosen_dist) {
	  chosen = i;
	  chosen_dist = min_dist[i];
	}
    }
    
    /* 2.2. add the color */
    min_dist[chosen] = 0;
    closest[chosen] = nadapt;
    
    /* 2.3. adjust the min_dist array */
    {
      int red = hist[chosen].red, green = hist[chosen].green,
	blue = hist[chosen].blue;
      Gif_Color *h = hist;
      for (i = 0; i < nhist; i++, h++)
	if (min_dist[i]) {
	  u_int32_t dist = (h->red - red) * (h->red - red)
	    + (h->green - green) * (h->green - green)
	    + (h->blue - blue) * (h->blue - blue);
	  if (dist < min_dist[i]) {
	    min_dist[i] = dist;
	    closest[i] = nadapt;
	  }
	}
    }
  }
  
  /* 3. make the new palette by choosing one color from each slot. */
  for (i = 0; i < nadapt; i++) {
    double red_total = 0, green_total = 0, blue_total = 0;
    u_int32_t pixel_total = 0, mismatch_pixel_total = 0;
    int match;
    for (j = 0; j < nhist; j++)
      if (closest[j] == i) {
	u_int32_t pixel = hist[j].pixel;
	red_total += hist[j].red * pixel;
	green_total += hist[j].green * pixel;
	blue_total += hist[j].blue * pixel;
	pixel_total += pixel;
	if (min_dist[j])
	  mismatch_pixel_total += pixel;
	else
	  match = j;
      }
    /* Only blend if total number of mismatched pixels exceeds total number of
       matched pixels. */
    if (mismatch_pixel_total > pixel_total - mismatch_pixel_total) {
      adapt[i].red = (byte)(red_total / pixel_total);
      adapt[i].green = (byte)(green_total / pixel_total);
      adapt[i].blue = (byte)(blue_total / pixel_total);
    } else
      adapt[i] = hist[match];
  }
  
  Gif_DeleteArray(min_dist);
  Gif_DeleteArray(closest);
  *nadapt_store = nadapt;
  return adapt;
}
