/* -*- mode: c; c-basic-offset: 2 -*- */
/* ungifwrt.c - Functions to write unGIFs -- GIFs with run-length compression,
   not LZW compression. Idea due to Hutchinson Avenue Software Corporation
   <http://www.hasc.com>.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of the LCDF GIF library.

   The LCDF GIF library is free software. It is distributed under the GNU
   General Public License, version 2; you can copy, distribute, or alter it at
   will, as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied. */

#if HAVE_CONFIG_H
# include <config.h>
#elif !defined(__cplusplus) && !defined(inline)
/* Assume we don't have inline by default */
# define inline
#endif
#include <lcdfgif/gif.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

#define WRITE_BUFFER_SIZE	255

struct Gif_Writer {
  FILE *f;
  uint8_t *v;
  uint32_t pos;
  uint32_t cap;
  Gif_CompressInfo gcinfo;
  int global_size;
  int local_size;
  int errors;
  int cleared;
  Gif_Code* rle_next;
  void (*byte_putter)(uint8_t, struct Gif_Writer *);
  void (*block_putter)(const uint8_t *, size_t, struct Gif_Writer *);
};


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
file_block_putter(const uint8_t *block, size_t size, Gif_Writer *grr)
{
  if (fwrite(block, 1, size, grr->f) != size)
    grr->errors = 1;
}


static void
memory_byte_putter(uint8_t b, Gif_Writer *grr)
{
  if (grr->pos >= grr->cap) {
    grr->cap = (grr->cap ? grr->cap * 2 : 1024);
    Gif_ReArray(grr->v, uint8_t, grr->cap);
  }
  if (grr->v) {
    grr->v[grr->pos] = b;
    grr->pos++;
  }
}

static void
memory_block_putter(const uint8_t *data, size_t len, Gif_Writer *grr)
{
  while (grr->pos + len >= grr->cap) {
    grr->cap = (grr->cap ? grr->cap * 2 : 1024);
    Gif_ReArray(grr->v, uint8_t, grr->cap);
  }
  if (grr->v) {
    memcpy(grr->v + grr->pos, data, len);
    grr->pos += len;
  }
}


static int
gif_writer_init(Gif_Writer* grr, FILE* f, const Gif_CompressInfo* gcinfo)
{
  grr->f = f;
  grr->v = NULL;
  grr->pos = grr->cap = 0;
  if (gcinfo)
    grr->gcinfo = *gcinfo;
  else
    Gif_InitCompressInfo(&grr->gcinfo);
  grr->errors = 0;
  grr->cleared = 0;
  /* 27.Jul.2001: Must allocate GIF_MAX_CODE + 1 because we assign to
     rle_next[GIF_MAX_CODE]! Thanks, Jeff Brown <jabrown@ipn.caida.org>, for
     supplying the buggy files. */
  grr->rle_next = Gif_NewArray(Gif_Code, GIF_MAX_CODE + 1);
  if (f) {
    grr->byte_putter = file_byte_putter;
    grr->block_putter = file_block_putter;
  } else {
    grr->byte_putter = memory_byte_putter;
    grr->block_putter = memory_block_putter;
  }
  return grr->rle_next != 0;
}

static void
gif_writer_cleanup(Gif_Writer* grr)
{
  Gif_DeleteArray(grr->v);
  Gif_DeleteArray(grr->rle_next);
}


static inline const uint8_t *
gif_imageline(Gif_Image *gfi, unsigned pos)
{
  unsigned y, x;
  if (gfi->width == 0)
    return NULL;
  y = pos / gfi->width;
  x = pos - y * gfi->width;
  if (y == (unsigned) gfi->height)
    return NULL;
  else if (!gfi->interlace)
    return gfi->img[y] + x;
  else
    return gfi->img[Gif_InterlaceLine(y, gfi->height)] + x;
}

static inline unsigned
gif_line_endpos(Gif_Image *gfi, unsigned pos)
{
  unsigned y = pos / gfi->width;
  return (y + 1) * gfi->width;
}

