/* xform.c - Image transformation functions for gifsicle.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include "kcolor.h"
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if HAVE_SYS_TYPES_H && HAVE_SYS_STAT_H
# include <sys/types.h>
# include <sys/stat.h>
#endif
#ifndef M_PI
/* -std=c89 does not define M_PI */
# define M_PI           3.14159265358979323846
#endif


/******
 * color transforms
 **/

Gt_ColorTransform *
append_color_transform(Gt_ColorTransform *list,
                       color_transform_func func, void *data)
{
  Gt_ColorTransform *trav;
  Gt_ColorTransform *xform = Gif_New(Gt_ColorTransform);
  xform->next = 0;
  xform->func = func;
  xform->data = data;

  for (trav = list; trav && trav->next; trav = trav->next)
    ;
  if (trav) {
    trav->next = xform;
    return list;
  } else
    return xform;
}

Gt_ColorTransform *
delete_color_transforms(Gt_ColorTransform *list, color_transform_func func)
{
  Gt_ColorTransform *prev = 0, *trav = list;
  while (trav) {
    Gt_ColorTransform *next = trav->next;
    if (trav->func == func) {
      if (prev) prev->next = next;
      else list = next;
      Gif_Delete(trav);
    } else
      prev = trav;
    trav = next;
  }
  return list;
}

void
apply_color_transforms(Gt_ColorTransform *list, Gif_Stream *gfs)
{
  int i;
  Gt_ColorTransform *xform;
  for (xform = list; xform; xform = xform->next) {
    if (gfs->global)
      xform->func(gfs->global, xform->data);
    for (i = 0; i < gfs->nimages; i++)
      if (gfs->images[i]->local)
        xform->func(gfs->images[i]->local, xform->data);
  }
}


typedef struct Gt_ColorChange {
  struct Gt_ColorChange *next;
  Gif_Color old_color;
  Gif_Color new_color;
} Gt_ColorChange;

void
color_change_transformer(Gif_Colormap *gfcm, void *thunk)
{
  int i, have;
  Gt_ColorChange *first_change = (Gt_ColorChange *)thunk;
  Gt_ColorChange *change;

  /* change colors named by color */
  for (i = 0; i < gfcm->ncol; i++)
    for (change = first_change; change; change = change->next) {
      if (!change->old_color.haspixel)
        have = GIF_COLOREQ(&gfcm->col[i], &change->old_color);
      else
        have = (change->old_color.pixel == (uint32_t)i);

      if (have) {
        gfcm->col[i] = change->new_color;
        break;                  /* ignore remaining color changes */
      }
    }
}

Gt_ColorTransform *
append_color_change(Gt_ColorTransform *list,
                    Gif_Color old_color, Gif_Color new_color)
{
  Gt_ColorTransform *xform;
  Gt_ColorChange *change = Gif_New(Gt_ColorChange);
  change->old_color = old_color;
  change->new_color = new_color;
  change->next = 0;

  for (xform = list; xform && xform->next; xform = xform->next)
    ;
  if (!xform || xform->func != &color_change_transformer)
    return append_color_transform(list, &color_change_transformer, change);
  else {
    Gt_ColorChange *prev = (Gt_ColorChange *)(xform->data);
    while (prev->next) prev = prev->next;
    prev->next = change;
    return list;
  }
}


void
pipe_color_transformer(Gif_Colormap *gfcm, void *thunk)
{
  int i, status;
  FILE *f;
  Gif_Color *col = gfcm->col;
  Gif_Colormap *new_cm = 0;
  char *command = (char *)thunk;
#ifdef HAVE_MKSTEMP
# ifdef P_tmpdir
  char tmp_file[] = P_tmpdir "/gifsicle.XXXXXX";
# else
  char tmp_file[] = "/tmp/gifsicle.XXXXXX";
# endif
#else
  char *tmp_file = tmpnam(0);
#endif
  char *new_command;

#ifdef HAVE_MKSTEMP
  {
    mode_t old_mode = umask(077);
    if (mkstemp(tmp_file) < 0)
      fatal_error("can%,t create temporary file!");
    umask(old_mode);
  }
#else
  if (!tmp_file)
    fatal_error("can%,t create temporary file!");
#endif

  new_command = Gif_NewArray(char, strlen(command) + strlen(tmp_file) + 4);
  sprintf(new_command, "%s  >%s", command, tmp_file);
  f = popen(new_command, "w");
  if (!f)
    fatal_error("can%,t run color transformation command: %s", strerror(errno));
  Gif_DeleteArray(new_command);

  for (i = 0; i < gfcm->ncol; i++)
    fprintf(f, "%d %d %d\n", col[i].gfc_red, col[i].gfc_green, col[i].gfc_blue);

  errno = 0;
  status = pclose(f);
  if (status < 0) {
      error(1, "color transformation error: %s", strerror(errno));
      goto done;
  } else if (status > 0) {
    error(1, "color transformation command failed");
    goto done;
  }

  f = fopen(tmp_file, "r");
  if (!f || feof(f)) {
    error(1, "color transformation command generated no output", command);
    if (f) fclose(f);
    goto done;
  }
  new_cm = read_colormap_file("<color transformation>", f);
  fclose(f);

  if (new_cm) {
    int nc = new_cm->ncol;
    if (nc < gfcm->ncol) {
      nc = gfcm->ncol;
      warning(1, "too few colors in color transformation results");
    } else if (nc > gfcm->ncol)
      warning(1, "too many colors in color transformation results");
    for (i = 0; i < nc; i++)
      col[i] = new_cm->col[i];
  }

 done:
  remove(tmp_file);
  Gif_DeleteColormap(new_cm);
}



/*****
 * crop image; returns true if the image exists
 **/

void
combine_crop(Gt_Crop* dstcrop, const Gt_Crop* srccrop, const Gif_Image* gfi)
{
    int cl = srccrop->x - gfi->left, cr = cl + srccrop->w,
        ct = srccrop->y - gfi->top, cb = ct + srccrop->h,
        dl = cl > 0 ? cl : 0, dr = cr < gfi->width ? cr : gfi->width,
        dt = ct > 0 ? ct : 0, db = cb < gfi->height ? cb : gfi->height;

    if (dl < dr) {
        dstcrop->x = dl;
        dstcrop->w = dr - dl;
    } else {
        dstcrop->x = (cl <= 0 ? 0 : srccrop->w - 1) + (srccrop->left_offset - gfi->left);
        dstcrop->w = 0;
    }
    if (dt < db) {
        dstcrop->y = dt;
        dstcrop->h = db - dt;
    } else {
        dstcrop->y = (ct <= 0 ? 0 : srccrop->h - 1) + (srccrop->top_offset - gfi->top);
        dstcrop->h = 0;
    }
}

