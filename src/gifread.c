/* gifread.c - Functions to read GIFs.
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
#include <stdarg.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif


#define CODE_STORE_SIZE 512

typedef struct {
  
  Gif_Stream *stream;
  
  Gif_Code *prefix;
  byte *suffix;
  u_int16_t *length;
  
  u_int16_t width;
  u_int16_t height;
  
  byte *image;
  byte *maximage;
  
  unsigned decodepos;
  
} Gif_Context;


typedef struct Gif_Reader {
  
  FILE *f;
  unsigned char *v;
  unsigned long w;
  byte (*bytegetter)(struct Gif_Reader *);
  void (*blockgetter)(byte *, int, struct Gif_Reader *);
  int (*eofer)(struct Gif_Reader *);
  
} Gif_Reader;


#define gifgetc(grr) ((char)(*grr->bytegetter)(grr))
#define gifgetbyte(grr) ((*grr->bytegetter)(grr))
#define gifgetblock(ptr, size, grr) ((*grr->blockgetter)(ptr, size, grr))
#define gifeof(grr) ((*grr->eofer)(grr))


static byte
Gif_Reader_file_bytegetter(Gif_Reader *grr)
{
  int i = getc(grr->f);
  return i == EOF ? 0 : (byte)i;
}

static void
Gif_Reader_file_blockgetter(byte *p, int s, Gif_Reader *grr)
{
  fread(p, s, 1, grr->f);
}

static int
Gif_Reader_file_eofer(Gif_Reader *grr)
{
  return feof(grr->f);
}


static byte
Gif_Reader_gifrec_bytegetter(Gif_Reader *grr)
{
  return grr->w ? (grr->w--, *grr->v++) : 0;
}

static void
Gif_Reader_gifrec_blockgetter(byte *p, int s, Gif_Reader *grr)
{
  if (s > grr->w) s = grr->w;
  memcpy(p, grr->v, s);
  grr->w -= s, grr->v += s;
}

static int
Gif_Reader_gifrec_eofer(Gif_Reader *grr)
{
  return grr->w == 0;
}


void
Gif_Error(char *message)
{
  fprintf(stderr, "gifread: error: %s\n", message);
  abort();
}


static u_int16_t
gifgetunsigned(Gif_Reader *grr)
{
  byte one = gifgetbyte(grr);
  byte two = gifgetbyte(grr);
  return one | (two << 8);
}


#define SAFELS(a,b) ((b) < 0 ? (a) >> -(b) : (a) << (b))

static byte
one_code(Gif_Context *gfc, Gif_Code code)
{
  byte *suffixes = gfc->suffix;
  Gif_Code *prefixes = gfc->prefix;
  byte *ptr;
  int lastsuffix;
  u_int16_t codelength = gfc->length[code];
  
  gfc->decodepos += codelength;
  ptr = gfc->image + gfc->decodepos;
  if (ptr > gfc->maximage || !codelength) {
    gfc->stream->errors++;
    return 0;
  }
  
  /* codelength will always be greater than 0. */
  do {
    lastsuffix = suffixes[code];
    *--ptr = lastsuffix;
    code = prefixes[code];
  } while (--codelength > 0);
  
  /* return the first pixel in the code, which, since we walked backwards
     through the code, was the last suffix we processed. */
  return lastsuffix;
}


