#ifndef GIF_H /* -*- mode: c -*- */
#define GIF_H
#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif

/* gif.h - Interface to the GIF library.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*. It is distributed under the GNU Public
   License, version 2 or later; you can copy, distribute, or alter it at will,
   as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied.

   *The LZW compression method used by GIFs is patented. Unisys, the patent
   holder, allows the compression algorithm to be used without a license in
   software distributed at no cost to the user. */

#define GIF_MAJOR_VERSION	0
#define GIF_MINOR_VERSION	95
#define GIF_VERSION		"0.95"

#ifndef BYTE
#define BYTE
typedef unsigned char byte;
#endif
@INTEGER_TYPES@

typedef struct Gif_Stream	Gif_Stream;
typedef struct Gif_Image	Gif_Image;
typedef struct Gif_Colormap	Gif_Colormap;
typedef struct Gif_Comment	Gif_Comment;
typedef struct Gif_Extension	Gif_Extension;
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
  
  Gif_Extension *extensions;
  
  unsigned errors;
  
  int userflags;
  int refcount;
  
};

Gif_Stream *	Gif_NewStream(void);
void		Gif_DeleteStream(Gif_Stream *);

Gif_Stream *	Gif_CopyStreamSkeleton(Gif_Stream *);
Gif_Stream *	Gif_CopyStreamImages(Gif_Stream *);

#define		Gif_ScreenWidth(gfs)		((gfs)->screen_width)
#define		Gif_ScreenHeight(gfs)		((gfs)->screen_height)
#define		Gif_ImageCount(gfs)		((gfs)->nimages)

void		Gif_CalculateScreenSize(Gif_Stream *, int force);
int		Gif_Unoptimize(Gif_Stream *);


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
  byte *image_data;
  void (*free_image_data)(void *);
  
  u_int32_t compressed_len;
  byte *compressed;
  void (*free_compressed)(void *);
  
  void *userdata;
  int refcount;
  
};

#define		GIF_DISPOSAL_NONE		0
#define		GIF_DISPOSAL_ASIS		1
#define		GIF_DISPOSAL_BACKGROUND		2
#define		GIF_DISPOSAL_PREVIOUS		3
  
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

int		Gif_UncompressImage(Gif_Image *);
int		Gif_CompressImage(Gif_Stream *, Gif_Image *);
void		Gif_ReleaseUncompressedImage(Gif_Image *);
void		Gif_ReleaseCompressedImage(Gif_Image *);
int		Gif_SetUncompressedImage(Gif_Image *, byte *data,
			void (*free_data)(void *), int data_interlaced);
int		Gif_CreateUncompressedImage(Gif_Image *);

int		Gif_ClipImage(Gif_Image *, int l, int t, int w, int h);


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
  u_int16_t capacity;
  u_int32_t userflags;
  Gif_Color *col;
  
};

Gif_Colormap *	Gif_NewColormap(void);
Gif_Colormap *	Gif_NewFullColormap(int count, int capacity);
void		Gif_DeleteColormap(Gif_Colormap *);

Gif_Colormap *	Gif_CopyColormap(Gif_Colormap *);

int		Gif_ColorEq(Gif_Color *, Gif_Color *);
#define		GIF_COLOREQ(c1, c2) \
((c1)->red==(c2)->red && (c1)->green==(c2)->green && (c1)->blue==(c2)->blue)

int		Gif_FindColor(Gif_Colormap *, Gif_Color *);
int		Gif_AddColor(Gif_Colormap *, Gif_Color *, int look_from);


/** GIF_COMMENT **/

struct Gif_Comment {
  char **str;
  int *len;
  int count;
  int cap;
};

Gif_Comment *	Gif_NewComment(void);
void		Gif_DeleteComment(Gif_Comment *);
int		Gif_AddCommentTake(Gif_Comment *, char *, int);
int		Gif_AddComment(Gif_Comment *, char *, int);


/** GIF_EXTENSION **/

struct Gif_Extension {

  int kind;
  char *application;
  byte *data;
  u_int32_t length;
  int position;
  
  Gif_Stream *stream;
  Gif_Extension *next;
  
};


Gif_Extension *	Gif_NewExtension(int, char *);
void		Gif_DeleteExtension(Gif_Extension *);
int		Gif_AddExtension(Gif_Stream *, Gif_Extension *, int);


/** READING AND WRITING **/

struct Gif_Record {
  unsigned char *data;
  u_int32_t length;
};

#define GIF_READ_NETSCAPE_WORKAROUND	(1)
#define GIF_READ_COMPRESSED		(2)
#define GIF_READ_UNCOMPRESSED		(4)
#define GIF_READ_CONST_RECORD		(8)

#define		Gif_ReadFile(f)   Gif_FullReadFile((f), GIF_READ_UNCOMPRESSED)
#define		Gif_ReadRecord(r) Gif_FullReadRecord((r),GIF_READ_UNCOMPRESSED)

Gif_Stream *	Gif_FullReadFile(FILE *, int read_flags);
Gif_Stream *	Gif_FullReadRecord(Gif_Record *, int read_flags);
int 		Gif_WriteFile(Gif_Stream *, FILE *);

int		Gif_InterlaceLine(int y, int height);
char *		Gif_CopyString(char *);

#ifdef GIF_DEBUGGING
#define		GIF_DEBUG(x)			Gif_Debug x
void 		Gif_Debug(char *x, ...);
#else
#define		GIF_DEBUG(x)
#endif

@MEMORY_PREPROCESSOR@
#ifndef Gif_New
#define Gif_New(t)		((t *)malloc(sizeof(t)))
#define Gif_NewArray(t, n)	((t *)malloc(sizeof(t) * (n)))
#define Gif_ReArray(p, t, n)	((p) = ((t*)realloc((void*)(p),sizeof(t)*(n))))
#define Gif_Delete(p)		(free((void *)(p)))
#define Gif_DeleteArray(p)	(free((void *)(p)))
#endif
#ifndef Gif_DeleteFunc
#define Gif_DeleteFunc		(&free)
#define Gif_DeleteArrayFunc	(&free)
#endif

typedef u_int16_t Gif_Code;
#define GIF_MAX_CODE_BITS	12
#define GIF_MAX_CODE		0x1000
#define GIF_MAX_BLOCK		255

#ifdef __cplusplus
}
#endif
#endif
