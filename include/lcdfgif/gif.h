#ifndef GIF_H
#define GIF_H
#ifdef __cplusplus
extern "C" {
#endif

/* gif.h - Public interface to the GIF library.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software; you can copy, distribute, or alter it at
   will, as long as this notice is kept intact and this source code is made
   available. Hypo(pa)thetical commerical developers are asked to write the
   author a note, which might make his day. There is no warranty, express or
   implied. */


#include <stdio.h>

#ifndef BYTE
#define BYTE
typedef unsigned char byte;
#endif
#ifndef INT16
#define INT16 short
#endif
#ifndef UINT16
#define UINT16 unsigned INT16
#endif
#ifndef INT32
#define INT32 int
#endif
#ifndef UINT32
#define UINT32 unsigned INT32
#endif


typedef struct {
  
  byte haspixel;
  byte red;
  byte green;
  byte blue;
  
  UINT32 pixel;
  
} Gif_Color;


typedef struct {
  
  unsigned char *data;
  UINT32 length;
  
} Gif_Record;


typedef struct Gif_Stream	Gif_Stream;
typedef struct Gif_Image	Gif_Image;
typedef struct Gif_Colormap	Gif_Colormap;
typedef struct Gif_Comment	Gif_Comment;


Gif_Stream *		Gif_ReadFile(FILE *);
int 			Gif_WriteFile(Gif_Stream *, FILE *);

Gif_Stream *		Gif_ReadRecord(Gif_Record *);

void			Gif_DeleteStream(Gif_Stream *);
void			Gif_DeleteColormap(Gif_Colormap *);

Gif_Stream *		Gif_CopyStreamSkeleton(Gif_Stream *);
Gif_Colormap *		Gif_CopyColormap(Gif_Colormap *);
Gif_Image *		Gif_CopyImage(Gif_Image *);

Gif_Color *		Gif_GetBackground(Gif_Stream *, Gif_Colormap *);
Gif_Colormap *		Gif_GetColormap(Gif_Stream *, int);
Gif_Colormap *		Gif_GetNamedColormap(Gif_Stream *, const char *);

int			Gif_ScreenWidth(Gif_Stream *);
int			Gif_ScreenHeight(Gif_Stream *);

int			Gif_ImageCount(Gif_Stream *);
Gif_Image *		Gif_GetImage(Gif_Stream *, int);
Gif_Image *		Gif_GetNamedImage(Gif_Stream *, const char *);

int			Gif_ImageWidth(Gif_Image *);
int			Gif_ImageHeight(Gif_Image *);
int			Gif_ImageNumber(Gif_Stream *, Gif_Image *);
UINT16			Gif_ImageDelay(Gif_Image *);
void *			Gif_ImageUserData(Gif_Image *);
void			Gif_SetImageUserData(Gif_Image *, void *);

void			Gif_Unoptimize(Gif_Stream *);


#ifdef __cplusplus
}
#endif
#endif
