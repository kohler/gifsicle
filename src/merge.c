/* merge.c - Functions which actually combine and manipulate GIF image data.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

/* First merging stage: Mark the used colors in all colormaps. */

void
unmark_colors(Gif_Colormap *gfcm)
{
  int i;
  if (gfcm)
    for (i = 0; i < gfcm->ncol; i++)
      gfcm->col[i].haspixel = 0;
}

void
unmark_colors_2(Gif_Colormap *gfcm)
{
    int i;
    for (i = 0; i < gfcm->ncol; i++) {
        gfcm->col[i].pixel = 256;
        gfcm->col[i].haspixel = 0;
    }
}


void
mark_used_colors(Gif_Stream *gfs, Gif_Image *gfi, Gt_Crop *crop,
                 int compress_immediately)
{
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    Gif_Color *col;
    int i, j, l, t, r, b, nleft, ncol, transp = gfi->transparent;

    /* There might not be a colormap. */
    if (!gfcm)
        return;
    col = gfcm->col;
    ncol = gfcm->ncol;

    /* Mark color used for transparency. */
    if (transp >= 0 && transp < ncol)
        col[transp].haspixel |= 2;

    /* Only mark colors until we've seen all of them. The left variable keeps
       track of how many are left. */
    for (i = nleft = 0; i < ncol; ++i)
        if (!(col[i].haspixel & 1) && i != transp)
            ++nleft;
    if (nleft == 0)
        return;

    if (gfi->img || Gif_UncompressImage(gfs, gfi) == 2)
        compress_immediately = 0;

    /* Loop over every pixel (until we've seen all colors) */
    if (crop) {
        Gt_Crop c;
        combine_crop(&c, crop, gfi);
        l = c.x;
        t = c.y;
        r = l + c.w;
        b = t + c.h;
    } else {
        l = t = 0;
        r = gfi->width;
        b = gfi->height;
    }

    for (j = t; j != b; ++j) {
        uint8_t *data = gfi->img[j] + l;
        for (i = l; i != r; ++i, ++data)
            if (*data < ncol && !(col[*data].haspixel & 1) && *data != transp) {
                col[*data].haspixel |= 1;
                --nleft;
                if (nleft == 0)
                    goto done;
            }
    }

  done:
    if (compress_immediately > 0)
        Gif_ReleaseUncompressedImage(gfi);
}


int
find_color_index(Gif_Color *c, int nc, Gif_Color *color)
{
  int index;
  for (index = 0; index < nc; index++)
    if (GIF_COLOREQ(&c[index], color))
      return index;
  return -1;
}


int
merge_colormap_if_possible(Gif_Colormap *dest, Gif_Colormap *src)
{
    Gif_Color *srccol;
    Gif_Color *destcol = dest->col;
    int ndestcol = dest->ncol;
    int dest_user_flags = dest->user_flags;
    int i, x;
    int trivial_map = 1;

    if (!src)
        return 1;

    srccol = src->col;
    for (i = 0; i < src->ncol; i++) {
        if (srccol[i].haspixel & 1) {
            /* Store an image color cell's mapping to the global colormap in
               its 'pixel' slot. This is useful caching: oftentimes many
               input frames will share a colormap */
            int mapto = (srccol[i].pixel < 256 ? (int)srccol[i].pixel : -1);

            if (mapto == -1)
                mapto = find_color_index(destcol, ndestcol, &srccol[i]);

            if (mapto == -1 && ndestcol < 256) {
                /* add the color */
                mapto = ndestcol;
                destcol[mapto] = srccol[i];
                ndestcol++;
            }

            if (mapto == -1)
                /* check for a pure-transparent color */
                for (x = 0; x < ndestcol; x++)
                    if (destcol[x].haspixel == 2) {
                        mapto = x;
                        destcol[mapto] = srccol[i];
                        break;
                    }

            if (mapto == -1)
                /* give up and require a local colormap */
                goto local_colormap_required;

            assert(mapto >= 0 && mapto < ndestcol);
            assert(GIF_COLOREQ(&destcol[mapto], &srccol[i]));

            srccol[i].pixel = mapto;
            destcol[mapto].haspixel = 1;
            if (mapto != i)
                trivial_map = 0;

        } else if (srccol[i].haspixel & 2) {
            /* a dedicated transparent color; if trivial_map & at end of
               colormap insert it with haspixel == 2. (strictly not
               necessary; we do it to try to keep the map trivial.) */
            if (trivial_map && i == ndestcol) {
                destcol[ndestcol] = srccol[i];
                ndestcol++;
            }
        }
    }

    /* success! save new number of colors */
    dest->ncol = ndestcol;
    dest->user_flags = dest_user_flags;
    return 1;

  /* failure: a local colormap is required */
 local_colormap_required:
    if (warn_local_colormaps == 1) {
        static int context = 0;
        if (!context) {
            warning(1, "too many colors, using local colormaps\n"
                    "  (You may want to try %<--colors 256%>.)");
            context = 1;
        } else
            warning(1, "too many colors, using local colormaps");
        warn_local_colormaps = 2;
    }

  /* 9.Dec.1998 - This must have been a longstanding bug! We MUST clear
     the cached mappings of any pixels in the source colormap we
     assigned this time through, since we are throwing those colors
     away. We assigned it this time through if the cached mapping is >=
     dest->ncol. */
  for (x = 0; x < i; x++)
    if ((srccol[x].haspixel & 1) && srccol[x].pixel >= (uint32_t)dest->ncol)
      srccol[x].pixel = 256;

  return 0;
}


