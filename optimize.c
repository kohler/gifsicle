/* optimize.c - Functions to optimize animated GIFs.
   Copyright (C) 1997-8 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2 or later; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include "gifsicle.h"
#include <assert.h>

#define OPTIMIZED_TRANSPARENCY 0

typedef struct {
  u_int16_t left;
  u_int16_t top;
  u_int16_t width;
  u_int16_t height;
  byte disposal;
  byte *needed_colors;
  u_int16_t required_color_count;
  u_int16_t global_penalty;
} Gif_OptData;

/* Screen width and height */
static int screen_width;
static int screen_height;

/* Colormap containing all colors in the image. May have >256 colors */
static Gif_Colormap *all_colormap;

/* The old global colormap, or a fake one we created if necessary */
static Gif_Colormap *in_global_map;

/* The new global colormap */
static Gif_Colormap *out_global_map;

#define TRANSP (0)
static int background;
#define NOT_IN_OUT_GLOBAL (256)
static u_int16_t *last_data;
static u_int16_t *this_data;
static u_int16_t *next_data;
static int image_index;

static int gif_color_count;


/*****
 * SIMPLE HELPERS
 * new and delete optimize data; and colormap_combine; and sorting permutations
 **/

Gif_OptData *
new_opt_data(void)
{
  Gif_OptData *od = Gif_New(Gif_OptData);
  od->needed_colors = 0;
  od->global_penalty = 1;
  return od;
}

void
delete_opt_data(Gif_OptData *od)
{
  if (!od) return;
  Gif_DeleteArray(od->needed_colors);
  Gif_Delete(od);
}


/* colormap_combine: Ensure that each color in `src' is represented in `dst'.
   For each color `i' in `src', src->col[i].pixel == some j so that
   GIF_COLOREQ(&src->col[i], &dst->col[j]). dst->col[0] is reserved for
   transparency; no source color will be mapped to it. */

void
colormap_combine(Gif_Colormap *dst, Gif_Colormap *src)
{
  Gif_Color *src_col, *dst_col;
  int i, j;

  /* expand dst->col if necessary. This might change dst->col */
  if (dst->ncol + src->ncol >= dst->capacity) {
    dst->capacity *= 2;
    Gif_ReArray(dst->col, Gif_Color, dst->capacity);
  }

  src_col = src->col;
  dst_col = dst->col;
  for (i = 0; i < src->ncol; i++, src_col++) {
    for (j = 1; j < dst->ncol; j++) {
      if (GIF_COLOREQ(src_col, &dst_col[j]))
	goto found;
    }
    dst_col[j] = *src_col;
    dst_col[j].pixel = 0;
    dst->ncol++;
   found:
    src_col->pixel = j;
  }
}


/* sort_permutation: sorts a given permutation `perm' according to the
   corresponding values in `values'. Thus, in the output, the sequence
   `[ values[perm[i]] | i <- 0..size-1 ]' will be monotonic, either up or
   (if is_down != 0) down. */

static u_int16_t *permuting_sort_values;

static int
permuting_sorter_up(const void *v1, const void *v2)
{
  const u_int16_t *n1 = (const u_int16_t *)v1;
  const u_int16_t *n2 = (const u_int16_t *)v2;
  return (permuting_sort_values[*n1] - permuting_sort_values[*n2]);
}

static int
permuting_sorter_down(const void *v1, const void *v2)
{
  const u_int16_t *n1 = (const u_int16_t *)v1;
  const u_int16_t *n2 = (const u_int16_t *)v2;
  return (permuting_sort_values[*n2] - permuting_sort_values[*n1]);
}

static u_int16_t *
sort_permutation(u_int16_t *perm, u_int16_t size, u_int16_t *values,
		 int is_down)
{
  permuting_sort_values = values;
  if (is_down)
    qsort(perm, size, sizeof(u_int16_t), permuting_sorter_down);
  else
    qsort(perm, size, sizeof(u_int16_t), permuting_sorter_up);
  permuting_sort_values = 0;
  return perm;
}


/*****
 * MANIPULATING IMAGE AREAS
 **/

static void
copy_data_area(u_int16_t *dst, u_int16_t *src, Gif_Image *area)
{
  int y, width, height;
  if (!area) return;
  width = area->width;
  height = area->height;
  dst += area->top * screen_width + area->left;
  src += area->top * screen_width + area->left;
  for (y = 0; y < height; y++) {
    memcpy(dst, src, sizeof(u_int16_t) * width);
    dst += screen_width;
    src += screen_width;
  }
}

static void
fill_data_area(u_int16_t *dst, u_int16_t value, Gif_Image *area)
{
  int x, y;
  int width = area->width;
  int height = area->height;
  dst += area->top * screen_width + area->left;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++)
      dst[x] = value;
    dst += screen_width;
  }
}

static void
erase_screen(u_int16_t *dst)
{
  u_int32_t i;
  u_int32_t screen_size = screen_width * screen_height;
  for (i = 0; i < screen_size; i++)
    *dst++ = background;
}

/*****
 * APPLY A GIF FRAME OR DISPOSAL TO AN IMAGE DESTINATION
 **/

static void
apply_frame(u_int16_t *dst, Gif_Image *gfi, int replace)
{
  int i, y;
  u_int16_t map[256];
  Gif_Colormap *colormap = gfi->local ? gfi->local : in_global_map;
  
  /* make sure transparency maps to TRANSP */
  for (i = 0; i < colormap->ncol; i++)
    map[i] = colormap->col[i].pixel;
  for (i = colormap->ncol; i < 256; i++)
    map[i] = TRANSP;
  if (gfi->transparent >= 0 && gfi->transparent < 256)
    map[gfi->transparent] = TRANSP;
  else
    replace = 1;
  
  /* map the image */
  dst += gfi->left + gfi->top * screen_width;
  for (y = 0; y < gfi->height; y++) {
    byte *gfi_pointer = gfi->img[y];
    int x;
    
    if (replace)
      for (x = 0; x < gfi->width; x++)
	dst[x] = map[gfi_pointer[x]];
    else
      for (x = 0; x < gfi->width; x++) {
	u_int16_t new_pixel = map[gfi_pointer[x]];
	if (new_pixel != TRANSP) dst[x] = new_pixel;
      }
    
    dst += screen_width;
  }
}

