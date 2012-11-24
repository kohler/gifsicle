/* -*- mode: c; c-basic-offset: 2 -*- */
/* gifwrite.c - Functions to write GIFs.
   Copyright (C) 1997-2011 Eddie Kohler, kohler@cs.ucla.edu
   This file is part of the LCDF GIF library.

   The LCDF GIF library is free software. It is distributed under the GNU
   General Public License, version 2; you can copy, distribute, or alter it at
   will, as long as this notice is kept intact and this source code is made
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
#define NODES_SIZE		GIF_MAX_CODE
#define LINKS_SIZE		GIF_MAX_CODE

/* 1.Aug.1999 - Removed code hashing in favor of an adaptive tree strategy
   based on Whirlgif-3.04, written by Hans Dinsen-Hansen <dino@danbbs.dk>. Mr.
   Dinsen-Hansen brought the adaptive tree strategy to my attention and argued
   at length that it was better than hashing. In fact, he was right: it runs a
   lot faster. However, it does NOT create "better" results in any way.

   Each code is represented by a Node. The Nodes form a tree with variable
   fan-out -- up to 'clear_code' children per Node. There are two kinds of
   Node, TABLE and LINKS. In a TABLE node, the children are stored in a table
   indexed by suffix -- thus, it's very efficient to determine a given child.
   In a LINKS node, the existent children are stored in a linked list. This is
   slightly slower to access. When a LINKS node gets more than
   'MAX_LINKS_TYPE-1' children, it is converted to a TABLE node. (This is why
   it's an adaptive tree.)

   Problems with this implementation: MAX_LINKS_TYPE is fixed, so GIFs with
   very small numbers of colors (2, 4, 8) won't get the speed benefits of
   TABLE nodes. */

#define TABLE_TYPE		0
#define LINKS_TYPE		1
#define MAX_LINKS_TYPE		5
typedef struct Gif_Node {
  Gif_Code code;
  uint8_t type;
  uint8_t suffix;
  struct Gif_Node *sibling;
  union {
    struct Gif_Node *s;
    struct Gif_Node **m;
  } child;
} Gif_Node;


typedef struct Gif_CodeTable {
  Gif_Node *nodes;
  int nodes_pos;
  Gif_Node **links;
  int links_pos;
  int clear_code;
} Gif_CodeTable;


