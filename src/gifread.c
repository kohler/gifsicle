/* gifread.c - Functions to read GIFs.
   Copyright (C) 1997-9 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*. It is distributed under the GNU General
   Public License, version 2 or later; you can copy, distribute, or alter it
   at will, as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied.

   *There is a patent on the LZW compression method used by GIFs, and included
   in gifwrite.c. Unisys, the patent holder, allows the compression algorithm
   to be used without a license in software distributed at no cost to the
   user. The decompression algorithm is not patented. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#elif !defined(__cplusplus)
/* Assume we don't have inline by default */
# define inline
#endif
#include <lcdfgif/gif.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  
  Gif_Stream *stream;
  
  Gif_Code *prefix;
  uint8_t *suffix;
  uint16_t *length;
  
  uint16_t width;
  uint16_t height;
  
  uint8_t *image;
  uint8_t *maximage;
  
  unsigned decodepos;

  Gif_ReadErrorHandler handler;
  void *handler_thunk;
  
} Gif_Context;


typedef struct Gif_Reader {
  
  FILE *f;
  const uint8_t *v;
  uint32_t w;
  uint32_t length;
  int is_record;
  int is_eoi;
  uint8_t (*byte_getter)(struct Gif_Reader *);
  void (*block_getter)(uint8_t *, uint32_t, struct Gif_Reader *);
  uint32_t (*offseter)(struct Gif_Reader *);
  int (*eofer)(struct Gif_Reader *);
  
} Gif_Reader;


#define gifgetc(grr)	((char)(*grr->byte_getter)(grr))
#define gifgetbyte(grr) ((*grr->byte_getter)(grr))
#define gifgetblock(ptr, size, grr) ((*grr->block_getter)(ptr, size, grr))
#define gifgetoffset(grr) ((*grr->offseter)(grr))
#define gifeof(grr)	((*grr->eofer)(grr))

static inline uint16_t
gifgetunsigned(Gif_Reader *grr)
{
  uint8_t one = gifgetbyte(grr);
  uint8_t two = gifgetbyte(grr);
  return one | (two << 8);
}


static uint8_t
file_byte_getter(Gif_Reader *grr)
{
  int i = getc(grr->f);
  return i == EOF ? 0 : (uint8_t)i;
}

static void
file_block_getter(uint8_t *p, uint32_t s, Gif_Reader *grr)
{
  fread(p, 1, s, grr->f);
}

static uint32_t
file_offseter(Gif_Reader *grr)
{
  return ftell(grr->f);
}

static int
file_eofer(Gif_Reader *grr)
{
  int c = getc(grr->f);
  if (c == EOF)
    return 1;
  else {
    ungetc(c, grr->f);
    return 0;
  }
}


static uint8_t
record_byte_getter(Gif_Reader *grr)
{
  return grr->w ? (grr->w--, *grr->v++) : 0;
}

static void
record_block_getter(uint8_t *p, uint32_t s, Gif_Reader *grr)
{
  if (s > grr->w) s = grr->w;
  memcpy(p, grr->v, s);
  grr->w -= s, grr->v += s;
}

static uint32_t
record_offseter(Gif_Reader *grr)
{
  return grr->length - grr->w;
}

static int
record_eofer(Gif_Reader *grr)
{
  return grr->w == 0;
}


static void
make_data_reader(Gif_Reader *grr, const uint8_t *data, uint32_t length)
{
  grr->v = data;
  grr->length = length;
  grr->w = length;
  grr->is_record = 1;
  grr->byte_getter = record_byte_getter;
  grr->block_getter = record_block_getter;
  grr->offseter = record_offseter;
  grr->eofer = record_eofer;
}


static void
gif_read_error(Gif_Context *gfc, const char *error)
{
  gfc->stream->errors++;
  if (gfc->handler)
    gfc->handler(error, gfc->stream->nimages, gfc->handler_thunk);
}