/* Write GIFs compressed with run-length encoding, an idea from code by
   Hutchinson Avenue Software Corporation <http://www.hasc.com> found in
   Thomas Boutell's gd library <http://www.boutell.com>. */

static int
write_compressed_data(Gif_Image *gfi,
		      int min_code_bits, Gif_Writer *grr)
{
  uint8_t stack_buffer[512 - 24];
  uint8_t *buf = stack_buffer;
  unsigned bufpos = 0;
  unsigned bufcap = sizeof(stack_buffer) * 8;

  unsigned pos;
  unsigned clear_bufpos, clear_pos;
  unsigned line_endpos;
  const uint8_t *imageline;

  unsigned run;
#ifndef GIF_NO_COMPRESSION
#define RUN_EWMA_SHIFT 4
#define RUN_EWMA_SCALE 19
#define RUN_INV_THRESH ((unsigned) (1 << RUN_EWMA_SCALE) / 3000)
  unsigned run_ewma;
  Gif_Code* rle_next = grr->rle_next;
#endif

  Gif_Code next_code = 0;
  Gif_Code output_code;
  uint8_t suffix;

  int cur_code_bits;

  /* Here we go! */
  gifputbyte(min_code_bits, grr);
#define CLEAR_CODE      ((Gif_Code) (1 << min_code_bits))
#define EOI_CODE        ((Gif_Code) (CLEAR_CODE + 1))
#define CUR_BUMP_CODE   (1 << cur_code_bits)
  grr->cleared = 0;

  cur_code_bits = min_code_bits + 1;
  /* next_code set by first runthrough of output clear_code */
  GIF_DEBUG(("clear(%d) eoi(%d) bits(%d) ", CLEAR_CODE, EOI_CODE, cur_code_bits));

  output_code = CLEAR_CODE;
  /* Because output_code is clear_code, we'll initialize next_code, et al.
     below. */

  pos = clear_pos = clear_bufpos = 0;
  line_endpos = gfi->width;
  imageline = gif_imageline(gfi, pos);

  while (1) {

    /*****
     * Output 'output_code' to the memory buffer. */
    if (bufpos + 32 >= bufcap) {
      unsigned ncap = bufcap * 2 + (24 << 3);
      uint8_t *nbuf = Gif_NewArray(uint8_t, ncap >> 3);
      if (!nbuf)
        goto error;
      memcpy(nbuf, buf, bufcap >> 3);
      if (buf != stack_buffer)
        Gif_DeleteArray(buf);
      buf = nbuf;
      bufcap = ncap;
    }

    {
      unsigned endpos = bufpos + cur_code_bits;
      do {
        if (bufpos & 7)
          buf[bufpos >> 3] |= output_code << (bufpos & 7);
        else if (bufpos & 0x7FF)
          buf[bufpos >> 3] = output_code >> (bufpos - endpos + cur_code_bits);
        else {
          buf[bufpos >> 3] = 255;
          endpos += 8;
        }

        bufpos += 8 - (bufpos & 7);
      } while (bufpos < endpos);
      bufpos = endpos;
    }


    /*****
     * Handle special codes. */

    if (output_code == CLEAR_CODE) {
      /* Clear data and prepare gfc */
      cur_code_bits = min_code_bits + 1;
      next_code = EOI_CODE + 1;
#ifndef GIF_NO_COMPRESSION
      {
        Gif_Code c;
        for (c = 0; c < CLEAR_CODE; ++c)
          rle_next[c] = CLEAR_CODE;
      }
      run_ewma = 1 << RUN_EWMA_SCALE;
#endif
      run = 0;
      clear_pos = clear_bufpos = 0;

      GIF_DEBUG(("clear "));

    } else if (output_code == EOI_CODE)
      break;

    else {
      if (next_code > CUR_BUMP_CODE && cur_code_bits < GIF_MAX_CODE_BITS)
        /* bump up compression size */
        ++cur_code_bits;

#ifndef GIF_NO_COMPRESSION
      /* Adjust current run length average. */
      run = (run << RUN_EWMA_SCALE) + (1 << (RUN_EWMA_SHIFT - 1));
      if (run < run_ewma)
        run_ewma -= (run_ewma - run) >> RUN_EWMA_SHIFT;
      else
        run_ewma += (run - run_ewma) >> RUN_EWMA_SHIFT;
#else
      if (cur_code_bits != min_code_bits) {
        /* never bump up compression size -- keep cur_code_bits small by
           generating clear_codes */
        output_code = CLEAR_CODE;
        continue;
      }
#endif

      /* Reset run length. */
      run = 0;
    }


    /*****
     * Find the next code to output. */

    /* If height is 0 -- no more pixels to write -- we output work_node next
       time around. */
    if (imageline) {
      output_code = suffix = *imageline;

      while (1) {
        imageline++;
        pos++;
        if (pos == line_endpos) {
          imageline = gif_imageline(gfi, pos);
          line_endpos += gfi->width;
        }
        run++;
#ifndef GIF_NO_COMPRESSION
        if (!imageline || *imageline != suffix || rle_next[output_code] == CLEAR_CODE)
          break;
        output_code = rle_next[output_code];
#else
        break;
#endif
      }

      /* Output the current code. */
      if (next_code < GIF_MAX_CODE) {
#ifndef GIF_NO_COMPRESSION
        if (imageline && *imageline == suffix) {
          rle_next[output_code] = next_code;
          rle_next[next_code] = CLEAR_CODE;
        }
#endif
        next_code++;
      } else
        next_code = GIF_MAX_CODE + 1; /* to match "> CUR_BUMP_CODE" above */

      /* Check whether to clear table. */
      if (next_code > 4094) {
        int do_clear = grr->gcinfo.flags & GIF_WRITE_EAGER_CLEAR;

#ifndef GIF_NO_COMPRESSION
        if (!do_clear) {
          unsigned pixels_left = gfi->width * gfi->height - pos;
          if (pixels_left) {
            /* Always clear if run_ewma gets small relative to
               min_code_bits. Otherwise, clear if #images/run is smaller
               than an empirical threshold, meaning it will take more than
               3000 or so average runs to complete the image. */
            if (run_ewma < ((36U << RUN_EWMA_SCALE) / min_code_bits)
                || pixels_left > UINT_MAX / RUN_INV_THRESH
                || run_ewma < pixels_left * RUN_INV_THRESH)
              do_clear = 1;
          }
        }
#else
        do_clear = 1;
#endif

        if ((do_clear || run < 7) && !clear_pos) {
          clear_pos = pos - run;
          clear_bufpos = bufpos;
        } else if (!do_clear && run > 50)
          clear_pos = clear_bufpos = 0;

        if (do_clear) {
          GIF_DEBUG(("rewind %u pixels/%d bits ", pos - clear_pos, bufpos + cur_code_bits - clear_bufpos));
          output_code = CLEAR_CODE;
          pos = clear_pos;
          imageline = gif_imageline(gfi, pos);
          line_endpos = gif_line_endpos(gfi, pos);
          bufpos = clear_bufpos;
          buf[bufpos >> 3] &= (1 << (bufpos & 7)) - 1;
          grr->cleared = 1;
        }
      }
    } else
      output_code = EOI_CODE;
  }

  /* Output memory buffer to stream. */
  bufpos = (bufpos + 7) >> 3;
  buf[(bufpos - 1) & 0xFFFFFF00] = (bufpos - 1) & 0xFF;
  buf[bufpos] = 0;
  gifputblock(buf, bufpos + 1, grr);

  if (buf != stack_buffer)
    Gif_DeleteArray(buf);
  return 1;

 error:
  if (buf != stack_buffer)
    Gif_DeleteArray(buf);
  return 0;
}


