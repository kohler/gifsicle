/* gifread.c - Functions to read GIFs.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*. It is distributed under the GNU Public
   License, version 2 or later; you can copy, distribute, or alter it at will,
   as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied.

   *The LZW compression method used by GIFs is patented. Unisys, the patent
   holder, allows the compression algorithm to be used without a license in
   software distributed at no cost to the user. */

#include "gif.h"
#include <stdarg.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif

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
  byte *v;
  u_int32_t w;
  int is_record;
  byte (*byte_getter)(struct Gif_Reader *);
  void (*block_getter)(byte *, u_int32_t, struct Gif_Reader *);
  int (*eofer)(struct Gif_Reader *);
  
} Gif_Reader;


void
Gif_Debug(char *format, ...)
{
  va_list val;
  va_start(val, format);
  vfprintf(stderr, format, val);
  va_end(val);
  fputc(' ', stderr);
}


#define gifgetc(grr)	((char)(*grr->byte_getter)(grr))
#define gifgetbyte(grr) ((*grr->byte_getter)(grr))
#define gifgetblock(ptr, size, grr) ((*grr->block_getter)(ptr, size, grr))
#define gifeof(grr)	((*grr->eofer)(grr))

#ifdef __GNUC__
__inline__
#endif
static u_int16_t
gifgetunsigned(Gif_Reader *grr)
{
  byte one = gifgetbyte(grr);
  byte two = gifgetbyte(grr);
  return one | (two << 8);
}


static byte
file_byte_getter(Gif_Reader *grr)
{
  int i = getc(grr->f);
  return i == EOF ? 0 : (byte)i;
}

static void
file_block_getter(byte *p, u_int32_t s, Gif_Reader *grr)
{
  GIF_DEBUG(("b%d", s));
  fread(p, s, 1, grr->f);
}

static int
file_eofer(Gif_Reader *grr)
{
  return feof(grr->f);
}


static byte
record_byte_getter(Gif_Reader *grr)
{
  return grr->w ? (grr->w--, *grr->v++) : 0;
}

static void
record_block_getter(byte *p, u_int32_t s, Gif_Reader *grr)
{
  if (s > grr->w) s = grr->w;
  memcpy(p, grr->v, s);
  grr->w -= s, grr->v += s;
}

static int
record_eofer(Gif_Reader *grr)
{
  return grr->w == 0;
}


