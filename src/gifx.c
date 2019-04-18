/* gifx.c - Functions to turn GIFs in memory into X Pixmaps.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of the LCDF GIF library.

   The LCDF GIF library is free software. It is distributed under the GNU
   General Public License, version 2; you can copy, distribute, or alter it at
   will, as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied. */

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <lcdfgif/gifx.h>
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
  uint16_t npixels;
  unsigned long *pixels;

  Gif_XColormap *next;

};

static unsigned long crap_pixels[256];


static void
load_closest(Gif_XContext *gfx)
{
  XColor *color;
  uint16_t ncolor;
  uint16_t ncolormap;
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
    c->gfc_red = color[i].red >> 8;
    c->gfc_green = color[i].green >> 8;
    c->gfc_blue = color[i].blue >> 8;
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
  uint32_t distance = 0x4000000;
  int i;

  load_closest(gfx);

  for (i = 0, closer = gfx->closest; i < gfx->nclosest; i++, closer++) {
    int redd = c->gfc_red - closer->gfc_red;
    int greend = c->gfc_green - closer->gfc_green;
    int blued = c->gfc_blue - closer->gfc_blue;
    uint32_t d = redd * redd + greend * greend + blued * blued;
    if (d < distance) {
      distance = d;
      got = closer;
    }
  }

  if (!got) return 0;
  if (!got->haspixel) {
    XColor xcol;
    xcol.red = got->gfc_red | (got->gfc_red << 8);
    xcol.green = got->gfc_green | (got->gfc_green << 8);
    xcol.blue = got->gfc_blue | (got->gfc_blue << 8);
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
  uint16_t size = gfxc->colormap->ncol;
  Gif_Color *c = gfxc->colormap->col;
  unsigned long *pixels = gfxc->pixels;
  XColor xcol;
  int i;
  if (!gfxc->allocated) {
    if (size > gfxc->npixels) size = gfxc->npixels;
    for (i = 0; i < size; i++, c++) {
      xcol.red = c->gfc_red | (c->gfc_red << 8);
      xcol.green = c->gfc_green | (c->gfc_green << 8);
      xcol.blue = c->gfc_blue | (c->gfc_blue << 8);
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
  pixels = gfxc ? Gif_NewArray(unsigned long, 256) : 0;
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

static int
put_sub_image_colormap(Gif_XContext *gfx,
                       Gif_Stream* gfs, Gif_Image *gfi, Gif_Colormap *gfcm,
		       int left, int top, int width, int height,
		       Pixmap pixmap, int pixmap_x, int pixmap_y)
{
  XImage *ximage;
  uint8_t *xdata;

  int i, j, k;
  size_t bytes_per_line;

  unsigned long saved_transparent = 0;
  int release_uncompressed = 0;
  uint16_t nct;
  unsigned long *pixels;

  /* Find the correct image and colormap */
  if (!gfi) return 0;
  if (!gfx->image_gc)
    gfx->image_gc = XCreateGC(gfx->display, pixmap, 0, 0);
  if (!gfx->image_gc)
    return 0;

  /* Make sure the image is uncompressed */
  if (!gfi->img && !gfi->image_data && gfi->compressed) {
      Gif_UncompressImage(gfs, gfi);
      release_uncompressed = 1;
  }

  /* Check subimage dimensions */
  if (width <= 0 || height <= 0 || left < 0 || top < 0
      || left+width <= 0 || top+height <= 0
      || left+width > gfi->width || top+height > gfi->height)
    return 0;

  /* Allocate colors from the colormap; make sure the transparent color
   * has the given pixel value */
  if (gfcm) {
    Gif_XColormap *gfxc = find_x_colormap_extension(gfx, gfcm, 1);
    if (!gfxc) return 0;
    allocate_colors(gfxc);
    pixels = gfxc->pixels;
    nct = gfxc->npixels;
  } else {
    for (i = 0; i < 256; i++) crap_pixels[i] = gfx->foreground_pixel;
    pixels = crap_pixels;
    nct = 256;
  }
  if (gfi->transparent >= 0 && gfi->transparent < 256) {
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
  xdata = Gif_NewArray(uint8_t, bytes_per_line * height);
  ximage->data = (char *)xdata;

  /* The main loop */
  if (ximage->bits_per_pixel % 8 == 0) {
    /* Optimize for cases where a pixel is exactly one or more bytes */
    int bytes_per_pixel = ximage->bits_per_pixel / 8;

    for (j = 0; j < height; j++) {
      uint8_t *line = gfi->img[top + j] + left;
      uint8_t *writer = xdata + bytes_per_line * j;
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
    uint32_t bits_per_pixel_mask = (1UL << bits_per_pixel) - 1;

    for (j = 0; j < height; j++) {
      int imshift = 0;
      uint32_t impixel = 0;
      uint8_t *line = gfi->img[top + j] + left;
      uint8_t *writer = xdata + bytes_per_line * j;

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
  if (gfi->transparent >= 0 && gfi->transparent < 256)
    pixels[ gfi->transparent ] = saved_transparent;

  /* Put it onto the pixmap */
  XPutImage(gfx->display, pixmap, gfx->image_gc, ximage, 0, 0,
	    pixmap_x, pixmap_y, width, height);

  Gif_DeleteArray(xdata);
  ximage->data = 0; /* avoid freeing it again in XDestroyImage */
  XDestroyImage(ximage);

  if (release_uncompressed)
    Gif_ReleaseUncompressedImage(gfi);

  return 1;
}


Pixmap
Gif_XSubImageColormap(Gif_XContext *gfx,
                      Gif_Stream* gfs, Gif_Image *gfi, Gif_Colormap *gfcm,
		      int left, int top, int width, int height)
{
  Pixmap pixmap =
    XCreatePixmap(gfx->display, gfx->drawable,
                  width ? width : 1, height ? height : 1, gfx->depth);
  if (pixmap) {
    if (put_sub_image_colormap(gfx, gfs, gfi, gfcm, left, top, width, height,
			       pixmap, 0, 0))
      return pixmap;
    else
      XFreePixmap(gfx->display, pixmap);
  }
  return None;
}

Pixmap
Gif_XImage(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi)
{
  Gif_Colormap *gfcm;
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) return None;
  gfcm = gfi->local;
  if (!gfcm) gfcm = gfs->global;
  return Gif_XSubImageColormap(gfx, gfs, gfi, gfcm,
			       0, 0, gfi->width, gfi->height);
}

Pixmap
Gif_XImageColormap(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Colormap *gfcm,
		   Gif_Image *gfi)
{
  if (!gfi && gfs->nimages) gfi = gfs->images[0];
  if (!gfi) return None;
  return Gif_XSubImageColormap(gfx, gfs, gfi, gfcm,
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
  return Gif_XSubImageColormap(gfx, gfs, gfi, gfcm,
			       left, top, width, height);
}


Pixmap
Gif_XSubMask(Gif_XContext* gfx, Gif_Stream* gfs, Gif_Image* gfi,
	     int left, int top, int width, int height)
{
  Pixmap pixmap = None;
  XImage *ximage;
  uint8_t *xdata;

  int i, j;
  int transparent;
  size_t bytes_per_line;
  int release_uncompressed = 0;

  /* Find the correct image */
  if (!gfi) return None;

  /* Check subimage dimensions */
  if (width <= 0 || height <= 0 || left < 0 || top < 0
      || left+width <= 0 || top+height <= 0
      || left+width > gfi->width || top+height > gfi->height)
    return None;

  /* Make sure the image is uncompressed */
  if (!gfi->img && !gfi->image_data && gfi->compressed) {
      Gif_UncompressImage(gfs, gfi);
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
  xdata = Gif_NewArray(uint8_t, bytes_per_line * height);
  ximage->data = (char *)xdata;

  transparent = gfi->transparent;

  /* The main loop */
  for (j = 0; j < height; j++) {
    int imshift = 0;
    uint32_t impixel = 0;
    uint8_t *line = gfi->img[top + j] + left;
    uint8_t *writer = xdata + bytes_per_line * j;

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
    XCreatePixmap(gfx->display, gfx->drawable,
                  width ? width : 1, height ? height : 1, 1);
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

static Pixmap
screen_pixmap(Gif_XContext *gfx, Gif_Stream *gfs)
{
  return XCreatePixmap(gfx->display, gfx->drawable,
		       gfs->screen_width, gfs->screen_height, gfx->depth);
}

static int
apply_background(Gif_XContext *gfx, Gif_Stream *gfs, int i, Pixmap pixmap)
{
  Gif_Image *gfi = gfs->images[i >= 0 ? i : 0];
  Gif_Colormap *gfcm = (gfi->local ? gfi->local : gfs->global);
  unsigned long bg_pixel;

  /* find bg_pixel */
  if (gfs->global && gfs->background < gfs->global->ncol
      && gfs->images[0]->transparent < 0) {
    Gif_XColormap *gfxc = find_x_colormap_extension(gfx, gfcm, 1);
    if (!gfxc)
      return -1;
    allocate_colors(gfxc);
    bg_pixel = gfxc->pixels[gfs->background];
  } else
    bg_pixel = gfx->transparent_pixel;

  /* install it as the foreground color on gfx->image_gc */
  if (!gfx->image_gc)
    gfx->image_gc = XCreateGC(gfx->display, pixmap, 0, 0);
  if (!gfx->image_gc)
    return -1;
  XSetForeground(gfx->display, gfx->image_gc, bg_pixel);
  gfx->transparent_pixel = bg_pixel;

  /* clear the image portion */
  if (i < 0)
    XFillRectangle(gfx->display, pixmap, gfx->image_gc,
		   0, 0, gfs->screen_width, gfs->screen_height);
  else /*if (gfi->transparent < 0)*/
    XFillRectangle(gfx->display, pixmap, gfx->image_gc,
		   gfi->left, gfi->top, gfi->width, gfi->height);
  /*else {
    Pixmap mask = Gif_XMask(gfx, gfs, gfi);
    if (mask == None)
      return -1;
    XSetClipMask(gfx->display, gfx->image_gc, mask);
    XSetClipOrigin(gfx->display, gfx->image_gc, gfi->left, gfi->top);
    XFillRectangle(gfx->display, pixmap, gfx->image_gc,
		   gfi->left, gfi->top, gfi->width, gfi->height);
    XSetClipMask(gfx->display, gfx->image_gc, None);
    XFreePixmap(gfx->display, mask);
  }*/

  return 0;
}

static int
apply_image(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi, Pixmap pixmap)
{
  Pixmap image = Gif_XImage(gfx, gfs, gfi), mask;
  if (image == None)
    return -1;

  if (gfi->transparent >= 0) {
    mask = Gif_XMask(gfx, gfs, gfi);
    if (mask == None) {
      XFreePixmap(gfx->display, image);
      return -1;
    }

    XSetClipMask(gfx->display, gfx->image_gc, mask);
    XSetClipOrigin(gfx->display, gfx->image_gc, gfi->left, gfi->top);
    XCopyArea(gfx->display, image, pixmap, gfx->image_gc,
	      0, 0, gfi->width, gfi->height, gfi->left, gfi->top);
    XSetClipMask(gfx->display, gfx->image_gc, None);
    XFreePixmap(gfx->display, mask);
  } else {
    XCopyArea(gfx->display, image, pixmap, gfx->image_gc,
	      0, 0, gfi->width, gfi->height, gfi->left, gfi->top);
  }

  XFreePixmap(gfx->display, image);
  return 0;
}

static int
fullscreen(Gif_Stream *gfs, Gif_Image *gfi, int require_opaque)
{
  return (gfi->left == 0 && gfi->top == 0 && gfi->width == gfs->screen_width
	  && gfi->height == gfs->screen_height
	  && (!require_opaque || gfi->transparent < 0));
}

Gif_XFrame *
Gif_NewXFrames(Gif_Stream *gfs)
{
  int i, last_postdisposal = -1;
  Gif_XFrame *fs = Gif_NewArray(Gif_XFrame, gfs->nimages);
  if (!fs)
    return 0;
  for (i = 0; i < gfs->nimages; ++i) {
    Gif_Image *gfi = gfs->images[i];
    fs[i].pixmap = None;
    if (gfi->disposal == GIF_DISPOSAL_PREVIOUS)
      fs[i].postdisposal = last_postdisposal;
    else
      fs[i].postdisposal = i;
    last_postdisposal = fs[i].postdisposal;
  }
  return fs;
}

void
Gif_DeleteXFrames(Gif_XContext *gfx, Gif_Stream *gfs, Gif_XFrame *fs)
{
  int i;
  for (i = 0; i < gfs->nimages; ++i)
    if (fs[i].pixmap)
      XFreePixmap(gfx->display, fs[i].pixmap);
  Gif_DeleteArray(fs);
}

Pixmap
Gif_XNextImage(Gif_XContext *gfx, Gif_Stream *gfs, int i, Gif_XFrame *frames)
{
  Pixmap result = None;
  unsigned long old_transparent = gfx->transparent_pixel;
  Gif_Image *gfi;
  int previ, scani;

  /* return already rendered pixmap if any */
  if (frames[i].pixmap != None)
    return frames[i].pixmap;

  /* render fullscreen image */
  gfi = gfs->images[i];
  if (fullscreen(gfs, gfi, 1)) {
    frames[i].pixmap = Gif_XImage(gfx, gfs, gfi);
    return frames[i].pixmap;
  }

  /* image is not full screen, need to find background */
  previ = i - 1;
  if (previ >= 0)
    previ = frames[previ].postdisposal;

  /* scan backwards for a renderable image */
  scani = previ;
  while (scani >= 0
	 && frames[scani].pixmap == None
	 && !fullscreen(gfs, gfs->images[scani], 1))
    --scani;

  /* create the pixmap */
  result = screen_pixmap(gfx, gfs);
  if (result == None)
    return None;

  /* scan forward to produce background */
  gfi = (scani >= 0 ? gfs->images[scani] : 0);
  if (gfi && (gfi->disposal != GIF_DISPOSAL_BACKGROUND
	      || !fullscreen(gfs, gfi, 1))) {
    /* perhaps we need to create an image (if so, must be fullscreen) */
    if (frames[scani].pixmap == None) {
      frames[scani].pixmap = Gif_XImage(gfx, gfs, gfi);
      if (frames[scani].pixmap == None)
	goto error_exit;
    }
    XCopyArea(gfx->display, frames[scani].pixmap, result, gfx->image_gc,
	      0, 0, gfs->screen_width, gfs->screen_height, 0, 0);
  }
  if (!gfi || gfi->disposal == GIF_DISPOSAL_BACKGROUND) {
    if (apply_background(gfx, gfs, scani, result) < 0)
      goto error_exit;
  }

  while (scani < previ) {
    ++scani;
    gfi = gfs->images[scani];
    if (gfi->disposal == GIF_DISPOSAL_BACKGROUND) {
      if (apply_background(gfx, gfs, scani, result) < 0)
	goto error_exit;
    } else if (gfi->disposal != GIF_DISPOSAL_PREVIOUS) {
      if (apply_image(gfx, gfs, gfs->images[scani], result) < 0)
	goto error_exit;
    }
  }

  /* apply image */
  if (gfs->screen_width != 0 && gfs->screen_height != 0) {
    if (apply_image(gfx, gfs, gfs->images[i], result) < 0)
      goto error_exit;
  }

  frames[i].pixmap = result;
  return frames[i].pixmap;

 error_exit:
  XFreePixmap(gfx->display, result);
  gfx->transparent_pixel = old_transparent;
  return None;
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
  if (prev)
    prev->next = gfxc->next;
  else
    gfx->xcolormap = gfxc->next;
  Gif_DeleteArray(gfxc->pixels);
  Gif_Delete(gfxc);
}

static void
delete_colormap_hook(int dummy, void *colormap_x, void *callback_x)
{
  Gif_Colormap *gfcm = (Gif_Colormap *)colormap_x;
  Gif_XContext *gfx = (Gif_XContext *)callback_x;
  Gif_XColormap *gfxc;
  (void) dummy;
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
  Gif_RemoveDeletionHook(GIF_T_COLORMAP, delete_colormap_hook, gfx);
  Gif_Delete(gfx);
}


#ifdef __cplusplus
}
#endif
