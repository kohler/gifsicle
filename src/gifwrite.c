/* gifwrite.c - Functions to write GIFs.
   Copyright (C) 1997-2001 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of the GIF library.

   The GIF library is free software*. With the exception of this file, the GIF
   library is distributed under the GNU General Public License, version 2 or
   later; you can copy, distribute, or alter it at will, as long as this
   notice is kept intact and this source code is made available. There is no
   warranty, express or implied. This file is distributed in the public
   domain.
   
   *The LZW compression method used by GIFs is patented. Unisys, the patent
   holder, allows the compression algorithm to be used without a license in
   software distributed at no cost to the user. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#elif !defined(__cplusplus) && !defined(inline)
/* Assume we don't have inline by default */
# define inline
#endif
#include "gif.h"
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
   fan-out -- up to `clear_code' children per Node. There are two kinds of
   Node, TABLE and LINKS. In a TABLE node, the children are stored in a table
   indexed by suffix -- thus, it's very efficient to determine a given child.
   In a LINKS node, the existent children are stored in a linked list. This is
   slightly slower to access. When a LINKS node gets more than
   `MAX_LINKS_TYPE-1' children, it is converted to a TABLE node. (This is why
   it's an adaptive tree.)
   
   Problems with this implementation: MAX_LINKS_TYPE is fixed, so GIFs with
   very small numbers of colors (2, 4, 8) won't get the speed benefits of
   TABLE nodes. */

#define TABLE_TYPE		0
#define LINKS_TYPE		1
#define MAX_LINKS_TYPE		5
typedef struct Gif_Node {
  
  Gif_Code code;
  byte type;
  byte suffix;
  struct Gif_Node *sibling;
  union {
    struct Gif_Node *s;
    struct Gif_Node **m;
  } child;
  
} Gif_Node;


typedef struct Gif_Context {
  
  Gif_Node *nodes;
  int nodes_pos;
  Gif_Node **links;
  int links_pos;
  
} Gif_Context;


typedef struct Gif_Writer {
  
  FILE *f;
  byte *v;
  u_int32_t pos;
  u_int32_t cap;
  int flags;
  int global_size;
  int local_size;
  void (*byte_putter)(byte, struct Gif_Writer *);
  void (*block_putter)(byte *, u_int16_t, struct Gif_Writer *);
  
} Gif_Writer;


#define gifputbyte(b, grr)	((*grr->byte_putter)(b, grr))
#define gifputblock(b, l, grr)	((*grr->block_putter)(b, l, grr))

static inline void
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