static void
apply_frame_disposal(u_int16_t *into_data, u_int16_t *from_data,
		     Gif_Image *gfi)
{
  if (gfi->disposal == GIF_DISPOSAL_NONE
      || gfi->disposal == GIF_DISPOSAL_ASIS)
    copy_data_area(into_data, from_data, gfi);
  
  else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
    fill_data_area(into_data, background, gfi);
}


/*****
 * FIND THE SMALLEST BOUNDING RECTANGLE ENCLOSING ALL CHANGES
 **/

/* find_difference_bounds: Find the smallest rectangular area containing all
   the changes and store it in `bounds'. */

static void
find_difference_bounds(Gif_OptData *bounds)
{
  int lf, rt, tp, bt, x, y;
  
  for (tp = 0; tp < screen_height; tp++)
    if (memcmp(last_data + screen_width * tp, this_data + screen_width * tp,
	       screen_width * sizeof(u_int16_t)) != 0)
      break;
  for (bt = screen_height - 1; bt >= tp; bt--)
    if (memcmp(last_data + screen_width * bt, this_data + screen_width * bt,
	       screen_width * sizeof(u_int16_t)) != 0)
      break;
  
  lf = screen_width;
  rt = 0;
  for (y = tp; y <= bt; y++) {
    u_int16_t *ld = last_data + screen_width * y;
    u_int16_t *td = this_data + screen_width * y;
    for (x = 0; x < lf; x++)
      if (ld[x] != td[x])
	break;
    lf = x;
    
    for (x = screen_width - 1; x > rt; x--)
      if (ld[x] != td[x])
	break;
    rt = x;
  }
  
  bounds->left = lf;
  bounds->top = tp;
  bounds->width = rt + 1 - lf;
  bounds->height = bt + 1 - tp;
}


/* expand_difference_bounds: If the current image has background disposal and
   the background is transparent, we must expand the difference bounds to
   include any blanked (newly transparent) pixels that are still transparent
   in the next image. This function does that by comparing this_data and
   next_data. The new bounds are passed and stored in `bounds'; the image's
   old bounds, which are also the maximum bounds, are passed in
   `this_bounds'. */

static void
expand_difference_bounds(Gif_OptData *bounds, Gif_Image *this_bounds)
{
  int x, y;
  
  int lf = bounds->left, tp = bounds->top;
  int rt = lf + bounds->width - 1, bt = tp + bounds->height - 1;
  
  int tlf = this_bounds->left, ttp = this_bounds->top;
  int trt = tlf + this_bounds->width - 1, tbt = ttp + this_bounds->height - 1;

  if (lf > rt || tp > bt)
    lf = 0, tp = 0, rt = screen_width - 1, bt = screen_height - 1;
  
  for (y = ttp; y < tp; y++) {
    u_int16_t *now = this_data + screen_width * y;
    u_int16_t *next = next_data + screen_width * y;
    for (x = tlf; x <= trt; x++)
      if (now[x] != TRANSP && next[x] == TRANSP)
	goto found_top;
  }
 found_top:
  tp = y;
  
  for (y = tbt; y > bt; y--) {
    u_int16_t *now = this_data + screen_width * y;
    u_int16_t *next = next_data + screen_width * y;
    for (x = tlf; x <= trt; x++)
      if (now[x] != TRANSP && next[x] == TRANSP)
	goto found_bottom;
  }
 found_bottom:
  bt = y;
  
  for (x = tlf; x < lf; x++) {
    u_int16_t *now = this_data + x;
    u_int16_t *next = next_data + x;
    for (y = tp; y <= bt; y++)
      if (now[y*screen_width] != TRANSP && next[y*screen_width] == TRANSP)
	goto found_left;
  }
 found_left:
  lf = x;
  
  for (x = trt; x > rt; x--) {
    u_int16_t *now = this_data + x;
    u_int16_t *next = next_data + x;
    for (y = tp; y <= bt; y++)
      if (now[y*screen_width] != TRANSP && next[y*screen_width] == TRANSP)
	goto found_right;
  }
 found_right:
  rt = x;
  
  bounds->left = lf;
  bounds->top = tp;
  bounds->width = rt + 1 - lf;
  bounds->height = bt + 1 - tp;
}


/* fix_difference_bounds: make sure the image isn't 0x0. */

static void
fix_difference_bounds(Gif_OptData *bounds)
{
  if (bounds->width == 0 || bounds->height == 0) {
    bounds->top = 0;
    bounds->left = 0;
    bounds->width = 1;
    bounds->height = 1;
  }
  /* assert that image lies completely within screen */
  assert(bounds->top < screen_height && bounds->left < screen_width
	 && bounds->top + bounds->height - 1 < screen_height
	 && bounds->left + bounds->width - 1 < screen_width);
}


/*****
 * DETERMINE WHICH COLORS ARE USED
 **/

#define REQUIRED	2
#define REPLACE_TRANSP	1

/* get_used_colors: mark which colors are needed by a given image. Returns a
   need array so that need[j] == REQUIRED if the output colormap must
   include all_color j; REPLACE_TRANSP if it should be replaced by
   transparency; and 0 if it's not in the image at all.
   
   If use_transparency > 0, then a pixel which was the same in the last frame
   may be replaced with transparency. If use_transparency == 2, transparency
   MUST be set. (This happens on the first image if the background should be
   transparent.) */