void
merge_stream(Gif_Stream *dest, Gif_Stream *src, int no_comments)
{
  int i;
  assert(dest->global);

  /* unmark colors in global and local colormaps -- 12/9 */
  if (src->global)
    unmark_colors_2(src->global);
  for (i = 0; i < src->nimages; i++)
    if (src->images[i]->local)
      unmark_colors_2(src->images[i]->local);

  if (dest->loopcount < 0)
    dest->loopcount = src->loopcount;

  if (src->end_comment && !no_comments) {
    if (!dest->end_comment)
      dest->end_comment = Gif_NewComment();
    merge_comments(dest->end_comment, src->end_comment);
  }
}


void
merge_comments(Gif_Comment *destc, Gif_Comment *srcc)
{
  int i;
  for (i = 0; i < srcc->count; i++)
    Gif_AddComment(destc, srcc->str[i], srcc->len[i]);
}


static void merge_image_input_colors(uint8_t* inused, const Gif_Image* srci) {
    int i, x, y, nleft = Gif_ImageColorBound(srci);
    for (i = 0; i != 256; ++i)
        inused[i] = 0;
    for (y = 0; y != srci->height && nleft > 0; ++y) {
        const uint8_t* data = srci->img[y];
        for (x = 0; x != srci->width; ++x, ++data) {
            nleft -= 1 - inused[*data];
            inused[*data] = 1;
        }
    }
    if (srci->transparent >= 0)
        inused[srci->transparent] = 0;
}