typedef struct Gif_Writer {
  FILE *f;
  uint8_t *v;
  uint32_t pos;
  uint32_t cap;
  Gif_CompressInfo gcinfo;
  int global_size;
  int local_size;
  int errors;
  int cleared;
  void (*byte_putter)(uint8_t, struct Gif_Writer *);
  void (*block_putter)(const uint8_t *, uint16_t, struct Gif_Writer *);
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
file_block_putter(const uint8_t *block, uint16_t size, Gif_Writer *grr)
{
  if (fwrite(block, 1, size, grr->f) != size)
    grr->errors = 1;
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
memory_block_putter(const uint8_t *data, uint16_t len, Gif_Writer *grr)
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


static int
gfc_init(Gif_CodeTable *gfc)
{
  gfc->nodes = Gif_NewArray(Gif_Node, NODES_SIZE);
  gfc->links = Gif_NewArray(Gif_Node *, LINKS_SIZE);
  return gfc->nodes && gfc->links;
}

static inline void
gfc_clear(Gif_CodeTable *gfc, Gif_Code clear_code)
{
  int c;
  /* The first clear_code nodes are reserved for single-pixel codes */
  gfc->nodes_pos = clear_code;
  gfc->links_pos = 0;
  for (c = 0; c < clear_code; c++) {
    gfc->nodes[c].code = c;
    gfc->nodes[c].type = LINKS_TYPE;
    gfc->nodes[c].suffix = c;
    gfc->nodes[c].child.s = 0;
  }
  gfc->clear_code = clear_code;
}

static inline Gif_Node *
gfc_lookup(Gif_CodeTable *gfc, Gif_Node *node, uint8_t suffix)
{
  if (!node)
    return &gfc->nodes[suffix];
  else if (node->type == TABLE_TYPE)
    return node->child.m[suffix];
  else {
    for (node = node->child.s; node; node = node->sibling)
      if (node->suffix == suffix)
        return node;
    return NULL;
  }
}

static void
gfc_change_node_to_table(Gif_CodeTable *gfc, Gif_Node *work_node,
                         Gif_Node *next_node)
{
  /* change links node to table node */
  Gif_Code c;
  Gif_Node **table = &gfc->links[gfc->links_pos];
  Gif_Node *n;
  gfc->links_pos += gfc->clear_code;

  for (c = 0; c < gfc->clear_code; c++)
    table[c] = 0;
  table[next_node->suffix] = next_node;
  for (n = work_node->child.s; n; n = n->sibling)
    table[n->suffix] = n;

  work_node->type = TABLE_TYPE;
  work_node->child.m = table;
}

static inline void
gfc_define(Gif_CodeTable *gfc, Gif_Node *work_node, uint8_t suffix,
           Gif_Code next_code)
{
  /* Add a new code to our dictionary. First reserve a node for the
     added code. It's LINKS_TYPE at first. */
  Gif_Node *next_node = &gfc->nodes[gfc->nodes_pos];
  gfc->nodes_pos++;
  next_node->code = next_code;
  next_node->type = LINKS_TYPE;
  next_node->suffix = suffix;
  next_node->child.s = 0;

  /* link next_node into work_node's set of children */
  if (work_node->type == TABLE_TYPE)
    work_node->child.m[suffix] = next_node;
  else if (work_node->type < MAX_LINKS_TYPE
           || gfc->links_pos + gfc->clear_code > LINKS_SIZE) {
    next_node->sibling = work_node->child.s;
    work_node->child.s = next_node;
    work_node->type++;
  } else
    gfc_change_node_to_table(gfc, work_node, next_node);
}

static inline const uint8_t *
gif_imageline(Gif_Image *gfi, int y)
{
  if (y == gfi->height)
    return NULL;
  else if (!gfi->interlace)
    return gfi->img[y];
  else
    return gfi->img[Gif_InterlaceLine(y, gfi->height)];
}

static int
write_compressed_data(Gif_Image *gfi,
		      int min_code_bits, Gif_CodeTable *gfc, Gif_Writer *grr)
{
  uint8_t stack_buffer[232];
  uint8_t *buf = stack_buffer;
  unsigned bufpos = 0;
  unsigned bufcap = sizeof(stack_buffer) * 8;

  unsigned xleft;
  unsigned ypos;
  const uint8_t *imageline;

  Gif_Node *work_node;
  int work_depth;
  Gif_Node *next_node;
  Gif_Code next_code = 0;
  Gif_Code output_code;
#define CUR_BUMP_CODE (1 << cur_code_bits)
  uint8_t suffix;

  int end_table_avg_depth = 0;
  int end_table_count = 0;

  int cur_code_bits;

  /* Here we go! */
  gifputbyte(min_code_bits, grr);
#define CLEAR_CODE	((Gif_Code) (1 << min_code_bits))
#define EOI_CODE	((Gif_Code) (CLEAR_CODE + 1))
  grr->cleared = 0;

  cur_code_bits = min_code_bits + 1;
  /* next_code set by first runthrough of output clear_code */
  GIF_DEBUG(("clear(%d) eoi(%d) bits(%d)", CLEAR_CODE, EOI_CODE, cur_code_bits));

  work_node = 0;
  work_depth = 0;
  output_code = CLEAR_CODE;
  /* Because output_code is clear_code, we'll initialize next_code, et al.
     below. */

  xleft = gfi->width;
  ypos = 0;
  imageline = gif_imageline(gfi, ypos);

  while (1) {

    /*****
     * Output 'output_code' to the memory buffer. */
    {
      unsigned startpos = bufpos;
      do {
        if (bufpos == bufcap) {
          unsigned ncap = bufcap * 2 + (24 << 3);
          uint8_t *nbuf = Gif_NewArray(uint8_t, ncap >> 3);
          if (!nbuf)
            return 0;
          memcpy(nbuf, buf, bufcap >> 3);
          if (buf != stack_buffer)
            Gif_DeleteArray(buf);
          buf = nbuf;
          bufcap = ncap;
        }

        if (bufpos & 7)
          buf[bufpos >> 3] |= output_code << (bufpos & 7);
        else
          buf[bufpos >> 3] = output_code >> (bufpos - startpos);

        bufpos += 8 - (bufpos & 7);
      } while (bufpos < startpos + cur_code_bits);
      bufpos = startpos + cur_code_bits;
    }


    /*****
     * Handle special codes. */

    if (output_code == CLEAR_CODE) {
      /* Clear data and prepare gfc */
      cur_code_bits = min_code_bits + 1;
      next_code = EOI_CODE + 1;
      gfc_clear(gfc, CLEAR_CODE);

      GIF_DEBUG(("\n"));

    } else if (output_code == EOI_CODE)
      break;

    else if (next_code > CUR_BUMP_CODE) {
      /* bump up compression size */
      if (cur_code_bits < GIF_MAX_CODE_BITS)
	++cur_code_bits;
      else if ((end_table_count > 8 /* totally arbitrary constants */
		&& end_table_avg_depth < 12 * 128)
	       || (grr->gcinfo.flags & GIF_WRITE_EAGER_CLEAR)) {
	output_code = CLEAR_CODE;
	grr->cleared = 1;
	continue;
      }
    }


    /*****
     * Find the next code to output. */

    /* If height is 0 -- no more pixels to write -- we output work_node next
       time around. */
    while (imageline) {
      suffix = *imageline;
      next_node = gfc_lookup(gfc, work_node, suffix);

      imageline++;
      xleft--;
      if (xleft == 0) {
	xleft = gfi->width;
	++ypos;
        imageline = gif_imageline(gfi, ypos);
      }

      if (!next_node) {
	/* Output the current code. */
	if (next_code < GIF_MAX_CODE) {
          gfc_define(gfc, work_node, suffix, next_code);
          next_code++;
	} else {
	  next_code = GIF_MAX_CODE + 1; /* to match "> CUR_BUMP_CODE" above */
	  if (end_table_count < 256)
	    ++end_table_count;
	  end_table_avg_depth +=
	    (work_depth * 128 - end_table_avg_depth) / end_table_count;
	}

	/* Output the current code. */
	output_code = work_node->code;
	work_node = &gfc->nodes[suffix];
	work_depth = 1;
	goto found_output_code;
      }

      work_node = next_node;
      ++work_depth;
    }

    /* Ran out of data if we get here. */
    output_code = (work_node ? work_node->code : EOI_CODE);
    work_node = 0;
    work_depth = 0;

   found_output_code: ;
  }

  /* Output memory buffer to stream. */
  {
    bufpos = (bufpos + 7) >> 3;
    unsigned outpos = 0;
    while (outpos < bufpos) {
      unsigned w = (bufpos - outpos > 255 ? 255 : bufpos - outpos);
      gifputbyte(w, grr);
      gifputblock(buf + outpos, w, grr);
      outpos += w;
    }
    gifputbyte(0, grr);
  }

  if (buf != stack_buffer)
    Gif_DeleteArray(buf);

  return 1;
}


static int
calculate_min_code_bits(Gif_Stream *gfs, Gif_Image *gfi,
                        const Gif_Writer *grr)
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
    if (Gif_UncompressImage(gfi))
      Gif_FullCompressImage(gfs, gfi, &grr->gcinfo);
  }

  return min_code_bits;
}


