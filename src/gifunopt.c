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


static void
put_image_in_screen(Gif_Stream *gfs, Gif_Image *gfi, byte *screendata)
{
  byte **img = gfi->img;
  int transparent = gfi->transparent;
  int y;
  int w = gfi->width;
  int h = gfi->height;
  if (gfi->left + w > gfs->screen_width) w = gfs->screen_width - gfi->left;
  if (gfi->top + h > gfs->screen_height) h = gfs->screen_height - gfi->top;
  
  for (y = 0; y < h; y++) {
    byte *move = screendata + gfs->screen_width * (y + gfi->top) + gfi->left;
    int x;
    for (x = 0; x < w; x++, move++)
      if (img[y][x] != transparent)
	*move = img[y][x];
  }
}


static void
put_background_in_screen(Gif_Stream *gfs, Gif_Image *gfi, byte *screendata)
{
  byte solid = gfi->transparent >= 0 ? gfi->transparent : gfs->background;
  int y;
  int w = gfi->width;
  int h = gfi->height;
  if (gfi->left + w > gfs->screen_width) w = gfs->screen_width - gfi->left;
  if (gfi->top + h > gfs->screen_height) h = gfs->screen_height - gfi->top;
  
  for (y = 0; y < h; y++) {
    byte *move = screendata + gfs->screen_width * (y + gfi->top) + gfi->left;
    memset(move, solid, w);
  }
}


static void
unoptimize_image(Gif_Stream *gfs, Gif_Image *gfi, byte *olddata)
{
  int size = gfs->screen_width * gfs->screen_height;
  byte *newdata = Gif_NewArray(byte, size);
  memcpy(newdata, olddata, size);
  
  put_image_in_screen(gfs, gfi, newdata);
  
  if (gfi->disposal == 0 || gfi->disposal == 1)
    memcpy(olddata, newdata, size);
  
  else if (gfi->disposal == 2)
    put_background_in_screen(gfs, gfi, olddata);
  
  Gif_DeleteArray(gfi->imagedata);
  gfi->left = 0;
  gfi->top = 0;
  gfi->width = gfs->screen_width;
  gfi->height = gfs->screen_height;
  gfi->disposal = 2;
  gfi->imagedata = newdata;
  Gif_MakeImg(gfi, gfi->imagedata, 0);
}


void
Gif_Unoptimize(Gif_Stream *gfs)
{
  int i;
  int size;
  byte *olddata;
  byte background;
  Gif_Image *gfi;
  
  if (gfs->nimages <= 1) return;
  
  if (gfs->screen_width <= 0)
    for (i = 0; i < gfs->nimages; i++)
      if (gfs->screen_width < gfs->images[i]->width)
	gfs->screen_width = gfs->images[i]->width;
  if (gfs->screen_height <= 0)
    for (i = 0; i < gfs->nimages; i++)
      if (gfs->screen_height < gfs->images[i]->height)
	gfs->screen_height = gfs->images[i]->height;
  size = gfs->screen_width * gfs->screen_height;
  
  olddata = Gif_NewArray(byte, size);
  gfi = gfs->images[0];
  background = gfi->transparent >= 0 ? gfi->transparent : gfs->background;
  memset(olddata, background, size);
  
  for (i = 0; i < gfs->nimages; i++) {
    gfi = gfs->images[i];
    
    if (gfi->left == 0 && gfi->top == 0
	&& gfi->width >= gfs->screen_width
	&& gfi->height >= gfs->screen_height
	&& gfi->transparent < 0) {
      
      if (gfi->disposal == 0 || gfi->disposal == 1) {
	byte **img = gfi->img;
	int y;
	for (y = 0; y < gfs->screen_height; y++)
	  memcpy(olddata + y * gfs->screen_width, img[y], gfs->screen_width);
	
      } else if (gfi->disposal == 2)
	memset(olddata, background, size);
      
    } else
      unoptimize_image(gfs, gfi, olddata);
  }
  
  Gif_DeleteArray(olddata);
}


#ifdef __cplusplus
}
#endif