int
crop_image(Gif_Image* gfi, Gt_Frame* fr, int preserve_total_crop)
{
    Gt_Crop c;
    int j;

    combine_crop(&c, fr->crop, gfi);

    fr->left_offset = fr->crop->left_offset;
    fr->top_offset = fr->crop->top_offset;

    if (c.w > 0 && c.h > 0 && gfi->img) {
        uint8_t** old_img = gfi->img;
        gfi->img = Gif_NewArray(uint8_t *, c.h + 1);
        for (j = 0; j < c.h; j++)
            gfi->img[j] = old_img[c.y + j] + c.x;
        gfi->img[c.h] = 0;
        Gif_DeleteArray(old_img);
        gfi->width = c.w;
        gfi->height = c.h;
    } else if (preserve_total_crop)
        Gif_MakeImageEmpty(gfi);
    else {
        Gif_DeleteArray(gfi->img);
        gfi->img = 0;
        gfi->width = gfi->height = 0;
    }

    gfi->left += c.x - fr->left_offset;
    gfi->top += c.y - fr->top_offset;
    return gfi->img != 0;
}


/*****
 * flip and rotate
 **/

void
flip_image(Gif_Image* gfi, Gt_Frame *fr, int is_vert)
{
  int x, y;
  int width = gfi->width;
  int height = gfi->height;
  uint8_t **img = gfi->img;

  /* horizontal flips */
  if (!is_vert) {
    uint8_t *buffer = Gif_NewArray(uint8_t, width);
    uint8_t *trav;
    for (y = 0; y < height; y++) {
      memcpy(buffer, img[y], width);
      trav = img[y] + width - 1;
      for (x = 0; x < width; x++)
        *trav-- = buffer[x];
    }
    gfi->left = fr->stream->screen_width - (gfi->left + width);
    if (fr->crop)
        fr->left_offset = fr->stream->screen_width - (fr->left_offset + fr->crop->w);
    Gif_DeleteArray(buffer);
  }

  /* vertical flips */
  if (is_vert) {
    uint8_t **buffer = Gif_NewArray(uint8_t *, height);
    memcpy(buffer, img, height * sizeof(uint8_t *));
    for (y = 0; y < height; y++)
      img[y] = buffer[height - y - 1];
    gfi->top = fr->stream->screen_height - (gfi->top + height);
    if (fr->crop)
        fr->top_offset = fr->stream->screen_height - (fr->top_offset + fr->crop->h);
    Gif_DeleteArray(buffer);
  }
}

void
rotate_image(Gif_Image* gfi, Gt_Frame* fr, int rotation)
{
  int x, y;
  int width = gfi->width;
  int height = gfi->height;
  uint8_t **img = gfi->img;
  uint8_t *new_data = Gif_NewArray(uint8_t, (unsigned) width * height);
  uint8_t *trav = new_data;

  /* this function can only rotate by 90 or 270 degrees */
  assert(rotation == 1 || rotation == 3);

  if (rotation == 1) {
    for (x = 0; x < width; x++)
      for (y = height - 1; y >= 0; y--)
        *trav++ = img[y][x];
    x = gfi->left;
    gfi->left = fr->stream->screen_height - (gfi->top + height);
    gfi->top = x;
    if (fr->crop) {
        x = fr->left_offset;
        fr->left_offset = fr->stream->screen_height - (fr->top_offset + fr->crop->h);
        fr->top_offset = x;
    }

  } else {
    for (x = width - 1; x >= 0; x--)
      for (y = 0; y < height; y++)
        *trav++ = img[y][x];
    y = gfi->top;
    gfi->top = fr->stream->screen_width - (gfi->left + width);
    gfi->left = y;
    if (fr->crop) {
        y = fr->top_offset;
        fr->top_offset = fr->stream->screen_width - (fr->left_offset + fr->crop->w);
        fr->left_offset = y;
    }
  }

  Gif_ReleaseUncompressedImage(gfi);
  gfi->width = height;
  gfi->height = width;
  Gif_SetUncompressedImage(gfi, new_data, Gif_Free, 0);
}


/*****
 * scale
 **/

/* kcscreen: A frame buffer containing `kacolor`s (one per pixel).
   Kcolors with components < 0 represent transparency. */
typedef struct {
    kacolor* data;      /* data buffer: (x,y) in data[y*width + x] */
    kacolor* scratch;   /* scratch buffer, used for previous disposal */
    unsigned width;     /* screen width */
    unsigned height;    /* screen height */
    kacolor bg;         /* background color */
} kcscreen;

/* initialize `kcs` to an empty state */
static void kcscreen_clear(kcscreen* kcs) {
    kcs->data = kcs->scratch = NULL;
}

/* initialize `kcs` for stream `gfs`. Screen size is `sw`x`sh`; if either
   component is <= 0, the value of `gfs->screen_*` is taken. */
static void kcscreen_init(kcscreen* kcs, Gif_Stream* gfs, int sw, int sh) {
    unsigned i, sz;
    assert(!kcs->data && !kcs->scratch);
    kcs->width = sw <= 0 ? gfs->screen_width : sw;
    kcs->height = sh <= 0 ? gfs->screen_height : sh;
    sz = (unsigned) kcs->width * kcs->height;
    kcs->data = Gif_NewArray(kacolor, sz);
    if ((gfs->nimages == 0 || gfs->images[0]->transparent < 0)
        && gfs->global && gfs->background < gfs->global->ncol) {
        kcs->bg.k = kc_makegfcg(&gfs->global->col[gfs->background]);
        kcs->bg.a[3] = KC_MAX;
    } else
        kcs->bg.a[0] = kcs->bg.a[1] = kcs->bg.a[2] = kcs->bg.a[3] = 0;
    for (i = 0; i != sz; ++i)
        kcs->data[i] = kcs->bg;
}

/* free memory associated with `kcs` */
static void kcscreen_cleanup(kcscreen* kcs) {
    Gif_DeleteArray(kcs->data);
    Gif_DeleteArray(kcs->scratch);
}

