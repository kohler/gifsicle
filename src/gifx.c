/* gifx.c - Functions to turn GIFs in memory into X Pixmaps.
   Copyright (C) 1997-8 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*; you can copy, distribute, or alter it at
   will, as long as this notice is kept intact and this source code is made
   available. Hypo(pa)thetical commerical developers are asked to write the
   author a note, which might make his day. There is no warranty, express or
   implied.

   *The LZW compression method used by GIFs is patented. Unisys, the patent
   holder, allows the compression algorithm to be used without a license in
   software distributed at no cost to the user. */

#include "gifx.h"
#include "gif.h"
#include <X11/Xutil.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif


#define SAFELS(a,b) ((b) < 0 ? (a) >> -(b) : (a) << (b))

static void
load_closest(Gif_XContext *gfx)
{
  XColor *color;
  u_int16_t ncolor;
  u_int16_t ncolormap;
  int i;
  
  if (gfx->closest) return;
  
  ncolormap = ncolor = gfx->ncolormap;
  if (ncolor > 256) ncolor = 256;
  color = Gif_NewArray(XColor, ncolor);
  
  if (ncolormap > 256)
    for (i = 0; i < ncolor; i++)
      color[i].pixel = (rand() >> 4) % ncolormap;
  else
    for (i = 0; i < ncolor; i++)
      color[i].pixel = i;
  XQueryColors(gfx->display, gfx->colormap, color, ncolor);
  
  gfx->closest = Gif_NewArray(Gif_Color, ncolor);
  for (i = 0; i < ncolor; i++) {
    Gif_Color *c = &gfx->closest[i];
    c->haspixel = 1;
    c->red = color[i].red >> 8;
    c->green = color[i].green >> 8;
    c->blue = color[i].blue >> 8;
    c->pixel = color[i].pixel;
  }
  gfx->nclosest = ncolor;
  
  Gif_DeleteArray(color);
}


static int
allocate_closest(Gif_XContext *gfx, Gif_Color *c)
{
  Gif_Color *closer;
  Gif_Color *got = 0;
  u_int32_t distance = 0x4000000;
  int i;
  
  load_closest(gfx);
  
  for (i = 0, closer = gfx->closest; i < gfx->nclosest; i++, closer++) {
    int redd = c->red - closer->red;
    int greend = c->green - closer->green;
    int blued = c->blue - closer->blue;
    u_int32_t d = redd * redd + greend * greend + blued * blued;
    if (d < distance) {
      distance = d;
      got = closer;
    }
  }
  
  if (!got) return 0;
  if (!got->haspixel) {
    XColor xcol;
    xcol.red = got->red | (got->red << 8);
    xcol.green = got->green | (got->green << 8);
    xcol.blue = got->blue | (got->blue << 8);
    if (XAllocColor(gfx->display, gfx->colormap, &xcol) == 0) {
      /* Probably was a read/write color cell. Get rid of it!! */
      *got = gfx->closest[gfx->nclosest - 1];
      gfx->nclosest--;
      return allocate_closest(gfx, c);
    }
    got->pixel = xcol.pixel;
    got->haspixel = 1;
  }
  
  c->pixel = got->pixel;
  c->haspixel = 1;
  return 1;
}


static void
allocate_colors(Gif_XContext *gfx, u_int16_t size, Gif_Color *c)
{
  XColor xcol;
  int i;
  for (i = 0; i < size; i++, c++)
    if (!c->haspixel) {
      xcol.red = c->red | (c->red << 8);
      xcol.green = c->green | (c->green << 8);
      xcol.blue = c->blue | (c->blue << 8);
      if (XAllocColor(gfx->display, gfx->colormap, &xcol)) {
	c->pixel = xcol.pixel;
	c->haspixel = 1;
      } else
	allocate_closest(gfx, c);
    }
}


#define BYTESIZE 8

