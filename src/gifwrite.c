/* gifwrite.c - Functions to write GIFs.
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


#define WRITE_BUFFER_SIZE	254
#define HASH_SIZE		(2 << GIF_MAX_CODE_SIZE)

typedef struct Gif_Context {
  
  Gif_Code *prefix;
  byte *suffix;
  Gif_Code *code;
  
} Gif_Context;



typedef struct Gif_Writer {

  FILE *f;

} Gif_Writer;


static void
gifputbyte(byte b, Gif_Writer *grr)
{
  fputc(b, grr->f);
}

static void
gifputunsigned(u_int16_t uns, Gif_Writer *grr)
{
  gifputbyte(uns & 0xFF, grr);
  gifputbyte(uns >> 8, grr);
}

static void
gifputblock(byte *block, u_int16_t size, Gif_Writer *grr)
{
  fwrite(block, size, 1, grr->f);
}


void
Gif_Debug(char *x, ...)
{
  va_list val;
  va_start(val, x);
  vfprintf(stderr, x, val);
  va_end(val);
  fputc(' ', stderr);
}


static u_int32_t
codehash(Gif_Context *gfc, Gif_Code prefix, byte suffix, Gif_Code eoi_code)
{
  u_int32_t hashish1 = (suffix << 10 ^ prefix) % HASH_SIZE;
  u_int32_t hashish2 = (prefix % HASH_SIZE) | 1;
  Gif_Code *prefixes = gfc->prefix;
  byte *suffixes = gfc->suffix;
  
  while ((prefixes[hashish1] != prefix || suffixes[hashish1] != suffix)
	 && prefixes[hashish1] != eoi_code)
    hashish1 = (hashish1 + hashish2) % HASH_SIZE;
  
  return hashish1;
}


static int
imagedata(byte **img, u_int16_t width, u_int16_t height, u_int16_t num_colors,
	  Gif_Context *gfc, Gif_Writer *grr)
{
  byte buffer[WRITE_BUFFER_SIZE];
  byte *buf;
  
  u_int16_t xleft;
  byte *imageline;
  
  u_int32_t leftover;
  byte amountleftover;
  
  Gif_Code work_code;
  Gif_Code next_code;
  Gif_Code output_code;
  Gif_Code clear_code;
  Gif_Code eoi_code;
  Gif_Code bump_code;
  byte suffix;
  
  u_int32_t hash;
  
  byte min_code_size;
  byte need;
  
  Gif_Code *prefixes = gfc->prefix;
  byte *suffixes = gfc->suffix;
  Gif_Code *codes = gfc->code;
  
  {
    int i = 2;
    min_code_size = 1;
    while (i < num_colors) {
      min_code_size++;
      i *= 2;
    }
    if (min_code_size == 1)
      min_code_size = 2;
    GIF_DEBUG(("mcs(%d)", min_code_size));
    gifputbyte(min_code_size, grr);
  }
    
  clear_code = 1 << min_code_size;
  eoi_code = clear_code + 1;
  
  need = min_code_size + 1;
  /* bump_code, next_code set by first runthrough of output clear_code */
  GIF_DEBUG(("clear(%d) eoi(%d) needed(%d)", clear_code, eoi_code, need));
  
  work_code = output_code = clear_code;
  /* Because output_code is clear_code, we'll initialize next_code, bump_code,
     et al. below. */
  
  amountleftover = 0;
  leftover = 0;
  buf = buffer;
  xleft = width;
  imageline = img[0];
  
  
  while (1) {
    
    /****************************************************************
      Output `output_code' to the data stream. */
    
    leftover |= output_code << amountleftover;
    amountleftover += need;
    while (amountleftover >= 8) {
      *buf++ = leftover & 0xFF;
      leftover = (leftover >> 8) & 0x00FFFFFF;
      amountleftover -= 8;
      if (buf == buffer + WRITE_BUFFER_SIZE) {
	GIF_DEBUG(("chunk"));
	gifputbyte(WRITE_BUFFER_SIZE, grr);
	gifputblock(buffer, WRITE_BUFFER_SIZE, grr);
	buf = buffer;
      }
    }
    
    if (output_code == clear_code) {
      
      Gif_Code c;
      need = min_code_size + 1;
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
      
      GIF_DEBUG(("%d/%d", next_code, bump_code));
      /* bump up compression size */
      if (need == GIF_MAX_CODE_SIZE) {
	output_code = clear_code;
	continue;
      } else {
	need++;
	bump_code <<= 1;
      }
      
    } else if (output_code == eoi_code)
      break;
    
    
    /****************************************************************
      Find the next code to output. */
    
    if (height == 0) {
      /* There may be one pixel left.
	 If there is not, work_code == eoi_code.
	 If there is, then work_code contains it. We output it, but the next
	 time through, work_code will be eoi_code as we want. */
      output_code = work_code;
      work_code = eoi_code;
      continue;
    }
    
    /* Actual code finding. */
    while (height != 0) {
      suffix = *imageline;
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
    
    if (prefixes[hash] == eoi_code) {
      /* We need to add something to the table. */
      
      prefixes[hash] = work_code;
      suffixes[hash] = suffix;
      codes[hash] = next_code;
      next_code++;
      
      output_code = work_code;
      work_code = suffix;	/* code for a pixel == the pixel value */
      
    } else {
      /* Ran out of data and don't need to add anything to the table. */
      output_code = work_code;
      work_code = eoi_code;
    }
    
  }
  
  
  if (amountleftover > 0)
    *buf++ = leftover;
  
  if (buf != buffer) {
    GIF_DEBUG(("imageblock(%d)", buf - buffer));
    gifputbyte(buf - buffer, grr);
    gifputblock(buffer, buf - buffer, grr);
  }
  
  gifputbyte(0, grr);
  return 1;
}


