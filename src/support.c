/* support.c - Support functions for gifsicle.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>

const char *program_name = "gifsicle";
static int verbose_pos = 0;
int error_count = 0;
int no_warnings = 0;


static void
verror(const char* landmark, int need_file,
       int seriousness, const char *fmt, va_list val)
{
    char pbuf[256], buf[BUFSIZ], xbuf[BUFSIZ];
    const char* xfmt;
    int n, i, p;
    size_t xi;

    if (!fmt || !*fmt)
        return;

    if (!landmark && need_file && active_output_data.active_output_name
        && mode != BLANK_MODE && mode != MERGING && nested_mode != MERGING)
        landmark = active_output_data.active_output_name;
    else if (!landmark)
        landmark = "";

    if (seriousness > 2)
        xfmt = "%s:%s%s fatal error: ";
    else if (seriousness == 1)
        xfmt = "%s:%s%s warning: ";
    else
        xfmt = "%s:%s%s ";
    snprintf(pbuf, sizeof(pbuf), xfmt, program_name, landmark, *landmark ? ":" : "");
    p = strlen(pbuf);

    Clp_vsnprintf(clp, buf, sizeof(buf), fmt, val);
    n = strlen(buf);
    if ((size_t) n + 1 < sizeof(buf) && (n == 0 || buf[n - 1] != '\n')) {
        buf[n++] = '\n';
        buf[n] = 0;
    }

    xi = 0;
    for (i = 0; i != n; ) {
        /* char* pos = (char*) memchr(&buf[i], '\n', n - i);
           int l = (pos ? &pos[1] - &buf[i] : n - i); */
        int l = n - i;
        int xd = snprintf(&xbuf[xi], sizeof(xbuf) - xi, "%.*s%.*s",
                          (int) p, pbuf, (int) l, &buf[i]);
        i += l;
        xi = (xi + xd > sizeof(xbuf) ? sizeof(xbuf) : xi + xd);
    }

    if (seriousness == 1 && no_warnings)
        return;
    else if (seriousness > 1)
        error_count++;

    verbose_endline();
    fwrite(xbuf, 1, xi, stderr);
}

void fatal_error(const char* format, ...) {
    va_list val;
    va_start(val, format);
    verror((const char*) 0, 0, 3, format, val);
    va_end(val);
    exit(EXIT_USER_ERR);
}

void lerror(const char* landmark, const char* format, ...) {
    va_list val;
    va_start(val, format);
    verror(landmark, 2, 2, format, val);
    va_end(val);
}

void error(int need_file, const char* format, ...) {
    va_list val;
    va_start(val, format);
    verror((const char*) 0, need_file, 2, format, val);
    va_end(val);
}

void lwarning(const char* landmark, const char* format, ...) {
    va_list val;
    va_start(val, format);
    verror(landmark, 2, 1, format, val);
    va_end(val);
}

void warning(int need_file, const char* format, ...) {
    va_list val;
    va_start(val, format);
    verror((const char*) 0, need_file, 1, format, val);
    va_end(val);
}

void
clp_error_handler(Clp_Parser *clp, const char *message)
{
    (void) clp;
    verbose_endline();
    fputs(message, stderr);
}


void
short_usage(void)
{
  fprintf(stderr, "Usage: %s [OPTION | FILE | FRAME]...\n\
Try '%s --help' for more information.\n",
          program_name, program_name);
}


void
usage(void)
{
  printf("\
'Gifsicle' manipulates GIF images. Its most common uses include combining\n\
single images into animations, adding transparency, optimizing animations for\n\
space, and printing information about GIFs.\n\
\n\
Usage: %s [OPTION | FILE | FRAME]...\n\n", program_name);
  printf("\
Mode options: at most one, before any filenames.\n\
  -m, --merge                   Merge mode: combine inputs, write stdout.\n\
  -b, --batch                   Batch mode: modify inputs, write back to\n\
                                same filenames.\n\
  -e, --explode                 Explode mode: write N files for each input,\n\
                                one per frame, to 'input.frame-number'.\n\
  -E, --explode-by-name         Explode mode, but write 'input.name'.\n\n");
  printf("\
General options: Also --no-OPTION for info and verbose.\n\
  -I, --info                    Print info about input GIFs. Two -I's means\n\
                                normal output is not suppressed.\n\
      --color-info, --cinfo     --info plus colormap details.\n\
      --extension-info, --xinfo --info plus extension details.\n\
      --size-info, --sinfo      --info plus compression information.\n\
  -V, --verbose                 Prints progress information.\n");
  printf("\
  -h, --help                    Print this message and exit.\n\
      --version                 Print version number and exit.\n\
  -o, --output FILE             Write output to FILE.\n\
  -w, --no-warnings             Don't report warnings.\n\
      --no-ignore-errors        Quit on very erroneous input GIFs.\n\
      --conserve-memory         Conserve memory at the expense of speed.\n\
      --multifile               Support concatenated GIF files.\n\
\n");
  printf("\
Frame selections:               #num, #num1-num2, #num1-, #name\n\
\n\
Frame change options:\n\
  --delete FRAMES               Delete FRAMES from input.\n\
  --insert-before FRAME GIFS    Insert GIFS before FRAMES in input.\n\
  --append GIFS                 Append GIFS to input.\n\
  --replace FRAMES GIFS         Replace FRAMES with GIFS in input.\n\
  --done                        Done with frame changes.\n\n");
  printf("\
Image options: Also --no-OPTION and --same-OPTION.\n\
  -B, --background COL          Make COL the background color.\n\
      --crop X,Y+WxH, --crop X,Y-X2,Y2\n\
                                Crop the image.\n\
      --crop-transparency       Crop transparent borders off the image.\n\
      --flip-horizontal, --flip-vertical\n\
                                Flip the image.\n");
  printf("\
  -i, --interlace               Turn on interlacing.\n\
  -S, --logical-screen WxH      Set logical screen to WxH.\n\
  -p, --position X,Y            Set frame position to (X,Y).\n\
      --rotate-90, --rotate-180, --rotate-270, --no-rotate\n\
                                Rotate the image.\n\
  -t, --transparent COL         Make COL transparent.\n\n");
  printf("\
Extension options:\n\
      --app-extension N D       Add an app extension named N with data D.\n\
  -c, --comment TEXT            Add a comment before the next frame.\n\
      --extension N D           Add an extension number N with data D.\n\
  -n, --name TEXT               Set next frame's name.\n\
      --no-comments, --no-names, --no-extensions\n\
                                Remove comments (names, extensions) from input.\n");
  printf("\
Animation options: Also --no-OPTION and --same-OPTION.\n\
  -d, --delay TIME              Set frame delay to TIME (in 1/100sec).\n\
  -D, --disposal METHOD         Set frame disposal to METHOD.\n\
  -l, --loopcount[=N]           Set loop extension to N (default forever).\n\
  -O, --optimize[=LEVEL]        Optimize output GIFs.\n\
  -U, --unoptimize              Unoptimize input GIFs.\n");
#if ENABLE_THREADS
  printf("\
  -j, --threads[=THREADS]       Use multiple threads to improve speed.\n");
#endif
  printf("\n\
Whole-GIF options: Also --no-OPTION.\n\
      --careful                 Write larger GIFs that avoid bugs in other\n\
                                programs.\n\
      --change-color COL1 COL2  Change COL1 to COL2 throughout.\n\
  -k, --colors N                Reduce the number of colors to N.\n\
      --color-method METHOD     Set method for choosing reduced colors.\n\
  -f, --dither                  Dither image after changing colormap.\n");
#if HAVE_POW
  printf("\
      --gamma G                 Set gamma for color reduction [2.2].\n");
#endif
  printf("\
      --lossy[=LOSSINESS]       Alter image colors to shrink output file size\n\
                                at the cost of artifacts and noise.\n\
      --resize WxH              Resize the output GIF to WxH.\n\
      --resize-width W          Resize to width W and proportional height.\n\
      --resize-height H         Resize to height H and proportional width.\n\
      --resize-fit WxH          Resize if necessary to fit within WxH.\n");
  printf("\
      --scale XFACTOR[xYFACTOR] Scale the output GIF by XFACTORxYFACTOR.\n\
      --resize-method METHOD    Set resizing method.\n\
      --resize-colors N         Resize can add new colors up to N.\n\
      --transform-colormap CMD  Transform each output colormap by shell CMD.\n\
      --use-colormap CMAP       Set output GIF's colormap to CMAP, which can\n\
                                be 'web', 'gray', 'bw', or a GIF file.\n\n");
  printf("\
Report bugs to <ekohler@gmail.com>.\n\
Too much information? Try '%s --help | more'.\n", program_name);
#ifdef GIF_UNGIF
  printf("\
This version of Gifsicle writes uncompressed GIFs, which can be far larger\n\
than compressed GIFs. See http://www.lcdf.org/gifsicle for more information.\n");
#endif
}