static void
get_used_colors(Gif_OptData *bounds, int use_transparency)
{
  int top = bounds->top, width = bounds->width, height = bounds->height;
  int i, x, y;
  int all_ncol = all_colormap->ncol;
  byte *need = Gif_NewArray(byte, all_ncol);
  
  for (i = 0; i < all_ncol; i++)
    need[i] = 0;
  
  /* set elements that are in the image. need == 2 means the color
     must be in the map; need == 1 means the color may be replaced by
     transparency. */
  for (y = top; y < top + height; y++) {
    u_int16_t *data = this_data + screen_width * y + bounds->left;
    u_int16_t *last = last_data + screen_width * y + bounds->left;
    for (x = 0; x < width; x++) {
      if (data[x] != last[x])
	need[data[x]] = REQUIRED;
      else if (need[data[x]] == 0)
	need[data[x]] = REPLACE_TRANSP;
    }
  }
  if (need[TRANSP] || use_transparency == 2)
    need[TRANSP] = REQUIRED;
  
  /* check for too many colors; also force transparency if needed */
  {
    int count[3];
    /* Count distinct pixels in each category */
    count[0] = count[1] = count[2] = 0;
    for (i = 0; i < all_ncol; i++)
      count[need[i]]++;
    /* If too many "potentially transparent" pixels, force transparency */
    if (count[REPLACE_TRANSP] + count[REQUIRED] > 256)
      use_transparency = 1;
    /* Make sure transparency is marked necessary if we use it */
    if (count[REPLACE_TRANSP] > 0 && use_transparency && !need[TRANSP]) {
      need[TRANSP] = REQUIRED;
      count[REQUIRED]++;
    }
    /* If not using transparency, change "potentially transparent" pixels to
       "actually used" pixels */
    if (!use_transparency) {
      for (i = 0; i < all_ncol; i++)
	if (need[i] == REPLACE_TRANSP) need[i] = REQUIRED;
      count[REQUIRED] += count[REPLACE_TRANSP];
    }
    /* If too many "actually used" pixels, fail miserably */
    if (count[REQUIRED] > 256)
      fatal_error("more than 256 colors in a frame");
    /* If we can afford to have transparency, and we want to use it, then
       include it */
    if (count[REQUIRED] < 256 && use_transparency && !need[TRANSP]) {
      need[TRANSP] = REQUIRED;
      count[REQUIRED]++;
    }
    bounds->required_color_count = count[REQUIRED];
  }
  
  bounds->needed_colors = need;
}


/*****
 * FIND SUBIMAGES AND COLORS USED
 **/

static void
create_subimages(Gif_Stream *gfs, int optimize_level)
{
  int screen_size;
  Gif_Image *last_gfi;
  int next_data_valid;
  
  screen_size = screen_width * screen_height;
  
  next_data = Gif_NewArray(u_int16_t, screen_size);
  next_data_valid = 0;
  
  /* do first image. Remember to uncompress it if necessary */
  erase_screen(last_data);
  erase_screen(this_data);
  last_gfi = 0;
  
  /* PRECONDITION: last_data -- garbage
     this_data -- equal to image data for previous image
     next_data -- equal to image data for next image if next_image_valid */
  for (image_index = 0; image_index < gfs->nimages; image_index++) {
    Gif_Image *gfi = gfs->images[image_index];
    Gif_OptData *subimage = new_opt_data();
    
    if (!gfi->img) Gif_UncompressImage(gfi);
    Gif_ReleaseCompressedImage(gfi);
    
    /* set up last_data to be equal to the last image */
    if (!last_gfi) {
      /* no last image; do nothing */
    } else if (last_gfi->disposal == GIF_DISPOSAL_ASIS
	       || last_gfi->disposal == GIF_DISPOSAL_NONE) {
      u_int16_t *temp = last_data;
      last_data = this_data;
      this_data = temp;
    } else if (last_gfi->disposal == GIF_DISPOSAL_BACKGROUND) {
      fill_data_area(last_data, background, last_gfi);
    }
    
    /* set up this_data to be equal to the current image */
    if (next_data_valid) {
      u_int16_t *temp = this_data;
      this_data = next_data;
      next_data = temp;
      next_data_valid = 0;
    } else {
      copy_data_area(this_data, last_data, last_gfi);
      apply_frame(this_data, gfi, 0);
    }
    
    /* find minimum area of difference between this image and last image */
    subimage->disposal = GIF_DISPOSAL_ASIS;
    if (image_index > 0)
      find_difference_bounds(subimage);
    else {
      subimage->left = gfi->left;
      subimage->top = gfi->top;
      subimage->width = gfi->width;
      subimage->height = gfi->height;
    }
    
    /* might need to expand difference border if transparent background &
       background disposal */
    if (gfi->disposal == GIF_DISPOSAL_BACKGROUND
	&& background == TRANSP
	&& image_index < gfs->nimages - 1) {
      /* set up next_data */
      Gif_Image *next_gfi = gfs->images[image_index + 1];
      memcpy(next_data, this_data, screen_size * sizeof(u_int16_t));
      apply_frame_disposal(next_data, this_data, gfi);
      apply_frame(next_data, next_gfi, 0);
      /* expand border as necessary */
      expand_difference_bounds(subimage, gfi);
      subimage->disposal = GIF_DISPOSAL_BACKGROUND;
    }
    
    fix_difference_bounds(subimage);
    
    /* set map of used colors */
    {
      int use_transparency = optimize_level > 1 && image_index > 0;
      if (image_index == 0 && background == TRANSP)
	use_transparency = 2;
      get_used_colors(subimage, use_transparency);
    }
    
    gfi->userdata = subimage;
    last_gfi = gfi;
  }
  
  Gif_DeleteArray(next_data);
}


