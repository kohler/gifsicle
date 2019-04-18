/* gifdiff.c - Gifdiff compares GIF images for identical appearance.
   Copyright (C) 1998-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of gifdiff, in the gifsicle package.

   Gifdiff is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include <lcdfgif/gif.h>
#include <lcdf/clp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#define QUIET_OPT               300
#define HELP_OPT                301
#define VERSION_OPT             302
#define IGNORE_REDUNDANCY_OPT   303
#define REDUNDANCY_OPT          304
#define IGNORE_BACKGROUND_OPT   305
#define BACKGROUND_OPT          306

const Clp_Option options[] = {
  { "help", 'h', HELP_OPT, 0, 0 },
  { "brief", 'q', QUIET_OPT, 0, Clp_Negate },
  { "redudancy", 0, REDUNDANCY_OPT, 0, Clp_Negate },
  { "ignore-redundancy", 'w', IGNORE_REDUNDANCY_OPT, 0, Clp_Negate },
  { "bg", 0, BACKGROUND_OPT, 0, Clp_Negate },
  { "ignore-bg", 0, IGNORE_BACKGROUND_OPT, 0, Clp_Negate },
  { "background", 0, BACKGROUND_OPT, 0, Clp_Negate },
  { "ignore-background", 'B', IGNORE_BACKGROUND_OPT, 0, Clp_Negate },
  { "version", 'v', VERSION_OPT, 0, 0 }
};

const char *program_name;

static const char *filename1;
static const char *filename2;

static unsigned screen_width, screen_height;
#define TRANSP (0)

static uint16_t *gdata[2];
static uint16_t *glast[2];
static uint16_t *scratch;
static uint16_t *line;

static int brief;
static int ignore_redundancy;
static int ignore_background;

static Clp_Parser* clp;


static void
combine_colormaps(Gif_Colormap *gfcm, Gif_Colormap *newcm)
{
  int i, gfcm_ncol = gfcm ? gfcm->ncol : 0;
  for (i = 0; i < gfcm_ncol; i++) {
    Gif_Color *c = &gfcm->col[i];
    c->pixel = Gif_AddColor(newcm, c, 1);
  }
}

static void
fill_area(uint16_t *data, int l, int t, int w, int h, uint16_t val)
{
  int x;
  data += screen_width * t + l;
  for (; h > 0; --h) {
    for (x = w; x > 0; --x)
      *data++ = val;
    data += screen_width - w;
  }
}

static void
copy_area(uint16_t *dst, const uint16_t *src, int l, int t, int w, int h)
{
  dst += screen_width * t + l;
  src += screen_width * t + l;
  for (; h > 0; --h, dst += screen_width, src += screen_width)
    memcpy(dst, src, sizeof(uint16_t) * w);
}

static void
expand_bounds(int *lf, int *tp, int *rt, int *bt, const Gif_Image *gfi)
{
  int empty = (*lf >= *rt || *tp >= *bt);
  if (empty || gfi->left < *lf)
    *lf = gfi->left;
  if (empty || gfi->top < *tp)
    *tp = gfi->top;
  if (empty || gfi->left + gfi->width > *rt)
    *rt = gfi->left + gfi->width;
  if (empty || gfi->top + gfi->height > *bt)
    *bt = gfi->top + gfi->height;
}


static int
apply_image(int is_second, Gif_Stream *gfs, int imageno, uint16_t background)
{
  int i, x, y, any_change;
  Gif_Image *gfi = gfs->images[imageno];
  Gif_Image *pgfi = imageno ? gfs->images[imageno - 1] : 0;
  int width = gfi->width;
  uint16_t map[256];
  uint16_t *data = gdata[is_second];
  uint16_t *last = glast[is_second];
  Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
  int gfcm_ncol = gfcm ? gfcm->ncol : 0;

  /* set up colormap */
  for (i = 0; i < gfcm_ncol; ++i)
    map[i] = gfcm->col[i].pixel;
  for (i = gfcm_ncol; i < 256; ++i)
    map[i] = 1;
  if (gfi->transparent >= 0 && gfi->transparent < 256)
    map[gfi->transparent] = TRANSP;

  /* if this image's disposal is 'previous', save the post-disposal version in
     'scratch' */
  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
    copy_area(scratch, data, gfi->left, gfi->top, gfi->width, gfi->height);
    if (pgfi && pgfi->disposal == GIF_DISPOSAL_PREVIOUS)
      copy_area(scratch, last, pgfi->left, pgfi->top, pgfi->width, pgfi->height);
    else if (pgfi && pgfi->disposal == GIF_DISPOSAL_BACKGROUND)
      fill_area(scratch, pgfi->left, pgfi->top, pgfi->width, pgfi->height, background);
  }

  /* uncompress and clip */
  Gif_UncompressImage(gfs, gfi);
  Gif_ClipImage(gfi, 0, 0, screen_width, screen_height);

  any_change = imageno == 0;
  {
    int lf = 0, tp = 0, rt = 0, bt = 0;
    expand_bounds(&lf, &tp, &rt, &bt, gfi);
    if (pgfi && pgfi->disposal == GIF_DISPOSAL_PREVIOUS)
      expand_bounds(&lf, &tp, &rt, &bt, pgfi);
    else if (pgfi && pgfi->disposal == GIF_DISPOSAL_BACKGROUND) {
      expand_bounds(&lf, &tp, &rt, &bt, pgfi);
      fill_area(last, pgfi->left, pgfi->top, pgfi->width, pgfi->height, background);
    } else
      pgfi = 0;
    for (y = tp; y < bt; ++y) {
      uint16_t *outd = data + screen_width * y + lf;
      if (!any_change)
        memcpy(line, outd, (rt - lf) * sizeof(uint16_t));
      if (pgfi && y >= pgfi->top && y < pgfi->top + pgfi->height)
        memcpy(outd + pgfi->left - lf,
               last + screen_width * y + pgfi->left,
               pgfi->width * sizeof(uint16_t));
      if (y >= gfi->top && y < gfi->top + gfi->height) {
        uint16_t *xoutd = outd + gfi->left - lf;
        const uint8_t *ind = gfi->img[y - gfi->top];
        for (x = 0; x < width; ++x, ++ind, ++xoutd)
          if (map[*ind] != TRANSP)
            *xoutd = map[*ind];
      }
      if (!any_change && memcmp(line, outd, (rt - lf) * sizeof(uint16_t)) != 0)
        any_change = 1;
    }
  }

  Gif_ReleaseUncompressedImage(gfi);
  Gif_ReleaseCompressedImage(gfi);

  /* switch 'glast' with 'scratch' if necessary */
  if (gfi->disposal == GIF_DISPOSAL_PREVIOUS) {
    uint16_t *x = scratch;
    scratch = glast[is_second];
    glast[is_second] = x;
  }

  return any_change;
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
    vfprintf(stdout, format, val);
    fputc('\n', stdout);
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
    sprintf(buf, "#%02X%02X%02X", c->gfc_red, c->gfc_green, c->gfc_blue);
  }
}


