/* gifread.c - Functions to read GIFs.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of the LCDF GIF library.

   The LCDF GIF library is free software. It is distributed under the GNU
   General Public License, version 2; you can copy, distribute, or alter it at
   will, as long as this notice is kept intact and this source code is made
   available. There is no warranty, express or implied. */

#if HAVE_CONFIG_H
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

  Gif_Image* gfi;
  Gif_ReadErrorHandler handler;
  int errors[2];

} Gif_Context;


typedef struct Gif_Reader {
  FILE *f;
  const uint8_t *v;
  uint32_t pos;
  uint32_t length;
  int is_record;
  int is_eoi;
  uint8_t (*byte_getter)(struct Gif_Reader *);
  uint32_t (*block_getter)(uint8_t*, uint32_t, struct Gif_Reader*);
  int (*eofer)(struct Gif_Reader *);
} Gif_Reader;

static Gif_ReadErrorHandler default_error_handler = 0;


#define gifgetc(grr)    ((char)(*grr->byte_getter)(grr))
#define gifgetbyte(grr) ((*grr->byte_getter)(grr))
#define gifgetblock(ptr, size, grr) ((*grr->block_getter)(ptr, size, grr))
#define gifeof(grr)     ((*grr->eofer)(grr))

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
    if (i != EOF) {
        ++grr->pos;
        return i;
    } else
        return 0;
}

static uint32_t
file_block_getter(uint8_t *p, uint32_t s, Gif_Reader *grr)
{
    size_t nread = fread(p, 1, s, grr->f);
    if (nread < s)
        memset(p + nread, 0, s - nread);
    grr->pos += nread;
    return nread;
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
    if (grr->pos < grr->length)
        return grr->v[grr->pos++];
    else
        return 0;
}

static uint32_t
record_block_getter(uint8_t *p, uint32_t s, Gif_Reader *grr)
{
    uint32_t ncopy = (grr->pos + s <= grr->length ? s : grr->length - grr->pos);
    memcpy(p, &grr->v[grr->pos], ncopy);
    grr->pos += ncopy;
    if (ncopy < s)
        memset(p + ncopy, 0, s - ncopy);
    return ncopy;
}

static int
record_eofer(Gif_Reader *grr)
{
    return grr->pos == grr->length;
}


static void
make_data_reader(Gif_Reader *grr, const uint8_t *data, uint32_t length)
{
  grr->v = data;
  grr->pos = 0;
  grr->length = length;
  grr->is_record = 1;
  grr->byte_getter = record_byte_getter;
  grr->block_getter = record_block_getter;
  grr->eofer = record_eofer;
}


static void
gif_read_error(Gif_Context *gfc, int is_error, const char *text)
{
    Gif_ReadErrorHandler handler = gfc->handler ? gfc->handler : default_error_handler;
    if (is_error >= 0)
        gfc->errors[is_error > 0] += 1;
    if (handler)
        handler(gfc->stream, gfc->gfi, is_error, text);
}


