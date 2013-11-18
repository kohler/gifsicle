/* quantize.c - Histograms and quantization for gifsicle.
   Copyright (C) 1997-2013 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include <assert.h>
#include <string.h>

static inline uint32_t color_distance(const Gif_Color* c, int r, int g, int b) {
    return (c->gfc_red - r) * (c->gfc_red - r)
        + (c->gfc_green - g) * (c->gfc_green - g)
        + (c->gfc_blue - b) * (c->gfc_blue - b);
}

typedef struct Gif_Histogram {
  Gif_Color *c;
  int n;
  int cap;
} Gif_Histogram;

static void add_histogram_color(Gif_Color *, Gif_Histogram *, unsigned long);

static void
init_histogram(Gif_Histogram *new_hist, Gif_Histogram *old_hist)
{
  int new_cap = (old_hist ? old_hist->cap * 2 : 1024);
  Gif_Color *nc = Gif_NewArray(Gif_Color, new_cap);
  int i;
  new_hist->c = nc;
  new_hist->n = 0;
  new_hist->cap = new_cap;
  for (i = 0; i < new_cap; i++)
    new_hist->c[i].haspixel = 0;
  if (old_hist)
    for (i = 0; i < old_hist->cap; i++)
      if (old_hist->c[i].haspixel)
	add_histogram_color(&old_hist->c[i], new_hist, old_hist->c[i].pixel);
}

static void
delete_histogram(Gif_Histogram *hist)
{
  Gif_DeleteArray(hist->c);
}

static void
add_histogram_color(Gif_Color *color, Gif_Histogram *hist, unsigned long count)
{
  Gif_Color *hc = hist->c;
  int hcap = hist->cap - 1;
  int i = (((color->gfc_red & 0xF0) << 4) | (color->gfc_green & 0xF0)
	   | (color->gfc_blue >> 4)) & hcap;
  int hash2 = ((((color->gfc_red & 0x0F) << 8) | ((color->gfc_green & 0x0F) << 4)
		| (color->gfc_blue & 0x0F)) & hcap) | 1;

  for (; hc[i].haspixel; i = (i + hash2) & hcap)
    if (GIF_COLOREQ(&hc[i], color)) {
      hc[i].pixel += count;
      color->haspixel = 1;
      color->pixel = i;
      return;
    }

  if (hist->n > ((hist->cap * 7) >> 3)) {
    Gif_Histogram new_hist;
    init_histogram(&new_hist, hist);
    delete_histogram(hist);
    *hist = new_hist;
    hc = hist->c;		/* 31.Aug.1999 - bug fix from Steven Marthouse
				   <comments@vrml3d.com> */
  }

  hist->n++;
  hc[i] = *color;
  hc[i].haspixel = 1;
  hc[i].pixel = count;
  color->haspixel = 1;
  color->pixel = i;
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
  Gif_Histogram hist;
  Gif_Color *linear;
  Gif_Color transparent_color;
  unsigned long ntransparent = 0;
  unsigned long nbackground = 0;
  int x, y, i;

  unmark_colors(gfs->global);
  for (i = 0; i < gfs->nimages; i++)
    unmark_colors(gfs->images[i]->local);

  init_histogram(&hist, 0);

  /* Count pixels. Be careful about values which are outside the range of the
     colormap. */
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    uint32_t count[256];
    Gif_Color *col;
    int ncol;
    int transparent = gfi->transparent;
    int only_compressed = (gfi->img == 0);
    if (!gfcm) continue;

    /* unoptimize the image if necessary */
    if (only_compressed)
      Gif_UncompressImage(gfi);

    /* sweep over the image data, counting pixels */
    for (x = 0; x < 256; x++)
      count[x] = 0;
    for (y = 0; y < gfi->height; y++) {
      uint8_t *data = gfi->img[y];
      for (x = 0; x < gfi->width; x++, data++)
	count[*data]++;
    }

    /* add counted colors to global histogram */
    col = gfcm->col;
    ncol = gfcm->ncol;
    for (x = 0; x < ncol; x++)
      if (count[x] && x != transparent) {
	if (col[x].haspixel)
	  hist.c[ col[x].pixel ].pixel += count[x];
	else
	  add_histogram_color(&col[x], &hist, count[x]);
      }
    if (transparent >= 0) {
      if (ntransparent == 0)
	  transparent_color = col[transparent];
      ntransparent += count[transparent];
    }

    /* if this image has background disposal, count its size towards the
       background's pixel count */
    if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
      nbackground += gfi->width * gfi->height;

    /* unoptimize the image if necessary */
    if (only_compressed)
      Gif_ReleaseUncompressedImage(gfi);
  }

  /* account for background by adding it to 'ntransparent' or the histogram */
  if (gfs->images[0]->transparent < 0 && gfs->global
      && gfs->background < gfs->global->ncol)
    add_histogram_color(&gfs->global->col[gfs->background], &hist, nbackground);
  else
    ntransparent += nbackground;

  /* now, make the linear histogram from the hashed histogram */
  linear = Gif_NewArray(Gif_Color, hist.n + 1);
  i = 0;

  /* Put all transparent pixels in histogram slot 0. Transparent pixels are
     marked by haspixel == 255. */
  if (ntransparent) {
    linear[0] = transparent_color;
    linear[0].haspixel = 255;
    linear[0].pixel = ntransparent;
    i++;
  }

  /* put hash histogram colors into linear histogram */
  for (x = 0; x < hist.cap; x++)
    if (hist.c[x].haspixel)
      linear[i++] = hist.c[x];

  delete_histogram(&hist);
  *nhist_store = i;
  return linear;
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
  return a->gfc_red - b->gfc_red;
}

