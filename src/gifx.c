/* gifx.c - Functions to turn GIFs in memory into X Pixmaps.
   Copyright (C) 1997-9 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*. It is distributed under the GNU Public
   License, version 2 or later; you can copy, distribute, or alter it at will,
   as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied.

   *The LZW compression method used by GIFs is patented. Unisys, the patent
   holder, allows the compression algorithm to be used without a license in
   software distributed at no cost to the user. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "gifx.h"
#include <X11/Xutil.h>
#include <assert.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SAFELS(a,b) ((b) < 0 ? (a) >> -(b) : (a) << (b))

struct Gif_XColormap {
  
  Gif_XContext *x_context;
  Gif_Colormap *colormap;
  
  int allocated;
  int claimed;
  u_int16_t npixels;
  unsigned long *pixels;
  
  Gif_XColormap *next;
  
};

static unsigned long crap_pixels[256];


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


static unsigned long
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
  
  return got->pixel;
}


static void
allocate_colors(Gif_XColormap *gfxc)
{
  Gif_XContext *gfx = gfxc->x_context;
  u_int16_t size = gfxc->colormap->ncol;
  Gif_Color *c = gfxc->colormap->col;
  unsigned long *pixels = gfxc->pixels;
  XColor xcol;
  int i;
  if (!gfxc->allocated) {
    if (size > gfxc->npixels) size = gfxc->npixels;
    for (i = 0; i < size; i++, c++) {
      xcol.red = c->red | (c->red << 8);
      xcol.green = c->green | (c->green << 8);
      xcol.blue = c->blue | (c->blue << 8);
      if (XAllocColor(gfx->display, gfx->colormap, &xcol))
	pixels[i] = xcol.pixel;
      else
	pixels[i] = allocate_closest(gfx, c);
    }
    gfxc->allocated = 1;
    gfxc->claimed = 0;
  }
}

static void
deallocate_colors(Gif_XColormap *gfxc)
{
  Gif_XContext *gfx = gfxc->x_context;
  if (gfxc->allocated && !gfxc->claimed) {
    XFreeColors(gfx->display, gfx->colormap, gfxc->pixels, gfxc->npixels, 0);
    gfxc->allocated = 0;
  }
}


static Gif_XColormap *
create_x_colormap_extension(Gif_XContext *gfx, Gif_Colormap *gfcm)
{
  Gif_XColormap *gfxc;
  unsigned long *pixels;
  if (!gfcm) return 0;
  gfxc = Gif_New(Gif_XColormap);
  pixels = gfxc ? Gif_NewArray(unsigned long, gfcm->ncol) : 0;
  if (pixels) {
    gfxc->x_context = gfx;
    gfxc->colormap = gfcm;
    gfxc->allocated = 0;
    gfxc->npixels = gfcm->ncol;
    gfxc->pixels = pixels;
    gfxc->next = gfx->xcolormap;
    gfx->xcolormap = gfxc;
    return gfxc;
  } else {
    Gif_Delete(gfxc);
    Gif_DeleteArray(pixels);
    return 0;
  }
}

static Gif_XColormap *
find_x_colormap_extension(Gif_XContext *gfx, Gif_Colormap *gfcm, int create)
{
  Gif_XColormap *gfxc = gfx->xcolormap;
  if (!gfcm) return 0;
  while (gfxc) {
    if (gfxc->colormap == gfcm)
      return gfxc;
    gfxc = gfxc->next;
  }
  if (create)
    return create_x_colormap_extension(gfx, gfcm);
  else
    return 0;
}

int
Gif_XAllocateColors(Gif_XContext *gfx, Gif_Colormap *gfcm)
{
  Gif_XColormap *gfxc = find_x_colormap_extension(gfx, gfcm, 1);
  if (gfxc) {
    allocate_colors(gfxc);
    return 1;
  } else
    return 0;
}

void
Gif_XDeallocateColors(Gif_XContext *gfx, Gif_Colormap *gfcm)
{
  Gif_XColormap *gfxc = find_x_colormap_extension(gfx, gfcm, 0);
  if (gfxc)
    deallocate_colors(gfxc);
}


unsigned long *
Gif_XClaimStreamColors(Gif_XContext *gfx, Gif_Stream *gfs, int *np_store)
{
  int i;
  int npixels = 0;
  unsigned long *pixels;
  Gif_Colormap *global = gfs->global;
  *np_store = 0;
  
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    Gif_Colormap *gfcm = (gfi->local ? gfi->local : global);
    Gif_XColormap *gfxc = find_x_colormap_extension(gfx, gfcm, 0);
    if (gfxc && gfxc->allocated && gfxc->claimed == 0) {
      gfxc->claimed = 2;
      npixels += gfxc->npixels;
      if (gfcm == global) global = 0;
    }
  }
  
  if (!npixels) return 0;
  
  pixels = Gif_NewArray(unsigned long, npixels);
  if (!pixels) return 0;
  *np_store = npixels;

  npixels = 0;
  global = gfs->global;
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    Gif_Colormap *gfcm = (gfi->local ? gfi->local : global);
    Gif_XColormap *gfxc = find_x_colormap_extension(gfx, gfcm, 0);
    if (gfxc && gfxc->allocated && gfxc->claimed == 2) {
      memcpy(pixels + npixels, gfxc->pixels, gfxc->npixels);
      npixels += gfxc->npixels;
      gfxc->claimed = 1;
      if (gfcm == global) global = 0;
    }
  }

  return pixels;
}


/* Getting pixmaps */

