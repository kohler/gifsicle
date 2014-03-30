/* -*- mode: c; c-basic-offset: 2 -*- */
/* gifwrite.c - Functions to write GIFs.
   Copyright (C) 1997-2014 Eddie Kohler, ekohler@gmail.com
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
#include <assert.h>
#include <limits.h>
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
  if (fwrite(block, 1, size, grr->f) != (size_t) size)
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
memory_block_putter(const uint8_t *data, uint16_t len, Gif_Writer *grr)
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
  assert(!node || (node >= gfc->nodes && node < gfc->nodes + NODES_SIZE));
  assert(suffix < gfc->clear_code);
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

/* Used to hold accumulated error for the current candidate match */
struct rgb {int r,g,b;};

/* Difference (MSE) between given color indexes + dithering error */
static inline unsigned int color_diff(Gif_Color a, Gif_Color b, int a_transaprent, int b_transparent, struct rgb dither)
{
  /* if one is transparent and the other is not, then return maximum difference */
  /* TODO: figure out what color is in the canvas under the transparent pixel and match against that */
  if (a_transaprent != b_transparent) return 1<<25;

  /* Two transparent colors are identical */
  if (a_transaprent) return 0;

  /* squared error with or without dithering. */
  unsigned int dith = (a.gfc_red-b.gfc_red+dither.r)*(a.gfc_red-b.gfc_red+dither.r)
  + (a.gfc_green-b.gfc_green+dither.g)*(a.gfc_green-b.gfc_green+dither.g)
  + (a.gfc_blue-b.gfc_blue+dither.b)*(a.gfc_blue-b.gfc_blue+dither.b);

  unsigned int undith = (a.gfc_red-b.gfc_red+dither.r/2)*(a.gfc_red-b.gfc_red+dither.r/2)
  + (a.gfc_green-b.gfc_green+dither.g/2)*(a.gfc_green-b.gfc_green+dither.g/2)
  + (a.gfc_blue-b.gfc_blue+dither.b/2)*(a.gfc_blue-b.gfc_blue+dither.b/2);

  /* Smaller error always wins, under assumption that dithering is not required and it's only done opportunistically */
  return dith < undith ? dith : undith;
}

/* difference between expected color a+dither and color b (used to calculate dithering required) */
static inline struct rgb diffused_difference(Gif_Color a, Gif_Color b, int a_transaprent, int b_transaprent, struct rgb dither)
{
  if (a_transaprent != b_transaprent) return (struct rgb){0,0,0};

  return (struct rgb) {
    a.gfc_red - b.gfc_red + dither.r * 3/4,
    a.gfc_green - b.gfc_green + dither.g * 3/4,
    a.gfc_blue - b.gfc_blue + dither.b * 3/4,
  };
}

static inline const uint8_t gif_pixel_at_pos(Gif_Image *gfi, unsigned pos);

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
    if (work_node->type < MAX_LINKS_TYPE)
      work_node->type++;
  } else
    gfc_change_node_to_table(gfc, work_node, next_node);
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

struct selected_node {
  Gif_Node *node; /* which node has been chosen by gfc_lookup_lossy */
  unsigned long pos, /* where the node ends */
  diff; /* what is the overall quality loss for that node */
};

static inline void
gfc_lookup_lossy_try_node(Gif_CodeTable *gfc, const Gif_Colormap *gfcm, Gif_Image *gfi,
  unsigned pos, Gif_Node *node, uint8_t suffix, uint8_t next_suffix,
  struct rgb dither, unsigned long base_diff, const int max_diff, struct selected_node *best_t);

/* Recursive loop
 * Find node that is descendant of node (or start new search if work_node is null) that best matches pixels starting at pos
 * base_diff and dither are distortion from search made so far */