static int
green_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->gfc_green - b->gfc_green;
}

static int
blue_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->gfc_blue - b->gfc_blue;
}


static void
assert_hist_transparency(Gif_Color *hist, int nhist)
{
  int i;
  for (i = 1; i < nhist; i++)
    assert(hist[i].haspixel != 255);
}


/* COLORMAP FUNCTIONS return a palette (a vector of Gif_Colors). The
   pixel fields are undefined; the haspixel fields are all 0. */

typedef struct {
  int first;
  int count;
  uint32_t pixel;
} adaptive_slot;

Gif_Colormap *
colormap_median_cut(Gif_Color *hist, int nhist, int adapt_size)
{
  adaptive_slot *slots = Gif_NewArray(adaptive_slot, adapt_size);
  Gif_Colormap *gfcm = Gif_NewFullColormap(adapt_size, 256);
  Gif_Color *adapt = gfcm->col;
  int nadapt;
  int i, j;

  /* This code was written with reference to ppmquant by Jef Poskanzer, part
     of the pbmplus package. */

  if (adapt_size < 2 || adapt_size > 256)
    fatal_error("adaptive palette size must be between 2 and 256");
  if (adapt_size >= nhist) {
    warning(1, "trivial adaptive palette (only %d colors in source)", nhist);
    adapt_size = nhist;
  }

  /* 0. remove any transparent color from consideration; reduce adaptive
     palette size to accommodate transparency if it looks like that'll be
     necessary */
  assert_hist_transparency(hist, nhist);
  if (adapt_size > 2 && adapt_size < nhist && hist[0].haspixel == 255
      && nhist <= 265)
    adapt_size--;
  if (hist[0].haspixel == 255) {
    hist[0] = hist[nhist - 1];
    nhist--;
  }

  /* 1. set up the first slot, containing all pixels. */
  {
    uint32_t total = 0;
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
      uint32_t split_pixel = 0;
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
	minc.gfc_red = min(minc.gfc_red, trav->gfc_red);
	maxc.gfc_red = max(maxc.gfc_red, trav->gfc_red);
	minc.gfc_green = min(minc.gfc_green, trav->gfc_green);
	maxc.gfc_green = max(maxc.gfc_green, trav->gfc_green);
	minc.gfc_blue = min(minc.gfc_blue, trav->gfc_blue);
	maxc.gfc_blue = max(maxc.gfc_blue, trav->gfc_blue);
      }
    }

    /* 2.3. decide how to split it. use the luminance method. also sort the
       colors. */
    {
      double red_diff = 0.299 * (maxc.gfc_red - minc.gfc_red);
      double green_diff = 0.587 * (maxc.gfc_green - minc.gfc_green);
      double blue_diff = 0.114 * (maxc.gfc_blue - minc.gfc_blue);
      if (red_diff >= green_diff && red_diff >= blue_diff)
	qsort(slice, split->count, sizeof(Gif_Color), red_sort_compare);
      else if (green_diff >= blue_diff)
	qsort(slice, split->count, sizeof(Gif_Color), green_sort_compare);
      else
	qsort(slice, split->count, sizeof(Gif_Color), blue_sort_compare);
    }

    /* 2.4. decide where to split the slot and split it there. */
    {
      uint32_t half_pixels = split->pixel / 2;
      uint32_t pixel_accum = slice[0].pixel;
      uint32_t diff1, diff2;
      for (i = 1; i < split->count - 1 && pixel_accum < half_pixels; i++)
	pixel_accum += slice[i].pixel;

      /* We know the area before the split has more pixels than the area
         after, possibly by a large margin (bad news). If it would shrink the
         margin, change the split. */
      diff1 = 2*pixel_accum - split->pixel;
      diff2 = split->pixel - 2*(pixel_accum - slice[i-1].pixel);
      if (diff2 < diff1 && i > 1) {
	i--;
	pixel_accum -= slice[i].pixel;
      }

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
      red_total += slice[j].gfc_red * slice[j].pixel;
      green_total += slice[j].gfc_green * slice[j].pixel;
      blue_total += slice[j].gfc_blue * slice[j].pixel;
    }
    adapt[i].gfc_red = (uint8_t)(red_total / slots[i].pixel);
    adapt[i].gfc_green = (uint8_t)(green_total / slots[i].pixel);
    adapt[i].gfc_blue = (uint8_t)(blue_total / slots[i].pixel);
    adapt[i].haspixel = 0;
  }

  Gif_DeleteArray(slots);
  gfcm->ncol = nadapt;
  return gfcm;
}