/* apply image `gfi` to screen `kcs`, using the color array `ks` */
static void kcscreen_apply(kcscreen* kcs, const Gif_Image* gfi,
                           const kcolor* ks) {
    int x, y;
    assert((unsigned) gfi->left + gfi->width <= kcs->width);
    assert((unsigned) gfi->top + gfi->height <= kcs->height);

    if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
        if (!kcs->scratch)
            kcs->scratch = Gif_NewArray(kacolor, kcs->width * kcs->height);
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            memcpy(&kcs->scratch[y * kcs->width + gfi->left],
                   &kcs->data[y * kcs->width + gfi->left],
                   sizeof(kacolor) * gfi->width);
    }

    for (y = gfi->top; y != gfi->top + gfi->height; ++y) {
        const uint8_t* linein = gfi->img[y - gfi->top];
        kacolor* lineout = &kcs->data[y * kcs->width + gfi->left];
        for (x = 0; x != gfi->width; ++x)
            if (linein[x] != gfi->transparent) {
                lineout[x].k = ks[linein[x]];
                lineout[x].a[3] = KC_MAX;
            }
    }
}

/* dispose of image `gfi`, which was previously applied by kcscreen_apply */
static void kcscreen_dispose(kcscreen* kcs, const Gif_Image* gfi) {
    int x, y;
    assert((unsigned) gfi->left + gfi->width <= kcs->width);
    assert((unsigned) gfi->top + gfi->height <= kcs->height);

    if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            memcpy(&kcs->data[y * kcs->width + gfi->left],
                   &kcs->scratch[y * kcs->width + gfi->left],
                   sizeof(kacolor) * gfi->width);
    } else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND) {
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            for (x = gfi->left; x != gfi->left + gfi->width; ++x)
                kcs->data[y * kcs->width + x] = kcs->bg;
    }
}


/* ksscreen: A frame buffer containing `scale_color`s (one per pixel).
   Kcolors with components < 0 represent transparency. */
typedef struct {
    scale_color* data;      /* data buffer: (x,y) in data[y*width + x] */
    scale_color* scratch;   /* scratch buffer, used for previous disposal */
    unsigned width;         /* screen width */
    unsigned height;        /* screen height */
    scale_color bg;         /* background color */
} ksscreen;

/* initialize `kss` to an empty state */
static void ksscreen_clear(ksscreen* kss) {
    kss->data = kss->scratch = NULL;
}

/* initialize `kss` for stream `gfs`. Screen size is `sw`x`sh`; if either
   component is <= 0, the value of `gfs->screen_*` is taken. */
static void ksscreen_init(ksscreen* kss, Gif_Stream* gfs, int sw, int sh) {
    unsigned i, sz;
    assert(!kss->data && !kss->scratch);
    kss->width = sw <= 0 ? gfs->screen_width : sw;
    kss->height = sh <= 0 ? gfs->screen_height : sh;
    sz = (unsigned) kss->width * kss->height;
    kss->data = Gif_NewArray(scale_color, sz);
    if ((gfs->nimages == 0 || gfs->images[0]->transparent < 0)
        && gfs->global && gfs->background < gfs->global->ncol) {
        kcolor k = kc_makegfcg(&gfs->global->col[gfs->background]);
        kss->bg = sc_makekc(&k);
    } else
        sc_clear(&kss->bg);
    for (i = 0; i != sz; ++i)
        kss->data[i] = kss->bg;
}

/* free memory associated with `kss` */
static void ksscreen_cleanup(ksscreen* kss) {
    Gif_DeleteArray(kss->data);
    Gif_DeleteArray(kss->scratch);
}

/* apply image `gfi` to screen `kss`, using the color array `ks` */
static void ksscreen_apply(ksscreen* kss, const Gif_Image* gfi,
                           const kcolor* ks) {
    int x, y;
    assert((unsigned) gfi->left + gfi->width <= kss->width);
    assert((unsigned) gfi->top + gfi->height <= kss->height);

    if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
        if (!kss->scratch)
            kss->scratch = Gif_NewArray(scale_color, kss->width * kss->height);
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            memcpy(&kss->scratch[y * kss->width + gfi->left],
                   &kss->data[y * kss->width + gfi->left],
                   sizeof(scale_color) * gfi->width);
    }

    for (y = gfi->top; y != gfi->top + gfi->height; ++y) {
        const uint8_t* linein = gfi->img[y - gfi->top];
        scale_color* lineout = &kss->data[y * kss->width + gfi->left];
        for (x = 0; x != gfi->width; ++x)
            if (linein[x] != gfi->transparent)
                lineout[x] = sc_makekc(&ks[linein[x]]);
    }
}

/* dispose of image `gfi`, which was previously applied by ksscreen_apply */
static void ksscreen_dispose(ksscreen* kss, const Gif_Image* gfi) {
    int x, y;
    assert((unsigned) gfi->left + gfi->width <= kss->width);
    assert((unsigned) gfi->top + gfi->height <= kss->height);

    if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            memcpy(&kss->data[y * kss->width + gfi->left],
                   &kss->scratch[y * kss->width + gfi->left],
                   sizeof(scale_color) * gfi->width);
    } else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND) {
        for (y = gfi->top; y != gfi->top + gfi->height; ++y)
            for (x = gfi->left; x != gfi->left + gfi->width; ++x)
                kss->data[y * kss->width + x] = kss->bg;
    }
}


typedef struct {
    float w;
    int ipos;
    int opos;
} scale_weight;

typedef struct {
    scale_weight* ws;
    int n;
} scale_weightset;

typedef struct {
    Gif_Stream* gfs;
    Gif_Image* gfi;
    int imageno;
    kd3_tree* kd3;
    ksscreen iscr;
    kcscreen oscr;
    kcscreen xscr;
    double oxf;                 /* (input width) / (output width) */
    double oyf;                 /* (input height) / (output height) */
    double ixf;                 /* (output width) / (input width) */
    double iyf;                 /* (output height) / (input height) */
    scale_weightset xweights;
    scale_weightset yweights;
    kd3_tree global_kd3;
    kd3_tree local_kd3;
    unsigned max_desired_dist;
    int scale_colors;
} scale_context;

#if ENABLE_THREADS
static pthread_mutex_t global_colormap_lock = PTHREAD_MUTEX_INITIALIZER;
#define GLOBAL_COLORMAP_LOCK() pthread_mutex_lock(&global_colormap_lock)
#define GLOBAL_COLORMAP_UNLOCK() pthread_mutex_unlock(&global_colormap_lock)
#else
#define GLOBAL_COLORMAP_LOCK() /* nada */
#define GLOBAL_COLORMAP_UNLOCK() /* nada */
#endif