static void
color_table(Gif_Color *c, u_int16_t size, Gif_Writer *grr)
{
  /* GIF format doesn't allow a colormap with only 1 entry. */
  int extra = 2, i;
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


static void
gif_image(Gif_Stream *gfs, Gif_Image *gfi, Gif_Context *gfc, Gif_Writer *grr)
{
  byte packed = 0;
  u_int16_t ncolor = 0;
  u_int16_t size = 2;
  
  gifputbyte(',', grr);
  gifputunsigned(gfi->left, grr);
  gifputunsigned(gfi->top, grr);
  gifputunsigned(gfi->width, grr);
  gifputunsigned(gfi->height, grr);
  
  if (gfi->local) {
    ncolor = gfi->local->ncol;
    packed |= 0x80;
    while (size < ncolor)
      size *= 2, packed++;
  } else {
    assert(gfs->global);
    ncolor = gfs->global->ncol;
  }
  if (gfi->interlace) packed |= 0x40;
  gifputbyte(packed, grr);
  
  if (gfi->local)
    color_table(gfi->local->col, gfi->local->ncol, grr);

  if (gfi->interlace) {
    byte **img = Gif_NewArray(byte *, gfi->height + 1);
    int y;
    for (y = 0; y < gfi->height; y++)
      img[y] = gfi->img[Gif_InterlaceLine(y, gfi->height)];
    img[gfi->height] = 0;
    
    imagedata(img, gfi->width, gfi->height, ncolor, gfc, grr);
    Gif_DeleteArray(img);
    
  } else
    imagedata(gfi->img, gfi->width, gfi->height, ncolor, gfc, grr);
}


static void
logical_screen_descriptor(Gif_Stream *gfs, Gif_Writer *grr)
{
  int i;
  byte packed = 0x70;		/* high resolution colors */
  
  if (gfs->screen_width <= 0)
    for (i = 0; i < gfs->nimages; i++)
      if (gfs->screen_width < gfs->images[i]->width)
	gfs->screen_width = gfs->images[i]->width;
  if (gfs->screen_height <= 0)
    for (i = 0; i < gfs->nimages; i++)
      if (gfs->screen_height < gfs->images[i]->height)
	gfs->screen_height = gfs->images[i]->height;
  gifputunsigned(gfs->screen_width, grr);
  gifputunsigned(gfs->screen_height, grr);
  
  if (gfs->global) {
    u_int16_t size = 2;
    packed |= 0x80;
    while (size < gfs->global->ncol)
      size *= 2, packed++;
  }
  
  gifputbyte(packed, grr);
  gifputbyte(gfs->background, grr);
  gifputbyte(0, grr);		/* no aspect ratio information */
  
  if (gfs->global)
    color_table(gfs->global->col, gfs->global->ncol, grr);
}


/* extension byte table:
   0x01 plain text extension
   0x43 colormap*
   0xCE name*
   0xF9 graphic control extension
   0xFE comment extension
   0xFF application extension
   */

static void
colormap_extension(Gif_Colormap *cm, Gif_Writer *grr)
{
  int size = cm->ncol;
  Gif_Color *c = cm->col;
  int maxsize = GIF_MAX_BLOCK / 3;
  gifputbyte('!', grr);
  gifputbyte(0x43, grr);
  while (size > 0) {
    byte m = size > maxsize ? maxsize : size;
    int i;
    gifputbyte(m * 3, grr);
    for (i = 0; i < m; i++) {
      gifputbyte(c[i].red, grr);
      gifputbyte(c[i].green, grr);
      gifputbyte(c[i].blue, grr);
    }
    c += m;
    size -= m;
  }
  gifputbyte(0, grr);
}


static void
graphic_control_extension(Gif_Image *gfi, Gif_Writer *grr)
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
name_extension(char *id, Gif_Writer *grr)
{
  gifputbyte('!', grr);
  gifputbyte(0xCE, grr);
  blast_data((byte *)id, strlen(id), grr);
}


static void
comment_extensions(Gif_Comment *gfcom, Gif_Writer *grr)
{
  int i;
  for (i = 0; i < gfcom->count; i++) {
    gifputbyte('!', grr);
    gifputbyte(0xFE, grr);
    blast_data((byte *)gfcom->str[i], gfcom->len[i], grr);
  }
}


static void
netscape_loop_extension(u_int16_t value, Gif_Writer *grr)
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
gif_write(Gif_Stream *gfs, Gif_Writer *grr)
{
  int i;
  Gif_Image *gfi;
  Gif_Context gfc;
  
  gfc.prefix = Gif_NewArray(Gif_Code, HASH_SIZE);
  gfc.suffix = Gif_NewArray(byte, HASH_SIZE);
  gfc.code = Gif_NewArray(Gif_Code, HASH_SIZE);
  
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
  
  logical_screen_descriptor(gfs, grr);
  
  if (gfs->loopcount > -1)
    netscape_loop_extension(gfs->loopcount, grr);
  
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    if (gfi->comment)
      comment_extensions(gfi->comment, grr);
    if (gfi->identifier)
      name_extension(gfi->identifier, grr);
    if (gfi->transparent != -1 || gfi->disposal || gfi->delay)
      graphic_control_extension(gfi, grr);
    gif_image(gfs, gfi, &gfc, grr);
  }
  
  if (gfs->comment)
    comment_extensions(gfs->comment, grr);
  
  gifputbyte(';', grr);
  
  Gif_DeleteArray(gfc.prefix);
  Gif_DeleteArray(gfc.suffix);
  Gif_DeleteArray(gfc.code);
}


int
Gif_WriteFile(Gif_Stream *gfs, FILE *f)
{
  Gif_Writer grr;
  grr.f = f;
  gif_write(gfs, &grr);
  return 0;
}


#ifdef __cplusplus
}
#endif
