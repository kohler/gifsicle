/* gifx.c - Functions to turn GIFs in memory into X Pixmaps.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software; you can copy, distribute, or alter it at
   will, as long as this notice is kept intact and this source code is made
   available. Hypo(pa)thetical commerical developers are asked to write the
   author a note, which might make his day. There is no warranty, express or
   implied. */

#include "gifx.h"
#include "gifint.h"
#include <X11/Xutil.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif


#define SAFELS(a,b) ((b) < 0 ? (a) >> -(b) : (a) << (b))

static void
loadclosest(Gif_XContext *gfx)
{
  XColor *color;
  UINT16 ncolor;
  UINT16 ncolormap;
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
allocateclosest(Gif_XContext *gfx, Gif_Color *c)
{
  Gif_Color *closer;
  Gif_Color *got = 0;
  UINT32 distance = 0x4000000;
  int i;
  
  loadclosest(gfx);
  
  for (i = 0, closer = gfx->closest; i < gfx->nclosest; i++, closer++) {
    int redd = c->red - closer->red;
    int greend = c->green - closer->green;
    int blued = c->blue - closer->blue;
    UINT32 d = redd * redd + greend * greend + blued * blued;
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
      return allocateclosest(gfx, c);
    }
    got->pixel = xcol.pixel;
    got->haspixel = 1;
  }
  
  c->pixel = got->pixel;
  c->haspixel = 1;
  return 1;
}


static void
allocatecolors(Gif_XContext *gfx, UINT16 size, Gif_Color *c)
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
	allocateclosest(gfx, c);
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
  byte **img;
  
  int i, j;
  byte bitsperpixel;
  int bitsperpixelmask;
  int bytesperline;
  
  unsigned long savedtransparent = 0;
  UINT16 nct;
  Gif_Color *ct;
  
  /* Find the correct image */
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) /* can't find an image to return */
    return None;
  
  /* Find the right colormap */
  if (gfi->local) gfcm = gfi->local;
  if (!gfcm) gfcm = Gif_GetColormap(gfs, 0);
  if (!gfcm) /* can't find a colormap to use */
    return None;

  /* Allocate colors from the colormap; make sure the transparent color
   * has the given pixel value */
  ct = gfcm->col;
  nct = gfcm->ncol;
  allocatecolors(gfx, nct, ct);
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
  xdata = Gif_NewArray(byte, ximage->bytes_per_line * gfi->height);
  ximage->data = (char *)xdata;
  
  bitsperpixel = ximage->bits_per_pixel;
  bitsperpixelmask = (1 << bitsperpixel) - 1;
  bytesperline = ximage->bytes_per_line;
  
  img = gfi->img;
  
  /* The main loop */
  for (j = 0; j < gfi->height; j++) {
    int imshift = 0;
    unsigned long impixel = 0;
    byte *writer = xdata + bytesperline * j;
    
    for (i = 0; i < gfi->width; i++) {
      
      unsigned long pixel;
      if (img[j][i] < nct)
	pixel = ct[ img[j][i] ].pixel & bitsperpixelmask;
      else
	pixel = ct[0].pixel & bitsperpixelmask;
      
      impixel |= SAFELS(pixel, imshift);
      while (imshift + bitsperpixel >= BYTESIZE) {
	*writer++ = impixel;
	imshift -= BYTESIZE;
	impixel = SAFELS(pixel, imshift);
      }
      imshift += bitsperpixel;
    }
    
    if (imshift)
      *writer++ = impixel;
  }
  
  /* Restore saved transparent pixel value */
  if (gfi->transparent > -1 && gfi->transparent < nct)
    ct[ gfi->transparent ].pixel = savedtransparent;

  /* Create the pixmap */
  pixmap =
    XCreatePixmap(gfx->display, gfx->drawable,
		  gfi->width, gfi->height, gfx->depth);
  if (pixmap) {
    GC gc;
    gc = XCreateGC(gfx->display, pixmap, 0, 0);
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
  return Gif_XImageColormap(gfx, gfs, Gif_GetColormap(gfs, 0), gfi);
}



Pixmap
Gif_XMask(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi)
{
  Pixmap pixmap = None;
  XImage *ximage;

  byte *xdata;
  byte **img;
  
  int i, j;
  int transparent;
  int bytesperline;

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
  xdata = Gif_NewArray(byte, ximage->bytes_per_line * gfi->height);
  ximage->data = (char *)xdata;
  bytesperline = ximage->bytes_per_line;
  
  img = gfi->img;
  transparent = gfi->transparent;
  
  /* The main loop */
  for (j = 0; j < gfi->height; j++) {
    int imshift = 0;
    unsigned long impixel = 0;
    byte *writer = xdata + bytesperline * j;
    
    for (i = 0; i < gfi->width; i++) {
      if (img[j][i] == transparent)
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
    GC gc;
    gc = XCreateGC(gfx->display, pixmap, 0, 0);
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
  allocatecolors(gfx, gfcm->ncol, gfcm->col);
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
  
#if 0
  {
    int vclass;
#if defined(__cplusplus) || defined(c_plusplus)
    vclass = gfx->visual->c_class;
#else
    vclass = gfx->visual->class;
#endif
    
    if (gfx->depth == 1)
      gfx->colormodel = GIF_COLORMODEL_MONO;
    else if (vclass == StaticGray || vclass == GrayScale)
      gfx->colormodel = GIF_COLORMODEL_GRAY;
    else
      gfx->colormodel = GIF_COLORMODEL_COLOR;
  }
#endif
  
  gfx->closest = 0;
  gfx->nclosest = 0;
  
  gfx->transparentvalue = 0;
  
  return gfx;
}


#ifdef __cplusplus
}
#endif
