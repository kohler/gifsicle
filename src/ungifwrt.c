/* -*- mode: c; c-basic-offset: 2 -*- */
/* ungifwrt.c - Functions to write unGIFs -- GIFs with run-length compression,
   not LZW compression. Idea due to Hutchinson Avenue Software Corporation
   <http://www.hasc.com>.
   Copyright (C) 1997-2014 Eddie Kohler, ekohler@gmail.com
   This file is part of the LCDF GIF library.

   The LCDF GIF library is free software. It is distributed under the GNU
   Public License, version 2; you can copy, distribute, or alter it at will,
   as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#elif !defined(__cplusplus) && !defined(inline)
/* Assume we don't have inline by default */
# define inline
#endif
#include <lcdfgif/gif.h>
#include <stdarg.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

#define WRITE_BUFFER_SIZE	255

typedef struct Gif_Context {

  Gif_Code *rle_next;

} Gif_Context;


typedef struct Gif_Writer {

  FILE *f;
  uint8_t *v;
  uint32_t pos;
  uint32_t cap;
  Gif_CompressInfo gcinfo;
  int global_size;
  int local_size;
  void (*byte_putter)(uint8_t, struct Gif_Writer *);
  void (*block_putter)(uint8_t *, uint16_t, struct Gif_Writer *);

} Gif_Writer;


#define gifputbyte(b, grr)	((*grr->byte_putter)(b, grr))
#define gifputblock(b, l, grr)	((*grr->block_putter)(b, l, grr))

static inline void
gifputunsigned(uint16_t uns, Gif_Writer *grr)
{
  gifputbyte(uns & 0xFF, grr);
  gifputbyte(uns >> 8, grr);
}


static void
file_byte_putter(uint8_t b, Gif_Writer *grr)
{
  fputc(b, grr->f);
}

static void
file_block_putter(uint8_t *block, uint16_t size, Gif_Writer *grr)
{
  fwrite(block, size, 1, grr->f);
}


static void
memory_byte_putter(uint8_t b, Gif_Writer *grr)
{
  if (grr->pos >= grr->cap) {
    grr->cap *= 2;
    Gif_ReArray(grr->v, uint8_t, grr->cap);
  }
  if (grr->v) {
    grr->v[grr->pos] = b;
    grr->pos++;
  }
}

static void
memory_block_putter(uint8_t *data, uint16_t len, Gif_Writer *grr)
{
  if (grr->pos + len >= grr->cap) {
    grr->cap *= 2;
    Gif_ReArray(grr->v, uint8_t, grr->cap);
  }
  if (grr->v) {
    memcpy(grr->v + grr->pos, data, len);
    grr->pos += len;
  }
}


#ifndef GIF_NO_COMPRESSION

/* Write GIFs compressed with run-length encoding, an idea from code by
   Hutchinson Avenue Software Corporation <http://www.hasc.com> found in
   Thomas Boutell's gd library <http://www.boutell.com>. */

