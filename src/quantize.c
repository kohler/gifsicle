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

  /* have to start from 1 because the 0 slot is the transparent color. */
  for (i = 1; i < nhist; i++)
    if (hist[i].red == color->red && hist[i].green == color->green
	&& hist[i].blue == color->blue) {
      color->haspixel = 1;
      color->pixel = i;
      return;
    }
  
  if (nhist >= *hist_cap_store) {
    *hist_cap_store *= 2;
    Gif_ReArray(hist, Gif_Color, *hist_cap_store);
    *hist_store = hist;
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
  
  /* Put all transparent pixels in histogram slot 0. Transparent pixels are
     marked by haspixel == 255. */
  hist[0].haspixel = 255;
  hist[0].pixel = 0;
  nhist++;

  /* Count pixels. Be careful about values which are outside the range of the
     colormap. */
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    Gif_Color *col;
    int ncol;
    int transparent = gfi->transparent;
    if (!gfcm) continue;
    
    col = gfcm->col;
    ncol = gfcm->ncol;
    
    /* mark the transparent color as mapping to histogram slot 0 */
    if (transparent >= 0 && transparent < ncol) {
      col[transparent].haspixel = 1;
      col[transparent].pixel = 0;
      hist[0].red = col[transparent].red;
      hist[0].green = col[transparent].green;
      hist[0].blue = col[transparent].blue;
    }
    
    /* sweep over the image data, counting pixels */
    for (y = 0; y < gfi->height; y++) {
      byte *data = gfi->img[y];
      for (x = 0; x < gfi->width; x++, data++) {
	byte value = *data;
	if (value > ncol) value = ncol;
	if (!col[value].haspixel)
	  add_histogram_color(&col[value], &hist, &nhist, &hist_cap);
	hist[ col[value].pixel ].pixel++;
      }
    }
    
    /* unmark the transparent color */
    if (transparent >= 0 && transparent < ncol)
      col[transparent].haspixel = 0;
  }
  
  /* get rid of the transparent slot if there was no transparency in the
     image */
  if (hist[0].pixel == 0) {
    hist[0] = hist[nhist - 1];
    nhist--;
  }
  
  *nhist_store = nhist;
  return hist;
}


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


/* Transparency note: It seems hard to be maximally flexible about
   transparency (like, if a frame doesn't use red, use red as its transparent
   color; if another frame doesn't use blue, use blue as its transparent
   color). So we just globally allocate one color slot exclusively for
   transparency if there's any transparency in the image. */

static int
check_hist_transparency(Gif_Color *hist, int nhist)
{
  int i;
  for (i = 0; i < nhist && hist[i].haspixel != 255; i++)
    ;
  if (i >= nhist)
    return -1;
  else {
    Gif_Color temp = hist[i];
    hist[i] = hist[nhist - 1];
    hist[nhist - 1] = temp;
    return nhist - 1;
  }
}


/* COLORMAP FUNCTIONS return a palette (a vector of Gif_Colors). The
   pixel fields are undefined; the haspixel fields are all 0. If there was
   transparency in the image, the last color in the palette represents
   transparency, and its haspixel field equals 255. */

typedef struct {
  int first;
  int count;
  u_int32_t pixel;
} adaptive_slot;

Gif_Colormap *
colormap_median_cut(Gif_Color *hist, int nhist, int adapt_size)
{
  adaptive_slot *slots = Gif_NewArray(adaptive_slot, adapt_size);
  Gif_Colormap *gfcm = Gif_NewFullColormap(adapt_size);
  Gif_Color *adapt = gfcm->col;
  int nadapt;
  int i, j, transparent;
  
  /* This code was written with reference to ppmquant by Jef Poskanzer, part
     of the pbmplus package. */
  
  if (adapt_size < 2 || adapt_size > 256)
    fatal_error("adaptive palette size must be between 2 and 256");
  if (adapt_size > nhist) {
    warning("trivial adaptive palette (only %d colors in source)", nhist);
    adapt_size = nhist;
  }
  
  /* 0. remove any transparent color from consideration */
  transparent = check_hist_transparency(hist, nhist);
  if (transparent >= 0) nhist--, adapt_size--;
  
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
    adapt[i].haspixel = 0;
  }
  
  /* 4. if there was transparency, add it */
  if (transparent >= 0) {
    adapt[nadapt] = hist[transparent];
    adapt[nadapt].haspixel = 255;
    nadapt++;
  }
    
  Gif_DeleteArray(slots);
  gfcm->ncol = nadapt;
  return gfcm;
}



