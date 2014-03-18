/* optimize.c - Functions to optimize animated GIFs.
   Copyright (C) 1997-2014 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include <assert.h>
#include <string.h>

typedef struct {
  int left;
  int top;
  int width;
  int height;
} Gif_OptBounds;

typedef struct {
  uint16_t left;
  uint16_t top;
  uint16_t width;
  uint16_t height;
  uint32_t size;
  uint8_t disposal;
  int transparent;
  uint8_t *needed_colors;
  uint16_t required_color_count;
  int32_t active_penalty;
  int32_t global_penalty;
  int32_t colormap_penalty;
  Gif_Image *new_gfi;
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
static uint16_t background;
#define NOT_IN_OUT_GLOBAL (256)
static uint16_t *last_data;
static uint16_t *this_data;
static uint16_t *next_data;
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


/* colormap_combine: Ensure that each color in 'src' is represented in 'dst'.
   For each color 'i' in 'src', src->col[i].pixel == some j so that
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


/* sort_permutation: sorts a given permutation 'perm' according to the
   corresponding values in 'values'. Thus, in the output, the sequence
   '[ values[perm[i]] | i <- 0..size-1 ]' will be monotonic, either up or
   (if is_down != 0) down. */

/* 9.Dec.1998 - Dumb idiot, it's time you stopped using C. The optimizer was
   broken because I switched to uint32_t's for the sorting values without
   considering the consequences; and the consequences were bad. */
static int32_t *permuting_sort_values;

static int
permuting_sorter_up(const void *v1, const void *v2)
{
  const uint16_t *n1 = (const uint16_t *)v1;
  const uint16_t *n2 = (const uint16_t *)v2;
  return (permuting_sort_values[*n1] - permuting_sort_values[*n2]);
}

static int
permuting_sorter_down(const void *v1, const void *v2)
{
  const uint16_t *n1 = (const uint16_t *)v1;
  const uint16_t *n2 = (const uint16_t *)v2;
  return (permuting_sort_values[*n2] - permuting_sort_values[*n1]);
}

static uint16_t *
sort_permutation(uint16_t *perm, int size, int32_t *values, int is_down)
{
  permuting_sort_values = values;
  if (is_down)
    qsort(perm, size, sizeof(uint16_t), permuting_sorter_down);
  else
    qsort(perm, size, sizeof(uint16_t), permuting_sorter_up);
  permuting_sort_values = 0;
  return perm;
}


/*****
 * MANIPULATING IMAGE AREAS
 **/

static Gif_OptBounds
safe_bounds(Gif_Image *area)
{
  /* Returns bounds constrained to lie within the screen. */
  Gif_OptBounds b;
  b.left = constrain(0, area->left, screen_width);
  b.top = constrain(0, area->top, screen_height);
  b.width = constrain(0, area->left + area->width, screen_width) - b.left;
  b.height = constrain(0, area->top + area->height, screen_height) - b.top;
  return b;
}

static void
copy_data_area(uint16_t *dst, uint16_t *src, Gif_Image *area)
{
  Gif_OptBounds ob;
  int y;
  if (!area)
    return;
  ob = safe_bounds(area);
  dst += ob.top * (unsigned) screen_width + ob.left;
  src += ob.top * (unsigned) screen_width + ob.left;
  for (y = 0; y < ob.height; y++) {
    memcpy(dst, src, sizeof(uint16_t) * ob.width);
    dst += screen_width;
    src += screen_width;
  }
}

static void
copy_data_area_subimage(uint16_t *dst, uint16_t *src, Gif_OptData *area)
{
  Gif_Image img;
  img.left = area->left;
  img.top = area->top;
  img.width = area->width;
  img.height = area->height;
  copy_data_area(dst, src, &img);
}

static void
fill_data_area(uint16_t *dst, uint16_t value, Gif_Image *area)
{
  int x, y;
  Gif_OptBounds ob = safe_bounds(area);
  dst += ob.top * (unsigned) screen_width + ob.left;
  for (y = 0; y < ob.height; y++) {
    for (x = 0; x < ob.width; x++)
      dst[x] = value;
    dst += screen_width;
  }
}

static void
fill_data_area_subimage(uint16_t *dst, uint16_t value, Gif_OptData *area)
{
  Gif_Image img;
  img.left = area->left;
  img.top = area->top;
  img.width = area->width;
  img.height = area->height;
  fill_data_area(dst, value, &img);
}

static void
erase_screen(uint16_t *dst)
{
  uint32_t i;
  uint32_t screen_size = (unsigned) screen_width * (unsigned) screen_height;
  for (i = 0; i < screen_size; i++)
    *dst++ = background;
}

/*****
 * APPLY A GIF FRAME OR DISPOSAL TO AN IMAGE DESTINATION
 **/

