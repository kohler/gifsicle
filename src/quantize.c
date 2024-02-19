/* quantize.c - Histograms and quantization for gifsicle.
   Copyright (C) 1997-2024 Eddie Kohler, ekohler@gmail.com
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
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <math.h>


/* COLORMAP FUNCTIONS return a palette (a vector of Gif_Colors). The
   pixel fields are undefined; the haspixel fields are all 0. */

typedef struct {
    int first;
    int size;
    uint32_t pixel;
} adaptive_slot;

Gif_Colormap* colormap_median_cut(kchist* kch, Gt_OutputData* od)
{
    int adapt_size = od->colormap_size;
    adaptive_slot *slots = Gif_NewArray(adaptive_slot, adapt_size);
    Gif_Colormap *gfcm = Gif_NewFullColormap(adapt_size, 256);
    Gif_Color *adapt = gfcm->col;
    int nadapt;
    int i, j, k;

    /* This code was written with reference to ppmquant by Jef Poskanzer,
       part of the pbmplus package. */

    if (adapt_size < 2 || adapt_size > 256)
        fatal_error("adaptive palette size must be between 2 and 256");
    if (adapt_size >= kch->n && !od->colormap_fixed)
        warning(1, "trivial adaptive palette (only %d %s in source)",
                kch->n, kch->n == 1 ? "color" : "colors");
    if (adapt_size >= kch->n)
        adapt_size = kch->n;

    /* 0. remove any transparent color from consideration; reduce adaptive
       palette size to accommodate transparency if it looks like that'll be
       necessary */
    if (adapt_size > 2 && adapt_size < kch->n && kch->n <= 265
        && od->colormap_needs_transparency)
        adapt_size--;

    /* 1. set up the first slot, containing all pixels. */
    slots[0].first = 0;
    slots[0].size = kch->n;
    slots[0].pixel = 0;
    for (i = 0; i < kch->n; i++)
        slots[0].pixel += kch->h[i].count;

    /* 2. split slots until we have enough. */
    for (nadapt = 1; nadapt < adapt_size; nadapt++) {
        adaptive_slot *split = 0;
        kcolor minc, maxc;
        kchistitem *slice;

        /* 2.1. pick the slot to split. */
        {
            uint32_t split_pixel = 0;
            for (i = 0; i < nadapt; i++)
                if (slots[i].size >= 2 && slots[i].pixel > split_pixel) {
                    split = &slots[i];
                    split_pixel = slots[i].pixel;
                }
            if (!split)
                break;
        }
        slice = &kch->h[split->first];

        /* 2.2. find its extent. */
        {
            kchistitem *trav = slice;
            minc = maxc = trav->ka.k;
            for (i = 1, trav++; i < split->size; i++, trav++)
                for (k = 0; k != 3; ++k) {
                    minc.a[k] = min(minc.a[k], trav->ka.a[k]);
                    maxc.a[k] = max(maxc.a[k], trav->ka.a[k]);
                }
        }

        /* 2.3. decide how to split it. use the luminance method. also sort
           the colors. */
        {
            double red_diff = 0.299 * (maxc.a[0] - minc.a[0]);
            double green_diff = 0.587 * (maxc.a[1] - minc.a[1]);
            double blue_diff = 0.114 * (maxc.a[2] - minc.a[2]);
            if (red_diff >= green_diff && red_diff >= blue_diff)
                qsort(slice, split->size, sizeof(kchistitem), kchistitem_compare_red);
            else if (green_diff >= blue_diff)
                qsort(slice, split->size, sizeof(kchistitem), kchistitem_compare_green);
            else
                qsort(slice, split->size, sizeof(kchistitem), kchistitem_compare_blue);
        }

        /* 2.4. decide where to split the slot and split it there. */
        {
            uint32_t half_pixels = split->pixel / 2;
            uint32_t pixel_accum = slice[0].count;
            uint32_t diff1, diff2;
            for (i = 1; i < split->size - 1 && pixel_accum < half_pixels; i++)
                pixel_accum += slice[i].count;

            /* We know the area before the split has more pixels than the
               area after, possibly by a large margin (bad news). If it
               would shrink the margin, change the split. */
            diff1 = 2*pixel_accum - split->pixel;
            diff2 = split->pixel - 2*(pixel_accum - slice[i-1].count);
            if (diff2 < diff1 && i > 1) {
                i--;
                pixel_accum -= slice[i].count;
            }

            slots[nadapt].first = split->first + i;
            slots[nadapt].size = split->size - i;
            slots[nadapt].pixel = split->pixel - pixel_accum;
            split->size = i;
            split->pixel = pixel_accum;
        }
    }

    /* 3. make the new palette by choosing one color from each slot. */
    for (i = 0; i < nadapt; i++) {
        double px[3];
        kchistitem* slice = &kch->h[ slots[i].first ];
        kcolor kc;
        px[0] = px[1] = px[2] = 0;
        for (j = 0; j != slots[i].size; ++j)
            for (k = 0; k != 3; ++k)
                px[k] += slice[j].ka.a[k] * (double) slice[j].count;
        kc.a[0] = (int) (px[0] / slots[i].pixel);
        kc.a[1] = (int) (px[1] / slots[i].pixel);
        kc.a[2] = (int) (px[2] / slots[i].pixel);
        adapt[i] = kc_togfcg(kc);
    }

    Gif_DeleteArray(slots);
    gfcm->ncol = nadapt;
    return gfcm;
}


void kcdiversity_init(kcdiversity* div, kchist* kch, int dodither) {
    int i;
    div->kch = kch;
    qsort(kch->h, kch->n, sizeof(kchistitem), kchistitem_compare_popularity);
    div->closest = Gif_NewArray(int, kch->n);
    div->min_dist = Gif_NewArray(uint32_t, kch->n);
    for (i = 0; i != kch->n; ++i)
        div->min_dist[i] = (uint32_t) -1;
    if (dodither) {
        div->min_dither_dist = Gif_NewArray(uint32_t, kch->n);
        for (i = 0; i != kch->n; ++i)
            div->min_dither_dist[i] = (uint32_t) -1;
    } else
        div->min_dither_dist = NULL;
    div->chosen = Gif_NewArray(int, kch->n);
    div->nchosen = 0;
}

void kcdiversity_cleanup(kcdiversity* div) {
    Gif_DeleteArray(div->closest);
    Gif_DeleteArray(div->min_dist);
    Gif_DeleteArray(div->min_dither_dist);
    Gif_DeleteArray(div->chosen);
}

int kcdiversity_find_popular(kcdiversity* div) {
    int i, n = div->kch->n;
    for (i = 0; i != n && div->min_dist[i] == 0; ++i)
        /* spin */;
    return i;
}

int kcdiversity_find_diverse(kcdiversity* div, double ditherweight) {
    int i, n = div->kch->n, chosen = kcdiversity_find_popular(div);
    if (chosen == n)
        /* skip */;
    else if (!ditherweight || !div->min_dither_dist) {
        for (i = chosen + 1; i != n; ++i)
            if (div->min_dist[i] > div->min_dist[chosen])
                chosen = i;
    } else {
        double max_dist = div->min_dist[chosen] + ditherweight * div->min_dither_dist[chosen];
        for (i = chosen + 1; i != n; ++i)
            if (div->min_dist[i] != 0) {
                double dist = div->min_dist[i] + ditherweight * div->min_dither_dist[i];
                if (dist > max_dist) {
                    chosen = i;
                    max_dist = dist;
                }
            }
    }
    return chosen;
}

int kcdiversity_choose(kcdiversity* div, int chosen, int dodither) {
    int i, j, k, n = div->kch->n;
    kchistitem* hist = div->kch->h;

    div->min_dist[chosen] = 0;
    if (div->min_dither_dist)
        div->min_dither_dist[chosen] = 0;
    div->closest[chosen] = chosen;

    /* adjust the min_dist array */
    for (i = 0; i != n; ++i)
        if (div->min_dist[i]) {
            uint32_t dist = kc_distance(hist[i].ka.k, hist[chosen].ka.k);
            if (dist < div->min_dist[i]) {
                div->min_dist[i] = dist;
                div->closest[i] = chosen;
            }
        }

    /* also account for dither distances */
    if (dodither && div->min_dither_dist)
        for (i = 0; i != div->nchosen; ++i) {
            kcolor x = hist[chosen].ka.k, *y = &hist[div->chosen[i]].ka.k;
            /* penalize combinations with large luminance difference */
            double dL = abs(kc_luminance(x) - kc_luminance(*y));
            dL = (dL > 8192 ? dL * 4 / 32767. : 1);
            /* create combination */
            for (k = 0; k != 3; ++k)
                x.a[k] = (x.a[k] + y->a[k]) >> 1;
            /* track closeness of combination to other colors */
            for (j = 0; j != n; ++j)
                if (div->min_dist[j]) {
                    double dist = kc_distance(hist[j].ka.k, x) * dL;
                    if (dist < div->min_dither_dist[j])
                        div->min_dither_dist[j] = (uint32_t) dist;
                }
        }

    div->chosen[div->nchosen] = chosen;
    ++div->nchosen;
    return chosen;
}

static void colormap_diversity_do_blend(kcdiversity* div) {
    int i, j, k, n = div->kch->n;
    kchistitem* hist = div->kch->h;
    int* chosenmap = Gif_NewArray(int, n);
    scale_color* di = Gif_NewArray(scale_color, div->nchosen);
    for (i = 0; i != div->nchosen; ++i)
        for (k = 0; k != 4; ++k)
            di[i].a[k] = 0;
    for (i = 0; i != div->nchosen; ++i)
        chosenmap[div->chosen[i]] = i;
    for (i = 0; i != n; ++i) {
        double count = hist[i].count;
        if (div->closest[i] == i)
            count *= 3;
        j = chosenmap[div->closest[i]];
        for (k = 0; k != 3; ++k)
            di[j].a[k] += hist[i].ka.a[k] * count;
        di[j].a[3] += count;
    }
    for (i = 0; i != div->nchosen; ++i) {
        int match = div->chosen[i];
        if (di[i].a[3] >= 5 * hist[match].count)
            for (k = 0; k != 3; ++k)
                hist[match].ka.a[k] = (int) (di[i].a[k] / di[i].a[3]);
    }
    Gif_DeleteArray(chosenmap);
    Gif_DeleteArray(di);
}