/*****
 * CALCULATE OUTPUT GLOBAL COLORMAP
 **/

/* choose_256_colors: If we need to have some local colormaps, then choose for
   the global colormap an optimal subset of all the colors, to minimize the
   size of extra local colormaps required.
   
   On return, the `global_penalty' component of an image's Gif_OptData
   structure is 0 iff that image will need a local colormap. */

static void
choose_256_colors(Gif_Stream *gfs, u_int16_t *global_all)
{
  int i, imagei;
  int all_ncol = all_colormap->ncol;
  u_int16_t *penalty = Gif_NewArray(u_int16_t, all_ncol);
  u_int16_t *ordering = Gif_NewArray(u_int16_t, all_ncol);
  int nordering = all_ncol - 1;
  int penalties_changed;
  
  for (i = 0; i < nordering; i++)
    ordering[i] = i + 1;
  
  /* choose appropriate penalties for each image */
  for (imagei = 0; imagei < gfs->nimages; imagei++) {
    Gif_OptData *optdata = (Gif_OptData *)gfs->images[imagei]->userdata;
    optdata->global_penalty = 1;
    for (i = 1; i < optdata->required_color_count; i *= 2)
      optdata->global_penalty *= 3;
  }
  
  /* set initial penalties for each color */
  for (i = 0; i < all_ncol; i++)
    penalty[i] = 0;
  for (imagei = 0; imagei < gfs->nimages; imagei++) {
    Gif_OptData *optdata = (Gif_OptData *)gfs->images[imagei]->userdata;
    byte *need = optdata->needed_colors;
    int this_penalty = optdata->global_penalty;
    for (i = 0; i < all_ncol; i++)
      if (need[i] == REQUIRED)
	penalty[i] += this_penalty;
  }

  /* be careful about the background!! which MUST be in the global colormap */
  if (background != TRANSP)
    penalty[background] = 0xFFFFU;

  /* loop, removing the most useless color each time, until we have exactly
     256 colors */
  penalties_changed = 1;
  while (nordering > 256) {
    int removed_color;
    
    if (penalties_changed)
      sort_permutation(ordering, nordering, penalty, 1);
    
    /* remove the color which is least expensive to remove */
    removed_color = ordering[nordering - 1];
    nordering--;

    /* adjust penalties. if an image now must have a local colormap, then any
       penalty values for its other colors shouldn't be considered */
    penalties_changed = 0;
    for (imagei = 0; imagei < gfs->nimages; imagei++) {
      Gif_OptData *optdata = (Gif_OptData *)gfs->images[imagei]->userdata;
      byte *need = optdata->needed_colors;
      int this_penalty = optdata->global_penalty;
      
      if (this_penalty == 0 || need[removed_color] != REQUIRED)
	continue;
      
      for (i = 0; i < all_ncol; i++)
	if (need[i] == REQUIRED)
	  penalty[i] -= this_penalty;
      optdata->global_penalty = 0;
      penalties_changed = 1;
    }
  }

  for (i = 0; i < 256; i++)
    global_all[i] = ordering[i];
  
  Gif_DeleteArray(ordering);
  Gif_DeleteArray(penalty);
}

/* create_out_global_map: The interface function to this pass. It creates
   out_global_map and sets pixel values on all_colormap appropriately.
   Specifically:
   
   all_colormap->col[P].pixel >= 256 ==> P is not in the global colormap.
   
   Otherwise, all_colormap->col[P].pixel == some J so that
   GIF_COLOREQ(&all_colormap->col[P], &out_global_map->col[J]). */

static void
create_out_global_map(Gif_Stream *gfs)
{
  int i, imagei;
  int all_ncol = all_colormap->ncol;
  u_int16_t *global_all = Gif_NewArray(u_int16_t, 256);
  int nglobal_all;

  /* 1. First determine which colors should be in the global colormap. Do this
     by calculating the penalty should a color NOT be in the global colormap;
     then choose the lowest of these.
     
     Note: Don't need to do this if there are <=256 colors overall. */
  
  if (all_ncol - 1 > 256) {
    nglobal_all = 256;
    choose_256_colors(gfs, global_all);
  } else {
    nglobal_all = all_ncol - 1;
    for (i = 0; i < nglobal_all; i++)
      global_all[i] = i + 1;
    /* Depend on each image's global_penalty being != 0 by default */
  }

  /* Now, reorder global colors. Colors used in a lot of images should appear
     first in the global colormap; then those images might be able to use a
     smaller min_code_size, since the colors they need are clustered at the
     beginning of the colormap. The strategy used here -- just count in how
     many images a pixel is used and sort on that -- isn't strictly optimal,
     but it does well for most images. */
  {
    u_int16_t *rank = Gif_NewArray(u_int16_t, all_ncol);
    for (i = 0; i < all_ncol; i++)
      rank[i] = 0;
    
    for (imagei = 0; imagei < gfs->nimages; imagei++) {
      Gif_OptData *optdata = (Gif_OptData *)gfs->images[imagei]->userdata;
      byte *need = optdata->needed_colors;
      /* ignore images that will require a local colormap */
      if (!optdata->global_penalty)
	continue;
      for (i = 1; i < all_ncol; i++)
	if (need[i] == REQUIRED)
	  rank[i]++;
    }
    
    sort_permutation(global_all, nglobal_all, rank, 1);
    
    Gif_DeleteArray(rank);
  }
  
  /* Finally, make out_global_map. */
  out_global_map = Gif_NewFullColormap(nglobal_all, 256);
  
  for (i = 0; i < all_ncol; i++)
    all_colormap->col[i].pixel = NOT_IN_OUT_GLOBAL;
  for (i = 0; i < nglobal_all; i++) {
    int all_i = global_all[i];
    out_global_map->col[i] = all_colormap->col[all_i];
    all_colormap->col[all_i].pixel = i;
  }
  
  /* set the stream's background color */
  if (background != TRANSP) {
    assert(all_colormap->col[background].pixel < nglobal_all);
    gfs->background = all_colormap->col[background].pixel;
  }
  
  Gif_DeleteArray(global_all);  
}