void
verbose_open(char open, const char *name)
{
  int l = strlen(name);
  if (verbose_pos && verbose_pos + 3 + l > 79) {
    fputc('\n', stderr);
    verbose_pos = 0;
  }
  if (verbose_pos) {
    fputc(' ', stderr);
    verbose_pos++;
  }
  fputc(open, stderr);
  fputs(name, stderr);
  verbose_pos += 1 + l;
}


void
verbose_close(char close)
{
  fputc(close, stderr);
  verbose_pos++;
}


void
verbose_endline(void)
{
  if (verbose_pos) {
    fputc('\n', stderr);
    fflush(stderr);
    verbose_pos = 0;
  }
}


/*****
 * Info functions
 **/

const char* debug_color_str(const Gif_Color* gfc) {
    static int whichbuf = 0;
    static char buf[4][8];
    whichbuf = (whichbuf + 1) % 4;
    sprintf(buf[whichbuf], "#%02X%02X%02X",
            gfc->gfc_red, gfc->gfc_green, gfc->gfc_blue);
    return buf[whichbuf];
}

static void
safe_puts(const char *s, uint32_t len, FILE *f)
{
  const char *last_safe = s;
  for (; len > 0; len--, s++)
    if (*s < ' ' || *s >= 0x7F || *s == '\\') {
      if (last_safe != s) {
        size_t n = s - last_safe;
        if (fwrite(last_safe, 1, n, f) != n)
          return;
      }
      last_safe = s + 1;
      switch (*s) {
       case '\a': fputs("\\a", f); break;
       case '\b': fputs("\\b", f); break;
       case '\f': fputs("\\f", f); break;
       case '\n': fputs("\\n", f); break;
       case '\r': fputs("\\r", f); break;
       case '\t': fputs("\\t", f); break;
       case '\v': fputs("\\v", f); break;
       case '\\': fputs("\\\\", f); break;
       case 0:    if (len > 1) fputs("\\000", f); break;
       default:   fprintf(f, "\\%03o", (unsigned char) *s); break;
      }
    }
  if (last_safe != s) {
    size_t n = s - last_safe;
    if (fwrite(last_safe, 1, n, f) != n)
      return;
  }
}


static void
comment_info(FILE *where, Gif_Comment *gfcom, const char *prefix)
{
  int i;
  for (i = 0; i < gfcom->count; i++) {
    fputs(prefix, where);
    safe_puts(gfcom->str[i], gfcom->len[i], where);
    fputc('\n', where);
  }
}


#define COLORMAP_COLS   4

static void
colormap_info(FILE *where, Gif_Colormap *gfcm, const char *prefix)
{
  int i, j;
  int nrows = ((gfcm->ncol - 1) / COLORMAP_COLS) + 1;

  for (j = 0; j < nrows; j++) {
    int which = j;
    fputs(prefix, where);
    for (i = 0; i < COLORMAP_COLS && which < gfcm->ncol; i++, which += nrows) {
      if (i) fputs("    ", where);
      fprintf(where, " %3d: #%02X%02X%02X", which, gfcm->col[which].gfc_red,
              gfcm->col[which].gfc_green, gfcm->col[which].gfc_blue);
    }
    fputc('\n', where);
  }
}


static void
extension_info(FILE *where, Gif_Stream *gfs, Gif_Extension *gfex,
               int count, int image_position)
{
  uint8_t *data = gfex->data;
  uint32_t pos = 0;
  uint32_t len = gfex->length;

  fprintf(where, "  extension %d: ", count);
  if (gfex->kind == 255) {
    fprintf(where, "app '");
    safe_puts(gfex->appname, gfex->applength, where);
    fprintf(where, "'");
  } else {
    if (gfex->kind >= 32 && gfex->kind < 127)
      fprintf(where, "'%c' (0x%02X)", gfex->kind, gfex->kind);
    else
      fprintf(where, "0x%02X", gfex->kind);
  }
  if (image_position >= gfs->nimages)
    fprintf(where, " at end");
  else
    fprintf(where, " before #%d", image_position);
  if (gfex->packetized)
    fprintf(where, " packetized");
  fprintf(where, "\n");

  /* Now, hexl the data. */
  while (len > 0) {
    uint32_t row = 16;
    uint32_t i;
    if (row > len) row = len;
    fprintf(where, "    %08x: ", pos);

    for (i = 0; i < row; i += 2) {
      if (i + 1 >= row)
        fprintf(where, "%02x   ", data[i]);
      else
        fprintf(where, "%02x%02x ", data[i], data[i+1]);
    }
    for (; i < 16; i += 2)
      fputs("     ", where);

    putc(' ', where);
    for (i = 0; i < row; i++, data++)
      putc((*data >= ' ' && *data < 127 ? *data : '.'), where);
    putc('\n', where);

    pos += row;
    len -= row;
  }
}