Pixmap
Gif_XImageColormap(Gif_XContext *gfx, Gif_Stream *gfs,
		   Gif_Colormap *gfcm, Gif_Image *gfi)
{
  Pixmap pixmap = None;
  XImage *ximage;
  byte *xdata;
  
  int i, j, k;
  int bytes_per_line;
  
  unsigned long savedtransparent = 0;
  u_int16_t nct;
  Gif_Color *ct;
  
  /* Find the correct image */
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) /* can't find an image to return */
    return None;
  
  /* Find the right colormap */
  if (!gfcm) /* can't find a colormap to use */
    return None;

  /* Allocate colors from the colormap; make sure the transparent color
   * has the given pixel value */
  ct = gfcm->col;
  nct = gfcm->ncol;
  allocate_colors(gfx, nct, ct);
  if (gfi->transparent > -1 && gfi->transparent < nct) {
    savedtransparent = ct[ gfi->transparent ].pixel;
    ct[ gfi->transparent ].pixel = gfx->transparentvalue;
  }
  
  /* Set up the X image */
  if (gfx->depth <= 8) i = 8;
  else if (gfx->depth <= 16) i = 16;
  else i = 32;
  ximage =
    XCreateImage(gfx->display, gfx->visual, gfx->depth,
		 gfx->depth == 1 ? XYBitmap : ZPixmap, 0, NULL,
		 gfi->width, gfi->height, i, 0);
  
  ximage->bitmap_bit_order = ximage->byte_order = LSBFirst;
  bytes_per_line = ximage->bytes_per_line;
  xdata = Gif_NewArray(byte, bytes_per_line * gfi->height);
  ximage->data = (char *)xdata;
  
  /* The main loop */
  if (ximage->bits_per_pixel % 8 == 0) {
    /* Optimize for cases where a pixel is exactly one or more bytes */
    int bytes_per_pixel = ximage->bits_per_pixel / 8;
    
    for (j = 0; j < gfi->height; j++) {
      byte *line = gfi->img[j];
      byte *writer = xdata + bytes_per_line * j;
      for (i = 0; i < gfi->width; i++) {
	u_int32_t pixel;
	if (line[i] < nct)
	  pixel = ct[line[i]].pixel;
	else
	  pixel = ct[0].pixel;
	for (k = 0; k < bytes_per_pixel; k++) {
	  *writer++ = pixel;
	  pixel >>= 8;
	}
      }
    }
    
  } else {
    /* Other bits-per-pixel */
    int bits_per_pixel = ximage->bits_per_pixel;
    u_int32_t bits_per_pixel_mask = (1UL << bits_per_pixel) - 1;
    
    for (j = 0; j < gfi->height; j++) {
      int imshift = 0;
      u_int32_t impixel = 0;
      byte *line = gfi->img[j];
      byte *writer = xdata + bytes_per_line * j;
      
      for (i = 0; i < gfi->width; i++) {
	u_int32_t pixel;
	if (line[i] < nct)
	  pixel = ct[line[i]].pixel;
	else
	  pixel = ct[0].pixel;
	
	impixel |= SAFELS(pixel & bits_per_pixel_mask, imshift);
	while (imshift + bits_per_pixel >= BYTESIZE) {
	  *writer++ = impixel;
	  imshift -= BYTESIZE;
	  impixel = SAFELS(pixel, imshift);
	}
	imshift += bits_per_pixel;
      }
      
      if (imshift)
	*writer++ = impixel;
    }
  }
  
  /* Restore saved transparent pixel value */
  if (gfi->transparent > -1 && gfi->transparent < nct)
    ct[ gfi->transparent ].pixel = savedtransparent;

  /* Create the pixmap */
  pixmap =
    XCreatePixmap(gfx->display, gfx->drawable,
		  gfi->width, gfi->height, gfx->depth);
  if (pixmap) {
    GC gc = XCreateGC(gfx->display, pixmap, 0, 0);
    XPutImage(gfx->display, pixmap, gc, ximage, 0, 0, 0, 0,
	      gfi->width, gfi->height);
    XFreeGC(gfx->display, gc);
  }
  
  Gif_DeleteArray(xdata);
  ximage->data = 0; /* avoid freeing it again in XDestroyImage */  
  XDestroyImage(ximage);
  return pixmap;
}


Pixmap
Gif_XImage(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi)
{
  Gif_Colormap *gfcm;
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) /* can't find an image to return */
    return None;
  gfcm = gfi->local;
  if (!gfcm) gfcm = gfs->global;
  return Gif_XImageColormap(gfx, gfs, gfcm, gfi);
}


Pixmap
Gif_XMask(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi)
{
  Pixmap pixmap = None;
  XImage *ximage;
  byte *xdata;
  
  int i, j;
  int transparent;
  int bytes_per_line;
  
  /* Find the correct image */
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi)
    return None;
  
  /* Create the X image */
  ximage =
    XCreateImage(gfx->display, gfx->visual, 1,
		 XYBitmap, 0, NULL,
		 gfi->width, gfi->height,
		 8, 0);
  
  ximage->bitmap_bit_order = ximage->byte_order = LSBFirst;
  bytes_per_line = ximage->bytes_per_line;
  xdata = Gif_NewArray(byte, bytes_per_line * gfi->height);
  ximage->data = (char *)xdata;
  
  transparent = gfi->transparent;
  
  /* The main loop */
  for (j = 0; j < gfi->height; j++) {
    int imshift = 0;
    unsigned long impixel = 0;
    byte *line = gfi->img[j];
    byte *writer = xdata + bytes_per_line * j;
    
    for (i = 0; i < gfi->width; i++) {
      if (line[i] == transparent)
	impixel |= 1 << imshift;
      
      if (++imshift >= BYTESIZE) {
	*writer++ = impixel;
	imshift = 0;
	impixel = 0;
      }
    }
    
    if (imshift)
      *writer++ = impixel;
  }
  
  /* Create the pixmap */
  pixmap =
    XCreatePixmap(gfx->display, gfx->drawable,
		  gfi->width, gfi->height, 1);
  if (pixmap) {
    GC gc = XCreateGC(gfx->display, pixmap, 0, 0);
    XPutImage(gfx->display, pixmap, gc, ximage, 0, 0, 0, 0,
	      gfi->width, gfi->height);
    XFreeGC(gfx->display, gc);
  }
  
  Gif_DeleteArray(xdata);
  ximage->data = 0; /* avoid freeing it again in XDestroyImage */  
  XDestroyImage(ximage);
  return pixmap;
}


void
Gif_XPreallocateColors(Gif_XContext *gfx, Gif_Colormap *gfcm)
{
  allocate_colors(gfx, gfcm->ncol, gfcm->col);
}


Gif_XContext *
Gif_NewXContext(Display *display, Window window)
{
  Gif_XContext *gfx;
  XWindowAttributes attr;
  
  gfx = Gif_New(Gif_XContext);
  gfx->display = display;
  gfx->drawable = window;
  
  XGetWindowAttributes(display, window, &attr);
  gfx->visual = attr.visual;
  gfx->colormap = attr.colormap;
  gfx->ncolormap = gfx->visual->map_entries;
  gfx->depth = attr.depth;
  
  gfx->closest = 0;
  gfx->nclosest = 0;
  
  gfx->transparentvalue = 0;
  
  return gfx;
}


#ifdef __cplusplus
}
#endif
