/* gifwrite.c - Functions to write GIFs.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*. It is distributed under the GNU Public
   License, version 2 or later; you can copy, distribute, or alter it at will,
   as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied.
   
   *The LZW compression method used by GIFs is patented. Unisys, the patent
   holder, allows the compression algorithm to be used without a license in
   software distributed at no cost to the user. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "gif.h"
#include <stdarg.h>
#ifdef __cplusplus
extern "C" {
#endif

#define WRITE_BUFFER_SIZE	255
#define HASH_SIZE		(2 << GIF_MAX_CODE_BITS)

typedef struct Gif_Context {
  
  Gif_Code *prefix;
  byte *suffix;
  Gif_Code *code;
  
} Gif_Context;



typedef struct Gif_Writer {
  
  FILE *f;
  byte *v;
  u_int32_t pos;
  u_int32_t cap;
  void (*byte_putter)(byte, struct Gif_Writer *);
  void (*block_putter)(byte *, u_int16_t, struct Gif_Writer *);
  
} Gif_Writer;


#define gifputbyte(b, grr)	((*grr->byte_putter)(b, grr))
#define gifputblock(b, l, grr)	((*grr->block_putter)(b, l, grr))

#ifdef __GNUC__
__inline__
#endif
static void
gifputunsigned(u_int16_t uns, Gif_Writer *grr)
{
  gifputbyte(uns & 0xFF, grr);
  gifputbyte(uns >> 8, grr);
}


static void
file_byte_putter(byte b, Gif_Writer *grr)
{
  fputc(b, grr->f);
}

static void
file_block_putter(byte *block, u_int16_t size, Gif_Writer *grr)
{
  fwrite(block, size, 1, grr->f);
}


static void
memory_byte_putter(byte b, Gif_Writer *grr)
{
  if (grr->pos >= grr->cap) {
    grr->cap *= 2;
    Gif_ReArray(grr->v, byte, grr->cap);
  }
  if (grr->v) {
    grr->v[grr->pos] = b;
    grr->pos++;
  }
}

static void
memory_block_putter(byte *data, u_int16_t len, Gif_Writer *grr)
{
  if (grr->pos + len >= grr->cap) {
    grr->cap *= 2;
    Gif_ReArray(grr->v, byte, grr->cap);
  }
  if (grr->v) {
    memcpy(grr->v + grr->pos, data, len);
    grr->pos += len;
  }
}


static u_int32_t
codehash(Gif_Context *gfc, Gif_Code prefix, byte suffix, Gif_Code eoi_code)
{
  u_int32_t hashish1 = (prefix << 9 ^ prefix << 2 ^ suffix) % HASH_SIZE;
  u_int32_t hashish2 = ((prefix | suffix << 7) % HASH_SIZE) | 1;
  Gif_Code *prefixes = gfc->prefix;
  byte *suffixes = gfc->suffix;

  while ((prefixes[hashish1] != prefix || suffixes[hashish1] != suffix)
	 && prefixes[hashish1] != eoi_code)
    hashish1 = (hashish1 + hashish2) % HASH_SIZE;
  
  return hashish1;
}

static void
print_output_code(Gif_Context *gfc, Gif_Code code, Gif_Code eoi_code)
{
  if (gfc->prefix[code] != eoi_code) {
    fprintf(stderr, "P%X", gfc->prefix[code]);
    print_output_code(gfc, gfc->prefix[code], eoi_code);
  }
  fprintf(stderr, "%X ", gfc->suffix[code]);
}


static void
write_compressed_data(byte **img, u_int16_t width, u_int16_t height,
		      byte min_code_bits, Gif_Context *gfc, Gif_Writer *grr)
{
  byte buffer[WRITE_BUFFER_SIZE];
  byte *buf;
  
  u_int16_t xleft;
  byte *imageline;
  
  u_int32_t leftover;
  byte bits_left_over;
  
  Gif_Code work_code;
  Gif_Code next_code;
  Gif_Code output_code;
  Gif_Code clear_code;
  Gif_Code eoi_code;
  Gif_Code bump_code;
  byte suffix;
  
  u_int32_t hash;
  
  byte cur_code_bits;
  
  Gif_Code *prefixes = gfc->prefix;
  byte *suffixes = gfc->suffix;
  Gif_Code *codes = gfc->code;
  
  /* Here we go! */
  gifputbyte(min_code_bits, grr);
  clear_code = 1 << min_code_bits;
  eoi_code = clear_code + 1;
  
  cur_code_bits = min_code_bits + 1;
  /* bump_code, next_code set by first runthrough of output clear_code */
  GIF_DEBUG(("clear(%d) eoi(%d)", clear_code, eoi_code));
  
  work_code = output_code = clear_code;
  /* Because output_code is clear_code, we'll initialize next_code, bump_code,
     et al. below. */
  
  bits_left_over = 0;
  leftover = 0;
  buf = buffer;
  xleft = width;
  imageline = img[0];
  
  
  while (1) {
    
    /*****
     * Output `output_code' to the data stream. */
    
    leftover |= output_code << bits_left_over;
    bits_left_over += cur_code_bits;
    while (bits_left_over >= 8) {
      *buf++ = leftover & 0xFF;
      leftover = (leftover >> 8) & 0x00FFFFFF;
      bits_left_over -= 8;
      if (buf == buffer + WRITE_BUFFER_SIZE) {
	GIF_DEBUG(("chunk"));
	gifputbyte(WRITE_BUFFER_SIZE, grr);
	gifputblock(buffer, WRITE_BUFFER_SIZE, grr);
	buf = buffer;
      }
    }
    
    if (output_code == clear_code) {
      
      Gif_Code c;
      cur_code_bits = min_code_bits + 1;
      next_code = eoi_code + 1;
      bump_code = clear_code << 1;
      
      for (hash = 0; hash < HASH_SIZE; hash++)
	prefixes[hash] = eoi_code;
      for (c = 0; c < clear_code; c++) {
	hash = codehash(gfc, clear_code, c, eoi_code);
	prefixes[hash] = clear_code;
	suffixes[hash] = c;
	codes[hash] = c;
      }
      
    } else if (next_code > bump_code) {
      
      /* bump up compression size */
      if (cur_code_bits == GIF_MAX_CODE_BITS) {
	output_code = clear_code;
	continue;
      } else {
	cur_code_bits++;
	bump_code <<= 1;
      }
      
    } else if (output_code == eoi_code)
      break;
    
    
    /*****
     * Find the next code to output. */
    
    /* If height is 0 -- no more pixels to write -- we output work_code next
       time around. */
    if (height == 0)
      goto out_of_data;
    
    /* Actual code finding. */
    while (height != 0) {
      suffix = *imageline;
      if (suffix >= clear_code) suffix = 0;
      hash = codehash(gfc, work_code, suffix, eoi_code);
      
      imageline++;
      xleft--;
      if (xleft == 0) {
	xleft = width;
	height--;
	img++;
	imageline = img[0];
      }
      
      if (prefixes[hash] == eoi_code)
	break;
      work_code = codes[hash];
    }
    
    if (hash < HASH_SIZE && prefixes[hash] == eoi_code) {
      /* We need to add something to the table. */
      
      prefixes[hash] = work_code;
      suffixes[hash] = suffix;
      codes[hash] = next_code;
      next_code++;
      
      output_code = work_code;
      work_code = suffix;	/* code for a pixel == the pixel value */
      
    } else {
      /* Ran out of data and don't need to add anything to the table. */
     out_of_data:
      output_code = work_code;
      work_code = eoi_code;
    }
    
  }
  
  if (bits_left_over > 0)
    *buf++ = leftover;
  
  if (buf != buffer) {
    GIF_DEBUG(("imageblock(%d)", buf - buffer));
    gifputbyte(buf - buffer, grr);
    gifputblock(buffer, buf - buffer, grr);
  }
  
  gifputbyte(0, grr);
}


