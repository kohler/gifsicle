/* gif.h - Interface to the LCDF GIF library.
   Copyright (C) 1997-2001 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the LCDF GIF library.
   
   The LCDF GIF library is free software*. It is distributed under the GNU
   General Public License, version 2 or later; you can copy, distribute, or
   alter it at will, as long as this notice is kept intact and this source
   code is made available. There is no warranty, express or implied.
   
   *There is a patent on the LZW compression method used by GIFs, and included
   in gifwrite.c. Unisys, the patent holder, allows the compression algorithm
   to be used without a license in software distributed at no cost to the
   user. The decompression algorithm is not patented. */

#ifndef LCDF_GIF_H /* -*- mode: c -*- */
#define LCDF_GIF_H
#include <stdio.h>
#include <stdlib.h>
#include <lcdf/inttypes.h>
#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: You should define the types uint8_t, uint16_t and uint32_t before
   including this file, probably by #including <inttypes.h>. */

#define GIF_MAJOR_VERSION	1
#define GIF_MINOR_VERSION	5
#define GIF_VERSION		"1.5"

typedef struct Gif_Stream	Gif_Stream;
typedef struct Gif_Image	Gif_Image;
typedef struct Gif_Colormap	Gif_Colormap;
typedef struct Gif_Comment	Gif_Comment;
typedef struct Gif_Extension	Gif_Extension;
typedef struct Gif_Record	Gif_Record;


/** GIF_STREAM **/

struct Gif_Stream {
  
    Gif_Colormap *global;
    uint8_t background;
  
    uint16_t screen_width;
    uint16_t screen_height;
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
  
    uint16_t delay;
    uint8_t disposal;
    uint16_t left;
    uint16_t top;
  
    uint16_t width;
    uint16_t height;
  
    uint8_t interlace;
    uint8_t **img;		/* img[y][x] == image byte (x,y) */
    uint8_t *image_data;
    void (*free_image_data)(void *);
  
    uint32_t compressed_len;
    uint8_t *compressed;
    void (*free_compressed)(void *);
  
    void *user_data;
    void (*free_user_data)(void *);
    int refcount;
  
};

#define		GIF_DISPOSAL_NONE		0
#define		GIF_DISPOSAL_ASIS		1
#define		GIF_DISPOSAL_BACKGROUND		2
#define		GIF_DISPOSAL_PREVIOUS		3
  
Gif_Image *	Gif_NewImage(void);
void		Gif_DeleteImage(Gif_Image *);

int		Gif_AddImage(Gif_Stream *, Gif_Image *);
void		Gif_RemoveImage(Gif_Stream *, int);
Gif_Image *	Gif_CopyImage(Gif_Image *);

Gif_Image *	Gif_GetImage(Gif_Stream *, int);
Gif_Image *	Gif_GetNamedImage(Gif_Stream *, const char *);
int		Gif_ImageNumber(Gif_Stream *, Gif_Image *);

#define		Gif_ImageWidth(gfi)		((gfi)->width)
#define		Gif_ImageHeight(gfi)		((gfi)->height)
#define		Gif_ImageDelay(gfi)		((gfi)->delay)
#define		Gif_ImageUserData(gfi)		((gfi)->userdata)
#define		Gif_SetImageUserData(gfi, v)	((gfi)->userdata = v)
  
typedef		void (*Gif_ReadErrorHandler)(const char *, int, void *);

#define		Gif_UncompressImage(gfi)     Gif_FullUncompressImage((gfi),0,0)
int		Gif_FullUncompressImage(Gif_Image*,Gif_ReadErrorHandler,void*);
int		Gif_CompressImage(Gif_Stream *, Gif_Image *);
int		Gif_FullCompressImage(Gif_Stream *, Gif_Image *, int);
void		Gif_ReleaseUncompressedImage(Gif_Image *);
void		Gif_ReleaseCompressedImage(Gif_Image *);
int		Gif_SetUncompressedImage(Gif_Image *, uint8_t *data,
			void (*free_data)(void *), int data_interlaced);
int		Gif_CreateUncompressedImage(Gif_Image *);

int		Gif_ClipImage(Gif_Image *, int l, int t, int w, int h);


/** GIF_COLORMAP **/