int
compare(Gif_Stream *s1, Gif_Stream *s2)
{
  Gif_Colormap *newcm;
  int imageno1, imageno2, background1, background2;
  char buf1[256], buf2[256], fbuf[256];

  was_different = 0;

  /* Compare image counts and screen sizes. If either of these differs, quit
     early. */
  Gif_CalculateScreenSize(s1, 0);
  Gif_CalculateScreenSize(s2, 0);

  if (s1->screen_width != s2->screen_width
      || s1->screen_height != s2->screen_height) {
    different("screen sizes differ: <%dx%d >%dx%d", s1->screen_width,
              s1->screen_height, s2->screen_width, s2->screen_height);
    return DIFFERENT;
  }

  if (s1->screen_width == 0 || s1->screen_height == 0
      || s2->screen_width == 0 || s2->screen_height == 0) {
    /* paranoia -- don't think this can happen */
    different("zero screen sizes");
    return DIFFERENT;
  }

  if (s1->nimages == 0 || s2->nimages == 0) {
    if (s1->nimages != s2->nimages) {
      different("frame counts differ: <#%d >#%d", s1->nimages, s2->nimages);
      return DIFFERENT;
    } else
      return SAME;
  }

  /* Create arrays for the image data */
  screen_width = s1->screen_width;
  screen_height = s1->screen_height;

  gdata[0] = Gif_NewArray(uint16_t, screen_width * screen_height);
  gdata[1] = Gif_NewArray(uint16_t, screen_width * screen_height);
  glast[0] = Gif_NewArray(uint16_t, screen_width * screen_height);
  glast[1] = Gif_NewArray(uint16_t, screen_width * screen_height);
  scratch = Gif_NewArray(uint16_t, screen_width * screen_height);
  line = Gif_NewArray(uint16_t, screen_width);

  /* Merge all distinct colors from the two images into one colormap, setting
     the 'pixel' slots in the images' colormaps to the corresponding values
     in the merged colormap. Don't forget transparency */
  newcm = Gif_NewFullColormap(1, 256);
  combine_colormaps(s1->global, newcm);
  combine_colormaps(s2->global, newcm);
  for (imageno1 = 0; imageno1 < s1->nimages; ++imageno1)
    combine_colormaps(s1->images[imageno1]->local, newcm);
  for (imageno2 = 0; imageno2 < s2->nimages; ++imageno2)
    combine_colormaps(s2->images[imageno2]->local, newcm);

  /* Choose the background values */
  background1 = background2 = TRANSP;
  if ((s1->nimages == 0 || s1->images[0]->transparent < 0)
      && s1->global && s1->background < s1->global->ncol)
      background1 = s1->global->col[ s1->background ].pixel;
  if ((s2->nimages == 0 || s2->images[0]->transparent < 0)
      && s2->global && s2->background < s2->global->ncol)
      background2 = s2->global->col[ s2->background ].pixel;

  /* Clear screens */
  fill_area(gdata[0], 0, 0, screen_width, screen_height, TRANSP);
  fill_area(gdata[1], 0, 0, screen_width, screen_height, TRANSP);

  /* Loopcounts differ? */
  if (s1->loopcount != s2->loopcount) {
    name_loopcount(s1->loopcount, buf1);
    name_loopcount(s2->loopcount, buf2);
    different("loop counts differ: <%s >%s", buf1, buf2);
  }

  /* Loop over frames, comparing image data and delays */
  apply_image(0, s1, 0, background1);
  apply_image(1, s2, 0, background2);
  imageno1 = imageno2 = 0;
  while (imageno1 != s1->nimages && imageno2 != s2->nimages) {
    int fi1 = imageno1, fi2 = imageno2,
      delay1 = s1->images[fi1]->delay, delay2 = s2->images[fi2]->delay;

    /* get message right */
    if (imageno1 == imageno2)
      sprintf(fbuf, "#%d", imageno1);
    else
      sprintf(fbuf, "<#%d >#%d", imageno1, imageno2);

    /* compare pixels */
    if (memcmp(gdata[0], gdata[1],
               screen_width * screen_height * sizeof(uint16_t)) != 0) {
      unsigned d, c = screen_width * screen_height;
      uint16_t *d1 = gdata[0], *d2 = gdata[1];
      for (d = 0; d < c; d++, d1++, d2++)
        if (*d1 != *d2) {
          name_color(*d1, newcm, buf1);
          name_color(*d2, newcm, buf2);
          different("frame %s pixels differ: %d,%d <%s >%s",
                    fbuf, d % screen_width, d / screen_width, buf1, buf2);
          break;
        }
    }

    /* compare background */
    if (!ignore_background && background1 != background2
        && (imageno1 == 0 || s1->images[imageno1 - 1]->disposal == GIF_DISPOSAL_BACKGROUND)
        && (imageno2 == 0 || s2->images[imageno2 - 1]->disposal == GIF_DISPOSAL_BACKGROUND)) {
        unsigned d, c = screen_width * screen_height;
        uint16_t *d1 = gdata[0], *d2 = gdata[1];
        for (d = 0; d < c; ++d, ++d1, ++d2)
            if (*d1 == TRANSP || *d2 == TRANSP) {
                name_color(background1, newcm, buf1);
                name_color(background2, newcm, buf2);
                different("frame %s background pixels differ: %d,%d <%s >%s",
                          fbuf, d % screen_width, d / screen_width, buf1, buf2);
                background1 = background2 = TRANSP;
                break;
            }
    }

    /* move to next images, skipping redundancy */
    for (++imageno1;
         imageno1 < s1->nimages && !apply_image(0, s1, imageno1, background1);
         ++imageno1)
      delay1 += s1->images[imageno1]->delay;
    for (++imageno2;
         imageno2 < s2->nimages && !apply_image(1, s2, imageno2, background2);
         ++imageno2)
      delay2 += s2->images[imageno2]->delay;

    if (!ignore_redundancy) {
      fi1 = (imageno1 - fi1) - (imageno2 - fi2);
      for (; fi1 > 0; --fi1)
        different("extra redundant frame: <#%d", imageno1 - fi1);
      for (; fi1 < 0; ++fi1)
        different("extra redundant frame: >#%d", imageno2 + fi1);
    }

    if (delay1 != delay2) {
      name_delay(delay1, buf1);
      name_delay(delay2, buf2);
      different("frame %s delays differ: <%s >%s", fbuf, buf1, buf2);
    }
  }

  if (imageno1 != s1->nimages || imageno2 != s2->nimages)
    different("frame counts differ: <#%d >#%d", s1->nimages, s2->nimages);

  /* That's it! */
  Gif_DeleteColormap(newcm);
  Gif_DeleteArray(gdata[0]);
  Gif_DeleteArray(gdata[1]);
  Gif_DeleteArray(glast[0]);
  Gif_DeleteArray(glast[1]);
  Gif_DeleteArray(scratch);
  Gif_DeleteArray(line);

  return was_different ? DIFFERENT : SAME;
}