static void sctx_init(scale_context* sctx, Gif_Stream* gfs, int nw, int nh) {
    sctx->gfs = gfs;
    sctx->gfi = NULL;
    sctx->global_kd3.ks = NULL;
    sctx->local_kd3.ks = NULL;
    ksscreen_clear(&sctx->iscr);
    kcscreen_clear(&sctx->oscr);
    kcscreen_clear(&sctx->xscr);
    sctx->iscr.width = gfs->screen_width;
    sctx->iscr.height = gfs->screen_height;
    sctx->oscr.width = nw;
    sctx->oscr.height = nh;
    sctx->xscr.width = nw;
    sctx->xscr.height = nh;
    sctx->ixf = (double) nw / gfs->screen_width;
    sctx->iyf = (double) nh / gfs->screen_height;
    sctx->oxf = gfs->screen_width / (double) nw;
    sctx->oyf = gfs->screen_height / (double) nh;
    sctx->xweights.ws = sctx->yweights.ws = NULL;
    sctx->xweights.n = sctx->yweights.n = 0;
    sctx->max_desired_dist = 16000;
    sctx->scale_colors = 0;
    sctx->kd3 = NULL;
    sctx->imageno = 0;
}

static void sctx_cleanup(scale_context* sctx) {
    if (sctx->global_kd3.ks)
        kd3_cleanup(&sctx->global_kd3);
    ksscreen_cleanup(&sctx->iscr);
    kcscreen_cleanup(&sctx->oscr);
    kcscreen_cleanup(&sctx->xscr);
    Gif_DeleteArray(sctx->xweights.ws);
    Gif_DeleteArray(sctx->yweights.ws);
}

static void scale_image_data_point(scale_context* sctx, Gif_Image* gfo) {
    Gif_Image* gfi = sctx->gfi;
    uint8_t* data = gfo->image_data;
    int xo, yo;
    uint16_t* xoff = Gif_NewArray(uint16_t, gfo->width);
    for (xo = 0; xo != gfo->width; ++xo)
        xoff[xo] = (int) ((gfo->left + xo + 0.5) * sctx->oxf) - gfi->left;

    for (yo = 0; yo != gfo->height; ++yo) {
        int yi = (int) ((gfo->top + yo + 0.5) * sctx->oyf) - gfi->top;
        const uint8_t* in_line = gfi->img[yi];
        for (xo = 0; xo != gfo->width; ++xo, ++data)
            *data = in_line[xoff[xo]];
    }

    Gif_DeleteArray(xoff);
}

static void scale_image_update_global_kd3(scale_context* sctx) {
    Gif_Colormap* gfcm = sctx->gfs->global;
    assert(sctx->kd3 == &sctx->global_kd3);
    while (sctx->kd3->nitems < gfcm->ncol) {
        Gif_Color* gfc = &gfcm->col[sctx->kd3->nitems];
        kd3_add8g(sctx->kd3, gfc->gfc_red, gfc->gfc_green, gfc->gfc_blue);
    }
}

static void scale_image_prepare(scale_context* sctx) {
    if (sctx->gfi->local) {
        sctx->kd3 = &sctx->local_kd3;
        kd3_init_build(sctx->kd3, NULL, sctx->gfi->local);
    } else {
        sctx->kd3 = &sctx->global_kd3;
        if (!sctx->kd3->ks)
            kd3_init(sctx->kd3, NULL);
        GLOBAL_COLORMAP_LOCK();
        scale_image_update_global_kd3(sctx);
        GLOBAL_COLORMAP_UNLOCK();
        if (!sctx->kd3->tree)
            kd3_build(sctx->kd3);
        kd3_enable_all(sctx->kd3);
    }
    if (sctx->gfi->transparent >= 0
        && sctx->gfi->transparent < sctx->kd3->nitems)
        kd3_disable(sctx->kd3, sctx->gfi->transparent);

    /* apply image to sctx->iscr */
    if (!sctx->iscr.data) {
        /* first image, set up screens */
        ksscreen_init(&sctx->iscr, sctx->gfs, 0, 0);
        kcscreen_init(&sctx->oscr, sctx->gfs,
                      sctx->oscr.width, sctx->oscr.height);
        kcscreen_init(&sctx->xscr, sctx->gfs,
                      sctx->xscr.width, sctx->xscr.height);
    }
    ksscreen_apply(&sctx->iscr, sctx->gfi, sctx->kd3->ks);
}

static void scale_image_output_row(scale_context* sctx, scale_color* sc,
                                   Gif_Image* gfo, int yo) {
    int k, xo;
    kacolor* oscr = &sctx->xscr.data[sctx->xscr.width * (yo + gfo->top)
                                     + gfo->left];

    for (xo = 0; xo != gfo->width; ++xo)
        if (sc[xo].a[3] <= (int) (KC_MAX / 4))
            oscr[xo] = kac_transparent();
        else {
            /* don't effectively mix partially transparent pixels with black */
            if (sc[xo].a[3] <= (int) (KC_MAX * 31 / 32))
                SCVEC_MULF(sc[xo], KC_MAX / sc[xo].a[3]);
            /* find closest color */
            for (k = 0; k != 3; ++k) {
                int v = (int) (sc[xo].a[k] + 0.5);
                oscr[xo].a[k] = KC_CLAMPV(v);
            }
            oscr[xo].a[3] = KC_MAX;
        }
}

static int scale_image_add_colors(scale_context* sctx, Gif_Image* gfo) {
    kchist kch;
    kcdiversity div;
    Gif_Color gfc;
    Gif_Colormap* gfcm = sctx->gfi->local ? sctx->gfi->local : sctx->gfs->global;
    int yo, xo, i, nadded;

    kchist_init(&kch);
    for (yo = 0; yo != gfo->height; ++yo) {
        kacolor* xscr = &sctx->xscr.data[sctx->xscr.width * (yo + gfo->top)
                                         + gfo->left];
        for (xo = 0; xo != gfo->width; ++xo)
            if (xscr[xo].a[3])
                kchist_add(&kch, xscr[xo].k, 1);
    }
    for (i = 0; i != gfcm->ncol; ++i)
        kchist_add(&kch, kc_makegfcg(&gfcm->col[i]), (kchist_count_t) -1);
    kchist_compress(&kch);

    kcdiversity_init(&div, &kch, 0);
    for (i = 0; i != kch.n && i != gfcm->ncol
             && kch.h[i].count == (kchist_count_t) -1; ++i)
        kcdiversity_choose(&div, i, 0);

    nadded = 0;
    while (gfcm->ncol < sctx->scale_colors) {
        int chosen = kcdiversity_find_diverse(&div, 0);
        if (chosen >= kch.n || div.min_dist[chosen] <= sctx->max_desired_dist)
            break;
        kcdiversity_choose(&div, chosen, 0);
        gfc = kc_togfcg(&kch.h[chosen].ka.k);
        Gif_AddColor(gfcm, &gfc, gfcm->ncol);
        kd3_add8g(sctx->kd3, gfc.gfc_red, gfc.gfc_green, gfc.gfc_blue);
        ++nadded;
    }

    kcdiversity_cleanup(&div);
    kchist_cleanup(&kch);
    return nadded != 0;
}