/*****
 * CREATE COLOR MAPPING FOR A PARTICULAR IMAGE
 **/

/* prepare_colormap_map: Create and return an array of bytes mapping from
   global pixel values to pixel values for this image. It may add colormap
   cells to `into'; if there isn't enough room in `into', it will return 0.
   It sets the `transparent' field of `gfi', but otherwise doesn't change or
   read it at all. */

static byte *
prepare_colormap_map(Gif_Image *gfi, Gif_Colormap *into, byte *need)
{
  int i;
  int is_global = (into == out_global_map);
  
  int all_ncol = all_colormap->ncol;
  Gif_Color *all_col = all_colormap->col;
  
  int ncol = into->ncol;
  Gif_Color *col = into->col;
  
  byte *map = Gif_NewArray(byte, all_ncol);
  byte into_used[256];
  
  /* keep track of which pixel indices in `into' have been used; initially,
     all unused */
  for (i = 0; i < 256; i++)
    into_used[i] = 0;

  /* go over all non-transparent global pixels which MUST appear
     (need[P]==REQUIRED) and place them in `into' */
  for (i = 1; i < all_ncol; i++) {
    int val;
    if (need[i] != REQUIRED)
      continue;
    
    /* fail if a needed pixel isn't in the global map */
    if (is_global) {
      val = all_col[i].pixel;
      if (val >= ncol) goto error;
    } else {
      /* always place colors in a local colormap */    
      if (ncol == 256) goto error;
      val = ncol;
      ncol++;
      col[val] = all_col[i];
    }

    map[i] = val;
    into_used[val] = 1;
  }
  
  /* now check for transparency */
  gfi->transparent = -1;
  if (need[TRANSP]) {
    int transparent = -1;
    
    /* first, look for an unused index in `into'. Pick the lowest one: the
       lower transparent index we get, the more likely we can shave a bit off
       min_code_bits later, thus saving space */
    for (i = 0; i < ncol; i++)
      if (!into_used[i]) {
	transparent = i;
	break;
      }
    
    /* otherwise, add another slot to `into'; it's pure transparent. If
       there's no room for it, then give up */
    if (transparent < 0) {
      if (is_global && all_col[TRANSP].pixel < NOT_IN_OUT_GLOBAL)
	transparent = all_col[TRANSP].pixel;
      else if (ncol < 256) {
	transparent = ncol;
	ncol++;
	if (is_global) all_col[TRANSP].pixel = transparent;
      } else
	goto error;
    }
    
    /* change mapping */
    map[TRANSP] = transparent;
    for (i = 1; i < all_ncol; i++)
      if (need[i] == REPLACE_TRANSP)
	map[i] = transparent;
    
    gfi->transparent = transparent;
  }
  
  /* If we get here, it worked! Commit state changes (the number of color
     cells in `into') and return the map. */
  into->ncol = ncol;
  return map;
  
 error:
  /* If we get here, it failed! Return 0 and don't change global state. */
  Gif_DeleteArray(map);
  return 0;
}


/* prepare_colormap: make a colormap up from the image data by fitting any
   used colors into a colormap. Returns a map from global color index to index
   in this image's colormap. May set a local colormap on `gfi'. */

static byte *
prepare_colormap(Gif_Image *gfi, byte *need)
{
  byte *map;
  
  /* map pixel values into the global colormap; if that doesn't work, add a
     local colormap */
  Gif_DeleteColormap(gfi->local);
  gfi->local = 0;
  map = prepare_colormap_map(gfi, out_global_map, need);
  if (!map) {
    gfi->local = Gif_NewFullColormap(0, 256);
    map = prepare_colormap_map(gfi, gfi->local, need);
  }
  
  return map;
}


/*****
 * CREATE OUTPUT FRAME DATA
 **/

/* simple_frame_data: just copy the data from the image into the frame data.
   No funkiness, no transparency, nothing */

static void
simple_frame_data(Gif_Image *gfi, byte *map)
{
  int top = gfi->top, width = gfi->width, height = gfi->height;
  int x, y;
  
  for (y = 0; y < height; y++) {
    u_int16_t *from = this_data + screen_width * (y+top) + gfi->left;
    byte *into = gfi->image_data + y * width;
    for (x = 0; x < width; x++)
      *into++ = map[*from++];
  }
}


#if OPTIMIZED_TRANSPARENCY

/*****
 * OPTIMIZED TRANSPARENCY: This lovely bit of code tries to create
 * transparency "optimally" -- according to a greedy algorithm (if a certain
 * stretch compresses better NOW w/o transparency, compress it w/o
 * transparency, not knowing or caring if that decision will later turn out
 * to be a bad one).
 *
 * The previous simple heuristic below seems to do better.
 **/

#define COMP_HASH_SIZE		(2 << GIF_MAX_CODE_BITS)
static Gif_Code *comp_prefix;
static byte *comp_suffix;
static Gif_Code *comp_code;
static u_int16_t *comp_code_length;
static u_int32_t *comp_hash;

static u_int32_t
comp_code_hash(Gif_Code prefix, byte suffix, Gif_Code eoi_code)
{
  u_int32_t hashish1 = (prefix << 9 ^ prefix << 2 ^ suffix) % COMP_HASH_SIZE;
  u_int32_t hashish2 = ((prefix | suffix << 7) % COMP_HASH_SIZE) | 1;
  Gif_Code *prefixes = comp_prefix;
  byte *suffixes = comp_suffix;

  while ((prefixes[hashish1] != prefix || suffixes[hashish1] != suffix)
	 && prefixes[hashish1] != eoi_code)
    hashish1 = (hashish1 + hashish2) % COMP_HASH_SIZE;
  
  return hashish1;
}