void short_usage(void) {
    Clp_fprintf(clp, stderr, "Usage: %s [OPTION]... FILE1 FILE2\n\
Try %<%s --help%> for more information.\n",
                program_name, program_name);
}

void usage(void) {
    Clp_fprintf(clp, stdout, "\
%<Gifdiff%> compares two GIF files (either images or animations) for identical\n\
visual appearance. An animation and an optimized version of the same animation\n\
should compare as the same. Gifdiff exits with status 0 if the images are\n\
the same, 1 if they%,re different, and 2 if there was some error.\n\
\n\
Usage: %s [OPTION]... FILE1 FILE2\n\n", program_name);
    Clp_fprintf(clp, stdout, "\
Options:\n\
  -q, --brief                   Don%,t report detailed differences.\n\
  -w, --ignore-redundancy       Ignore differences in redundant frames.\n\
  -B, --ignore-background       Ignore differences in background colors.\n\
  -h, --help                    Print this message and exit.\n\
  -v, --version                 Print version number and exit.\n\
\n\
Report bugs to <ekohler@gmail.com>.\n");
}


void fatal_error(const char* format, ...) {
    char buf[BUFSIZ];
    int n = snprintf(buf, BUFSIZ, "%s: ", program_name);
    va_list val;
    va_start(val, format);
    Clp_vsnprintf(clp, buf + n, BUFSIZ - n, format, val);
    va_end(val);
    fputs(buf, stderr);
    exit(2);                    /* exit(2) for trouble */
}