static void
change_node_to_table(Gif_Context *gfc, Gif_Node *work_node,
		     Gif_Node *next_node, Gif_Code clear_code)
{
  /* change links node to table node */
  Gif_Code c;
  Gif_Node **table = &gfc->links[gfc->links_pos];
  Gif_Node *n;
  gfc->links_pos += clear_code;
  
  for (c = 0; c < clear_code; c++)
    table[c] = 0;
  table[next_node->suffix] = next_node;
  for (n = work_node->child.s; n; n = n->sibling)
    table[n->suffix] = n;
  
  work_node->type = TABLE_TYPE;
  work_node->child.m = table;
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
  
  Gif_Node *work_node;
  Gif_Node *next_node;
  Gif_Code next_code;
  Gif_Code output_code;
  Gif_Code clear_code;
  Gif_Code eoi_code;
#define CUR_BUMP_CODE (1 << cur_code_bits)
  byte suffix;
  
  byte cur_code_bits;
  
  /* Here we go! */
  gifputbyte(min_code_bits, grr);
  clear_code = 1 << min_code_bits;
  eoi_code = clear_code + 1;
  
  cur_code_bits = min_code_bits + 1;
  /* next_code set by first runthrough of output clear_code */
  GIF_DEBUG(("clear(%d) eoi(%d) bits(%d)",clear_code,eoi_code,cur_code_bits));
  
  work_node = 0;
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
     * Output `output_code' to the data stream. */

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
      
      /* The first clear_code nodes are reserved for single-pixel codes */
      gfc->nodes_pos = clear_code;
      gfc->links_pos = 0;
      for (c = 0; c < clear_code; c++) {
	gfc->nodes[c].code = c;
	gfc->nodes[c].type = LINKS_TYPE;
	gfc->nodes[c].suffix = c;
	gfc->nodes[c].child.s = 0;
      }
      
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
    
    /* If height is 0 -- no more pixels to write -- we output work_node next
       time around. */
    while (height != 0) {
      suffix = *imageline;
      if (suffix >= clear_code)
	/* should not happen unless GIF_WRITE_CAREFUL_MIN_CODE_BITS */
	suffix = 0;
      if (!work_node)
	next_node = &gfc->nodes[suffix];
      else if (work_node->type == TABLE_TYPE)
	next_node = work_node->child.m[suffix];
      else
	for (next_node = work_node->child.s; next_node;
	     next_node = next_node->sibling)
	  if (next_node->suffix == suffix)
	    break;
      
      imageline++;
      xleft--;
      if (xleft == 0) {
	xleft = width;
	height--;
	img++;
	imageline = img[0];
      }
      
      if (!next_node) {
	/* We need to output the current code and add a new one to our
	   dictionary. First reserve a node for the added code. It's
	   LINKS_TYPE at first. */
	next_node = &gfc->nodes[gfc->nodes_pos];
	gfc->nodes_pos++;
	next_node->code = next_code;
	next_code++;
	next_node->type = LINKS_TYPE;
	next_node->suffix = suffix;
	next_node->child.s = 0;
	
	/* link next_node into work_node's set of children */
	if (work_node->type == TABLE_TYPE)
	  work_node->child.m[suffix] = next_node;
	else if (work_node->type < MAX_LINKS_TYPE
		 || gfc->links_pos + clear_code > LINKS_SIZE) {
	  next_node->sibling = work_node->child.s;
	  work_node->child.s = next_node;
	  work_node->type++;
	} else
	  change_node_to_table(gfc, work_node, next_node, clear_code);
	
	/* Output the current code. */
	output_code = work_node->code;
	work_node = &gfc->nodes[suffix];
	goto found_output_code;
      }
      
      work_node = next_node;
    }
    
    /* Ran out of data if we get here. */
    output_code = (work_node ? work_node->code : eoi_code);
    work_node = 0;
    
   found_output_code: ;
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
calculate_min_code_bits(Gif_Stream *gfs, Gif_Image *gfi, Gif_Writer *grr)
{
  int colors_used = -1, min_code_bits, i;

  if (grr->flags & GIF_WRITE_CAREFUL_MIN_CODE_SIZE) {
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
      byte *data = gfi->img[y];
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

  if ((grr->flags & GIF_WRITE_CAREFUL_MIN_CODE_SIZE)
      && gfi->compressed && gfi->compressed[0] != min_code_bits) {
    /* if compressed image disagrees with careful min_code_bits, recompress */
    if (Gif_UncompressImage(gfi))
      Gif_FullCompressImage(gfs, gfi, grr->flags);
  }
  
  return min_code_bits;
}

static int
write_image_data(Gif_Image *gfi, byte min_code_bits,
		 Gif_Context *gfc, Gif_Writer *grr)
{
  byte **img = gfi->img;
  u_int16_t width = gfi->width, height = gfi->height;
    
  if (gfi->interlace) {
    u_int16_t y;
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


static int get_color_table_size(Gif_Stream *, Gif_Image *, Gif_Writer *);

int
Gif_FullCompressImage(Gif_Stream *gfs, Gif_Image *gfi, int flags)
{
  int ok = 0;
  byte min_code_bits;
  Gif_Writer grr;
  Gif_Context gfc;
  
  if (gfi->compressed && gfi->free_compressed) {
    (*gfi->free_compressed)((void *)gfi->compressed);
    gfi->compressed = 0;
  }
  
  gfc.nodes = Gif_NewArray(Gif_Node, NODES_SIZE);
  gfc.links = Gif_NewArray(Gif_Node *, LINKS_SIZE);
  
  grr.v = Gif_NewArray(byte, 1024);
  grr.pos = 0;
  grr.cap = 1024;
  grr.byte_putter = memory_byte_putter;
  grr.block_putter = memory_block_putter;
  grr.flags = flags;
  grr.global_size = get_color_table_size(gfs, 0, &grr);
  grr.local_size = get_color_table_size(gfs, gfi, &grr);
  
  if (!gfc.nodes || !gfc.links || !grr.v)
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
  gfi->free_compressed = Gif_DeleteArrayFunc;
  Gif_DeleteArray(gfc.nodes);
  Gif_DeleteArray(gfc.links);
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
  if (grr->flags & GIF_WRITE_CAREFUL_MIN_CODE_SIZE) {
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
write_image(Gif_Stream *gfs, Gif_Image *gfi, Gif_Context *gfc, Gif_Writer *grr)
{
  byte min_code_bits, packed = 0;
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
     recompression, if GIF_WRITE_CAREFUL_MIN_CODE_BITS is true) */
  min_code_bits = calculate_min_code_bits(gfs, gfi, grr);
  
  /* use existing compressed data if it exists. This will tend to whip
     people's asses who uncompress an image, keep the compressed data around,
     but modify the uncompressed data anyway. That sucks. */
  if (gfi->compressed) {
    byte *compressed = gfi->compressed;
    u_int32_t compressed_len = gfi->compressed_len;
    while (compressed_len > 0) {
      u_int16_t amt = (compressed_len > 0x7000 ? 0x7000 : compressed_len);
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
  byte packed = 0x70;		/* high resolution colors */
  grr->global_size = get_color_table_size(gfs, 0, grr);

  Gif_CalculateScreenSize(gfs, 0);
  gifputunsigned(gfs->screen_width, grr);
  gifputunsigned(gfs->screen_height, grr);
  
  if (grr->global_size > 0) {
    u_int16_t size = 2;
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
  byte packed = 0;
  gifputbyte('!', grr);
  gifputbyte(0xF9, grr);
  gifputbyte(4, grr);
  if (gfi->transparent >= 0) packed |= 0x01;
  packed |= (gfi->disposal & 0x07) << 2;
  gifputbyte(packed, grr);
  gifputunsigned(gfi->delay, grr);
  gifputbyte((byte)gfi->transparent, grr);
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
  gifputblock((byte *)"!\xFF\x0BNETSCAPE2.0\x03\x01", 16, grr);
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
  
  gfc.nodes = Gif_NewArray(Gif_Node, NODES_SIZE);
  gfc.links = Gif_NewArray(Gif_Node *, LINKS_SIZE);
  if (!gfc.nodes || !gfc.links)
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
  Gif_DeleteArray(gfc.nodes);
  Gif_DeleteArray(gfc.links);
  return ok;
}


int
Gif_FullWriteFile(Gif_Stream *gfs, int flags, FILE *f)
{
  Gif_Writer grr;
  grr.f = f;
  grr.byte_putter = file_byte_putter;
  grr.block_putter = file_block_putter;
  grr.flags = flags;
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