static void scale_image_complete(scale_context* sctx, Gif_Image* gfo) {
    int yo, xo, transparent = sctx->gfi->transparent;
    unsigned dist, dist2, max_dist;

 retry:
    max_dist = 0;
    /* look up palette for output screen */
    for (yo = 0; yo != gfo->height; ++yo) {
        uint8_t* data = gfo->img[yo];
        kacolor* xscr = &sctx->xscr.data[sctx->xscr.width * (yo + gfo->top)
                                         + gfo->left];
        kacolor* oscr = &sctx->oscr.data[sctx->oscr.width * (yo + gfo->top)
                                         + gfo->left];
        for (xo = 0; xo != gfo->width; ++xo)
            if (xscr[xo].a[3]) {
                data[xo] = kd3_closest_transformed(sctx->kd3, &xscr[xo].k, &dist);
                /* maybe previous color is actually closer to desired */
                if (transparent >= 0 && oscr[xo].a[3]) {
                    dist2 = kc_distance(&oscr[xo].k, &xscr[xo].k);
                    if (dist2 <= dist) {
                        data[xo] = transparent;
                        dist = dist2;
                    }
                }
                max_dist = dist > max_dist ? dist : max_dist;
            } else
                data[xo] = transparent;
    }

    /* if the global colormap has changed, we must retry
       (this can only happen if ENABLE_THREADS) */
    if (!sctx->gfi->local) {
        Gif_Colormap* gfcm = sctx->gfs->global;
        GLOBAL_COLORMAP_LOCK();
        if (gfcm->ncol > sctx->kd3->nitems) {
            scale_image_update_global_kd3(sctx);
            GLOBAL_COLORMAP_UNLOCK();
            goto retry;
        }
    }

    /* maybe add some colors */
    if (max_dist > sctx->max_desired_dist) {
        Gif_Colormap* gfcm = sctx->gfi->local ? sctx->gfi->local : sctx->gfs->global;
        if (gfcm->ncol < sctx->scale_colors
            && scale_image_add_colors(sctx, gfo)) {
            if (!sctx->gfi->local)
                GLOBAL_COLORMAP_UNLOCK();
            goto retry;
        }
    }

    if (!sctx->gfi->local)
        GLOBAL_COLORMAP_UNLOCK();

    /* apply disposal to sctx->iscr and sctx->oscr */
    if (sctx->imageno != sctx->gfs->nimages - 1) {
        ksscreen_dispose(&sctx->iscr, sctx->gfi);

        if (sctx->gfi->disposal == GIF_DISPOSAL_BACKGROUND)
            kcscreen_dispose(&sctx->oscr, gfo);
        else if (sctx->gfi->disposal != GIF_DISPOSAL_PREVIOUS)
            kcscreen_apply(&sctx->oscr, gfo, sctx->kd3->ks);
    }

    if (sctx->gfi->local)
        kd3_cleanup(sctx->kd3);
}

typedef struct pixel_range {
    int lo;
    int hi;
} pixel_range;

typedef struct pixel_range2 {
    float bounds[2];
    int lo;
    int hi;
} pixel_range2;

static inline pixel_range make_pixel_range(int xi, int maxi,
                                           int maxo, float f) {
    pixel_range pr;
    pr.lo = (int) (xi * f);
    pr.hi = (int) ((xi + 1) * f);
    pr.hi = (xi + 1 == maxi ? maxo : pr.hi);
    pr.hi = (pr.hi == pr.lo ? pr.hi + 1 : pr.hi);
    return pr;
}

static inline pixel_range2 make_pixel_range2(int xi, int maxi,
                                             int maxo, float f) {
    pixel_range2 pr;
    pr.bounds[0] = xi * f;
    pr.bounds[1] = (xi + 1) * f;
    pr.lo = (int) pr.bounds[0];
    if (xi + 1 == maxi)
        pr.hi = maxo;
    else
        pr.hi = (int) ceil(pr.bounds[1]);
    if (pr.hi == pr.lo)
        ++pr.hi;
    return pr;
}

static void scale_image_data_box(scale_context* sctx, Gif_Image* gfo) {
    uint16_t* xoff = Gif_NewArray(uint16_t, sctx->iscr.width);
    scale_color* sc = Gif_NewArray(scale_color, gfo->width);
    int* nsc = Gif_NewArray(int, gfo->width);
    int xi0, xi1, xo, yo, i, j;
    scale_image_prepare(sctx);

    /* develop scale mapping of input -> output pixels */
    for (xo = 0; xo != gfo->width; ++xo) {
        pixel_range xpr = make_pixel_range(xo + gfo->left, sctx->oscr.width,
                                           sctx->iscr.width, sctx->oxf);
        for (i = xpr.lo; i != xpr.hi; ++i)
            xoff[i] = xo;
    }
    xi0 = (int) (gfo->left * sctx->oxf);
    xi1 = make_pixel_range(gfo->left + gfo->width - 1, sctx->oscr.width,
                           sctx->iscr.width, sctx->oxf).hi;

    /* walk over output image */
    for (yo = 0; yo != gfo->height; ++yo) {
        pixel_range ypr = make_pixel_range(yo + gfo->top, sctx->oscr.height,
                                           sctx->iscr.height, sctx->oyf);
        for (xo = 0; xo != gfo->width; ++xo)
            sc_clear(&sc[xo]);
        for (xo = 0; xo != gfo->width; ++xo)
            nsc[xo] = 0;

        /* collect input mixes */
        for (j = ypr.lo; j != ypr.hi; ++j) {
            const scale_color* indata = &sctx->iscr.data[sctx->iscr.width*j];
            for (i = xi0; i != xi1; ++i) {
                ++nsc[xoff[i]];
                SCVEC_ADDV(sc[xoff[i]], indata[i]);
            }
        }

        /* scale channels */
        for (xo = 0; xo != gfo->width; ++xo)
            SCVEC_DIVF(sc[xo], (float) nsc[xo]);

        /* generate output */
        scale_image_output_row(sctx, sc, gfo, yo);
    }

    scale_image_complete(sctx, gfo);

    Gif_DeleteArray(xoff);
    Gif_DeleteArray(sc);
    Gif_DeleteArray(nsc);
}

