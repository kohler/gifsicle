/* support.c - Support functions for gifsicle.
   Copyright (C) 1997-8 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2 or later; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include "gifsicle.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>


const char *program_name = "gifsicle";
static int verbose_pos = 0;


static void
verror(int is_warning, const char *location, char *message, va_list val)
{
  char buffer[BUFSIZ];
  verbose_endline();
  
  /* try and keep error messages together (no interleaving of error messages
     from two gifsicle processes in the same command line) by calling fprintf
     only once */
  if (strlen(message) + strlen(location) + 13 < BUFSIZ) {
    sprintf(buffer, "%s: %s%s\n", location, (is_warning ? "warning: " : ""),
	    message);
    vfprintf(stderr, buffer, val);
  } else {
    fprintf(stderr, "%s: ", location);
    if (is_warning) fprintf(stderr, "warning: ");
    vfprintf(stderr, message, val);
    putc('\n', stderr);
  }
}

void
fatal_error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  verror(0, program_name, message, val);
  va_end(val);
  exit(1);
}

void
error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  verror(0, program_name, message, val);
  va_end(val);
}

void
warning(char *message, ...)
{
  va_list val;
  va_start(val, message);
  verror(1, program_name, message, val);
  va_end(val);
}


void
short_usage(void)
{
  fprintf(stderr, "Usage: %s [options, frames, and filenames] ...\n\
Type %s --help for more information.\n",
	  program_name, program_name);
}