static Gif_Colormap *
colormap_diversity(Gif_Color *hist, int nhist, int adapt_size, int blend)
{
  u_int32_t *min_dist = Gif_NewArray(u_int32_t, nhist);
  int *closest = Gif_NewArray(int, nhist);
  Gif_Colormap *gfcm = Gif_NewFullColormap(adapt_size);
  Gif_Color *adapt = gfcm->col;
  int nadapt = 0;
  int i, j, transparent;
  
  /* This code was uses XV's modified diversity algorithm, and was written
     with reference to XV's implementation of that algorithm by John Bradley
     <bradley@cis.upenn.edu> and Tom Lane <Tom.Lane@g.gp.cs.cmu.edu>. */
  
  if (adapt_size < 2 || adapt_size > 256)
    fatal_error("adaptive palette size must be between 2 and 256");
  if (adapt_size > nhist) {
    warning("trivial adaptive palette (only %d colors in source)", nhist);
    adapt_size = nhist;
  }
  
  /* 0. remove any transparent color from consideration */
  transparent = check_hist_transparency(hist, nhist);
  if (transparent >= 0) nhist--, adapt_size--;
  /* blending has bad effects when there are very few colors */
  if (adapt_size < 4)
    blend = 0;
  
  /* 1. initialize min_dist and sort the colors in order of popularity. */
  for (i = 0; i < nhist; i++)
    min_dist[i] = 0x7FFFFFFF;
  
  qsort(hist, nhist, sizeof(Gif_Color), popularity_sort_compare);
  
  /* 2. choose colors one at a time */
  for (nadapt = 0; nadapt < adapt_size; nadapt++) {
    int chosen = 0;
    
    /* 2.1. choose the color to be added */
    if (nadapt == 0 || (nadapt >= 10 && nadapt % 2 == 0)) {
      /* 2.1a. choose based on popularity from unchosen colors; we've sorted
	 them on popularity, so just choose the first in the list */
      for (; chosen < nhist; chosen++)
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
    if (!blend || 2 * mismatch_pixel_total <= pixel_total)
      adapt[i] = hist[match];
    else {
      adapt[i].red = (byte)(red_total / pixel_total);
      adapt[i].green = (byte)(green_total / pixel_total);
      adapt[i].blue = (byte)(blue_total / pixel_total);
    }
    adapt[i].haspixel = 0;
  }
  
  /* 4. if there was transparency, add it */
  if (transparent >= 0) {
    adapt[nadapt] = hist[transparent];
    adapt[nadapt].haspixel = 255;
    nadapt++;
  }
  
  Gif_DeleteArray(min_dist);
  Gif_DeleteArray(closest);
  gfcm->ncol = nadapt;
  return gfcm;
}


Gif_Colormap *
colormap_blend_diversity(Gif_Color *hist, int nhist, int adapt_size)
{
  return colormap_diversity(hist, nhist, adapt_size, 1);
}

Gif_Colormap *
colormap_flat_diversity(Gif_Color *hist, int nhist, int adapt_size)
{
  return colormap_diversity(hist, nhist, adapt_size, 0);
}


struct color_hash_item {
  byte red;
  byte green;
  byte blue;
  u_int32_t pixel;
  color_hash_item *next;
};
#define COLOR_HASH_SIZE 20023
#define COLOR_HASH_CODE(r, g, b)	((u_int32_t)(r * 33023 + g * 30013 + b * 27011) % COLOR_HASH_SIZE)


/*****
 * color_hash_item allocation and deallocation
 **/

static color_hash_item *hash_item_alloc_list;
static int hash_item_alloc_left;
#define HASH_ITEM_ALLOC_AMOUNT 512

static color_hash_item **
new_color_hash(void)
{
  int i;
  color_hash_item **hash = Gif_NewArray(color_hash_item *, COLOR_HASH_SIZE);
  for (i = 0; i < COLOR_HASH_SIZE; i++)
    hash[i] = 0;
  return hash;
}


static color_hash_item *
new_color_hash_item(byte red, byte green, byte blue)
{
  color_hash_item *chi;
  if (hash_item_alloc_left <= 0) {
    color_hash_item *new_alloc =
      Gif_NewArray(color_hash_item, HASH_ITEM_ALLOC_AMOUNT);
    new_alloc[HASH_ITEM_ALLOC_AMOUNT-1].next = hash_item_alloc_list;
    hash_item_alloc_list = new_alloc;
    hash_item_alloc_left = HASH_ITEM_ALLOC_AMOUNT - 1;
  }
  
  --hash_item_alloc_left;
  chi = &hash_item_alloc_list[hash_item_alloc_left];
  chi->red = red;
  chi->green = green;
  chi->blue = blue;
  chi->next = 0;
  return chi;
}

static void
free_all_color_hash_items(void)
{
  while (hash_item_alloc_list) {
    color_hash_item *next =
      hash_item_alloc_list[HASH_ITEM_ALLOC_AMOUNT - 1].next;
    Gif_DeleteArray(hash_item_alloc_list);
    hash_item_alloc_list = next;
  }
  hash_item_alloc_left = 0;
}


static int
hash_color(byte red, byte green, byte blue,
	   color_hash_item **hash, Gif_Colormap *new_cm)
{
  u_int32_t hash_code = COLOR_HASH_CODE(red, green, blue);
  color_hash_item *prev = 0, *trav;
  
  for (trav = hash[hash_code]; trav; prev = trav, trav = trav->next)
    if (trav->red == red && trav->green == green && trav->blue == blue)
      return trav->pixel;
  
  trav = new_color_hash_item(red, green, blue);
  if (prev)
    prev->next = trav;
  else
    hash[hash_code] = trav;
  
  /* find the closest color in the new colormap */
  {
    Gif_Color *col = new_cm->col;
    int ncol = new_cm->ncol, i, found;
    u_int32_t min_dist = 0xFFFFFFFFU;
    
    for (i = 0; i < ncol; i++)
      if (col[i].haspixel != 255) {
	u_int32_t dist = (red - col[i].red) * (red - col[i].red)
	  + (green - col[i].green) * (green - col[i].green)
	  + (blue - col[i].blue) * (blue - col[i].blue);
	if (dist < min_dist) {
	  min_dist = dist;
	  found = i;
	}
      }
    
    trav->pixel = found;
    return found;
  }
}


void
colormap_image_posterize(Gif_Image *gfi, Gif_Colormap *old_cm,
			 Gif_Colormap *new_cm, int new_transparent,
			 color_hash_item **hash)
{
  int ncol = old_cm->ncol;
  Gif_Color *col = old_cm->col;
  int map[256]; 
  int i, j;
  
  /* mark transparent color if it exists */
  if (gfi->transparent >= 0 && gfi->transparent < ncol) {
    col[gfi->transparent].haspixel = 255;
    col[gfi->transparent].pixel = new_transparent;
  }
  
  /* find closest colors in new colormap */
  for (i = 0; i < ncol; i++)
    if (col[i].haspixel)
      map[i] = col[i].pixel;
    else {
      map[i] = col[i].pixel =
	hash_color(col[i].red, col[i].green, col[i].blue, hash, new_cm);
      col[i].haspixel = 1;
    }
  
  /* map image */
  for (j = 0; j < gfi->height; j++) {
    byte *data = gfi->img[j];
    for (i = 0; i < gfi->width; i++, data++)
      *data = map[*data];
  }
  
  /* unmark transparent color */
  if (gfi->transparent >= 0 && gfi->transparent < ncol) {
    col[gfi->transparent].haspixel = 0;
    gfi->transparent = new_transparent;
  }
}


#define DITHER_SCALE	1024
#define DITHER_SCALE_M1	(DITHER_SCALE-1)

void
colormap_image_floyd_steinberg(Gif_Image *gfi, Gif_Colormap *old_cm,
			       Gif_Colormap *new_cm, int new_transparent,
			       color_hash_item **hash)
{
  int width = gfi->width;
  int dither_direction = 0;
  int transparent = gfi->transparent;
  int i, j;
  int32_t *r_err, *g_err, *b_err, *r_err1, *g_err1, *b_err1;
  Gif_Color *col = old_cm->col;
  Gif_Color *new_col = new_cm->col;
  
  /* This code was written with reference to ppmquant by Jef Poskanzer, part
     of the pbmplus package. */
  
  /* Initialize Floyd-Steinberg error vectors to small random values, so
     we don't get an artifact on the top row */
  r_err = Gif_NewArray(int32_t, width + 2);
  g_err = Gif_NewArray(int32_t, width + 2);
  b_err = Gif_NewArray(int32_t, width + 2);
  r_err1 = Gif_NewArray(int32_t, width + 2);
  g_err1 = Gif_NewArray(int32_t, width + 2);
  b_err1 = Gif_NewArray(int32_t, width + 2);
  for (i = 0; i < width + 2; i++) {
    r_err[i] = random() % (DITHER_SCALE_M1 * 2) - DITHER_SCALE_M1;
    g_err[i] = random() % (DITHER_SCALE_M1 * 2) - DITHER_SCALE_M1;
    b_err[i] = random() % (DITHER_SCALE_M1 * 2) - DITHER_SCALE_M1;
  }
  
  /* Do the image! */
  for (j = 0; j < gfi->height; j++) {
    int d0, d1, d2, d3;		/* used for error diffusion */
    byte *data;
    int x;
    
    if (dither_direction) {
      data = &gfi->img[j][width-1];
      x = width - 1;
      d0 = 0, d1 = 2, d2 = 1, d3 = 0;
    } else {
      data = gfi->img[j];
      x = 0;
      d0 = 2, d1 = 0, d2 = 1, d3 = 2;
    }
    
    for (i = 0; i < width + 2; i++)
      r_err1[i] = g_err1[i] = b_err1[i] = 0;

    /* Do a single row */
    while (x >= 0 && x < width) {
      int e, use_r, use_g, use_b;
      
      /* the transparent color never gets adjusted */
      if (*data == transparent) {
	*data = new_transparent;
	goto next;
      }
      
      /* use Floyd-Steinberg errors to adjust actual color */
      use_r = col[*data].red + r_err[x+1] / DITHER_SCALE;
      use_g = col[*data].green + g_err[x+1] / DITHER_SCALE;
      use_b = col[*data].blue + b_err[x+1] / DITHER_SCALE;
      use_r = max(use_r, 0);  use_r = min(use_r, 255);
      use_g = max(use_g, 0);  use_g = min(use_g, 255);
      use_b = max(use_b, 0);  use_b = min(use_b, 255);
      
      *data = hash_color(use_r, use_g, use_b, hash, new_cm);
      
      /* calculate and propagate the error between desired and selected color.
	 Assume that, with a large scale (1024), we don't need to worry about
	 image artifacts caused by error accumulation (the fact that the
	 error terms might not sum to the error). */
      e = (use_r - new_col[*data].red) * DITHER_SCALE;
      if (e) {
	r_err [x+d0] += (e * 7) / 16;
	r_err1[x+d1] += (e * 3) / 16;
	r_err1[x+d2] += (e * 5) / 16;
	r_err1[x+d3] += e / 16;
      }
      
      e = (use_g - new_col[*data].green) * DITHER_SCALE;
      if (e) {
	g_err [x+d0] += (e * 7) / 16;
	g_err1[x+d1] += (e * 3) / 16;
	g_err1[x+d2] += (e * 5) / 16;
	g_err1[x+d3] += e / 16;
      }
      
      e = (use_b - new_col[*data].blue) * DITHER_SCALE;
      if (e) {
	b_err [x+d0] += (e * 7) / 16;
	b_err1[x+d1] += (e * 3) / 16;
	b_err1[x+d2] += (e * 5) / 16;
	b_err1[x+d3] += e / 16;
      }
      
     next:
      if (dither_direction)
	x--, data--;
      else
	x++, data++;
    }
    /* Did a single row */
    
    /* change dithering directions */
    {
      int32_t *temp;
      temp = r_err;  r_err = r_err1;  r_err1 = temp;
      temp = g_err;  g_err = g_err1;  g_err1 = temp;
      temp = b_err;  b_err = b_err1;  b_err1 = temp;
      dither_direction = !dither_direction;
    }
  }
  
  if (transparent >= 0)
    gfi->transparent = new_transparent;
  
  /* delete temporary storage */
  Gif_DeleteArray(r_err);
  Gif_DeleteArray(g_err);
  Gif_DeleteArray(b_err);
  Gif_DeleteArray(r_err1);
  Gif_DeleteArray(g_err1);
  Gif_DeleteArray(b_err1);
}


void
colormap_stream(Gif_Stream *gfs, Gif_Colormap *new_cm,
		colormap_image_func image_changer)
{ 
  color_hash_item **hash = new_color_hash();
  int new_transparent = new_cm->ncol - 1;
  int i;
  
  unmark_colors(gfs->global);
  for (i = 0; i < gfs->nimages; i++)
    unmark_colors(gfs->images[i]->local);
  
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    if (gfcm)
      image_changer(gfi, gfcm, new_cm, new_transparent, hash);
    if (gfi->local) {
      Gif_DeleteColormap(gfi->local);
      gfi->local = 0;
    }
  }
  
  Gif_DeleteColormap(gfs->global);
  gfs->global = Gif_CopyColormap(new_cm);

  free_all_color_hash_items();
  Gif_DeleteArray(hash);
}