static uint8_t
one_code(Gif_Context *gfc, Gif_Code code)
{
  uint8_t *suffixes = gfc->suffix;
  Gif_Code *prefixes = gfc->prefix;
  uint8_t *ptr;
  int lastsuffix;
  uint16_t codelength = gfc->length[code];

  gfc->decodepos += codelength;
  ptr = gfc->image + gfc->decodepos;
  if (ptr > gfc->maximage || !codelength) {
    gif_read_error(gfc, (!codelength ? "bad code" : "too much image data"));
    /* 5/26/98 It's not good enough simply to count an error, because in the
       read_image_data function, if code == next_code, we will store a byte in
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

static int
read_image_block(Gif_Reader *grr, uint8_t *buffer, int *bit_pos_store,
		 int *bit_len_store, int bits_needed)
{
  int bit_position = *bit_pos_store;
  int bit_length = *bit_len_store;
  uint8_t block_len;
  
  while (bit_position + bits_needed > bit_length) {
    /* Read in the next data block. */
    if (bit_position >= 8) {
      /* Need to shift down the upper, unused part of `buffer' */
      int i = bit_position / 8;
      buffer[0] = buffer[i];
      buffer[1] = buffer[i+1];
      bit_position -= i * 8;
      bit_length -= i * 8;
    }
    block_len = gifgetbyte(grr);
    GIF_DEBUG(("\nimage_block(%d)", block_len));
    if (block_len == 0) return 0;
    gifgetblock(buffer + bit_length / 8, block_len, grr);
    bit_length += block_len * 8;
  }
  
  *bit_pos_store = bit_position;
  *bit_len_store = bit_length;
  return 1;
}


static void
read_image_data(Gif_Context *gfc, Gif_Reader *grr)
{
  /* we need a bit more than GIF_MAX_BLOCK in case a single code is split
     across blocks */
  uint8_t buffer[GIF_MAX_BLOCK + 5];
  int i;
  uint32_t accum;
  
  int bit_position;
  int bit_length;
  
  Gif_Code code;
  Gif_Code old_code;
  Gif_Code clear_code;
  Gif_Code eoi_code;
  Gif_Code next_code;
#define CUR_BUMP_CODE (1 << bits_needed)
#define CUR_CODE_MASK ((1 << bits_needed) - 1)
  
  int min_code_size;
  int bits_needed;
  
  gfc->decodepos = 0;
  
  min_code_size = gifgetbyte(grr);
  GIF_DEBUG(("\n\nmin_code_size(%d)", min_code_size));
  if (min_code_size >= GIF_MAX_CODE_BITS) {
    gif_read_error(gfc, "min_code_size too big");
    min_code_size = GIF_MAX_CODE_BITS - 1;
  } else if (min_code_size < 2) {
    gif_read_error(gfc, "min_code_size too small");
    min_code_size = 2;
  }
  clear_code = 1 << min_code_size;
  for (code = 0; code < clear_code; code++) {
    gfc->prefix[code] = 49428;
    gfc->suffix[code] = (uint8_t)code;
    gfc->length[code] = 1;
  }
  eoi_code = clear_code + 1;
  
  next_code = eoi_code;
  bits_needed = min_code_size + 1;
  
  code = clear_code;
  
  bit_length = bit_position = 0;
  /* Thus the `Read in the next data block.' code below will be invoked on the
     first time through: exactly right! */
  
  while (1) {
    
    old_code = code;
    
    /* GET A CODE INTO THE `code' VARIABLE.
     * 
     * 9.Dec.1998 - Rather than maintain a byte pointer and a bit offset into
     * the current byte (and the processing associated with that), we maintain
     * one number: the offset, in bits, from the beginning of `buffer'. This
     * much cleaner choice was inspired by Patrick J. Naughton
     * <naughton@wind.sun.com>'s GIF-reading code, which does the same thing.
     * His code distributed as part of XV in xvgif.c. */
    
    if (bit_position + bits_needed > bit_length)
      /* Read in the next data block. */
      if (!read_image_block(grr, buffer, &bit_position, &bit_length,
			    bits_needed))
	goto zero_length_block;
    
    i = bit_position / 8;
    accum = buffer[i] + (buffer[i+1] << 8);
    if (bits_needed >= 8)
      accum |= (buffer[i+2]) << 16;
    code = (Gif_Code)((accum >> (bit_position % 8)) & CUR_CODE_MASK);
    bit_position += bits_needed;
    
    GIF_DEBUG(("%d", code));
    
    /* CHECK FOR SPECIAL OR BAD CODES: clear_code, eoi_code, or a code that is
     * too large. */
    if (code == clear_code) {
      bits_needed = min_code_size + 1;
      next_code = eoi_code;
      continue;
      
    } else if (code == eoi_code)
      break;
    
    else if (code > next_code && next_code) {
      /* code > next_code: a (hopefully recoverable) error.
	 
	 Bug fix, 5/27: Do this even if old_code == clear_code, and set code
	 to 0 to prevent errors later. (If we didn't zero code, we'd later set
	 old_code = code; then we had old_code >= next_code; so the prefixes
	 array got all screwed up!) */
      gif_read_error(gfc, "unexpected code");
      code = 0;
    }
    
    /* PROCESS THE CURRENT CODE and define the next code. If no meaningful
     * next code should be defined, then we have set next_code to either
     * `eoi_code' or `clear_code' -- so we'll store useless prefix/suffix data
     * in a useless place. */
    
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
    
    /* Increment next_code except for the `clear_code' special case (that's
       when we're reading at the end of a GIF) */
    if (next_code != clear_code) {
      next_code++;
      if (next_code == CUR_BUMP_CODE) {
	if (bits_needed < GIF_MAX_CODE_BITS)
	  bits_needed++;
	else
	  next_code = clear_code;
      }
    }
    
  }
  
  /* read blocks until zero-length reached. */
  i = gifgetbyte(grr);
  GIF_DEBUG(("\nafter_image(%d)\n", i));
  while (i > 0) {
    gifgetblock(buffer, i, grr);
    i = gifgetbyte(grr);
    GIF_DEBUG(("\nafter_image(%d)\n", i));
  }
  
  /* zero-length block reached. */
 zero_length_block:
  
  if (gfc->image + gfc->decodepos < gfc->maximage)
    gif_read_error(gfc, "not enough image data for image size");
  else if (gfc->image + gfc->decodepos > gfc->maximage)
    gif_read_error(gfc, "too much image data for image size");
}