static int
write_image_data(byte **img, u_int16_t width, u_int16_t height,
		 byte interlaced, u_int16_t num_colors,
		 Gif_Context *gfc, Gif_Writer *grr)
{
  int i;
  u_int16_t x, y, max_color;
  byte min_code_bits;
  
  max_color = 0;
  for (y = 0; y < height && max_color < 128; y++) {
    byte *data = img[y];
    for (x = width; x > 0; x--, data++)
      if (*data > max_color)
	max_color = *data;
  }
  
  min_code_bits = 2;		/* min_code_bits of 1 isn't allowed */
  i = 4;
  while (i < max_color + 1) {
    min_code_bits++;
    i *= 2;
  }
  
  if (interlaced) {
    int y;
    byte **nimg = Gif_NewArray(byte *, height + 1);
    if (!nimg) return 0;
    
    for (y = 0; y < height; y++)
      nimg[y] = img[Gif_InterlaceLine(y, height)];
    nimg[height] = 0;
    
    write_compressed_data(nimg, width, height, min_code_bits, gfc, grr);
    
    Gif_DeleteArray(nimg);
  } else
    write_compressed_data(img, width, height, min_code_bits, gfc, grr);
  
  return 1;
}


int
Gif_CompressImage(Gif_Stream *gfs, Gif_Image *gfi)
{
  int ok = 0;
  int ncolor;
  Gif_Writer grr;
  Gif_Context gfc;

  if (gfi->compressed && gfi->free_compressed)
    (*gfi->free_compressed)((void *)gfi->compressed);
  
  gfc.prefix = Gif_NewArray(Gif_Code, HASH_SIZE);
  gfc.suffix = Gif_NewArray(byte, HASH_SIZE);
  gfc.code = Gif_NewArray(Gif_Code, HASH_SIZE);
  
  grr.v = Gif_NewArray(byte, 1024);
  grr.pos = 0;
  grr.cap = 1024;
  grr.byte_putter = memory_byte_putter;
  grr.block_putter = memory_block_putter;
  
  if (!gfc.prefix || !gfc.suffix || !gfc.code || !grr.v)
    goto done;
  
  ncolor = -1;
  if (gfi->local)
    ncolor = gfi->local->ncol;
  else if (gfs->global)
    ncolor = gfs->global->ncol;
  if (ncolor < 0 || ncolor > 256) ncolor = 256;
  
  ok = write_image_data(gfi->img, gfi->width, gfi->height, gfi->interlace,
			ncolor, &gfc, &grr);
  
 done:
  if (!ok) {
    Gif_DeleteArray(grr.v);
    grr.v = 0;
  }
  gfi->compressed = grr.v;
  gfi->compressed_len = grr.pos;
  gfi->free_compressed = Gif_DeleteArrayFunc;
  Gif_DeleteArray(gfc.prefix);
  Gif_DeleteArray(gfc.suffix);
  Gif_DeleteArray(gfc.code);
  return grr.v != 0;
}


