/* gifunopt.c - Unoptimization function for the GIF library.
   Copyright (C) 1997-2025 Eddie Kohler, ekohler@gmail.com
   This file is part of the LCDF GIF library.

   The LCDF GIF library is free software. It is distributed under the GNU
   General Public License, version 2; you can copy, distribute, or alter it at
   will, as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied. */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <lcdfgif/gif.h>
#include <assert.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

#define TRANSPARENT 256

/* set *w and *h to the size of the part of gfi that's on the screen.
   Frames can extend past the logical screen, or start beyond it. */
static void
clip_to_screen(Gif_Stream *gfs, Gif_Image *gfi, unsigned *w, unsigned *h)
{
  *w = gfi->left < gfs->screen_width ? gfs->screen_width - gfi->left : 0;
  if (*w > gfi->width)
    *w = gfi->width;
  *h = gfi->top < gfs->screen_height ? gfs->screen_height - gfi->top : 0;
  if (*h > gfi->height)
    *h = gfi->height;
}

static void
put_image_in_screen(Gif_Stream *gfs, Gif_Image *gfi, uint16_t *screen)
{
  int transparent = gfi->transparent;
  unsigned x, y, w, h;
  clip_to_screen(gfs, gfi, &w, &h);

  for (y = 0; y < h; y++) {
    uint16_t *move = screen + (unsigned) gfs->screen_width * (y + gfi->top) + gfi->left;
    uint8_t *line = gfi->img[y];
    for (x = 0; x < w; x++, move++, line++)
      if (*line != transparent)
        *move = *line;
  }
}


static void
put_background_in_screen(Gif_Stream *gfs, Gif_Image *gfi, uint16_t *screen,
                         uint16_t background)
{
  unsigned x, y, w, h;
  clip_to_screen(gfs, gfi, &w, &h);

  for (y = 0; y < h; y++) {
    uint16_t *move = screen + (unsigned) gfs->screen_width * (y + gfi->top) + gfi->left;
    for (x = 0; x < w; x++, move++)
      *move = background;
  }
}


/* Draw gfi on screen and return the resulting screen. For PREVIOUS
   disposal, this is a copy, so that screen is unchanged after disposal. */
static uint16_t *
draw_frame(Gif_Stream *gfs, Gif_Image *gfi, uint16_t *screen)
{
  unsigned size = (unsigned) gfs->screen_width * (unsigned) gfs->screen_height;
  uint16_t *shown = screen;
  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
    shown = Gif_NewArray(uint16_t, size);
    if (!shown)
      return 0;
    memcpy(shown, screen, size * sizeof(uint16_t));
  }
  put_image_in_screen(gfs, gfi, shown);
  return shown;
}

static void
dispose_frame(Gif_Stream *gfs, Gif_Image *gfi, uint16_t *screen,
              uint16_t *shown, uint16_t background)
{
  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS)
    Gif_DeleteArray(shown);
  else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
    put_background_in_screen(gfs, gfi, screen, background);
}


/* Return the first color unused in shown, or -1 if all 256 colors are
   used. Set *has_transparent if shown has transparent pixels. */
static int
first_unused_color(Gif_Stream *gfs, const uint16_t *shown, int *has_transparent)
{
  int have[257];
  unsigned pos, size = (unsigned) gfs->screen_width * (unsigned) gfs->screen_height;
  int i;

  assert(TRANSPARENT == 256);
  for (i = 0; i < 257; i++)
    have[i] = 0;
  for (pos = 0; pos != size; ++pos)
    have[shown[pos]] = 1;

  *has_transparent = have[TRANSPARENT];
  for (i = 0; i < 256; i++)
    if (!have[i])
      return i;
  return -1;
}


/* Return unoptimized image data for shown, mapping transparency to
   transparent. */
static uint8_t *
unoptimized_data(Gif_Stream *gfs, const uint16_t *shown, int transparent)
{
  unsigned pos, size = (unsigned) gfs->screen_width * (unsigned) gfs->screen_height;
  uint8_t *new_data = Gif_NewArray(uint8_t, size);
  if (new_data)
    for (pos = 0; pos != size; ++pos)
      new_data[pos] = shown[pos] == TRANSPARENT ? transparent : shown[pos];
  return new_data;
}


static int
no_more_transparency(Gif_Image *gfi1, Gif_Image *gfi2)
{
  int t1 = gfi1->transparent, t2 = gfi2->transparent, y;
  if (t1 < 0)
    return 1;
  for (y = 0; y < gfi1->height; ++y) {
    uint8_t *d1 = gfi1->img[y], *d2 = gfi2->img[y], *ed1 = d1 + gfi1->width;
    while (d1 < ed1) {
      if (*d1 == t1 && *d2 != t2)
        return 0;
      ++d1, ++d2;
    }
  }
  return 1;
}


/* Return true if the unoptimized frames might use all 256 colors. They can
   only use the opaque colors in the frames and the background. A global
   colormap alone doesn't rule this out: a frame can show all 256 colors
   plus transparency, leaving no index for the transparent color. */