static void
write_compressed_data(uint8_t **img, uint16_t width, uint16_t height,
		      uint8_t min_code_bits, Gif_Context *gfc, Gif_Writer *grr)
{
  uint8_t buffer[WRITE_BUFFER_SIZE];
  uint8_t *buf;

  uint16_t xleft;
  uint8_t *imageline;

  uint32_t leftover;
  uint8_t bits_left_over;

  Gif_Code next_code;
  Gif_Code output_code;
  Gif_Code clear_code;
  Gif_Code eoi_code;
#define CUR_BUMP_CODE (1 << cur_code_bits)
  uint8_t suffix;

  Gif_Code *rle_next = gfc->rle_next;

  uint8_t cur_code_bits;

  /* Here we go! */
  gifputbyte(min_code_bits, grr);
  clear_code = 1 << min_code_bits;
  eoi_code = clear_code + 1;

  cur_code_bits = min_code_bits + 1;
  /* next_code set by first runthrough of output clear_code */
  GIF_DEBUG(("clear(%d) eoi(%d) bits(%d)",clear_code,eoi_code,cur_code_bits));

  output_code = clear_code;
  /* Because output_code is clear_code, we'll initialize next_code, et al.
     below. */

  bits_left_over = 0;
  leftover = 0;
  buf = buffer;
  xleft = width;
  imageline = img[0];

  while (1) {

    /*****
     * Output 'output_code' to the data stream. */

    leftover |= output_code << bits_left_over;
    bits_left_over += cur_code_bits;
    while (bits_left_over >= 8) {
      *buf++ = leftover & 0xFF;
      leftover = (leftover >> 8) & 0x00FFFFFF;
      bits_left_over -= 8;
      if (buf == buffer + WRITE_BUFFER_SIZE) {
	gifputbyte(WRITE_BUFFER_SIZE, grr);
	gifputblock(buffer, WRITE_BUFFER_SIZE, grr);
	buf = buffer;
      }
    }

    if (output_code == clear_code) {
      /* Clear data and prepare gfc */
      Gif_Code c;

      cur_code_bits = min_code_bits + 1;
      next_code = eoi_code + 1;

      for (c = 0; c < clear_code; c++)
	rle_next[c] = clear_code;

    } else if (next_code > CUR_BUMP_CODE) {
      /* bump up compression size */
      if (cur_code_bits == GIF_MAX_CODE_BITS) {
	output_code = clear_code;
	continue;
      } else
	cur_code_bits++;

    } else if (output_code == eoi_code)
      break;


    /*****
     * Find the next code to output. */

    /* If height is 0 -- no more pixels to write -- output eoi_code. */
    if (height == 0)
      output_code = eoi_code;
    else {
      output_code = suffix = *imageline;
      if (output_code >= clear_code)
	/* should not happen unless GIF_WRITE_CAREFUL_MIN_CODE_SIZE */
	output_code = suffix = 0;
      goto next_pixel;

      while (height != 0 && *imageline == suffix
	     && rle_next[output_code] != clear_code) {
	output_code = rle_next[output_code];
       next_pixel:
	imageline++;
	xleft--;
	if (xleft == 0) {
	  xleft = width;
	  height--;
	  img++;
	  imageline = img[0];
	}
      }

      if (height != 0 && *imageline == suffix) {
	rle_next[output_code] = next_code;
	rle_next[next_code] = clear_code;
      }
      next_code++;
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

#else /* GIF_NO_COMPRESSION */

static void
write_compressed_data(uint8_t **img, uint16_t width, uint16_t height,
		      uint8_t min_code_bits, Gif_Context *gfc, Gif_Writer *grr)
{
  uint8_t buffer[WRITE_BUFFER_SIZE];
  uint8_t *buf;

  uint16_t xleft;
  uint8_t *imageline;

  uint32_t leftover;
  uint8_t bits_left_over;

  Gif_Code next_code;
  Gif_Code output_code;
  Gif_Code clear_code;
  Gif_Code eoi_code;
  Gif_Code bump_code;

  uint8_t cur_code_bits;

  /* Here we go! */
  gifputbyte(min_code_bits, grr);
  clear_code = 1 << min_code_bits;
  eoi_code = clear_code + 1;

  cur_code_bits = min_code_bits + 1;
  /* bump_code, next_code set by first runthrough of output clear_code */
  GIF_DEBUG(("clear(%d) eoi(%d)", clear_code, eoi_code));

  output_code = clear_code;
  /* Because output_code is clear_code, we'll initialize next_code, bump_code,
     et al. below. */

  bits_left_over = 0;
  leftover = 0;
  buf = buffer;
  xleft = width;
  imageline = img[0];


  while (1) {

    /*****
     * Output 'output_code' to the data stream. */

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

      cur_code_bits = min_code_bits + 1;
      next_code = eoi_code + 1;
      bump_code = clear_code << 1;

    } else if (next_code == bump_code) {

      /* never bump up compression size -- keep cur_code_bits small by
         generating clear_codes */
      output_code = clear_code;
      continue;

    } else if (output_code == eoi_code)
      break;


    /*****
     * Find the next code to output. */

    /* If height is 0 -- no more pixels to write -- output eoi_code. */
    if (height == 0)
      output_code = eoi_code;
    else {
      output_code = *imageline;
      if (output_code >= clear_code) output_code = 0;

      imageline++;
      xleft--;
      if (xleft == 0) {
	xleft = width;
	height--;
	img++;
	imageline = img[0];
      }

      next_code++;
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

#endif /* GIF_NO_COMPRESSION */


static int
calculate_min_code_bits(Gif_Stream *gfs, Gif_Image *gfi, Gif_Writer *grr)
{
  int colors_used = -1, min_code_bits, i;

  if (grr->gcinfo.flags & GIF_WRITE_CAREFUL_MIN_CODE_SIZE) {
    /* calculate m_c_b based on colormap */
    if (grr->local_size > 0)
      colors_used = grr->local_size;
    else if (grr->global_size > 0)
      colors_used = grr->global_size;

  } else if (gfi->compressed) {
    /* take m_c_b from compressed image */
    colors_used = 1 << gfi->compressed[0];

  } else if (gfi->img) {
    /* calculate m_c_b from uncompressed data */
    int x, y, width = gfi->width, height = gfi->height;
    colors_used = 0;
    for (y = 0; y < height && colors_used < 128; y++) {
      uint8_t *data = gfi->img[y];
      for (x = width; x > 0; x--, data++)
	if (*data > colors_used)
	  colors_used = *data;
    }
    colors_used++;

  } else {
    /* should never happen */
    colors_used = 256;
  }

  min_code_bits = 2;		/* min_code_bits of 1 isn't allowed */
  i = 4;
  while (i < colors_used) {
    min_code_bits++;
    i *= 2;
  }

  if ((grr->gcinfo.flags & GIF_WRITE_CAREFUL_MIN_CODE_SIZE)
      && gfi->compressed && gfi->compressed[0] != min_code_bits) {
    /* if compressed image disagrees with careful min_code_bits, recompress */
    if (Gif_UncompressImage(gfs, gfi))
      Gif_FullCompressImage(gfs, gfi, &grr->gcinfo);
  }

  return min_code_bits;
}

static int
write_image_data(Gif_Image *gfi, uint8_t min_code_bits,
		 Gif_Context *gfc, Gif_Writer *grr)
{
  uint8_t **img = gfi->img;
  uint16_t width = gfi->width, height = gfi->height;

  if (gfi->interlace) {
    uint16_t y;
    uint8_t **nimg = Gif_NewArray(uint8_t *, height + 1);
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


static int get_color_table_size(Gif_Stream *, Gif_Image *, Gif_Writer *);

int
Gif_FullCompressImage(Gif_Stream *gfs, Gif_Image *gfi,
                      const Gif_CompressInfo *gcinfo)
{
  int ok = 0;
  uint8_t min_code_bits;
  Gif_Writer grr;
  Gif_Context gfc;

  if (gfi->compressed && gfi->free_compressed) {
    (*gfi->free_compressed)((void *)gfi->compressed);
    gfi->compressed = 0;
  }

  /* 27.Jul.2001: Must allocate GIF_MAX_CODE + 1 because we assign to
     rle_next[GIF_MAX_CODE]! Thanks, Jeff Brown <jabrown@ipn.caida.org>, for
     supplying the buggy files. */
  gfc.rle_next = Gif_NewArray(Gif_Code, GIF_MAX_CODE + 1);

  grr.v = Gif_NewArray(uint8_t, 1024);
  grr.pos = 0;
  grr.cap = 1024;
  grr.byte_putter = memory_byte_putter;
  grr.block_putter = memory_block_putter;
  if (gcinfo)
    grr.gcinfo = *gcinfo;
  else
    Gif_InitCompressInfo(&grr.gcinfo);
  grr.global_size = get_color_table_size(gfs, 0, &grr);
  grr.local_size = get_color_table_size(gfs, gfi, &grr);

  if (!grr.v || !gfc.rle_next)
    goto done;

  min_code_bits = calculate_min_code_bits(gfs, gfi, &grr);
  ok = write_image_data(gfi, min_code_bits, &gfc, &grr);

 done:
  if (!ok) {
    Gif_DeleteArray(grr.v);
    grr.v = 0;
  }
  gfi->compressed = grr.v;
  gfi->compressed_len = grr.pos;
  gfi->free_compressed = Gif_Free;
  Gif_DeleteArray(gfc.rle_next);
  return grr.v != 0;
}


static int
get_color_table_size(Gif_Stream *gfs, Gif_Image *gfi, Gif_Writer *grr)
{
  Gif_Colormap *gfcm = (gfi ? gfi->local : gfs->global);
  int ncol, totalcol, i;

  if (!gfcm || gfcm->ncol <= 0)
    return 0;

  /* Make sure ncol is reasonable */
  ncol = gfcm->ncol;

  /* Possibly bump up 'ncol' based on 'transparent' values, if
     careful_min_code_bits */
  if (grr->gcinfo.flags & GIF_WRITE_CAREFUL_MIN_CODE_SIZE) {
    if (gfi && gfi->transparent >= ncol)
      ncol = gfi->transparent + 1;
    else if (!gfi)
      for (i = 0; i < gfs->nimages; i++)
	if (gfs->images[i]->transparent >= ncol)
	  ncol = gfs->images[i]->transparent + 1;
  }

  /* Make sure the colormap is a power of two entries! */
  /* GIF format doesn't allow a colormap with only 1 entry. */
  if (ncol > 256)
    ncol = 256;
  for (totalcol = 2; totalcol < ncol; totalcol *= 2)
    /* nada */;

  return totalcol;
}

static void
write_color_table(Gif_Colormap *gfcm, int totalcol, Gif_Writer *grr)
{
  Gif_Color *c = gfcm->col;
  int i, ncol = gfcm->ncol;

  for (i = 0; i < ncol && i < totalcol; i++, c++) {
    gifputbyte(c->gfc_red, grr);
    gifputbyte(c->gfc_green, grr);
    gifputbyte(c->gfc_blue, grr);
  }

  /* Pad out colormap with black. */
  for (; i < totalcol; i++) {
    gifputbyte(0, grr);
    gifputbyte(0, grr);
    gifputbyte(0, grr);
  }
}


static int
write_image(Gif_Stream *gfs, Gif_Image *gfi, Gif_Context *gfc, Gif_Writer *grr)
{
  uint8_t min_code_bits, packed = 0;
  grr->local_size = get_color_table_size(gfs, gfi, grr);

  gifputbyte(',', grr);
  gifputunsigned(gfi->left, grr);
  gifputunsigned(gfi->top, grr);
  gifputunsigned(gfi->width, grr);
  gifputunsigned(gfi->height, grr);

  if (grr->local_size > 0) {
    int size = 2;
    packed |= 0x80;
    while (size < grr->local_size)
      size *= 2, packed++;
  }

  if (gfi->interlace) packed |= 0x40;
  gifputbyte(packed, grr);

  if (grr->local_size > 0)
    write_color_table(gfi->local, grr->local_size, grr);

  /* calculate min_code_bits here (because calculation may involve
     recompression, if GIF_WRITE_CAREFUL_MIN_CODE_SIZE is true) */
  min_code_bits = calculate_min_code_bits(gfs, gfi, grr);

  /* use existing compressed data if it exists. This will tend to whip
     people's asses who uncompress an image, keep the compressed data around,
     but modify the uncompressed data anyway. That sucks. */
  if (gfi->compressed) {
    uint8_t *compressed = gfi->compressed;
    uint32_t compressed_len = gfi->compressed_len;
    while (compressed_len > 0) {
      uint16_t amt = (compressed_len > 0x7000 ? 0x7000 : compressed_len);
      gifputblock(compressed, amt, grr);
      compressed += amt;
      compressed_len -= amt;
    }

  } else
    write_image_data(gfi, min_code_bits, gfc, grr);

  return 1;
}


static void
write_logical_screen_descriptor(Gif_Stream *gfs, Gif_Writer *grr)
{
  uint8_t packed = 0x70;		/* high resolution colors */
  grr->global_size = get_color_table_size(gfs, 0, grr);

  Gif_CalculateScreenSize(gfs, 0);
  gifputunsigned(gfs->screen_width, grr);
  gifputunsigned(gfs->screen_height, grr);

  if (grr->global_size > 0) {
    uint16_t size = 2;
    packed |= 0x80;
    while (size < grr->global_size)
      size *= 2, packed++;
  }

  gifputbyte(packed, grr);
  gifputbyte(gfs->background, grr);
  gifputbyte(0, grr);		/* no aspect ratio information */

  if (grr->global_size > 0)
    write_color_table(gfs->global, grr->global_size, grr);
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
  uint8_t packed = 0;
  gifputbyte('!', grr);
  gifputbyte(0xF9, grr);
  gifputbyte(4, grr);
  if (gfi->transparent >= 0) packed |= 0x01;
  packed |= (gfi->disposal & 0x07) << 2;
  gifputbyte(packed, grr);
  gifputunsigned(gfi->delay, grr);
  gifputbyte((uint8_t)gfi->transparent, grr);
  gifputbyte(0, grr);
}


static void
blast_data(uint8_t *data, int len, Gif_Writer *grr)
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
  blast_data((uint8_t *)id, strlen(id), grr);
}


static void
write_comment_extensions(Gif_Comment *gfcom, Gif_Writer *grr)
{
  int i;
  for (i = 0; i < gfcom->count; i++) {
    gifputbyte('!', grr);
    gifputbyte(0xFE, grr);
    blast_data((uint8_t *)gfcom->str[i], gfcom->len[i], grr);
  }
}


static void
write_netscape_loop_extension(uint16_t value, Gif_Writer *grr)
{
  gifputblock((uint8_t *)"!\xFF\x0BNETSCAPE2.0\x03\x01", 16, grr);
  gifputunsigned(value, grr);
  gifputbyte(0, grr);
}


static void
write_generic_extension(Gif_Extension *gfex, Gif_Writer *grr)
{
  uint32_t pos = 0;
  if (gfex->kind < 0) return;	/* ignore our private extensions */

  gifputbyte('!', grr);
  gifputbyte(gfex->kind, grr);
  if (gfex->kind == 255) {	/* an application extension */
    int len = gfex->application ? strlen(gfex->application) : 0;
    if (len) {
      gifputbyte(len, grr);
      gifputblock((uint8_t *)gfex->application, len, grr);
    }
  }
  while (pos + 255 < gfex->length) {
    gifputbyte(255, grr);
    gifputblock(gfex->data + pos, 255, grr);
    pos += 255;
  }
  if (pos < gfex->length) {
    uint32_t len = gfex->length - pos;
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

  gfc.rle_next = Gif_NewArray(Gif_Code, GIF_MAX_CODE + 1);
  if (!gfc.rle_next)
    goto done;

  {
    uint8_t isgif89a = 0;
    if (gfs->comment || gfs->loopcount > -1)
      isgif89a = 1;
    for (i = 0; i < gfs->nimages && !isgif89a; i++) {
      gfi = gfs->images[i];
      if (gfi->identifier || gfi->transparent != -1 || gfi->disposal ||
	  gfi->delay || gfi->comment)
	isgif89a = 1;
    }
    if (isgif89a)
      gifputblock((uint8_t *)"GIF89a", 6, grr);
    else
      gifputblock((uint8_t *)"GIF87a", 6, grr);
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
  Gif_DeleteArray(gfc.rle_next);
  return ok;
}


int
Gif_FullWriteFile(Gif_Stream *gfs, const Gif_CompressInfo *gcinfo, FILE *f)
{
  Gif_Writer grr;
  grr.f = f;
  grr.byte_putter = file_byte_putter;
  grr.block_putter = file_block_putter;
  if (gcinfo)
    grr.gcinfo = *gcinfo;
  else
    Gif_InitCompressInfo(&grr.gcinfo);
  return write_gif(gfs, &grr);
}


#undef Gif_CompressImage
#undef Gif_WriteFile

int
Gif_CompressImage(Gif_Stream *gfs, Gif_Image *gfi)
{
  return Gif_FullCompressImage(gfs, gfi, 0);
}

int
Gif_WriteFile(Gif_Stream *gfs, FILE *f)
{
  return Gif_FullWriteFile(gfs, 0, f);
}


#ifdef __cplusplus
}
#endif