static void
make_data_reader(Gif_Reader *grr, byte *data, u_int32_t length)
{
  grr->v = data;
  grr->w = length;
  grr->is_record = 1;
  grr->byte_getter = record_byte_getter;
  grr->block_getter = record_block_getter;
  grr->eofer = record_eofer;
}


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
    /* 5/26/98 It's not good enough simply to count an error, because in the
       image_data function, if code == next_code, we will store a byte in
       gfc->image[gfc->decodepos-1]. Thus, fix decodepos so it's w/in the
       image. */
    gfc->decodepos = gfc->maximage - gfc->image;
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
image_data(Gif_Context *gfc, Gif_Reader *grr)
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
  if (min_code_size >= GIF_MAX_CODE_BITS) {
    gfc->stream->errors++;
    min_code_size = GIF_MAX_CODE_BITS - 1;
  }
  clear_code = 1 << min_code_size;
  codemask = 0;
  for (code = 0; code < clear_code; code++) {
    gfc->prefix[code] = 49428;
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
      bits_needed = min_code_size + 1;
      next_code = eoi_code + 1;
      max_code = clear_code << 1;
      codemask = clear_code | (clear_code - 1);
      continue;
      
    } else if (code == eoi_code)
      break;

    else if (code > next_code && next_code) {
      /* code > next_code: a (hopefully recoverable) error.
	 
	 Bug fix, 5/27: Do this earlier, even if old_code == clear_code, and
	 set code to 0 to prevent errors later. (If we didn't clear code, we'd
	 later set old_code = code; then we had old_code >= next_code; so the
	 prefixes array got all screwed up!) */
      gfc->stream->errors++;
      code = 0;
    }
    
    if (old_code != clear_code && next_code) {
      /* Process the current code and define the next code. */
      
      /* *First,* set up the prefix and length for the next code
	 (in case code == next_code). */
      gfc->prefix[next_code] = old_code;
      gfc->length[next_code] = gfc->length[old_code] + 1;
      
      /* Use one_code to process code. It's nice that it returns the first
	 pixel in code: that's what we need. */
      gfc->suffix[next_code] = one_code(gfc, code);
      
      /* Special processing if code == next_code: we didn't know code's final
	 suffix when we called one_code, but we do now. */
      if (code == next_code)
	gfc->image[gfc->decodepos - 1] = gfc->suffix[next_code];
      
      next_code++;
      
    } else
      /* Just process the current code. */
      one_code(gfc, code);
    
    if (next_code == max_code) {
      if (bits_needed < GIF_MAX_CODE_BITS) {
	bits_needed++;
	max_code <<= 1;
	codemask = (codemask << 1) | 1;
      } else
	next_code = 0;
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


static Gif_Colormap *
color_table(int size, Gif_Reader *grr)
{
  Gif_Colormap *gfcm = Gif_NewFullColormap(size, size);
  Gif_Color *c;
  if (!gfcm) return 0;
  
  GIF_DEBUG(("colormap(%d)", size));
  for (c = gfcm->col; size; size--, c++) {
    c->red = gifgetbyte(grr);
    c->green = gifgetbyte(grr);
    c->blue = gifgetbyte(grr);
    c->haspixel = 0;
  }
  
  return gfcm;
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
    int ncol = 1 << ((packed & 0x07) + 1);
    gfs->global = color_table(ncol, grr);
    if (!gfs->global) return 0;
  }
  
  return 1;
}


static int
read_compressed_image(Gif_Image *gfi, Gif_Reader *grr, int read_flags)
{
  byte buffer[GIF_MAX_BLOCK];
  int i;
  
  if (grr->is_record) {
    byte *first = grr->v;
    byte *last;

    /* min code size */
    gifgetbyte(grr);
    
    i = gifgetbyte(grr);
    while (i > 0) {
      gifgetblock(buffer, i, grr);
      i = gifgetbyte(grr);
    }
    last = grr->v;
    
    gfi->compressed_len = last - first;
    if (read_flags & GIF_READ_CONST_RECORD) {
      gfi->compressed = first;
      gfi->free_compressed = 0;
    } else {
      gfi->compressed = Gif_NewArray(byte, gfi->compressed_len);
      gfi->free_compressed = Gif_DeleteArrayFunc;
      if (!gfi->compressed) return 0;
      memcpy(gfi->compressed, first, gfi->compressed_len);
    }
    
  } else {
    /* non-record; have to read it block by block. */
    u_int32_t comp_cap = 1024;
    u_int32_t comp_len;
    byte *comp = Gif_NewArray(byte, comp_cap);
    int i;
    if (!comp) return 0;
    
    /* min code size */
    i = gifgetbyte(grr);
    comp[0] = i;
    comp_len = 1;
    
    i = gifgetbyte(grr);
    while (i > 0) {
      /* add 2 before check so we don't have to check after loop when appending
	 0 block */
      if (comp_len + i + 2 > comp_cap) {
	comp_cap *= 2;
	Gif_ReArray(comp, byte, comp_cap);
	if (!comp) return 0;
      }
      comp[comp_len] = i;
      gifgetblock(comp + comp_len + 1, i, grr);
      comp_len += i + 1;
      i = gifgetbyte(grr);
    }
    comp[comp_len++] = 0;
    
    gfi->compressed = comp;
    gfi->compressed_len = comp_len;
    gfi->free_compressed = Gif_DeleteArrayFunc;
  }
  
  return 1;
}


static int
uncompress_image(Gif_Context *gfc, Gif_Image *gfi, Gif_Reader *grr)
{
  if (!Gif_CreateUncompressedImage(gfi)) return 0;
  gfc->width = gfi->width;
  gfc->height = gfi->height;
  gfc->image = gfi->image_data;
  gfc->maximage = gfi->image_data + gfi->width * gfi->height;
  image_data(gfc, grr);
  return 1;
}