#define BYTESIZE 8

Pixmap
Gif_XSubImageColormap(Gif_XContext *gfx, Gif_Stream *gfs,
		      Gif_Colormap *gfcm, Gif_Image *gfi,
		      int left, int top, int width, int height)
{
  Pixmap pixmap = None;
  XImage *ximage;
  byte *xdata;
  
  int i, j, k;
  int bytes_per_line;
  
  unsigned long saved_transparent = 0;
  int release_uncompressed = 0;
  u_int16_t nct;
  unsigned long *pixels;
  
  /* Find the correct image and colormap */
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) return None;

  /* Make sure the image is uncompressed */
  if (!gfi->img && !gfi->image_data && gfi->compressed) {
    Gif_UncompressImage(gfi);
    release_uncompressed = 1;
  }
  
  /* Check subimage dimensions */
  if (width <= 0 || height <= 0 || left < 0 || top < 0
      || left+width <= 0 || top+height <= 0
      || left+width > gfi->width || top+height > gfi->height)
    return None;
  
  /* Allocate colors from the colormap; make sure the transparent color
   * has the given pixel value */
  if (gfcm) {
    Gif_XColormap *gfxc = find_x_colormap_extension(gfx, gfcm, 1);
    if (!gfxc) return None;
    allocate_colors(gfxc);
    pixels = gfxc->pixels;
    nct = gfxc->npixels;
  } else {
    for (i = 0; i < 256; i++) crap_pixels[i] = gfx->foreground_pixel;
    pixels = crap_pixels;
    nct = 256;
  }
  if (gfi->transparent > -1 && gfi->transparent < nct) {
    saved_transparent = pixels[ gfi->transparent ];
    pixels[ gfi->transparent ] = gfx->transparent_pixel;
  }
  
  /* Set up the X image */
  if (gfx->depth <= 8) i = 8;
  else if (gfx->depth <= 16) i = 16;
  else i = 32;
  ximage =
    XCreateImage(gfx->display, gfx->visual, gfx->depth,
		 gfx->depth == 1 ? XYBitmap : ZPixmap, 0, NULL,
		 width, height, i, 0);
  
  ximage->bitmap_bit_order = ximage->byte_order = LSBFirst;
  bytes_per_line = ximage->bytes_per_line;
  xdata = Gif_NewArray(byte, bytes_per_line * height);
  ximage->data = (char *)xdata;
  
  /* The main loop */
  if (ximage->bits_per_pixel % 8 == 0) {
    /* Optimize for cases where a pixel is exactly one or more bytes */
    int bytes_per_pixel = ximage->bits_per_pixel / 8;
    
    for (j = 0; j < height; j++) {
      byte *line = gfi->img[top + j] + left;
      byte *writer = xdata + bytes_per_line * j;
      for (i = 0; i < width; i++) {
	unsigned long pixel;
	if (line[i] < nct)
	  pixel = pixels[line[i]];
	else
	  pixel = pixels[0];
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
    
    for (j = 0; j < height; j++) {
      int imshift = 0;
      u_int32_t impixel = 0;
      byte *line = gfi->img[top + j] + left;
      byte *writer = xdata + bytes_per_line * j;
      
      for (i = 0; i < width; i++) {
	unsigned long pixel;
	if (line[i] < nct)
	  pixel = pixels[line[i]];
	else
	  pixel = pixels[0];
	
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
    pixels[ gfi->transparent ] = saved_transparent;

  /* Create the pixmap */
  pixmap =
    XCreatePixmap(gfx->display, gfx->drawable, width, height, gfx->depth);
  if (!gfx->image_gc)
    gfx->image_gc = XCreateGC(gfx->display, pixmap, 0, 0);
  
  if (pixmap && gfx->image_gc)
    XPutImage(gfx->display, pixmap, gfx->image_gc, ximage, 0, 0, 0, 0,
	      width, height);
  
  Gif_DeleteArray(xdata);
  ximage->data = 0; /* avoid freeing it again in XDestroyImage */  
  XDestroyImage(ximage);

  if (release_uncompressed)
    Gif_ReleaseUncompressedImage(gfi);
  
  return pixmap;
}


Pixmap
Gif_XImage(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi)
{
  Gif_Colormap *gfcm;
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) return None;
  gfcm = gfi->local;
  if (!gfcm) gfcm = gfs->global;
  return Gif_XSubImageColormap(gfx, gfs, gfcm, gfi,
			       0, 0, gfi->width, gfi->height);
}


Pixmap
Gif_XImageColormap(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Colormap *gfcm,
		   Gif_Image *gfi)
{
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) return None;
  return Gif_XSubImageColormap(gfx, gfs, gfcm, gfi,
			       0, 0, gfi->width, gfi->height);
}


Pixmap
Gif_XSubImage(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi,
	      int left, int top, int width, int height)
{
  Gif_Colormap *gfcm;
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) return None;
  gfcm = gfi->local;
  if (!gfcm) gfcm = gfs->global;
  return Gif_XSubImageColormap(gfx, gfs, gfcm, gfi,
			       left, top, width, height);
}


Pixmap
Gif_XSubMask(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi,
	     int left, int top, int width, int height)
{
  Pixmap pixmap = None;
  XImage *ximage;
  byte *xdata;
  
  int i, j;
  int transparent;
  int bytes_per_line;
  int release_uncompressed = 0;
  
  /* Find the correct image */
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) return None;
  
  /* Check subimage dimensions */
  if (width <= 0 || height <= 0 || left < 0 || top < 0
      || left+width <= 0 || top+height <= 0
      || left+width > gfi->width || top+height > gfi->height)
    return None;
  
  /* Make sure the image is uncompressed */
  if (!gfi->img && !gfi->image_data && gfi->compressed) {
    Gif_UncompressImage(gfi);
    release_uncompressed = 1;
  }
  
  /* Create the X image */
  ximage =
    XCreateImage(gfx->display, gfx->visual, 1,
		 XYBitmap, 0, NULL,
		 width, height,
		 8, 0);
  
  ximage->bitmap_bit_order = ximage->byte_order = LSBFirst;
  bytes_per_line = ximage->bytes_per_line;
  xdata = Gif_NewArray(byte, bytes_per_line * height);
  ximage->data = (char *)xdata;
  
  transparent = gfi->transparent;
  
  /* The main loop */
  for (j = 0; j < height; j++) {
    int imshift = 0;
    u_int32_t impixel = 0;
    byte *line = gfi->img[top + j] + left;
    byte *writer = xdata + bytes_per_line * j;
    
    for (i = 0; i < width; i++) {
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
    XCreatePixmap(gfx->display, gfx->drawable, width, height, 1);
  if (!gfx->mask_gc)
    gfx->mask_gc = XCreateGC(gfx->display, pixmap, 0, 0);
  
  if (pixmap && gfx->mask_gc)
    XPutImage(gfx->display, pixmap, gfx->mask_gc, ximage, 0, 0, 0, 0,
	      width, height);
  
  Gif_DeleteArray(xdata);
  ximage->data = 0; /* avoid freeing it again in XDestroyImage */  
  XDestroyImage(ximage);

  if (release_uncompressed)
    Gif_ReleaseUncompressedImage(gfi);
  
  return pixmap;
}


Pixmap
Gif_XMask(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi)
{
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) return None;
  return Gif_XSubMask(gfx, gfs, gfi, 0, 0, gfi->width, gfi->height);
}


/** CREATING AND DESTROYING XCONTEXTS **/

static void
delete_xcolormap(Gif_XColormap *gfxc)
{
  Gif_XContext *gfx = gfxc->x_context;
  Gif_XColormap *prev = 0, *trav = gfx->xcolormap;
  while (trav != gfxc && trav) {
    prev = trav;
    trav = trav->next;
  }
  if (gfx->free_deleted_colormap_pixels)
    deallocate_colors(gfxc);
  if (prev) prev->next = gfxc->next;
  else gfx->xcolormap = gfxc->next;
  Gif_DeleteArray(gfxc->pixels);
  Gif_Delete(gfxc);
}

static void
delete_colormap_hook(int dummy, void *colormap_x, void *callback_x)
{
  Gif_Colormap *gfcm = (Gif_Colormap *)colormap_x;
  Gif_XContext *gfx = (Gif_XContext *)callback_x;
  Gif_XColormap *gfxc;
  for (gfxc = gfx->xcolormap; gfxc; gfxc = gfxc->next)
    if (gfxc->colormap == gfcm) {
      delete_xcolormap(gfxc);
      return;
    }
}


Gif_XContext *
Gif_NewXContextFromVisual(Display *display, int screen_number,
			  Visual *visual, int depth, Colormap colormap)
{
  Gif_XContext *gfx;
  
  gfx = Gif_New(Gif_XContext);
  gfx->display = display;
  gfx->screen_number = screen_number;
  gfx->drawable = RootWindow(display, screen_number);
  
  gfx->visual = visual;
  gfx->colormap = colormap;
  gfx->ncolormap = visual->map_entries;
  gfx->depth = depth;
  
  gfx->closest = 0;
  gfx->nclosest = 0;

  gfx->free_deleted_colormap_pixels = 0;
  gfx->xcolormap = 0;

  gfx->image_gc = None;
  gfx->mask_gc = None;
  
  gfx->transparent_pixel = 0UL;
  gfx->foreground_pixel = 1UL;
  gfx->refcount = 0;

  Gif_AddDeletionHook(GIF_T_COLORMAP, delete_colormap_hook, gfx);
  return gfx;
}


Gif_XContext *
Gif_NewXContext(Display *display, Window window)
{
  XWindowAttributes attr;
  XGetWindowAttributes(display, window, &attr);
  return Gif_NewXContextFromVisual(display, XScreenNumberOfScreen(attr.screen),
				   attr.visual, attr.depth, attr.colormap);
}


void
Gif_DeleteXContext(Gif_XContext *gfx)
{
  if (!gfx) return;
  if (--gfx->refcount > 0) return;
  while (gfx->xcolormap)
    delete_xcolormap(gfx->xcolormap);
  if (gfx->image_gc)
    XFreeGC(gfx->display, gfx->image_gc);
  if (gfx->mask_gc)
    XFreeGC(gfx->display, gfx->mask_gc);
  Gif_DeleteArray(gfx->closest);
  Gif_Delete(gfx);
  Gif_RemoveDeletionHook(GIF_T_COLORMAP, delete_colormap_hook, gfx);
}


#ifdef __cplusplus
}
#endif