void error(const char* format, ...) {
    char buf[BUFSIZ];
    int n = snprintf(buf, BUFSIZ, "%s: ", program_name);
    va_list val;
    va_start(val, format);
    Clp_vsnprintf(clp, buf + n, BUFSIZ - n, format, val);
    va_end(val);
    fputs(buf, stderr);
}

static int gifread_error_count;

static void
gifread_error(Gif_Stream* gfs, Gif_Image* gfi,
              int is_error, const char *message)
{
  static int last_is_error = 0;
  static int last_which_image = 0;
  static char last_message[256];
  static int different_error_count = 0;
  static int same_error_count = 0;
  int which_image = Gif_ImageNumber(gfs, gfi);
  const char *filename = gfs->landmark;
  if (which_image < 0)
    which_image = gfs->nimages;

  if (gifread_error_count == 0) {
    last_which_image = -1;
    last_message[0] = 0;
    different_error_count = 0;
  }

  gifread_error_count++;
  if (last_message[0] && different_error_count <= 10
      && (last_which_image != which_image || message == 0
          || strcmp(message, last_message) != 0)) {
    const char *etype = last_is_error ? "error" : "warning";
    error("While reading %<%s%> frame #%d:\n", filename, last_which_image);
    if (same_error_count == 1)
      error("  %s: %s\n", etype, last_message);
    else if (same_error_count > 0)
      error("  %s: %s (%d times)\n", etype, last_message, same_error_count);
    same_error_count = 0;
    last_message[0] = 0;
  }

  if (message) {
    if (last_message[0] == 0)
      different_error_count++;
    same_error_count++;
    strcpy(last_message, message);
    last_which_image = which_image;
    last_is_error = is_error;
  } else
    last_message[0] = 0;

  if (different_error_count == 11 && message) {
    error("(more errors while reading %<%s%>)\n", filename);
    different_error_count++;
  }
}