static uint8_t
one_code(Gif_Context *gfc, Gif_Code code)
{
  uint8_t *suffixes = gfc->suffix;
  Gif_Code *prefixes = gfc->prefix;
  uint8_t *ptr;
  int lastsuffix = 0;
  int codelength = gfc->length[code];

  gfc->decodepos += codelength;
  ptr = gfc->image + gfc->decodepos;
  while (codelength > 0) {
      lastsuffix = suffixes[code];
      code = prefixes[code];
      --ptr;
      if (ptr < gfc->maximage)
          *ptr = lastsuffix;
      --codelength;
  }

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
      /* Need to shift down the upper, unused part of 'buffer' */
      int i = bit_position / 8;
      buffer[0] = buffer[i];
      buffer[1] = buffer[i+1];
      bit_position -= i * 8;
      bit_length -= i * 8;
    }
    block_len = gifgetbyte(grr);
    GIF_DEBUG(("\nimage_block(%d) ", block_len));
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
  GIF_DEBUG(("\n\nmin_code_size(%d) ", min_code_size));
  if (min_code_size >= GIF_MAX_CODE_BITS) {
    gif_read_error(gfc, 1, "image corrupted, min_code_size too big");
    min_code_size = GIF_MAX_CODE_BITS - 1;
  } else if (min_code_size < 2) {
    gif_read_error(gfc, 1, "image corrupted, min_code_size too small");
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
  /* Thus the 'Read in the next data block.' code below will be invoked on the
     first time through: exactly right! */

  while (1) {

    old_code = code;

    /* GET A CODE INTO THE 'code' VARIABLE.
     *
     * 9.Dec.1998 - Rather than maintain a byte pointer and a bit offset into
     * the current byte (and the processing associated with that), we maintain
     * one number: the offset, in bits, from the beginning of 'buffer'. This
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

    GIF_DEBUG(("%d ", code));

    /* CHECK FOR SPECIAL OR BAD CODES: clear_code, eoi_code, or a code that is
     * too large. */
    if (code == clear_code) {
      GIF_DEBUG(("clear "));
      bits_needed = min_code_size + 1;
      next_code = eoi_code;
      continue;

    } else if (code == eoi_code)
      break;

    else if (code > next_code && next_code && next_code != clear_code) {
      /* code > next_code: a (hopefully recoverable) error.

         Bug fix, 5/27: Do this even if old_code == clear_code, and set code
         to 0 to prevent errors later. (If we didn't zero code, we'd later set
         old_code = code; then we had old_code >= next_code; so the prefixes
         array got all screwed up!)

         Bug fix, 4/12/2010: It is not an error if next_code == clear_code.
         This happens at the end of a large GIF: see the next comment ("If no
         meaningful next code should be defined...."). */
      if (gfc->errors[1] < 20)
          gif_read_error(gfc, 1, "image corrupted, code out of range");
      else if (gfc->errors[1] == 20)
          gif_read_error(gfc, 1, "(not reporting more errors)");
      code = 0;
    }

    /* PROCESS THE CURRENT CODE and define the next code. If no meaningful
     * next code should be defined, then we have set next_code to either
     * 'eoi_code' or 'clear_code' -- so we'll store useless prefix/suffix data
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
    /* 7.Mar.2014 -- Avoid error if image has zero width/height. */
    if (code == next_code && gfc->image + gfc->decodepos <= gfc->maximage)
      gfc->image[gfc->decodepos - 1] = gfc->suffix[next_code];

    /* Increment next_code except for the 'clear_code' special case (that's
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
 zero_length_block: {
      long delta = (long) (gfc->maximage - gfc->image) - (long) gfc->decodepos;
      char buf[BUFSIZ];
      if (delta > 0) {
          sprintf(buf, "missing %ld %s of image data", delta,
                  delta == 1 ? "pixel" : "pixels");
          gif_read_error(gfc, 1, buf);
          memset(&gfc->image[gfc->decodepos], 0, delta);
      } else if (delta < -1) {
          /* One pixel of superfluous data is OK; that could be the
             code == next_code case. */
          sprintf(buf, "%ld superfluous pixels of image data", -delta);
          gif_read_error(gfc, 0, buf);
      }
  }
}


static Gif_Colormap *
read_color_table(int size, Gif_Reader *grr)
{
  Gif_Colormap *gfcm = Gif_NewFullColormap(size, size);
  Gif_Color *c;
  if (!gfcm) return 0;

  GIF_DEBUG(("colormap(%d) ", size));
  for (c = gfcm->col; size; size--, c++) {
    c->gfc_red = gifgetbyte(grr);
    c->gfc_green = gifgetbyte(grr);
    c->gfc_blue = gifgetbyte(grr);
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
  } else
      gfs->background = 256;

  return 1;
}


static int
read_compressed_image(Gif_Image *gfi, Gif_Reader *grr, int read_flags)
{
  if (grr->is_record) {
    const uint32_t image_pos = grr->pos;

    /* scan over image */
    ++grr->pos; /* skip min code size */
    while (grr->pos < grr->length) {
        int amt = grr->v[grr->pos];
        grr->pos += amt + 1;
        if (amt == 0)
            break;
    }
    if (grr->pos > grr->length)
        grr->pos = grr->length;

    gfi->compressed_len = grr->pos - image_pos;
    gfi->compressed_errors = 0;
    if (read_flags & GIF_READ_CONST_RECORD) {
      gfi->compressed = (uint8_t*) &grr->v[image_pos];
      gfi->free_compressed = 0;
    } else {
      gfi->compressed = Gif_NewArray(uint8_t, gfi->compressed_len);
      gfi->free_compressed = Gif_Free;
      if (!gfi->compressed) return 0;
      memcpy(gfi->compressed, &grr->v[image_pos], gfi->compressed_len);
    }

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

    gfi->compressed_len = comp_len;
    gfi->compressed_errors = 0;
    gfi->compressed = comp;
    gfi->free_compressed = Gif_Free;
  }

  return 1;
}


static int
uncompress_image(Gif_Context *gfc, Gif_Image *gfi, Gif_Reader *grr)
{
    int old_nerrors;
    if (!Gif_CreateUncompressedImage(gfi, gfi->interlace))
        return 0;
    gfc->width = gfi->width;
    gfc->height = gfi->height;
    gfc->image = gfi->image_data;
    gfc->maximage = gfi->image_data + (unsigned) gfi->width * (unsigned) gfi->height;
    old_nerrors = gfc->errors[1];
    read_image_data(gfc, grr);
    gfi->compressed_errors = gfc->errors[1] - old_nerrors;
    return 1;
}