void
stream_info(FILE *where, Gif_Stream *gfs, const char *filename, int flags)
{
  Gif_Extension *gfex;
  int n, i;

  if (!gfs)
    return;

  verbose_endline();
  fprintf(where, "* %s %d image%s\n", (filename ? filename : "<stdin>"),
          gfs->nimages, gfs->nimages == 1 ? "" : "s");
  fprintf(where, "  logical screen %dx%d\n",
          gfs->screen_width, gfs->screen_height);

  if (gfs->global) {
    fprintf(where, "  global color table [%d]\n", gfs->global->ncol);
    if (flags & INFO_COLORMAPS)
      colormap_info(where, gfs->global, "  |");
    fprintf(where, "  background %d\n", gfs->background);
  }

  if (gfs->end_comment)
    comment_info(where, gfs->end_comment, "  end comment ");

  if (gfs->loopcount == 0)
    fprintf(where, "  loop forever\n");
  else if (gfs->loopcount > 0)
    fprintf(where, "  loop count %u\n", (unsigned)gfs->loopcount);

  n = 0;
  for (i = 0; i < gfs->nimages; ++i)
      for (gfex = gfs->images[i]->extension_list; gfex; gfex = gfex->next, ++n)
          if (flags & INFO_EXTENSIONS)
              extension_info(where, gfs, gfex, n, i);
  for (gfex = gfs->end_extension_list; gfex; gfex = gfex->next, ++n)
      if (flags & INFO_EXTENSIONS)
          extension_info(where, gfs, gfex, n, gfs->nimages);
  if (n && !(flags & INFO_EXTENSIONS))
    fprintf(where, "  extensions %d\n", n);
}


static const char *disposal_names[] = {
  "none", "asis", "background", "previous", "4", "5", "6", "7"
};

void
image_info(FILE *where, Gif_Stream *gfs, Gif_Image *gfi, int flags)
{
  int num;
  if (!gfs || !gfi)
    return;
  num = Gif_ImageNumber(gfs, gfi);

  verbose_endline();
  fprintf(where, "  + image #%d ", num);
  if (gfi->identifier)
    fprintf(where, "#%s ", gfi->identifier);

  fprintf(where, "%dx%d", gfi->width, gfi->height);
  if (gfi->left || gfi->top)
    fprintf(where, " at %d,%d", gfi->left, gfi->top);

  if (gfi->interlace)
    fprintf(where, " interlaced");

  if (gfi->transparent >= 0)
    fprintf(where, " transparent %d", gfi->transparent);

  fprintf(where, "\n");

  if ((flags & INFO_SIZES) && gfi->compressed)
    fprintf(where, "    compressed size %u\n", gfi->compressed_len);

  if (gfi->comment)
    comment_info(where, gfi->comment, "    comment ");

  if (gfi->local) {
    fprintf(where, "    local color table [%d]\n", gfi->local->ncol);
    if (flags & INFO_COLORMAPS)
      colormap_info(where, gfi->local, "    |");
  }

  if (gfi->disposal || gfi->delay) {
    fprintf(where, "   ");
    if (gfi->disposal)
      fprintf(where, " disposal %s", disposal_names[gfi->disposal]);
    if (gfi->delay)
      fprintf(where, " delay %d.%02ds",
              gfi->delay / 100, gfi->delay % 100);
    fprintf(where, "\n");
  }
}


char *
explode_filename(const char *filename, int number, const char *name, int max_nimages)
{
  static char *s;
  int l = strlen(filename);
  l += name ? strlen(name) : 10;

  Gif_Delete(s);
  s = Gif_NewArray(char, l + 3);
  if (name)
    sprintf(s, "%s.%s", filename, name);
  else if (max_nimages <= 1000)
    sprintf(s, "%s.%03d", filename, number);
  else {
    int digits;
    unsigned j;
    unsigned max = (max_nimages < 0 ? 0 : max_nimages);
    for (digits = 4, j = 10000; max > j; digits++)
      j *= 10;
    sprintf(s, "%s.%0*d", filename, digits, number);
  }

  return s;
}


/*****
 * parsing functions
 **/

int frame_spec_1;
int frame_spec_2;
char *frame_spec_name;
int dimensions_x;
int dimensions_y;
int position_x;
int position_y;
Gif_Color parsed_color;
Gif_Color parsed_color2;
double parsed_scale_factor_x;
double parsed_scale_factor_y;

int
parse_frame_spec(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  char *c;
  (void)thunk;

  frame_spec_1 = 0;
  frame_spec_2 = -1;
  frame_spec_name = 0;

  if (!input && !input_name)
    input_stream(0);
  if (!input)
    return 0;

  if (arg[0] != '#') {
    if (complain)
      return Clp_OptionError(clp, "frame specifications must start with #");
    else
      return 0;
  }
  arg++;
  c = (char *)arg;

  /* Get a number range (#x, #x-y, or #x-). First, read x. */
  if (isdigit(c[0]))
    frame_spec_1 = frame_spec_2 = strtol(c, &c, 10);
  else if (c[0] == '-' && isdigit(c[1]))
    frame_spec_1 = frame_spec_2 = Gif_ImageCount(input) + strtol(c, &c, 10);

  /* Then, if the next character is a dash, read y. Be careful to prevent
     #- from being interpreted as a frame range. */
  if (c[0] == '-' && (frame_spec_2 >= 0 || c[1] != 0)) {
    c++;
    if (isdigit(c[0]))
      frame_spec_2 = strtol(c, &c, 10);
    else if (c[0] == '-' && isdigit(c[1]))
      frame_spec_2 = Gif_ImageCount(input) + strtol(c, &c, 10);
    else
      frame_spec_2 = Gif_ImageCount(input) - 1;
  }

  /* It really was a number range (and not a frame name)
     only if c is now at the end of the argument. */
  if (c[0] != 0) {
    Gif_Image *gfi = Gif_GetNamedImage(input, arg);
    if (gfi) {
      frame_spec_name = (char *)arg;
      frame_spec_1 = frame_spec_2 = Gif_ImageNumber(input, gfi);
      return 1;
    } else if (complain < 0)    /* -1 is special value meaning 'don't complain
                                   about frame NAMES, but do complain about
                                   frame numbers.' */
      return -97;               /* Return -97 on bad frame name. */
    else if (complain)
      return Clp_OptionError(clp, "no frame named %<#%s%>", arg);
    else
      return 0;

  } else {
    if (frame_spec_1 >= 0 && frame_spec_1 < Gif_ImageCount(input)
        && frame_spec_2 >= 0 && frame_spec_2 < Gif_ImageCount(input))
      return 1;
    else if (!complain)
      return 0;
    else
      return Clp_OptionError(clp, "frame %<#%s%> out of range, image has %d frames", arg, Gif_ImageCount(input));
  }
}

int
parse_dimensions(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  char *val;
  (void)thunk;

  if (*arg == '_' && arg[1] == 'x') {
    dimensions_x = 0;
    val = (char *)(arg + 1);
  } else
    dimensions_x = strtol(arg, &val, 10);
  if (*val == 'x') {
    if (val[1] == '_' && val[2] == 0) {
      dimensions_y = 0;
      val = val + 2;
    } else
      dimensions_y = strtol(val + 1, &val, 10);
    if (*val == 0)
      return 1;
  }

  if (complain)
    return Clp_OptionError(clp, "invalid dimensions %<%s%> (want WxH)", arg);
  else
    return 0;
}