static void
apply_frame(uint16_t *dst, Gif_Stream* gfs, Gif_Image* gfi,
            int replace, int save_uncompressed)
{
  int i, y, was_compressed = 0;
  uint16_t map[256];
  Gif_Colormap *colormap = gfi->local ? gfi->local : in_global_map;
  Gif_OptBounds ob = safe_bounds(gfi);

  if (!gfi->img) {
    was_compressed = 1;
    Gif_UncompressImage(gfs, gfi);
  }

  /* make sure transparency maps to TRANSP */
  for (i = 0; i < colormap->ncol; i++)
    map[i] = colormap->col[i].pixel;
  /* out-of-bounds colors map to 0, for the sake of argument */
  for (i = colormap->ncol; i < 256; i++)
    map[i] = colormap->col[0].pixel;
  if (gfi->transparent >= 0 && gfi->transparent < 256)
    map[gfi->transparent] = TRANSP;
  else
    replace = 1;

  /* map the image */
  dst += ob.left + ob.top * (unsigned) screen_width;
  for (y = 0; y < ob.height; y++) {
    uint8_t *gfi_pointer = gfi->img[y];
    int x;

    if (replace)
      for (x = 0; x < ob.width; x++)
	dst[x] = map[gfi_pointer[x]];
    else
      for (x = 0; x < ob.width; x++) {
	uint16_t new_pixel = map[gfi_pointer[x]];
	if (new_pixel != TRANSP)
	    dst[x] = new_pixel;
      }

    dst += screen_width;
  }

  if (was_compressed && !save_uncompressed)
    Gif_ReleaseUncompressedImage(gfi);
}

static void
apply_frame_disposal(uint16_t *into_data, uint16_t *from_data,
		     uint16_t *previous_data, Gif_Image *gfi)
{
  unsigned screen_size = (unsigned) screen_width * (unsigned) screen_height;
  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS)
    memcpy(into_data, previous_data, sizeof(uint16_t) * screen_size);
  else {
    memcpy(into_data, from_data, sizeof(uint16_t) * screen_size);
    if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
      fill_data_area(into_data, background, gfi);
  }
}


/*****
 * FIND THE SMALLEST BOUNDING RECTANGLE ENCLOSING ALL CHANGES
 **/

/* find_difference_bounds: Find the smallest rectangular area containing all
   the changes and store it in 'bounds'. */

