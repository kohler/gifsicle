/* gifdiff.c - Gifdiff compares GIF images for identical appearance.
   Copyright (C) 1998 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifdiff, in the gifsicle package.

   Gifdiff is free software. It is distributed under the GNU Public License,
   version 2 or later; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include "gif.h"
#include "clp.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define QUIET_OPT		300
#define HELP_OPT		301
#define VERSION_OPT		302

Clp_Option options[] = {
  { "help", 'h', HELP_OPT, 0, 0 },
  { "brief", 'q', QUIET_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 },
};

static const char *program_name;

static const char *filename1;
static const char *filename2;

static int screen_width, screen_height;
#define TRANSP (0)

static u_int16_t *data1;
static u_int16_t *data2;
static u_int16_t *last1;
static u_int16_t *last2;

static int brief;


static void
combine_colormaps(Gif_Colormap *gfcm, Gif_Colormap *newcm)
{
  int i;
  if (!gfcm) return;
  for (i = 0; i < gfcm->ncol; i++) {
    Gif_Color *c = &gfcm->col[i];
    c->pixel = Gif_AddColor(newcm, c, 1);
  }
}

static void
fill_area(u_int16_t *data, int l, int t, int w, int h, u_int16_t val)
{
  int x, y;
  for (y = t; y < t+h; y++) {
    u_int16_t *d = data + screen_width * y + l;
    for (x = 0; x < w; x++)
      *d++ = val;
  }
}


static void
apply_image(int is_second, Gif_Stream *gfs, Gif_Image *gfi)
{
  int i, x, y;
  int width = gfi->width;
  u_int16_t map[256];
  u_int16_t *data = (is_second ? data2 : data1);
  u_int16_t *last = (is_second ? last2 : last1);
  Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
  
  /* set up colormap */
  for (i = 0; i < 256; i++)
    map[i] = 1;
  if (gfs)
    for (i = 0; i < gfcm->ncol; i++)
      map[i] = gfcm->col[i].pixel;
  if (gfi->transparent >= 0 && gfi->transparent < 256)
    map[gfi->transparent] = TRANSP;
  
  /* copy image over */
  Gif_UncompressImage(gfi);
  Gif_ClipImage(gfi, 0, 0, screen_width, screen_height);
  for (y = 0; y < gfi->height; y++) {
    u_int16_t *outd = data + screen_width * (y + gfi->top) + gfi->left;
    byte *ind = gfi->img[y];
    if (gfi->disposal == GIF_DISPOSAL_PREVIOUS)
      memcpy(last + screen_width * y + gfi->left, outd,
	     sizeof(u_int16_t) * width);
    for (x = 0; x < width; x++, outd++, ind++)
      if (map[*ind] != TRANSP)
	*outd = map[*ind];
  }
  if (gfi->compressed)
    Gif_ReleaseCompressedImage(gfi);
}

static void
apply_image_disposal(int is_second, Gif_Stream *gfs, Gif_Image *gfi,
		     u_int16_t background)
{
  int x, y, width = gfi->width;
  u_int16_t *data = (is_second ? data2 : data1);
  u_int16_t *last = (is_second ? last2 : last1);
  
  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS)
    for (y = gfi->top; y < gfi->top + gfi->height; y++)
      memcpy(data + screen_width * y + gfi->left,
	     last + screen_width * y + gfi->left,
	     sizeof(u_int16_t) * width);
  else if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
    for (y = gfi->top; y < gfi->top + gfi->height; y++) {
      u_int16_t *d = data + screen_width * y + gfi->left;
      for (x = 0; x < gfi->width; x++)
	*d++ = background;
    }
}


#define SAME 0
#define DIFFERENT 1
static int was_different;

static void
different(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  if (!brief) {
    vfprintf(stderr, format, val);
    fputc('\n', stderr);
  }
  va_end(val);
  was_different = 1;
}


static void
name_loopcount(int loopcount, char *buf)
{
  if (loopcount < 0)
    strcpy(buf, "none");
  else if (loopcount == 0)
    strcpy(buf, "forever");
  else
    sprintf(buf, "%d", loopcount);
}