int
parse_position(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  char *val;
  (void)thunk;

  position_x = strtol(arg, &val, 10);
  if (*val == ',') {
    position_y = strtol(val + 1, &val, 10);
    if (*val == 0)
      return 1;
  }

  if (complain)
    return Clp_OptionError(clp, "invalid position %<%s%> (want 'X,Y')", arg);
  else
    return 0;
}

static double strtod_fraction(const char* arg, char** endptr) {
    char* val, *val2;
    double d = strtod(arg, &val);
    if (arg != val && *val == '/') {
        double denom = strtod(val + 1, &val2);
        if (val2 != val + 1 && denom) {
            d /= denom;
            val = val2;
        }
    }
    if (endptr)
        *endptr = val;
    return d;
}

int
parse_scale_factor(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  char *val;
  (void)thunk;

  parsed_scale_factor_x = strtod_fraction(arg, &val);
  if (*val == 'x') {
    parsed_scale_factor_y = strtod_fraction(val + 1, &val);
    if (*val == 0)
      return 1;
  } else if (*val == 0) {
    parsed_scale_factor_y = parsed_scale_factor_x;
    return 1;
  }

  if (complain)
    return Clp_OptionError(clp, "invalid scale factor %<%s%> (want XxY)", arg);
  else
    return 0;
}

int
parse_rectangle(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  const char *input_arg = arg;
  char *val;
  int x = position_x = strtol(arg, &val, 10);
  (void)thunk;

  if (*val == ',') {
    int y = position_y = strtol(val + 1, &val, 10);
    if (*val == '-' && parse_position(clp, val + 1, 0, 0)) {
      if (x >= 0 && y >= 0
          && (position_x <= 0 || x < position_x)
          && (position_y <= 0 || y < position_y)) {
        /* 18.May.2008: Found it unintuitive that X,Y-0,0 acted like X,Y+-Xx-Y.
           Therefore changed it so that X,Y-0,0 acts like X,Y+0,0, and similar
           for negative dimensions.  Probably safe to change this behavior
           since only X,Y+0,0 was documented. */
        dimensions_x = (position_x <= 0 ? -position_x : position_x - x);
        dimensions_y = (position_y <= 0 ? -position_y : position_y - y);
        position_x = x;
        position_y = y;
        return 1;
      }
    } else if (*val == '+' && parse_dimensions(clp, val + 1, 0, 0))
      return 1;
  } else if (*val == 'x') {
    dimensions_x = position_x;
    dimensions_y = strtol(val + 1, &val, 10);
    if (*val == 0) {
      position_x = position_y = 0;
      return 1;
    }
  }

  if (complain)
    return Clp_OptionError(clp, "invalid rectangle %<%s%> (want X1,Y1-X2,Y2 or X1,Y1+WxH", input_arg);
  else
    return 0;
}

static int
xvalue(char c)
{
  switch (c) {
   case '0': case '1': case '2': case '3': case '4':
   case '5': case '6': case '7': case '8': case '9':
    return c - '0';
   case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
    return c - 'A' + 10;
   case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    return c - 'a' + 10;
   default:
    return -1;
  }
}

static int
parse_hex_color_channel(const char *s, int ndigits)
{
  int val1 = xvalue(s[0]);
  if (val1 < 0) return -1;
  if (ndigits == 1)
    return val1 * 16 + val1;
  else {
    int val2 = xvalue(s[1]);
    if (val2 < 0) return -1;
    return val1 * 16 + val2;
  }
}

int
parse_color(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  const char *input_arg = arg;
  char *str;
  int red, green, blue;
  (void)thunk;

  if (*arg == '#') {
    int len = strlen(++arg);
    if (len == 0 || len % 3 != 0
        || (int)strspn(arg, "0123456789ABCDEFabcdef") != len) {
      if (complain)
        Clp_OptionError(clp, "invalid color %<%s%> (want #RGB or #RRGGBB)",
                        input_arg);
      return 0;
    }

    len /= 3;
    red   = parse_hex_color_channel(&arg[ 0 * len ], len);
    green = parse_hex_color_channel(&arg[ 1 * len ], len);
    blue  = parse_hex_color_channel(&arg[ 2 * len ], len);
    goto gotrgb;

  } else if (!isdigit(*arg))
    goto error;

  red = strtol(arg, &str, 10);
  if (*str == 0) {
    if (red < 0 || red > 255)
      goto error;
    parsed_color.haspixel = 1;
    parsed_color.pixel = red;
    return 1;

  } else if (*str != ',' && *str != '/')
    goto error;

  if (*++str == 0) goto error;
  green = strtol(str, &str, 10);
  if (*str != ',' && *str != '/') goto error;

  if (*++str == 0) goto error;
  blue = strtol(str, &str, 10);
  if (*str != 0) goto error;

 gotrgb:
  if (red < 0 || green < 0 || blue < 0
      || red > 255 || green > 255 || blue > 255)
    goto error;
  parsed_color.gfc_red = red;
  parsed_color.gfc_green = green;
  parsed_color.gfc_blue = blue;
  parsed_color.haspixel = 0;
  return 1;

 error:
  if (complain)
    return Clp_OptionError(clp, "invalid color %<%s%>", input_arg);
  else
    return 0;
}

int
parse_two_colors(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  Gif_Color old_color;
  if (parse_color(clp, arg, complain, thunk) <= 0)
    return 0;
  old_color = parsed_color;

  arg = Clp_Shift(clp, 0);
  if (!arg && complain)
    return Clp_OptionError(clp, "%<%O%> takes two color arguments");
  else if (!arg)
    return 0;

  if (parse_color(clp, arg, complain, thunk) <= 0)
    return 0;

  parsed_color2 = parsed_color;
  parsed_color = old_color;
  return 1;
}


/*****
 * reading a file as a colormap
 **/

static Gif_Colormap *
read_text_colormap(FILE *f, const char *name)
{
  char buf[BUFSIZ];
  Gif_Colormap *cm = Gif_NewFullColormap(0, 256);
  Gif_Color *col = cm->col;
  int ncol = 0;
  unsigned red, green, blue;
  float fred, fgreen, fblue;

  while (fgets(buf, BUFSIZ, f)) {

    if (sscanf(buf, "%g %g %g", &fred, &fgreen, &fblue) == 3) {
      if (fred < 0) fred = 0;
      if (fgreen < 0) fgreen = 0;
      if (fblue < 0) fblue = 0;
      red = (unsigned)(fred + .5);
      green = (unsigned)(fgreen + .5);
      blue = (unsigned)(fblue + .5);
      goto found;

    } else if (buf[0] == '#'
               && strspn(buf + 1, "0123456789abcdefABCDEF") == 3
               && (!buf[4] || isspace((unsigned char) buf[4]))) {
      sscanf(buf + 1, "%1x%1x%1x", &red, &green, &blue);
      red += red << 4;
      green += green << 4;
      blue += blue << 4;
      goto found;

    } else if (buf[0] == '#'
               && strspn(buf + 1, "0123456789abcdefABCDEF") == 6
               && (!buf[7] || isspace((unsigned char) buf[7]))) {
      sscanf(buf + 1, "%2x%2x%2x", &red, &green, &blue);
     found:
      if (red > 255) red = 255;
      if (green > 255) green = 255;
      if (blue > 255) blue = 255;
      if (ncol >= 256) {
        lerror(name, "maximum 256 colors allowed in colormap");
        break;
      } else {
        col[ncol].gfc_red = red;
        col[ncol].gfc_green = green;
        col[ncol].gfc_blue = blue;
        ncol++;
      }
    }

    /* handle too-long lines gracefully */
    if (strchr(buf, '\n') == 0) {
      int c;
      for (c = getc(f); c != '\n' && c != EOF; c = getc(f))
        ;
    }
  }

  if (ncol == 0) {
    lerror(name, "file not in colormap format");
    Gif_DeleteColormap(cm);
    return 0;
  } else {
    cm->ncol = ncol;
    return cm;
  }
}