static void
write_color_table(Gif_Color *c, u_int16_t size, Gif_Writer *grr)
{
  /* GIF format doesn't allow a colormap with only 1 entry. */
  int extra = 2, i;
  if (size > 256) size = 256;
  /* Make sure the colormap is a power of two entries! */
  while (extra < size) extra *= 2;
  
  for (i = 0; i < size; i++, c++) {
    gifputbyte(c->red, grr);
    gifputbyte(c->green, grr);
    gifputbyte(c->blue, grr);
  }
  
  /* Pad out colormap with black. */
  for (; i < extra; i++) {
    gifputbyte(0, grr);
    gifputbyte(0, grr);
    gifputbyte(0, grr);
  }
}


static int
write_image(Gif_Stream *gfs, Gif_Image *gfi, Gif_Context *gfc, Gif_Writer *grr)
{
  byte packed = 0;
  u_int16_t ncolor = 256;
  u_int16_t size = 2;
  
  gifputbyte(',', grr);
  gifputunsigned(gfi->left, grr);
  gifputunsigned(gfi->top, grr);
  gifputunsigned(gfi->width, grr);
  gifputunsigned(gfi->height, grr);
  
  if (gfi->local) {
    ncolor = gfi->local->ncol;
    packed |= 0x80;
    while (size < ncolor && size < 256)
      size *= 2, packed++;
  } else
    ncolor = gfs->global->ncol;
  
  if (gfi->interlace) packed |= 0x40;
  gifputbyte(packed, grr);
  
  if (gfi->local)
    write_color_table(gfi->local->col, gfi->local->ncol, grr);
  
  /* use existing compressed data if it exists. This will tend to whip
     people's asses who uncompress an image, keep the compressed data around,
     but modify the uncompressed data anyway. That sucks. */
  if (gfi->compressed) {
    byte *compressed = gfi->compressed;
    u_int32_t len = gfi->compressed_len;
    while (len > 0x1000) {
      gifputblock(compressed, 0x1000, grr);
      len -= 0x1000;
      compressed += 0x1000;
    }
    if (len > 0) gifputblock(compressed, len, grr);
    
  } else
    write_image_data(gfi->img, gfi->width, gfi->height, gfi->interlace,
		     ncolor, gfc, grr);
  
  return 1;
}