int
Gif_UncompressImage(Gif_Image *gfi)
{
  Gif_Context gfc;
  Gif_Stream fake_gfs;
  Gif_Reader grr;
  int ok = 0;
  
  /* return right away if image is already uncompressed. this might screw over
     people who expect re-uncompressing to restore the compressed version. */
  if (gfi->img)
    return 1;
  if (gfi->image_data)
    /* we have uncompressed data, but not an `img' array;
       this shouldn't happen */
    return 0;
  
  fake_gfs.errors = 0;
  gfc.stream = &fake_gfs;
  gfc.prefix = Gif_NewArray(Gif_Code, GIF_MAX_CODE);
  gfc.suffix = Gif_NewArray(byte, GIF_MAX_CODE);
  gfc.length = Gif_NewArray(u_int16_t, GIF_MAX_CODE);
  
  if (gfi && gfc.prefix && gfc.suffix && gfc.length && gfi->compressed) {
    make_data_reader(&grr, gfi->compressed, gfi->compressed_len);
    ok = uncompress_image(&gfc, gfi, &grr);
  }
  
  Gif_DeleteArray(gfc.prefix);
  Gif_DeleteArray(gfc.suffix);
  Gif_DeleteArray(gfc.length);
  return ok && !fake_gfs.errors;
}


