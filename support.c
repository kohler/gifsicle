/* support.c - Support functions for gifsicle.
   Copyright (C) 1997-8 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software; you can copy, distribute, or alter it at will, as
   long as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include "gifsicle.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>


const char *program_name = "gifsicle";
static int verbose_pos = 0;


void
fatal_error(char *message, ...)
{
  va_list val;
  verbose_endline();
  va_start(val, message);
  fprintf(stderr, "%s: ", program_name);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
  exit(1);
}


void
error(char *message, ...)
{
  va_list val;
  verbose_endline();
  va_start(val, message);
  fprintf(stderr, "%s: ", program_name);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
}


void
warning(char *message, ...)
{
  va_list val;
  verbose_endline();
  va_start(val, message);
  fprintf(stderr, "%s: warning: ", program_name);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
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
  fprintf(stderr, "Usage: %s [options, frames, and filenames] ...\n\
Mode options: at most one, before any filenames.\n\
  --merge, -m                   Merge mode: combine inputs, write stdout.\n\
  --batch, -b                   Batch mode: modify inputs, write back to\n\
                                same filenames.\n\
  --explode, -e                 Explode mode: write N files for each input,\n\
                                one per frame, to `input.frame-number'.\n\
  --explode-by-name, -E         Explode mode, but write `input.name'.\n\
General options: Also --no-opt/+o for info and verbose.\n\
  --info, -I                    Print info about input GIFs. Two -I's means\n\
                                normal output is not suppressed.\n\
  --color-info, --cinfo         --info plus colormap details.\n\
  --extension-info, --einfo     --info plus extension details.\n\
  --verbose, -v                 Prints progress information.\n\
  --help, -h                    Print this message and exit.\n\
  --version                     Print version number and exit.\n\
  --output FILE, -o FILE        Write output to FILE.\n\
Frame selections:               #num, #num1-num2, #num1-, #-num2, #name\n\
Frame change options:\n\
  --delete FRAMES               Delete FRAMES from input.\n\
  --insert-before FRAME GIFS    Insert GIFS before FRAMES in input.\n\
  --append GIFS                 Append GIFS to input.\n\
  --replace FRAMES GIFS         Replace FRAMES with GIFS in input.\n\
  --done                        Done with frame changes.\n\
Image options: Also --no-opt/+o and --same-opt.\n\
  --comment TEXT, -c TEXT       Adds a comment before the next frame.\n\
  --crop X,Y+WxH or X,Y-X2,Y2   Clips the image.\n\
  --interlace, -i               Turns on interlacing.\n\
  --logical-screen WxH, -S WxH  Sets logical screen to WxH.\n\
  --name TEXT, -n TEXT          Sets next frame's name.\n\
  --position X,Y, -p X,Y        Sets frame position to (X,Y).\n\
  --transparent COL, -t COL     Makes COL transparent.\n\
Animation options: Also --no-opt/+o and --same-opt.\n\
  --delay TIME, -d TIME         Sets frame delay to TIME (in 1/100sec).\n\
  --disposal METHOD, -D METHOD  Sets frame disposal to METHOD.\n\
  --loopcount[=N], -l[N]        Sets loop extension to N (default forever).\n\
  --optimize[=LEV], -O[LEV]     Optimize output GIFs.\n\
  --unoptimize, -U              Unoptimize input GIFs.\n\
Whole-GIF options: Also --no-opt.\n\
  --change-color COL1 COL2      Changes COL1 to COL2 throughout.\n\
  --colors N, -k N              Reduces the number of colors to N.\n\
  --color-method METHOD         Set method for choosing reduced colors.\n\
  --dither, -f                  Dither image after changing colormap.\n\
  --use-colormap FILE or `web'  Use specified colormap.\n\
",
	  program_name);
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
comment_info(Gif_Comment *gfcom, char *prefix)
{
  int i;
  for (i = 0; i < gfcom->count; i++) {
    fputs(prefix, stderr);
    safe_puts(gfcom->str[i], gfcom->len[i], stderr);
    fputc('\n', stderr);
  }
}


#define COLORMAP_COLS	4

static void
colormap_info(Gif_Colormap *gfcm, char *prefix)
{
  int i, j;
  int nrows = ((gfcm->ncol - 1) / COLORMAP_COLS) + 1;
  
  for (j = 0; j < nrows; j++) {
    int which = j;
    fputs(prefix, stderr);
    for (i = 0; i < COLORMAP_COLS && which < gfcm->ncol; i++, which += nrows) {
      if (i) fputs("    ", stderr);
      fprintf(stderr, " %3d: #%02X%02X%02X", which, gfcm->col[which].red,
	      gfcm->col[which].green, gfcm->col[which].blue);
    }
    fputc('\n', stderr);
  }
}


static void
extension_info(Gif_Stream *gfs, Gif_Extension *gfex, int count)
{
  byte *data = gfex->data;
  u_int32_t pos = 0;
  u_int32_t len = gfex->length;
  
  fprintf(stderr, "  extension %d: ", count);
  if (gfex->kind == 255) {
    fprintf(stderr, "app `");
    safe_puts(gfex->application, strlen(gfex->application), stderr);
    fprintf(stderr, "'");
  } else {
    if (gfex->kind >= 32 && gfex->kind < 127)
      fprintf(stderr, "`%c' (0x%02X)", gfex->kind, gfex->kind);
    else
      fprintf(stderr, "0x%02X", gfex->kind);
  }
  if (gfex->position >= gfs->nimages)
    fprintf(stderr, " at end\n");
  else
    fprintf(stderr, " before #%d\n", gfex->position);
  
  /* Now, hexl the data. */
  while (len > 0) {
    int row = 16;
    int i;
    if (row > len) row = len;
    fprintf(stderr, "    %08x: ", pos);
    
    for (i = 0; i < row; i += 2) {
      if (i + 1 >= row)
	fprintf(stderr, "%02x   ", data[i]);
      else
	fprintf(stderr, "%02x%02x ", data[i], data[i+1]);
    }
    for (; i < 16; i += 2)
      fputs("     ", stderr);
    
    putc(' ', stderr);
    for (i = 0; i < row; i++, data++)
      putc((*data >= ' ' && *data < 127 ? *data : '.'), stderr);
    putc('\n', stderr);
    
    pos += row;
    len -= row;
  }
}