static int
calculate_min_code_bits(Gif_Image *gfi, const Gif_Writer *grr)
{
  int colors_used = -1, min_code_bits, i;

  if (grr->gcinfo.flags & GIF_WRITE_CAREFUL_MIN_CODE_SIZE) {
    /* calculate m_c_b based on colormap */
    if (grr->local_size > 0)
      colors_used = grr->local_size;
    else if (grr->global_size > 0)
      colors_used = grr->global_size;

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

  } else if (gfi->compressed) {
    /* take m_c_b from compressed image */
    colors_used = 1 << gfi->compressed[0];

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

  return min_code_bits;
}


static int get_color_table_size(const Gif_Stream *gfs, Gif_Image *gfi,
				Gif_Writer *grr);

static void
save_compression_result(Gif_Image *gfi, Gif_Writer *grr, int ok)
{
  if (!(grr->gcinfo.flags & GIF_WRITE_SHRINK)
      || (ok && (!gfi->compressed || gfi->compressed_len > grr->pos))) {
    if (gfi->compressed)
      (*gfi->free_compressed)((void *) gfi->compressed);
    if (ok) {
      gfi->compressed_len = grr->pos;
      gfi->compressed_errors = 0;
      gfi->compressed = grr->v;
      gfi->free_compressed = Gif_Free;
      grr->v = 0;
      grr->cap = 0;
    } else
      gfi->compressed = 0;
  }
  grr->pos = 0;
}

int
Gif_FullCompressImage(Gif_Stream *gfs, Gif_Image *gfi,
		      const Gif_CompressInfo *gcinfo)
{
  int ok = 0;
  uint8_t min_code_bits;
  Gif_Writer grr;

  if (!gif_writer_init(&grr, NULL, gcinfo)) {
    if (!(grr.gcinfo.flags & GIF_WRITE_SHRINK))
      Gif_ReleaseCompressedImage(gfi);
    goto done;
  }

  grr.global_size = get_color_table_size(gfs, 0, &grr);
  grr.local_size = get_color_table_size(gfs, gfi, &grr);

  min_code_bits = calculate_min_code_bits(gfi, &grr);
  ok = write_compressed_data(gfi, min_code_bits, &grr);
  save_compression_result(gfi, &grr, ok);

  if ((grr.gcinfo.flags & (GIF_WRITE_OPTIMIZE | GIF_WRITE_EAGER_CLEAR))
      == GIF_WRITE_OPTIMIZE
      && grr.cleared && ok) {
    grr.gcinfo.flags |= GIF_WRITE_EAGER_CLEAR | GIF_WRITE_SHRINK;
    if (write_compressed_data(gfi, min_code_bits, &grr))
      save_compression_result(gfi, &grr, 1);
  }

 done:
  gif_writer_cleanup(&grr);
  return ok;
}


static int
get_color_table_size(const Gif_Stream *gfs, Gif_Image *gfi, Gif_Writer *grr)
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
write_image(Gif_Stream *gfs, Gif_Image *gfi, Gif_Writer *grr)
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
  min_code_bits = calculate_min_code_bits(gfi, grr);

  /* use existing compressed data if it exists. This will tend to whip
     people's asses who uncompress an image, keep the compressed data around,
     but modify the uncompressed data anyway. That sucks. */
  if (gfi->compressed
      && (!(grr->gcinfo.flags & GIF_WRITE_CAREFUL_MIN_CODE_SIZE)
          || gfi->compressed[0] == min_code_bits)) {
    uint8_t *compressed = gfi->compressed;
    uint32_t compressed_len = gfi->compressed_len;
    while (compressed_len > 0) {
      uint16_t amt = (compressed_len > 0x7000 ? 0x7000 : compressed_len);
      gifputblock(compressed, amt, grr);
      compressed += amt;
      compressed_len -= amt;
    }

  } else if (!gfi->img) {
    Gif_UncompressImage(gfs, gfi);
    write_compressed_data(gfi, min_code_bits, grr);
    Gif_ReleaseUncompressedImage(gfi);

  } else
    write_compressed_data(gfi, min_code_bits, grr);

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
  if (gfs->background < grr->global_size)
    gifputbyte(gfs->background, grr);
  else
    gifputbyte(255, grr);
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
blast_data(const uint8_t *data, int len, Gif_Writer *grr)
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
    blast_data((const uint8_t *)gfcom->str[i], gfcom->len[i], grr);
  }
}


