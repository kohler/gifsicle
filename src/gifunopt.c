/* gifunopt.c - Unoptimization function for the GIF library.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*. It is distributed under the GNU Public
   License, version 2 or later; you can copy, distribute, or alter it at will,
   as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied.

   *The LZW compression method used by GIFs is patented. Unisys, the patent
   holder, allows the compression algorithm to be used without a license in
   software distributed at no cost to the user. */

#include "gif.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif


/* Returns non0 iff the old transparent color (old_transparent) was written
   over with an opaque color. */

static int
put_image_in_screen(Gif_Stream *gfs, Gif_Image *gfi, byte *screendata,
		    int old_transparent)
{
  int old_transparent_opaque = 0;
  int transparent = gfi->transparent;
  int x, y;
  int w = gfi->width;
  int h = gfi->height;
  if (gfi->left + w > gfs->screen_width) w = gfs->screen_width - gfi->left;
  if (gfi->top + h > gfs->screen_height) h = gfs->screen_height - gfi->top;
  
  for (y = 0; y < h; y++) {
    byte *move = screendata + gfs->screen_width * (y + gfi->top) + gfi->left;
    byte *line = gfi->img[y];
    for (x = 0; x < w; x++, move++, line++)
      if (*line != transparent) {
	*move = *line;
	if (*line == old_transparent)
	  old_transparent_opaque = 1;
      }
  }
  
  return old_transparent_opaque;
}


static void
put_background_in_screen(Gif_Stream *gfs, Gif_Image *gfi, byte *screendata,
			 int transparent)
{
  byte solid;
  int y;
  int w = gfi->width;
  int h = gfi->height;
  if (gfi->left + w > gfs->screen_width) w = gfs->screen_width - gfi->left;
  if (gfi->top + h > gfs->screen_height) h = gfs->screen_height - gfi->top;
  
  if (gfi->transparent >= 0)
    solid = transparent;
  else if (gfs->images[0]->transparent >= 0)
    solid = transparent;
  else
    solid = gfs->background;
  
  for (y = 0; y < h; y++) {
    byte *move = screendata + gfs->screen_width * (y + gfi->top) + gfi->left;
    memset(move, solid, w);
  }
}


static int
fix_transparency_conflict(Gif_Stream *gfs, Gif_Image *gfi,
			  byte *old_data, int old_transparent, byte *new_data)
{
  int have[256];
  int transparent = gfi->transparent;
  int size = gfs->screen_width * gfs->screen_height;
  int i, x, y;
  int have_transparent;
  assert(old_transparent >= 0);
  
  for (i = 0; i < 256; i++)
    have[i] = 0;
  
  /* mark colors used opaquely in the old image */
  for (i = 0; i < size; i++)
    have[old_data[i]] = 1;
  have[old_transparent] = 0;
  
  /* mark colors used opaquely in the new image */
  if (transparent >= 0) have_transparent = have[transparent];
  for (y = 0; y < gfi->height; y++) {
    byte *line = gfi->img[y];
    for (x = 0; x < gfi->width; x++)
      have[line[x]] = 1;
  }
  if (transparent >= 0) have[transparent] = have_transparent;
  
  /* the new transparent color is a color unused in either */
  transparent = -1;
  for (i = 0; i < 256 && transparent < 0; i++)
    if (!have[i])
      transparent = i;
  if (transparent < 0)
    return -1;
  if (transparent >= gfs->global->ncol) {
    Gif_ReArray(gfs->global->col, Gif_Color, 256);
    if (!gfs->global->col) return -1;
    gfs->global->ncol = transparent + 1;
  }
  
  /* transform old transparent colors into new transparent colors */
  for (i = 0; i < size; i++, old_data++, new_data++)
    if (*old_data == old_transparent)
      *new_data = transparent;
    else
      *new_data = *old_data;
  
  return transparent;
}


static int
unoptimize_image(Gif_Stream *gfs, Gif_Image *gfi,
		 byte **old_data_store, int *old_transparent_store,
		 byte *old_data_buffer)
{
  byte *old_data = *old_data_store;
  int old_transparent = *old_transparent_store;
  int transparent;
  int size = gfs->screen_width * gfs->screen_height;
  byte *new_data = Gif_NewArray(byte, size);
  if (!new_data) return 0;
  
  /* Oops! May need to uncompress it */
  Gif_UncompressImage(gfi);
  Gif_ReleaseCompressedImage(gfi);
  
  /* Treat a full replacement frame (as big as the screen and no transparency)
     specially, since we can do it a lot faster. */
  if (gfi->left == 0 && gfi->top == 0
      && gfi->width >= gfs->screen_width && gfi->height >= gfs->screen_height
      && gfi->transparent < 0) {
    int j;
    int screen_width = gfs->screen_width;
    for (j = 0; j < gfs->screen_height; j++)
      memcpy(new_data + screen_width * j, gfi->img[j], screen_width);
    transparent = -1;
    if (gfi->disposal == GIF_DISPOSAL_BACKGROUND
	&& gfs->images[0]->transparent >= 0)
      transparent = gfs->images[0]->transparent;
    
  } else {
    memcpy(new_data, old_data, size);
    
    if (put_image_in_screen(gfs, gfi, new_data, old_transparent)) {
      /* This is the bad case. The old transparent color became opaque in this
	 image, and it was actually used. */
      transparent = fix_transparency_conflict
	(gfs, gfi, old_data, old_transparent, new_data);
      if (transparent < 0) {
	Gif_DeleteArray(new_data);
	return 0;
      }
      put_image_in_screen(gfs, gfi, new_data, transparent);
      
    } else
      transparent = old_transparent;
  }
    
  if (gfi->disposal == GIF_DISPOSAL_NONE || gfi->disposal == GIF_DISPOSAL_ASIS)
    /* Reuse new_data as the next old_data if possible. */
    old_data = new_data;
  else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND) {
    if (old_data != old_data_buffer)
      memcpy(old_data_buffer, old_data, size);
    old_data = old_data_buffer;
    put_background_in_screen(gfs, gfi, old_data, transparent);
  }
  
  gfi->left = 0;
  gfi->top = 0;
  gfi->width = gfs->screen_width;
  gfi->height = gfs->screen_height;
  gfi->disposal = GIF_DISPOSAL_BACKGROUND;
  gfi->transparent = transparent;
  Gif_SetUncompressedImage(gfi, new_data, Gif_DeleteArrayFunc, 0);
  
  *old_transparent_store = transparent;
  *old_data_store = old_data;
  return 1;
}


int
Gif_Unoptimize(Gif_Stream *gfs)
{
  int ok = 1;
  int i, size, was_transparent;
  byte *old_data;
  byte *old_data_buffer;
  byte background;
  Gif_Image *gfi;
  
  if (gfs->nimages <= 1) return 1;
  for (i = 0; i < gfs->nimages; i++)
    if (gfs->images[i]->local)
      return 0;
  if (!gfs->global)
    return 0;

  Gif_CalculateScreenSize(gfs, 0);
  size = gfs->screen_width * gfs->screen_height;
  
  old_data_buffer = Gif_NewArray(byte, size);
  gfi = gfs->images[0];
  background = gfi->transparent >= 0 ? gfi->transparent : gfs->background;
  was_transparent = gfi->transparent;
  old_data = old_data_buffer;
  memset(old_data, background, size);
  
  for (i = 0; i < gfs->nimages; i++)
    if (!unoptimize_image(gfs, gfs->images[i], &old_data, &was_transparent,
			  old_data_buffer))
      ok = 0;
  
  Gif_DeleteArray(old_data_buffer);
  return ok;
}


#ifdef __cplusplus
}
#endif