Gif_Image *
merge_image(Gif_Stream *dest, Gif_Stream *src, Gif_Image *srci,
            Gt_Frame* srcfr, int same_compressed_ok)
{
  Gif_Colormap *imagecm;
  int imagecm_ncol;
  int i;
  Gif_Colormap *localcm = 0;
  Gif_Colormap *destcm = dest->global;

  uint8_t map[256];             /* map[input pixel value] == output pixval */
  int trivial_map;              /* does the map take input pixval --> the same
                                   pixel value for all colors in the image? */
  uint8_t inused[256];          /* inused[input pival] == 1 iff used */
  uint8_t used[256];            /* used[output pixval K] == 1 iff K was used
                                   in the image */


  Gif_Image *desti;

  /* mark colors that were actually used in this image */
  imagecm = srci->local ? srci->local : src->global;
  imagecm_ncol = imagecm ? imagecm->ncol : 0;
  merge_image_input_colors(inused, srci);
  for (i = imagecm_ncol; i != 256; ++i)
      if (inused[i]) {
          lwarning(srcfr->input_filename, "some colors undefined by colormap");
          break;
      }

  /* map[old_pixel_value] == new_pixel_value */
  for (i = 0; i < 256; i++)
      map[i] = used[i] = 0;

  /* Merge the colormap */
  if (merge_colormap_if_possible(dest->global, imagecm)) {
      /* Create 'map' and 'used' for global colormap. */
      for (i = 0; i != imagecm_ncol; ++i)
          if (inused[i])
              map[i] = imagecm->col[i].pixel;

  } else {
      /* Need a local colormap. */
      destcm = localcm = Gif_NewFullColormap(0, 256);
      for (i = 0; i != imagecm_ncol; ++i)
          if (inused[i]) {
              map[i] = localcm->ncol;
              localcm->col[localcm->ncol] = imagecm->col[i];
              ++localcm->ncol;
          }
  }

  trivial_map = 1;
  for (i = 0; i != 256; ++i)
      if (inused[i]) {
          used[map[i]] = 1;
          trivial_map = trivial_map && map[i] == i;
      }

  /* Decide on a transparent index */
  if (srci->transparent >= 0) {
    int found_transparent = -1;

    /* try to keep the map trivial -- prefer same transparent index */
    if (trivial_map && !used[srci->transparent])
      found_transparent = srci->transparent;
    else
      for (i = destcm->ncol - 1; i >= 0; i--)
        if (!used[i])
          found_transparent = i;

    /* 1.Aug.1999 - Allow for the case that the transparent index is bigger
       than the number of colors we've created thus far. */
    if (found_transparent < 0 || found_transparent >= destcm->ncol) {
      Gif_Color *c;
      found_transparent = destcm->ncol;
      /* 1.Aug.1999 - Don't update destcm->ncol -- we want the output colormap
         to be as small as possible. */
      c = &destcm->col[found_transparent];
      if (imagecm && srci->transparent < imagecm->ncol)
        *c = imagecm->col[srci->transparent];
      c->haspixel = 2;
      assert(c->haspixel == 2 && found_transparent < 256);
    }

    map[srci->transparent] = found_transparent;
    if (srci->transparent != found_transparent) trivial_map = 0;
  }

  assert(destcm->ncol <= 256);
  /* Make the new image. */
  desti = Gif_NewImage();

  desti->identifier = Gif_CopyString(srci->identifier);
  if (srci->transparent > -1)
    desti->transparent = map[srci->transparent];
  desti->delay = srci->delay;
  desti->disposal = srci->disposal;
  desti->left = srci->left;
  desti->top = srci->top;
  desti->interlace = srci->interlace;

  desti->width = srci->width;
  desti->height = srci->height;
  desti->local = localcm;

  if (trivial_map && same_compressed_ok && srci->compressed
      && !srci->compressed_errors) {
    desti->compressed_len = srci->compressed_len;
    desti->compressed = Gif_NewArray(uint8_t, srci->compressed_len);
    desti->free_compressed = Gif_Free;
    memcpy(desti->compressed, srci->compressed, srci->compressed_len);
  } else {
    int i, j;
    Gif_CreateUncompressedImage(desti, desti->interlace);

    if (trivial_map)
      for (j = 0; j < desti->height; j++)
        memcpy(desti->img[j], srci->img[j], desti->width);

    else
      for (j = 0; j < desti->height; j++) {
        uint8_t *srcdata = srci->img[j];
        uint8_t *destdata = desti->img[j];
        for (i = 0; i < desti->width; i++, srcdata++, destdata++)
          *destdata = map[*srcdata];
      }
  }

  /* comments and extensions */
  if (srci->comment) {
    desti->comment = Gif_NewComment();
    merge_comments(desti->comment, srci->comment);
  }
  if (srci->extension_list && !srcfr->no_extensions) {
      Gif_Extension* gfex;
      for (gfex = srci->extension_list; gfex; gfex = gfex->next)
          if (gfex->kind != 255 || !srcfr->no_app_extensions)
              Gif_AddExtension(dest, desti, Gif_CopyExtension(gfex));
  }
  while (srcfr->extensions) {
      Gif_Extension* next = srcfr->extensions->next;
      Gif_AddExtension(dest, desti, srcfr->extensions);
      srcfr->extensions = next;
  }

  Gif_AddImage(dest, desti);
  return desti;
}