static int
gif_image(Gif_Reader *grr, Gif_Context *gfc, Gif_Image *gfi, int read_flags)
     /* returns 0 on memory error */
{
  byte packed;
  
  gfi->left = gifgetunsigned(grr);
  gfi->top = gifgetunsigned(grr);
  gfi->width = gifgetunsigned(grr);
  gfi->height = gifgetunsigned(grr);
  packed = gifgetbyte(grr);
  GIF_DEBUG(("<%ux%u>", gfi->width, gfi->height));
  
  if (packed & 0x80) { /* have a local color table */
    int ncol = 1 << ((packed & 0x07) + 1);
    gfi->local = color_table(ncol, grr);
    if (!gfi->local) return 0;
  }
  
  gfi->interlace = (packed & 0x40) != 0;
  
  /* Keep the compressed data if asked */
  if (read_flags & GIF_READ_COMPRESSED) {
    if (!read_compressed_image(gfi, grr, read_flags))
      return 0;
    if (read_flags & GIF_READ_UNCOMPRESSED) {
      Gif_Reader new_grr;
      make_data_reader(&new_grr, gfi->compressed, gfi->compressed_len);
      if (!uncompress_image(gfc, gfi, &new_grr))
	return 0;
    }
    
  } else if (read_flags & GIF_READ_UNCOMPRESSED) {
    if (!uncompress_image(gfc, gfi, grr))
      return 0;
    
  } else {
    /* skip over the image */
    byte buffer[GIF_MAX_BLOCK];
    int i = gifgetbyte(grr);
    while (i > 0) {
      gifgetblock(buffer, i, grr);
      i = gifgetbyte(grr);
    }
  }
  
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


static int
unknown_extension(Gif_Stream *gfs, int kind, char *app_name, int position,
		  Gif_Reader *grr)
{
  byte block_len = gifgetbyte(grr);
  byte *data = 0;
  byte data_len = 0;
  Gif_Extension *gfex = 0;

  while (block_len > 0) {
    if (data) Gif_ReArray(data, byte, data_len + block_len + 1);
    else data = Gif_NewArray(byte, block_len + 1);
    if (!data) goto done;
    gifgetblock(data + data_len, block_len, grr);
    data_len += block_len;
    block_len = gifgetbyte(grr);
  }
  
  gfex = Gif_NewExtension(kind, app_name);
  if (gfex) {
    gfex->data = data;
    gfex->length = data_len;
    data[data_len] = 0;
    Gif_AddExtension(gfs, gfex, position);
  }
  
 done:
  if (!gfex) Gif_DeleteArray(data);
  while (block_len > 0) {
    byte buffer[GIF_MAX_BLOCK];
    gifgetblock(buffer, block_len, grr);
    block_len = gifgetbyte(grr);
  }
  return gfex != 0;
}


static int
application_extension(Gif_Stream *gfs, Gif_Image *gfi, int position,
		      Gif_Reader *grr)
{
  byte buffer[GIF_MAX_BLOCK + 1];
  byte len = gifgetbyte(grr);
  gifgetblock(buffer, len, grr);
  
  /* Read the Netscape loop extension. */
  if (len == 11 && memcmp(buffer, "NETSCAPE2.0", 11) == 0) {
    
    len = gifgetbyte(grr);
    if (len == 3) {
      gifgetbyte(grr); /* throw away the 1 */
      gfs->loopcount = gifgetunsigned(grr);
      len = gifgetbyte(grr);
      if (len) gfs->errors++;
    } else
      gfs->errors++;
    
    while (len > 0) {
      gifgetblock(buffer, len, grr);
      len = gifgetbyte(grr);
    }
    return 1;
    
  } else {
    buffer[len] = 0;
    return unknown_extension(gfs, 0xFF, (char *)buffer, position, grr);
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
    if (!gfcom || !Gif_AddCommentTake(gfcom, m, len))
      return 0;
  }
  return 1;
}


static Gif_Stream *
gif_read(Gif_Reader *grr, int read_flags)
{
  Gif_Stream *gfs;
  Gif_Image *gfi;
  Gif_Image *new_gfi;
  Gif_Context gfc;
  int extension_position = 0;
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
  gfc.prefix = Gif_NewArray(Gif_Code, GIF_MAX_CODE);
  gfc.suffix = Gif_NewArray(byte, GIF_MAX_CODE);
  gfc.length = Gif_NewArray(u_int16_t, GIF_MAX_CODE);
  
  if (!gfs || !gfi || !gfc.prefix || !gfc.suffix || !gfc.length)
    goto done;
  
  GIF_DEBUG(("\nGIF"));
  if (!logical_screen_descriptor(gfs, grr))
    goto done;
  GIF_DEBUG(("logscrdesc"));
  
  while (!gifeof(grr)) {
    
    byte block = gifgetbyte(grr);
    
    switch (block) {
      
     case ',': /* image block */
      GIF_DEBUG(("imageread"));
      
      gfi->identifier = last_name;
      last_name = 0;
      if (!gif_image(grr, &gfc, gfi, read_flags) || !Gif_AddImage(gfs, gfi)) {
	Gif_DeleteImage(gfi);
	goto done;
      }
      
      new_gfi = Gif_NewImage();
      if (!new_gfi) goto done;
      
      if (read_flags & GIF_READ_NETSCAPE_WORKAROUND) {
	new_gfi->transparent = gfi->transparent;
	new_gfi->delay = gfi->delay;
	new_gfi->disposal = gfi->disposal;
      }
      gfi = new_gfi;
      extension_position++;
      break;
      
     case ';': /* terminator */
      GIF_DEBUG(("term"));
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
	application_extension(gfs, gfi, extension_position, grr);
	break;
	
       default:
	unknown_extension(gfs, block, 0, extension_position, grr);
	break;

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
Gif_FullReadFile(FILE *f, int read_flags)
{
  Gif_Reader grr;
  grr.f = f;
  grr.is_record = 0;
  grr.byte_getter = file_byte_getter;
  grr.block_getter = file_block_getter;
  grr.eofer = file_eofer;
  return gif_read(&grr, read_flags);
}


Gif_Stream *
Gif_FullReadRecord(Gif_Record *gifrec, int read_flags)
{
  Gif_Reader grr;
  make_data_reader(&grr, gifrec->data, gifrec->length);
  if (read_flags & GIF_READ_CONST_RECORD)
    read_flags |= GIF_READ_COMPRESSED;
  return gif_read(&grr, read_flags);
}


#ifdef __cplusplus
}
#endif