static Gif_Colormap *
colormap_diversity(Gif_Color *hist, int nhist, int adapt_size, int blend)
{
  uint32_t *min_dist = Gif_NewArray(uint32_t, nhist);
  int *closest = Gif_NewArray(int, nhist);
  Gif_Colormap *gfcm = Gif_NewFullColormap(adapt_size, 256);
  Gif_Color *adapt = gfcm->col;
  int nadapt = 0;
  int i, j, match = 0;

  /* This code was uses XV's modified diversity algorithm, and was written
     with reference to XV's implementation of that algorithm by John Bradley
     <bradley@cis.upenn.edu> and Tom Lane <Tom.Lane@g.gp.cs.cmu.edu>. */

  if (adapt_size < 2 || adapt_size > 256)
    fatal_error("adaptive palette size must be between 2 and 256");
  if (adapt_size > nhist) {
    warning(1, "trivial adaptive palette (only %d colors in source)", nhist);
    adapt_size = nhist;
  }

  /* 0. remove any transparent color from consideration; reduce adaptive
     palette size to accommodate transparency if it looks like that'll be
     necessary */
  assert_hist_transparency(hist, nhist);
  /* It will be necessary to accommodate transparency if (1) there is
     transparency in the image; (2) the adaptive palette isn't trivial; and
     (3) there are a small number of colors in the image (arbitrary constant:
     <= 265), so it's likely that most images will use most of the slots, so
     it's likely there won't be unused slots. */
  if (adapt_size > 2 && adapt_size < nhist && hist[0].haspixel == 255
      && nhist <= 265)
    adapt_size--;
  if (hist[0].haspixel == 255) {
    hist[0] = hist[nhist - 1];
    nhist--;
  }

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
      uint32_t chosen_dist = 0;
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
      int red = hist[chosen].gfc_red, green = hist[chosen].gfc_green,
	blue = hist[chosen].gfc_blue;
      Gif_Color *h = hist;
      for (i = 0; i < nhist; i++, h++)
	if (min_dist[i]) {
          uint32_t dist = color_distance(h, red, green, blue);
	  if (dist < min_dist[i]) {
	    min_dist[i] = dist;
	    closest[i] = nadapt;
	  }
	}
    }
  }

  /* 3. make the new palette by choosing one color from each slot. */
  if (!blend) {
    for (i = 0; i < nadapt; i++) {
      for (j = 0; j < nhist; j++)
	if (closest[j] == i && !min_dist[j])
	  match = j;
      adapt[i] = hist[match];
      adapt[i].haspixel = 0;
    }

  } else {
    for (i = 0; i < nadapt; i++) {
      double red_total = 0, green_total = 0, blue_total = 0;
      uint32_t pixel_total = 0, mismatch_pixel_total = 0;
      for (j = 0; j < nhist; j++)
	if (closest[j] == i) {
	  uint32_t pixel = hist[j].pixel;
	  red_total += hist[j].gfc_red * pixel;
	  green_total += hist[j].gfc_green * pixel;
	  blue_total += hist[j].gfc_blue * pixel;
	  pixel_total += pixel;
	  if (min_dist[j])
	    mismatch_pixel_total += pixel;
	  else
	    match = j;
	}
      /* Only blend if total number of mismatched pixels exceeds total number
	 of matched pixels by a large margin. */
      if (3 * mismatch_pixel_total <= 2 * pixel_total)
	adapt[i] = hist[match];
      else {
	/* Favor, by a smallish amount, the color the plain diversity
           algorithm would pick. */
	uint32_t pixel = hist[match].pixel * 2;
	red_total += hist[match].gfc_red * pixel;
	green_total += hist[match].gfc_green * pixel;
	blue_total += hist[match].gfc_blue * pixel;
	pixel_total += pixel;
	adapt[i].gfc_red = (uint8_t)(red_total / pixel_total);
	adapt[i].gfc_green = (uint8_t)(green_total / pixel_total);
	adapt[i].gfc_blue = (uint8_t)(blue_total / pixel_total);
      }
      adapt[i].haspixel = 0;
    }
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


/*****
 * color_hash allocation and deallocation
 **/

typedef struct colormap_hash_item {
    uint32_t color; // (1<<24) | (red<<16) | (green<<8) | (blue)
    uint32_t pixel;
} colormap_hash_item;

#define COLORMAP_HASH_NLAST 4

struct colormap_hash {
    colormap_hash_item* hash;
    int nhash;
    int hashfull;
    Gif_Color* col;
    int ncol;
    int grayscale;
    uint32_t* colmindist;
};

static int colormap_hash_sizes[] = {
    1021, 4093, 16381, 65521, 262139, 1048571, 4194301
};

#define COLORMAP_HASHCOLOR(r, g, b) \
    ((1U << 24) | ((uint8_t) b << 16) | ((uint8_t) g << 8) | ((uint8_t) r))

static inline int colormap_hash_bucket(const colormap_hash* ch,
                                       int r, int g, int b) {
    uint32_t color = COLORMAP_HASHCOLOR(r, g, b);
    int bk = color % ch->nhash, step = 0;
    while (ch->hash[bk].color && ch->hash[bk].color != color) {
        if (!step)
            step = (((uint8_t) g << 16) | ((uint8_t) r << 8) | ((uint8_t) b))
                % 1024 + 1;
        bk += step;
        if (bk >= ch->nhash)
            bk -= ch->nhash;
    }
    return bk;
}

void colormap_hash_populate(colormap_hash* ch) {
    int i, bk;
    ch->grayscale = 1;
    for (i = 0; i != ch->ncol; ++i) {
        const Gif_Color* c = &ch->col[i];
        ch->grayscale = ch->grayscale && c->gfc_red == c->gfc_green
            && c->gfc_green == c->gfc_blue;
        bk = colormap_hash_bucket(ch, c->gfc_red, c->gfc_green, c->gfc_blue);
        if (!ch->hash[bk].color) {
            ch->hash[bk].color = COLORMAP_HASHCOLOR(c->gfc_red, c->gfc_green, c->gfc_blue);
            ch->hash[bk].pixel = i;
            ++ch->hashfull;
        }
    }
}

void colormap_hash_init(colormap_hash* ch, const Gif_Colormap* gfcm) {
    ch->nhash = colormap_hash_sizes[0];
    ch->hash = Gif_NewArray(colormap_hash_item, ch->nhash);
    memset(ch->hash, 0, sizeof(colormap_hash_item) * ch->nhash);
    ch->hashfull = 0;
    ch->col = gfcm->col;
    ch->ncol = gfcm->ncol;
    colormap_hash_populate(ch);
    ch->colmindist = (uint32_t*) NULL;
}

void colormap_hash_cleanup(colormap_hash* ch) {
    Gif_DeleteArray(ch->hash);
    Gif_DeleteArray(ch->colmindist);
}

void colormap_hash_set_colmindist(colormap_hash* ch) {
    int i, j;
    if (ch->colmindist)
        return;
    ch->colmindist = Gif_NewArray(uint32_t, ch->ncol);
    for (i = 0; i != ch->ncol; ++i)
        ch->colmindist[i] = (uint32_t) -1;
    for (i = 0; i != ch->ncol; ++i)
        for (j = i + 1; j != ch->ncol; ++j) {
            const Gif_Color* ci = &ch->col[i], *cj = &ch->col[j];
            uint32_t dist = color_distance(ci, cj->gfc_red, cj->gfc_green, cj->gfc_blue);
            // That's the squared distance; we want the square of 1/2 the
            // distance
            dist /= 4;
            if (dist < ch->colmindist[i])
                ch->colmindist[i] = dist;
            if (dist < ch->colmindist[j])
                ch->colmindist[j] = dist;
        }
}

int closest_color_rgb_linear(Gif_Color* col, int ncol, int r, int g, int b) {
    int i, found = 0;
    uint32_t min_dist = 0xFFFFFFFFU;

    /* Use straight-line Euclidean distance in RGB space */
    for (i = 0; i < ncol; i++)
	if (col[i].haspixel != 255) {
            uint32_t dist = color_distance(&col[i], r, g, b);
            if (dist < min_dist) {
                min_dist = dist;
                found = i;
            }
	}

    return found;
}

int closest_color_grayscale_luminance(Gif_Color* col, int ncol,
                                      int r, int g, int b) {
    int i, found = 0;
    uint32_t min_dist = 0xFFFFFFFFU;
    /* If the new colormap is 100% grayscale, then use distance in luminance
       space instead of distance in RGB space. The weights for the R,G,B
       components in luminance space are 0.299,0.587,0.114. We calculate a
       gray value for the input color first and compare that against the
       available grays in the colormap. Thanks to Christian Kumpf,
       <kumpf@igd.fhg.de>, for providing a patch.

       Note on the calculation of 'gray': Using the factors 306, 601, and
       117 (proportional to 0.299,0.587,0.114) we get a scaled gray value
       between 0 and 255 * 1024. */
    int gray = 306 * r + 601 * g + 117 * b;

    for (i = 0; i < ncol; i++)
	if (col[i].haspixel != 255) {
            int in_gray = 1024 * col[i].gfc_red;
            uint32_t dist = abs(gray - in_gray);
            if (dist < min_dist) {
                min_dist = dist;
                found = i;
            }
	}

    return found;
}

void colormap_hash_grow(colormap_hash* ch) {
    size_t nhashsizes = sizeof(colormap_hash_sizes)/sizeof(colormap_hash_sizes[0]);
    int sizeindex;
    for (sizeindex = 0; sizeindex < (int) nhashsizes
             && colormap_hash_sizes[sizeindex] <= ch->nhash; ++sizeindex)
        /* do nothing */;
    if (sizeindex < (int) nhashsizes) {
        Gif_DeleteArray(ch->hash);
        ch->nhash = colormap_hash_sizes[sizeindex];
        ch->hash = Gif_NewArray(colormap_hash_item, ch->nhash);
    }
    memset(ch->hash, 0, sizeof(colormap_hash_item) * ch->nhash);
    ch->hashfull = 0;
    colormap_hash_populate(ch);
}

int colormap_hash_lookup(colormap_hash* ch, int r, int g, int b) {
    int bk;

    bk = colormap_hash_bucket(ch, r, g, b);
    if (ch->hash[bk].color)
        goto done;

    if (ch->hashfull + ch->hashfull > ch->nhash) {
        colormap_hash_grow(ch);
        bk = colormap_hash_bucket(ch, r, g, b);
    }

    /* find the closest color in the new colormap */
    ch->hash[bk].color = COLORMAP_HASHCOLOR(r, g, b);
    if (ch->grayscale)
        ch->hash[bk].pixel =
            closest_color_grayscale_luminance(ch->col, ch->ncol, r, g, b);
    else
        ch->hash[bk].pixel =
            closest_color_rgb_linear(ch->col, ch->ncol, r, g, b);
    ++ch->hashfull;

 done:
    return ch->hash[bk].pixel;
}


void
colormap_image_posterize(Gif_Image *gfi, uint8_t *new_data,
			 Gif_Colormap *old_cm, colormap_hash* ch,
			 uint32_t *histogram)
{
  int ncol = old_cm->ncol;
  Gif_Color *col = old_cm->col;
  int map[256];
  int i, j;
  int transparent = gfi->transparent;

  /* find closest colors in new colormap */
  for (i = 0; i < ncol; i++)
    if (col[i].haspixel)
      map[i] = col[i].pixel;
    else {
      map[i] = col[i].pixel =
          colormap_hash_lookup(ch, col[i].gfc_red, col[i].gfc_green, col[i].gfc_blue);
      col[i].haspixel = 1;
    }

  /* map image */
  for (j = 0; j < gfi->height; j++) {
    uint8_t *data = gfi->img[j];
    for (i = 0; i < gfi->width; i++, data++, new_data++)
      if (*data != transparent) {
	*new_data = map[*data];
	histogram[*new_data]++;
      }
  }
}


#define DITHER_SCALE	1024
#define DITHER_SCALE_M1	(DITHER_SCALE-1)
#define N_RANDOM_VALUES	512

void
colormap_image_floyd_steinberg(Gif_Image *gfi, uint8_t *all_new_data,
			       Gif_Colormap *old_cm,
                               colormap_hash* ch, uint32_t *histogram)
{
  static int32_t *random_values = 0;

  int width = gfi->width;
  int dither_direction = 0;
  int transparent = gfi->transparent;
  int i, j;
  int32_t *r_err, *g_err, *b_err, *r_err1, *g_err1, *b_err1;
  Gif_Color *col = old_cm->col;

  /* Initialize distances */
  colormap_hash_set_colmindist(ch);
  for (i = 0; i < old_cm->ncol; ++i)
      if (!col[i].haspixel) {
          col[i].pixel =
              colormap_hash_lookup(ch, col[i].gfc_red, col[i].gfc_green, col[i].gfc_blue);
          col[i].haspixel = 1;
      }

  /* This code was written with reference to ppmquant by Jef Poskanzer, part
     of the pbmplus package. */

  /* Initialize Floyd-Steinberg error vectors to small random values, so we
     don't get artifacts on the top row */
  r_err = Gif_NewArray(int32_t, width + 2);
  g_err = Gif_NewArray(int32_t, width + 2);
  b_err = Gif_NewArray(int32_t, width + 2);
  r_err1 = Gif_NewArray(int32_t, width + 2);
  g_err1 = Gif_NewArray(int32_t, width + 2);
  b_err1 = Gif_NewArray(int32_t, width + 2);
  /* Use the same random values on each call in an attempt to minimize
     "jumping dithering" effects on animations */
  if (!random_values) {
    random_values = Gif_NewArray(int32_t, N_RANDOM_VALUES);
    for (i = 0; i < N_RANDOM_VALUES; i++)
      random_values[i] = RANDOM() % (DITHER_SCALE_M1 * 2) - DITHER_SCALE_M1;
  }
  for (i = 0; i < gfi->width + 2; i++) {
    int j = (i + gfi->left) * 3;
    r_err[i] = random_values[ (j + 0) % N_RANDOM_VALUES ];
    g_err[i] = random_values[ (j + 1) % N_RANDOM_VALUES ];
    b_err[i] = random_values[ (j + 2) % N_RANDOM_VALUES ];
  }
  /* *_err1 initialized below */

  /* Do the image! */
  for (j = 0; j < gfi->height; j++) {
    int d0, d1, d2, d3;		/* used for error diffusion */
    uint8_t *data, *new_data;
    int x;

    if (dither_direction) {
      x = width - 1;
      d0 = 0, d1 = 2, d2 = 1, d3 = 0;
    } else {
      x = 0;
      d0 = 2, d1 = 0, d2 = 1, d3 = 2;
    }
    data = &gfi->img[j][x];
    new_data = all_new_data + j * width + x;

    for (i = 0; i < width + 2; i++)
      r_err1[i] = g_err1[i] = b_err1[i] = 0;

    /* Do a single row */
    while (x >= 0 && x < width) {
      int e, use_r, use_g, use_b;

      /* the transparent color never gets adjusted */
      if (*data == transparent)
	goto next;

      /* use Floyd-Steinberg errors to adjust actual color */
      use_r = col[*data].gfc_red + r_err[x+1] / DITHER_SCALE;
      use_g = col[*data].gfc_green + g_err[x+1] / DITHER_SCALE;
      use_b = col[*data].gfc_blue + b_err[x+1] / DITHER_SCALE;
      use_r = max(use_r, 0);  use_r = min(use_r, 255);
      use_g = max(use_g, 0);  use_g = min(use_g, 255);
      use_b = max(use_b, 0);  use_b = min(use_b, 255);

      e = col[*data].pixel;
      if (color_distance(&ch->col[e], use_r, use_g, use_b) < ch->colmindist[e])
          *new_data = e;
      else
          *new_data = colormap_hash_lookup(ch, use_r, use_g, use_b);
      histogram[*new_data]++;

      /* calculate and propagate the error between desired and selected color.
	 Assume that, with a large scale (1024), we don't need to worry about
	 image artifacts caused by error accumulation (the fact that the
	 error terms might not sum to the error). */
      e = (use_r - ch->col[*new_data].gfc_red) * DITHER_SCALE;
      if (e) {
	r_err [x+d0] += (e * 7) / 16;
	r_err1[x+d1] += (e * 3) / 16;
	r_err1[x+d2] += (e * 5) / 16;
	r_err1[x+d3] += e / 16;
      }

      e = (use_g - ch->col[*new_data].gfc_green) * DITHER_SCALE;
      if (e) {
	g_err [x+d0] += (e * 7) / 16;
	g_err1[x+d1] += (e * 3) / 16;
	g_err1[x+d2] += (e * 5) / 16;
	g_err1[x+d3] += e / 16;
      }

      e = (use_b - ch->col[*new_data].gfc_blue) * DITHER_SCALE;
      if (e) {
	b_err [x+d0] += (e * 7) / 16;
	b_err1[x+d1] += (e * 3) / 16;
	b_err1[x+d2] += (e * 5) / 16;
	b_err1[x+d3] += e / 16;
      }

     next:
      if (dither_direction)
	x--, data--, new_data--;
      else
	x++, data++, new_data++;
    }
    /* Did a single row */

    /* change dithering directions */
    {
      int32_t *temp;
      temp = r_err; r_err = r_err1; r_err1 = temp;
      temp = g_err; g_err = g_err1; g_err1 = temp;
      temp = b_err; b_err = b_err1; b_err1 = temp;
      dither_direction = !dither_direction;
    }
  }

  /* delete temporary storage */
  Gif_DeleteArray(r_err);
  Gif_DeleteArray(g_err);
  Gif_DeleteArray(b_err);
  Gif_DeleteArray(r_err1);
  Gif_DeleteArray(g_err1);
  Gif_DeleteArray(b_err1);
}


/* return value 1 means run the image_changer again */
static int
try_assign_transparency(Gif_Image *gfi, Gif_Colormap *old_cm, uint8_t *new_data,
			Gif_Colormap *new_cm, int *new_ncol,
			uint32_t *histogram)
{
  uint32_t min_used;
  int i, j;
  int transparent = gfi->transparent;
  int new_transparent = -1;
  Gif_Color transp_value;

  if (transparent < 0)
    return 0;

  if (old_cm)
    transp_value = old_cm->col[transparent];

  /* look for an unused pixel in the existing colormap; prefer the same color
     we had */
  for (i = 0; i < *new_ncol; i++)
    if (histogram[i] == 0 && GIF_COLOREQ(&transp_value, &new_cm->col[i])) {
      new_transparent = i;
      goto found;
    }
  for (i = 0; i < *new_ncol; i++)
    if (histogram[i] == 0) {
      new_transparent = i;
      goto found;
    }

  /* try to expand the colormap */
  if (*new_ncol < 256) {
    assert(*new_ncol < new_cm->capacity);
    new_transparent = *new_ncol;
    new_cm->col[new_transparent] = transp_value;
    (*new_ncol)++;
    goto found;
  }

  /* not found: mark the least-frequently-used color as the new transparent
     color and return 1 (meaning 'dither again') */
  assert(*new_ncol == 256);
  min_used = 0xFFFFFFFFU;
  for (i = 0; i < 256; i++)
    if (histogram[i] < min_used) {
      new_transparent = i;
      min_used = histogram[i];
    }
  new_cm->col[new_transparent].haspixel = 255; /* mark it unusable */
  return 1;

 found:
  for (j = 0; j < gfi->height; j++) {
    uint8_t *data = gfi->img[j];
    for (i = 0; i < gfi->width; i++, data++, new_data++)
      if (*data == transparent)
	*new_data = new_transparent;
  }

  gfi->transparent = new_transparent;
  return 0;
}

void
colormap_stream(Gif_Stream *gfs, Gif_Colormap *new_cm,
		colormap_image_func image_changer)
{
  colormap_hash hash;
  int background_transparent = gfs->images[0]->transparent >= 0;
  Gif_Color *new_col = new_cm->col;
  int new_ncol = new_cm->ncol;
  int imagei, j;
  int compress_new_cm = 1;

  /* make sure colormap has enough space */
  if (new_cm->capacity < 256) {
    Gif_Color *x = Gif_NewArray(Gif_Color, 256);
    memcpy(x, new_col, sizeof(Gif_Color) * new_ncol);
    Gif_DeleteArray(new_col);
    new_cm->col = new_col = x;
    new_cm->capacity = 256;
  }
  assert(new_cm->capacity >= 256);

  /* new_col[j].pixel == number of pixels with color j in the new image. */
  for (j = 0; j < 256; j++)
    new_col[j].pixel = 0;

  /* initialize hash */
  colormap_hash_init(&hash, new_cm);

  for (imagei = 0; imagei < gfs->nimages; imagei++) {
    Gif_Image *gfi = gfs->images[imagei];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    int only_compressed = (gfi->img == 0);

    if (gfcm) {
      /* If there was an old colormap, change the image data */
      uint8_t *new_data = Gif_NewArray(uint8_t, gfi->width * gfi->height);
      uint32_t histogram[256];
      unmark_colors(new_cm);
      unmark_colors(gfcm);

      if (only_compressed)
	Gif_UncompressImage(gfi);

      do {
	for (j = 0; j < 256; j++)
            histogram[j] = 0;
	image_changer(gfi, new_data, gfcm, &hash, histogram);
      } while (try_assign_transparency(gfi, gfcm, new_data, new_cm, &new_ncol,
				       histogram));

      Gif_ReleaseUncompressedImage(gfi);
      /* version 1.28 bug fix: release any compressed version or it'll cause
         bad images */
      Gif_ReleaseCompressedImage(gfi);
      Gif_SetUncompressedImage(gfi, new_data, Gif_DeleteArrayFunc, 0);

      if (only_compressed) {
	Gif_FullCompressImage(gfs, gfi, &gif_write_info);
	Gif_ReleaseUncompressedImage(gfi);
      }

      /* update count of used colors */
      for (j = 0; j < 256; j++)
	new_col[j].pixel += histogram[j];
      if (gfi->transparent >= 0)
	/* we don't have data on the number of used colors for transparency
	   so fudge it. */
	new_col[gfi->transparent].pixel += gfi->width * gfi->height / 8;

    } else {
      /* Can't compress new_cm afterwards if we didn't actively change colors
	 over */
      compress_new_cm = 0;
    }

    if (gfi->local) {
      Gif_DeleteColormap(gfi->local);
      gfi->local = 0;
    }
  }

  /* Set new_cm->ncol from new_ncol. We didn't update new_cm->ncol before so
     the closest-color algorithms wouldn't see any new transparent colors.
     That way added transparent colors were only used for transparency. */
  new_cm->ncol = new_ncol;

  /* change the background. I hate the background by now */
  if (background_transparent)
    gfs->background = gfs->images[0]->transparent;
  else if (gfs->global && gfs->background < gfs->global->ncol) {
    Gif_Color *c = &gfs->global->col[ gfs->background ];
    gfs->background = colormap_hash_lookup(&hash, c->gfc_red, c->gfc_green, c->gfc_blue);
    new_col[gfs->background].pixel++;
  }

  Gif_DeleteColormap(gfs->global);
  colormap_hash_cleanup(&hash);

  /* We may have used only a subset of the colors in new_cm. We try to store
     only that subset, just as if we'd piped the output of 'gifsicle
     --use-colormap=X' through 'gifsicle' another time. */
  gfs->global = Gif_CopyColormap(new_cm);
  for (j = 0; j < new_cm->ncol; ++j)
      gfs->global->col[j].haspixel = 0;
  if (compress_new_cm) {
    /* only bother to recompress if we'll get anything out of it */
    compress_new_cm = 0;
    for (j = 0; j < new_cm->ncol - 1; j++)
      if (new_col[j].pixel == 0 || new_col[j].pixel < new_col[j+1].pixel) {
	compress_new_cm = 1;
	break;
      }
  }

  if (compress_new_cm) {
    int map[256];

    /* Gif_CopyColormap copies the 'pixel' values as well */
    new_col = gfs->global->col;
    for (j = 0; j < new_cm->ncol; j++)
      new_col[j].haspixel = j;

    /* sort based on popularity */
    qsort(new_col, new_cm->ncol, sizeof(Gif_Color), popularity_sort_compare);

    /* set up the map and reduce the number of colors */
    for (j = 0; j < new_cm->ncol; j++)
      map[ new_col[j].haspixel ] = j;
    for (j = 0; j < new_cm->ncol; j++)
      if (!new_col[j].pixel) {
	gfs->global->ncol = j;
	break;
      }

    /* map the image data, transparencies, and background */
    gfs->background = map[gfs->background];
    for (imagei = 0; imagei < gfs->nimages; imagei++) {
      Gif_Image *gfi = gfs->images[imagei];
      int only_compressed = (gfi->img == 0);
      uint32_t size;
      uint8_t *data;
      if (only_compressed)
	Gif_UncompressImage(gfi);

      data = gfi->image_data;
      for (size = gfi->width * gfi->height; size > 0; size--, data++)
	*data = map[*data];
      if (gfi->transparent >= 0)
	gfi->transparent = map[gfi->transparent];

      if (only_compressed) {
	Gif_FullCompressImage(gfs, gfi, &gif_write_info);
	Gif_ReleaseUncompressedImage(gfi);
      }
    }
  }
}
