/* gifunopt.c - Unoptimization function for the GIF library.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*; you can copy, distribute, or alter it at
   will, as long as this notice is kept intact and this source code is made
   available. Hypo(pa)thetical commerical developers are asked to write the
   author a note, which might make his day. There is no warranty, express or
   implied.

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
			  byte *old_data, int old_transparent)
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
    gfs->global->col = Gif_ReArray(gfs->global->col, Gif_Color, 256);
    if (!gfs->global->col) return -1;
    gfs->global->ncol = transparent + 1;
  }
  
  /* transform old transparent colors into new transparent colors */
  for (i = 0; i < size; i++)
    if (old_data[i] == old_transparent)
      old_data[i] = transparent;
  
  return transparent;
}


static int
unoptimize_image(Gif_Stream *gfs, Gif_Image *gfi, byte *old_data,
		 int old_transparent)
{
  int transparent;
  int size = gfs->screen_width * gfs->screen_height;
  byte *new_data = Gif_NewArray(byte, size);
  if (!new_data) return -2;
  
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
      transparent =
	fix_transparency_conflict(gfs, gfi, old_data, old_transparent);
      if (transparent < 0)
	return -2;
      memcpy(new_data, old_data, size);
      put_image_in_screen(gfs, gfi, new_data, transparent);
      
    } else
      transparent = old_transparent;
  }
    
  if (gfi->disposal == GIF_DISPOSAL_NONE || gfi->disposal == GIF_DISPOSAL_ASIS)
    memcpy(old_data, new_data, size);
  else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
    put_background_in_screen(gfs, gfi, old_data, transparent);
  
  if (gfi->free_image_data)
    (*gfi->free_image_data)((void *)gfi->image_data);
  gfi->left = 0;
  gfi->top = 0;
  gfi->width = gfs->screen_width;
  gfi->height = gfs->screen_height;
  gfi->disposal = GIF_DISPOSAL_BACKGROUND;
  gfi->transparent = transparent;
  gfi->image_data = new_data;
  gfi->free_image_data = Gif_DeleteArrayFunc;
  Gif_MakeImg(gfi, gfi->image_data, 0);
  
  return transparent;
}


int
Gif_Unoptimize(Gif_Stream *gfs)
{
  int ok = 1;
  int i;
  int size;
  int was_transparent;
  byte *old_data;
  byte background;
  Gif_Image *gfi;
  
  if (gfs->nimages <= 1) return 1;
  for (i = 0; i < gfs->nimages; i++)
    if (gfs->images[i]->local)
      return 0;
  if (!gfs->global)
    return 0;
  
  if (gfs->screen_width <= 0)
    for (i = 0; i < gfs->nimages; i++)
      if (gfs->screen_width < gfs->images[i]->width)
	gfs->screen_width = gfs->images[i]->width;
  if (gfs->screen_height <= 0)
    for (i = 0; i < gfs->nimages; i++)
      if (gfs->screen_height < gfs->images[i]->height)
	gfs->screen_height = gfs->images[i]->height;
  size = gfs->screen_width * gfs->screen_height;
  
  old_data = Gif_NewArray(byte, size);
  gfi = gfs->images[0];
  background = gfi->transparent >= 0 ? gfi->transparent : gfs->background;
  was_transparent = gfi->transparent;
  memset(old_data, background, size);
  
  for (i = 0; i < gfs->nimages; i++) {
    gfi = gfs->images[i];
    was_transparent = unoptimize_image(gfs, gfi, old_data, was_transparent);
    if (was_transparent == -2)
      ok = 0;
  }
  
  Gif_DeleteArray(old_data);
  return ok;
}


#ifdef __cplusplus
}
#endif