typedef struct {
  
    uint8_t haspixel;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
  
    uint32_t pixel;
  
} Gif_Color;


struct Gif_Colormap {
  
    int ncol;
    int capacity;
    uint32_t userflags;
    int refcount;
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
int		Gif_AddComment(Gif_Comment *, const char *, int);


/** GIF_EXTENSION **/

struct Gif_Extension {
  
    int kind;			/* negative kinds are reserved */
    char *application;
    uint8_t *data;
    uint32_t length;
    int position;
  
    Gif_Stream *stream;
    Gif_Extension *next;
    void (*free_data)(void *);
  
};


Gif_Extension *	Gif_NewExtension(int, const char *);
void		Gif_DeleteExtension(Gif_Extension *);
int		Gif_AddExtension(Gif_Stream *, Gif_Extension *, int);
Gif_Extension * Gif_GetExtension(Gif_Stream *, int, Gif_Extension *);


/** READING AND WRITING **/

struct Gif_Record {
    const unsigned char *data;
    uint32_t length;
};

#define GIF_READ_COMPRESSED		1
#define GIF_READ_UNCOMPRESSED		2
#define GIF_READ_CONST_RECORD		4
#define GIF_READ_TRAILING_GARBAGE_OK	8
#define GIF_WRITE_CAREFUL_MIN_CODE_SIZE	1

Gif_Stream *	Gif_ReadFile(FILE *);
Gif_Stream *	Gif_FullReadFile(FILE *, int flags, Gif_ReadErrorHandler,
				 void *);
Gif_Stream *	Gif_ReadRecord(const Gif_Record *);
Gif_Stream *	Gif_FullReadRecord(const Gif_Record *, int flags,
				   Gif_ReadErrorHandler, void *);
int		Gif_WriteFile(Gif_Stream *, FILE *);
int 		Gif_FullWriteFile(Gif_Stream *, int flags, FILE *);

#define	Gif_ReadFile(f)		Gif_FullReadFile((f),GIF_READ_UNCOMPRESSED,0,0)
#define	Gif_ReadRecord(r)	Gif_FullReadRecord((r),GIF_READ_UNCOMPRESSED,0,0)
#define Gif_CompressImage(s, i)	Gif_FullCompressImage((s),(i),0)
#define Gif_WriteFile(s, f)	Gif_FullWriteFile((s),0,(f))


/** HOOKS AND MISCELLANEOUS **/

int		Gif_InterlaceLine(int y, int height);
char *		Gif_CopyString(const char *);

#define GIF_T_STREAM			(0)
#define GIF_T_IMAGE			(1)
#define GIF_T_COLORMAP			(2)
typedef void	(*Gif_DeletionHookFunc)(int, void *, void *);
int		Gif_AddDeletionHook(int, Gif_DeletionHookFunc, void *);
void		Gif_RemoveDeletionHook(int, Gif_DeletionHookFunc, void *);

#ifdef GIF_DEBUGGING
#define		GIF_DEBUG(x)			Gif_Debug x
void 		Gif_Debug(char *x, ...);
#else
#define		GIF_DEBUG(x)
#endif

typedef uint16_t Gif_Code;
#define GIF_MAX_CODE_BITS	12
#define GIF_MAX_CODE		0x1000
#define GIF_MAX_BLOCK		255

#ifndef Gif_New
# ifndef xmalloc
#  define xmalloc		malloc
#  define xrealloc		realloc
#  define xfree			free
# endif
# define Gif_New(t)		((t *)xmalloc(sizeof(t)))
# define Gif_NewArray(t, n)	((t *)xmalloc(sizeof(t) * (n)))
# define Gif_ReArray(p, t, n)	((p)=((t*)xrealloc((void*)(p),sizeof(t)*(n))))
#endif
#ifndef Gif_DeleteFunc
# define Gif_DeleteFunc		(&xfree)
# define Gif_DeleteArrayFunc	(&xfree)
#endif
#ifndef Gif_Delete
# define Gif_Delete(p)		(*Gif_DeleteFunc)((void *)(p))
# define Gif_DeleteArray(p)	(*Gif_DeleteArrayFunc)((void *)(p))
#endif

#ifdef __cplusplus
}
#endif
#endif
