#ifndef GIFX_H
#define GIFX_H
#ifdef __cplusplus
extern "C" {
#endif

/* gifx.h - Functions to turn GIFs in memory into X Pixmaps.
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
#include <X11/Xlib.h>


typedef struct Gif_XContext Gif_XContext;

struct Gif_XContext {
  
  Display *display;
  Drawable drawable;
  Visual *visual;
  u_int16_t depth;
  u_int16_t ncolormap;
  Colormap colormap;
  
  u_int16_t nclosest;
  Gif_Color *closest;
  
  unsigned long transparent_value;
  int refcount;
  
};


Gif_XContext *		Gif_NewXContext(Display *, Window);
Gif_XContext *		Gif_NewXContextFromVisual(Display *, int screen_number,
					Visual *, int depth, Colormap);
void			Gif_DeleteXContext(Gif_XContext *);

Pixmap			Gif_XImage(Gif_XContext *, Gif_Stream *, Gif_Image *);
Pixmap			Gif_XImageColormap(Gif_XContext *, Gif_Stream *,
					   Gif_Colormap *, Gif_Image *);
Pixmap			Gif_XMask(Gif_XContext *, Gif_Stream *, Gif_Image *);

void			Gif_XPreallocateColors(Gif_XContext *, Gif_Colormap *);


#ifdef __cplusplus
}
#endif
#endif