static void
write_logical_screen_descriptor(Gif_Stream *gfs, Gif_Writer *grr)
{
  byte packed = 0x70;		/* high resolution colors */

  Gif_CalculateScreenSize(gfs, 0);
  gifputunsigned(gfs->screen_width, grr);
  gifputunsigned(gfs->screen_height, grr);
  
  if (gfs->global) {
    u_int16_t size = 2;
    packed |= 0x80;
    while (size < gfs->global->ncol && size < 256)
      size *= 2, packed++;
  }
  
  gifputbyte(packed, grr);
  gifputbyte(gfs->background, grr);
  gifputbyte(0, grr);		/* no aspect ratio information */
  
  if (gfs->global)
    write_color_table(gfs->global->col, gfs->global->ncol, grr);
}


/* extension byte table:
   0x01 plain text extension
   0xCE name*
   0xF9 graphic control extension
   0xFE comment extension
   0xFF application extension
   */

static void
write_graphic_control_extension(Gif_Image *gfi, Gif_Writer *grr)
{
  byte packed = 0;
  gifputbyte('!', grr);
  gifputbyte(0xF9, grr);
  gifputbyte(4, grr);
  if (gfi->transparent >= 0) packed |= 0x01;
  packed |= (gfi->disposal & 0x07) << 2;
  gifputbyte(packed, grr);
  gifputunsigned(gfi->delay, grr);
  gifputbyte(gfi->transparent, grr);
  gifputbyte(0, grr);
}


static void
blast_data(byte *data, int len, Gif_Writer *grr)
{
  while (len > 0) {
    int s = len > 255 ? 255 : len;
    gifputbyte(s, grr);
    gifputblock(data, s, grr);
    data += s;
    len -= s;
  }
  gifputbyte(0, grr);
}


static void
write_name_extension(char *id, Gif_Writer *grr)
{
  gifputbyte('!', grr);
  gifputbyte(0xCE, grr);
  blast_data((byte *)id, strlen(id), grr);
}


static void
write_comment_extensions(Gif_Comment *gfcom, Gif_Writer *grr)
{
  int i;
  for (i = 0; i < gfcom->count; i++) {
    gifputbyte('!', grr);
    gifputbyte(0xFE, grr);
    blast_data((byte *)gfcom->str[i], gfcom->len[i], grr);
  }
}