void
usage(void)
{
  printf("\
`Gifsicle' manipulates GIF images in many different ways. Its most common\n\
uses include combining single frames into animations, adding transparency,\n\
optimizing animations for space, and printing information about GIFs.\n\
\n\
Usage: %s [options, frames, and filenames] ...\n\
\n\
Mode options: at most one, before any filenames.\n\
  --merge, -m                   Merge mode: combine inputs, write stdout.\n\
  --batch, -b                   Batch mode: modify inputs, write back to\n\
                                same filenames.\n\
  --explode, -e                 Explode mode: write N files for each input,\n\
                                one per frame, to `input.frame-number'.\n\
  --explode-by-name, -E         Explode mode, but write `input.name'.\n\
\n\
General options: Also --no-OPTION for info and verbose.\n\
  --info, -I                    Print info about input GIFs. Two -I's means\n\
                                normal output is not suppressed.\n\
  --color-info, --cinfo         --info plus colormap details.\n\
  --extension-info, --xinfo     --info plus extension details.\n\
  --verbose, -v                 Prints progress information.\n\
  --help, -h                    Print this message and exit.\n\
  --version                     Print version number and exit.\n\
  --output FILE, -o FILE        Write output to FILE.\n\
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
  --background COL, -B COL      Makes COL the background color.\n\
  --crop X,Y+WxH or X,Y-X2,Y2   Clips the image.\n\
  --flip-horizontal, --flip-vertical\n\
                                Flips the image.\n\
  --interlace, -i               Turns on interlacing.\n\
  --logical-screen WxH, -S WxH  Sets logical screen to WxH.\n\
  --position X,Y, -p X,Y        Sets frame position to (X,Y).\n\
  --rotate-90, --rotate-180, --rotate-270, --no-rotate\n\
                                Rotates the image.\n\
  --transparent COL, -t COL     Makes COL transparent.\n\
\n\
Extension options: Also --no-OPTION and --same-OPTION.\n\
  --app-extension N D, -x N D   Adds an app extension named N with data D.\n\
  --comment TEXT, -c TEXT       Adds a comment before the next frame.\n\
  --extension N D               Adds an extension number N with data D.\n\
  --name TEXT, -n TEXT          Sets next frame's name.\n\
\n");
  printf("\
Animation options: Also --no-OPTION and --same-OPTION.\n\
  --delay TIME, -d TIME         Sets frame delay to TIME (in 1/100sec).\n\
  --disposal METHOD, -D METHOD  Sets frame disposal to METHOD.\n\
  --loopcount[=N], -l[N]        Sets loop extension to N (default forever).\n\
  --optimize[=LEV], -O[LEV]     Optimize output GIFs.\n\
  --unoptimize, -U              Unoptimize input GIFs.\n\
\n\
Whole-GIF options: Also --no-OPTION.\n\
  --change-color COL1 COL2      Changes COL1 to COL2 throughout.\n\
  --colors N, -k N              Reduces the number of colors to N.\n\
  --color-method METHOD         Set method for choosing reduced colors.\n\
  --dither, -f                  Dither image after changing colormap.\n\
  --transform-colormap CMD      Transform each colormap by the shell CMD.\n\
  --use-colormap CMAP           Set the GIF's colormap to CMAP, which can be\n\
                                `web', `gray', `bw', or a GIF file.\n\
\n");
  printf("\
Report bugs to <eddietwo@lcs.mit.edu>.\n\
Too much information? Try `%s --help | more'.\n", program_name);
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
    fprintf(where, " compressed size %u min_bits %d", gfi->compressed_len,
	    *gfi->compressed);
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
explode_filename(char *filename, int number, char *name)
{
  static char *s;
  int l = strlen(filename);
  l += name ? strlen(name) : 10;
  
  Gif_Delete(s);
  s = Gif_NewArray(char, l + 3);
  if (name)
    sprintf(s, "%s.%s", filename, name);
  else
    sprintf(s, "%s.%d", filename, number);
  
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


int
parse_frame_spec(Clp_Parser *clp, const char *arg, void *v, int complain)
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
parse_dimensions(Clp_Parser *clp, const char *arg, void *v, int complain)
{
  char *val;
  
  dimensions_x = strtol(arg, &val, 10);
  if (*val == 'x') {
    dimensions_y = strtol(val + 1, &val, 10);
    if (*val == 0)
      return 1;
  }
  
  if (complain)
    return Clp_OptionError(clp, "`%O's argument must be a dimension pair, WxH");
  else
    return 0;
}


int
parse_position(Clp_Parser *clp, const char *arg, void *v, int complain)
{
  char *val;
  
  position_x = strtol(arg, &val, 10);
  if (*val == ',') {
    position_y = strtol(val + 1, &val, 10);
    if (*val == 0)
      return 1;
  }
  
  if (complain)
    return Clp_OptionError(clp, "`%O's argument must be a position, X,Y");
  else
    return 0;
}


int
parse_rectangle(Clp_Parser *clp, const char *arg, void *v, int complain)
{
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
    return Clp_OptionError(clp, "`%O's argument must be a rectangle, X1,Y1-X2,Y2 or X1,Y1+WxH");
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
parse_color(Clp_Parser *clp, const char *arg, void *v, int complain)
{
  char *str;
  int red, green, blue;
  
  if (*arg == '#') {
    int len = strlen(++arg);
    if (!len || len % 3 != 0) goto error;
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
    return Clp_OptionError(clp, "`%O's argument must be a valid color");
  else
    return 0;
}


int
parse_two_colors(Clp_Parser *clp, const char *arg, void *v, int complain)
{
  Gif_Color old_color;
  Clp_ParserState *save;
  if (!parse_color(clp, arg, v, complain))
    return 0;
  old_color = parsed_color;
  
  save = Clp_NewParserState();
  Clp_SaveParser(clp, save);
  if (Clp_Next(clp) != Clp_NotOption) {
    Clp_RestoreParser(clp, save);
    Clp_DeleteParserState(save);
    if (complain)
      return Clp_OptionError(clp, "`%O' takes two color arguments");
    else
      return 0;
  } else {
    Clp_DeleteParserState(save);
    if (!parse_color(clp, clp->arg, v, complain))
      return 0;
    parsed_color2 = parsed_color;
    parsed_color = old_color;
    return 1;
  }
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
  def_frame.name_change = 0;

  def_frame.comment = 0;
  def_frame.comment_change = 0;  

  def_frame.background.haspixel = 0;
  def_frame.background_change = 0;

  def_frame.extensions = 0;
  def_frame.extensions_change = 0;
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
  
  /* Warn about background option not in first position */
  if (number > 0 && def_frame.background.haspixel)
    warning("`--background' is only effective on the first frame");
  
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

static int warned_background_conflicts;

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
  *background = merger[0]->background;
  
  /* This code is SUCH a PAIN in the PATOOTIE!! */
  
  /* 1. user set the background to a color index
     -> find the associated color & fall through to case 2 */
  if (background->haspixel == 2) {
    Gif_Stream *gfs = merger[0]->stream;
    if (background->pixel == 256
	|| background->pixel == gfs->images[0]->transparent)
      background->pixel = 256;
    else if (gfs->global && background->pixel < gfs->global->ncol) {
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
    int report_conflict = 0;
    for (i = 0; i < nmerger; i++) {
      Gif_Color new_bg;
      Gif_Stream *gfs = merger[i]->stream;
      int relevant = merger[i]->image->disposal == GIF_DISPOSAL_BACKGROUND;
      if (!relevant && report_conflict) continue;
      
      if (gfs->background == gfs->images[0]->transparent
	  || gfs->background == merger[i]->image->transparent)
	new_bg.pixel = 256;
      else if (gfs->global && gfs->background < gfs->global->ncol) {
	new_bg = gfs->global->col[ gfs->background ];
	new_bg.pixel = 0;
      } else
	continue;
      
      if (report_conflict < 2) *background = new_bg;
      report_conflict = relevant ? 2 : 1;
      
      /* check for conflicting background requirements */
      if ((new_bg.pixel == 256) != (background->pixel == 256)
	  || (new_bg.pixel == 0 && !GIF_COLOREQ(&new_bg, background)))
	if (!warned_background_conflicts) {
	  warning("some required background colors conflict; I picked one");
	  warning("  (some animation frames will appear incorrect)");
	  warned_background_conflicts = 1;
	}
    }
    background->haspixel = (report_conflict > 1);
  }
  
  /* 3. user set the background to a specific color, or we need the background
        color to make the animation work (both cases flagged with
	haspixel == 1)
     -> put it in the colormap immediately */
  if (background->haspixel == 1) {
    dest_global->ncol = 1;
    dest_global->col[0] = *background;
    background->haspixel = 1;
    background->pixel = 0;
  }
}


static int
find_color_or_error(Gif_Color *color, Gif_Stream *gfs, Gif_Image *gfi,
		    char *color_context)
{
  Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
  int index;
  
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
do_flip(Gif_Image *gfi, int screen_width, int screen_height, int is_vert)
{
  int x, y;
  int width = gfi->width;
  int height = gfi->height;
  byte **img = gfi->img;
  
  /* horizontal flips */
  if (!is_vert) {
    byte *buffer = Gif_NewArray(byte, width);
    byte *trav;
    for (y = 0; y < height; y++) {
      memcpy(buffer, img[y], width);
      trav = img[y] + width - 1;
      for (x = 0; x < width; x++)
	*trav-- = buffer[x];
    }
    gfi->left = screen_width - (gfi->left + width);
    Gif_DeleteArray(buffer);
  }
  
  /* vertical flips */
  if (is_vert) {
    byte **buffer = Gif_NewArray(byte *, height);
    memcpy(buffer, img, height * sizeof(byte *));
    for (y = 0; y < height; y++)
      img[y] = buffer[height - y - 1];
    gfi->top = screen_height - (gfi->top + height);
    Gif_DeleteArray(buffer);
  }
}


static void
do_rotate(Gif_Image *gfi, int screen_width, int screen_height, Gt_Frame *fr,
	  int first_image)
{
  int x, y;
  int width = gfi->width;
  int height = gfi->height;
  byte **img = gfi->img;
  byte *new_data = Gif_NewArray(byte, width * height);
  byte *trav = new_data;
  
  if (fr->rotation == 1) {
    for (x = 0; x < width; x++)
      for (y = height - 1; y >= 0; y--)
	*trav++ = img[y][x];
    x = gfi->left;
    gfi->left = screen_height - (gfi->top + height);
    gfi->top = x;
    
  } else {
    for (x = width - 1; x >= 0; x--)
      for (y = 0; y < height; y++)
	*trav++ = img[y][x];
    y = gfi->top;
    gfi->top = screen_width - (gfi->left + width);
    gfi->left = y;
  }
  
  /* If this is the first frame, set the frame's screen width & height as the
     flipped screen height & width. This ensures that if a single rotated
     frame is output, its logical screen will be rotated too. */
  if (fr->screen_width <= 0 && first_image) {
    fr->screen_width = screen_height;
    fr->screen_height = screen_width;
  }
  
  Gif_ReleaseUncompressedImage(gfi);
  gfi->width = height;
  gfi->height = width;
  Gif_SetUncompressedImage(gfi, new_data, Gif_DeleteArrayFunc, 0);
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
    do_flip(desti, screen_width, screen_height, 0);
  if (fr->flip_vertical)
    do_flip(desti, screen_width, screen_height, 1);
  
  if (fr->rotation == 1)
    do_rotate(desti, screen_width, screen_height, fr, first_image);
  else if (fr->rotation == 2) {
    do_flip(desti, screen_width, screen_height, 0);
    do_flip(desti, screen_width, screen_height, 1);
  } else if (fr->rotation == 3)
    do_rotate(desti, screen_width, screen_height, fr, first_image);
  
  /* handle screen size, which might have height & width exchanged */
  if (fr->rotation == 1 || fr->rotation == 3)
    handle_screen(dest, screen_height, screen_width);
  else
    handle_screen(dest, screen_width, screen_height);
}


Gif_Stream *
merge_frame_interval(Gt_Frameset *fset, int f1, int f2,
		     int compress_immediately)
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
  
  /* decide on the background */
  warned_background_conflicts = 0;
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
  
  /* Copy stream-wide information from the last frame in the set */
  {
    Gt_Frame *fr = merger[nmerger - 1];
    if (fr->loopcount > -2)
      dest->loopcount = fr->loopcount;
    if (fr->screen_width >= 0)
      dest->screen_width = fr->screen_width;
    if (fr->screen_height >= 0)
      dest->screen_height = fr->screen_height;
  }
  
  /* Set the background */
  if (dest_background.haspixel == 0)
    dest_background.pixel =
      find_color_or_error(&dest_background, dest, dest->images[0], 0);
  else if (dest_background.pixel == 256) {
    if (dest->images[0]->transparent < 0) {
      warning("no transparency in first image:");
      warning("transparent background won't work as expected");
    } else
      dest_background.pixel = dest->images[0]->transparent;
  }
  dest->background = dest_background.pixel;

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