static void
write_netscape_loop_extension(uint16_t value, Gif_Writer *grr)
{
  gifputblock((const uint8_t *)"!\xFF\x0BNETSCAPE2.0\x03\x01", 16, grr);
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
    if (gfex->applength) {
      gifputbyte(gfex->applength, grr);
      gifputblock((const uint8_t*) gfex->appname, gfex->applength, grr);
    }
  }
  if (gfex->packetized)
    gifputblock(gfex->data, gfex->length, grr);
  else {
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
  }
  gifputbyte(0, grr);
}

static int
write_gif(Gif_Stream *gfs, Gif_Writer *grr)
{
  Gif_Extension* gfex;
  int ok = 0;
  int i;

  {
    uint8_t isgif89a = 0;
    if (gfs->end_comment || gfs->end_extension_list || gfs->loopcount > -1)
      isgif89a = 1;
    for (i = 0; i < gfs->nimages && !isgif89a; i++) {
      Gif_Image* gfi = gfs->images[i];
      if (gfi->identifier || gfi->transparent != -1 || gfi->disposal
          || gfi->delay || gfi->comment || gfi->extension_list)
        isgif89a = 1;
    }
    if (isgif89a)
      gifputblock((const uint8_t *)"GIF89a", 6, grr);
    else
      gifputblock((const uint8_t *)"GIF87a", 6, grr);
  }

  write_logical_screen_descriptor(gfs, grr);

  if (gfs->loopcount > -1)
    write_netscape_loop_extension(gfs->loopcount, grr);

  for (i = 0; i < gfs->nimages; i++)
    if (!Gif_IncrementalWriteImage(grr, gfs, gfs->images[i]))
      goto done;

  for (gfex = gfs->end_extension_list; gfex; gfex = gfex->next)
    write_generic_extension(gfex, grr);
  if (gfs->end_comment)
    write_comment_extensions(gfs->end_comment, grr);

  gifputbyte(';', grr);
  ok = 1;

 done:
  return ok;
}