static Gif_Colormap *
colormap_diversity(kchist* kch, Gt_OutputData* od, int blend)
{
    int adapt_size = od->colormap_size;
    kcdiversity div;
    Gif_Colormap* gfcm = Gif_NewFullColormap(adapt_size, 256);
    int nadapt = 0;
    int chosen;

    /* This code uses XV's modified diversity algorithm, and was written
       with reference to XV's implementation of that algorithm by John Bradley
       <bradley@cis.upenn.edu> and Tom Lane <Tom.Lane@g.gp.cs.cmu.edu>. */

    if (adapt_size < 2 || adapt_size > 256)
        fatal_error("adaptive palette size must be between 2 and 256");
    if (adapt_size > kch->n && !od->colormap_fixed)
        warning(1, "trivial adaptive palette (only %d colors in source)", kch->n);
    if (adapt_size > kch->n)
        adapt_size = kch->n;

    /* 0. remove any transparent color from consideration; reduce adaptive
       palette size to accommodate transparency if it looks like that'll be
       necessary */
    /* It will be necessary to accommodate transparency if (1) there is
       transparency in the image; (2) the adaptive palette isn't trivial; and
       (3) there are a small number of colors in the image (arbitrary constant:
       <= 265), so it's likely that most images will use most of the slots, so
       it's likely there won't be unused slots. */
    if (adapt_size > 2 && adapt_size < kch->n && kch->n <= 265
        && od->colormap_needs_transparency)
        adapt_size--;

    /* blending has bad effects when there are very few colors */
    if (adapt_size < 4)
        blend = 0;

    /* 1. initialize min_dist and sort the colors in order of popularity. */
    kcdiversity_init(&div, kch, od->dither_type != dither_none);

    /* 2. choose colors one at a time */
    for (nadapt = 0; nadapt < adapt_size; nadapt++) {
        /* 2.1. choose the color to be added */
        if (nadapt == 0 || (nadapt >= 10 && nadapt % 2 == 0))
            /* 2.1a. want most popular unchosen color */
            chosen = kcdiversity_find_popular(&div);
        else if (od->dither_type == dither_none)
            /* 2.1b. choose based on diversity from unchosen colors */
            chosen = kcdiversity_find_diverse(&div, 0);
        else {
            /* 2.1c. choose based on diversity from unchosen colors, but allow
               dithered combinations to stand in for colors, particularly early
               on in the color finding process */
            /* Weight assigned to dithered combinations drops as we proceed. */
#if HAVE_POW
            double ditherweight = 0.05 + pow(0.25, 1 + (nadapt - 1) / 3.);
#else
            double ditherweight = nadapt < 4 ? 0.25 : 0.125;
#endif
            chosen = kcdiversity_find_diverse(&div, ditherweight);
        }

        kcdiversity_choose(&div, chosen,
                           od->dither_type != dither_none
                           && nadapt > 0 && nadapt < 64);
    }

    /* 3. make the new palette by choosing one color from each slot. */
    if (blend)
        colormap_diversity_do_blend(&div);

    for (nadapt = 0; nadapt != div.nchosen; ++nadapt)
        gfcm->col[nadapt] = kc_togfcg(kch->h[div.chosen[nadapt]].ka.k);
    gfcm->ncol = nadapt;

    kcdiversity_cleanup(&div);
    return gfcm;
}


Gif_Colormap* colormap_blend_diversity(kchist* kch, Gt_OutputData* od) {
    return colormap_diversity(kch, od, 1);
}

Gif_Colormap* colormap_flat_diversity(kchist* kch, Gt_OutputData* od) {
    return colormap_diversity(kch, od, 0);
}