static int get_color_table_size(const Gif_Stream *gfs, Gif_Image *gfi,
				Gif_Writer *grr);

int
Gif_FullCompressImage(Gif_Stream *gfs, Gif_Image *gfi,
		      const Gif_CompressInfo *gcinfo)
{
  int ok = 0;
  uint8_t min_code_bits;
  Gif_Writer grr;
  Gif_CodeTable gfc;

  if (gfi->compressed && gfi->free_compressed) {
    (*gfi->free_compressed)((void *)gfi->compressed);
    gfi->compressed = 0;
  }

  gfc_init(&gfc);

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
  grr.errors = 0;

  if (!gfc.nodes || !gfc.links || !grr.v)
    goto done;

  min_code_bits = calculate_min_code_bits(gfs, gfi, &grr);
  ok = write_compressed_data(gfi, min_code_bits, &gfc, &grr);

  if ((grr.gcinfo.flags & (GIF_WRITE_OPTIMIZE | GIF_WRITE_EAGER_CLEAR))
      == GIF_WRITE_OPTIMIZE
      && grr.cleared && ok) {
    uint8_t *old_v = grr.v;
    uint32_t old_pos = grr.pos;
    grr.v = Gif_NewArray(uint8_t, grr.cap);
    grr.pos = 0;
    grr.gcinfo.flags = grr.gcinfo.flags | GIF_WRITE_EAGER_CLEAR;
    if (grr.v && write_compressed_data(gfi, min_code_bits, &gfc, &grr)
	&& grr.pos < old_pos) {
      Gif_DeleteArray(old_v);
      goto done;
    }
    Gif_DeleteArray(grr.v);
    grr.v = old_v;
    grr.pos = old_pos;
  }

 done:
  if (!ok) {
    Gif_DeleteArray(grr.v);
    grr.v = 0;
  }
  gfi->compressed = grr.v;
  gfi->compressed_len = grr.pos;
  gfi->free_compressed = Gif_DeleteArrayFunc;
  Gif_DeleteArray(gfc.nodes);
  Gif_DeleteArray(gfc.links);
  return grr.v != 0;
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
    gifputbyte(c->red, grr);
    gifputbyte(c->green, grr);
    gifputbyte(c->blue, grr);
  }

  /* Pad out colormap with black. */
  for (; i < totalcol; i++) {
    gifputbyte(0, grr);
    gifputbyte(0, grr);
    gifputbyte(0, grr);
  }
}


static int
write_image(Gif_Stream *gfs, Gif_Image *gfi, Gif_CodeTable *gfc,
            Gif_Writer *grr)
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
    write_compressed_data(gfi, min_code_bits, gfc, grr);

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
    int len = gfex->application ? strlen(gfex->application) : 0;
    if (len) {
      gifputbyte(len, grr);
      gifputblock((const uint8_t *)gfex->application, len, grr);
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
  Gif_CodeTable gfc;

  gfc_init(&gfc);
  if (!gfc.nodes || !gfc.links)
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
      gifputblock((const uint8_t *)"GIF89a", 6, grr);
    else
      gifputblock((const uint8_t *)"GIF87a", 6, grr);
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
  Gif_DeleteArray(gfc.nodes);
  Gif_DeleteArray(gfc.links);
  return ok;
}


int
Gif_FullWriteFile(Gif_Stream *gfs, const Gif_CompressInfo *gcinfo,
		  FILE *f)
{
  Gif_Writer grr;
  grr.f = f;
  grr.byte_putter = file_byte_putter;
  grr.block_putter = file_block_putter;
  if (gcinfo)
    grr.gcinfo = *gcinfo;
  else
    Gif_InitCompressInfo(&grr.gcinfo);
  grr.errors = 0;
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