static inline float mix_factor(int val, const float bounds[2]) {
    if (val < bounds[0] && bounds[1] < val + 1)
        return bounds[1] - bounds[0];
    else if (val < bounds[0])
        return 1 - (bounds[0] - val);
    else if (bounds[1] < val + 1)
        return bounds[1] - val;
    else
        return 1.0;
}

typedef struct scale_mixinfo {
    uint16_t xi;
    uint16_t xo;
    float f;
} scale_mixinfo;

static void scale_image_data_mix(scale_context* sctx, Gif_Image* gfo) {
    int xo, yo, i, j, nmix = 0, mixcap = sctx->iscr.width * 2;
    scale_color* sc = Gif_NewArray(scale_color, gfo->width);
    scale_mixinfo* mix = Gif_NewArray(scale_mixinfo, mixcap);

    scale_image_prepare(sctx);

    /* construct mixinfo */
    for (xo = 0; xo != gfo->width; ++xo) {
        pixel_range2 xpr = make_pixel_range2(xo + gfo->left, sctx->oscr.width,
                                             sctx->iscr.width, sctx->oxf);
        while (nmix + xpr.hi - xpr.lo > mixcap) {
            mixcap *= 2;
            Gif_ReArray(mix, scale_mixinfo, mixcap);
        }
        for (i = xpr.lo; i < xpr.hi; ++i, ++nmix) {
            mix[nmix].xi = i;
            mix[nmix].xo = xo;
            mix[nmix].f = mix_factor(i, xpr.bounds);
        }
    }

    for (yo = 0; yo != gfo->height; ++yo) {
        pixel_range2 ypr = make_pixel_range2(yo + gfo->top, sctx->oscr.height,
                                             sctx->iscr.height, sctx->oyf);

        for (xo = 0; xo != gfo->width; ++xo)
            sc_clear(&sc[xo]);

        /* collect input mixes */
        for (j = ypr.lo; j < ypr.hi; ++j) {
            float jf = mix_factor(j, ypr.bounds) * sctx->ixf * sctx->iyf;
            const scale_color* indata = &sctx->iscr.data[sctx->iscr.width*j];

            for (i = 0; i != nmix; ++i)
                SCVEC_ADDVxF(sc[mix[i].xo], indata[mix[i].xi],
                             (float) (mix[i].f * jf));
        }

        /* generate output */
        scale_image_output_row(sctx, sc, gfo, yo);
    }

    scale_image_complete(sctx, gfo);

    Gif_DeleteArray(sc);
    Gif_DeleteArray(mix);
}


static void scale_weightset_add(scale_weightset* wset,
                                int ipos, int opos, double w) {
    if (wset->n && wset->ws[wset->n - 1].ipos == ipos
        && wset->ws[wset->n - 1].opos == opos)
        wset->ws[wset->n - 1].w += w;
    else {
        if (!wset->ws)
            wset->ws = Gif_NewArray(scale_weight, 256);
        else if (wset->n > 128 && (wset->n & (wset->n - 1)) == 0)
            Gif_ReArray(wset->ws, scale_weight, wset->n * 2);
        wset->ws[wset->n].w = w;
        wset->ws[wset->n].ipos = ipos;
        wset->ws[wset->n].opos = opos;
        ++wset->n;
    }
}

static inline double scale_weight_cubic(double x, double b, double c) {
    x = fabs(x);
    if (x < 1)
        return ((12 - 9*b - 6*c) * x*x*x
                + (-18 + 12*b + 6*c) * x*x
                + (6 - 2*b)) / 6;
    else if (x < 2)
        return ((-b - 6*c) * x*x*x
                + (6*b + 30*c) * x*x
                + (-12*b - 48*c) * x
                + (8*b + 24*c)) / 6;
    else
        return 0;
}

static inline double scale_weight_catrom(double x) {
    return scale_weight_cubic(x, 0, 0.5);
}

static inline double scale_weight_mitchell(double x) {
    return scale_weight_cubic(x, 1.0/3, 1.0/3);
}

static inline double sinc(double x) {
    if (x <= 0.000000005)
        return 1;
    else
        return sin(M_PI * x) / (M_PI * x);
}

static inline double scale_weight_lanczos(double x, int lobes) {
    x = fabs(x);
    if (x < lobes)
        return sinc(x) * sinc(x / lobes);
    else
        return 0;
}

static inline double scale_weight_lanczos2(double x) {
    return scale_weight_lanczos(x, 2);
}

static inline double scale_weight_lanczos3(double x) {
    return scale_weight_lanczos(x, 3);
}

static void scale_weightset_create(scale_weightset* wset, int nin, int nout,
                                   double (*weightf)(double), double radius) {
    double of = (double) nin / nout;
    double reduction = of > 1.0 ? of : 1.0;
    int opos;

    for (opos = 0; opos != nout; ++opos) {
        double icenter = (opos + 0.5) * of - 0.5;
        int ipos0 = (int) ceil(icenter - radius * reduction - 0.0001);
        int ipos1 = (int) floor(icenter + radius * reduction + 0.0001) + 1;
        int wset0 = wset->n;
        double sum = 0;

        for (; ipos0 != ipos1; ++ipos0) {
            double w = (*weightf)((ipos0 - icenter) / reduction);
            if (w != 0.0) {
                int ripos = ipos0 < 0 ? 0 : (ipos0 < nin ? ipos0 : nin - 1);
                scale_weightset_add(wset, ripos, opos, w);
                sum += w;
            }
        }

        for (; wset0 != wset->n; ++wset0)
            wset->ws[wset0].w /= sum;
    }

    scale_weightset_add(wset, nin, nout, 0);
}