int
Gif_FullUncompressImage(Gif_Stream* gfs, Gif_Image* gfi,
                        Gif_ReadErrorHandler h)
{
  Gif_Context gfc;
  Gif_Reader grr;
  int ok = 0;

  /* return right away if image is already uncompressed. this might screw over
     people who expect re-uncompressing to restore the compressed version. */
  if (gfi->img)
    return 2;
  if (gfi->image_data)
    /* we have uncompressed data, but not an 'img' array;
       this shouldn't happen */
    return 0;

  gfc.stream = gfs;
  gfc.gfi = gfi;
  gfc.prefix = Gif_NewArray(Gif_Code, GIF_MAX_CODE);
  gfc.suffix = Gif_NewArray(uint8_t, GIF_MAX_CODE);
  gfc.length = Gif_NewArray(uint16_t, GIF_MAX_CODE);
  gfc.handler = h;
  gfc.errors[0] = gfc.errors[1] = 0;

  if (gfc.prefix && gfc.suffix && gfc.length && gfi->compressed) {
    make_data_reader(&grr, gfi->compressed, gfi->compressed_len);
    ok = uncompress_image(&gfc, gfi, &grr);
  }

  Gif_DeleteArray(gfc.prefix);
  Gif_DeleteArray(gfc.suffix);
  Gif_DeleteArray(gfc.length);
  if (gfc.errors[0] || gfc.errors[1])
      gif_read_error(&gfc, -1, 0);
  return ok && !gfc.errors[1];
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
  /* Mainline GIF processors (Firefox, etc.) process missing width (height)
     as screen_width (screen_height). */
  if (gfi->width == 0)
      gfi->width = gfc->stream->screen_width;
  if (gfi->height == 0)
      gfi->height = gfc->stream->screen_height;
  /* If still zero, error. */
  if (gfi->width == 0 || gfi->height == 0) {
      gif_read_error(gfc, 1, "image has zero width and/or height");
      Gif_MakeImageEmpty(gfi);
      read_flags = 0;
  }
  /* If position out of range, error. */
  if ((unsigned) gfi->left + (unsigned) gfi->width > 0xFFFF
      || (unsigned) gfi->top + (unsigned) gfi->height > 0xFFFF) {
      gif_read_error(gfc, 1, "image position and/or dimensions out of range");
      Gif_MakeImageEmpty(gfi);
      read_flags = 0;
  }
  GIF_DEBUG(("<%ux%u> ", gfi->width, gfi->height));

  packed = gifgetbyte(grr);
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
    gif_read_error(gfc, 1, "bad graphic extension");
    gifgetblock(crap, len, grr);
  }

  len = gifgetbyte(grr);
  while (len > 0) {
    gif_read_error(gfc, 1, "bad graphic extension");
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
    gifgetblock((uint8_t *)data + total_len, len, grr);

    total_len += len;
    data[total_len] = 0;

    len = gifgetbyte(grr);
  }

  if (store_len) *store_len = total_len;
  return data;
}


static int
read_unknown_extension(Gif_Context* gfc, Gif_Reader* grr,
                       int kind, char* appname, int applength)
{
    uint8_t block_len = gifgetbyte(grr);
    uint8_t* data = 0;
    int data_len = 0;
    Gif_Extension *gfex = 0;

    while (block_len > 0) {
        Gif_ReArray(data, uint8_t, data_len + block_len + 2);
        if (!data)
            goto done;
        data[data_len] = block_len;
        gifgetblock(data + data_len + 1, block_len, grr);
        data_len += block_len + 1;
        block_len = gifgetbyte(grr);
    }

    if (data)
        gfex = Gif_NewExtension(kind, appname, applength);
    if (gfex) {
        gfex->data = data;
        gfex->free_data = Gif_Free;
        gfex->length = data_len;
        gfex->packetized = 1;
        data[data_len] = 0;
        Gif_AddExtension(gfc->stream, gfc->gfi, gfex);
    }

 done:
    if (!gfex)
        Gif_DeleteArray(data);
    while (block_len > 0) {
        uint8_t buffer[GIF_MAX_BLOCK];
        gifgetblock(buffer, block_len, grr);
        block_len = gifgetbyte(grr);
    }
    return gfex != 0;
}