static void
find_difference_bounds(Gif_OptData *bounds, Gif_Image *gfi, Gif_Image *last)
{
  int lf, rt, lf_min, rt_max, tp, bt, x, y;
  Gif_OptBounds ob;

  /* 1.Aug.99 - use current bounds if possible, since this function is a speed
     bottleneck */
  if (!last || last->disposal == GIF_DISPOSAL_NONE
      || last->disposal == GIF_DISPOSAL_ASIS) {
    ob = safe_bounds(gfi);
    lf_min = ob.left;
    rt_max = ob.left + ob.width - 1;
    tp = ob.top;
    bt = ob.top + ob.height - 1;
  } else {
    lf_min = 0;
    rt_max = screen_width - 1;
    tp = 0;
    bt = screen_height - 1;
  }

  for (; tp < screen_height; tp++)
    if (memcmp(last_data + (unsigned) screen_width * tp,
               this_data + (unsigned) screen_width * tp,
	       screen_width * sizeof(uint16_t)) != 0)
      break;
  for (; bt >= tp; bt--)
    if (memcmp(last_data + (unsigned) screen_width * bt,
               this_data + (unsigned) screen_width * bt,
	       screen_width * sizeof(uint16_t)) != 0)
      break;

  lf = screen_width;
  rt = 0;
  for (y = tp; y <= bt; y++) {
    uint16_t *ld = last_data + (unsigned) screen_width * y;
    uint16_t *td = this_data + (unsigned) screen_width * y;
    for (x = lf_min; x < lf; x++)
      if (ld[x] != td[x])
	break;
    lf = x;

    for (x = rt_max; x > rt; x--)
      if (ld[x] != td[x])
	break;
    rt = x;
  }

  /* 19.Aug.1999 - handle case when there's no difference between frames */
  if (tp > bt) {
    tp = bt = gfi->top;
    lf = rt = gfi->left;
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
   next_data. The new bounds are passed and stored in 'bounds'; the image's
   old bounds, which are also the maximum bounds, are passed in
   'this_bounds'. */

static int
expand_difference_bounds(Gif_OptData *bounds, Gif_Image *this_bounds)
{
  int x, y, expanded = 0;
  Gif_OptBounds ob = safe_bounds(this_bounds);

  if (bounds->width <= 0 || bounds->height <= 0) {
      bounds->left = bounds->top = 0;
      bounds->width = screen_width;
      bounds->height = screen_height;
  }

  /* 20.Nov.2013 - The image `bounds` might be larger than `this_bounds`
     because of a previous frame's background disposal. Don't accidentally
     shrink `this_bounds`. */
  if (ob.left > bounds->left) {
      ob.width = (ob.left + ob.width) - bounds->left;
      ob.left = bounds->left;
  }
  if (ob.top > bounds->top) {
      ob.height = (ob.top + ob.height) - bounds->top;
      ob.top = bounds->top;
  }
  if (ob.left + ob.width < bounds->left + bounds->width)
      ob.width = bounds->left + bounds->width - ob.left;
  if (ob.top + ob.height < bounds->top + bounds->height)
      ob.height = bounds->top + bounds->height - ob.top;

  for (; ob.top < bounds->top; ++ob.top, --ob.height) {
    uint16_t *now = this_data + (unsigned) screen_width * ob.top;
    uint16_t *next = next_data + (unsigned) screen_width * ob.top;
    for (x = ob.left; x < ob.left + ob.width; ++x)
      if (now[x] != TRANSP && next[x] == TRANSP) {
	expanded = 1;
	goto found_top;
      }
  }

 found_top:
  for (; ob.top + ob.height > bounds->top + bounds->height; --ob.height) {
    uint16_t *now = this_data + (unsigned) screen_width * (ob.top + ob.height - 1);
    uint16_t *next = next_data + (unsigned) screen_width * (ob.top + ob.height - 1);
    for (x = ob.left; x < ob.left + ob.width; ++x)
      if (now[x] != TRANSP && next[x] == TRANSP) {
	expanded = 1;
	goto found_bottom;
      }
  }

 found_bottom:
  for (; ob.left < bounds->left; ++ob.left, --ob.width) {
    uint16_t *now = this_data + ob.left;
    uint16_t *next = next_data + ob.left;
    for (y = ob.top; y < ob.top + ob.height; ++y)
      if (now[y * (unsigned) screen_width] != TRANSP
          && next[y * (unsigned) screen_width] == TRANSP) {
	expanded = 1;
	goto found_left;
      }
  }

 found_left:
  for (; ob.left + ob.width > bounds->left + bounds->width; --ob.width) {
    uint16_t *now = this_data + ob.left + ob.width - 1;
    uint16_t *next = next_data + ob.left + ob.width - 1;
    for (y = ob.top; y < ob.top + ob.height; ++y)
      if (now[y * (unsigned) screen_width] != TRANSP
          && next[y * (unsigned) screen_width] == TRANSP) {
	expanded = 1;
	goto found_right;
      }
  }

 found_right:
  if (!expanded)
    for (y = ob.top; y < ob.top + ob.height; ++y) {
      uint16_t *now = this_data + y * (unsigned) screen_width;
      uint16_t *next = next_data + y * (unsigned) screen_width;
      for (x = ob.left; x < ob.left + ob.width; ++x)
	if (now[x] != TRANSP && next[x] == TRANSP) {
	  expanded = 1;
	  break;
	}
    }

  bounds->left = ob.left;
  bounds->top = ob.top;
  bounds->width = ob.width;
  bounds->height = ob.height;
  return expanded;
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
	 && bounds->top + bounds->height <= screen_height
	 && bounds->left + bounds->width <= screen_width);
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
  uint8_t *need = Gif_NewArray(uint8_t, all_ncol);

  for (i = 0; i < all_ncol; i++)
    need[i] = 0;

  /* set elements that are in the image. need == 2 means the color
     must be in the map; need == 1 means the color may be replaced by
     transparency. */
  for (y = top; y < top + height; y++) {
    uint16_t *data = this_data + (unsigned) screen_width * y + bounds->left;
    uint16_t *last = last_data + (unsigned) screen_width * y + bounds->left;
    for (x = 0; x < width; x++) {
      if (data[x] != last[x])
	need[data[x]] = REQUIRED;
      else if (need[data[x]] == 0)
	need[data[x]] = REPLACE_TRANSP;
    }
  }
  if (need[TRANSP])
    need[TRANSP] = REQUIRED;

  /* check for too many colors; also force transparency if needed */
  {
    int count[3];
    /* Count distinct pixels in each category */
    count[0] = count[1] = count[2] = 0;
    for (i = 0; i < all_ncol; i++)
      count[need[i]]++;
    /* If use_transparency is large and there's room, add transparency */
    if (use_transparency > 1 && !need[TRANSP] && count[REQUIRED] < 256) {
      need[TRANSP] = REQUIRED;
      count[REQUIRED]++;
    }
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
	if (need[i] == REPLACE_TRANSP)
	  need[i] = REQUIRED;
      count[REQUIRED] += count[REPLACE_TRANSP];
    }
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
create_subimages(Gif_Stream *gfs, int optimize_flags, int save_uncompressed)
{
  unsigned screen_size;
  Gif_Image *last_gfi;
  int next_data_valid;
  uint16_t *previous_data = 0;
  int local_color_tables = 0;

  screen_size = (unsigned) screen_width * (unsigned) screen_height;

  next_data = Gif_NewArray(uint16_t, screen_size);
  next_data_valid = 0;

  /* do first image. Remember to uncompress it if necessary */
  erase_screen(last_data);
  erase_screen(this_data);
  last_gfi = 0;

  /* PRECONDITION:
     previous_data -- garbage
     last_data -- optimized image after disposal of previous optimized frame
     this_data -- input image after disposal of previous input frame
     next_data -- input image after application of current input frame,
                  if next_image_valid */
  for (image_index = 0; image_index < gfs->nimages; image_index++) {
    Gif_Image *gfi = gfs->images[image_index];
    Gif_OptData *subimage = new_opt_data();
    if (gfi->local)
        local_color_tables = 1;

    /* save previous data if necessary */
    if (gfi->disposal == GIF_DISPOSAL_PREVIOUS
        || (local_color_tables && image_index > 0
            && last_gfi->disposal > GIF_DISPOSAL_ASIS)) {
      if (!previous_data)
	previous_data = Gif_NewArray(uint16_t, screen_size);
      memcpy(previous_data, this_data, sizeof(uint16_t) * screen_size);
    }

    /* set this_data equal to the current image */
    if (next_data_valid) {
      uint16_t *temp = this_data;
      this_data = next_data;
      next_data = temp;
      next_data_valid = 0;
    } else
        apply_frame(this_data, gfs, gfi, 0, save_uncompressed);

 retry_frame:
    /* find minimum area of difference between this image and last image */
    subimage->disposal = GIF_DISPOSAL_ASIS;
    if (image_index > 0)
      find_difference_bounds(subimage, gfi, last_gfi);
    else {
      Gif_OptBounds ob = safe_bounds(gfi);
      subimage->left = ob.left;
      subimage->top = ob.top;
      subimage->width = ob.width;
      subimage->height = ob.height;
    }

    /* might need to expand difference border if transparent background &
       background disposal */
    if ((gfi->disposal == GIF_DISPOSAL_BACKGROUND
	 || gfi->disposal == GIF_DISPOSAL_PREVIOUS)
	&& background == TRANSP
	&& image_index < gfs->nimages - 1) {
      /* set up next_data */
      Gif_Image *next_gfi = gfs->images[image_index + 1];
      apply_frame_disposal(next_data, this_data, previous_data, gfi);
      apply_frame(next_data, gfs, next_gfi, 0, save_uncompressed);
      next_data_valid = 1;
      /* expand border as necessary */
      if (expand_difference_bounds(subimage, gfi))
	subimage->disposal = GIF_DISPOSAL_BACKGROUND;
    }

    fix_difference_bounds(subimage);

    /* set map of used colors */
    {
      int use_transparency = (optimize_flags & GT_OPT_MASK) > 1
          && image_index > 0;
      if (image_index == 0 && background == TRANSP)
	use_transparency = 2;
      get_used_colors(subimage, use_transparency);
      /* Gifsicle's optimization strategy normally creates frames with ASIS
         or BACKGROUND disposal (not PREVIOUS disposal). However, there are
         cases when PREVIOUS disposal is strictly required, or a frame would
         require more than 256 colors. Detect this case and try to recover. */
      if (subimage->required_color_count > 256) {
          if (image_index > 0 && local_color_tables) {
              Gif_OptData *subimage = (Gif_OptData*) last_gfi->user_data;
              if ((last_gfi->disposal == GIF_DISPOSAL_PREVIOUS
                   || last_gfi->disposal == GIF_DISPOSAL_BACKGROUND)
                  && subimage->disposal != last_gfi->disposal) {
                  subimage->disposal = last_gfi->disposal;
                  memcpy(last_data, previous_data, sizeof(uint16_t) * screen_size);
                  goto retry_frame;
              }
          }
          fatal_error("%d colors required in a frame (256 is max)",
                      subimage->required_color_count);
      }
    }

    gfi->user_data = subimage;
    last_gfi = gfi;

    /* Apply optimized disposal to last_data and unoptimized disposal to
       this_data. Before 9.Dec.1998 I applied unoptimized disposal uniformly
       to both. This led to subtle bugs. After all, to determine bounds, we
       want to compare the current image (only obtainable through unoptimized
       disposal) with what WILL be left after the previous OPTIMIZED image's
       disposal. This fix is repeated in create_new_image_data */
    if (subimage->disposal == GIF_DISPOSAL_BACKGROUND)
      fill_data_area_subimage(last_data, background, subimage);
    else
      copy_data_area_subimage(last_data, this_data, subimage);

    if (last_gfi->disposal == GIF_DISPOSAL_BACKGROUND)
      fill_data_area(this_data, background, last_gfi);
    else if (last_gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
      uint16_t *temp = previous_data;
      previous_data = this_data;
      this_data = temp;
    }
  }

  Gif_DeleteArray(next_data);
  if (previous_data)
    Gif_DeleteArray(previous_data);
}


/*****
 * CALCULATE OUTPUT GLOBAL COLORMAP
 **/

/* create_out_global_map: The interface function to this pass. It creates
   out_global_map and sets pixel values on all_colormap appropriately.
   Specifically:

   all_colormap->col[P].pixel >= 256 ==> P is not in the global colormap.

   Otherwise, all_colormap->col[P].pixel == the J so that
   GIF_COLOREQ(&all_colormap->col[P], &out_global_map->col[J]).

   On return, the 'colormap_penalty' component of an image's Gif_OptData
   structure is <0 iff that image will need a local colormap.

   20.Aug.1999 - updated to new version that arranges the entire colormap, not
   just the stuff above 256 colors. */

static void
increment_penalties(Gif_OptData *opt, int32_t *penalty, int32_t delta)
{
  int i;
  int all_ncol = all_colormap->ncol;
  uint8_t *need = opt->needed_colors;
  for (i = 1; i < all_ncol; i++)
    if (need[i] == REQUIRED)
      penalty[i] += delta;
}

static void
create_out_global_map(Gif_Stream *gfs)
{
  int all_ncol = all_colormap->ncol;
  int32_t *penalty = Gif_NewArray(int32_t, all_ncol);
  uint16_t *permute = Gif_NewArray(uint16_t, all_ncol);
  uint16_t *ordering = Gif_NewArray(uint16_t, all_ncol);
  int cur_ncol, i, imagei;
  int nglobal_all = (all_ncol <= 257 ? all_ncol - 1 : 256);
  int permutation_changed;

  /* initial permutation is null */
  for (i = 0; i < all_ncol - 1; i++)
    permute[i] = i + 1;

  /* choose appropriate penalties for each image */
  for (imagei = 0; imagei < gfs->nimages; imagei++) {
    Gif_OptData *opt = (Gif_OptData *)gfs->images[imagei]->user_data;
    opt->global_penalty = opt->colormap_penalty = 1;
    for (i = 2; i < opt->required_color_count; i *= 2)
      opt->colormap_penalty *= 3;
    opt->active_penalty =
      (all_ncol > 257 ? opt->colormap_penalty : opt->global_penalty);
  }

  /* set initial penalties for each color */
  for (i = 1; i < all_ncol; i++)
    penalty[i] = 0;
  for (imagei = 0; imagei < gfs->nimages; imagei++) {
    Gif_OptData *opt = (Gif_OptData *)gfs->images[imagei]->user_data;
    increment_penalties(opt, penalty, opt->active_penalty);
  }
  permutation_changed = 1;

  /* Loop, removing one color at a time. */
  for (cur_ncol = all_ncol - 1; cur_ncol; cur_ncol--) {
    uint16_t removed;

    /* sort permutation based on penalty */
    if (permutation_changed)
      sort_permutation(permute, cur_ncol, penalty, 1);
    permutation_changed = 0;

    /* update reverse permutation */
    removed = permute[cur_ncol - 1];
    ordering[removed] = cur_ncol - 1;

    /* decrement penalties for colors that are out of the running */
    for (imagei = 0; imagei < gfs->nimages; imagei++) {
      Gif_OptData *opt = (Gif_OptData *)gfs->images[imagei]->user_data;
      uint8_t *need = opt->needed_colors;
      if (opt->global_penalty > 0 && need[removed] == REQUIRED) {
	increment_penalties(opt, penalty, -opt->active_penalty);
	opt->global_penalty = 0;
	opt->colormap_penalty = (cur_ncol > 256 ? -1 : 0);
	permutation_changed = 1;
      }
    }

    /* change colormap penalties if we're no longer working w/globalmap */
    if (cur_ncol == 257) {
      for (i = 0; i < all_ncol; i++)
	penalty[i] = 0;
      for (imagei = 0; imagei < gfs->nimages; imagei++) {
	Gif_OptData *opt = (Gif_OptData *)gfs->images[imagei]->user_data;
	opt->active_penalty = opt->global_penalty;
	increment_penalties(opt, penalty, opt->global_penalty);
      }
      permutation_changed = 1;
    }
  }

  /* make sure background is in the global colormap */
  if (background != TRANSP && ordering[background] >= 256) {
    uint16_t other = permute[255];
    ordering[other] = ordering[background];
    ordering[background] = 255;
  }

  /* assign out_global_map based on permutation */
  out_global_map = Gif_NewFullColormap(nglobal_all, 256);

  for (i = 1; i < all_ncol; i++)
    if (ordering[i] < 256) {
      out_global_map->col[ordering[i]] = all_colormap->col[i];
      all_colormap->col[i].pixel = ordering[i];
    } else
      all_colormap->col[i].pixel = NOT_IN_OUT_GLOBAL;

  /* set the stream's background color */
  if (background != TRANSP)
    gfs->background = ordering[background];

  /* cleanup */
  Gif_DeleteArray(penalty);
  Gif_DeleteArray(permute);
  Gif_DeleteArray(ordering);
}


/*****
 * CREATE COLOR MAPPING FOR A PARTICULAR IMAGE
 **/

/* sort_colormap_permutation_rgb: for canonicalizing local colormaps by
   arranging them in RGB order */

static int
colormap_rgb_permutation_sorter(const void *v1, const void *v2)
{
  const Gif_Color *col1 = (const Gif_Color *)v1;
  const Gif_Color *col2 = (const Gif_Color *)v2;
  int value1 = (col1->gfc_red << 16) | (col1->gfc_green << 8) | col1->gfc_blue;
  int value2 = (col2->gfc_red << 16) | (col2->gfc_green << 8) | col2->gfc_blue;
  return value1 - value2;
}


/* prepare_colormap_map: Create and return an array of bytes mapping from
   global pixel values to pixel values for this image. It may add colormap
   cells to 'into'; if there isn't enough room in 'into', it will return 0. It
   sets the 'transparent' field of 'gfi->optdata', but otherwise doesn't
   change or read it at all. */

static uint8_t *
prepare_colormap_map(Gif_Image *gfi, Gif_Colormap *into, uint8_t *need)
{
  int i;
  int is_global = (into == out_global_map);

  int all_ncol = all_colormap->ncol;
  Gif_Color *all_col = all_colormap->col;

  int ncol = into->ncol;
  Gif_Color *col = into->col;

  uint8_t *map = Gif_NewArray(uint8_t, all_ncol);
  uint8_t into_used[256];

  /* keep track of which pixel indices in 'into' have been used; initially,
     all unused */
  for (i = 0; i < 256; i++)
    into_used[i] = 0;

  /* go over all non-transparent global pixels which MUST appear
     (need[P]==REQUIRED) and place them in 'into' */
  for (i = 1; i < all_ncol; i++) {
    int val;
    if (need[i] != REQUIRED)
      continue;

    /* fail if a needed pixel isn't in the global map */
    if (is_global) {
      val = all_col[i].pixel;
      if (val >= ncol)
	goto error;
    } else {
      /* always place colors in a local colormap */
      if (ncol == 256)
	goto error;
      val = ncol;
      col[val] = all_col[i];
      col[val].pixel = i;
      ncol++;
    }

    map[i] = val;
    into_used[val] = 1;
  }

  if (!is_global) {
    qsort(col, ncol, sizeof(Gif_Color), colormap_rgb_permutation_sorter);
    for (i = 0; i < ncol; ++i)
      map[col[i].pixel] = i;
  }

  /* now check for transparency */
  gfi->transparent = -1;
  if (need[TRANSP]) {
    int transparent = -1;

    /* first, look for an unused index in 'into'. Pick the lowest one: the
       lower transparent index we get, the more likely we can shave a bit off
       min_code_bits later, thus saving space */
    for (i = 0; i < ncol; i++)
      if (!into_used[i]) {
	transparent = i;
	break;
      }

    /* otherwise, [1.Aug.1999] use a fake slot for the purely transparent
       color. Don't actually enter the transparent color into the colormap --
       we might be able to output a smaller colormap! If there's no room for
       it, give up */
    if (transparent < 0) {
      if (ncol < 256) {
	transparent = ncol;
	/* 1.Aug.1999 - don't increase ncol */
	col[ncol] = all_col[TRANSP];
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
     cells in 'into') and return the map. */
  into->ncol = ncol;
  return map;

 error:
  /* If we get here, it failed! Return 0 and don't change global state. */
  Gif_DeleteArray(map);
  return 0;
}


/* prepare_colormap: make a colormap up from the image data by fitting any
   used colors into a colormap. Returns a map from global color index to index
   in this image's colormap. May set a local colormap on 'gfi'. */

static uint8_t *
prepare_colormap(Gif_Image *gfi, uint8_t *need)
{
  uint8_t *map;

  /* try to map pixel values into the global colormap */
  Gif_DeleteColormap(gfi->local);
  gfi->local = 0;
  map = prepare_colormap_map(gfi, out_global_map, need);

  if (!map) {
    /* that didn't work; add a local colormap. */
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
simple_frame_data(Gif_Image *gfi, uint8_t *map)
{
  Gif_OptBounds ob = safe_bounds(gfi);
  int x, y;
  unsigned scan_width = gfi->width;

  for (y = 0; y < ob.height; y++) {
    uint16_t *from = this_data + (unsigned) screen_width * (y + ob.top) + ob.left;
    uint8_t *into = gfi->image_data + y * scan_width;
    for (x = 0; x < ob.width; x++)
      *into++ = map[*from++];
  }
}


/* transp_frame_data: copy the frame data into the actual image, using
   transparency occasionally according to a heuristic described below */

static void
transp_frame_data(Gif_Stream *gfs, Gif_Image *gfi, uint8_t *map,
		  int optimize_flags, Gif_CompressInfo *gcinfo)
{
  Gif_OptBounds ob = safe_bounds(gfi);
  int x, y, transparent = gfi->transparent;
  uint16_t *last = 0;
  uint16_t *cur = 0;
  uint8_t *data, *begin_same;
  uint8_t *t2_data = 0, *last_for_t2;
  int nsame;

  /* First, try w/o transparency. Compare this to the result using
     transparency and pick the better of the two. */
  simple_frame_data(gfi, map);
  Gif_FullCompressImage(gfs, gfi, gcinfo);
  gcinfo->flags |= GIF_WRITE_SHRINK;

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

     However, it DOES do better than the complicated, greedy algorithm that
     preceded it; and now we pick either the transparency-optimized version or
     the normal version, whichever compresses smaller, for the best of both
     worlds. (9/98)

     On several images, making SINGLE color runs transparent wins over the
     previous heuristic, so try both at optimize level 3 or above (the cost is
     ~30%). (2/11) */

    data = begin_same = last_for_t2 = gfi->image_data;
    nsame = 0;

    for (y = 0; y < ob.height; ++y) {
	last = last_data + (unsigned) screen_width * (y + ob.top) + ob.left;
	cur = this_data + (unsigned) screen_width * (y + ob.top) + ob.left;
	for (x = 0; x < ob.width; ++x) {
	    if (*cur != *last && map[*cur] != transparent) {
		if (nsame == 1 && data[-1] != transparent
		    && (optimize_flags & GT_OPT_MASK) > 2) {
		    if (!t2_data)
			t2_data = Gif_NewArray(uint8_t, (size_t) ob.width * (size_t) ob.height);
		    memcpy(t2_data + (last_for_t2 - gfi->image_data),
			   last_for_t2, begin_same - last_for_t2);
		    memset(t2_data + (begin_same - gfi->image_data),
			   transparent, data - begin_same);
		    last_for_t2 = data;
		}
		nsame = 0;
	    } else if (nsame == 0) {
		begin_same = data;
		++nsame;
	    } else if (nsame == 1 && map[*cur] != data[-1]) {
		memset(begin_same, transparent, data - begin_same);
		++nsame;
	    }
	    if (nsame > 1)
		*data = transparent;
	    else
		*data = map[*cur];
	    ++data, ++cur, ++last;
	}
    }

    if (t2_data)
	memcpy(t2_data + (last_for_t2 - gfi->image_data),
	       last_for_t2, data - last_for_t2);


    /* Now, try compressed transparent version(s) and pick the better of the
       two (or three). */
    Gif_FullCompressImage(gfs, gfi, gcinfo);
    if (t2_data) {
	Gif_SetUncompressedImage(gfi, t2_data, Gif_Free, 0);
        Gif_FullCompressImage(gfs, gfi, gcinfo);
    }
    Gif_ReleaseUncompressedImage(gfi);

    gcinfo->flags &= ~GIF_WRITE_SHRINK;
}


/*****
 * CREATE NEW IMAGE DATA
 **/

/* last == what last image ended up looking like
   this == what new image should look like

   last = apply O1 + dispose O1 + ... + apply On-1 + dispose On-1
   this = apply U1 + dispose U1 + ... + apply Un-1 + dispose Un-1 + apply Un

   invariant: apply O1 + dispose O1 + ... + apply Ok
   === apply U1 + dispose U1 + ... + apply Uk */

static void
create_new_image_data(Gif_Stream *gfs, int optimize_flags)
{
  Gif_Image cur_unopt_gfi;	/* placehoder; maintains pre-optimization
				   image size so we can apply background
				   disposal */
  unsigned screen_size = (unsigned) screen_width * (unsigned) screen_height;
  uint16_t *previous_data = 0;
  Gif_CompressInfo gcinfo = gif_write_info;
  if ((optimize_flags & GT_OPT_MASK) >= 3)
      gcinfo.flags |= GIF_WRITE_OPTIMIZE;

  gfs->global = out_global_map;

  /* do first image. Remember to uncompress it if necessary */
  erase_screen(last_data);
  erase_screen(this_data);

  for (image_index = 0; image_index < gfs->nimages; image_index++) {
    Gif_Image *cur_gfi = gfs->images[image_index];
    Gif_OptData *opt = (Gif_OptData *)cur_gfi->user_data;
    int was_compressed = (cur_gfi->img == 0);

    /* save previous data if necessary */
    if (cur_gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
      if (!previous_data)
          previous_data = Gif_NewArray(uint16_t, screen_size);
      copy_data_area(previous_data, this_data, cur_gfi);
    }

    /* set up this_data to be equal to the current image */
    apply_frame(this_data, gfs, cur_gfi, 0, 0);

    /* save actual bounds and disposal from unoptimized version so we can
       apply the disposal correctly next time through */
    cur_unopt_gfi = *cur_gfi;

    /* set bounds and disposal from optdata */
    Gif_ReleaseUncompressedImage(cur_gfi);
    cur_gfi->left = opt->left;
    cur_gfi->top = opt->top;
    cur_gfi->width = opt->width;
    cur_gfi->height = opt->height;
    cur_gfi->disposal = opt->disposal;
    if (image_index > 0)
	cur_gfi->interlace = 0;

    /* find the new image's colormap and then make new data */
    {
      uint8_t *map = prepare_colormap(cur_gfi, opt->needed_colors);
      uint8_t *data = Gif_NewArray(uint8_t, (size_t) cur_gfi->width * (size_t) cur_gfi->height);
      Gif_SetUncompressedImage(cur_gfi, data, Gif_Free, 0);

      /* don't use transparency on first frame */
      if ((optimize_flags & GT_OPT_MASK) > 1 && image_index > 0
	  && cur_gfi->transparent >= 0)
	transp_frame_data(gfs, cur_gfi, map, optimize_flags, &gcinfo);
      else
	simple_frame_data(cur_gfi, map);

      if (cur_gfi->img) {
	if (was_compressed || (optimize_flags & GT_OPT_MASK) > 1) {
	  Gif_FullCompressImage(gfs, cur_gfi, &gcinfo);
	  Gif_ReleaseUncompressedImage(cur_gfi);
	} else			/* bug fix 22.May.2001 */
	  Gif_ReleaseCompressedImage(cur_gfi);
      }

      Gif_DeleteArray(map);
    }

    delete_opt_data(opt);
    cur_gfi->user_data = 0;

    /* Set up last_data and this_data. last_data must contain this_data + new
       disposal. this_data must contain this_data + old disposal. */
    if (cur_gfi->disposal == GIF_DISPOSAL_NONE
	|| cur_gfi->disposal == GIF_DISPOSAL_ASIS)
      copy_data_area(last_data, this_data, cur_gfi);
    else if (cur_gfi->disposal == GIF_DISPOSAL_BACKGROUND)
      fill_data_area(last_data, background, cur_gfi);
    else if (cur_gfi->disposal != GIF_DISPOSAL_PREVIOUS)
      assert(0 && "optimized frame has strange disposal");

    if (cur_unopt_gfi.disposal == GIF_DISPOSAL_BACKGROUND)
      fill_data_area(this_data, background, &cur_unopt_gfi);
    else if (cur_unopt_gfi.disposal == GIF_DISPOSAL_PREVIOUS)
      copy_data_area(this_data, previous_data, &cur_unopt_gfi);
  }

  if (previous_data)
      Gif_DeleteArray(previous_data);
}


/*****
 * INITIALIZATION AND FINALIZATION
 **/

static int
initialize_optimizer(Gif_Stream *gfs)
{
  int i;
  unsigned screen_size;

  if (gfs->nimages < 1)
    return 0;

  /* combine colormaps */
  all_colormap = Gif_NewFullColormap(1, 384);
  all_colormap->col[0].gfc_red = 255;
  all_colormap->col[0].gfc_green = 255;
  all_colormap->col[0].gfc_blue = 255;

  in_global_map = gfs->global;
  if (!in_global_map) {
    Gif_Color *col;
    in_global_map = Gif_NewFullColormap(256, 256);
    col = in_global_map->col;
    for (i = 0; i < 256; i++, col++)
      col->gfc_red = col->gfc_green = col->gfc_blue = i;
  }

  {
    int any_globals = 0;
    int first_transparent = -1;
    for (i = 0; i < gfs->nimages; i++) {
      Gif_Image *gfi = gfs->images[i];
      if (gfi->local)
	colormap_combine(all_colormap, gfi->local);
      else
	any_globals = 1;
      if (gfi->transparent >= 0 && first_transparent < 0)
	first_transparent = i;
    }
    if (any_globals)
      colormap_combine(all_colormap, in_global_map);

    /* try and maintain transparency's pixel value */
    if (first_transparent >= 0) {
      Gif_Image *gfi = gfs->images[first_transparent];
      Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
      all_colormap->col[TRANSP] = gfcm->col[gfi->transparent];
    }
  }

  /* find screen_width and screen_height, and clip all images to screen */
  Gif_CalculateScreenSize(gfs, 0);
  screen_width = gfs->screen_width;
  screen_height = gfs->screen_height;
  for (i = 0; i < gfs->nimages; i++)
    Gif_ClipImage(gfs->images[i], 0, 0, screen_width, screen_height);

  /* create data arrays */
  screen_size = (unsigned) screen_width * (unsigned) screen_height;
  last_data = Gif_NewArray(uint16_t, screen_size);
  this_data = Gif_NewArray(uint16_t, screen_size);

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

  return 1;
}

static void
finalize_optimizer(Gif_Stream *gfs, int optimize_flags)
{
  int i;

  if (background == TRANSP)
    gfs->background = (uint8_t)gfs->images[0]->transparent;

  /* 11.Mar.2010 - remove entirely transparent frames. */
  for (i = 1; i < gfs->nimages && !(optimize_flags & GT_OPT_KEEPEMPTY); ++i) {
    Gif_Image *gfi = gfs->images[i];
    if (gfi->width == 1 && gfi->height == 1 && gfi->transparent >= 0
	&& !gfi->identifier && !gfi->comment
	&& (gfi->disposal == GIF_DISPOSAL_ASIS
	    || gfi->disposal == GIF_DISPOSAL_NONE
	    || gfi->disposal == GIF_DISPOSAL_PREVIOUS)
	&& gfi->delay && gfs->images[i-1]->delay) {
      Gif_UncompressImage(gfs, gfi);
      if (gfi->img[0][0] == gfi->transparent
	  && (gfs->images[i-1]->disposal == GIF_DISPOSAL_ASIS
	      || gfs->images[i-1]->disposal == GIF_DISPOSAL_NONE)) {
	gfs->images[i-1]->delay += gfi->delay;
	Gif_DeleteImage(gfi);
	memmove(&gfs->images[i], &gfs->images[i+1], sizeof(Gif_Image *) * (gfs->nimages - i - 1));
	--gfs->nimages;
	--i;
      }
    }
  }

  /* 10.Dec.1998 - prefer GIF_DISPOSAL_NONE to GIF_DISPOSAL_ASIS. This is
     semantically "wrong" -- it's better to set the disposal explicitly than
     rely on default behavior -- but will result in smaller GIF files, since
     the graphic control extension can be left off in many cases. */
  for (i = 0; i < gfs->nimages; i++)
    if (gfs->images[i]->disposal == GIF_DISPOSAL_ASIS
	&& gfs->images[i]->delay == 0
	&& gfs->images[i]->transparent < 0)
      gfs->images[i]->disposal = GIF_DISPOSAL_NONE;

  Gif_DeleteColormap(in_global_map);
  Gif_DeleteColormap(all_colormap);

  Gif_DeleteArray(last_data);
  Gif_DeleteArray(this_data);
}


/* the interface function! */

void
optimize_fragments(Gif_Stream *gfs, int optimize_flags, int huge_stream)
{
  if (!initialize_optimizer(gfs))
    return;

  create_subimages(gfs, optimize_flags, !huge_stream);
  create_out_global_map(gfs);
  create_new_image_data(gfs, optimize_flags);

  finalize_optimizer(gfs, optimize_flags);
}