static struct selected_node
gfc_lookup_lossy(Gif_CodeTable *gfc, const Gif_Colormap *gfcm, Gif_Image *gfi,
  unsigned pos, Gif_Node *node, unsigned long base_diff, struct rgb dither, const int max_diff)
{
  unsigned image_endpos = gfi->width * gfi->height;

  struct selected_node t,  best_t = {node, pos+1, base_diff};
  if (pos >= image_endpos) return best_t;

  uint8_t suffix = gif_pixel_at_pos(gfi, pos);
  assert(!node || (node >= gfc->nodes && node < gfc->nodes + NODES_SIZE));
  assert(suffix < gfc->clear_code);
  if (!node) {
    /* prefix of the new node must be same as suffix of previously added node */
    return gfc_lookup_lossy(gfc, gfcm, gfi, pos+1, &gfc->nodes[suffix], base_diff, (struct rgb){0,0,0}, max_diff);
  }

  /* search all nodes that are less than max_diff different from the desired pixel */
  if (node->type == TABLE_TYPE) {
    int i;
    for(i=0; i < gfc->clear_code; i++) {
      if (!node->child.m[i]) continue;
      gfc_lookup_lossy_try_node(gfc, gfcm, gfi, pos, node->child.m[i], suffix, i, dither, base_diff, max_diff, &best_t);
    }
  }
  else {
    for (node = node->child.s; node; node = node->sibling) {
      gfc_lookup_lossy_try_node(gfc, gfcm, gfi, pos, node, suffix, node->suffix, dither, base_diff, max_diff, &best_t);
    }
  }

  return best_t;
}

/**
 * Replaces best_t with a new node if it's better
 *
 * @param node        Current node to search
 * @param suffix      Previous pixel
 * @param next_suffix Next pixel to evaluate (must correspond to the node given)
 * @param dither      Desired dithering
 * @param base_diff   Difference accumulated in the search so far
 * @param max_diff    Maximum allowed pixel difference
 * @param best_t      Current best candidate (input/output argument)
 */
static inline void
gfc_lookup_lossy_try_node(Gif_CodeTable *gfc, const Gif_Colormap *gfcm, Gif_Image *gfi,
  unsigned pos, Gif_Node *node, uint8_t suffix, uint8_t next_suffix,
  struct rgb dither, unsigned long base_diff, const int max_diff, struct selected_node *best_t)
{
  unsigned int diff = suffix == next_suffix ? 0 : color_diff(gfcm->col[suffix], gfcm->col[next_suffix], suffix == gfi->transparent, next_suffix == gfi->transparent, dither);
  if (diff <= max_diff) {
    struct rgb new_dither = diffused_difference(gfcm->col[suffix], gfcm->col[next_suffix], suffix == gfi->transparent, next_suffix == gfi->transparent, dither);
    /* if the candidate pixel is good enough, check all possible continuations of that dictionary string */
    struct selected_node t = gfc_lookup_lossy(gfc, gfcm, gfi, pos+1, node, base_diff + diff, new_dither, max_diff);

    /* search is biased towards finding longest candidate that is below treshold rather than a match with minimum average error */
    if (t.pos > best_t->pos || (t.pos == best_t->pos && t.diff < best_t->diff)) {
      *best_t = t;
    }
  }
}

static inline const uint8_t
gif_pixel_at_pos(Gif_Image *gfi, unsigned pos)
{
  unsigned y = pos / gfi->width, x = pos - y * gfi->width;
  if (!gfi->interlace)
    return gfi->img[y][x];
  else
    return gfi->img[Gif_InterlaceLine(y, gfi->height)][x];
}