void
colormap_image_posterize(Gif_Image *gfi, uint8_t *new_data,
                         Gif_Colormap *old_cm, kd3_tree* kd3,
                         uint32_t *histogram)
{
  int ncol = old_cm->ncol;
  Gif_Color *col = old_cm->col;
  int map[256];
  int i, j;
  int transparent = gfi->transparent;

  /* find closest colors in new colormap */
  assert(old_cm->capacity >= 256);
  for (i = 0; i < ncol; ++i) {
      map[i] = col[i].pixel = kd3_closest8g(kd3, col[i].gfc_red, col[i].gfc_green, col[i].gfc_blue);
      col[i].haspixel = 1;
  }
  for (i = ncol; i < 256; ++i) {
      map[i] = col[i].pixel = 0;
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


#define DITHER_SCALE    1024
#define DITHER_SHIFT    10
#define DITHER_SCALE_M1 (DITHER_SCALE-1)
#define DITHER_ITEM2ERR (1<<(DITHER_SHIFT-7))
#define N_RANDOM_VALUES 512

void
colormap_image_floyd_steinberg(Gif_Image *gfi, uint8_t *all_new_data,
                               Gif_Colormap *old_cm, kd3_tree* kd3,
                               uint32_t *histogram)
{
  static int32_t *random_values = 0;

  int width = gfi->width;
  int dither_direction = 0;
  int transparent = gfi->transparent;
  int i, j, k;
  kcolor *old_kc;
  wkcolor *err, *err1;

  /* Initialize distances; beware uninitialized colors */
  assert(old_cm->capacity >= 256);
  old_kc = Gif_NewArray(kcolor, old_cm->capacity);
  for (i = 0; i < old_cm->ncol; ++i) {
      Gif_Color* c = &old_cm->col[i];
      old_kc[i] = kd3_makegfcg(kd3, c);
      c->pixel = kd3_closest_transformed(kd3, old_kc[i], NULL);
      c->haspixel = 1;
  }
  for (i = old_cm->ncol; i < 256; ++i) {
      Gif_Color* c = &old_cm->col[i];
      old_kc[i] = kd3_makegfcg(kd3, c);
      c->pixel = 0;
      c->haspixel = 1;
  }

  /* This code was written with reference to ppmquant by Jef Poskanzer, part
     of the pbmplus package. */

  /* Initialize Floyd-Steinberg error vectors to small random values, so we
     don't get artifacts on the top row */
  err = Gif_NewArray(wkcolor, width + 2);
  err1 = Gif_NewArray(wkcolor, width + 2);
  /* Use the same random values on each call in an attempt to minimize
     "jumping dithering" effects on animations */
  if (!random_values) {
    random_values = Gif_NewArray(int32_t, N_RANDOM_VALUES);
    for (i = 0; i < N_RANDOM_VALUES; i++)
      random_values[i] = RANDOM() % (DITHER_SCALE_M1 * 2) - DITHER_SCALE_M1;
  }
  for (i = 0; i < gfi->width + 2; i++) {
    j = (i + gfi->left) * 3;
    for (k = 0; k < 3; ++k)
        err[i].a[k] = random_values[ (j + k) % N_RANDOM_VALUES ];
  }
  /* err1 initialized below */

  kd3_build_xradius(kd3);

  /* Do the image! */
  for (j = 0; j < gfi->height; j++) {
    int d0, d1, d2, d3;         /* used for error diffusion */
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
    new_data = all_new_data + j * (unsigned) width + x;

    for (i = 0; i < width + 2; i++)
      err1[i].a[0] = err1[i].a[1] = err1[i].a[2] = 0;

    /* Do a single row */
    while (x >= 0 && x < width) {
      int e;
      kcolor use;

      /* the transparent color never gets adjusted */
      if (*data == transparent)
        goto next;

      /* find desired new color */
      use = old_kc[*data];

      /* use Floyd-Steinberg errors to adjust */
      for (k = 0; k < 3; ++k) {
          int v = use.a[k]
              + (err[x+1].a[k] & ~(DITHER_ITEM2ERR-1)) / DITHER_ITEM2ERR;
          use.a[k] = KC_CLAMPV(v);
      }

      e = old_cm->col[*data].pixel;
      if (kc_distance(kd3->ks[e], use) < kd3->xradius[e])
          *new_data = e;
      else
          *new_data = kd3_closest_transformed(kd3, use, NULL);
      histogram[*new_data]++;

      /* calculate and propagate the error between desired and selected color.
         Assume that, with a large scale (1024), we don't need to worry about
         image artifacts caused by error accumulation (the fact that the
         error terms might not sum to the error). */
      for (k = 0; k < 3; ++k) {
          e = (use.a[k] - kd3->ks[*new_data].a[k]) * DITHER_ITEM2ERR;
          if (e) {
              err [x+d0].a[k] += ((e * 7) & ~15) / 16;
              err1[x+d1].a[k] += ((e * 3) & ~15) / 16;
              err1[x+d2].a[k] += ((e * 5) & ~15) / 16;
              err1[x+d3].a[k] += ( e      & ~15) / 16;
          }
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
      wkcolor *temp = err1;
      err1 = err;
      err = temp;
      dither_direction = !dither_direction;
    }
  }

  /* delete temporary storage */
  Gif_DeleteArray(err);
  Gif_DeleteArray(err1);
  Gif_DeleteArray(old_kc);
}

/* This function is a variant of the colormap_image_floyd_steinberg function
    since the Atkinson dither is a variant of the Floyd-Steinberg dither. */
void
colormap_image_atkinson(Gif_Image *gfi, uint8_t *all_new_data,
                        Gif_Colormap *old_cm, kd3_tree* kd3,
                        uint32_t *histogram)
{
  static int32_t *random_values = 0;
  int transparent = gfi->transparent;
  int i, j, k;
  kcolor *old_kc;
  wkcolor *err[3];

  /* Initialize distances; beware uninitialized colors */
  assert(old_cm->capacity >= 256);
  old_kc = Gif_NewArray(kcolor, old_cm->capacity);
  for (i = 0; i < old_cm->ncol; ++i) {
      Gif_Color* c = &old_cm->col[i];
      old_kc[i] = kd3_makegfcg(kd3, c);
      c->pixel = kd3_closest_transformed(kd3, old_kc[i], NULL);
      c->haspixel = 1;
  }
  for (i = old_cm->ncol; i < 256; ++i) {
      Gif_Color* c = &old_cm->col[i];
      old_kc[i] = kd3_makegfcg(kd3, c);
      c->pixel = 0;
      c->haspixel = 1;
  }

  /* Initialize Atkinson error vectors */
  for (i = 0; i < 3; i++)
    err[i] = Gif_NewArray(wkcolor, gfi->width + 2);

  /* Use the same random values on each call in an attempt to minimize
     "jumping dithering" effects on animations */
  if (!random_values) {
    random_values = Gif_NewArray(int32_t, N_RANDOM_VALUES);
    for (i = 0; i < N_RANDOM_VALUES; i++)
      random_values[i] = RANDOM() % (DITHER_SCALE_M1 * 2) - DITHER_SCALE_M1;
  }

  for (i = 0; i < gfi->width + 2; i++) {
    j = (i + gfi->left) * 3;
    for (k = 0; k < 3; ++k)
        err[0][i].a[k] = random_values[ (j + k) % N_RANDOM_VALUES ];
  }

  kd3_build_xradius(kd3);

  /* Do the image! */
  for (j = 0; j < gfi->height; j++) {
    uint8_t *data = &gfi->img[j][0];
    uint8_t *new_data = all_new_data + j * (unsigned) gfi->width;

    /* Initialize error rows for this pass */
    for (i = 0; i < gfi->width + 2; i++)
      for (k = 0; k < 3; k++)
        err[1][i].a[k] = err[2][i].a[k] = 0;

    for (int x = 0; x < gfi->width; x++, data++, new_data++) {
      int e;
      kcolor use;

      /* Transparent color never gets adjusted, skip */
      if (*data == transparent) {
        continue;
      }

      /* Calculate desired new color including current error */
      use = old_kc[*data];

      for (k = 0; k < 3; ++k)
        use.a[k] = KC_CLAMPV(use.a[k] + err[0][x].a[k] / DITHER_SCALE);

      /* Find the closest color in the colormap */
      e = old_cm->col[*data].pixel;
      if (kc_distance(kd3->ks[e], use) < kd3->xradius[e])
          *new_data = e;
      else
          *new_data = kd3_closest_transformed(kd3, use, NULL);
      histogram[*new_data]++;

      /* Calculate and propagate the error using the Atkinson dithering
         algorithm */
      for (k = 0; k < 3; ++k) {
        int e = (use.a[k] - kd3->ks[*new_data].a[k]) * DITHER_SCALE;
        if (x + 1 < gfi->width) err[0][x + 1].a[k] += e / 8;
        if (x + 2 < gfi->width) err[0][x + 2].a[k] += e / 8;
        if (j + 1 < gfi->height) {
          err[1][x].a[k] += e / 8;
          err[1][x + 1].a[k] += e / 8;
          if (x + 1 < gfi->width) err[1][x + 1].a[k] += e / 8;
        }
        if (j + 2 < gfi->height)
          err[2][x].a[k] += e / 8;
      }
    }

    /* Rotate error buffers */
    wkcolor *temp = err[0];
    err[0] = err[1];
    err[1] = err[2];
    err[2] = temp;
  }

  /* Delete temporary storage */
  for (i = 0; i < 3; i++) {
    Gif_DeleteArray(err[i]);
  }
  Gif_DeleteArray(old_kc);
}

typedef struct odselect_planitem {
    uint8_t plan;
    uint16_t frac;
} odselect_planitem;

static int* ordered_dither_lum;

static void plan_from_cplan(uint8_t* plan, int nplan,
                            const odselect_planitem* cp, int ncp, int whole) {
    int i, cfrac_subt = 0, planpos = 0, end_planpos;
    for (i = 0; i != ncp; ++i) {
        cfrac_subt += cp[i].frac;
        end_planpos = cfrac_subt * nplan / whole;
        while (planpos != end_planpos)
            plan[planpos++] = cp[i].plan;
    }
    assert(planpos == nplan);
}

static int ordered_dither_plan_compare(const void* xa, const void* xb) {
    const uint8_t* a = (const uint8_t*) xa;
    const uint8_t* b = (const uint8_t*) xb;
    if (ordered_dither_lum[*a] != ordered_dither_lum[*b])
        return ordered_dither_lum[*a] - ordered_dither_lum[*b];
    else
        return *a - *b;
}

static int kc_line_closest(kcolor p0, kcolor p1, kcolor ref,
                           double* t, unsigned* dist) {
    wkcolor p01, p0ref;
    kcolor online;
    unsigned den;
    int d;
    for (d = 0; d != 3; ++d) {
        p01.a[d] = p1.a[d] - p0.a[d];
        p0ref.a[d] = ref.a[d] - p0.a[d];
    }
    den = (unsigned)
        (p01.a[0]*p01.a[0] + p01.a[1]*p01.a[1] + p01.a[2]*p01.a[2]);
    if (den == 0)
        return 0;
    /* NB: We've run out of bits of precision. We can calculate the
       denominator in unsigned arithmetic, but the numerator might
       be negative, or it might be so large that it is unsigned.
       Calculate the numerator as a double. */
    *t = ((double) p01.a[0]*p0ref.a[0] + p01.a[1]*p0ref.a[1]
          + p01.a[2]*p0ref.a[2]) / den;
    if (*t < 0 || *t > 1)
        return 0;
    for (d = 0; d != 3; ++d) {
        int v = (int) (p01.a[d] * *t) + p0.a[d];
        online.a[d] = KC_CLAMPV(v);
    }
    *dist = kc_distance(online, ref);
    return 1;
}

static int kc_plane_closest(kcolor p0, kcolor p1, kcolor p2, kcolor ref,
                            double* t, unsigned* dist) {
    wkcolor p0ref, p01, p02;
    double n[3], pvec[3], det, qvec[3], u, v;
    int d;

    /* Calculate the non-unit normal of the plane determined by the input
       colors (p0-p2) */
    for (d = 0; d != 3; ++d) {
        p0ref.a[d] = ref.a[d] - p0.a[d];
        p01.a[d] = p1.a[d] - p0.a[d];
        p02.a[d] = p2.a[d] - p0.a[d];
    }
    n[0] = p01.a[1]*p02.a[2] - p01.a[2]*p02.a[1];
    n[1] = p01.a[2]*p02.a[0] - p01.a[0]*p02.a[2];
    n[2] = p01.a[0]*p02.a[1] - p01.a[1]*p02.a[0];

    /* Moeller-Trumbore ray tracing algorithm: trace a ray from `ref` along
       normal `n`; convert to barycentric coordinates to see if the ray
       intersects with the triangle. */
    pvec[0] = n[1]*p02.a[2] - n[2]*p02.a[1];
    pvec[1] = n[2]*p02.a[0] - n[0]*p02.a[2];
    pvec[2] = n[0]*p02.a[1] - n[1]*p02.a[0];

    det = pvec[0]*p01.a[0] + pvec[1]*p01.a[1] + pvec[2]*p01.a[2];
    if (fabs(det) <= 0.0001220703125) /* optimizer will take care of that */
        return 0;
    det = 1 / det;

    u = (p0ref.a[0]*pvec[0] + p0ref.a[1]*pvec[1] + p0ref.a[2]*pvec[2]) * det;
    if (u < 0 || u > 1)
        return 0;

    qvec[0] = p0ref.a[1]*p01.a[2] - p0ref.a[2]*p01.a[1];
    qvec[1] = p0ref.a[2]*p01.a[0] - p0ref.a[0]*p01.a[2];
    qvec[2] = p0ref.a[0]*p01.a[1] - p0ref.a[1]*p01.a[0];

    v = (n[0]*qvec[0] + n[1]*qvec[1] + n[2]*qvec[2]) * det;
    if (v < 0 || v > 1 || u + v > 1)
        return 0;

    /* Now we know at there is a point in the triangle that is closer to
       `ref` than any point along its edges. Return the barycentric
       coordinates for that point and the distance to that point. */
    t[0] = u;
    t[1] = v;
    v = (p02.a[0]*qvec[0] + p02.a[1]*qvec[1] + p02.a[2]*qvec[2]) * det;
    *dist = (unsigned) (v * v * (n[0]*n[0] + n[1]*n[1] + n[2]*n[2]) + 0.5);
    return 1;
}

static void limit_ordered_dither_plan(uint8_t* plan, int nplan, int nc,
                                      kcolor want, kd3_tree* kd3) {
    unsigned mindist, dist;
    int ncp = 0, nbestcp = 0, i, j, k;
    double t[2];
    odselect_planitem cp[256], bestcp[16];
    nc = nc <= 16 ? nc : 16;

    /* sort colors */
    cp[0].plan = plan[0];
    cp[0].frac = 1;
    for (ncp = i = 1; i != nplan; ++i)
        if (plan[i - 1] == plan[i])
            ++cp[ncp - 1].frac;
        else {
            cp[ncp].plan = plan[i];
            cp[ncp].frac = 1;
            ++ncp;
        }

    /* calculate plan */
    mindist = (unsigned) -1;
    for (i = 0; i != ncp; ++i) {
        /* check for closest single color */
        dist = kc_distance(kd3->ks[cp[i].plan], want);
        if (dist < mindist) {
            bestcp[0].plan = cp[i].plan;
            bestcp[0].frac = KC_WHOLE;
            nbestcp = 1;
            mindist = dist;
        }

        for (j = i + 1; nc >= 2 && j < ncp; ++j) {
            /* check for closest blend of two colors */
            if (kc_line_closest(kd3->ks[cp[i].plan],
                                kd3->ks[cp[j].plan],
                                want, &t[0], &dist)
                && dist < mindist) {
                bestcp[0].plan = cp[i].plan;
                bestcp[1].plan = cp[j].plan;
                bestcp[1].frac = (int) (KC_WHOLE * t[0]);
                bestcp[0].frac = KC_WHOLE - bestcp[1].frac;
                nbestcp = 2;
                mindist = dist;
            }

            for (k = j + 1; nc >= 3 && k < ncp; ++k)
                /* check for closest blend of three colors */
                if (kc_plane_closest(kd3->ks[cp[i].plan],
                                     kd3->ks[cp[j].plan],
                                     kd3->ks[cp[k].plan],
                                     want, &t[0], &dist)
                    && dist < mindist) {
                    bestcp[0].plan = cp[i].plan;
                    bestcp[1].plan = cp[j].plan;
                    bestcp[1].frac = (int) (KC_WHOLE * t[0]);
                    bestcp[2].plan = cp[k].plan;
                    bestcp[2].frac = (int) (KC_WHOLE * t[1]);
                    bestcp[0].frac = KC_WHOLE - bestcp[1].frac - bestcp[2].frac;
                    nbestcp = 3;
                    mindist = dist;
                }
        }
    }

    plan_from_cplan(plan, nplan, bestcp, nbestcp, KC_WHOLE);
}

static void set_ordered_dither_plan(uint8_t* plan, int nplan, int nc,
                                    Gif_Color* gfc, kd3_tree* kd3) {
    kcolor want, cur;
    wkcolor err;
    int i, d;

    want = kd3_makegfcg(kd3, gfc);

    wkc_clear(&err);
    for (i = 0; i != nplan; ++i) {
        for (d = 0; d != 3; ++d) {
            int v = want.a[d] + err.a[d];
            cur.a[d] = KC_CLAMPV(v);
        }
        plan[i] = kd3_closest_transformed(kd3, cur, NULL);
        for (d = 0; d != 3; ++d)
            err.a[d] += want.a[d] - kd3->ks[plan[i]].a[d];
    }

    qsort(plan, nplan, 1, ordered_dither_plan_compare);

    if (nc < nplan && plan[0] != plan[nplan-1]) {
        int ncp = 1;
        for (i = 1; i != nplan; ++i)
            ncp += plan[i-1] != plan[i];
        if (ncp > nc)
            limit_ordered_dither_plan(plan, nplan, nc, want, kd3);
    }

    gfc->haspixel = 1;
}

static void pow2_ordered_dither(Gif_Image* gfi, uint8_t* all_new_data,
                                Gif_Colormap* old_cm, kd3_tree* kd3,
                                uint32_t* histogram, const uint8_t* matrix,
                                uint8_t* plan) {
    int mws, nplans, i, x, y;
    for (mws = 0; (1 << mws) != matrix[0]; ++mws)
        /* nada */;
    for (nplans = 0; (1 << nplans) != matrix[2]; ++nplans)
        /* nada */;

    for (y = 0; y != gfi->height; ++y) {
        uint8_t *data, *new_data, *thisplan;
        data = gfi->img[y];
        new_data = all_new_data + y * (unsigned) gfi->width;

        for (x = 0; x != gfi->width; ++x)
            /* the transparent color never gets adjusted */
            if (data[x] != gfi->transparent) {
                thisplan = &plan[data[x] << nplans];
                if (!old_cm->col[data[x]].haspixel)
                    set_ordered_dither_plan(thisplan, 1 << nplans, matrix[3],
                                            &old_cm->col[data[x]], kd3);
                i = matrix[4 + ((x + gfi->left) & (matrix[0] - 1))
                           + (((y + gfi->top) & (matrix[1] - 1)) << mws)];
                new_data[x] = thisplan[i];
                histogram[new_data[x]]++;
            }
    }
}

static void colormap_image_ordered(Gif_Image* gfi, uint8_t* all_new_data,
                                   Gif_Colormap* old_cm, kd3_tree* kd3,
                                   uint32_t* histogram,
                                   const uint8_t* matrix) {
    int mw = matrix[0], mh = matrix[1], nplan = matrix[2];
    uint8_t* plan = Gif_NewArray(uint8_t, nplan * old_cm->ncol);
    int i, x, y;

    /* Written with reference to Joel Ylilouma's versions. */

    /* Initialize colors */
    assert(old_cm->capacity >= 256);
    for (i = 0; i != 256; ++i)
        old_cm->col[i].haspixel = 0;

    /* Initialize luminances, create luminance sorter */
    ordered_dither_lum = Gif_NewArray(int, kd3->nitems);
    for (i = 0; i != kd3->nitems; ++i)
        ordered_dither_lum[i] = kc_luminance(kd3->ks[i]);

    /* Do the image! */
    if ((mw & (mw - 1)) == 0 && (mh & (mh - 1)) == 0
        && (nplan & (nplan - 1)) == 0)
        pow2_ordered_dither(gfi, all_new_data, old_cm, kd3, histogram,
                            matrix, plan);
    else
        for (y = 0; y != gfi->height; ++y) {
            uint8_t *data, *new_data, *thisplan;
            data = gfi->img[y];
            new_data = all_new_data + y * (unsigned) gfi->width;

            for (x = 0; x != gfi->width; ++x)
                /* the transparent color never gets adjusted */
                if (data[x] != gfi->transparent) {
                    thisplan = &plan[nplan * data[x]];
                    if (!old_cm->col[data[x]].haspixel)
                        set_ordered_dither_plan(thisplan, nplan, matrix[3],
                                                &old_cm->col[data[x]], kd3);
                    i = matrix[4 + (x + gfi->left) % mw
                               + ((y + gfi->top) % mh) * mw];
                    new_data[x] = thisplan[i];
                    histogram[new_data[x]]++;
                }
        }

    /* delete temporary storage */
    Gif_DeleteArray(ordered_dither_lum);
    Gif_DeleteArray(plan);
}


static void dither(Gif_Image* gfi, uint8_t* new_data, Gif_Colormap* old_cm,
                   kd3_tree* kd3, uint32_t* histogram, Gt_OutputData* od) {
    if (od->dither_type == dither_default
        || od->dither_type == dither_floyd_steinberg)
        colormap_image_floyd_steinberg(gfi, new_data, old_cm, kd3, histogram);
    else if (od->dither_type == dither_ordered
             || od->dither_type == dither_ordered_new)
        colormap_image_ordered(gfi, new_data, old_cm, kd3, histogram,
                               od->dither_data);
    else if (od->dither_type == dither_atkinson)
        colormap_image_atkinson(gfi, new_data, old_cm, kd3, histogram);
    else
        colormap_image_posterize(gfi, new_data, old_cm, kd3, histogram);
}

/* return value 1 means run the dither again */
static int
try_assign_transparency(Gif_Image *gfi, Gif_Colormap *old_cm, uint8_t *new_data,
                        Gif_Colormap *new_cm, int *new_ncol,
                        kd3_tree* kd3, uint32_t *histogram)
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
  else
    GIF_SETCOLOR(&transp_value, 0, 0, 0);

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
  kd3_disable(kd3, new_transparent); /* mark it as unusable */
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

static int color_compare_popularity(const void* va, const void* vb) {
    const Gif_Color* a = (const Gif_Color*) va;
    const Gif_Color* b = (const Gif_Color*) vb;
    return a->pixel > b->pixel ? -1 : a->pixel != b->pixel;
}

void
colormap_stream(Gif_Stream* gfs, Gif_Colormap* new_cm, Gt_OutputData* od)
{
  kd3_tree kd3;
  Gif_Color *new_col = new_cm->col;
  int new_ncol = new_cm->ncol, new_gray;
  int imagei, j;
  int compress_new_cm = 1;
  assert(new_cm->capacity >= 256);

  /* new_col[j].pixel == number of pixels with color j in the new image. */
  for (j = 0; j < 256; j++)
    new_col[j].pixel = 0;

  /* initialize kd3 tree */
  new_gray = 1;
  for (j = 0; new_gray && j < new_cm->ncol; ++j)
      if (new_col[j].gfc_red != new_col[j].gfc_green
          || new_col[j].gfc_red != new_col[j].gfc_blue)
          new_gray = 0;
  kd3_init_build(&kd3, new_gray ? kc_luminance_transform : NULL, new_cm);

  for (imagei = 0; imagei < gfs->nimages; imagei++) {
    Gif_Image *gfi = gfs->images[imagei];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    int only_compressed = (gfi->img == 0);

    if (gfcm) {
      /* If there was an old colormap, change the image data */
      uint8_t *new_data = Gif_NewArray(uint8_t, (unsigned) gfi->width * (unsigned) gfi->height);
      uint32_t histogram[256];
      unmark_colors(new_cm);
      unmark_colors(gfcm);

      if (only_compressed)
          Gif_UncompressImage(gfs, gfi);

      kd3_enable_all(&kd3);
      do {
        for (j = 0; j < 256; j++)
            histogram[j] = 0;
        dither(gfi, new_data, gfcm, &kd3, histogram, od);
      } while (try_assign_transparency(gfi, gfcm, new_data, new_cm, &new_ncol,
                                       &kd3, histogram));

      Gif_ReleaseUncompressedImage(gfi);
      /* version 1.28 bug fix: release any compressed version or it'll cause
         bad images */
      Gif_ReleaseCompressedImage(gfi);
      Gif_SetUncompressedImage(gfi, new_data, Gif_Free, 0);

      /* update count of used colors */
      for (j = 0; j < 256; j++)
        new_col[j].pixel += histogram[j];
      if (gfi->transparent >= 0)
        /* we don't have data on the number of used colors for transparency
           so fudge it. */
        new_col[gfi->transparent].pixel += (unsigned) gfi->width * (unsigned) gfi->height / 8;

    } else {
      /* Can't compress new_cm afterwards if we didn't actively change colors
         over */
      compress_new_cm = 0;
    }

    if (gfi->local) {
      Gif_DeleteColormap(gfi->local);
      gfi->local = 0;
    }

    /* 1.92: recompress *after* deleting the local colormap */
    if (gfcm && only_compressed) {
        Gif_FullCompressImage(gfs, gfi, &gif_write_info);
        Gif_ReleaseUncompressedImage(gfi);
    }
  }

  /* Set new_cm->ncol from new_ncol. We didn't update new_cm->ncol before so
     the closest-color algorithms wouldn't see any new transparent colors.
     That way added transparent colors were only used for transparency. */
  new_cm->ncol = new_ncol;

  /* change the background. I hate the background by now */
  if ((gfs->nimages == 0 || gfs->images[0]->transparent < 0)
      && gfs->global && gfs->background < gfs->global->ncol) {
      Gif_Color *c = &gfs->global->col[gfs->background];
      gfs->background = kd3_closest8g(&kd3, c->gfc_red, c->gfc_green, c->gfc_blue);
      new_col[gfs->background].pixel++;
  } else if (gfs->nimages > 0 && gfs->images[0]->transparent >= 0)
      gfs->background = gfs->images[0]->transparent;
  else
      gfs->background = 0;

  Gif_DeleteColormap(gfs->global);
  kd3_cleanup(&kd3);

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
    qsort(new_col, new_cm->ncol, sizeof(Gif_Color), color_compare_popularity);

    /* set up the map and reduce the number of colors */
    for (j = 0; j < new_cm->ncol; j++)
      map[ new_col[j].haspixel ] = j;
    for (j = 0; j < new_cm->ncol; j++)
      if (!new_col[j].pixel) {
        gfs->global->ncol = j;
        break;
      }

    /* map the image data, transparencies, and background */
    if (gfs->background < gfs->global->ncol)
        gfs->background = map[gfs->background];
    for (imagei = 0; imagei < gfs->nimages; imagei++) {
      Gif_Image *gfi = gfs->images[imagei];
      int only_compressed = (gfi->img == 0);
      uint32_t size;
      uint8_t *data;
      if (only_compressed)
          Gif_UncompressImage(gfs, gfi);

      data = gfi->image_data;
      for (size = (unsigned) gfi->width * (unsigned) gfi->height; size > 0; size--, data++)
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


/* Halftone algorithms */

static const uint8_t dither_matrix_o3x3[4 + 3*3] = {
    3, 3, 9, 9,
    2, 6, 3,
    5, 0, 8,
    1, 7, 4
};

static const uint8_t dither_matrix_o4x4[4 + 4*4] = {
    4, 4, 16, 16,
     0,  8,  3, 10,
    12,  4, 14,  6,
     2, 11,  1,  9,
    15,  7, 13,  5
};

static const uint8_t dither_matrix_o8x8[4 + 8*8] = {
    8, 8, 64, 64,
     0, 48, 12, 60,  3, 51, 15, 63,
    32, 16, 44, 28, 35, 19, 47, 31,
     8, 56,  4, 52, 11, 59,  7, 55,
    40, 24, 36, 20, 43, 27, 39, 23,
     2, 50, 14, 62,  1, 49, 13, 61,
    34, 18, 46, 30, 33, 17, 45, 29,
    10, 58,  6, 54,  9, 57,  5, 53,
    42, 26, 38, 22, 41, 25, 37, 21
};

static const uint8_t dither_matrix_ro64x64[4 + 64*64] = {
    64, 64, 16, 16,
     6, 15,  2, 15,  2, 14,  1, 13,  2, 14,  5, 13,  0, 14,  0,  9,  6, 10,  7, 13,  6, 13,  3, 10,  5, 15,  4, 11,  0, 11,  6, 10,  7, 12,  7, 13,  0,  9,  6, 15,  6, 10,  0, 15,  1, 15,  0,  8,  0, 15,  6, 15,  7, 15,  7,  9,  1, 15,  3,  8,  1,  8,  0, 14,
     9,  3, 10,  5, 10,  6, 10,  5,  9,  6,  9,  2,  9,  4, 13,  4, 13,  3,  8,  3, 10,  1, 13,  6, 11,  1, 12,  3, 14,  5, 15,  3,  8,  3,  8,  3, 12,  4, 11,  3, 13,  3,  8,  4,  9,  6, 12,  4, 11,  6, 11,  3, 10,  0, 12,  1, 11,  7, 12,  4, 12,  4, 11,  5,
     1, 14,  0, 10,  2,  9,  2, 11,  1,  8,  1,  8,  3,  9,  4, 15,  7, 13,  7, 14,  7, 14,  0, 10,  0, 14,  7,  9,  0, 11,  1, 15,  0, 11,  0, 11,  3, 15,  7, 14,  6, 10,  5,  8,  0, 11,  0,  8,  7, 11,  0, 15,  0, 12,  1, 13,  6,  9,  0, 15,  4,  9,  1,  8,
    10,  5, 15,  6, 13,  6, 14,  7, 14,  4, 12,  5, 15,  6, 10,  2, 11,  3, 10,  3, 11,  3, 13,  6, 11,  5, 14,  3, 14,  6,  8,  5, 14,  5, 14,  7, 10,  7, 11,  3, 13,  3, 13,  2, 14,  4, 15,  5, 15,  3,  8,  4, 11,  4,  9,  5, 12,  3,  8,  5, 15,  2, 13,  5,
     3, 10,  2,  9,  5, 15,  4,  9,  0, 11,  7, 11,  0, 14,  5, 11,  5, 14,  7, 15,  6,  9,  0,  9,  0,  8,  4, 14,  6, 12,  0, 11,  4, 15,  5,  8,  6, 10,  6, 11,  6, 10,  3, 12,  5,  9,  6,  9,  5, 12,  6, 12,  7, 14,  0, 14,  2, 15,  5,  8,  2, 10,  3,  9,
    14,  7, 14,  7,  8,  1, 12,  2, 14,  4, 15,  3, 11,  5, 12,  2,  8,  0, 10,  3, 12,  1, 13,  5, 14,  5, 11,  0, 11,  3, 13,  7,  8,  1, 12,  3, 15,  3, 14,  2, 15,  3, 11,  6, 15,  1, 12,  1,  9,  3,  9,  3, 11,  3, 11,  7, 11,  5, 12,  0, 14,  6, 13,  6,
     5, 10,  6, 12,  3, 15,  3,  8,  7, 10,  0, 15,  7, 10,  5,  8,  1, 14,  5, 10,  7, 14,  2, 14,  0, 14,  1,  8,  3,  8,  5, 10,  5, 15,  0,  8,  6, 14,  0,  8,  2, 15,  4, 11,  6, 10,  0,  8,  5,  9,  4, 14,  1, 15,  3, 10,  1, 15,  0, 15,  5,  8,  7, 13,
    15,  3,  8,  0,  9,  6, 12,  6, 15,  0, 11,  4, 14,  3, 15,  3, 10,  5, 12,  3, 10,  3,  9,  6,  9,  5, 13,  5, 15,  6, 14,  0,  9,  0, 12,  4, 11,  3, 15,  4, 11,  6, 15,  2, 15,  3, 14,  4, 13,  2, 11,  1,  8,  5, 12,  7,  9,  4,  8,  5, 12,  2, 11,  0,
     0, 13,  5, 11,  5, 14,  5, 15,  7,  9,  5,  8,  2, 10,  3, 15,  6,  9,  3, 12,  5, 11,  6, 14,  3, 13,  6, 13,  0,  9,  1, 11,  3,  8,  3,  9,  4,  9,  6, 14,  7,  8,  6, 12,  7, 13,  6,  9,  5, 13,  3, 15,  5, 14,  3, 15,  4, 12,  4, 12,  2, 13,  0, 15,
    10,  7, 14,  1,  8,  2, 10,  2, 12,  0, 13,  1, 13,  5, 11,  6, 12,  3,  9,  6, 15,  1,  9,  1, 10,  6, 10,  3, 15,  6, 15,  5, 15,  7, 12,  6, 12,  0,  9,  3, 12,  3, 11,  3,  8,  3, 13,  2,  9,  3,  9,  6, 10,  2, 11,  6,  9,  0,  9,  1,  9,  6, 10,  4,
     3, 10,  4,  9,  4,  9,  6, 13,  3, 15,  0,  8,  4, 10,  0, 14,  5, 15,  7, 15,  2, 12,  7, 10,  5, 15,  7,  8,  0,  9,  0,  8,  6, 14,  4, 15,  2, 15,  1, 15,  0,  9,  5, 14,  0, 12,  7, 10,  6,  8,  6, 11,  0, 13,  7, 14,  1, 13,  2, 10,  5,  9,  0, 14,
    14,  6, 13,  1, 12,  3,  8,  3,  9,  6, 12,  4, 15,  2,  8,  5,  9,  1, 10,  1,  8,  5, 15,  0,  8,  2, 12,  3, 12,  5, 12,  5, 11,  2, 10,  2,  8,  5, 10,  5, 12,  4,  9,  3,  8,  4, 13,  1, 15,  0, 12,  3,  8,  4, 10,  3,  9,  5, 13,  5, 13,  3, 11,  4,
     4, 12,  3, 10,  6,  9,  0, 14,  0,  8,  1,  9,  1,  9,  6, 15,  0, 10,  2,  9,  6, 10,  7, 10,  2, 14,  4,  8,  0, 13,  4,  8,  1, 11,  6, 14,  7, 14,  0,  8,  4, 14,  7, 14,  4,  9,  2, 12,  0, 12,  3,  8,  5,  9,  6, 14,  0,  9,  2, 14,  5, 10,  1, 15,
    10,  2, 15,  6, 14,  3, 11,  6, 15,  4, 15,  4, 13,  5, 11,  3, 12,  4, 14,  5, 12,  0, 13,  3,  8,  6, 15,  2, 10,  6, 13,  2, 12,  6,  8,  3, 10,  3, 14,  5,  8,  0, 10,  3, 13,  2,  9,  6, 10,  6, 15,  4, 13,  2,  8,  3, 14,  6, 11,  7, 13,  0, 10,  5,
     2, 13,  1,  9,  5, 10,  3, 15,  6, 10,  0, 15,  4, 15,  4, 15,  6,  9,  0, 14,  1,  9,  6, 12,  0, 11,  5,  9,  0, 14,  5, 14,  6,  9,  3, 11,  7, 11,  1, 15,  1,  9,  3, 13,  5, 12,  0,  9,  2,  9,  6, 13,  2,  9,  6, 13,  7, 11,  7,  8,  0, 13,  3,  9,
     9,  6, 13,  6, 13,  0,  8,  5, 14,  2, 11,  7, 11,  2, 11,  2, 12,  3, 11,  4, 13,  4, 10,  3, 15,  5, 13,  0, 11,  6, 10,  0, 13,  3, 15,  7, 14,  3,  8,  4, 12,  6,  9,  6, 10,  3, 14,  5, 13,  5, 11,  3, 13,  6,  8,  2, 14,  3, 15,  3, 11,  4, 12,  6,
     4, 10,  7, 11,  1, 10,  2, 14,  7, 14,  0, 14,  4, 15,  0,  8,  0, 15,  5, 15,  7, 11,  7, 14,  6, 12,  0,  8,  1, 15,  1, 14,  2,  8,  0, 11,  4,  8,  5, 11,  1, 15,  0, 12,  7, 14,  0, 11,  4,  8,  2,  8,  0, 14,  2, 14,  4,  9,  4, 10,  7, 15,  5, 14,
    14,  0, 13,  2, 14,  5, 11,  5, 10,  2, 10,  6, 11,  3, 12,  5, 11,  7, 11,  1, 12,  3, 11,  3,  9,  3, 12,  4, 11,  6,  9,  5, 14,  5, 12,  7, 14,  2, 12,  1,  8,  4, 11,  7, 10,  3, 14,  4, 14,  0, 14,  5, 11,  7, 11,  5, 15,  1, 14,  0, 10,  3, 10,  0,
     5, 12,  7, 10,  7, 11,  0, 14,  5, 14,  1, 14,  3,  8,  0, 15,  0,  9,  5,  9,  1,  8,  6, 15,  6, 15,  7, 13,  7,  9,  4, 11,  0, 10,  6, 10,  7, 12,  7, 15,  0,  9,  7, 14,  2, 11,  0, 12,  7, 11,  7, 13,  0,  9,  6, 14,  2, 11,  1, 15,  0,  8,  0, 10,
    10,  3, 15,  3, 15,  3,  8,  4, 11,  3, 11,  6, 15,  6, 11,  5, 12,  5, 13,  0, 15,  4,  8,  1, 11,  3, 10,  3, 15,  3, 14,  1, 14,  6, 13,  3, 11,  3, 11,  3, 13,  6,  8,  3, 12,  5,  8,  4, 13,  3,  8,  2, 13,  4,  8,  3, 14,  7,  8,  5, 14,  4, 15,  5,
     3,  9,  0, 10,  0, 12,  3, 11,  7, 10,  6,  8,  7, 12,  0, 11,  1,  8,  5, 12,  5, 12,  7, 11,  2, 14,  0, 10,  0, 12,  0, 15,  2,  9,  5,  9,  0, 15,  4, 14,  6,  9,  7, 11,  6, 10,  0,  9,  1,  9,  0, 14,  4, 12,  5,  9,  6, 10,  4, 11,  6, 12,  5, 12,
    12,  6, 15,  4,  8,  4, 15,  7, 15,  3, 14,  0,  9,  3, 13,  5, 14,  5, 11,  1,  9,  0, 15,  3,  8,  7, 12,  7,  8,  5,  9,  4, 12,  6, 12,  1, 10,  6,  9,  0, 15,  3, 14,  0, 15,  3, 13,  5, 15,  5,  9,  6, 10,  0, 14,  0, 13,  1, 15,  1,  8,  3,  8,  1,
     2,  8,  7, 15,  0, 11,  1, 12,  1, 12,  0, 15,  0, 11,  2, 13,  1, 10,  5,  8,  7, 13,  6, 10,  0, 11,  4, 12,  7, 10,  1,  9,  1,  9,  0, 12,  2, 11,  7, 15,  1, 11,  0,  9,  2,  8,  4, 12,  2, 12,  6, 14,  4, 14,  3, 13,  3, 15,  1, 14,  4, 15,  6, 12,
    13,  6, 11,  3, 12,  4,  8,  4,  9,  4,  9,  5, 12,  6, 10,  5, 14,  7, 14,  3, 10,  2, 13,  3, 14,  5,  8,  2, 13,  1, 15,  6, 15,  4,  8,  4, 15,  5, 10,  1, 14,  5, 12,  4, 13,  5, 11,  1,  9,  5,  9,  3,  9,  0, 10,  7, 10,  7, 11,  6, 10,  3,  9,  0,
     3, 15,  5,  8,  1,  9,  5,  8,  4, 13,  6,  8,  4, 15,  6, 10,  6, 14,  7, 11,  0,  9,  1, 12,  0,  8,  6, 12,  6, 10,  3, 12,  7, 13,  7, 11,  3,  8,  0, 13,  7, 12,  0, 15,  4, 12,  5,  8,  4, 15,  5, 10,  0, 15,  3, 15,  1,  8,  1, 15,  4, 14,  6, 15,
    10,  6, 14,  0, 13,  4, 13,  1,  9,  1, 13,  3, 11,  2, 12,  1,  8,  3, 12,  3, 14,  6,  8,  4, 15,  4,  8,  3, 15,  1,  9,  6, 11,  2, 14,  3, 13,  7, 10,  4, 10,  3,  8,  7, 11,  2, 12,  1, 11,  1, 14,  3,  9,  6,  9,  7, 13,  5, 11,  7, 10,  2,  9,  3,
     1,  9,  0, 14,  7, 12,  7, 14,  5,  9,  6, 10,  6, 14,  6, 15,  1, 14,  7, 10,  3, 12,  0, 14,  5,  9,  6,  8,  1, 12,  0, 13,  2,  8,  4,  9,  6, 15,  0, 14,  4, 10,  7, 11,  5, 14,  2,  8,  4, 15,  3,  8,  7, 13,  1,  9,  0, 15,  0, 14,  5, 14,  0, 14,
    15,  4, 10,  6, 11,  1, 10,  2, 14,  3, 15,  1, 10,  3, 10,  3,  8,  4, 12,  3, 10,  6,  9,  6, 14,  2, 12,  3, 11,  4,  9,  6, 12,  5, 15,  0, 10,  3,  8,  4, 14,  1, 14,  3, 11,  2, 12,  7, 11,  2, 12,  6, 10,  1, 13,  4, 11,  4, 10,  5, 11,  2, 11,  7,
     0,  8,  1, 11,  4,  8,  4, 10,  6, 11,  3, 10,  6, 10,  6,  8,  1, 11,  1, 15,  7, 15,  6,  9,  1,  9,  7,  8,  0, 14,  5, 12,  3, 13,  4, 12,  0, 15,  5, 12,  1, 12,  7,  9,  6,  8,  3, 15,  0, 13,  6, 15,  7, 15,  7, 13,  2, 15,  1, 14,  2, 15,  0,  8,
    12,  5, 13,  5, 15,  1, 12,  1, 15,  1, 14,  6, 14,  2, 14,  2, 14,  6, 11,  6, 11,  3, 12,  2, 13,  6, 15,  3,  9,  5,  8,  2,  8,  5,  9,  1,  9,  4,  9,  2,  8,  4, 13,  1, 13,  0, 11,  6, 11,  6,  9,  2, 11,  3, 10,  1, 10,  6,  9,  6, 11,  7, 15,  5,
     5, 12,  0,  9,  5,  8,  5,  9,  0, 10,  2, 13,  4,  8,  5, 14,  2,  9,  2, 15,  4,  8,  2,  8,  0, 12,  6, 12,  0, 10,  5, 15,  4,  9,  3, 13,  0,  9,  4, 15,  1, 14,  1,  9,  0,  9,  2, 14,  6, 12,  0, 15,  7,  9,  2,  9,  0, 15,  6, 10,  3, 15,  3, 13,
     9,  1, 15,  5, 12,  3, 14,  0, 15,  7,  8,  5, 14,  1, 11,  1, 13,  6, 10,  7, 13,  0, 14,  5, 10,  7, 11,  3, 15,  6, 11,  1, 12,  2,  8,  4, 13,  5, 10,  2, 10,  6, 13,  5, 13,  4, 10,  5,  9,  3,  8,  4, 12,  1, 12,  5, 11,  6, 14,  3, 11,  7, 11,  7,
     2, 11,  4, 12,  7,  8,  3, 15,  3, 11,  4, 11,  7, 10,  1, 10,  2, 10,  0, 11,  4, 12,  0, 10,  1,  8,  7, 11,  1, 10,  1, 15,  7, 10,  2, 11,  1,  9,  6, 14,  2, 11,  1, 14,  4, 12,  2, 13,  1,  9,  5,  8,  7, 11,  7, 10,  7, 15,  4, 13,  3, 10,  1, 11,
    13,  5,  8,  0, 15,  3, 11,  6, 12,  7, 15,  3, 15,  1, 15,  4, 14,  5, 13,  6,  8,  1, 15,  6, 13,  5, 12,  2, 15,  4,  9,  4, 15,  3, 14,  6, 14,  5,  9,  3, 12,  6, 11,  5,  8,  2,  9,  5, 12,  5, 15,  1, 13,  3, 14,  2,  8,  0,  8,  0, 12,  7, 15,  5,
     0, 14,  1,  8,  2,  8,  2, 15,  2,  9,  6, 12,  0, 12,  7, 12,  7, 13,  0, 15,  6, 14,  5, 11,  1, 12,  5,  8,  5,  8,  1, 14,  0, 10,  4, 12,  6,  9,  6,  8,  0,  9,  0, 15,  4, 15,  5, 13,  1, 15,  5, 12,  7, 10,  6, 14,  4,  9,  6, 15,  0, 15,  7, 13,
    10,  4, 13,  5, 13,  5, 11,  5, 12,  5,  9,  3,  9,  4,  8,  3,  9,  3, 10,  6, 11,  1, 15,  2,  8,  5, 15,  1, 13,  2, 11,  6, 14,  6,  8,  2, 15,  3, 14,  3, 13,  6,  9,  6,  9,  0, 10,  2, 10,  7, 11,  1, 15,  3,  9,  3, 13,  3, 10,  3,  9,  4, 10,  1,
     4, 11,  7, 10,  2, 14,  4, 15,  3, 15,  2, 15,  7, 10,  2, 14,  1,  8,  0, 15,  2,  8,  1, 12,  2, 13,  1,  8,  7, 12,  6, 11,  1, 10,  4, 11,  2, 15,  0, 13,  0, 12,  7, 11,  5, 12,  7,  8,  6, 15,  4, 12,  7, 10,  6, 10,  0, 10,  0, 11,  4, 12,  7, 10,
    13,  2, 14,  2, 10,  7, 11,  1, 11,  7, 11,  7, 12,  3, 11,  7, 13,  4, 11,  6, 12,  5, 11,  6, 10,  7, 13,  4, 11,  3, 15,  3, 14,  4, 15,  3,  8,  5, 10,  7, 10,  6, 15,  3,  9,  3, 15,  1, 11,  3,  9,  0, 15,  1, 15,  3, 15,  4, 13,  5,  8,  3, 15,  3,
     3,  9,  1, 12,  4, 14,  0, 11,  7, 15,  1, 11,  3, 14,  0, 11,  5, 10,  2, 10,  2,  8,  6, 14,  7, 11,  0, 10,  7, 14,  7, 15,  6,  9,  6,  9,  1, 12,  7,  9,  4,  8,  7, 13,  0, 13,  6, 12,  6,  8,  1,  9,  1, 15,  4, 12,  0, 12,  4, 10,  0, 13,  0, 12,
    13,  5, 10,  6,  9,  1, 13,  6, 11,  0, 14,  7, 11,  7, 14,  5, 15,  2, 13,  5, 15,  4,  9,  2, 13,  3, 14,  7, 10,  3, 10,  3, 12,  3, 12,  2,  8,  5, 15,  3, 13,  3, 11,  3, 10,  4,  9,  3, 15,  1, 14,  4, 11,  6, 10,  3,  8,  4, 13,  0, 10,  4,  8,  5,
     5,  8,  3, 13,  4, 14,  1, 13,  7,  9,  2, 14,  4, 14,  4, 12,  7, 10,  6, 10,  5, 12,  2, 11,  6, 12,  7, 13,  4, 11,  2, 11,  3,  8,  5, 15,  2, 10,  1,  9,  2, 12,  0, 12,  0, 10,  1, 12,  0,  9,  7, 10,  0, 15,  4,  9,  0,  8,  1, 14,  4, 14,  4, 13,
    12,  2,  8,  6, 10,  0,  8,  5, 13,  3,  8,  5, 10,  3, 11,  2, 15,  0, 13,  3,  8,  2, 15,  6,  9,  1,  8,  3, 14,  1, 15,  5, 14,  7,  8,  0, 12,  6, 15,  5,  8,  5,  9,  4, 15,  5,  8,  5, 15,  4, 12,  3,  9,  6, 12,  3, 13,  5, 11,  7, 10,  1,  8,  3,
     4, 15,  2, 11,  2, 10,  6, 11,  3, 14,  7, 15,  0, 10,  0, 14,  3, 12,  4,  8,  7,  9,  3, 11,  0, 13,  2, 12,  2, 13,  5,  8,  5,  8,  2, 11,  4, 12,  2, 10,  7,  8,  6, 10,  7, 12,  7, 11,  0, 15,  7,  8,  7, 15,  0,  9,  2, 12,  6, 13,  0,  9,  4,  9,
     8,  3, 12,  5, 15,  5, 13,  1,  8,  7,  8,  3, 12,  5,  9,  5,  9,  6, 14,  1, 12,  3, 15,  7, 10,  7,  8,  5,  8,  5, 15,  0, 13,  1, 14,  7,  9,  1, 14,  7, 13,  2, 12,  3,  8,  1, 13,  3,  8,  4, 12,  0, 11,  3, 12,  5, 11,  5,  9,  3, 12,  6, 14,  2,
     5, 13,  2,  8,  6, 12,  7, 10,  5, 14,  4, 11,  2, 11,  2, 13,  2, 13,  5, 10,  5,  9,  0, 10,  7, 13,  4, 15,  2,  9,  1,  9,  5,  9,  6, 10,  4, 12,  5, 12,  4, 10,  3, 14,  3, 15,  1,  8,  4, 12,  5,  8,  5, 14,  7, 11,  7, 15,  5, 11,  2, 13,  1,  9,
    10,  0, 13,  6, 10,  2, 15,  3,  8,  1, 12,  2, 14,  6,  8,  5, 10,  7, 13,  1, 13,  1, 12,  7,  9,  1,  9,  2, 12,  5, 12,  4, 12,  3, 12,  2,  9,  0,  8,  1, 15,  1, 10,  7, 10,  6, 12,  4, 11,  1, 14,  2, 11,  0, 14,  1,  9,  2, 12,  2,  8,  5, 13,  5,
     6, 10,  7, 11,  0,  9,  3,  9,  5, 11,  0,  8,  0, 13,  2, 13,  4, 13,  5,  9,  4,  8,  2,  8,  0, 10,  0, 14,  6,  9,  6,  9,  4, 14,  7,  9,  0, 11,  7, 10,  7, 11,  7, 14,  4, 14,  5, 15,  4,  8,  4, 11,  5, 14,  0,  8,  1, 14,  7, 14,  2, 11,  7, 13,
    12,  3, 13,  3, 14,  5, 15,  6, 12,  0, 15,  4, 11,  5,  9,  5,  9,  2, 14,  1, 12,  1, 13,  5, 15,  4, 11,  7, 12,  1, 13,  2,  8,  1, 12,  0, 15,  6, 14,  2, 14,  3, 10,  1, 11,  3, 11,  0, 12,  1, 15,  2, 11,  1, 12,  4,  9,  4, 10,  2, 13,  6, 10,  2,
     2,  8,  4,  8,  4,  9,  0,  9,  2, 15,  5,  9,  6, 11,  7, 14,  0,  9,  2, 12,  2,  8,  0, 10,  1,  9,  7,  8,  2,  9,  1, 11,  2, 11,  2,  8,  1,  9,  1, 11,  7, 13,  6, 11,  1, 11,  0,  9,  2, 13,  4, 14,  1, 15,  2,  8,  7, 15,  0, 13,  6,  9,  4, 13,
    13,  5, 13,  1, 14,  0, 12,  6,  9,  6, 12,  0, 13,  0, 11,  3, 13,  6, 11,  7, 12,  5, 14,  6, 14,  5, 15,  3, 13,  6, 14,  7, 15,  6, 15,  5, 13,  4, 14,  5,  8,  2, 14,  3, 14,  6, 14,  5, 10,  6, 10,  2,  9,  5, 15,  5, 11,  1,  8,  5, 13,  3, 11,  2,
     2,  8,  6,  8,  0,  9,  1, 15,  0, 11,  4, 15,  7,  8,  4,  8,  1, 13,  7, 11,  1, 13,  5, 15,  1, 15,  2,  9,  0, 13,  5, 12,  3, 12,  2,  8,  1, 10,  1, 13,  5, 15,  3,  9,  3,  9,  2, 12,  5, 14,  6, 13,  1,  9,  6,  9,  1,  8,  4, 15,  7, 10,  7, 15,
    14,  5, 14,  3, 12,  4, 10,  7, 12,  7,  8,  2, 12,  1, 13,  2, 11,  4, 13,  3,  8,  5,  8,  1, 10,  7, 12,  6,  9,  5,  8,  0,  8,  7, 14,  5, 13,  6,  8,  4,  8,  2, 12,  6, 13,  5,  8,  7,  8,  3, 10,  3, 13,  4, 12,  1, 13,  4, 11,  1, 14,  3,  8,  1,
     2, 13,  6, 10,  0,  9,  1, 15,  0, 12,  4, 11,  2,  9,  0,  9,  0, 13,  1,  8,  0, 11,  5,  9,  6, 13,  2, 15,  2, 12,  7, 12,  6, 15,  2, 14,  1, 13,  1, 14,  4, 11,  2, 14,  1,  9,  0, 15,  1, 12,  5, 14,  6, 13,  2, 15,  3,  9,  1, 15,  7,  8,  1, 14,
    11,  7, 13,  3, 15,  6,  8,  4,  8,  4, 14,  2, 14,  6, 13,  6, 11,  4, 14,  4, 15,  4, 14,  3, 10,  2, 10,  5,  9,  6,  9,  3,  9,  1, 11,  6,  9,  4,  8,  4, 15,  0,  8,  4, 13,  4,  9,  4,  9,  5,  9,  3, 10,  2,  9,  5, 12,  6,  8,  4, 12,  0, 11,  4,
     4, 10,  4, 13,  1, 12,  7,  8,  0, 13,  4, 12,  2, 13,  1, 14,  7, 15,  3,  8,  7, 12,  3, 10,  4,  9,  4, 11,  7,  8,  2, 11,  2, 10,  0, 12,  1, 14,  6, 14,  7, 13,  7, 11,  3, 10,  0, 10,  4,  9,  3, 13,  0, 13,  7,  8,  7,  9,  7, 11,  2,  8,  1, 14,
    15,  0, 11,  1,  9,  6, 15,  3, 10,  7,  9,  1,  9,  7, 10,  7, 10,  1, 12,  5,  8,  1, 15,  7, 13,  3, 12,  2, 13,  2, 14,  7, 14,  5, 10,  4,  8,  4, 11,  1, 10,  3, 14,  0, 14,  6, 14,  5, 13,  2,  8,  7, 10,  7, 13,  2, 14,  2, 13,  2, 14,  5, 11,  6,
     7, 11,  5, 13,  1, 11,  7,  8,  5, 14,  4, 13,  1, 14,  5, 10,  2, 15,  4, 13,  7, 11,  7, 10,  1,  8,  5,  9,  4, 12,  7, 10,  1, 13,  7, 13,  7, 11,  0, 14,  5, 14,  2, 14,  4, 12,  4, 13,  7, 13,  7,  8,  2, 14,  2, 13,  5,  9,  5,  9,  2,  8,  1, 10,
    12,  0,  8,  2, 13,  4, 12,  1, 11,  1, 10,  1, 10,  4, 12,  2, 11,  5,  8,  1, 15,  3, 14,  1, 13,  4, 14,  2,  9,  2, 12,  2,  8,  4,  8,  2, 12,  3, 10,  6, 11,  0, 11,  7, 10,  1,  9,  2, 10,  0, 13,  1, 11,  7, 10,  6, 13,  1, 12,  1, 12,  6, 14,  7,
     2, 14,  4, 15,  7, 14,  5,  8,  4, 13,  2, 10,  4, 13,  0, 13,  1, 11,  1, 12,  7, 12,  6, 13,  4, 13,  4, 13,  2, 12,  2, 13,  1, 13,  7, 10,  2, 11,  2, 10,  7, 15,  7, 11,  5, 13,  2,  8,  2, 14,  4,  9,  2, 14,  0,  8,  1,  8,  7, 10,  5, 15,  6, 14,
    11,  7, 10,  2, 11,  1, 13,  0, 11,  1, 15,  7,  8,  1,  9,  4, 13,  4,  8,  4,  8,  3, 10,  0,  9,  0,  9,  2,  9,  7,  9,  6, 10,  7, 13,  1, 13,  7, 14,  6, 11,  3, 12,  2,  8,  2, 12,  5, 11,  5, 12,  2, 10,  5, 14,  4, 12,  4, 13,  2,  9,  1, 10,  0,
     2, 12,  0, 13,  7, 13,  2, 12,  4,  8,  1, 12,  4, 13,  6, 11,  7, 13,  7, 13,  1,  9,  7, 10,  2, 10,  1,  9,  2, 10,  1, 11,  6,  9,  4, 13,  2, 10,  0, 10,  2, 14,  0, 13,  7, 10,  7, 10,  0, 12,  0,  9,  0, 13,  2, 13,  1,  9,  0, 15,  2, 14,  2, 13,
    11,  7,  8,  4, 10,  2, 10,  7, 12,  1, 11,  7,  8,  2, 13,  1, 10,  0, 10,  2, 13,  6, 14,  0, 14,  7, 13,  6, 14,  5, 13,  4, 15,  1, 10,  2, 13,  7, 13,  7,  9,  4,  9,  6, 13,  3, 13,  3,  8,  4, 13,  4, 10,  6, 10,  5, 12,  4, 10,  7, 11,  6,  9,  6,
     4, 14,  6, 11,  7, 13,  6, 10,  4,  8,  4, 11,  6,  8,  5, 13,  7, 14,  7, 14,  1,  9,  0, 12,  1,  9,  1, 12,  4, 14,  7, 10,  4, 13,  7, 13,  7, 11,  4, 10,  1, 11,  7, 13,  4, 12,  1, 10,  4, 12,  2,  8,  2, 12,  2, 10,  2, 12,  1, 12,  4, 12,  2, 12,
     8,  2, 13,  2,  8,  1, 13,  1, 12,  1, 12,  2, 12,  2, 11,  1,  9,  2, 11,  3, 12,  4,  8,  4, 12,  4, 10,  6, 10,  1, 13,  2, 11,  2, 10,  1, 12,  0, 14,  2, 14,  4,  9,  2,  8,  1, 13,  4,  8,  0, 12,  4, 11,  6, 12,  6, 11,  7, 10,  7, 11,  2, 10,  7
};

/*static const uint8_t dither_matrix_halftone8[4 + 8*8] = {
    8, 8, 64, 3,
    60, 53, 42, 26, 27, 43, 54, 61,
    52, 41, 25, 13, 14, 28, 44, 55,
    40, 24, 12,  5,  6, 15, 29, 45,
    39, 23,  4,  0,  1,  7, 16, 30,
    38, 22, 11,  3,  2,  8, 17, 31,
    51, 37, 21, 10,  9, 18, 32, 46,
    59, 50, 36, 20, 19, 33, 47, 56,
    63, 58, 49, 35, 34, 48, 57, 62
}; */

static const uint8_t dither_matrix_diagonal45_8[4 + 8*8] = {
    8, 8, 64, 2,
    16, 32, 48, 56, 40, 24,  8,  0,
    36, 52, 60, 44, 28, 12,  4, 20,
    49, 57, 41, 25,  9,  1, 17, 33,
    61, 45, 29, 13,  5, 21, 37, 53,
    42, 26, 10,  2, 18, 34, 50, 58,
    30, 14,  6, 22, 38, 54, 62, 46,
    11,  3, 19, 35, 51, 59, 43, 27,
     7, 23, 39, 55, 63, 47, 31, 15
};


typedef struct halftone_pixelinfo {
    int x;
    int y;
    double distance;
    double angle;
} halftone_pixelinfo;

static inline halftone_pixelinfo* halftone_pixel_make(int w, int h) {
    int x, y, k;
    halftone_pixelinfo* hp = Gif_NewArray(halftone_pixelinfo, w * h);
    for (y = k = 0; y != h; ++y)
        for (x = 0; x != w; ++x, ++k) {
            hp[k].x = x;
            hp[k].y = y;
            hp[k].distance = -1;
        }
    return hp;
}

static inline void halftone_pixel_combine(halftone_pixelinfo* hp,
                                          double cx, double cy) {
    double d = (hp->x - cx) * (hp->x - cx) + (hp->y - cy) * (hp->y - cy);
    if (hp->distance < 0 || d < hp->distance) {
        hp->distance = d;
        hp->angle = atan2(hp->y - cy, hp->x - cx);
    }
}

static inline int halftone_pixel_compare(const void* va, const void* vb) {
    const halftone_pixelinfo* a = (const halftone_pixelinfo*) va;
    const halftone_pixelinfo* b = (const halftone_pixelinfo*) vb;
    if (fabs(a->distance - b->distance) > 0.01)
        return a->distance < b->distance ? -1 : 1;
    else
        return a->angle < b->angle ? -1 : 1;
}

static uint8_t* halftone_pixel_matrix(halftone_pixelinfo* hp,
                                      int w, int h, int nc) {
    int i;
    uint8_t* m = Gif_NewArray(uint8_t, 4 + w * h);
    m[0] = w;
    m[1] = h;
    m[3] = nc;
    if (w * h > 255) {
        double s = 255. / (w * h);
        m[2] = 255;
        for (i = 0; i != w * h; ++i)
            m[4 + hp[i].x + hp[i].y*w] = (int) (i * s);
    } else {
        m[2] = w * h;
        for (i = 0; i != w * h; ++i)
            m[4 + hp[i].x + hp[i].y*w] = i;
    }
    Gif_DeleteArray(hp);
    return m;
}

static uint8_t* make_halftone_matrix_square(int w, int h, int nc) {
    halftone_pixelinfo* hp = halftone_pixel_make(w, h);
    int i;
    for (i = 0; i != w * h; ++i)
        halftone_pixel_combine(&hp[i], (w-1)/2.0, (h-1)/2.0);
    qsort(hp, w * h, sizeof(*hp), halftone_pixel_compare);
    return halftone_pixel_matrix(hp, w, h, nc);
}

static uint8_t* make_halftone_matrix_triangular(int w, int h, int nc) {
    halftone_pixelinfo* hp = halftone_pixel_make(w, h);
    int i;
    for (i = 0; i != w * h; ++i) {
        halftone_pixel_combine(&hp[i], (w-1)/2.0, (h-1)/2.0);
        halftone_pixel_combine(&hp[i], -0.5, -0.5);
        halftone_pixel_combine(&hp[i], w-0.5, -0.5);
        halftone_pixel_combine(&hp[i], -0.5, h-0.5);
        halftone_pixel_combine(&hp[i], w-0.5, h-0.5);
    }
    qsort(hp, w * h, sizeof(*hp), halftone_pixel_compare);
    return halftone_pixel_matrix(hp, w, h, nc);
}

int set_dither_type(Gt_OutputData* od, const char* name) {
    int parm[4], nparm = 0;
    const char* comma = strchr(name, ',');
    char buf[256];

    /* separate arguments from dither name */
    if (comma && (size_t) (comma - name) < sizeof(buf)) {
        memcpy(buf, name, comma - name);
        buf[comma - name] = 0;
        name = buf;
    }
    for (nparm = 0;
         comma && *comma && isdigit((unsigned char) comma[1]);
         ++nparm)
        parm[nparm] = strtol(&comma[1], (char**) &comma, 10);

    /* parse dither name */
    if (od->dither_type == dither_ordered_new)
        Gif_DeleteArray(od->dither_data);
    od->dither_type = dither_none;

    if (strcmp(name, "none") == 0 || strcmp(name, "posterize") == 0)
        /* ok */;
    else if (strcmp(name, "default") == 0)
        od->dither_type = dither_default;
    else if (strcmp(name, "floyd-steinberg") == 0
             || strcmp(name, "fs") == 0)
        od->dither_type = dither_floyd_steinberg;
    else if (strcmp(name, "atkinson") == 0
            || strcmp(name, "at") == 0)
        od->dither_type = dither_atkinson;
    else if (strcmp(name, "o3") == 0 || strcmp(name, "o3x3") == 0
             || (strcmp(name, "o") == 0 && nparm >= 1 && parm[0] == 3)) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_o3x3;
    } else if (strcmp(name, "o4") == 0 || strcmp(name, "o4x4") == 0
               || (strcmp(name, "o") == 0 && nparm >= 1 && parm[0] == 4)) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_o4x4;
    } else if (strcmp(name, "o8") == 0 || strcmp(name, "o8x8") == 0
               || (strcmp(name, "o") == 0 && nparm >= 1 && parm[0] == 8)) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_o8x8;
    } else if (strcmp(name, "ro64") == 0 || strcmp(name, "ro64x64") == 0
               || strcmp(name, "o") == 0 || strcmp(name, "ordered") == 0) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_ro64x64;
    } else if (strcmp(name, "diag45") == 0 || strcmp(name, "diagonal") == 0) {
        od->dither_type = dither_ordered;
        od->dither_data = dither_matrix_diagonal45_8;
    } else if (strcmp(name, "halftone") == 0 || strcmp(name, "half") == 0
               || strcmp(name, "trihalftone") == 0
               || strcmp(name, "trihalf") == 0) {
        int size = nparm >= 1 && parm[0] > 0 ? parm[0] : 6;
        int ncol = nparm >= 2 && parm[1] > 1 ? parm[1] : 2;
        od->dither_type = dither_ordered_new;
        od->dither_data = make_halftone_matrix_triangular(size, (int) (size * sqrt(3) + 0.5), ncol);
    } else if (strcmp(name, "sqhalftone") == 0 || strcmp(name, "sqhalf") == 0
               || strcmp(name, "squarehalftone") == 0) {
        int size = nparm >= 1 && parm[0] > 0 ? parm[0] : 6;
        int ncol = nparm >= 2 && parm[1] > 1 ? parm[1] : 2;
        od->dither_type = dither_ordered_new;
        od->dither_data = make_halftone_matrix_square(size, size, ncol);
    } else
        return -1;

    if (od->dither_type == dither_ordered
        && nparm >= 2 && parm[1] > 1 && parm[1] != od->dither_data[3]) {
        int size = od->dither_data[0] * od->dither_data[1];
        uint8_t* dd = Gif_NewArray(uint8_t, 4 + size);
        memcpy(dd, od->dither_data, 4 + size);
        dd[3] = parm[1];
        od->dither_data = dd;
        od->dither_type = dither_ordered_new;
    }
    return 0;
}