static Gif_Colormap *
read_color_table(int size, Gif_Reader *grr)
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
read_logical_screen_descriptor(Gif_Stream *gfs, Gif_Reader *grr)
     /* returns 0 on memory error */
{
  uint8_t packed;
  
  /* we don't care about logical screen width or height */
  gfs->screen_width = gifgetunsigned(grr);
  gfs->screen_height = gifgetunsigned(grr);
  
  packed = gifgetbyte(grr);
  gfs->background = gifgetbyte(grr);
  
  /* don't care about pixel aspect ratio */
  gifgetbyte(grr);
  
  if (packed & 0x80) { /* have a global color table */
    int ncol = 1 << ((packed & 0x07) + 1);
    gfs->global = read_color_table(ncol, grr);
    if (!gfs->global) return 0;
    gfs->global->refcount = 1;
  }
  
  return 1;
}


static int
read_compressed_image(Gif_Image *gfi, Gif_Reader *grr, int read_flags)
{
  if (grr->is_record) {
    const uint8_t *first = grr->v;
    uint32_t pos;
    
    /* scan over image */
    pos = 1;			/* skip min code size */
    while (pos < grr->w) {
      int amt = grr->v[pos];
      pos += amt + 1;
      if (amt == 0) break;
    }
    if (pos > grr->w) pos = grr->w;
    
    gfi->compressed_len = pos;
    if (read_flags & GIF_READ_CONST_RECORD) {
      gfi->compressed = (uint8_t *)first;
      gfi->free_compressed = 0;
    } else {
      gfi->compressed = Gif_NewArray(uint8_t, gfi->compressed_len);
      gfi->free_compressed = Gif_DeleteArrayFunc;
      if (!gfi->compressed) return 0;
      memcpy(gfi->compressed, first, gfi->compressed_len);
    }
    
    /* move reader over that image */
    grr->v += pos;
    grr->w -= pos;
    
  } else {
    /* non-record; have to read it block by block. */
    uint32_t comp_cap = 1024;
    uint32_t comp_len;
    uint8_t *comp = Gif_NewArray(uint8_t, comp_cap);
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
	Gif_ReArray(comp, uint8_t, comp_cap);
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
  read_image_data(gfc, grr);
  return 1;
}


int
Gif_FullUncompressImage(Gif_Image *gfi, Gif_ReadErrorHandler h, void *hthunk)
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
  gfc.suffix = Gif_NewArray(uint8_t, GIF_MAX_CODE);
  gfc.length = Gif_NewArray(uint16_t, GIF_MAX_CODE);
  gfc.handler = h;
  gfc.handler_thunk = hthunk;
  
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
read_image(Gif_Reader *grr, Gif_Context *gfc, Gif_Image *gfi, int read_flags)
     /* returns 0 on memory error */
{
  uint8_t packed;
  
  gfi->left = gifgetunsigned(grr);
  gfi->top = gifgetunsigned(grr);
  gfi->width = gifgetunsigned(grr);
  gfi->height = gifgetunsigned(grr);
  packed = gifgetbyte(grr);
  GIF_DEBUG(("<%ux%u>", gfi->width, gfi->height));
  
  if (packed & 0x80) { /* have a local color table */
    int ncol = 1 << ((packed & 0x07) + 1);
    gfi->local = read_color_table(ncol, grr);
    if (!gfi->local) return 0;
    gfi->local->refcount = 1;
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
    uint8_t buffer[GIF_MAX_BLOCK];
    int i = gifgetbyte(grr);
    while (i > 0) {
      gifgetblock(buffer, i, grr);
      i = gifgetbyte(grr);
    }
  }
  
  return 1;
}


static void
read_graphic_control_extension(Gif_Context *gfc, Gif_Image *gfi,
			       Gif_Reader *grr)
{
  uint8_t len;
  uint8_t crap[GIF_MAX_BLOCK];
  
  len = gifgetbyte(grr);
  
  if (len == 4) {
    uint8_t packed = gifgetbyte(grr);
    gfi->disposal = (packed >> 2) & 0x07;
    gfi->delay = gifgetunsigned(grr);
    gfi->transparent = gifgetbyte(grr);
    if (!(packed & 0x01)) /* transparent color doesn't exist */
      gfi->transparent = -1;
    len -= 4;
  }
  
  if (len > 0) {
    gif_read_error(gfc, "odd graphic extension format");
    gifgetblock(crap, len, grr);
  }
  
  len = gifgetbyte(grr);
  while (len > 0) {
    gif_read_error(gfc, "odd graphic extension format");
    gifgetblock(crap, len, grr);
    len = gifgetbyte(grr);
  }
}


static char *last_name;


static char *
suck_data(char *data, int *store_len, Gif_Reader *grr)
{
  uint8_t len = gifgetbyte(grr);
  int total_len = 0;
  
  while (len > 0) {
    Gif_ReArray(data, char, total_len + len + 1);
    if (!data) return 0;
    gifgetblock((uint8_t *)data, len, grr);
    
    total_len += len;
    data[total_len] = 0;
    
    len = gifgetbyte(grr);
  }
  
  if (store_len) *store_len = total_len;
  return data;
}


static int
read_unknown_extension(Gif_Stream *gfs, int kind, char *app_name, int position,
		       Gif_Reader *grr)
{
  uint8_t block_len = gifgetbyte(grr);
  uint8_t *data = 0;
  uint8_t data_len = 0;
  Gif_Extension *gfex = 0;

  while (block_len > 0) {
    if (data) Gif_ReArray(data, uint8_t, data_len + block_len + 1);
    else data = Gif_NewArray(uint8_t, block_len + 1);
    if (!data) goto done;
    gifgetblock(data + data_len, block_len, grr);
    data_len += block_len;
    block_len = gifgetbyte(grr);
  }

  if (data)
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
    uint8_t buffer[GIF_MAX_BLOCK];
    gifgetblock(buffer, block_len, grr);
    block_len = gifgetbyte(grr);
  }
  return gfex != 0;
}


