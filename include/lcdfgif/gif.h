#ifndef GIF_H /* -*- mode: c -*- */
#define GIF_H
#ifdef __cplusplus
extern "C" {
#endif

/* gif.h - Interface to the GIF library.
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

#include <stdio.h>
#include <stdlib.h>

#define GIF_MAJOR_VERSION	@GIF_MAJOR_VERSION@
#define GIF_MINOR_VERSION	@GIF_MINOR_VERSION@

#ifndef BYTE
#define BYTE
typedef unsigned char		byte;
#endif
@GIF_TYPES@

typedef struct Gif_Stream	Gif_Stream;
typedef struct Gif_Image	Gif_Image;
typedef struct Gif_Colormap	Gif_Colormap;
typedef struct Gif_Comment	Gif_Comment;
typedef struct Gif_Record	Gif_Record;


/** GIF_STREAM **/

struct Gif_Stream {
  
  Gif_Colormap *global;
  byte background;
  
  u_int16_t screen_width;
  u_int16_t screen_height;
  long loopcount;		/* -1 means no loop count */
  
  Gif_Comment *comment;
  
  Gif_Image **images;
  int nimages;
  int imagescap;
  
  unsigned errors;
  unsigned odd_extensions;
  unsigned odd_app_extensions;
  
  int userflags;
  int refcount;
  
};

Gif_Stream *	Gif_NewStream(void);
void		Gif_DeleteStream(Gif_Stream *);

Gif_Stream *	Gif_ReadFile(FILE *);
int 		Gif_WriteFile(Gif_Stream *, FILE *);
Gif_Stream *	Gif_ReadRecord(Gif_Record *);

Gif_Stream *	Gif_CopyStreamSkeleton(Gif_Stream *);

#define		Gif_ScreenWidth(gfs)		((gfs)->screen_width)
#define		Gif_ScreenHeight(gfs)		((gfs)->screen_height)
#define		Gif_ImageCount(gfs)		((gfs)->nimages)

void		Gif_Unoptimize(Gif_Stream *);


/** GIF_IMAGE **/

struct Gif_Image {
  
  char *identifier;
  Gif_Comment *comment;
  
  Gif_Colormap *local;
  short transparent;		/* -1 means no transparent index */
  
  u_int16_t delay;
  byte disposal;
  u_int16_t left;
  u_int16_t top;
  
  u_int16_t width;
  u_int16_t height;
  
  byte interlace;
  byte **img;			/* img[y][x] == image byte (x,y) */
  byte *imagedata;
  
  void *userdata;
  int refcount;
  
};

Gif_Image *	Gif_NewImage(void);
void		Gif_DeleteImage(Gif_Image *);

int		Gif_AddImage(Gif_Stream *, Gif_Image *);
Gif_Image *	Gif_CopyImage(Gif_Image *);

Gif_Image *	Gif_GetImage(Gif_Stream *, int);
Gif_Image *	Gif_GetNamedImage(Gif_Stream *, const char *);
int		Gif_ImageNumber(Gif_Stream *, Gif_Image *);

#define		Gif_ImageWidth(gfi)		((gfi)->width)
#define		Gif_ImageHeight(gfi)		((gfi)->height)
#define		Gif_ImageDelay(gfi)		((gfi)->delay)
#define		Gif_ImageUserData(gfi)		((gfi)->userdata)
#define		Gif_SetImageUserData(gfi, v)	((gfi)->userdata = v)

int		Gif_MakeImg(Gif_Image *, byte *, int data_interlaced);


/** GIF_COLORMAP **/

typedef struct {
  
  byte haspixel;
  byte red;
  byte green;
  byte blue;
  
  u_int32_t pixel;
  
} Gif_Color;


struct Gif_Colormap {
  
  u_int16_t ncol;
  Gif_Color *col;
  
};

Gif_Colormap *	Gif_NewColormap(void);
Gif_Colormap *	Gif_NewFullColormap(int);
void		Gif_DeleteColormap(Gif_Colormap *);

Gif_Colormap *	Gif_CopyColormap(Gif_Colormap *);

Gif_Color *	Gif_GetBackground(Gif_Stream *, Gif_Colormap *);


/** GIF_COMMENT **/

struct Gif_Comment {
  char **str;
  int *len;
  int count;
  int cap;
};

Gif_Comment *	Gif_NewComment(void);
void		Gif_DeleteComment(Gif_Comment *);

int		Gif_AddComment(Gif_Comment *, char *, int);


/** MISCELLANEOUS **/


struct Gif_Record {
  unsigned char *data;
  u_int32_t length;
};

Gif_Stream *	Gif_FullReadFile(FILE *, int netscape_workaround);

int		Gif_InterlaceLine(int y, int height);
char *		Gif_CopyString(char *);
void		Gif_Error(char *);

#ifdef GIF_DEBUGGING
#define		GIF_DEBUG(x)			Gif_Debug x
void 		Gif_Debug(char *x, ...);
#else
#define		GIF_DEBUG(x)
#endif

#ifndef Gif_New
#define Gif_New(t)		((t *)malloc(sizeof(t)))
#define Gif_NewArray(t, n)	((t *)malloc(sizeof(t) * n))
#define Gif_ReArray(p, t, n)	((p) = ((t *)realloc((void *)p, sizeof(t)*n)))
#define Gif_Delete(p)		(free((void *)p))
#define Gif_DeleteArray(p)	(free((void *)p))
#endif

typedef u_int16_t Gif_Code;
#define GIF_MAX_CODE_SIZE	12
#define GIF_MAX_CODE		0xFFF
#define GIF_MAX_BLOCK		255

#ifdef __cplusplus
}
#endif
#endif