static void
name_delay(int delay, char *buf)
{
  if (delay == 0)
    strcpy(buf, "none");
  else
    sprintf(buf, "%d.%02ds", delay / 100, delay % 100);
}

static void
name_color(int color, Gif_Colormap *gfcm, char *buf)
{
  if (color == TRANSP)
    strcpy(buf, "transparent");
  else {
    Gif_Color *c = &gfcm->col[color];
    sprintf(buf, "#%02X%02X%02X", c->red, c->green, c->blue);
  }
}


int
compare(Gif_Stream *s1, Gif_Stream *s2)
{
  Gif_Colormap *newcm;
  int imageno, background1, background2;
  char buf1[256], buf2[256];
  
  was_different = 0;

  /* Compare image counts and screen sizes. If either of these differs, quit
     early. */
  Gif_CalculateScreenSize(s1, 0);
  Gif_CalculateScreenSize(s2, 0);
  
  if (s1->nimages != s2->nimages)
    different("frame counts differ: <%d >%d", s1->nimages, s2->nimages);
  if (s1->screen_width != s2->screen_width
      || s1->screen_height != s2->screen_height)
    different("screen sizes differ: <%dx%d >%dx%d", s1->screen_width,
	      s1->screen_height, s2->screen_width, s2->screen_height);

  if (was_different)
    return DIFFERENT;
  else if (s1->nimages == 0)
    return SAME;

  /* Create arrays for the image data */
  screen_width = s1->screen_width;
  screen_height = s1->screen_height;
  
  data1 = Gif_NewArray(u_int16_t, screen_width * screen_height);
  data2 = Gif_NewArray(u_int16_t, screen_width * screen_height);
  last1 = Gif_NewArray(u_int16_t, screen_width * screen_height);
  last2 = Gif_NewArray(u_int16_t, screen_width * screen_height);

  /* Merge all distinct colors from the two images into one colormap, setting
     the `pixel' slots in the images' colormaps to the corresponding values
     in the merged colormap. Don't forget transparency */
  newcm = Gif_NewFullColormap(1, 256);
  combine_colormaps(s1->global, newcm);
  combine_colormaps(s2->global, newcm);
  for (imageno = 0; imageno < s1->nimages; imageno++) {
    combine_colormaps(s1->images[imageno]->local, newcm);
    combine_colormaps(s2->images[imageno]->local, newcm);
  }
  
  /* Choose the background values and clear the image data arrays */
  if (s1->images[0]->transparent >= 0 || !s1->global)
    background1 = TRANSP;
  else
    background1 = s1->global->col[ s1->background ].pixel;

  if (s2->images[0]->transparent >= 0 || !s2->global)
    background2 = TRANSP;
  else
    background2 = s2->global->col[ s2->background ].pixel;
  
  fill_area(data1, 0, 0, screen_width, screen_height, background1);
  fill_area(data2, 0, 0, screen_width, screen_height, background2);
  
  /* Loopcounts differ? */
  if (s1->loopcount != s2->loopcount) {
    name_loopcount(s1->loopcount, buf1);
    name_loopcount(s2->loopcount, buf2);
    different("loop counts differ: <%s >%s", buf1, buf2);
  }
  
  /* Loop over frames, comparing image data and delays */
  for (imageno = 0; imageno < s1->nimages; imageno++) {
    Gif_Image *gfi1 = s1->images[imageno], *gfi2 = s2->images[imageno];
    apply_image(0, s1, gfi1);
    apply_image(1, s2, gfi2);
    
    if (memcmp(data1, data2, screen_width * screen_height * sizeof(u_int16_t))
	!= 0) {
      int d, c = screen_width * screen_height;
      u_int16_t *d1 = data1, *d2 = data2;
      for (d = 0; d < c; d++, d1++, d2++)
	if (*d1 != *d2) {
	  name_color(*d1, newcm, buf1);
	  name_color(*d2, newcm, buf2);
	  different("frame #%d pixels differ: %d,%d <%s >%s",
		    imageno, d % screen_width, d / screen_width, buf1, buf2);
	  break;
	}
    }
    
    if (gfi1->delay != gfi2->delay) {
      name_delay(gfi1->delay, buf1);
      name_delay(gfi2->delay, buf2);
      different("frame #%d delays differ: <%s >%s", imageno, buf1, buf2);
    }
    
    apply_image_disposal(0, s1, gfi1, background1);
    apply_image_disposal(1, s2, gfi2, background2);
  }

  /* That's it! */
  Gif_DeleteColormap(newcm);
  Gif_DeleteArray(data1);
  Gif_DeleteArray(data2);
  Gif_DeleteArray(last1);
  Gif_DeleteArray(last2);
  
  return was_different ? DIFFERENT : SAME;
}


