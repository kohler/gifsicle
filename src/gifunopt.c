/* gifunopt.c - Unoptimization function for the GIF library.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
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

static void
put_image_in_screen(Gif_Stream *gfs, Gif_Image *gfi, uint16_t *screen)
{
  int transparent = gfi->transparent;
  int x, y;
  int w = gfi->width;
  int h = gfi->height;
  if (gfi->left + w > gfs->screen_width)
      w = gfs->screen_width - gfi->left;
  if (gfi->top + h > gfs->screen_height)
      h = gfs->screen_height - gfi->top;

  for (y = 0; y < h; y++) {
    uint16_t *move = screen + gfs->screen_width * (y + gfi->top) + gfi->left;
    uint8_t *line = gfi->img[y];
    for (x = 0; x < w; x++, move++, line++)
      if (*line != transparent)
        *move = *line;
  }
}


static void
put_background_in_screen(Gif_Stream *gfs, Gif_Image *gfi, uint16_t *screen)
{
  uint16_t solid;
  int x, y;
  int w = gfi->width;
  int h = gfi->height;
  if (gfi->left + w > gfs->screen_width) w = gfs->screen_width - gfi->left;
  if (gfi->top + h > gfs->screen_height) h = gfs->screen_height - gfi->top;

  if (gfi->transparent < 0 && gfs->images[0]->transparent < 0
      && gfs->global && gfs->background < gfs->global->ncol)
      solid = gfs->background;
  else
      solid = TRANSPARENT;

  for (y = 0; y < h; y++) {
    uint16_t *move = screen + gfs->screen_width * (y + gfi->top) + gfi->left;
    for (x = 0; x < w; x++, move++)
      *move = solid;
  }
}


static int
create_image_data(Gif_Stream *gfs, Gif_Image *gfi, uint16_t *screen,
                  uint8_t *new_data, int *used_transparent)
{
  int have[257];
  int transparent = -1;
  unsigned pos, size = gfs->screen_width * gfs->screen_height;
  uint16_t *move;
  int i;

  /* mark colors used opaquely in the image */
  assert(TRANSPARENT == 256);
  for (i = 0; i < 257; i++)
    have[i] = 0;
  for (pos = 0, move = screen; pos != size; ++pos, move++)
    have[*move] = 1;

  /* the new transparent color is a color unused in either */
  if (have[TRANSPARENT]) {
    for (i = 0; i < 256 && transparent < 0; i++)
      if (!have[i])
        transparent = i;
    if (transparent < 0)
      goto error;
    if (transparent >= gfs->global->ncol) {
      Gif_ReArray(gfs->global->col, Gif_Color, 256);
      if (!gfs->global->col) goto error;
      gfs->global->ncol = transparent + 1;
    }
  }

  /* map the wide image onto the new data */
  *used_transparent = 0;
  for (pos = 0, move = screen; pos != size; ++pos, move++, new_data++)
    if (*move == TRANSPARENT) {
      *new_data = transparent;
      *used_transparent = 1;
    } else
      *new_data = *move;

  gfi->transparent = transparent;
  return 1;

 error:
  return 0;
}


static int
unoptimize_image(Gif_Stream *gfs, Gif_Image *gfi, uint16_t *screen)
{
  unsigned size = gfs->screen_width * gfs->screen_height;
  int used_transparent;
  uint8_t *new_data = Gif_NewArray(uint8_t, size);
  uint16_t *new_screen = screen;
  if (!new_data) return 0;

  /* Oops! May need to uncompress it */
  Gif_UncompressImage(gfs, gfi);
  Gif_ReleaseCompressedImage(gfi);

  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
    new_screen = Gif_NewArray(uint16_t, size);
    if (!new_screen) return 0;
    memcpy(new_screen, screen, size * sizeof(uint16_t));
  }

  put_image_in_screen(gfs, gfi, new_screen);
  if (!create_image_data(gfs, gfi, new_screen, new_data, &used_transparent)) {
    Gif_DeleteArray(new_data);
    return 0;
  }

  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS)
    Gif_DeleteArray(new_screen);
  else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
    put_background_in_screen(gfs, gfi, screen);

  gfi->left = 0;
  gfi->top = 0;
  gfi->width = gfs->screen_width;
  gfi->height = gfs->screen_height;
  gfi->disposal = used_transparent;
  Gif_SetUncompressedImage(gfi, new_data, Gif_Free, 0);

  return 1;
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


int
Gif_FullUnoptimize(Gif_Stream *gfs, int flags)
{
  int ok = 1;
  int i;
  unsigned pos, size;
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
  size = gfs->screen_width * gfs->screen_height;

  screen = Gif_NewArray(uint16_t, size);
  gfi = gfs->images[0];
  if (gfi->transparent < 0
      && gfs->global && gfs->background < gfs->global->ncol)
      background = gfs->background;
  else
      background = TRANSPARENT;
  for (pos = 0; pos != size; ++pos)
    screen[pos] = background;

  for (i = 0; i < gfs->nimages; i++)
    if (!unoptimize_image(gfs, gfs->images[i], screen))
      ok = 0;

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