static int
read_application_extension(Gif_Context *gfc, Gif_Reader *grr)
{
  Gif_Stream *gfs = gfc->stream;
  uint8_t buffer[GIF_MAX_BLOCK + 1];
  uint8_t len = gifgetbyte(grr);
  gifgetblock(buffer, len, grr);

  /* Read the Netscape loop extension. */
  if (len == 11
      && (memcmp(buffer, "NETSCAPE2.0", 11) == 0
          || memcmp(buffer, "ANIMEXTS1.0", 11) == 0)) {

    len = gifgetbyte(grr);
    if (len == 3) {
      gifgetbyte(grr); /* throw away the 1 */
      gfs->loopcount = gifgetunsigned(grr);
      len = gifgetbyte(grr);
      if (len)
        gif_read_error(gfc, 1, "bad loop extension");
    } else
      gif_read_error(gfc, 1, "bad loop extension");

    while (len > 0) {
      gifgetblock(buffer, len, grr);
      len = gifgetbyte(grr);
    }
    return 1;

  } else
    return read_unknown_extension(gfc, grr, 0xFF, (char*)buffer, len);
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
         const char* landmark, Gif_ReadErrorHandler handler)
{
  Gif_Stream *gfs;
  Gif_Image *gfi;
  Gif_Context gfc;
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
  gfc.gfi = gfi;
  gfc.errors[0] = gfc.errors[1] = 0;

  if (!gfs || !gfi || !gfc.prefix || !gfc.suffix || !gfc.length)
    goto done;
  gfs->landmark = landmark;

  GIF_DEBUG(("\nGIF "));
  if (!read_logical_screen_descriptor(gfs, grr))
    goto done;
  GIF_DEBUG(("logscrdesc "));

  while (!gifeof(grr)) {

    uint8_t block = gifgetbyte(grr);

    switch (block) {

     case ',': /* image block */
      GIF_DEBUG(("imageread %d ", gfs->nimages));

      gfi->identifier = last_name;
      last_name = 0;
      if (!Gif_AddImage(gfs, gfi))
          goto done;
      else if (!read_image(grr, &gfc, gfi, read_flags)) {
          Gif_RemoveImage(gfs, gfs->nimages - 1);
          gfi = 0;
          goto done;
      }

      gfc.gfi = gfi = Gif_NewImage();
      if (!gfi)
          goto done;
      break;

     case ';': /* terminator */
      GIF_DEBUG(("term\n"));
      goto done;

     case '!': /* extension */
      block = gifgetbyte(grr);
      GIF_DEBUG(("ext(0x%02X) ", block));
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
        read_application_extension(&gfc, grr);
        break;

       default:
        read_unknown_extension(&gfc, grr, block, 0, 0);
        break;

      }
      break;

     default:
       if (!unknown_block_type) {
         char buf[256];
         sprintf(buf, "unknown block type %d at file offset %u", block, grr->pos - 1);
         gif_read_error(&gfc, 1, buf);
       }
       if (++unknown_block_type > 20)
         goto done;
       break;

    }

  }

 done:

  /* Move comments and extensions after last image into stream. */
  if (gfs && gfi) {
      Gif_Extension* gfex;
      gfs->end_comment = gfi->comment;
      gfi->comment = 0;
      gfs->end_extension_list = gfi->extension_list;
      gfi->extension_list = 0;
      for (gfex = gfs->end_extension_list; gfex; gfex = gfex->next)
          gfex->image = NULL;
  }

  Gif_DeleteImage(gfi);
  Gif_DeleteArray(last_name);
  Gif_DeleteArray(gfc.prefix);
  Gif_DeleteArray(gfc.suffix);
  Gif_DeleteArray(gfc.length);
  gfc.gfi = 0;
  last_name = 0;

  if (gfs)
    gfs->errors = gfc.errors[1];
  if (gfs && gfc.errors[1] == 0
      && !(read_flags & GIF_READ_TRAILING_GARBAGE_OK)
      && !grr->eofer(grr))
      gif_read_error(&gfc, 0, "trailing garbage after GIF ignored");
  /* finally, export last message */
  gif_read_error(&gfc, -1, 0);

  return gfs;
}


Gif_Stream *
Gif_FullReadFile(FILE *f, int read_flags,
                 const char* landmark, Gif_ReadErrorHandler h)
{
  Gif_Reader grr;
  if (!f) return 0;
  grr.f = f;
  grr.pos = 0;
  grr.is_record = 0;
  grr.byte_getter = file_byte_getter;
  grr.block_getter = file_block_getter;
  grr.eofer = file_eofer;
  return read_gif(&grr, read_flags, landmark, h);
}

Gif_Stream *
Gif_FullReadRecord(const Gif_Record *gifrec, int read_flags,
                   const char* landmark, Gif_ReadErrorHandler h)
{
  Gif_Reader grr;
  if (!gifrec) return 0;
  make_data_reader(&grr, gifrec->data, gifrec->length);
  if (read_flags & GIF_READ_CONST_RECORD)
    read_flags |= GIF_READ_COMPRESSED;
  return read_gif(&grr, read_flags, landmark, h);
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

void
Gif_SetErrorHandler(Gif_ReadErrorHandler handler) {
    default_error_handler = handler;
}

#ifdef __cplusplus
}
#endif
