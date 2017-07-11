#ifndef LCDF_GIFX_H
#define LCDF_GIFX_H
#include <lcdfgif/gif.h>
#ifdef __cplusplus
extern "C" {
#endif

/* gifx.h - Functions to turn GIFs in memory into X Pixmaps.
   Copyright (C) 1997-2017 Eddie Kohler, ekohler@gmail.com
   This file is part of the LCDF GIF library.

   The LCDF GIF library is free software*. It is distributed under the GNU
   General Public License, version 2; you can copy, distribute, or
   alter it at will, as long as this notice is kept intact and this source
   code is made available. There is no warranty, express or implied. */

#include <X11/Xlib.h>

#define GIFX_COLORMAP_EXTENSION -107


typedef struct Gif_XContext Gif_XContext;
typedef struct Gif_XColormap Gif_XColormap;
typedef struct Gif_XFrame Gif_XFrame;

struct Gif_XContext {
    Display *display;
    int screen_number;
    Drawable drawable;
    Visual *visual;
    uint16_t depth;
    uint16_t ncolormap;
    Colormap colormap;

    uint16_t nclosest;
    Gif_Color *closest;

    int free_deleted_colormap_pixels;
    Gif_XColormap *xcolormap;

    GC image_gc;
    GC mask_gc;

    unsigned long transparent_pixel;
    unsigned long foreground_pixel;
    int refcount;
};

struct Gif_XFrame {
    Pixmap pixmap;
    int postdisposal;
    int user_data;
};

Gif_XContext *	Gif_NewXContext(Display *display, Window window);
Gif_XContext *	Gif_NewXContextFromVisual(Display *display, int screen_number,
				Visual *visual, int depth, Colormap cmap);
void		Gif_DeleteXContext(Gif_XContext *gfx);

Pixmap		Gif_XImage(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi);
Pixmap		Gif_XImageColormap(Gif_XContext *gfx, Gif_Stream *gfs,
				Gif_Colormap *gfcm, Gif_Image *gfi);
Pixmap		Gif_XSubImage(Gif_XContext *gfx, Gif_Stream *gfs,
				Gif_Image *gfi, int l, int t, int w, int h);
Pixmap		Gif_XSubImageColormap(Gif_XContext *gfx,
                                Gif_Stream* gfs, Gif_Image *gfi,
				Gif_Colormap* gfcm, int l, int t, int w, int h);

Pixmap		Gif_XMask(Gif_XContext *gfx, Gif_Stream *gfs, Gif_Image *gfi);
Pixmap		Gif_XSubMask(Gif_XContext* gfx, Gif_Stream* gfs, Gif_Image* gfi,
				int l, int t, int w, int h);

Gif_XFrame *	Gif_NewXFrames(Gif_Stream *gfs);
void		Gif_DeleteXFrames(Gif_XContext *gfx, Gif_Stream *gfs,
				Gif_XFrame *frames);
Pixmap		Gif_XNextImage(Gif_XContext *gfx, Gif_Stream *gfs, int i,
				Gif_XFrame *frames);

int		Gif_XAllocateColors(Gif_XContext *gfx, Gif_Colormap *gfcm);
void		Gif_XDeallocateColors(Gif_XContext *gfx, Gif_Colormap *gfcm);
unsigned long *	Gif_XClaimStreamColors(Gif_XContext *gfx, Gif_Stream *gfs,
				int *np_store);


#ifdef __cplusplus
}
#endif
#endif