static void scale_image_data_weighted(scale_context* sctx, Gif_Image* gfo,
                                      double (*weightf)(double),
                                      double radius) {
    int yi, yi0, yi1, xo, yo;
    scale_color* sc = Gif_NewArray(scale_color, gfo->width);
    scale_color* kcx = Gif_NewArray(scale_color, gfo->width * sctx->iscr.height);
    const scale_weight* w, *ww;

    if (!sctx->xweights.ws) {
        scale_weightset_create(&sctx->xweights, sctx->iscr.width,
                               sctx->oscr.width, weightf, radius);
        scale_weightset_create(&sctx->yweights, sctx->iscr.height,
                               sctx->oscr.height, weightf, radius);
    }

    scale_image_prepare(sctx);

    /* scale every input row by the X scaling factor */
    {
        double ycmp = radius * (sctx->oyf > 1 ? sctx->oyf : 1);
        yi0 = (int) floor(gfo->top * sctx->oyf - ycmp - 0.0001);
        yi0 = yi0 < 0 ? 0 : yi0;
        yi1 = (int) ceil((gfo->top + gfo->height) * sctx->oyf + ycmp + 0.0001) + 1;
        yi1 = yi1 < (int) sctx->iscr.height ? yi1 : (int) sctx->iscr.height;
    }

    for (ww = sctx->xweights.ws; ww->opos < gfo->left; ++ww)
        /* skip */;
    for (yi = yi0; yi != yi1; ++yi) {
        const scale_color* iscr = &sctx->iscr.data[sctx->iscr.width * yi];
        scale_color* oscr = &kcx[gfo->width * yi];
        for (xo = 0; xo != gfo->width; ++xo)
            sc_clear(&oscr[xo]);
        for (w = ww; w->opos < gfo->left + gfo->width; ++w)
            SCVEC_ADDVxF(oscr[w->opos - gfo->left], iscr[w->ipos], w->w);
    }

    /* scale the resulting rows by the y factor */
    for (w = sctx->yweights.ws; w->opos < gfo->top; ++w)
        /* skip */;
    for (yo = 0; yo != gfo->height; ++yo) {
        for (xo = 0; xo != gfo->width; ++xo)
            sc_clear(&sc[xo]);
        for (; w->opos < gfo->top + yo + 1; ++w) {
            const scale_color* iscr = &kcx[gfo->width * w->ipos];
            assert(w->ipos >= yi0 && w->ipos < yi1);
            for (xo = 0; xo != gfo->width; ++xo)
                SCVEC_ADDVxF(sc[xo], iscr[xo], w->w);
        }
        /* generate output */
        scale_image_output_row(sctx, sc, gfo, yo);
    }

    scale_image_complete(sctx, gfo);

    Gif_DeleteArray(sc);
    Gif_DeleteArray(kcx);
}

static void scale_image_data_catrom(scale_context* sctx, Gif_Image* gfo) {
    scale_image_data_weighted(sctx, gfo, scale_weight_catrom, 2);
}

static void scale_image_data_mitchell(scale_context* sctx, Gif_Image* gfo) {
    scale_image_data_weighted(sctx, gfo, scale_weight_mitchell, 2);
}

static void scale_image_data_lanczos2(scale_context* sctx, Gif_Image* gfo) {
    scale_image_data_weighted(sctx, gfo, scale_weight_lanczos2, 2);
}

static void scale_image_data_lanczos3(scale_context* sctx, Gif_Image* gfo) {
    scale_image_data_weighted(sctx, gfo, scale_weight_lanczos3, 3);
}


static void scale_image(scale_context* sctx, int method) {
    Gif_Image* gfi = sctx->gfi;
    Gif_Image gfo;
    int was_compressed = (gfi->img == 0);

    /* Fri 9 Jan 1999: Fix problem with resizing animated GIFs: we scaled
       from left edge of the *subimage* to right edge of the subimage,
       causing consistency problems when several subimages overlap.
       Solution: always use scale factors relating to the *whole image* (the
       screen size). */

    /* calculate new width and height based on the four edges (left, right,
       top, bottom). This is better than simply multiplying the width and
       height by the scale factors because it avoids roundoff
       inconsistencies between frames on animated GIFs. Don't allow 0-width
       or 0-height images; GIF doesn't support them well. */
    gfo = *gfi;
    gfo.img = NULL;
    gfo.image_data = NULL;
    gfo.compressed = NULL;

    gfo.left = (int) (gfi->left * sctx->ixf);
    gfo.top = (int) (gfi->top * sctx->iyf);
    gfo.width = (int) ceil((gfi->left + gfi->width) * sctx->ixf) - gfo.left;
    gfo.height = (int) ceil((gfi->top + gfi->height) * sctx->iyf) - gfo.top;
    /* account for floating point errors at bottom right edge */
    if ((unsigned) gfi->left + gfi->width == sctx->iscr.width)
        gfo.width = sctx->oscr.width - gfo.left;
    if ((unsigned) gfi->top + gfi->height == sctx->iscr.height)
        gfo.height = sctx->oscr.height - gfo.top;

    /* Point scaling method is special: rather than rendering the input
       image into a frame buffer, it works on per-frame data. Thus we must
       verify that the output bounds completely fit into the input image's
       per-frame data. Using pixel midpoints complicates this task. */
    if (method == SCALE_METHOD_POINT) {
        if (gfo.width && (int) ((gfo.left + 0.5) * sctx->oxf) < gfi->left)
            ++gfo.left, --gfo.width;
        if (gfo.width && (int) ((gfo.left + gfo.width - 0.5) * sctx->oxf)
                             >= gfi->left + gfi->width)
            --gfo.width;
        if (gfo.height && (int) ((gfo.top + 0.5) * sctx->oyf) < gfi->top)
            ++gfo.top, --gfo.height;
        if (gfo.height && (int) ((gfo.top + gfo.height - 0.5) * sctx->oyf)
                              >= gfi->top + gfi->height)
            --gfo.height;
    }

    if (gfo.width == 0 || gfo.height == 0) {
        gfo.width = gfo.height = 1;
        Gif_CreateUncompressedImage(&gfo, 0);
        gfo.image_data[0] = gfo.transparent = 0;
        gfo.disposal = GIF_DISPOSAL_ASIS;
        goto done;
    }

    if (was_compressed)
        Gif_UncompressImage(sctx->gfs, gfi);
    Gif_CreateUncompressedImage(&gfo, 0);

    if (method == SCALE_METHOD_MIX)
        scale_image_data_mix(sctx, &gfo);
    else if (method == SCALE_METHOD_BOX)
        scale_image_data_box(sctx, &gfo);
    else if (method == SCALE_METHOD_CATROM)
        scale_image_data_catrom(sctx, &gfo);
    else if (method == SCALE_METHOD_LANCZOS2)
        scale_image_data_lanczos2(sctx, &gfo);
    else if (method == SCALE_METHOD_LANCZOS3)
        scale_image_data_lanczos3(sctx, &gfo);
    else if (method == SCALE_METHOD_MITCHELL)
        scale_image_data_mitchell(sctx, &gfo);
    else
        scale_image_data_point(sctx, &gfo);

 done:
    Gif_ReleaseUncompressedImage(gfi);
    Gif_ReleaseCompressedImage(gfi);
    *gfi = gfo;
    if (was_compressed) {
        Gif_FullCompressImage(sctx->gfs, gfi, &gif_write_info);
        Gif_ReleaseUncompressedImage(gfi);
    }
}