static void
write_netscape_loop_extension(u_int16_t value, Gif_Writer *grr)
{
  gifputbyte('!', grr);
  gifputbyte(255, grr);
  gifputbyte(11, grr);
  gifputblock((byte *)"NETSCAPE2.0", 11, grr);
  gifputbyte(3, grr);
  gifputbyte(1, grr);
  gifputunsigned(value, grr);
  gifputbyte(0, grr);
}


static void
write_generic_extension(Gif_Extension *gfex, Gif_Writer *grr)
{
  u_int32_t pos = 0;
  if (gfex->kind < 0) return;	/* ignore our private extensions */
  
  gifputbyte('!', grr);
  gifputbyte(gfex->kind, grr);
  if (gfex->kind == 255) {	/* an application extension */
    int len = gfex->application ? strlen(gfex->application) : 0;
    if (len) {
      gifputbyte(len, grr);
      gifputblock((byte *)gfex->application, len, grr);
    }
  }
  while (pos + 255 < gfex->length) {
    gifputbyte(255, grr);
    gifputblock(gfex->data + pos, 255, grr);
    pos += 255;
  }
  if (pos < gfex->length) {
    u_int32_t len = gfex->length - pos;
    gifputbyte(len, grr); 
    gifputblock(gfex->data + pos, len, grr);
  }
  gifputbyte(0, grr);
}


static int
write_gif(Gif_Stream *gfs, Gif_Writer *grr)
{
  int ok = 0;
  int i;
  Gif_Image *gfi;
  Gif_Extension *gfex = gfs->extensions;
  Gif_Context gfc;
  
  gfc.prefix = Gif_NewArray(Gif_Code, HASH_SIZE);
  gfc.suffix = Gif_NewArray(byte, HASH_SIZE);
  gfc.code = Gif_NewArray(Gif_Code, HASH_SIZE);
  if (!gfc.prefix || !gfc.suffix || !gfc.code)
    goto done;
  
  {
    byte isgif89a = 0;
    if (gfs->comment || gfs->loopcount > -1)
      isgif89a = 1;
    for (i = 0; i < gfs->nimages && !isgif89a; i++) {
      gfi = gfs->images[i];
      if (gfi->identifier || gfi->transparent != -1 || gfi->disposal ||
	  gfi->delay || gfi->comment)
	isgif89a = 1;
    }
    if (isgif89a)
      gifputblock((byte *)"GIF89a", 6, grr);
    else
      gifputblock((byte *)"GIF87a", 6, grr);
  }
  
  write_logical_screen_descriptor(gfs, grr);
  
  if (gfs->loopcount > -1)
    write_netscape_loop_extension(gfs->loopcount, grr);
  
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    while (gfex && gfex->position == i) {
      write_generic_extension(gfex, grr);
      gfex = gfex->next;
    }
    if (gfi->comment)
      write_comment_extensions(gfi->comment, grr);
    if (gfi->identifier)
      write_name_extension(gfi->identifier, grr);
    if (gfi->transparent != -1 || gfi->disposal || gfi->delay)
      write_graphic_control_extension(gfi, grr);
    if (!write_image(gfs, gfi, &gfc, grr))
      goto done;
  }
  
  while (gfex) {
    write_generic_extension(gfex, grr);
    gfex = gfex->next;
  }
  if (gfs->comment)
    write_comment_extensions(gfs->comment, grr);
  
  gifputbyte(';', grr);
  ok = 1;
  
 done:
  Gif_DeleteArray(gfc.prefix);
  Gif_DeleteArray(gfc.suffix);
  Gif_DeleteArray(gfc.code);
  return ok;
}


int
Gif_WriteFile(Gif_Stream *gfs, FILE *f)
{
  Gif_Writer grr;
  grr.f = f;
  grr.byte_putter = file_byte_putter;
  grr.block_putter = file_block_putter;
  return write_gif(gfs, &grr);
}


#ifdef __cplusplus
}
#endif