static int
read_application_extension(Gif_Context *gfc, int position, Gif_Reader *grr)
{
  Gif_Stream *gfs = gfc->stream;
  uint8_t buffer[GIF_MAX_BLOCK + 1];
  uint8_t len = gifgetbyte(grr);
  gifgetblock(buffer, len, grr);
  
  /* Read the Netscape loop extension. */
  if (len == 11 && memcmp(buffer, "NETSCAPE2.0", 11) == 0) {
    
    len = gifgetbyte(grr);
    if (len == 3) {
      gifgetbyte(grr); /* throw away the 1 */
      gfs->loopcount = gifgetunsigned(grr);
      len = gifgetbyte(grr);
      if (len) gif_read_error(gfc, "bad loop extension");
    } else
      gif_read_error(gfc, "bad loop extension");
    
    while (len > 0) {
      gifgetblock(buffer, len, grr);
      len = gifgetbyte(grr);
    }
    return 1;
    
  } else {
    buffer[len] = 0;
    return read_unknown_extension(gfs, 0xFF, (char *)buffer, position, grr);
  }
}


static int
read_comment_extension(Gif_Image *gfi, Gif_Reader *grr)
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
read_gif(Gif_Reader *grr, int read_flags,
	 Gif_ReadErrorHandler handler, void *handler_thunk)
{
  Gif_Stream *gfs;
  Gif_Image *gfi;
  Gif_Image *new_gfi;
  Gif_Context gfc;
  int extension_position = 0;
  int unknown_block_type = 0;
  
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
  gfc.suffix = Gif_NewArray(uint8_t, GIF_MAX_CODE);
  gfc.length = Gif_NewArray(uint16_t, GIF_MAX_CODE);
  gfc.handler = handler;
  gfc.handler_thunk = handler_thunk;
  
  if (!gfs || !gfi || !gfc.prefix || !gfc.suffix || !gfc.length)
    goto done;
  
  GIF_DEBUG(("\nGIF"));
  if (!read_logical_screen_descriptor(gfs, grr))
    goto done;
  GIF_DEBUG(("logscrdesc"));
  
  while (!gifeof(grr)) {
    
    uint8_t block = gifgetbyte(grr);
    
    switch (block) {
      
     case ',': /* image block */
      GIF_DEBUG(("imageread %d", gfs->nimages));
      
      gfi->identifier = last_name;
      last_name = 0;
      if (!read_image(grr, &gfc, gfi, read_flags)
	  || !Gif_AddImage(gfs, gfi)) {
	Gif_DeleteImage(gfi);
	goto done;
      }
      
      new_gfi = Gif_NewImage();
      if (!new_gfi) goto done;
      
      gfi = new_gfi;
      extension_position++;
      break;
      
     case ';': /* terminator */
      GIF_DEBUG(("term\n"));
      goto done;
      
     case '!': /* extension */
      block = gifgetbyte(grr);
      GIF_DEBUG(("ext(0x%02X)", block));
      switch (block) {
	
       case 0xF9:
	read_graphic_control_extension(&gfc, gfi, grr);
	break;
	
       case 0xCE:
	last_name = suck_data(last_name, 0, grr);
	break;
	
       case 0xFE:
	if (!read_comment_extension(gfi, grr)) goto done;
	break;
	
       case 0xFF:
	read_application_extension(&gfc, extension_position, grr);
	break;
	
       default:
	read_unknown_extension(gfs, block, 0, extension_position, grr);
	break;

      }
      break;
      
     default:
       if (!unknown_block_type) {
	 char buf[256];
	 sprintf(buf, "unknown block type %d at file offset %d", block, gifgetoffset(grr) - 1);
	 gif_read_error(&gfc, buf);
	 unknown_block_type = 1;
       }
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

  if (gfs->errors == 0 && !(read_flags & GIF_READ_TRAILING_GARBAGE_OK) && !grr->eofer(grr)) {
    gif_read_error(&gfc, "trailing garbage after GIF ignored");
    /* but clear error count, since the GIF itself was all right */
    gfs->errors = 0;
  }
  
  return gfs;
}


Gif_Stream *
Gif_FullReadFile(FILE *f, int read_flags,
		 Gif_ReadErrorHandler h, void *hthunk)
{
  Gif_Reader grr;
  if (!f) return 0;
  grr.f = f;
  grr.is_record = 0;
  grr.byte_getter = file_byte_getter;
  grr.block_getter = file_block_getter;
  grr.offseter = file_offseter;
  grr.eofer = file_eofer;
  return read_gif(&grr, read_flags, h, hthunk);
}

Gif_Stream *
Gif_FullReadRecord(const Gif_Record *gifrec, int read_flags,
		   Gif_ReadErrorHandler h, void *hthunk)
{
  Gif_Reader grr;
  if (!gifrec) return 0;
  make_data_reader(&grr, gifrec->data, gifrec->length);
  if (read_flags & GIF_READ_CONST_RECORD)
    read_flags |= GIF_READ_COMPRESSED;
  return read_gif(&grr, read_flags, h, hthunk);
}


#undef Gif_ReadFile
#undef Gif_ReadRecord

Gif_Stream *
Gif_ReadFile(FILE *f)
{
  return Gif_FullReadFile(f, GIF_READ_UNCOMPRESSED, 0, 0);
}

Gif_Stream *
Gif_ReadRecord(const Gif_Record *gifrec)
{
  return Gif_FullReadRecord(gifrec, GIF_READ_UNCOMPRESSED, 0, 0);
}


#ifdef __cplusplus
}
#endif