int
Gif_FullWriteFile(Gif_Stream *gfs, const Gif_CompressInfo *gcinfo,
		  FILE *f)
{
  Gif_Writer grr;
  int ok = gif_writer_init(&grr, f, gcinfo)
           && write_gif(gfs, &grr);
  gif_writer_cleanup(&grr);
  return ok;
}


Gif_Writer*
Gif_IncrementalWriteFileInit(Gif_Stream* gfs, const Gif_CompressInfo* gcinfo,
                             FILE *f)
{
    Gif_Writer* grr = Gif_New(Gif_Writer);
    if (!grr || !gif_writer_init(grr, f, gcinfo)) {
        Gif_Delete(grr);
        return NULL;
    }
    gifputblock((const uint8_t *)"GIF89a", 6, grr);
    write_logical_screen_descriptor(gfs, grr);
    if (gfs->loopcount > -1)
        write_netscape_loop_extension(gfs->loopcount, grr);
    return grr;
}

int
Gif_IncrementalWriteImage(Gif_Writer* grr, Gif_Stream* gfs, Gif_Image* gfi)
{
    Gif_Extension *gfex;
    for (gfex = gfi->extension_list; gfex; gfex = gfex->next)
        write_generic_extension(gfex, grr);
    if (gfi->comment)
        write_comment_extensions(gfi->comment, grr);
    if (gfi->identifier)
        write_name_extension(gfi->identifier, grr);
    if (gfi->transparent != -1 || gfi->disposal || gfi->delay)
        write_graphic_control_extension(gfi, grr);
    return write_image(gfs, gfi, grr);
}

int
Gif_IncrementalWriteComplete(Gif_Writer* grr, Gif_Stream* gfs)
{
    Gif_Extension* gfex;
    for (gfex = gfs->end_extension_list; gfex; gfex = gfex->next)
        write_generic_extension(gfex, grr);
    if (gfs->end_comment)
        write_comment_extensions(gfs->end_comment, grr);
    gifputbyte(';', grr);
    gif_writer_cleanup(grr);
    Gif_Delete(grr);
    return 1;
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
