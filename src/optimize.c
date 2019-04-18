/* optimize.c - Functions to optimize animated GIFs.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include "kcolor.h"
#include <assert.h>
#include <string.h>

typedef int32_t penalty_type;

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
  unsigned required_color_count;
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
/* Histogram so we can find colors quickly */
static kchist all_colormap_hist;

/* The old global colormap, or a fake one we created if necessary */
static Gif_Colormap *in_global_map;

/* The new global colormap */
static Gif_Colormap *out_global_map;

#define TRANSP (0)
#define NOT_IN_OUT_GLOBAL (256)
static unsigned background;
static int image_index;

static penalty_type *permuting_sort_values;

#define REQUIRED        2
#define REPLACE_TRANSP  1


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


/* all_colormap_add: Ensure that each color in 'src' is represented in
   'all_colormap'. For each color 'i' in 'src', src->col[i].pixel == some j
   so that GIF_COLOREQ(&src->col[i], &all_colormap->col[j]).
   all_colormap->col[0] is reserved for transparency; no source color will
   be mapped to it. */

static void all_colormap_add(const Gif_Colormap* src) {
    int i;

    /* expand dst->col if necessary. This might change dst->col */
    if (all_colormap->ncol + src->ncol >= all_colormap->capacity) {
        all_colormap->capacity *= 2;
        Gif_ReArray(all_colormap->col, Gif_Color, all_colormap->capacity);
    }

    for (i = 0; i < src->ncol; ++i) {
        kchistitem* khi = kchist_add(&all_colormap_hist,
                                     kc_makegfcng(&src->col[i]), 0);
        if (!khi->count) {
            all_colormap->col[all_colormap->ncol] = src->col[i];
            all_colormap->col[all_colormap->ncol].pixel = 0;
            khi->count = all_colormap->ncol;
            ++all_colormap->ncol;
        }
        src->col[i].pixel = khi->count;
    }
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


/*****
 * FIND THE SMALLEST BOUNDING RECTANGLE ENCLOSING ALL CHANGES
 **/

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
 * CALCULATE OUTPUT GLOBAL COLORMAP
 **/

static void
increment_penalties(Gif_OptData *opt, penalty_type *penalty, int32_t delta)
{
  int i;
  int all_ncol = all_colormap->ncol;
  uint8_t *need = opt->needed_colors;
  for (i = 1; i < all_ncol; i++)
    if (need[i] == REQUIRED)
      penalty[i] += delta;
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
 * INITIALIZATION AND FINALIZATION
 **/

static int
initialize_optimizer(Gif_Stream *gfs)
{
  int i;

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

    kchist_init(&all_colormap_hist);
    for (i = 0; i < gfs->nimages; i++) {
      Gif_Image *gfi = gfs->images[i];
      if (gfi->local)
        all_colormap_add(gfi->local);
      else
        any_globals = 1;
      if (gfi->transparent >= 0 && first_transparent < 0)
        first_transparent = i;
    }
    if (any_globals)
      all_colormap_add(in_global_map);
    kchist_cleanup(&all_colormap_hist);

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

  /* choose background */
  if (gfs->images[0]->transparent < 0
      && gfs->global && gfs->background < in_global_map->ncol)
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
}


/* two versions of the optimization template */
#define palindex_type uint16_t
#define X(t) t ## 16
#include "opttemplate.c"
#undef palindex_type
#undef X

#define palindex_type uint32_t
#define X(t) t ## 32
#include "opttemplate.c"

/* the interface function! */

void
optimize_fragments(Gif_Stream *gfs, int optimize_flags, int huge_stream)
{
    if (!initialize_optimizer(gfs))
        return;
    if ((unsigned) all_colormap->ncol >= 0xFFFF) {
        create_subimages32(gfs, optimize_flags, !huge_stream);
        create_out_global_map32(gfs);
        create_new_image_data32(gfs, optimize_flags);
        finalize_optimizer_data32();
    } else {
        create_subimages16(gfs, optimize_flags, !huge_stream);
        create_out_global_map16(gfs);
        create_new_image_data16(gfs, optimize_flags);
        finalize_optimizer_data16();
    }
    finalize_optimizer(gfs, optimize_flags);
}