static void
no_gifread_error(Gif_Stream* gfs, Gif_Image* gfi,
                 int is_error, const char *message) {
    (void) gfs, (void) gfi, (void) is_error, (void) message;
}

Gif_Colormap *
read_colormap_file(const char *name, FILE *f)
{
  Gif_Colormap *cm = 0;
  int c;
  int my_file = 0;

  if (name && strcmp(name, "-") == 0)
    name = 0;
  if (!f) {
    my_file = 1;
    if (!name)
      f = stdin;
    else
      f = fopen(name, "rb");
    if (!f) {
      lerror(name, "%s", name, strerror(errno));
      return 0;
    }
  }

  name = name ? name : "<stdin>";
  if (verbosing) verbose_open('<', name);

  c = getc(f);
  ungetc(c, f);
  if (c == 'G') {
    Gif_Stream *gfs = Gif_FullReadFile(f, GIF_READ_COMPRESSED, 0, no_gifread_error);
    if (!gfs)
      lerror(name, "file not in GIF format");
    else if (!gfs->global
             && (gfs->nimages == 0 || !gfs->images[0]->local))
      lerror(name, "can%,t use as palette (no global color table)");
    else {
      if (gfs->errors)
        lwarning(name, "there were errors reading this GIF");
      cm = Gif_CopyColormap(gfs->global ? gfs->global : gfs->images[0]->local);
    }

    Gif_DeleteStream(gfs);
  } else
    cm = read_text_colormap(f, name);

  if (my_file) fclose(f);
  if (verbosing) verbose_close('>');
  return cm;
}


/*****
 * Frame stuff
 **/


Gt_Frameset *
new_frameset(int initial_cap)
{
  Gt_Frameset *fs = Gif_New(Gt_Frameset);
  if (initial_cap < 0) initial_cap = 0;
  fs->cap = initial_cap;
  fs->count = 0;
  fs->f = Gif_NewArray(Gt_Frame, initial_cap);
  return fs;
}


void
clear_def_frame_once_options(void)
{
  /* Get rid of next-frame-only options.

     This causes problems with frame selection. In the command 'gifsicle
     -nblah f.gif', the name should be applied to frame 0 of f.gif. This will
     happen automatically when f.gif is read, since all of its frames will be
     added when it is input. After frame 0, the name in def_frame will be
     cleared.

     Now, 'gifsicle -nblah f.gif #1' should apply the name to frame 1 of
     f.gif. But once f.gif is input, its frames are added, and the name
     component of def_frame is cleared!! So when #1 comes around it's gone!

     We handle this in gifsicle.c using the _change fields. */

  def_frame.name = 0;
  def_frame.comment = 0;
  def_frame.extensions = 0;
}


Gt_Frame *
add_frame(Gt_Frameset *fset, Gif_Stream *gfs, Gif_Image *gfi)
{
    int number = fset->count++;
    while (number >= fset->cap) {
        fset->cap *= 2;
        Gif_ReArray(fset->f, Gt_Frame, fset->cap);
    }

  /* Mark the stream and the image both */
  gfs->refcount++;
  gfi->refcount++;
  fset->f[number] = def_frame;
  fset->f[number].stream = gfs;
  fset->f[number].image = gfi;

  clear_def_frame_once_options();

  return &fset->f[number];
}


static Gt_Frame **merger = 0;
static int nmerger = 0;
static int mergercap = 0;

static void
merger_add(Gt_Frame *fp)
{
  while (nmerger >= mergercap)
    if (mergercap) {
      mergercap *= 2;
      Gif_ReArray(merger, Gt_Frame *, mergercap);
    } else {
      mergercap = 16;
      merger = Gif_NewArray(Gt_Frame *, mergercap);
    }
  merger[ nmerger++ ] = fp;
}


static void
merger_flatten(Gt_Frameset *fset, int f1, int f2)
{
  int i;
  assert(f1 >= 0 && f2 < fset->count);
  for (i = f1; i <= f2; i++) {
    Gt_Frameset *nest = FRAME(fset, i).nest;

    if (nest && nest->count > 0) {
      if (FRAME(fset, i).use < 0 && nest->count == 1) {
        /* use < 0 means use the frame's delay, disposal and name (if not
           explicitly overridden), but not the frame itself. */
        if (FRAME(nest, 0).delay < 0)
          FRAME(nest, 0).delay = FRAME(fset, i).image->delay;
        if (FRAME(nest, 0).disposal < 0)
          FRAME(nest, 0).disposal = FRAME(fset, i).image->disposal;
        if (FRAME(nest, 0).name == 0 && FRAME(nest, 0).no_name == 0)
          FRAME(nest, 0).name =
            Gif_CopyString(FRAME(fset, i).image->identifier);
      }
      merger_flatten(nest, 0, nest->count - 1);
    }

    if (FRAME(fset, i).use > 0)
      merger_add(&FRAME(fset, i));
  }
}


static int
find_color_or_error(Gif_Color *color, Gif_Stream *gfs, Gif_Image *gfi,
                    const char *color_context)
{
  Gif_Colormap *gfcm = gfs->global;
  int index;
  if (gfi && gfi->local) gfcm = gfi->local;

  if (color->haspixel == 2) {   /* have pixel value, not color */
    if (color->pixel < (uint32_t)gfcm->ncol)
      return color->pixel;
    else {
      if (color_context)
          lwarning(gfs->landmark, "%s color out of range", color_context);
      return -1;
    }
  }

  index = Gif_FindColor(gfcm, color);
  if (index < 0 && color_context)
    lwarning(gfs->landmark, "%s color not in colormap", color_context);
  return index;
}