#define ACTIVE_CODES_SIZE	256

typedef struct Gif_ActiveCode {
  Gif_Code code;
  int ntransp;
  struct Gif_ActiveCode *next;
} Gif_ActiveCode;

static Gif_ActiveCode *comp_active_bank;
static Gif_ActiveCode *comp_active_free;

static void
allocate_active_codes(void)
{
  int i;
  Gif_ActiveCode *ac = Gif_NewArray(Gif_ActiveCode, 256);
  for (i = 1; i < 255; i++)
    ac[i].next = &ac[i+1];
  ac[255].next = comp_active_free;
  ac[0].next = comp_active_bank;
  comp_active_free = &ac[1];
  comp_active_bank = &ac[0];
}

static void
destroy_all_active_codes(void)
{
  Gif_ActiveCode *ac = comp_active_bank;
  while (ac) {
    Gif_ActiveCode *next = ac->next;
    Gif_DeleteArray(ac);
    ac = next;
  }
  comp_active_free = 0;
  comp_active_bank = 0;
}

#ifdef __GNUC__
__inline__
#endif
static Gif_ActiveCode *
new_active_code(void)
{
  Gif_ActiveCode *a;
  if (!comp_active_free) allocate_active_codes();
  a = comp_active_free;
  comp_active_free = a->next;
  return a;
}


static Gif_ActiveCode *comp_active_head;
static Gif_ActiveCode *comp_active_tail;

static void
scan_active_codes(Gif_ActiveCode *head, byte suffix, Gif_Code eoi_code,
		  int transparent)
{
  Gif_ActiveCode *ac;
  for (ac = head; ac; ac = ac->next) {
    int hash = comp_code_hash(ac->code, suffix, eoi_code);
    if (comp_prefix[hash] != eoi_code) {
      Gif_ActiveCode *nc = new_active_code();
      nc->code = comp_code[hash];
      nc->ntransp = ac->ntransp + (suffix == transparent);
      nc->next = comp_active_head;
      comp_active_head = nc;
      if (!comp_active_tail) comp_active_tail = nc;
    }
  }
}

static Gif_Code
best_active_code(Gif_ActiveCode *head)
{
  Gif_ActiveCode *best = head, *ac;
  for (ac = head->next; ac; ac = ac->next)
    if (ac->ntransp > best->ntransp)
      best = ac;
  return best->code;
}

static void
optimized_transp_frame_data(Gif_Image *gfi, byte *map)
{
  int x_pos, height;
  u_int32_t hash;
  byte min_code_bits, cur_code_bits;
  
  Gif_Code work_code, output_code, next_code;
  Gif_Code bump_code, clear_code, eoi_code;
  
  long num_codes_output = 0;
  
  u_int16_t *last_ptr, *this_ptr;
  byte *out_ptr;
  int transparent = gfi->transparent;
  
  {
    Gif_Colormap *gfcm = (gfi->local ? gfi->local : out_global_map);
    int i;
    min_code_bits = 2;		/* min_code_bits of 1 isn't allowed */
    for (i = 4; i < gfcm->ncol; i *= 2)
      min_code_bits++;
  }
  
  clear_code = 1 << min_code_bits;
  eoi_code = clear_code + 1;
  cur_code_bits = min_code_bits + 1;
  comp_code_length[clear_code] = comp_code_length[eoi_code] = 0;
  
  work_code = output_code = clear_code;
  /* Because output_code is clear_code, we'll initialize next_code, bump_code,
     et al. below. */
  
  last_ptr = last_data + (gfi->top * screen_width) + gfi->left;
  this_ptr = this_data + (gfi->top * screen_width) + gfi->left;
  out_ptr = gfi->image_data;
  height = gfi->height;
  x_pos = 0;
  
  while (1) {
    
    /* must write data into image here */
    num_codes_output++;
    {
      Gif_Code c = output_code;
      byte *output;
      output = out_ptr = out_ptr + comp_code_length[output_code];
      while (c != clear_code && c != eoi_code) {
	hash = comp_hash[c];
	output--;
	*output = comp_suffix[hash];
	c = comp_prefix[hash];
      }
    }
    
    /* output a code */
    if (output_code == clear_code) {
      
      Gif_Code c;
      cur_code_bits = min_code_bits + 1;
      next_code = eoi_code + 1;
      bump_code = clear_code << 1;
      
      for (hash = 0; hash < COMP_HASH_SIZE; hash++)
	comp_prefix[hash] = eoi_code;
      for (c = 0; c < clear_code; c++) {
	hash = comp_code_hash(clear_code, c, eoi_code);
	comp_prefix[hash] = clear_code;
	comp_suffix[hash] = c;
	comp_code[hash] = c;
	comp_hash[c] = hash;
	comp_code_length[c] = 1;
      }
      
    } else if (next_code > bump_code) {
      
      /* bump up compression size */
      if (cur_code_bits == GIF_MAX_CODE_BITS) {
	output_code = clear_code;
	continue;
      } else {
	cur_code_bits++;
	bump_code <<= 1;
      }
      
    } else if (output_code == eoi_code)
      break;
    
    
    /*****
     * Find the next code to output. */
    
    if (height == 0) {
      /* There may be one pixel left.
	 If there is not, work_code == eoi_code.
	 If there is, then work_code contains it. We output it, but the next
	 time through, work_code will be eoi_code as we want. */
      output_code = work_code;
      work_code = eoi_code;
      continue;
    }
    
    /* Actual code finding.
       This part was changed to support optimization. We keep a set of all
       the current runs that match. */
    comp_active_head = comp_active_tail = new_active_code();
    comp_active_head->code = work_code;
    comp_active_head->ntransp = 0;
    comp_active_head->next = 0;
    
    while (height != 0) {
      Gif_ActiveCode *old_head = comp_active_head;
      Gif_ActiveCode *old_tail = comp_active_tail;
      byte suffix;
      comp_active_head = comp_active_tail = 0;
      
      suffix = map[*this_ptr];
      scan_active_codes(old_head, suffix, eoi_code, transparent);
      if (*this_ptr == *last_ptr && suffix != transparent && transparent > 0) {
	suffix = transparent;
	scan_active_codes(old_head, suffix, eoi_code, transparent);
      }
      
      last_ptr++, this_ptr++, x_pos++;
      if (x_pos == gfi->width) {
	height--;
	last_ptr += screen_width - gfi->width;
	this_ptr += screen_width - gfi->width;
	x_pos = 0;
      }
      
      if (comp_active_head == 0) {
	/* We need to add something to the table. */
	work_code = best_active_code(old_head);
	hash = comp_code_hash(work_code, suffix, eoi_code);
	comp_prefix[hash] = work_code;
	comp_suffix[hash] = suffix;
	comp_code[hash] = next_code;
	comp_hash[next_code] = hash;
	comp_code_length[next_code] = 1 + comp_code_length[work_code];
	//assert(hash>=0 && hash<COMP_HASH_SIZE && next_code>=0 && next_code<=GIF_MAX_CODE);
	next_code++;
	
	output_code = work_code;
	work_code = suffix;	/* code for a pixel == the pixel value */
	
	old_tail->next = comp_active_free;
	comp_active_free = old_head;
	
	goto done;
      }
      
      old_tail->next = comp_active_free;
      comp_active_free = old_head;
    }
    
    /* Ran out of data and don't need to add anything to the table. */
    output_code = best_active_code(comp_active_head);
    work_code = eoi_code;
    comp_active_tail->next = comp_active_free;
    comp_active_free = comp_active_head;
    
   done:
    ;
  }
}