static void
imagedata(Gif_Context *gfc, Gif_Reader *grr)
{
  byte buffer[GIF_MAX_BLOCK];
  byte *buf;
  byte *bufmax;
  int i;
  
  int leftover;
  int amountleftover;
  
  Gif_Code code;
  Gif_Code old_code;
  Gif_Code clear_code;
  Gif_Code eoi_code;
  Gif_Code next_code;
  Gif_Code max_code;
  
  int min_code_size;
  int bits_needed;
  Gif_Code codemask;
  
  gfc->decodepos = 0;
  
  min_code_size = gifgetbyte(grr);
  if (min_code_size >= GIF_MAX_CODE_SIZE) {
    gfc->stream->errors++;
    min_code_size = GIF_MAX_CODE_SIZE - 1;
  }
  clear_code = 1 << min_code_size;
  codemask = 0;
  for (code = 0; code < clear_code; code++) {
    gfc->suffix[code] = code;
    gfc->length[code] = 1;
  }
  eoi_code = clear_code + 1;
  
  next_code = eoi_code + 1;
  max_code = clear_code << 1;
  codemask = clear_code | (clear_code - 1);
  bits_needed = min_code_size + 1;
  amountleftover = 0;
  leftover = 0;
  GIF_DEBUG(("clear(%d) mask(%d) eoi(%d) needed(%d)", clear_code, codemask,
	     eoi_code, bits_needed));
  
  code = clear_code;
  
  buf = bufmax = buffer;
  /* Thus buf == bufmax, and the `Read in the next data block.' code below
     will be invoked on the first time through: exactly right! */
  
  while (1) {
    
    old_code = code;
    
    /* Get a code into the `code' variable. */
    {
      int had = amountleftover;
      int left = bits_needed - amountleftover;
      int shift = amountleftover;
      
      code = leftover;
      
      while (left > 0) {
	if (buf == bufmax) {
	  /* Read in the next data block. */
	  i = gifgetbyte(grr);
	  GIF_DEBUG(("imageblock(%d)", i));
	  if (i == 0) goto zero_length_block;
	  buf = buffer;
	  bufmax = buffer + i;
	  gifgetblock(buffer, i, grr);
	}
	leftover = *buf++;
	code |= (leftover << shift);
	shift += 8;
	left -= 8;
	had = 8;
      }
      
      code &= codemask;
      
      if (left < 0) {
	amountleftover = -left;
	leftover >>= had + left;
      } else {
	leftover = 0;
	amountleftover = 0;
      }
    }
    
    if (code == clear_code) {
      GIF_DEBUG(("!!"));
      bits_needed = min_code_size + 1;
      next_code = eoi_code + 1;
      max_code = clear_code << 1;
      codemask = clear_code | (clear_code - 1);
      continue;
      
    } else if (code == eoi_code)
      break;
    
    if (old_code != clear_code && next_code) {
      /* Process the current code and define the next code. */
      
      /* *First,* set up the prefix and length for the next code
	 (in case code == next_code). */
      gfc->prefix[next_code] = old_code;
      gfc->length[next_code] = gfc->length[old_code] + 1;
      
      /* Use one_code to process code. It's nice that it returns the first
	 pixel in code: that's what we need. */
      if (code <= next_code)
	gfc->suffix[next_code] = one_code(gfc, code);
      else /* code > next_code: a (hopefully recoverable) error. */
	gfc->stream->errors++;
      
      /* Special processing if code == next_code: we didn't know code's final
	 suffix when we called one_code, but we do now. */
      if (code == next_code)
	gfc->image[ gfc->decodepos - 1 ] = gfc->suffix[next_code];
      
      next_code++;
      
    } else
      /* Just process the current code. */
      one_code(gfc, code);
    
    if (next_code == max_code) {
      if (bits_needed < GIF_MAX_CODE_SIZE) {
	bits_needed++;
	max_code <<= 1;
	codemask = (codemask << 1) | 1;
      } else
	next_code = 0;
      GIF_DEBUG(("%d/%d", bits_needed, max_code));
    }
    
  }
  
  /* read blocks until zero-length reached. */
  i = gifgetbyte(grr);
  while (i > 0) {
    gifgetblock(buffer, i, grr);
    i = gifgetbyte(grr);
  }
  
  /* zero-length block reached. */
 zero_length_block:
  
  if (gfc->image + gfc->decodepos != gfc->maximage)
    /* Not enough data, or too much data, was read. */
    gfc->stream->errors++;
}


static Gif_Color *
color_table(int size, Gif_Reader *grr)
{
  Gif_Color *c;
  Gif_Color *ct = Gif_NewArray(Gif_Color, size);
  if (!ct) return 0;
  
  GIF_DEBUG(("colormap(%d)", size));
  for (c = ct; size; size--, c++) {
    c->red = gifgetbyte(grr);
    c->green = gifgetbyte(grr);
    c->blue = gifgetbyte(grr);
    c->haspixel = 0;
  }
  
  return ct;
}


static int
logical_screen_descriptor(Gif_Stream *gfs, Gif_Reader *grr)
     /* returns 0 on memory error */
{
  byte packed;
  
  /* we don't care about logical screen width or height */
  gfs->screen_width = gifgetunsigned(grr);
  gfs->screen_height = gifgetunsigned(grr);
  
  packed = gifgetbyte(grr);
  gfs->background = gifgetbyte(grr);
  
  /* don't care about pixel aspect ratio */
  gifgetbyte(grr);
  
  if (packed & 0x80) { /* have a global color table */
    gfs->global = Gif_NewColormap();
    if (!gfs->global) return 0;
    gfs->global->ncol = 1 << ((packed & 0x07) + 1);
    gfs->global->col = color_table(gfs->global->ncol, grr);
    if (!gfs->global->col) return 0;
  }
  
  return 1;
}