void
stream_info(Gif_Stream *gfs, char *filename, int colormaps, int extensions)
{
  Gif_Extension *gfex;
  int n;
  
  if (!gfs) return;
  gfs->userflags = 0; /* clear userflags to indicate stream info produced */
  
  verbose_endline();
  fprintf(stderr, "* %s %d image%s\n", filename, gfs->nimages,
	  gfs->nimages == 1 ? "" : "s");
  fprintf(stderr, "  logical screen %dx%d\n",
	  gfs->screen_width, gfs->screen_height);
  
  if (gfs->global) {
    fprintf(stderr, "  global color table [%d]\n", gfs->global->ncol);
    if (colormaps) colormap_info(gfs->global, "  |");
    fprintf(stderr, "  background %d\n", gfs->background);
  }
  
  if (gfs->comment)
    comment_info(gfs->comment, "  end comment ");
  
  if (gfs->loopcount == 0)
    fprintf(stderr, "  loop forever\n");
  else if (gfs->loopcount > 0)
    fprintf(stderr, "  loop count %u\n", (unsigned)gfs->loopcount);
  
  for (n = 0, gfex = gfs->extensions; gfex; gfex = gfex->next, n++)
    if (extensions)
      extension_info(gfs, gfex, n);
  if (n && !extensions)
    fprintf(stderr, "  extensions %d\n", n);
}


static char *disposal_names[] = {
  "none", "asis", "background", "previous", "4", "5", "6", "7"
};

void
image_info(Gif_Stream *gfs, Gif_Image *gfi, int colormaps)
{
  int num;
  if (!gfs || !gfi) return;
  num = Gif_ImageNumber(gfs, gfi);
  
  verbose_endline();
  fprintf(stderr, "  + image #%d ", num);
  if (gfi->identifier)
    fprintf(stderr, "#%s ", gfi->identifier);
  
  fprintf(stderr, "%dx%d", gfi->width, gfi->height);
  if (gfi->left || gfi->top)
    fprintf(stderr, " at %d,%d", gfi->left, gfi->top);
  
  if (gfi->interlace)
    fprintf(stderr, " interlaced");
  
  if (gfi->transparent >= 0)
    fprintf(stderr, " transparent %d", gfi->transparent);
  fprintf(stderr, "\n");
  
  if (gfi->comment)
    comment_info(gfi->comment, "    comment ");
  
  if (gfi->local) {
    fprintf(stderr, "    local color table [%d]\n", gfi->local->ncol);
    if (colormaps) colormap_info(gfi->local, "    |");
  }
  
  if (gfi->disposal || gfi->delay) {
    fprintf(stderr, "   ");
    if (gfi->disposal)
      fprintf(stderr, " disposal %s", disposal_names[gfi->disposal]);
    if (gfi->delay)
      fprintf(stderr, " delay %d.%02ds",
	      gfi->delay / 100, gfi->delay % 100);
    fprintf(stderr, "\n");
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

void blank_frameset(Gt_Frameset *fset, int f1, int f2);


Gt_Frameset *
new_frameset(int initial_cap)
{
  Gt_Frameset *fs = (Gt_Frameset *)malloc(sizeof(Gt_Frameset));
  if (initial_cap < 0) initial_cap = 0;
  fs->cap = initial_cap;
  fs->count = 0;
  fs->f = (Gt_Frame *)malloc(sizeof(Gt_Frame) * initial_cap);
  return fs;
}


Gt_Frame *
add_frame(Gt_Frameset *fset, int number, Gif_Stream *gfs, Gif_Image *gfi)
{
  if (number < 0) {
    while (fset->count >= fset->cap) {
      fset->cap *= 2;
      fset->f = (Gt_Frame *)realloc(fset->f, sizeof(Gt_Frame) * fset->cap);
    }
    number = fset->count++;
  } else {
    assert(number < fset->count);
    blank_frameset(fset, number, number);
  }
  
  gfs->refcount++;
  fset->f[number] = def_frame;
  fset->f[number].stream = gfs;
  fset->f[number].image = gfi;
  
  /* Get rid of next-frame-only options */
  def_frame.name = 0;
  def_frame.comment = 0;
  
  return &fset->f[number];
}


static Gif_Extension *
copy_extension(Gif_Extension *src)
{
  Gif_Extension *dest = Gif_NewExtension(src->kind, src->application);
  if (!dest) return 0;
  dest->data = Gif_NewArray(byte, src->length);
  dest->length = src->length;
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
      merger = (Gt_Frame **)realloc(merger, sizeof(Gt_Frame *) * mergercap);
    } else {
      mergercap = 16;
      merger = (Gt_Frame **)malloc(sizeof(Gt_Frame *) * mergercap);
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
      if (FRAME(fset, i).use < 0 && FRAME(nest, nest->count - 1).delay < 0)
	/* use < 0 means use the frame's delay (if not explicitly overridden),
	   but not the frame itself. */
	FRAME(nest, nest->count - 1).delay = FRAME(fset, i).image->delay;
      
      merger_flatten(nest, 0, nest->count - 1);
    }
    
    if (FRAME(fset, i).use > 0)
      merger_add(&FRAME(fset, i));
  }
}