static Gif_Stream *
read_stream(const char **filename)
{
    FILE *f;
    Gif_Stream *gfs;
    if (*filename == 0) {
#if 0
        /* Since gifdiff always takes explicit filename arguments,
           allow explicit reads from terminal. */
#ifndef OUTPUT_GIF_TO_TERMINAL
        if (isatty(fileno(stdin))) {
            fatal_error("<stdin>: is a terminal\n");
            return NULL;
        }
#endif
#endif
        f = stdin;
#if defined(_MSDOS) || defined(_WIN32)
        _setmode(_fileno(stdin), _O_BINARY);
#elif defined(__DJGPP__)
        setmode(fileno(stdin), O_BINARY);
#elif defined(__EMX__)
        _fsetmode(stdin, "b");
#endif
        *filename = "<stdin>";
    } else {
        f = fopen(*filename, "rb");
        if (!f)
            fatal_error("%s: %s\n", *filename, strerror(errno));
    }
    gifread_error_count = 0;
    gfs = Gif_FullReadFile(f, GIF_READ_COMPRESSED, *filename, gifread_error);
    if (!gfs)
        fatal_error("%s: file not in GIF format\n", *filename);
    return gfs;
}

int
main(int argc, char *argv[])
{
  int how_many_inputs = 0;
  int status;
  const char **inputp;
  Gif_Stream *gfs1, *gfs2;

  clp = Clp_NewParser(argc, (const char * const *)argv,
                      sizeof(options) / sizeof(options[0]), options);

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
      printf("gifdiff (LCDF Gifsicle) %s\n", VERSION);
      printf("Copyright (C) 1998-2019 Eddie Kohler\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;

     case QUIET_OPT:
      brief = !clp->negated;
      break;

     case IGNORE_REDUNDANCY_OPT:
      ignore_redundancy = !clp->negated;
      break;

     case REDUNDANCY_OPT:
      ignore_redundancy = !!clp->negated;
      break;

     case IGNORE_BACKGROUND_OPT:
      ignore_background = !clp->negated;
      break;

     case BACKGROUND_OPT:
      ignore_background = !!clp->negated;
      break;

     case Clp_NotOption:
      if (how_many_inputs == 2) {
        error("too many file arguments\n");
        goto bad_option;
      }
      inputp = (how_many_inputs == 0 ? &filename1 : &filename2);
      how_many_inputs++;
      if (strcmp(clp->vstr, "-") == 0)
        *inputp = 0;
      else
        *inputp = clp->vstr;
      break;

     bad_option:
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
    fatal_error("need exactly 2 file arguments\n");
  if (filename1 == 0 && filename2 == 0)
    fatal_error("can%,t read both files from stdin\n");

  gfs1 = read_stream(&filename1);
  gfs2 = read_stream(&filename2);

  status = (compare(gfs1, gfs2) == DIFFERENT);
  if (status == 1 && brief)
    printf("GIF files %s and %s differ\n", filename1, filename2);

  Gif_DeleteStream(gfs1);
  Gif_DeleteStream(gfs2);
  return status;
}
