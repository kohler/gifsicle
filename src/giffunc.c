/* giffunc.c - General functions for the GIF library.
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

#include "gif.h"
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif


Gif_Stream *
Gif_NewStream(void)
{
  Gif_Stream *gfs = Gif_New(Gif_Stream);
  if (!gfs) return 0;
  gfs->global = 0;
  gfs->background = 0;
  gfs->screen_width = gfs->screen_height = 0;
  gfs->loopcount = -1;
  gfs->comment = 0;
  gfs->images = 0;
  gfs->nimages = gfs->imagescap = 0;
  gfs->extensions = 0;
  gfs->errors = 0;
  gfs->userflags = 0;
  gfs->refcount = 0;
  return gfs;
}


Gif_Image *
Gif_NewImage(void)
{
  Gif_Image *gfi = Gif_New(Gif_Image);
  if (!gfi) return 0;
  gfi->identifier = 0;
  gfi->comment = 0;
  gfi->local = 0;
  gfi->transparent = -1;
  gfi->disposal = GIF_DISPOSAL_NONE;
  gfi->delay = 0;
  gfi->left = gfi->top = gfi->width = gfi->height = 0;
  gfi->interlace = 0;
  gfi->img = 0;
  gfi->image_data = 0;
  gfi->free_image_data = 0;
  gfi->compressed_len = 0;
  gfi->compressed = 0;
  gfi->free_compressed = 0;
  gfi->userdata = 0;
  gfi->refcount = 0;
  return gfi;
}


Gif_Colormap *
Gif_NewColormap(void)
{
  Gif_Colormap *gfcm = Gif_New(Gif_Colormap);
  if (!gfcm) return 0;
  gfcm->ncol = 0;
  gfcm->col = 0;
  return gfcm;
}


Gif_Colormap *
Gif_NewFullColormap(int nc)
{
  Gif_Colormap *gfcm = Gif_New(Gif_Colormap);
  if (!gfcm) return 0;
  gfcm->ncol = nc;
  gfcm->col = Gif_NewArray(Gif_Color, nc);
  if (!gfcm->col) {
    Gif_Delete(gfcm);
    return 0;
  } else
    return gfcm;
}


Gif_Comment *
Gif_NewComment(void)
{
  Gif_Comment *gfcom = Gif_New(Gif_Comment);
  if (!gfcom) return 0;
  gfcom->str = 0;
  gfcom->len = 0;
  gfcom->count = gfcom->cap = 0;
  return gfcom;
}


Gif_Extension *
Gif_NewExtension(int kind, char *app_name)
{
  Gif_Extension *gfex = Gif_New(Gif_Extension);
  if (!gfex) return 0;
  gfex->kind = kind;
  gfex->application = Gif_CopyString(app_name);
  gfex->data = 0;
  gfex->position = 0;
  gfex->stream = 0;
  gfex->next = 0;
  if (!gfex->application && app_name) {
    Gif_DeleteExtension(gfex);
    return 0;
  }
  return gfex;
}


char *
Gif_CopyString(char *s)
{
  int l;
  char *copy;
  if (!s) return 0;
  l = strlen(s);
  copy = Gif_NewArray(char, l + 1);
  if (!copy) return 0;
  memcpy(copy, s, l + 1);
  return copy;
}


int
Gif_AddImage(Gif_Stream *gfs, Gif_Image *gfi)
{
  if (gfs->nimages >= gfs->imagescap) {
    if (gfs->imagescap) gfs->imagescap *= 2;
    else gfs->imagescap = 2;
    Gif_ReArray(gfs->images, Gif_Image *, gfs->imagescap);
    if (!gfs->images) return 0;
  }
  gfs->images[gfs->nimages] = gfi;
  gfs->nimages++;
  gfi->refcount++;
  return 1;
}


int
Gif_AddCommentTake(Gif_Comment *gfcom, char *x, int xlen)
{
  if (gfcom->count >= gfcom->cap) {
    if (gfcom->cap) gfcom->cap *= 2;
    else gfcom->cap = 2;
    Gif_ReArray(gfcom->str, char *, gfcom->cap);
    Gif_ReArray(gfcom->len, int, gfcom->cap);
    if (!gfcom->str || !gfcom->len) return 0;
  }
  if (xlen < 0) xlen = strlen(x);
  gfcom->str[ gfcom->count ] = x;
  gfcom->len[ gfcom->count ] = xlen;
  gfcom->count++;
  return 1;
}


int
Gif_AddComment(Gif_Comment *gfcom, char *x, int xlen)
{
  char *new_x;
  if (xlen < 0) xlen = strlen(x);
  new_x = Gif_NewArray(char, xlen);
  if (!new_x) return 0;
  memcpy(new_x, x, xlen);
  if (Gif_AddCommentTake(gfcom, new_x, xlen) == 0) {
    Gif_DeleteArray(new_x);
    return 0;
  } else
    return 1;
}


int
Gif_AddExtension(Gif_Stream *gfs, Gif_Extension *gfex, int pos)
{
  Gif_Extension *prev, *trav;
  if (gfex->stream) return 0;
  for (prev = 0, trav = gfs->extensions;
       trav && trav->position <= pos;
       prev = trav, trav = trav->next)
    ;
  if (prev) prev->next = gfex;
  else gfs->extensions = gfex;
  gfex->next = trav;
  return 1;
}


int
Gif_ImageNumber(Gif_Stream *gfs, Gif_Image *gfi)
{
  int i;
  for (i = 0; i < gfs->nimages; i++)
    if (gfs->images[i] == gfi)
      return i;
  return -1;
}


Gif_Stream *
Gif_CopyStreamSkeleton(Gif_Stream *gfs)
{
  Gif_Stream *ngfs = Gif_NewStream();
  ngfs->global = Gif_CopyColormap(gfs->global);
  ngfs->background = gfs->background;
  ngfs->screen_width = gfs->screen_width;
  ngfs->screen_height = gfs->screen_height;
  ngfs->loopcount = gfs->loopcount;
  if (gfs->global && !ngfs->global) {
    Gif_DeleteStream(ngfs);
    return 0;
  } else
    return ngfs;
}


Gif_Stream *
Gif_CopyStreamImages(Gif_Stream *gfs)
{
  Gif_Stream *ngfs = Gif_CopyStreamSkeleton(gfs);
  int i;
  if (!ngfs) return 0;
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = Gif_CopyImage(gfs->images[i]);
    if (!gfi || !Gif_AddImage(ngfs, gfi)) {
      Gif_DeleteStream(ngfs);
      return 0;
    }
  }
  return ngfs;
}


Gif_Colormap *
Gif_CopyColormap(Gif_Colormap *src)
{
  int i;
  Gif_Colormap *dest;
  if (!src) return 0;
  
  dest = Gif_NewFullColormap(src->ncol);
  if (!dest) return 0;
  
  for (i = 0; i < src->ncol; i++) {
    dest->col[i] = src->col[i];
    dest->col[i].haspixel = 0;
  }
  
  return dest;
}


Gif_Image *
Gif_CopyImage(Gif_Image *src)
{
  Gif_Image *dest;
  byte *data;
  int i;
  if (!src) return 0;
  
  dest = Gif_NewImage();
  if (!dest) return 0;
  
  dest->identifier = Gif_CopyString(src->identifier);
  if (!dest->identifier && src->identifier) goto failure;
  if (src->comment) {
    dest->comment = Gif_NewComment();
    if (!dest->comment) goto failure;
    for (i = 0; i < src->comment->count; i++)
      if (!Gif_AddComment(dest->comment, src->comment->str[i],
			  src->comment->len[i]))
	goto failure;
  }
  
  dest->local = Gif_CopyColormap(src->local);
  if (!dest->local && src->local) goto failure;
  dest->transparent = src->transparent;
  
  dest->delay = src->delay;
  dest->disposal = src->disposal;
  dest->left = src->left;
  dest->top = src->top;
  
  dest->width = src->width;
  dest->height = src->height;
  
  dest->interlace = src->interlace;
  if (src->img) {
    dest->img = Gif_NewArray(byte *, dest->height + 1);
    dest->image_data = Gif_NewArray(byte, dest->width * dest->height);
    dest->free_image_data = Gif_DeleteArrayFunc;
    if (!dest->img || !dest->image_data) goto failure;
    for (i = 0, data = dest->image_data; i < dest->height; i++) {
      memcpy(data, src->img[i], dest->width);
      dest->img[i] = data;
      data += dest->width;
    }
    dest->img[dest->height] = 0;
  }
  if (src->compressed) {
    if (src->free_compressed == 0)
      dest->compressed = src->compressed;
    else {
      dest->compressed = Gif_NewArray(byte, src->compressed_len);
      dest->free_compressed = Gif_DeleteArrayFunc;
      memcpy(dest->compressed, src->compressed, src->compressed_len);
    }
    dest->compressed_len = src->compressed_len;
  }
  
  dest->userdata = src->userdata;
  return dest;
  
 failure:
  Gif_DeleteImage(dest);
  return 0;
}


void
Gif_DeleteStream(Gif_Stream *gfs)
{
  Gif_Extension *gfex;
  int i;
  if (!gfs) return;
  if (--gfs->refcount > 0) return;
  
  Gif_DeleteColormap(gfs->global);
  Gif_DeleteComment(gfs->comment);
  
  for (i = 0; i < gfs->nimages; i++)
    Gif_DeleteImage(gfs->images[i]);
  Gif_DeleteArray(gfs->images);
  
  gfex = gfs->extensions;
  while (gfex) {
    Gif_Extension *next = gfex->next;
    gfex->stream = 0;
    Gif_DeleteExtension(gfex);
    gfex = next;
  }
  
  Gif_Delete(gfs);
}


void
Gif_DeleteImage(Gif_Image *gfi)
{
  if (!gfi) return;
  if (--gfi->refcount > 0) return;
  
  Gif_DeleteArray(gfi->identifier);
  Gif_DeleteComment(gfi->comment);
  Gif_DeleteColormap(gfi->local);
  if (gfi->image_data && gfi->free_image_data)
    (*gfi->free_image_data)((void *)gfi->image_data);
  Gif_DeleteArray(gfi->img);
  if (gfi->compressed && gfi->free_compressed)
    (*gfi->free_compressed)((void *)gfi->compressed);
  Gif_Delete(gfi);
}


void
Gif_DeleteColormap(Gif_Colormap *gfcm)
{
  if (!gfcm) return;
  Gif_DeleteArray(gfcm->col);
  Gif_Delete(gfcm);
}


void
Gif_DeleteComment(Gif_Comment *gfcom)
{
  int i;
  if (!gfcom) return;
  for (i = 0; i < gfcom->count; i++)
    Gif_DeleteArray(gfcom->str[i]);
  Gif_DeleteArray(gfcom->str);
  Gif_DeleteArray(gfcom->len);
  Gif_Delete(gfcom);
}


void
Gif_DeleteExtension(Gif_Extension *gfex)
{
  Gif_DeleteArray(gfex->data);
  Gif_DeleteArray(gfex->application);
  if (gfex->stream) {
    Gif_Stream *gfs = gfex->stream;
    Gif_Extension *prev, *trav;
    for (prev = 0, trav = gfs->extensions;
	 trav && trav != gfex;
	 prev = trav, trav = trav->next)
      ;
    if (trav) {
      if (prev) prev->next = trav->next;
      else gfs->extensions = trav->next;
    }
  }
  Gif_Delete(gfex);
}


Gif_Image *
Gif_GetImage(Gif_Stream *gfs, int imagenumber)
{
  if (imagenumber >= 0 && imagenumber < gfs->nimages)
    return gfs->images[imagenumber];
  else
    return 0;
}


Gif_Image *
Gif_GetNamedImage(Gif_Stream *gfs, const char *name)
{
  int i;
  
  if (!name)
    return gfs->nimages ? gfs->images[0] : 0;
  
  for (i = 0; i < gfs->nimages; i++)
    if (gfs->images[i]->identifier &&
	strcmp(gfs->images[i]->identifier, name) == 0)
      return gfs->images[i];
  
  return 0;
}


void
Gif_ReleaseCompressedImage(Gif_Image *gfi)
{
  if (gfi->compressed && gfi->free_compressed)
    (*gfi->free_compressed)(gfi->compressed);
  gfi->compressed = 0;
  gfi->compressed_len = 0;
  gfi->free_compressed = 0;
}

void
Gif_ReleaseUncompressedImage(Gif_Image *gfi)
{
  Gif_DeleteArray(gfi->img);
  if (gfi->image_data && gfi->free_image_data)
    (*gfi->free_image_data)(gfi->image_data);
  gfi->img = 0;
  gfi->image_data = 0;
  gfi->free_image_data = 0;
}


Gif_Color *
Gif_GetBackground(Gif_Stream *gfs, Gif_Colormap *gfcm)
{
  if (!gfcm)
    gfcm = gfs->global;
  if (gfcm && gfs->background < gfcm->ncol)
    return &gfcm->col[gfs->background];
  else
    return 0;
}


int
Gif_InterlaceLine(int line, int height)
{
  height--;
  if (line > height / 2)
    return line * 2 - ( height       | 1);
  else if (line > height / 4)
    return line * 4 - ((height & ~1) | 2);
  else if (line > height / 8)
    return line * 8 - ((height & ~3) | 4);
  else
    return line * 8;
}


int
Gif_MakeImg(Gif_Image *gfi, byte *image_data, int data_interlaced)
{
  int i;
  int width = gfi->width;
  int height = gfi->height;
  
  byte **img = Gif_NewArray(byte *, height + 1);
  if (!img) return 0;
  
  if (data_interlaced)
    for (i = 0; i < height; i++)
      img[ Gif_InterlaceLine(i, height) ] = image_data + width * i;
  else
    for (i = 0; i < height; i++)
      img[i] = image_data + width * i;
  img[height] = 0;
  
  Gif_DeleteArray(gfi->img);
  gfi->img = img;
  return 1;
}


#ifdef GIF_DEBUGGING
#include <stdarg.h>

void
Gif_Debug(char *x, ...)
{
  va_list val;
  va_start(val, x);
  vfprintf(stderr, x, val);
  fputc('\n', stderr);
  va_end(val);
}

#endif


#ifdef __cplusplus
}
#endif