static int
gif_image(Gif_Reader *grr, Gif_Context *gfc, Gif_Image *gfi)
     /* returns 0 on memory error */
{
  byte interlace;
  byte packed;
  
  gfi->left = gifgetunsigned(grr);
  gfi->top = gifgetunsigned(grr);
  gfi->width = gifgetunsigned(grr);
  gfi->height = gifgetunsigned(grr);
  packed = gifgetbyte(grr);
  
  if (packed & 0x80) { /* have a local color table */
    gfi->local = Gif_NewColormap();
    if (!gfi->local) return 0;
    gfi->local->ncol = 1 << ((packed & 0x07) + 1);
    gfi->local->col = color_table(gfi->local->ncol, grr);
    if (!gfi->local->col) return 0;
  }
  
  interlace = (packed & 0x40) != 0;
  gfi->interlace = interlace;
  
  GIF_DEBUG(("%dx%d", gfi->width, gfi->height));
  gfi->imagedata = Gif_NewArray(byte, gfi->width * gfi->height);
  if (!gfi->imagedata) return 0;
  if (!Gif_MakeImg(gfi, gfi->imagedata, interlace)) return 0;
  
  gfc->width = gfi->width;
  gfc->height = gfi->height;
  gfc->image = gfi->imagedata;
  gfc->maximage = gfi->imagedata + gfi->width * gfi->height;
  imagedata(gfc, grr);
  return 1;
}


static void
graphic_control_extension(Gif_Stream *gfs, Gif_Image *gfi, Gif_Reader *grr)
{
  byte len;
  byte crap[GIF_MAX_BLOCK];
  
  len = gifgetbyte(grr);
  
  if (len == 4) {
    byte packed = gifgetbyte(grr);
    gfi->disposal = (packed >> 2) & 0x07;
    gfi->delay = gifgetunsigned(grr);
    gfi->transparent = gifgetbyte(grr);
    if (!(packed & 0x01)) /* transparent color doesn't exist */
      gfi->transparent = -1;
    len -= 4;
  }
  
  if (len > 0) {
    gfs->errors++;
    gifgetblock(crap, len, grr);
  }
  
  len = gifgetbyte(grr);
  while (len > 0) {
    gfs->errors++;
    gifgetblock(crap, len, grr);
    len = gifgetbyte(grr);
  }
}


static char *last_name;


static char *
suck_data(char *data, int *store_len, Gif_Reader *grr)
{
  byte len = gifgetbyte(grr);
  int total_len = 0;
  
  while (len > 0) {
    Gif_ReArray(data, char, total_len + len + 1);
    if (!data) return 0;
    gifgetblock((byte *)data, len, grr);
    
    total_len += len;
    data[total_len] = 0;
    
    len = gifgetbyte(grr);
  }
  
  if (store_len) *store_len = total_len;
  return data;
}


static void
application_extension(Gif_Stream *gfs, Gif_Image *gfi, Gif_Reader *grr)
{
  byte len = gifgetbyte(grr);
  byte crap[GIF_MAX_BLOCK];
  
  /* Read the Netscape loop extension. */
  if (len == 11) {
    gifgetblock(crap, 11, grr);
    len = gifgetbyte(grr);
    if (memcmp(crap, "NETSCAPE2.0", 11) == 0 && len == 3) {
      gifgetbyte(grr); /* throw away the 1 */
      gfs->loopcount = gifgetunsigned(grr);
      len = gifgetbyte(grr);
    } else
      gfs->odd_app_extensions++;
  } else
    gfs->odd_app_extensions++;
  
  while (len > 0) {
    gifgetblock(crap, len, grr);
    len = gifgetbyte(grr);
  }
}


static int
comment_extension(Gif_Image *gfi, Gif_Reader *grr)
{
  int len;
  Gif_Comment *gfcom = gfi->comment;
  char *m = suck_data(0, &len, grr);
  if (m) {
    if (!gfcom)
      gfcom = gfi->comment = Gif_NewComment();
    if (!gfcom || !Gif_AddComment(gfcom, m, len))
      return 0;
  }
  return 1;
}