static void
set_background(Gif_Stream *gfs, Gt_OutputData *output_data)
{
    Gif_Color background;
    int i, j, conflict, want_transparent;
    Gif_Image *gfi;

    /* Check for user-specified background. */
    /* If they specified the number, silently cooperate. */
    if (output_data->background.haspixel == 2) {
        gfs->background = output_data->background.pixel;
        return;
    }

    /* Otherwise, if they specified a color, search for it. */
    if (output_data->background.haspixel) {
        if (gfs->images[0]->transparent >= 0) {
            static int context = 0;
            if (!context) {
                warning(1, "irrelevant background color\n"
                        "  (The background will appear transparent because"
                        "  the first image contains transparency.)");
                context = 1;
            } else
                warning(1, "irrelevant background color");
        }
        background = output_data->background;
        goto search;
    }

    /* If we get here, user doesn't care about background. */
    /* Search for required background colors. */
    conflict = want_transparent = background.haspixel = 0;
    for (i = j = 0; i < nmerger; i++) {
        if (merger[i]->total_crop) /* frame does not correspond to an image */
            continue;
        gfi = gfs->images[j];
        if (gfi->disposal == GIF_DISPOSAL_BACKGROUND
            || (j == 0 && (gfi->left != 0 || gfi->top != 0
                           || gfi->width != gfs->screen_width
                           || gfi->height != gfs->screen_height))) {
            /* transparent.haspixel set below, at merge_frame_done */
            int original_bg_transparent = (merger[i]->transparent.haspixel == 2);
            if ((original_bg_transparent && background.haspixel)
                || (!original_bg_transparent && want_transparent))
                conflict = 2;
            else if (original_bg_transparent)
                want_transparent = 1;
            else if (merger[i]->transparent.haspixel) {
                if (background.haspixel
                    && !GIF_COLOREQ(&background, &merger[i]->transparent))
                    conflict = 1;
                else {
                    background = merger[i]->transparent;
                    background.haspixel = 1;
                }
            }
        }
        j++;
    }

    /* Report conflicts. */
    if (conflict || (want_transparent && gfs->images[0]->transparent < 0)) {
        static int context = 0;
        if (!context) {
            warning(1, "input images have conflicting background colors\n"
                    "  (This means some animation frames may appear incorrect.)");
            context = 1;
        } else
            warning(1, "input images have conflicting background colors");
    }

    /* If no important background color, bag. */
    if (!background.haspixel) {
        gfs->background = 0;
        return;
    }

  search:
    i = find_color_or_error(&background, gfs, 0, "background");
    gfs->background = (i >= 0 ? i : 0);
}



static void
fix_total_crop(Gif_Stream *dest, Gif_Image *srci, int merger_index)
{
  /* Salvage any relevant information from a frame that's been completely
     cropped away. This ends up being comments and delay. */
  Gt_Frame *fr = merger[merger_index];
  Gt_Frame *next_fr = 0;
  Gif_Image *prev_image;
  assert(dest->nimages > 0);
  prev_image = dest->images[dest->nimages - 1];
  if (merger_index < nmerger - 1)
    next_fr = merger[merger_index + 1];

  /* Don't save identifiers since the frame that was to be identified, is
     gone. Save comments though. */
  if (!fr->no_comments && srci->comment && next_fr) {
    if (!next_fr->comment)
      next_fr->comment = Gif_NewComment();
    merge_comments(next_fr->comment, srci->comment);
  }
  if (fr->comment && next_fr) {
    if (!next_fr->comment)
      next_fr->comment = Gif_NewComment();
    merge_comments(next_fr->comment, fr->comment);
    Gif_DeleteComment(fr->comment);
    fr->comment = 0;
  }

  /* Save delay by adding it to the previous frame's delay. */
  if (fr->delay < 0)
    fr->delay = srci->delay;
  prev_image->delay += fr->delay;

  /* Mark this image as totally cropped. */
  fr->total_crop = 1;
}


static void
handle_screen(Gif_Stream *dest, uint16_t width, uint16_t height)
{
    /* Set the screen width & height, if the current input width and height are
       larger */
    if (dest->screen_width < width)
        dest->screen_width = width;
    if (dest->screen_height < height)
        dest->screen_height = height;
}

static void
handle_flip_and_screen(Gif_Stream* dest, Gif_Image* desti, Gt_Frame* fr)
{
    Gif_Stream* gfs = fr->stream;

    desti->left += fr->left_offset;
    desti->top += fr->top_offset;

    if (fr->flip_horizontal)
        flip_image(desti, fr, 0);
    if (fr->flip_vertical)
        flip_image(desti, fr, 1);

    if (fr->rotation == 1)
        rotate_image(desti, fr, 1);
    else if (fr->rotation == 2) {
        flip_image(desti, fr, 0);
        flip_image(desti, fr, 1);
    } else if (fr->rotation == 3)
        rotate_image(desti, fr, 3);

    desti->left -= fr->left_offset;
    desti->top -= fr->top_offset;

    /* handle screen size, which might have height & width exchanged */
    if (fr->rotation == 1 || fr->rotation == 3)
        handle_screen(dest, gfs->screen_height, gfs->screen_width);
    else
        handle_screen(dest, gfs->screen_width, gfs->screen_height);
}

static void
analyze_crop(int nmerger, Gt_Crop* crop, int compress_immediately)
{
  int i, nframes = 0;
  int l = 0x7FFFFFFF, r = 0, t = 0x7FFFFFFF, b = 0;
  Gif_Stream* cropped_gfs = 0;

  /* count frames to which this crop applies */
  for (i = 0; i < nmerger; i++)
      if (merger[i]->crop == crop) {
          cropped_gfs = merger[i]->stream;
          nframes++;
      }

  /* find border of frames */
  for (i = 0; i < nmerger; i++)
    if (merger[i]->crop == crop) {
      Gt_Frame *fr = merger[i];
      int ll, tt, rr, bb;
      if (!fr->position_is_offset) {
        ll = fr->image->left;
        tt = fr->image->top;
        rr = fr->image->left + fr->image->width;
        bb = fr->image->top + fr->image->height;
      } else {
        ll = tt = 0;
        rr = fr->stream->screen_width;
        bb = fr->stream->screen_height;
      }
      if (ll < l)
        l = ll;
      if (tt < t)
        t = tt;
      if (rr > r)
        r = rr;
      if (bb > b)
        b = bb;
    }

  if (t > b)
    /* total crop */
    l = r = t = b = 0;

  crop->x = crop->spec_x + l;
  crop->y = crop->spec_y + t;
  crop->w = crop->spec_w + (crop->spec_w <= 0 ? r - crop->x : 0);
  crop->h = crop->spec_h + (crop->spec_h <= 0 ? b - crop->y : 0);
  crop->left_offset = crop->x;
  crop->top_offset = crop->y;
  if (crop->x < 0 || crop->y < 0 || crop->w <= 0 || crop->h <= 0
      || crop->x + crop->w > r || crop->y + crop->h > b) {
      lerror(cropped_gfs ? cropped_gfs->landmark : (const char*) 0,
             "cropping dimensions don%,t fit image");
      crop->ready = 2;
  } else
      crop->ready = 1;

  /* Remove transparent edges. */
  if (crop->transparent_edges && crop->ready == 1) {
    int have_l = crop->x, have_t = crop->y,
      have_r = crop->x + crop->w, have_b = crop->y + crop->h;
    l = t = 0x7FFFFFFF, r = b = 0;

    for (i = 0;
         i < nmerger && (l > have_l || t > have_t || r < have_r || b < have_b);
         ++i)
      if (merger[i]->crop == crop) {
        Gt_Frame *fr = merger[i];
        Gif_Image *srci = fr->image;
        int ll = constrain(have_l, srci->left, have_r),
          tt = constrain(have_t, srci->top, have_b),
          rr = constrain(have_l, srci->left + srci->width, have_r),
          bb = constrain(have_t, srci->top + srci->height, have_b);

        if (srci->transparent >= 0) {
          int x, y;
          uint8_t **img;
          Gif_UncompressImage(fr->stream, srci);
          img = srci->img;

          /* Move top edge down over transparency */
          while (tt < bb && tt < t) {
            uint8_t *data = img[tt - srci->top];
            for (x = 0; x < srci->width; ++x)
              if (data[x] != srci->transparent)
                goto found_top;
            ++tt;
          }

        found_top:
          /* Move bottom edge up over transparency */
          while (bb > tt + 1 && bb > b) {
            uint8_t *data = img[bb - 1 - srci->top];
            for (x = 0; x < srci->width; ++x)
              if (data[x] != srci->transparent)
                goto found_bottom;
            --bb;
          }

        found_bottom:
          if (tt < bb) {
            /* Move left edge right over transparency */
            while (ll < rr && ll < l) {
              for (y = tt - srci->top; y < bb - srci->top; ++y)
                if (img[y][ll - srci->left] != srci->transparent)
                  goto found_left;
              ++ll;
            }

          found_left:
            /* Move right edge left over transparency */
            while (rr > ll + 1 && rr > r) {
              for (y = tt - srci->top; y < bb - srci->top; ++y)
                if (img[y][rr - 1 - srci->left] != srci->transparent)
                  goto found_right;
              --rr;
            }
          }

        found_right:
          if (compress_immediately > 0 && srci->compressed)
            Gif_ReleaseUncompressedImage(srci);
        }

        if (tt < bb) {
          if (ll < l)
            l = ll;
          if (tt < t)
            t = tt;
          if (rr > r)
            r = rr;
          if (bb > b)
            b = bb;
        }
      }

    if (t > b)
      crop->w = crop->h = 0;
    else {
      crop->x = l;
      crop->y = t;
      crop->w = r - l;
      crop->h = b - t;
    }
  }
}