static int
might_run_out_of_colors(Gif_Stream *gfs, uint16_t background)
{
  int have[256], ncolors = 0, i, x, y;
  for (i = 0; i < 256; i++)
    have[i] = 0;
  if (background != TRANSPARENT) {
    have[background] = 1;
    ncolors = 1;
  }
  for (i = 0; i < gfs->nimages && ncolors < 256; i++) {
    Gif_Image *gfi = gfs->images[i];
    Gif_UncompressImage(gfs, gfi);
    for (y = 0; y < gfi->height; y++)
      for (x = 0; x < gfi->width; x++) {
        uint8_t p = gfi->img[y][x];
        if (!have[p] && p != gfi->transparent) {
          have[p] = 1;
          ++ncolors;
        }
      }
  }
  return ncolors >= 256;
}


/* Make sure the global colormap has a slot for color index c. */
static void
ensure_global_color(Gif_Stream *gfs, int c)
{
  Gif_Colormap *gfcm = gfs->global;
  if (c >= gfcm->ncol) {
    if (gfcm->capacity < 256) {
      Gif_ReArray(gfcm->col, Gif_Color, 256);
      gfcm->capacity = 256;
    }
    for (; gfcm->ncol <= c; gfcm->ncol++)
      GIF_SETCOLOR(&gfcm->col[gfcm->ncol], 0, 0, 0);
  }
}


/* Unoptimize every frame. If !commit, only check whether that's possible,
   without changing the stream. */
static int
unoptimize_frames(Gif_Stream *gfs, uint16_t *screen, uint16_t background,
                  int commit)
{
  unsigned pos, size = (unsigned) gfs->screen_width * (unsigned) gfs->screen_height;
  int i, unused, has_transparent;
  int frame0_unused = -1, frame0_transparent = 0, any_transparent = 0;
  uint16_t *shown;
  uint8_t *new_data = 0;

  for (pos = 0; pos != size; ++pos)
    screen[pos] = background;

  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    Gif_UncompressImage(gfs, gfi);
    if (!(shown = draw_frame(gfs, gfi, screen)))
      return 0;

    /* transparency is represented by a color unused in the frame */
    unused = first_unused_color(gfs, shown, &has_transparent);
    if (i == 0) {
      frame0_unused = unused;
      frame0_transparent = has_transparent;
    } else if (has_transparent)
      any_transparent = 1;
    if (!has_transparent)
      unused = -1;
    else if (unused < 0) {
      dispose_frame(gfs, gfi, screen, shown, background);
      return 0;
    }

    if (commit)
      new_data = unoptimized_data(gfs, shown, unused);
    dispose_frame(gfs, gfi, screen, shown, background);

    if (commit) {
      if (!new_data)
        return 0;
      if (unused >= 0)
        ensure_global_color(gfs, unused);
      Gif_ReleaseCompressedImage(gfi);
      gfi->left = 0;
      gfi->top = 0;
      gfi->width = gfs->screen_width;
      gfi->height = gfs->screen_height;
      gfi->transparent = unused;
      Gif_SetUncompressedImage(gfi, new_data, Gif_Free, 0);
    }
  }

  /* The background is transparent iff the first frame has a transparent
     color. If any frame shows a transparent background, keep a
     transparent color in the first frame, even if it is unused there. */
  if (background == TRANSPARENT && !frame0_transparent && any_transparent) {
    if (frame0_unused < 0)
      return 0;
    if (commit) {
      ensure_global_color(gfs, frame0_unused);
      gfs->images[0]->transparent = frame0_unused;
    }
  }
  return 1;
}


int
Gif_FullUnoptimize(Gif_Stream *gfs, int flags)
{
  int ok = 1;
  int i;
  uint16_t *screen;
  uint16_t background;
  Gif_Image *gfi;

  if (gfs->nimages < 1) return 1;
  for (i = 0; i < gfs->nimages; i++)
    if (gfs->images[i]->local)
      return 0;
  if (!gfs->global)
    return 0;

  Gif_CalculateScreenSize(gfs, 0);
  screen = Gif_NewArray(uint16_t, (unsigned) gfs->screen_width * (unsigned) gfs->screen_height);
  if (!screen)
    return 0;

  gfi = gfs->images[0];
  if (gfi->transparent < 0
      && gfs->global && gfs->background < gfs->global->ncol)
      background = gfs->background;
  else
      background = TRANSPARENT;

  /* Unoptimizing fails only if a frame needs a transparent color but uses
     all 256 colors. If that's possible, check first, so failure leaves the
     stream unchanged. */
  if (might_run_out_of_colors(gfs, background))
    ok = unoptimize_frames(gfs, screen, background, 0);
  if (ok)
    ok = unoptimize_frames(gfs, screen, background, 1);

  if (ok) {
    if (flags & GIF_UNOPTIMIZE_SIMPLEST_DISPOSAL) {
      /* set disposal based on use of transparency.
         If (every transparent pixel in frame i is also transparent in frame
         i - 1), then frame i - 1 gets disposal ASIS; otherwise, disposal
         BACKGROUND. */
      for (i = 0; i < gfs->nimages; ++i)
        if (i == gfs->nimages - 1
            || no_more_transparency(gfs->images[i+1], gfs->images[i]))
          gfs->images[i]->disposal = GIF_DISPOSAL_NONE;
        else
          gfs->images[i]->disposal = GIF_DISPOSAL_BACKGROUND;
    } else
      for (i = 0; i < gfs->nimages; ++i)
        gfs->images[i]->disposal = GIF_DISPOSAL_BACKGROUND;
  }

  Gif_DeleteArray(screen);
  return ok;
}

int
Gif_Unoptimize(Gif_Stream *gfs)
{
  return Gif_FullUnoptimize(gfs, 0);
}

#ifdef __cplusplus
}
#endif