static int
write_compressed_data(Gif_Stream *gfs, Gif_Image *gfi,
		      int min_code_bits, Gif_CodeTable *gfc, Gif_Writer *grr)
{
  uint8_t stack_buffer[232];
  uint8_t *buf = stack_buffer;
  unsigned bufpos = 0;
  unsigned bufcap = sizeof(stack_buffer) * 8;

  unsigned pos;
  unsigned clear_bufpos, clear_pos;
  unsigned line_endpos;
  unsigned image_endpos;
  const uint8_t *imageline;

  Gif_Node *work_node;
  unsigned run;
#define RUN_EWMA_SHIFT 4
#define RUN_EWMA_SCALE 19
#define RUN_INV_THRESH ((unsigned) (1 << RUN_EWMA_SCALE) / 3000)
  unsigned run_ewma;
  Gif_Node *next_node;
  Gif_Code next_code = 0;
  Gif_Code output_code;
#define CUR_BUMP_CODE (1 << cur_code_bits)
  uint8_t suffix;

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
  run = 0;
  run_ewma = 1 << RUN_EWMA_SCALE;
  output_code = CLEAR_CODE;
  /* Because output_code is clear_code, we'll initialize next_code, et al.
     below. */

  Gif_Colormap *gfcm;

  pos = clear_pos = clear_bufpos = 0;
  if (grr->gcinfo.loss) {
    image_endpos = gfi->height * gfi->width;
    gfcm = (gfi->local ? gfi->local : gfs->global);
  } else {
    line_endpos = gfi->width;
    imageline = gif_imageline(gfi, pos);
  }

  while (1) {

    /*****
     * Output 'output_code' to the memory buffer. */
    if (bufpos + cur_code_bits >= bufcap) {
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

    {
      unsigned startpos = bufpos;
      do {
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
      run_ewma = 1 << RUN_EWMA_SCALE;
      gfc_clear(gfc, CLEAR_CODE);
      clear_pos = clear_bufpos = 0;

      GIF_DEBUG(("clear"));

    } else if (output_code == EOI_CODE)
      break;

    else if (next_code > CUR_BUMP_CODE && cur_code_bits < GIF_MAX_CODE_BITS)
      /* bump up compression size */
      ++cur_code_bits;


    /*****
     * Find the next code to output. */
    if (grr->gcinfo.loss) {
      int lastpos = pos;
      struct selected_node t = gfc_lookup_lossy(gfc, gfcm, gfi, pos, NULL, 0, (struct rgb){0,0,0}, grr->gcinfo.loss * 10);
      pos = t.pos;
      work_node = t.node;
      run = pos - lastpos - 1;

      if (pos <= image_endpos) {
        /* Output the current code. */
        if (next_code < GIF_MAX_CODE) {
          gfc_define(gfc, work_node, gif_pixel_at_pos(gfi, pos-1), next_code);
          next_code++;
        } else
          next_code = GIF_MAX_CODE + 1; /* to match "> CUR_BUMP_CODE" above */

        /* Check whether to clear table. */
        if (next_code > 4094) {
          int do_clear = grr->gcinfo.flags & GIF_WRITE_EAGER_CLEAR;

          if (!do_clear) {
            unsigned pixels_left = image_endpos - pos;
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

          if ((do_clear || run < 7) && !clear_pos) {
            clear_pos = pos - (run + 1);
            clear_bufpos = bufpos;
          } else if (!do_clear && run > 50)
            clear_pos = clear_bufpos = 0;

          if (do_clear) {
            GIF_DEBUG(("rewind %u pixels/%d bits", pos - clear_pos, bufpos + cur_code_bits - clear_bufpos));
            output_code = CLEAR_CODE;
            pos = clear_pos;

            bufpos = clear_bufpos;
            buf[bufpos >> 3] &= (1 << (bufpos & 7)) - 1;
            grr->cleared = 1;
            continue;
          }
        }

        /* Adjust current run length average. */
        run = (run << RUN_EWMA_SCALE) + (1 << (RUN_EWMA_SHIFT - 1));
        if (run < run_ewma)
          run_ewma -= (run_ewma - run) >> RUN_EWMA_SHIFT;
        else
          run_ewma += (run - run_ewma) >> RUN_EWMA_SHIFT;

        pos--;
      }

      output_code = (work_node ? work_node->code : EOI_CODE);
    } else {
      /* If height is 0 -- no more pixels to write -- we output work_node next
         time around. */
      while (imageline) {
        suffix = *imageline;
        next_node = gfc_lookup(gfc, work_node, suffix);

        imageline++;
        pos++;
        if (pos == line_endpos) {
          imageline = gif_imageline(gfi, pos);
          line_endpos += gfi->width;
        }

        if (!next_node) {
          /* Output the current code. */
          if (next_code < GIF_MAX_CODE) {
            gfc_define(gfc, work_node, suffix, next_code);
            next_code++;
          } else
            next_code = GIF_MAX_CODE + 1; /* to match "> CUR_BUMP_CODE" above */

              /* Check whether to clear table. */
              if (next_code > 4094) {
                int do_clear = grr->gcinfo.flags & GIF_WRITE_EAGER_CLEAR;

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

                if ((do_clear || run < 7) && !clear_pos) {
                  clear_pos = pos - (run + 1);
                  clear_bufpos = bufpos;
                } else if (!do_clear && run > 50)
                  clear_pos = clear_bufpos = 0;

                if (do_clear) {
                  GIF_DEBUG(("rewind %u pixels/%d bits", pos - clear_pos, bufpos + cur_code_bits - clear_bufpos));
                  output_code = CLEAR_CODE;
                  pos = clear_pos;
                  imageline = gif_imageline(gfi, pos);
                  line_endpos = gif_line_endpos(gfi, pos);
                  bufpos = clear_bufpos;
                  buf[bufpos >> 3] &= (1 << (bufpos & 7)) - 1;
                  work_node = 0;
                  run = 0;
                  grr->cleared = 1;
                  goto found_output_code;
                }
              }

              /* Adjust current run length average. */
              run = (run << RUN_EWMA_SCALE) + (1 << (RUN_EWMA_SHIFT - 1));
              if (run < run_ewma)
                run_ewma -= (run_ewma - run) >> RUN_EWMA_SHIFT;
              else
                run_ewma += (run - run_ewma) >> RUN_EWMA_SHIFT;

          /* Output the current code. */
          output_code = work_node->code;
          work_node = &gfc->nodes[suffix];
          run = 1;
          goto found_output_code;
        }

        work_node = next_node;
        ++run;
      }

      /* Ran out of data if we get here. */
      output_code = (work_node ? work_node->code : EOI_CODE);
      work_node = 0;
      run = 0;

      found_output_code: ;
   }
  }

  /* Output memory buffer to stream. */
  {
    unsigned outpos = 0;
    bufpos = (bufpos + 7) >> 3;
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
      gfi->compressed = grr->v;
      gfi->compressed_len = grr->pos;
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
  Gif_CodeTable gfc;

  gfc_init(&gfc);

  grr.v = NULL;
  grr.pos = grr.cap = 0;
  grr.byte_putter = memory_byte_putter;
  grr.block_putter = memory_block_putter;
  if (gcinfo)
    grr.gcinfo = *gcinfo;
  else
    Gif_InitCompressInfo(&grr.gcinfo);
  grr.global_size = get_color_table_size(gfs, 0, &grr);
  grr.local_size = get_color_table_size(gfs, gfi, &grr);
  grr.errors = 0;

  if (!gfc.nodes || !gfc.links) {
    if (!(grr.gcinfo.flags & GIF_WRITE_SHRINK))
      Gif_ReleaseCompressedImage(gfi);
    goto done;
  }

  min_code_bits = calculate_min_code_bits(gfi, &grr);
  ok = write_compressed_data(gfs, gfi, min_code_bits, &gfc, &grr);
  save_compression_result(gfi, &grr, ok);

  if ((grr.gcinfo.flags & (GIF_WRITE_OPTIMIZE | GIF_WRITE_EAGER_CLEAR))
      == GIF_WRITE_OPTIMIZE
      && grr.cleared && ok) {
    grr.gcinfo.flags |= GIF_WRITE_EAGER_CLEAR | GIF_WRITE_SHRINK;
    if (write_compressed_data(gfs, gfi, min_code_bits, &gfc, &grr))
      save_compression_result(gfi, &grr, 1);
  }

 done:
  Gif_DeleteArray(grr.v);
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
    write_compressed_data(gfs, gfi, min_code_bits, gfc, grr);
    Gif_ReleaseUncompressedImage(gfi);

  } else
    write_compressed_data(gfs, gfi, min_code_bits, gfc, grr);

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