static inline int
apply_frame_transparent(Gif_Image *gfi, Gt_Frame *fr)
{
    int old_transparent = gfi->transparent;
    if (fr->transparent.haspixel == 255)
        gfi->transparent = -1;
    else if (fr->transparent.haspixel) {
        gfi->transparent =
            find_color_or_error(&fr->transparent, fr->stream, gfi,
                                "transparent");
        if (gfi->transparent < 0)
            fr->transparent.haspixel = 0;
    }
    return old_transparent;
}

static void mark_used_background_color(Gt_Frame* fr) {
    Gif_Stream* gfs = fr->stream;
    Gif_Image* gfi = fr->image;
    if ((fr->transparent.haspixel != 0
         ? fr->transparent.haspixel != 255
         : gfi->transparent < 0)
        && ((fr->disposal >= 0
             ? fr->disposal
             : gfi->disposal) == GIF_DISPOSAL_BACKGROUND
            || gfi->left != 0
            || gfi->top != 0
            || gfi->width != gfs->screen_width
            || gfi->height != gfs->screen_height)
        && gfs->global && gfs->background < gfs->global->ncol)
        gfs->global->col[gfs->background].haspixel |= 1;
}

Gif_Stream *
merge_frame_interval(Gt_Frameset *fset, int f1, int f2,
                     Gt_OutputData *output_data, int compress_immediately,
                     int *huge_stream)
{
  Gif_Stream *dest = Gif_NewStream();
  Gif_Colormap *global = Gif_NewFullColormap(256, 256);
  int i, same_compressed_ok, all_same_compressed_ok;

  global->ncol = 0;
  dest->global = global;
  if (output_data->active_output_name)
      dest->landmark = output_data->active_output_name;
  /* 11/23/98 A new stream's screen size is 0x0; we'll use the max of the
     merged-together streams' screen sizes by default (in merge_stream()) */

  if (f2 < 0)
      f2 = fset->count - 1;
  nmerger = 0;
  merger_flatten(fset, f1, f2);
  if (nmerger == 0) {
    error(1, "empty output GIF not written");
    return 0;
  }

  /* decide whether stream is huge */
  {
    size_t s;
    for (i = s = 0; i < nmerger; i++)
        s += (((unsigned) merger[i]->image->width
               * (unsigned) merger[i]->image->height) / 1024) + 1;
    *huge_stream = (s > 200 * 1024); /* 200 MB */
    if (*huge_stream && !compress_immediately) {
      warning(1, "huge GIF, conserving memory (processing may take a while)");
      compress_immediately = 1;
    }
  }

  /* merge stream-specific info and clear colormaps */
  for (i = 0; i < nmerger; i++)
    merger[i]->stream->user_flags = 1;
  for (i = 0; i < nmerger; i++) {
    if (merger[i]->stream->user_flags) {
      Gif_Stream *src = merger[i]->stream;
      Gif_CalculateScreenSize(src, 0);
      /* merge_stream() unmarks the global colormap */
      merge_stream(dest, src, merger[i]->no_comments);
      src->user_flags = 0;
    }
    if (merger[i]->image->local)
      unmark_colors_2(merger[i]->image->local);
  }

  /* is it ok to save the same compressed image? This is true only if we
     will recompress later from scratch. */
  /* 13.Aug.2002 - Can't save the same compressed image if we crop, so turn
     off all_same_compressed_ok below ('analyze crops'). Reported by Tom
     Schumm <phong@phong.org>. */
  if (output_data->colormap_size > 0
      || output_data->colormap_fixed
      || (output_data->optimizing & GT_OPT_MASK)
      || output_data->scaling > 0)
    all_same_compressed_ok = 1;
  else
    all_same_compressed_ok = 0;

  /* analyze crops */
  for (i = 0; i < nmerger; i++)
    if (merger[i]->crop)
      merger[i]->crop->ready = all_same_compressed_ok = 0;
  for (i = 0; i < nmerger; i++)
    if (merger[i]->crop && !merger[i]->crop->ready)
      analyze_crop(nmerger, merger[i]->crop, compress_immediately);

  /* mark used colors */
  for (i = 0; i < nmerger; ++i) {
      int old_transp = apply_frame_transparent(merger[i]->image, merger[i]);
      mark_used_colors(merger[i]->stream, merger[i]->image, merger[i]->crop,
                       compress_immediately);
      merger[i]->image->transparent = old_transp;
      mark_used_background_color(merger[i]);
  }

  /* copy stream-wide information from output_data */
  if (output_data->loopcount > -2)
    dest->loopcount = output_data->loopcount;
  dest->screen_width = dest->screen_height = 0;

  /** ACTUALLY MERGE FRAMES INTO THE NEW STREAM **/
  for (i = 0; i < nmerger; i++) {
    Gt_Frame *fr = merger[i];
    Gif_Image *srci;
    Gif_Image *desti;
    int old_transp;

    /* Make a copy of the image and crop it if we're cropping */
    fr->left_offset = fr->top_offset = 0;
    if (fr->crop) {
      int preserve_total_crop;
      srci = Gif_CopyImage(fr->image);
      Gif_UncompressImage(fr->stream, srci);

      /* Zero-delay frames are a special case.  You might think it was OK to
         get rid of totally-cropped delay-0 frames, but many browsers treat
         zero-delay frames as 100ms.  So don't completely crop a zero-delay
         frame: leave it around.  Problem reported by Calle Kabo. */
      preserve_total_crop = (dest->nimages == 0 || fr->delay == 0
                             || (fr->delay < 0 && srci->delay == 0));

      if (!crop_image(srci, fr, preserve_total_crop)) {
        /* We cropped the image out of existence! Be careful not to make 0x0
           frames. */
        fix_total_crop(dest, srci, i);
        goto merge_frame_done;
      }
    } else {
      srci = fr->image;
      Gif_UncompressImage(fr->stream, srci);
    }

    old_transp = apply_frame_transparent(srci, fr);

    /* Is it ok to use the old image's compressed version? */
    /* 11.Feb.2003 -- It is NOT ok to use the old image's compressed version
       if you are going to flip or rotate the image. Crash reported by Dan
       Lasley <Dan_Lasley@hilton.com>. */
    same_compressed_ok = all_same_compressed_ok;
    if ((fr->interlacing >= 0 && fr->interlacing != srci->interlace)
        || fr->flip_horizontal || fr->flip_vertical || fr->rotation)
      same_compressed_ok = 0;

    desti = merge_image(dest, fr->stream, srci, fr, same_compressed_ok);

    srci->transparent = old_transp; /* restore real transparent value */

    /* Flipping and rotating, and also setting the screen size */
    if (fr->flip_horizontal || fr->flip_vertical || fr->rotation)
        handle_flip_and_screen(dest, desti, fr);
    else
        handle_screen(dest, fr->stream->screen_width, fr->stream->screen_height);

    /* Names and comments */
    if (fr->name || fr->no_name) {
      Gif_DeleteArray(desti->identifier);
      desti->identifier = Gif_CopyString(fr->name);
    }
    if (fr->no_comments && desti->comment) {
      Gif_DeleteComment(desti->comment);
      desti->comment = 0;
    }
    if (fr->comment) {
      if (!desti->comment) desti->comment = Gif_NewComment();
      merge_comments(desti->comment, fr->comment);
      /* delete the comment early to help with memory; set field to 0 so we
         don't re-free it later */
      Gif_DeleteComment(fr->comment);
      fr->comment = 0;
    }

    if (fr->interlacing >= 0)
      desti->interlace = fr->interlacing;
    if (fr->left >= 0)
      desti->left = fr->left + (fr->position_is_offset ? desti->left : 0);
    if (fr->top >= 0)
      desti->top = fr->top + (fr->position_is_offset ? desti->top : 0);

    if (fr->delay >= 0)
      desti->delay = fr->delay;
    if (fr->disposal >= 0)
      desti->disposal = fr->disposal;

    /* compress immediately if possible to save on memory */
    if (desti->img) {
      if (compress_immediately > 0) {
        Gif_FullCompressImage(dest, desti, &gif_write_info);
        Gif_ReleaseUncompressedImage(desti);
      } else if (desti->compressed)
        Gif_ReleaseCompressedImage(desti);
    } else if (compress_immediately <= 0) {
      Gif_UncompressImage(dest, desti);
      Gif_ReleaseCompressedImage(desti);
    }

   merge_frame_done:
    /* 6/17/02 store information about image's background */
    if (fr->stream->images[0]->transparent >= 0)
        fr->transparent.haspixel = 2;
    else if (fr->stream->global
             && fr->stream->background < fr->stream->global->ncol) {
        fr->transparent = fr->stream->global->col[fr->stream->background];
        fr->transparent.haspixel = 1;
    } else
        fr->transparent.haspixel = 0;

    /* Destroy the copied, cropped image if necessary */
    if (fr->crop)
      Gif_DeleteImage(srci);

    /* if we can, delete the image's data right now to save memory */
    srci = fr->image;
    assert(srci->refcount > 1);
    if (--srci->refcount == 1) {
      /* only 1 reference ==> the reference is from the input stream itself */
      Gif_ReleaseUncompressedImage(srci);
      Gif_ReleaseCompressedImage(srci);
    }
    fr->image = 0;

    /* 5/26/98 Destroy the stream now to help with memory. Assumes that
       all frames are added with add_frame() which properly increments the
       stream's refcount. Set field to 0 so we don't redelete */
    Gif_DeleteStream(fr->stream);
    fr->stream = 0;
  }
  /** END MERGE LOOP **/

  /* Cropping the whole output? Reset logical screen */
  if (merger[0]->crop && merger[0]->crop == merger[nmerger - 1]->crop) {
    /* 13.May.2008: Set the logical screen to the cropped dimensions */
    /* 18.May.2008: Unless --crop-transparency is on */
    Gt_Crop* crop = merger[0]->crop;
    if (crop->transparent_edges)
        dest->screen_width = dest->screen_height = 0;
    else if (merger[0]->rotation == 1 || merger[0]->rotation == 3) {
        dest->screen_width = (crop->h > 0 ? crop->h : 0);
        dest->screen_height = (crop->w > 0 ? crop->w : 0);
    } else {
        dest->screen_width = (crop->w > 0 ? crop->w : 0);
        dest->screen_height = (crop->h > 0 ? crop->h : 0);
    }
  }

  /* Set the logical screen from the user's preferences */
  if (output_data->screen_width >= 0)
    dest->screen_width = output_data->screen_width;
  if (output_data->screen_height >= 0)
    dest->screen_height = output_data->screen_height;
  Gif_CalculateScreenSize(dest, 0);

  /* Find the background color in the colormap, or add it if we can */
  set_background(dest, output_data);

  /* The global colormap might be empty even given images that use it, if
     those images are entirely transparent. Since absent global colormaps
     are surprising, assign a black-and-white colormap */
  if (dest->global->ncol == 0) {
      for (i = 0; i != dest->nimages; ++i)
          if (!dest->images[i]->local) {
              GIF_SETCOLOR(&dest->global->col[0], 0, 0, 0);
              GIF_SETCOLOR(&dest->global->col[1], 255, 255, 255);
              dest->global->ncol = 2;
              break;
          }
  }

  return dest;
}


void
blank_frameset(Gt_Frameset *fset, int f1, int f2, int delete_object)
{
  int i;
  if (delete_object) f1 = 0, f2 = -1;
  if (f2 < 0) f2 = fset->count - 1;
  for (i = f1; i <= f2; i++) {
    /* We may have deleted stream and image earlier to save on memory; see
       above in merge_frame_interval(); but if we didn't, do it now. */
    if (FRAME(fset, i).image && FRAME(fset, i).image->refcount > 1)
      FRAME(fset, i).image->refcount--;
    Gif_DeleteStream(FRAME(fset, i).stream);
    Gif_DeleteComment(FRAME(fset, i).comment);
    if (FRAME(fset, i).nest)
      blank_frameset(FRAME(fset, i).nest, 0, 0, 1);
  }
  if (delete_object) {
    Gif_DeleteArray(fset->f);
    Gif_Delete(fset);
  }
}


void
clear_frameset(Gt_Frameset *fset, int f1)
{
  blank_frameset(fset, f1, -1, 0);
  fset->count = f1;
}