static Gif_Stream *
gif_read(Gif_Reader *grr, int graphic_extension_long_scope)
{
  Gif_Stream *gfs;
  Gif_Image *gfi;
  Gif_Image *newgfi;
  Gif_Context gfc;
  int ok = 0;
  
  if (gifgetc(grr) != 'G' ||
      gifgetc(grr) != 'I' ||
      gifgetc(grr) != 'F')
    return 0;
  (void)gifgetc(grr);
  (void)gifgetc(grr);
  (void)gifgetc(grr);
  
  gfs = Gif_NewStream();
  gfi = Gif_NewImage();
  
  gfc.stream = gfs;
  gfc.prefix = Gif_NewArray(Gif_Code, GIF_MAX_CODE + 1);
  gfc.suffix = Gif_NewArray(byte, GIF_MAX_CODE + 1);
  gfc.length = Gif_NewArray(u_int16_t, GIF_MAX_CODE + 1);

  if (!gfs || !gfi || !gfc.prefix || !gfc.suffix || !gfc.length)
    goto done;
  
  GIF_DEBUG(("GIF"));
  if (!logical_screen_descriptor(gfs, grr))
    goto done;
  GIF_DEBUG(("logscrdesc"));
  
  while (!gifeof(grr)) {
    
    byte block = gifgetbyte(grr);
    GIF_DEBUG(("block(%x)", block));
    
    switch (block) {
      
     case ',': /* image block */
      GIF_DEBUG(("imageread"));
      
      gfi->identifier = last_name;
      last_name = 0;
      if (!gif_image(grr, &gfc, gfi) || !Gif_AddImage(gfs, gfi)) {
	Gif_DeleteImage(gfi);
	goto done;
      }
      
      newgfi = Gif_NewImage();
      if (!newgfi) goto done;
      
      if (graphic_extension_long_scope) {
	newgfi->transparent = gfi->transparent;
	newgfi->delay = gfi->delay;
	newgfi->disposal = gfi->disposal;
      }
      gfi = newgfi;
      break;
      
     case ';': /* terminator */
      GIF_DEBUG(("fuck"));
      ok = 1;
      goto done;
      
     case '!': /* extension */
      GIF_DEBUG(("ext"));
      block = gifgetbyte(grr);
      switch (block) {
	
       case 0xF9:
	graphic_control_extension(gfs, gfi, grr);
	break;
	
       case 0xCE:
	last_name = suck_data(last_name, 0, grr);
	break;
	
       case 0xFE:
	if (!comment_extension(gfi, grr)) goto done;
	break;
	
       case 0xFF:
	application_extension(gfs, gfi, grr);
	break;
	
       default:
	 {
	   byte crap[GIF_MAX_BLOCK];
	   byte len = gifgetbyte(grr);
	   while (len > 0) {
	     gifgetblock(crap, len, grr);
	     len = gifgetbyte(grr);
	   }
	   gfs->odd_extensions++;
	   break;
	 }
	
      }
      break;
      
     default:
      gfs->errors++;
      break;
      
    }
    
  }
  
 done:
  
  /* Move comments after last image into stream. */
  gfs->comment = gfi->comment;
  gfi->comment = 0;
  
  Gif_DeleteImage(gfi);
  Gif_DeleteArray(last_name);
  Gif_DeleteArray(gfc.prefix);
  Gif_DeleteArray(gfc.suffix);
  Gif_DeleteArray(gfc.length);
  if (ok)
    return gfs;
  else {
    Gif_DeleteStream(gfs);
    return 0;
  }
}


Gif_Stream *
Gif_FullReadFile(FILE *f, int graphic_extension_long_scope)
{
  Gif_Reader grr;
  grr.f = f;
  grr.bytegetter = Gif_Reader_file_bytegetter;
  grr.blockgetter = Gif_Reader_file_blockgetter;
  grr.eofer = Gif_Reader_file_eofer;
  return gif_read(&grr, graphic_extension_long_scope);
}


Gif_Stream *
Gif_ReadFile(FILE *f)
{
  return Gif_FullReadFile(f, 0);
}


Gif_Stream *
Gif_ReadRecord(Gif_Record *gifrec)
{
  Gif_Reader grr;
  grr.v = gifrec->data;
  grr.w = gifrec->length;
  grr.bytegetter = Gif_Reader_gifrec_bytegetter;
  grr.blockgetter = Gif_Reader_gifrec_blockgetter;
  grr.eofer = Gif_Reader_gifrec_eofer;
  return gif_read(&grr, 0);
}


#ifdef __cplusplus
}
#endif