void
short_usage(void)
{
  fprintf(stderr, "Usage: %s [options] GIF-file-1 GIF-file-2\n\
Type %s --help for more information.\n",
	  program_name, program_name);
}


void
usage(void)
{
  printf("\
`Gifdiff' compares two GIF files (either images or animations) for identical\n\
visual appearance. An animation and an optimized version of the same animation\n\
should compare as the same. Gifdiff exits with status 0 if the images are\n\
the same, 1 if they're different, and 2 if there was some error.\n\
\n\
Usage: %s [options] GIF-file-1 GIF-file-2\n\
\n\
Options:\n\
  --brief, -q                   Don't report detailed differences.\n\
  --help, -h                    Print this message and exit.\n\
  --version, -v                 Print version number and exit.\n\
\n\
Report bugs to <eddietwo@lcs.mit.edu>.\n", program_name);
}


void
fatal_error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: ", program_name);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
  exit(2);			/* exit(2) for trouble */
}

void
error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: ", program_name);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
}

void
warning(char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: warning: ", program_name);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
}


int
main(int argc, char **argv)
{
  int how_many_inputs = 0;
  int status;
  const char **inputp;
  FILE *f1, *f2;
  Gif_Stream *gfs1, *gfs2;
  
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  
  program_name = Clp_ProgramName(clp);
  brief = 0;
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("Gifdiff version %s\n", VERSION);
      printf("Copyright (C) 1998 Eddie Kohler\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case QUIET_OPT:
      brief = !clp->negated;
      break;
      
     case Clp_NotOption:
      if (how_many_inputs == 2)
	fatal_error("too many file arguments");
      inputp = (how_many_inputs == 0 ? &filename1 : &filename2);
      how_many_inputs++;
      if (strcmp(clp->arg, "-") == 0)
	*inputp = 0;
      else
	*inputp = clp->arg;
      break;
      
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;
      
     case Clp_Done:
      goto done;
      
    }
  }
  
 done:
  
  if (how_many_inputs < 2)
    fatal_error("need exactly 2 file arguments");
  if (filename1 == 0 && filename2 == 0)
    fatal_error("can't read both files from stdin");
  
  if (filename1 == 0) {
    f1 = stdin;
    filename1 = "<stdin>";
  } else {
    f1 = fopen(filename1, "rb");
    if (!f1)
      fatal_error("%s: %s", filename1, strerror(errno));
  }
  gfs1 = Gif_FullReadFile(f1, GIF_READ_COMPRESSED);
  if (!gfs1)
    fatal_error("`%s' doesn't seem to contain a GIF", filename1);
  
  if (filename2 == 0) {
    f2 = stdin;
    filename2 = "<stdin>";
  } else {
    f2 = fopen(filename2, "rb");
    if (!f2)
      fatal_error("%s: %s", filename2, strerror(errno));
  }
  gfs2 = Gif_FullReadFile(f2, GIF_READ_COMPRESSED);
  if (!gfs2) fatal_error("`%s' doesn't seem to contain a GIF", filename2);
  
  status = (compare(gfs1, gfs2) == DIFFERENT);
  if (status == 1 && brief)
    printf("GIF files %s and %s differ\n", filename1, filename2);

  Gif_DeleteStream(gfs1);
  Gif_DeleteStream(gfs2);
#ifdef DMALLOC
  dmalloc_report();
#endif
  return status;
}