#endif


/* transp_frame_data: copy the frame data into the actual image, using
   transparency occasionally according to a heuristic described below */

static void
transp_frame_data(Gif_Stream *gfs, Gif_Image *gfi, byte *map)
{
  int top = gfi->top, width = gfi->width, height = gfi->height;
  int x, y;
  int transparent = gfi->transparent;
  
  /* First, try w/o transparency. Compare this to the result using
     transparency and pick the better of the two. */
  simple_frame_data(gfi, map);
  Gif_CompressImage(gfs, gfi);
  
  /* Actually copy data to frame.
    
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
    on some images, but does *worse than nothing at all* on others.
    
    However, it DOES do better than the complicated, greedy algorithm I
    commented out above; and now we pick either the transparency-optimized
    version or the normal version, whichever compresses smaller, for the best
    of both worlds. (9/98) */
  
  for (y = 0; y < height; y++) {
    u_int16_t *last = last_data + screen_width * (y+top) + gfi->left;
    u_int16_t *cur = this_data + screen_width * (y+top) + gfi->left;
    byte *data = gfi->img[y];
    int lastpixel = *last;
    int pixelcount;
    
    x = 0;
    
   normal_runs:
    /* Copy pixels (no transparency); measure runs which could be
       made transparent. */
    pixelcount = 0;
    for (; x < width; x++, last++, cur++, data++) {
      *data = map[*cur];
      
      /* If this pixel could be transparent... */
      if (map[*cur] == transparent)
	goto combine_transparent_runs;
      else if (*cur == *last) {
	if (*cur == lastpixel)
	  /* within a transparent run */
	  pixelcount++;
	else if (pixelcount > 0)
	  /* Ooo!! enough adjacent transparent runs -- go & combine them */
	  goto combine_transparent_runs;
	else {
	  /* starting a new transparent run */
	  lastpixel = *cur;
	  pixelcount++;
	}
	
      } else
	pixelcount = 0;
    }
    /* If we get here, we've finished up the row. */
    goto row_finished;
    
   combine_transparent_runs:
    /* Here's where we make transparent runs. */
    memset(data - pixelcount, transparent, pixelcount);
    for (; x < width; x++, last++, cur++, data++) {
      if (*last == *cur || map[*cur] == transparent)
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
  
  /* Now, try w/transparency and pick the better of the two. */
  {
    byte *old_compressed = gfi->compressed;
    void (*old_free_compressed)(void *) = gfi->free_compressed;
    u_int32_t old_compressed_len = gfi->compressed_len;
    gfi->compressed = 0;	/* prevent freeing old_compressed */
    Gif_CompressImage(gfs, gfi);
    if (gfi->compressed_len > old_compressed_len) {
      Gif_ReleaseCompressedImage(gfi);
      gfi->compressed = old_compressed;
      gfi->free_compressed = old_free_compressed;
      gfi->compressed_len = old_compressed_len;
    } else
      (*old_free_compressed)(old_compressed);
    Gif_ReleaseUncompressedImage(gfi);
  }
}


/*****
 * CREATE NEW IMAGE DATA
 **/

static void
create_new_image_data(Gif_Stream *gfs, int optimize_level)
{
  Gif_Image *last_gfi;
  
  gfs->global = out_global_map;
  
  /* do first image. Remember to uncompress it if necessary */
  erase_screen(last_data);
  erase_screen(this_data);
  last_gfi = 0;
  
  /* PRECONDITION: last_data -- garbage
     this_data -- equal to image data for previous image
     next_data -- equal to image data for next image if next_image_valid */
  for (image_index = 0; image_index < gfs->nimages; image_index++) {
    Gif_Image *new_gfi = gfs->images[image_index];
    Gif_OptData *optdata = (Gif_OptData *)new_gfi->userdata;
    
    /* set up last_data to be equal to the last image */
    if (!last_gfi) {
      /* do nothing */
    } else if (last_gfi->disposal == GIF_DISPOSAL_ASIS
	       || last_gfi->disposal == GIF_DISPOSAL_NONE) {
      u_int16_t *temp = last_data;
      last_data = this_data;
      this_data = temp;
    } else if (last_gfi->disposal == GIF_DISPOSAL_BACKGROUND) {
      fill_data_area(last_data, background, last_gfi);
    }
    
    /* set up this_data to be equal to the current image */
    copy_data_area(this_data, last_data, last_gfi);
    apply_frame(this_data, new_gfi, 0);
    
    /* set bounds and disposal from optdata */
    Gif_ReleaseUncompressedImage(new_gfi);
    new_gfi->left = optdata->left;
    new_gfi->top = optdata->top;
    new_gfi->width = optdata->width;
    new_gfi->height = optdata->height;
    new_gfi->disposal = optdata->disposal;
    if (image_index > 0) new_gfi->interlace = 0;
    
    /* find the new image's colormap and then make new data */
    {
      byte *map = prepare_colormap(new_gfi, optdata->needed_colors);
      byte *data = Gif_NewArray(byte, new_gfi->width * new_gfi->height);
      new_gfi->image_data = data;
      new_gfi->free_image_data = Gif_DeleteArrayFunc;
      Gif_MakeImg(new_gfi, data, 0);
      
      /* don't use transparency on first frame */
      if (optimize_level > 1 && image_index > 0)
	transp_frame_data(gfs, new_gfi, map);
	/*optimized_transp_frame_data(new_gfi, map);*/
      else
	simple_frame_data(new_gfi, map);
      
      Gif_DeleteArray(map);
    }
    
    delete_opt_data(optdata);
    new_gfi->userdata = 0;
    
    last_gfi = new_gfi;
  }
}


/*****
 * INITIALIZATION AND FINALIZATION
 **/

static int
initialize_optimizer(Gif_Stream *gfs, int optimize_level)
{
  int i, screen_size;
  
  if (gfs->nimages <= 1)
    return 0;
  
  /* combine colormaps */
  all_colormap = Gif_NewFullColormap(1, 384);
  all_colormap->col[0].red = 255;
  all_colormap->col[0].green = 255;
  all_colormap->col[0].blue = 255;
  
  in_global_map = gfs->global;
  if (!in_global_map) {
    Gif_Color *col;
    in_global_map = Gif_NewFullColormap(256, 256);
    col = in_global_map->col;
    for (i = 0; i < 256; i++, col++)
      col->red = col->green = col->blue = i;
  }
  
  {
    int any_globals = 0;
    for (i = 0; i < gfs->nimages; i++) {
      Gif_Image *gfi = gfs->images[i];
      if (gfi->local)
	colormap_combine(all_colormap, gfi->local);
      else
	any_globals = 1;
    }
    if (any_globals)
      colormap_combine(all_colormap, in_global_map);
  }
  
  /* find screen_width and screen_height, and clip all images to screen */
  Gif_CalculateScreenSize(gfs, 0);
  screen_width = gfs->screen_width;
  screen_height = gfs->screen_height;
  for (i = 0; i < gfs->nimages; i++)
    Gif_ClipImage(gfs->images[i], 0, 0, screen_width, screen_height);
  
  /* create data arrays */
  screen_size = screen_width * screen_height;
  last_data = Gif_NewArray(u_int16_t, screen_size);
  this_data = Gif_NewArray(u_int16_t, screen_size);
  
  /* set up colormaps */
  gif_color_count = 2;
  while (gif_color_count < gfs->global->ncol && gif_color_count < 256)
    gif_color_count *= 2;
  
  /* choose background */
  if (gfs->images[0]->transparent < 0
      && gfs->background < in_global_map->ncol)
    background = in_global_map->col[gfs->background].pixel;
  else
    background = TRANSP;
  
#if OPTIMIZED_TRANSPARENCY
  /* set up hash arrays */
  if (optimize_level >= 2) {
    comp_prefix = Gif_NewArray(Gif_Code, COMP_HASH_SIZE);
    comp_suffix = Gif_NewArray(byte, COMP_HASH_SIZE);
    comp_code = Gif_NewArray(Gif_Code, COMP_HASH_SIZE);
    /* need to add 1 to end of array because we actually store data for
       code == GIF_MAX_CODE (but don't use it). */
    comp_code_length = Gif_NewArray(u_int16_t, GIF_MAX_CODE + 1);
    comp_hash = Gif_NewArray(u_int32_t, GIF_MAX_CODE + 1);
  }
#endif
  
  return 1;
}

static void
finalize_optimizer(Gif_Stream *gfs)
{
  Gif_DeleteColormap(in_global_map);
  Gif_DeleteColormap(all_colormap);
  
  Gif_DeleteArray(last_data);
  Gif_DeleteArray(this_data);

#if OPTIMIZED_TRANSPARENCY
  destroy_all_active_codes();
  Gif_DeleteArray(comp_prefix);
  Gif_DeleteArray(comp_suffix);
  Gif_DeleteArray(comp_code);
  Gif_DeleteArray(comp_code_length);
  Gif_DeleteArray(comp_hash);
#endif
}


/* the interface function! */

void
optimize_fragments(Gif_Stream *gfs, int optimize_level)
{
  if (!initialize_optimizer(gfs, optimize_level))
    return;

  create_subimages(gfs, optimize_level);
  create_out_global_map(gfs);
  create_new_image_data(gfs, optimize_level);
  
  finalize_optimizer(gfs);
}