#if ENABLE_THREADS
typedef struct {
    pthread_t threadid;
    Gif_Stream* gfs;
    int imageno;
    int* next_imageno;
    int nw;
    int nh;
    int scale_colors;
    int method;
} scale_thread_context;

#if !HAVE___SYNC_ADD_AND_FETCH
static pthread_mutex_t next_imageno_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

void* scale_image_threaded(void* args) {
    scale_thread_context* ctx = (scale_thread_context*) args;
    scale_context sctx;
    sctx_init(&sctx, ctx->gfs, ctx->nw, ctx->nh);
    sctx.scale_colors = ctx->scale_colors;
    do {
        sctx.imageno = ctx->imageno;
        sctx.gfi = ctx->gfs->images[ctx->imageno];
        scale_image(&sctx, ctx->method);
#if HAVE___SYNC_ADD_AND_FETCH
        ctx->imageno = __sync_add_and_fetch(ctx->next_imageno, 1);
#else
        pthread_mutex_lock(&next_imageno_lock);
        ctx->imageno = ++*ctx->next_imageno;
        pthread_mutex_unlock(&next_imageno_lock);
#endif
    } while (ctx->imageno < ctx->gfs->nimages);
    sctx_cleanup(&sctx);
    return 0;
}
#endif

void
resize_dimensions(int* w, int* h, double new_width, double new_height,
                  int flags)
{
    if (new_width < 0.5 && new_height < 0.5)
        /* do nothing */
        return;
    else if (new_width < 0.5)
        new_width = *w * new_height / *h;
    else if (new_height < 0.5)
        new_height = *h * new_width / *w;

    if (flags & GT_RESIZE_FIT) {
        double factor, xfactor, yfactor;
        if (((flags & GT_RESIZE_FIT_DOWN)
             && *w < new_width + 0.5
             && *h < new_height + 0.5)
            || ((flags & GT_RESIZE_FIT_UP)
                && (*w >= new_width + 0.5
                    || *h >= new_height + 0.5)))
            return;
        xfactor = new_width / *w;
        yfactor = new_height / *h;
        if ((xfactor < yfactor) == !(flags & GT_RESIZE_MIN_DIMEN))
            factor = xfactor;
        else
            factor = yfactor;
        new_width = *w * factor;
        new_height = *h * factor;
    }

    if (new_width >= GIF_MAX_SCREEN_WIDTH + 0.5
        || new_height >= GIF_MAX_SCREEN_HEIGHT + 0.5)
        fatal_error("new image is too large (max size 65535x65535)");

    *w = (int) (new_width + 0.5);
    *h = (int) (new_height + 0.5);

    /* refuse to create 0-pixel dimensions */
    if (*w == 0)
        *w = 1;
    if (*h == 0)
        *h = 1;
}

void
resize_stream(Gif_Stream* gfs,
              double new_width, double new_height,
              int flags, int method, int scale_colors)
{
    int nw, nh, nthreads = thread_count, i;
    (void) i;

    Gif_CalculateScreenSize(gfs, 0);
    assert(gfs->nimages > 0);
    nw = gfs->screen_width;
    nh = gfs->screen_height;
    resize_dimensions(&nw, &nh, new_width, new_height, flags);
    if (nw == gfs->screen_width && nh == gfs->screen_height)
        return;

    /* no point to MIX or BOX method if we're expanding the image in
       both dimens */
    if (method == SCALE_METHOD_BOX
        && gfs->screen_width <= nw && gfs->screen_height <= nh)
        method = SCALE_METHOD_POINT;
    if (method == SCALE_METHOD_MIX
        && gfs->screen_width <= nw && gfs->screen_height <= nh
        && (nw % gfs->screen_width) == 0 && (nh % gfs->screen_height) == 0)
        method = SCALE_METHOD_POINT;
    /* ensure we understand the method */
    if (method != SCALE_METHOD_MIX && method != SCALE_METHOD_BOX
        && method != SCALE_METHOD_CATROM && method != SCALE_METHOD_LANCZOS2
        && method != SCALE_METHOD_LANCZOS3 && method != SCALE_METHOD_MITCHELL)
        method = SCALE_METHOD_POINT;

    if (nthreads > gfs->nimages)
        nthreads = gfs->nimages;
#if ENABLE_THREADS
    /* Threaded resize only works if the input image is unoptimized. */
    for (i = 0; nthreads > 1 && i < gfs->nimages; ++i)
        if (gfs->images[i]->left != 0
            || gfs->images[i]->top != 0
            || gfs->images[i]->width != gfs->screen_width
            || gfs->images[i]->height != gfs->screen_height
            || (i != gfs->nimages - 1
                && gfs->images[i]->disposal != GIF_DISPOSAL_BACKGROUND
                && gfs->images[i+1]->transparent >= 0)) {
            warning(1, "image too complex for multithreaded resize, using 1 thread\n  (Try running the GIF through %<gifsicle -U%>.)");
            nthreads = 1;
        }
    if (nthreads > 1) {
        int next_imageno = nthreads - 1, i;
        scale_thread_context* ctx = Gif_NewArray(scale_thread_context, nthreads);
        for (i = 0; i < nthreads; ++i) {
            ctx[i].gfs = gfs;
            ctx[i].imageno = i;
            ctx[i].next_imageno = &next_imageno;
            ctx[i].nw = nw;
            ctx[i].nh = nh;
            ctx[i].scale_colors = scale_colors;
            ctx[i].method = method;
            pthread_create(&ctx[i].threadid, NULL, scale_image_threaded, &ctx[i]);
        }
        for (i = 0; i < nthreads; ++i)
            pthread_join(ctx[i].threadid, NULL);
        Gif_DeleteArray(ctx);
    }
#else
    nthreads = 1;
#endif

    if (nthreads <= 1) {
        scale_context sctx;
        sctx_init(&sctx, gfs, nw, nh);
        sctx.scale_colors = scale_colors;

        for (sctx.imageno = 0; sctx.imageno < gfs->nimages; ++sctx.imageno) {
            sctx.gfi = gfs->images[sctx.imageno];
            scale_image(&sctx, method);
        }
        sctx_cleanup(&sctx);
    }

    gfs->screen_width = nw;
    gfs->screen_height = nh;
}
