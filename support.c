/* support.c - Support functions for gifsicle.
   Copyright (C) 1997-9 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2 or later; you can copy, distribute, or alter it at will, as long
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


static void
verror(int seriousness, char *message, va_list val)
{
  char pattern[BUFSIZ];
  char buffer[BUFSIZ];
  verbose_endline();
  
  if (seriousness > 1)
    sprintf(pattern, "%s: fatal error: %%s\n", program_name);
  else if (seriousness == 0)
    sprintf(pattern, "%s: warning: %%s\n", program_name);
  else
    sprintf(pattern, "%s: %%s\n", program_name);
  
  if (seriousness != 0)
    error_count++;
  
  /* try and keep error messages together (no interleaving of error messages
     from two gifsicle processes in the same command line) by calling fprintf
     only once */
  if (strlen(message) + strlen(pattern) < BUFSIZ) {
    sprintf(buffer, pattern, message);
    vfprintf(stderr, buffer, val);
  } else {
    fwrite(pattern, 1, strlen(pattern) - 3, stderr);
    vfprintf(stderr, message, val);
    putc('\n', stderr);
  }
}

void
fatal_error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  verror(2, message, val);
  va_end(val);
  exit(EXIT_USER_ERR);
}

void
error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  verror(1, message, val);
  va_end(val);
}

void
warning(char *message, ...)
{
  va_list val;
  va_start(val, message);
  verror(0, message, val);
  va_end(val);
}

void
clp_error_handler(char *message)
{
  verbose_endline();
  fputs(message, stderr);
}