Gif_Stream *
merge_frame_interval(Gt_Frameset *fset, int f1, int f2)
{
  Gif_Stream *dest = Gif_NewStream();
  Gif_Colormap *global = Gif_NewFullColormap(256);
  int i;
  
  global->ncol = 0;
  dest->global = global;
  
  if (f2 < 0) f2 = fset->count - 1;
  nmerger = 0;
  merger_flatten(fset, f1, f2);
  
  for (i = 0; i < nmerger; i++)
    merger[i]->stream->userflags = 1;
  
  for (i = 0; i < nmerger; i++)
    if (merger[i]->stream->userflags) {
      Gif_Stream *src = merger[i]->stream;
      merge_stream(dest, src, merger[i]->no_comments);
      src->userflags = 0;
    }
  
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
  
  for (i = 0; i < nmerger; i++) {
    Gt_Frame *fr = merger[i];
    Gif_Image *desti;
    Gif_Extension *gfex;
    int old_transparent = fr->image->transparent;
    int new_transparent = -2;
    
    /* First, check for extensions */
    gfex = fr->stream->extensions;
    while (gfex && gfex->position < i)
      gfex = gfex->next;
    while (!fr->no_extensions && gfex && gfex->position == i) {
      Gif_AddExtension(dest, copy_extension(gfex), i);
      gfex = gfex->next;
    }
    
    /* Make a copy of the image and crop it if we're cropping */
    if (fr->crop) {
      fr->image = Gif_CopyImage(fr->image);
      crop_image(fr->image, fr->crop);
    }
    
    /* Get new transparent index (in old colormap) and temporarily
       alter the image's transparency value */
    if (fr->transparent.haspixel == 255)
      new_transparent = -1;
    else if (fr->transparent.haspixel == 2)
      new_transparent = fr->transparent.pixel;
    else if (fr->transparent.haspixel) {
      new_transparent =
	find_image_color(fr->stream, fr->image, &fr->transparent);
      if (new_transparent < 0)
	warning("can't make transparent color");
    }
    if (new_transparent > -2)
      fr->image->transparent = new_transparent;
    
    desti = merge_image(dest, fr->stream, fr->image);
    
    if (fr->name || fr->no_name) {
      /* Use the fact that Gif_CopyString(0) == 0 */
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
    
    fr->image->transparent = old_transparent;
    /* Destroy the copied, cropped image if necessary */
    if (fr->crop)
      Gif_DeleteImage(fr->image);
  }
  
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
  
  return dest;
}


void
blank_frameset(Gt_Frameset *fset, int f1, int f2)
{
  int i;
  if (f2 < 0) f2 = fset->count - 1;
  for (i = f1; i <= f2; i++) {
    Gif_DeleteStream(FRAME(fset, i).stream);
    if (FRAME(fset, i).comment)
      Gif_DeleteComment( FRAME(fset, i).comment );
    if (FRAME(fset, i).nest) {
      blank_frameset(FRAME(fset, i).nest, 0, -1);
      free(FRAME(fset, i).nest);
    }
  }
}


void
clear_frameset(Gt_Frameset *fset, int f1)
{
  blank_frameset(fset, f1, -1);
  fset->count = f1;
}