void
short_usage(void)
{
  fprintf(stderr, "Usage: %s [OPTION | FILE | FRAME]...\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}


void
usage(void)
{
  printf("\
`Gifsicle' manipulates GIF images. Its most common uses include combining\n\
single images into animations, adding transparency, optimizing animations for\n\
space, and printing information about GIFs.\n\
\n\
Usage: %s [OPTION | FILE | FRAME]...\n\
\n\
Mode options: at most one, before any filenames.\n\
  -m, --merge                   Merge mode: combine inputs, write stdout.\n\
  -b, --batch                   Batch mode: modify inputs, write back to\n\
                                same filenames.\n\
  -e, --explode                 Explode mode: write N files for each input,\n\
                                one per frame, to `input.frame-number'.\n\
  -E, --explode-by-name         Explode mode, but write `input.name'.\n\
\n\
General options: Also --no-OPTION for info and verbose.\n\
  -I, --info                    Print info about input GIFs. Two -I's means\n\
                                normal output is not suppressed.\n\
      --color-info, --cinfo     --info plus colormap details.\n\
      --extension-info, --xinfo --info plus extension details.\n\
  -v, --verbose                 Prints progress information.\n\
  -h, --help                    Print this message and exit.\n\
      --version                 Print version number and exit.\n\
  -o, --output FILE             Write output to FILE.\n\
\n", program_name);
  printf("\
Frame selections:               #num, #num1-num2, #num1-, #name\n\
\n\
Frame change options:\n\
  --delete FRAMES               Delete FRAMES from input.\n\
  --insert-before FRAME GIFS    Insert GIFS before FRAMES in input.\n\
  --append GIFS                 Append GIFS to input.\n\
  --replace FRAMES GIFS         Replace FRAMES with GIFS in input.\n\
  --done                        Done with frame changes.\n\
\n\
Image options: Also --no-OPTION and --same-OPTION.\n\
  -B, --background COL          Makes COL the background color.\n\
      --crop X,Y+WxH, --crop X,Y-X2,Y2\n\
                                Crops the image.\n\
      --flip-horizontal, --flip-vertical\n\
                                Flips the image.\n\
  -i, --interlace               Turns on interlacing.\n\
  -S, --logical-screen WxH      Sets logical screen to WxH.\n\
  -p, --position X,Y            Sets frame position to (X,Y).\n\
      --rotate-90, --rotate-180, --rotate-270, --no-rotate\n\
                                Rotates the image.\n\
  -t, --transparent COL         Makes COL transparent.\n\
\n");
  printf("\
Extension options: Also --no-OPTION and --same-OPTION.\n\
  -x, --app-extension N D       Adds an app extension named N with data D.\n\
  -c, --comment TEXT            Adds a comment before the next frame.\n\
      --extension N D           Adds an extension number N with data D.\n\
  -n, --name TEXT               Sets next frame's name.\n\
\n\
Animation options: Also --no-OPTION and --same-OPTION.\n\
  -d, --delay TIME              Sets frame delay to TIME (in 1/100sec).\n\
  -D, --disposal METHOD         Sets frame disposal to METHOD.\n\
  -l, --loopcount[=N]           Sets loop extension to N (default forever).\n\
  -O, -optimize[=LEV]           Optimize output GIFs.\n\
  -U, --unoptimize              Unoptimize input GIFs.\n\
\n");
  printf("\
Whole-GIF options: Also --no-OPTION.\n\
      --change-color COL1 COL2  Changes COL1 to COL2 throughout.\n\
  -k, --colors N                Reduces the number of colors to N.\n\
      --color-method METHOD     Set method for choosing reduced colors.\n\
  -f, --dither                  Dither image after changing colormap.\n\
      --resize WxH              Resizes the output GIF to WxH.\n\
      --scale XFACTOR[xYFACTOR] Scales the output GIF by XFACTORxYFACTOR.\n\
      --transform-colormap CMD  Transform each output colormap by shell CMD.\n\
      --use-colormap CMAP       Set output GIF's colormap to CMAP, which can\n\
                                be `web', `gray', `bw', or a GIF file.\n\
\n\
Report bugs to <eddietwo@lcs.mit.edu>.\n\
Too much information? Try `%s --help | more'.\n", program_name);
#ifdef GIF_UNGIF
  printf("\
This version of gifsicle writes uncompressed GIFs, which can be far larger\n\
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


static void
safe_puts(const char *s, u_int32_t len, FILE *f)
{
  const char *last_safe = s;
  for (; len > 0; len--, s++)
    if (*s < ' ' || *s >= 0x7F || *s == '\\') {
      if (last_safe != s)
	fwrite(last_safe, 1, s - last_safe, f);
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
       case 0:	  if (len > 1) fputs("\\000", f); break;
       default:	  fprintf(f, "\\%03o", *s); break;
      }
    }
  if (last_safe != s)
    fwrite(last_safe, 1, s - last_safe, f);
}


static void
comment_info(FILE *where, Gif_Comment *gfcom, char *prefix)
{
  int i;
  for (i = 0; i < gfcom->count; i++) {
    fputs(prefix, where);
    safe_puts(gfcom->str[i], gfcom->len[i], where);
    fputc('\n', where);
  }
}


#define COLORMAP_COLS	4

static void
colormap_info(FILE *where, Gif_Colormap *gfcm, char *prefix)
{
  int i, j;
  int nrows = ((gfcm->ncol - 1) / COLORMAP_COLS) + 1;
  
  for (j = 0; j < nrows; j++) {
    int which = j;
    fputs(prefix, where);
    for (i = 0; i < COLORMAP_COLS && which < gfcm->ncol; i++, which += nrows) {
      if (i) fputs("    ", where);
      fprintf(where, " %3d: #%02X%02X%02X", which, gfcm->col[which].red,
	      gfcm->col[which].green, gfcm->col[which].blue);
    }
    fputc('\n', where);
  }
}


static void
extension_info(FILE *where, Gif_Stream *gfs, Gif_Extension *gfex, int count)
{
  byte *data = gfex->data;
  u_int32_t pos = 0;
  u_int32_t len = gfex->length;
  
  fprintf(where, "  extension %d: ", count);
  if (gfex->kind == 255) {
    fprintf(where, "app `");
    safe_puts(gfex->application, strlen(gfex->application), where);
    fprintf(where, "'");
  } else {
    if (gfex->kind >= 32 && gfex->kind < 127)
      fprintf(where, "`%c' (0x%02X)", gfex->kind, gfex->kind);
    else
      fprintf(where, "0x%02X", gfex->kind);
  }
  if (gfex->position >= gfs->nimages)
    fprintf(where, " at end\n");
  else
    fprintf(where, " before #%d\n", gfex->position);
  
  /* Now, hexl the data. */
  while (len > 0) {
    u_int32_t row = 16;
    u_int32_t i;
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
stream_info(FILE *where, Gif_Stream *gfs, char *filename,
	    int colormaps, int extensions)
{
  Gif_Extension *gfex;
  int n;
  
  if (!gfs) return;
  gfs->userflags = 0; /* clear userflags to indicate stream info produced */
  
  verbose_endline();
  fprintf(where, "* %s %d image%s\n", filename, gfs->nimages,
	  gfs->nimages == 1 ? "" : "s");
  fprintf(where, "  logical screen %dx%d\n",
	  gfs->screen_width, gfs->screen_height);
  
  if (gfs->global) {
    fprintf(where, "  global color table [%d]\n", gfs->global->ncol);
    if (colormaps) colormap_info(where, gfs->global, "  |");
    fprintf(where, "  background %d\n", gfs->background);
  }
  
  if (gfs->comment)
    comment_info(where, gfs->comment, "  end comment ");
  
  if (gfs->loopcount == 0)
    fprintf(where, "  loop forever\n");
  else if (gfs->loopcount > 0)
    fprintf(where, "  loop count %u\n", (unsigned)gfs->loopcount);
  
  for (n = 0, gfex = gfs->extensions; gfex; gfex = gfex->next, n++)
    if (extensions)
      extension_info(where, gfs, gfex, n);
  if (n && !extensions)
    fprintf(where, "  extensions %d\n", n);
}


static char *disposal_names[] = {
  "none", "asis", "background", "previous", "4", "5", "6", "7"
};

void
image_info(FILE *where, Gif_Stream *gfs, Gif_Image *gfi, int colormaps)
{
  int num;
  if (!gfs || !gfi) return;
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
  
#ifdef PRINT_SIZE
  if (gfi->compressed)
    fprintf(where, " compressed size %u", gfi->compressed_len);
#endif
  
  fprintf(where, "\n");
  
  if (gfi->comment)
    comment_info(where, gfi->comment, "    comment ");
  
  if (gfi->local) {
    fprintf(where, "    local color table [%d]\n", gfi->local->ncol);
    if (colormaps) colormap_info(where, gfi->local, "    |");
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
explode_filename(char *filename, int number, char *name, int max_nimages)
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
  else if (c[0] == '-' && isdigit(c[1])) {
    frame_spec_1 = frame_spec_2 = Gif_ImageCount(input) + strtol(c, &c, 10);
    if (frame_spec_1 < 0)
      return complain ? Clp_OptionError(clp, "there are only %d frames",
					Gif_ImageCount(input)) : 0;
  }
  
  /* Then, if the next character is a dash, read y. Be careful to prevent
     #- from being interpreted as a frame range. */
  if (c[0] == '-' && (frame_spec_2 > 0 || c[1] != 0)) {
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
    } else if (complain)
      return Clp_OptionError(clp, "no frame named `#%s'", arg);
    else
      return 0;
    
  } else {
    if (frame_spec_1 >= 0 && frame_spec_1 <= frame_spec_2
	&& frame_spec_2 < Gif_ImageCount(input))
      return 1;
    else if (!complain)
      return 0;
    
    if (frame_spec_1 == frame_spec_2)
      return Clp_OptionError(clp, "no frame number #%d", frame_spec_1);
    else if (frame_spec_1 < 0)
      return Clp_OptionError(clp, "frame numbers can't be negative");
    else if (frame_spec_1 > frame_spec_2)
      return Clp_OptionError(clp, "empty frame range");
    else
      return Clp_OptionError(clp, "there are only %d frames",
			     Gif_ImageCount(input));
  }
}

int
parse_dimensions(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  char *val;
  
  dimensions_x = strtol(arg, &val, 10);
  if (*val == 'x') {
    dimensions_y = strtol(val + 1, &val, 10);
    if (*val == 0)
      return 1;
  }
  
  if (complain)
    return Clp_OptionError(clp, "invalid dimensions `%s' (want WxH)", arg);
  else
    return 0;
}

int
parse_position(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  char *val;
  
  position_x = strtol(arg, &val, 10);
  if (*val == ',') {
    position_y = strtol(val + 1, &val, 10);
    if (*val == 0)
      return 1;
  }
  
  if (complain)
    return Clp_OptionError(clp, "invalid position `%s' (want `X,Y')", arg);
  else
    return 0;
}

int
parse_scale_factor(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  char *val;
  
  parsed_scale_factor_x = strtod(arg, &val);
  if (*val == 'x') {
    parsed_scale_factor_y = strtod(val + 1, &val);
    if (*val == 0)
      return 1;
  } else if (*val == 0) {
    parsed_scale_factor_y = parsed_scale_factor_x;
    return 1;
  }
  
  if (complain)
    return Clp_OptionError(clp, "invalid scale factor `%s' (want XxY)", arg);
  else
    return 0;
}

int
parse_rectangle(Clp_Parser *clp, const char *arg, int complain, void *thunk)
{
  const char *input_arg = arg;
  char *val;
  
  int x = position_x = strtol(arg, &val, 10);
  if (*val == ',') {
    int y = position_y = strtol(val + 1, &val, 10);
    if (*val == '-' && parse_position(clp, val + 1, 0, 0)) {
      if (x >= 0 && y >= 0 && x < position_x && y < position_y) {
	dimensions_x = position_x - x;
	dimensions_y = position_y - y;
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
    return Clp_OptionError(clp, "invalid rectangle `%s' (want `X1,Y1-X2,Y2' or `X1,Y1+WxH'", input_arg);
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
  
  if (*arg == '#') {
    int len = strlen(++arg);
    if (!len || len % 3 != 0 || strspn(arg, "0123456789ABCDEFabcdef") != len) {
      if (complain)
	Clp_OptionError(clp, "invalid color `%s' (want `#RGB' or `#RRGGBB')",
			input_arg);
      return 0;
    }
    
    len /= 3;
    red	  = parse_hex_color_channel(&arg[ 0 * len ], len);
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
  parsed_color.red = red;
  parsed_color.green = green;
  parsed_color.blue = blue;
  parsed_color.haspixel = 0;
  return 1;
  
 error:
  if (complain)
    return Clp_OptionError(clp, "invalid color `%s'", input_arg);
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
    return Clp_OptionError(clp, "`%O' takes two color arguments");
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
read_text_colormap(FILE *f, char *name)
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
      
    } else if (sscanf(buf, "#%2x%2x%2x", &red, &green, &blue) == 3) {
     found:
      if (red > 255) red = 255;
      if (green > 255) green = 255;
      if (blue > 255) blue = 255;
      if (ncol >= 256) {
	error("%s: maximum 256 colors allowed in colormap", name);
	break;
      } else {
	col[ncol].red = red;
	col[ncol].green = green;
	col[ncol].blue = blue;
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
    error("`%s' doesn't seem to contain a colormap", name);
    Gif_DeleteColormap(cm);
    return 0;
  } else {
    cm->ncol = ncol;
    return cm;
  }
}

Gif_Colormap *
read_colormap_file(char *name, FILE *f)
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
      error("%s: %s", name, strerror(errno));
      return 0;
    }
  }
  
  name = name ? name : "<stdin>";
  if (verbosing) verbose_open('<', name);
  
  c = getc(f);
  ungetc(c, f);
  if (c == 'G') {
    Gif_Stream *gfs = Gif_ReadFile(f);
    if (!gfs)
      error("`%s' doesn't seem to contain a GIF", name);
    else if (!gfs->global)
      error("can't use `%s' as a palette (no global color table)", name);
    else {
      if (gfs->errors)
	warning("there were errors reading `%s'", name);
      cm = Gif_CopyColormap(gfs->global);
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
     
     This causes problems with frame selection. In the command `gifsicle
     -nblah f.gif', the name should be applied to frame 0 of f.gif. This will
     happen automatically when f.gif is read, since all of its frames will be
     added when it is input. After frame 0, the name in def_frame will be
     cleared.
     
     Now, `gifsicle -nblah f.gif #1' should apply the name to frame 1 of
     f.gif. But once f.gif is input, its frames are added, and the name
     component of def_frame is cleared!! So when #1 comes around it's gone!
     
     We handle this in gifsicle.c using the _change fields. */
  
  def_frame.name = 0;
  def_frame.comment = 0;
  def_frame.extensions = 0;
}


Gt_Frame *
add_frame(Gt_Frameset *fset, int number, Gif_Stream *gfs, Gif_Image *gfi)
{
  if (number < 0) {
    while (fset->count >= fset->cap) {
      fset->cap *= 2;
      Gif_ReArray(fset->f, Gt_Frame, fset->cap);
    }
    number = fset->count++;
  } else {
    assert(number < fset->count);
    blank_frameset(fset, number, number, 0);
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


static Gif_Extension *
copy_extension(Gif_Extension *src)
{
  Gif_Extension *dest = Gif_NewExtension(src->kind, src->application);
  if (!dest) return 0;
  dest->data = Gif_NewArray(byte, src->length);
  dest->length = src->length;
  dest->free_data = Gif_DeleteArrayFunc;
  if (!dest->data) {
    Gif_DeleteExtension(dest);
    return 0;
  }
  memcpy(dest->data, src->data, src->length);
  return dest;
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
	/* use < 0 means use the frame's delay and name (if not explicitly
	   overridden), but not the frame itself. */
	if (FRAME(nest, 0).delay < 0)
	  FRAME(nest, 0).delay = FRAME(fset, i).image->delay;
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


static void
find_background(Gif_Colormap *dest_global, Gif_Color *background)
{
  int i;
  /* This code is SUCH a PAIN in the PATOOTIE!! */
  
  /* 0. report warnings if the user wants an impossible background setting */
  if (background->haspixel) {
    int first_img_transp = 0;
    if (merger[0]->transparent.haspixel < 255)
      first_img_transp = (merger[0]->image->transparent >= 0
			  || merger[0]->transparent.haspixel);
    if (first_img_transp) {
      static int context = 0;
      warning("irrelevant background color");
      if (!context) {
	warning("(The background will appear transparent because");
	warning("the first image contains transparency.)");
	context = 1;
      }
    }
  }
  
  /* 1. user set the background to a color index
     -> find the associated color */
  if (background->haspixel == 2) {
    Gif_Stream *gfs = merger[0]->stream;
    if (gfs->global && background->pixel < gfs->global->ncol) {
      *background = gfs->global->col[ background->pixel ];
      background->haspixel = 1;
    } else {
      error("background color index `%d' out of range", background->pixel);
      background->haspixel = 0;
    }
  }
  
  /* 2. search the existing streams and images for background colors
        (prefer colors from images with disposal == BACKGROUND)
     -> choose the color. warn if two such images have conflicting colors */
  if (background->haspixel == 0) {
    int relevance = 0;
    int saved_bg_transparent = 0;
    for (i = 0; i < nmerger; i++) {
      Gif_Stream *gfs = merger[i]->stream;
      int bg_disposal = merger[i]->image->disposal == GIF_DISPOSAL_BACKGROUND;
      int bg_transparent = gfs->images[0]->transparent >= 0;
      int bg_exists = gfs->global && gfs->background < gfs->global->ncol;
      
      if ((!bg_exists && !bg_transparent) || (bg_disposal + 1 < relevance))
	continue;
      else if (bg_disposal + 1 > relevance) {
	if (bg_exists)
	  *background = gfs->global->col[gfs->background];
	else
	  background->red = background->green = background->blue = 0;
	saved_bg_transparent = bg_transparent;
	relevance = bg_disposal + 1;
	continue;
      }
      
      /* check for conflicting background requirements */
      if (bg_transparent != saved_bg_transparent
	  || (!saved_bg_transparent &&
	      !GIF_COLOREQ(background, &gfs->global->col[gfs->background]))) {
	static int context = 0;
	warning("input images have conflicting background colors");
	if (!context) {
	  warning("(This means some animation frames may appear incorrect.)");
	  context = 1;
	}
	break;
      }
    }
    background->haspixel = relevance != 0;
  }
  
  /* 3. we found a background color
     -> force the merging process to keep it in the global colormap with
     COLORMAP_ENSURE_SLOT_255. See merge.c function ensure_slot_255 */
  if (background->haspixel) {
    dest_global->userflags |= COLORMAP_ENSURE_SLOT_255;
    dest_global->col[255] = *background;
  }
}


static int
find_color_or_error(Gif_Color *color, Gif_Stream *gfs, Gif_Image *gfi,
		    char *color_context)
{
  Gif_Colormap *gfcm = gfs->global;
  int index;
  if (gfi && gfi->local) gfcm = gfi->local;
  
  if (color->haspixel == 2) {	/* have pixel value, not color */
    if (color->pixel < gfcm->ncol)
      return color->pixel;
    else {
      if (color_context) error("%s color out of range", color_context);
      return -1;
    }
  }
  
  index = Gif_FindColor(gfcm, color);
  if (index < 0 && color_context)
    error("%s color not in colormap", color_context);
  return index;
}


static void
fix_total_crop(Gif_Stream *dest, Gif_Image *srci, int merger_index)
{
  /* Salvage any relevant information from a frame that's been completely
     cropped away. This ends up being comments and delay. */
  Gt_Frame *fr = merger[merger_index];
  Gt_Frame *next_fr = 0;
  Gif_Image *prev_image = 0;
  if (dest->nimages > 0) prev_image = dest->images[dest->nimages - 1];
  if (merger_index < nmerger - 1) next_fr = merger[merger_index + 1];
  
  /* Don't save identifiers since the frame that was to be identified, is
     gone. Save comments though. */
  if (!fr->no_comments && srci->comment && next_fr) {
    if (!next_fr->comment) next_fr->comment = Gif_NewComment();
    merge_comments(next_fr->comment, srci->comment);
  }
  if (fr->comment && next_fr) {
    if (!next_fr->comment) next_fr->comment = Gif_NewComment();
    merge_comments(next_fr->comment, fr->comment);
    Gif_DeleteComment(fr->comment);
    fr->comment = 0;
  }
  
  /* Save delay by adding it to the previous frame's delay. */
  if (fr->delay < 0)
    fr->delay = srci->delay;
  prev_image->delay += fr->delay;
}


static void
handle_screen(Gif_Stream *dest, u_int16_t width, u_int16_t height)
{
  /* Set the screen width & height, if the current input width and height are
     larger */
  if (dest->screen_width < width)
    dest->screen_width = width;
  if (dest->screen_height < height)
    dest->screen_height = height;
}

static void
handle_flip_and_screen(Gif_Stream *dest, Gif_Image *desti,
		       Gt_Frame *fr, int first_image)
{
  Gif_Stream *gfs = fr->stream;
  
  u_int16_t screen_width = gfs->screen_width;
  u_int16_t screen_height = gfs->screen_height;
  
  if (fr->flip_horizontal)
    flip_image(desti, screen_width, screen_height, 0);
  if (fr->flip_vertical)
    flip_image(desti, screen_width, screen_height, 1);
  
  if (fr->rotation == 1)
    rotate_image(desti, screen_width, screen_height, 1);
  else if (fr->rotation == 2) {
    flip_image(desti, screen_width, screen_height, 0);
    flip_image(desti, screen_width, screen_height, 1);
  } else if (fr->rotation == 3)
    rotate_image(desti, screen_width, screen_height, 3);
  
  /* handle screen size, which might have height & width exchanged */
  if (fr->rotation == 1 || fr->rotation == 3)
    handle_screen(dest, screen_height, screen_width);
  else
    handle_screen(dest, screen_width, screen_height);
}


Gif_Stream *
merge_frame_interval(Gt_Frameset *fset, int f1, int f2,
		     Gt_OutputData *output_data, int compress_immediately)
{
  Gif_Stream *dest = Gif_NewStream();
  Gif_Colormap *global = Gif_NewFullColormap(256, 256);
  Gif_Color dest_background;
  int i;
  
  global->ncol = 0;
  dest->global = global;
  /* 11/23/98 A new stream's screen size is 0x0; we'll use the max of the
     merged-together streams' screen sizes by default (in merge_stream()) */
  
  if (f2 < 0) f2 = fset->count - 1;
  nmerger = 0;
  merger_flatten(fset, f1, f2);
  if (nmerger == 0) {
    error("empty output GIF not written");
    return 0;
  }
  
  /* merge stream-specific info and clear colormaps */
  for (i = 0; i < nmerger; i++)
    merger[i]->stream->userflags = 1;
  for (i = 0; i < nmerger; i++) {
    if (merger[i]->stream->userflags) {
      Gif_Stream *src = merger[i]->stream;
      Gif_CalculateScreenSize(src, 0);
      /* merge_stream() unmarks the global colormap */
      merge_stream(dest, src, merger[i]->no_comments);
      src->userflags = 0;
    }
    if (merger[i]->image->local)
      unmark_colors_2(merger[i]->image->local);
  }
  
  /* decide on the background. use the one from output_data */
  dest_background = output_data->background;
  find_background(global, &dest_background);
  
  /* check for cropping the whole stream */
  for (i = 0; i < nmerger; i++)
    if (merger[i]->crop)
      merger[i]->crop->whole_stream = 0;
  if (merger[0]->crop) {
    merger[0]->crop->whole_stream = 1;
    for (i = 1; i < nmerger; i++)
      if (merger[i]->crop != merger[0]->crop)
	merger[0]->crop->whole_stream = 0;
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
    int old_transparent;
    
    /* First, check for extensions */
    {
      int j;
      Gif_Extension *gfex = fr->stream->extensions;
      for (j = 0; fr->stream->images[j] != fr->image; j++) ;
      while (gfex && gfex->position < j)
	gfex = gfex->next;
      while (!fr->no_extensions && gfex && gfex->position == j) {
	Gif_AddExtension(dest, copy_extension(gfex), i);
	gfex = gfex->next;
      }
      gfex = fr->extensions;
      while (gfex) {
	Gif_Extension *next = gfex->next;
	Gif_AddExtension(dest, gfex, i);
	gfex = next;
      }
    }
    
    /* Make a copy of the image and crop it if we're cropping */
    if (fr->crop) {
      srci = Gif_CopyImage(fr->image);
      Gif_UncompressImage(srci);
      if (!crop_image(srci, fr->crop)) {
	/* We cropped the image out of existence! Be careful not to make 0x0
           frames. */
	fix_total_crop(dest, srci, i);
	goto merge_frame_done;
      }
    } else {
      srci = fr->image;
      Gif_UncompressImage(srci);
    }
    
    /* It was pretty stupid to remove this code, which I did between 1.2b6 and
       1.2 */
    old_transparent = srci->transparent;
    if (fr->transparent.haspixel == 255)
      srci->transparent = -1;
    else if (fr->transparent.haspixel)
      srci->transparent =
	find_color_or_error(&fr->transparent, fr->stream, srci, "transparent");
    
    desti = merge_image(dest, fr->stream, srci);
    
    srci->transparent = old_transparent; /* restore real transparent value */
    
    /* Flipping and rotating, and also setting the screen size */
    if (fr->flip_horizontal || fr->flip_vertical || fr->rotation)
      handle_flip_and_screen(dest, desti, fr, i == 0);
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
      desti->left = fr->left;
    if (fr->top >= 0)
      desti->top = fr->top;
    
    if (fr->delay >= 0)
      desti->delay = fr->delay;
    if (fr->disposal >= 0)
      desti->disposal = fr->disposal;
    
    /* compress immediately if possible to save on memory */
    if (compress_immediately) {
      Gif_CompressImage(dest, desti);
      Gif_ReleaseUncompressedImage(desti);
    }
    
   merge_frame_done:
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
      fr->image = 0;
    }
    
    /* 5/26/98 Destroy the stream now to help with memory. Assumes that
       all frames are added with add_frame() which properly increments the
       stream's refcount. Set field to 0 so we don't redelete */
    Gif_DeleteStream(fr->stream);
    fr->stream = 0;
  }
  /** END MERGE LOOP **/

  /* Cropping the whole output? Clear the logical screen */
  if (merger[0]->crop && merger[0]->crop == merger[nmerger-1]->crop)
    dest->screen_width = dest->screen_height = 0;
  /* Set the logical screen from the user's preferences */
  if (output_data->screen_width >= 0)
    dest->screen_width = output_data->screen_width;
  if (output_data->screen_height >= 0)
    dest->screen_height = output_data->screen_height;
  
  /* Find the background color in the colormap, or add it if we can */
  {
    int bg = find_color_or_error(&dest_background, dest, 0, 0);
    if (bg < 0 && dest->images[0]->transparent >= 0)
      dest->background = dest->images[0]->transparent;
    else if (bg < 0 && global->ncol < 256) {
      dest->background = global->ncol;
      global->col[ global->ncol ] = dest_background;
      global->ncol++;
    } else
      dest->background = bg;
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
