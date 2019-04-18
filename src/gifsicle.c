/* -*- c-basic-offset: 2 -*- */
/* gifsicle.c - gifsicle's main loop.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include "kcolor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef static_assert
#define static_assert(x, msg) switch ((int) (x)) case 0: case !!((int) (x)):
#endif

Gt_Frame def_frame;

Gt_Frameset *frames = 0;
int first_input_frame = 0;
Gt_Frameset *nested_frames = 0;

Gif_Stream *input = 0;
const char *input_name = 0;
static int unoptimizing = 0;

const int GIFSICLE_DEFAULT_THREAD_COUNT = 8;
int thread_count = 0;

static int gif_read_flags = 0;
static int nextfile = 0;
Gif_CompressInfo gif_write_info;

static int frames_done = 0;
static int files_given = 0;

int warn_local_colormaps = 1;

static Gt_ColorTransform *input_transforms;
static Gt_ColorTransform *output_transforms;

int mode = BLANK_MODE;
int nested_mode = 0;

static int infoing = 0;
int verbosing = 0;
static int no_ignore_errors = 0;


#define CHANGED(next, flag)     (((next) & (1<<(flag))) != 0)
#define UNCHECKED_MARK_CH(where, what)                  \
  next_##where |= 1<<what;
#define MARK_CH(where, what)    do {                        \
    if (CHANGED(next_##where, what))                        \
      redundant_option_warning(where##_option_types[what]); \
    UNCHECKED_MARK_CH(where, what);                         \
  } while (0)

/* frame option types */
static int next_frame = 0;
#define CH_INTERLACE            0
#define CH_DISPOSAL             1
#define CH_DELAY                2
#define CH_TRANSPARENT          3
#define CH_COMMENT              4
#define CH_NAME                 5
#define CH_POSITION             6
#define CH_CROP                 7
#define CH_EXTENSION            8
#define CH_FLIP                 9
#define CH_ROTATE               10
static const char *frame_option_types[] = {
  "interlace", "disposal", "delay", "transparency",
  "comment", "name", "position", "crop",
  "extension", "flip", "rotation"
};

/* input option types */
static int next_input = 0;
#define CH_UNOPTIMIZE           0
#define CH_CHANGE_COLOR         1
static const char *input_option_types[] = {
  "unoptimization", "color change"
};

/* output option types */
static Gt_OutputData def_output_data;
Gt_OutputData active_output_data;
static int next_output = 0;
static int active_next_output = 0;
static int any_output_successful = 0;
#define CH_LOOPCOUNT            0
#define CH_LOGICAL_SCREEN       1
#define CH_OPTIMIZE             2
#define CH_OUTPUT               3
#define CH_COLORMAP             4
#define CH_DITHER               5
#define CH_USE_COLORMAP         6
#define CH_COLORMAP_METHOD      7
#define CH_BACKGROUND           8
#define CH_COLOR_TRANSFORM      9
#define CH_RESIZE               10
#define CH_MEMORY               11
#define CH_GAMMA                12
#define CH_RESIZE_METHOD        13
#define CH_SCALE_COLORS         14
static const char *output_option_types[] = {
  "loopcount", "logical screen", "optimization", "output file",
  "colormap size", "dither", "colormap", "colormap method",
  "background", "color transformation", "resize", "memory conservation",
  "gamma", "resize method", "resize colors"
};


#define SAME_INTERLACE_OPT      300
#define INFO_OPT                301
#define DISPOSAL_OPT            302
#define SAME_LOOPCOUNT_OPT      303
#define SAME_DISPOSAL_OPT       304
#define SAME_DELAY_OPT          305
#define SAME_TRANSPARENT_OPT    308
#define LOGICAL_SCREEN_OPT      309
#define COMMENT_OPT             310
#define UNOPTIMIZE_OPT          311
#define CAREFUL_OPT             312
#define OPTIMIZE_OPT            313
#define SAME_LOGICAL_SCREEN_OPT 314
#define DELETE_OPT              315
#define REPLACE_OPT             316
#define INSERT_OPT              317
#define ALTER_DONE_OPT          318
#define APPEND_OPT              319
#define COLOR_INFO_OPT          320
#define VERBOSE_OPT             321
#define NO_COMMENTS_OPT         322
#define SAME_COMMENTS_OPT       323
#define NAME_OPT                324
#define SAME_NAME_OPT           325
#define NO_NAME_OPT             326
#define POSITION_OPT            327
#define SAME_POSITION_OPT       328
#define VERSION_OPT             329
#define HELP_OPT                330
#define OUTPUT_OPT              331
#define CROP_OPT                332
#define SAME_CROP_OPT           333
#define CHANGE_COLOR_OPT        334
#define COLORMAP_OPT            335
#define COLORMAP_ALGORITHM_OPT  336
#define DITHER_OPT              337
#define USE_COLORMAP_OPT        338
#define NO_EXTENSIONS_OPT       339
#define SAME_EXTENSIONS_OPT     340
#define EXTENSION_INFO_OPT      341
#define BACKGROUND_OPT          342
#define SAME_BACKGROUND_OPT     343
#define FLIP_HORIZ_OPT          344
#define FLIP_VERT_OPT           345
#define NO_FLIP_OPT             346
#define ROTATE_90_OPT           347
#define ROTATE_180_OPT          348
#define ROTATE_270_OPT          349
#define NO_ROTATE_OPT           350
#define APP_EXTENSION_OPT       351
#define EXTENSION_OPT           352
#define COLOR_TRANSFORM_OPT     353
#define RESIZE_OPT              354
#define SCALE_OPT               355
#define NO_WARNINGS_OPT         356
#define WARNINGS_OPT            357
#define RESIZE_WIDTH_OPT        358
#define RESIZE_HEIGHT_OPT       359
#define CROP_TRANSPARENCY_OPT   360
#define CONSERVE_MEMORY_OPT     361
#define MULTIFILE_OPT           362
#define NEXTFILE_OPT            363
#define RESIZE_FIT_OPT          364
#define RESIZE_FIT_WIDTH_OPT    365
#define RESIZE_FIT_HEIGHT_OPT   366
#define SIZE_INFO_OPT           367
#define GAMMA_OPT               368
#define GRAY_OPT                369
#define RESIZE_METHOD_OPT       370
#define RESIZE_COLORS_OPT       371
#define NO_APP_EXTENSIONS_OPT   372
#define SAME_APP_EXTENSIONS_OPT 373
#define IGNORE_ERRORS_OPT       374
#define THREADS_OPT             375
#define RESIZE_GEOMETRY_OPT     376
#define RESIZE_TOUCH_OPT        377
#define RESIZE_TOUCH_WIDTH_OPT  378
#define RESIZE_TOUCH_HEIGHT_OPT 379
#define LOSSY_OPT               380

#define LOOP_TYPE               (Clp_ValFirstUser)
#define DISPOSAL_TYPE           (Clp_ValFirstUser + 1)
#define DIMENSIONS_TYPE         (Clp_ValFirstUser + 2)
#define FRAME_SPEC_TYPE         (Clp_ValFirstUser + 3)
#define COLOR_TYPE              (Clp_ValFirstUser + 4)
#define POSITION_TYPE           (Clp_ValFirstUser + 5)
#define RECTANGLE_TYPE          (Clp_ValFirstUser + 6)
#define TWO_COLORS_TYPE         (Clp_ValFirstUser + 7)
#define COLORMAP_ALG_TYPE       (Clp_ValFirstUser + 8)
#define SCALE_FACTOR_TYPE       (Clp_ValFirstUser + 9)
#define OPTIMIZE_TYPE           (Clp_ValFirstUser + 10)
#define RESIZE_METHOD_TYPE      (Clp_ValFirstUser + 11)

const Clp_Option options[] = {

  { "append", 0, APPEND_OPT, 0, 0 },
  { "app-extension", 'x', APP_EXTENSION_OPT, Clp_ValString, 0 },
  { "no-app-extensions", 0, NO_APP_EXTENSIONS_OPT, 0, 0 },

  { "background", 'B', BACKGROUND_OPT, COLOR_TYPE, Clp_Negate },
  { "batch", 'b', 'b', 0, 0 },
  { "bg", 0, BACKGROUND_OPT, COLOR_TYPE, Clp_Negate },

  { "careful", 0, CAREFUL_OPT, 0, Clp_Negate },
  { "change-color", 0, CHANGE_COLOR_OPT, TWO_COLORS_TYPE, Clp_Negate },
  { "cinfo", 0, COLOR_INFO_OPT, 0, Clp_Negate },
  { "clip", 0, CROP_OPT, RECTANGLE_TYPE, Clp_Negate },
  { "colors", 'k', COLORMAP_OPT, Clp_ValInt, Clp_Negate },
  { "color-method", 0, COLORMAP_ALGORITHM_OPT, COLORMAP_ALG_TYPE, 0 },
  { "color-info", 0, COLOR_INFO_OPT, 0, Clp_Negate },
  { "comment", 'c', COMMENT_OPT, Clp_ValString, 0 },
  { "no-comments", 'c', NO_COMMENTS_OPT, 0, 0 },
  { "conserve-memory", 0, CONSERVE_MEMORY_OPT, 0, Clp_Negate },
  { "crop", 0, CROP_OPT, RECTANGLE_TYPE, Clp_Negate },
  { "crop-transparency", 0, CROP_TRANSPARENCY_OPT, 0, Clp_Negate },

  { "delay", 'd', 'd', Clp_ValInt, Clp_Negate },
  { "delete", 0, DELETE_OPT, 0, 0 },
  { "disposal", 'D', DISPOSAL_OPT, DISPOSAL_TYPE, Clp_Negate },
  { 0, 'f', DITHER_OPT, 0, Clp_Negate },
  { "dither", 0, DITHER_OPT, Clp_ValString, Clp_Negate | Clp_Optional },
  { "done", 0, ALTER_DONE_OPT, 0, 0 },

  { "explode", 'e', 'e', 0, 0 },
  { "explode-by-name", 'E', 'E', 0, 0 },
  { "extension", 0, EXTENSION_OPT, Clp_ValString, 0 },
  { "no-extension", 0, NO_EXTENSIONS_OPT, 0, 0 },
  { "no-extensions", 'x', NO_EXTENSIONS_OPT, 0, 0 },
  { "extension-info", 0, EXTENSION_INFO_OPT, 0, Clp_Negate },

  { "flip-horizontal", 0, FLIP_HORIZ_OPT, 0, Clp_Negate },
  { "flip-vertical", 0, FLIP_VERT_OPT, 0, Clp_Negate },
  { "no-flip", 0, NO_FLIP_OPT, 0, 0 },

  { "gamma", 0, GAMMA_OPT, Clp_ValString, Clp_Negate },
  { "gray", 0, GRAY_OPT, 0, 0 },

  { "help", 'h', HELP_OPT, 0, 0 },

  { "ignore-errors", 0, IGNORE_ERRORS_OPT, 0, Clp_Negate },
  { "info", 'I', INFO_OPT, 0, Clp_Negate },
  { "insert-before", 0, INSERT_OPT, FRAME_SPEC_TYPE, 0 },
  { "interlace", 'i', 'i', 0, Clp_Negate },

  { "logical-screen", 'S', LOGICAL_SCREEN_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "loopcount", 'l', 'l', LOOP_TYPE, Clp_Optional | Clp_Negate },
  { "lossy", 0, LOSSY_OPT, Clp_ValInt, Clp_Optional },

  { "merge", 'm', 'm', 0, 0 },
  { "method", 0, COLORMAP_ALGORITHM_OPT, COLORMAP_ALG_TYPE, 0 },
  { "multifile", 0, MULTIFILE_OPT, 0, Clp_Negate },

  { "name", 'n', NAME_OPT, Clp_ValString, 0 },
  { "nextfile", 0, NEXTFILE_OPT, 0, Clp_Negate },
  { "no-names", 'n', NO_NAME_OPT, 0, 0 },

  { "optimize", 'O', OPTIMIZE_OPT, OPTIMIZE_TYPE, Clp_Negate | Clp_Optional },
  { "output", 'o', OUTPUT_OPT, Clp_ValStringNotOption, 0 },

  { "position", 'p', POSITION_OPT, POSITION_TYPE, Clp_Negate },

  { "replace", 0, REPLACE_OPT, FRAME_SPEC_TYPE, 0 },
  { "resize", 0, RESIZE_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-width", 0, RESIZE_WIDTH_OPT, Clp_ValUnsigned, Clp_Negate },
  { "resize-height", 0, RESIZE_HEIGHT_OPT, Clp_ValUnsigned, Clp_Negate },
  { "resiz", 0, RESIZE_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resi", 0, RESIZE_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "res", 0, RESIZE_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-fit", 0, RESIZE_FIT_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-fit-width", 0, RESIZE_FIT_WIDTH_OPT, Clp_ValUnsigned, Clp_Negate },
  { "resize-fit-height", 0, RESIZE_FIT_HEIGHT_OPT, Clp_ValUnsigned, Clp_Negate },
  { "resize-fi", 0, RESIZE_FIT_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-f", 0, RESIZE_FIT_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-touch", 0, RESIZE_TOUCH_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-touch-width", 0, RESIZE_TOUCH_WIDTH_OPT, Clp_ValUnsigned, Clp_Negate },
  { "resize-touch-height", 0, RESIZE_TOUCH_HEIGHT_OPT, Clp_ValUnsigned, Clp_Negate },
  { "resize-touc", 0, RESIZE_TOUCH_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-tou", 0, RESIZE_TOUCH_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-to", 0, RESIZE_TOUCH_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-t", 0, RESIZE_TOUCH_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "resize-geometry", 0, RESIZE_GEOMETRY_OPT, Clp_ValString, Clp_Negate },
  { "resize-method", 0, RESIZE_METHOD_OPT, RESIZE_METHOD_TYPE, 0 },
  { "resize-colors", 0, RESIZE_COLORS_OPT, Clp_ValInt, Clp_Negate },
  { "rotate-90", 0, ROTATE_90_OPT, 0, 0 },
  { "rotate-180", 0, ROTATE_180_OPT, 0, 0 },
  { "rotate-270", 0, ROTATE_270_OPT, 0, 0 },
  { "no-rotate", 0, NO_ROTATE_OPT, 0, 0 },

  { "same-app-extensions", 0, SAME_APP_EXTENSIONS_OPT, 0, 0 },
  { "same-background", 0, SAME_BACKGROUND_OPT, 0, 0 },
  { "same-bg", 0, SAME_BACKGROUND_OPT, 0, 0 },
  { "same-clip", 0, SAME_CROP_OPT, 0, 0 },
  { "same-comments", 0, SAME_COMMENTS_OPT, 0, 0 },
  { "same-crop", 0, SAME_CROP_OPT, 0, 0 },
  { "same-extensions", 0, SAME_EXTENSIONS_OPT, 0, 0 },
  { "same-interlace", 0, SAME_INTERLACE_OPT, 0, 0 },
  { "same-logical-screen", 0, SAME_LOGICAL_SCREEN_OPT, 0, 0 },
  { "same-loopcount", 0, SAME_LOOPCOUNT_OPT, 0, 0 },
  { "same-disposal", 0, SAME_DISPOSAL_OPT, 0, 0 },
  { "same-delay", 0, SAME_DELAY_OPT, 0, 0 },
  { "same-names", 0, SAME_NAME_OPT, 0, 0 },
  { "same-position", 0, SAME_POSITION_OPT, 0, 0 },
  { "same-screen", 0, SAME_LOGICAL_SCREEN_OPT, 0, 0 },
  { "same-transparent", 0, SAME_TRANSPARENT_OPT, 0, 0 },
  { "scale", 0, SCALE_OPT, SCALE_FACTOR_TYPE, Clp_Negate },
  { "scale-method", 0, RESIZE_METHOD_OPT, RESIZE_METHOD_TYPE, 0 },
  { "scale-colors", 0, RESIZE_COLORS_OPT, Clp_ValInt, Clp_Negate },
  { "screen", 0, LOGICAL_SCREEN_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "sinfo", 0, SIZE_INFO_OPT, 0, Clp_Negate },
  { "size-info", 0, SIZE_INFO_OPT, 0, Clp_Negate },

  { "transform-colormap", 0, COLOR_TRANSFORM_OPT, Clp_ValStringNotOption,
    Clp_Negate },
  { "transparent", 't', 't', COLOR_TYPE, Clp_Negate },

  { "unoptimize", 'U', UNOPTIMIZE_OPT, 0, Clp_Negate },
  { "use-colormap", 0, USE_COLORMAP_OPT, Clp_ValString, Clp_Negate },

  { "verbose", 'V', VERBOSE_OPT, 0, Clp_Negate },
  { 0, 'v', VERBOSE_OPT, 0, Clp_Negate },
  { "version", 0, VERSION_OPT, 0, 0 },

  { 0, 'w', NO_WARNINGS_OPT, 0, Clp_Negate },
  { "warnings", 0, WARNINGS_OPT, 0, Clp_Negate },

  { "xinfo", 0, EXTENSION_INFO_OPT, 0, Clp_Negate },
  { "threads", 'j', THREADS_OPT, Clp_ValUnsigned, Clp_Optional | Clp_Negate },

};

Clp_Parser* clp;


static void combine_output_options(void);
static void initialize_def_frame(void);
static void redundant_option_warning(const char *);

#if 0
void resize_check(void)
{
    int nw, nh;
    nw = nh = 256;
    resize_dimensions(&nw, &nh, 640, 480, 0);
    assert(nw == 640 && nh == 480);

    nw = nh = 256;
    resize_dimensions(&nw, &nh, 640, 480, GT_RESIZE_FIT);
    assert(nw == 480 && nh == 480);

    nw = nh = 512;
    resize_dimensions(&nw, &nh, 640, 480, GT_RESIZE_FIT);
    assert(nw == 480 && nh == 480);

    nw = nh = 256;
    resize_dimensions(&nw, &nh, 640, 480, GT_RESIZE_FIT | GT_RESIZE_FIT_DOWN);
    assert(nw == 256 && nh == 256);

    nw = nh = 512;
    resize_dimensions(&nw, &nh, 640, 480, GT_RESIZE_FIT | GT_RESIZE_FIT_DOWN);
    assert(nw == 480 && nh == 480);

    nw = nh = 256;
    resize_dimensions(&nw, &nh, 640, 480, GT_RESIZE_FIT | GT_RESIZE_FIT_UP);
    assert(nw == 480 && nh == 480);

    nw = nh = 512;
    resize_dimensions(&nw, &nh, 640, 480, GT_RESIZE_FIT | GT_RESIZE_FIT_UP);
    assert(nw == 512 && nh == 512);

    nw = nh = 256;
    resize_dimensions(&nw, &nh, 640, 480, GT_RESIZE_FIT | GT_RESIZE_FIT_UP | GT_RESIZE_MIN_DIMEN);
    assert(nw == 640 && nh == 640);

    fprintf(stderr, "got\n");
}
#endif


static void
set_mode(int newmode)
{
  if (mode == BLANK_MODE) {
    if (newmode != BLANK_MODE)
      mode = newmode;
    else if (infoing == 1)
      mode = INFOING;
    else
      mode = MERGING;
  }

  if (mode != INFOING && infoing == 1)
    fatal_error("%<--info%> suppresses normal output, can%,t use with an\n  output mode like %<--merge%> or %<--batch%>.\n  (Try %<-II%>, which doesn%,t suppress normal output.)");
  if (newmode != BLANK_MODE && newmode != mode)
    fatal_error("too late to change modes");
}


void
set_frame_change(int kind)
{
  int i;
  Gt_Frameset *fset;

  set_mode(BLANK_MODE);
  if (mode < DELETING && frames_done) {
    fatal_error("frame selection and frame changes don%,t mix");
    return;
  }
  assert(!nested_mode);
  nested_mode = mode;
  if (frame_spec_1 > frame_spec_2) {
    i = frame_spec_1;
    frame_spec_1 = frame_spec_2;
    frame_spec_2 = i;
  }

  switch (kind) {

   case DELETE_OPT:
    mode = DELETING;
    break;

   case REPLACE_OPT:
    for (i = frame_spec_1; i < frame_spec_2; i++)
      FRAME(frames, i).use = 0;
    /* We want to use the last frame's delay, but nothing else about it. */
    FRAME(frames, i).use = -1;
    /* FALLTHRU */

   case INSERT_OPT:
    /* Define a nested frameset (or use an existing one). */
    fset = FRAME(frames, frame_spec_2).nest;
    if (!fset) fset = new_frameset(8);
    FRAME(frames, frame_spec_2).nest = fset;

    /* Later: Merge frames at the end of the nested frameset. */
    mode = INSERTING;
    nested_frames = frames;
    frames = fset;
    break;

   case APPEND_OPT:
    /* Just merge frames at the end of this frameset. */
    mode = INSERTING;
    break;

  }
}

static void
frame_change_done(void)
{
  if (nested_mode)
    mode = nested_mode;
  if (nested_frames)
    frames = nested_frames;
  nested_mode = 0;
  nested_frames = 0;
}


static void
show_frame(int imagenumber, int usename)
{
  Gif_Image *gfi;
  Gt_Frame *frame;

  if (!input || !(gfi = Gif_GetImage(input, imagenumber)))
    return;

  switch (mode) {

   case MERGING:
   case INSERTING:
   case EXPLODING:
   case INFOING:
   case BATCHING:
    if (!frames_done)
      clear_frameset(frames, first_input_frame);
    frame = add_frame(frames, input, gfi);
    if (usename)
      frame->explode_by_name = 1;
    break;

   case DELETING:
    frame = &FRAME(frames, first_input_frame + imagenumber);
    frame->use = 0;
    break;

  }

  next_frame = 0;
  frames_done = 1;
}


/*****
 * input a stream
 **/

static void
gifread_error(Gif_Stream* gfs, Gif_Image* gfi,
              int is_error, const char* message)
{
  static int last_is_error = 0;
  static char last_landmark[256];
  static char last_message[256];
  static int different_error_count = 0;
  static int same_error_count = 0;
  char landmark[256];
  int which_image = Gif_ImageNumber(gfs, gfi);
  if (gfs && which_image < 0)
    which_image = gfs->nimages;

  /* ignore warnings if "no_warning" */
  if (no_warnings && is_error == 0)
    return;

  if (message) {
    const char *filename = gfs && gfs->landmark ? gfs->landmark : "<unknown>";
    if (gfi && (which_image != 0 || gfs->nimages != 1))
      snprintf(landmark, sizeof(landmark), "%s:#%d",
               filename, which_image < 0 ? gfs->nimages : which_image);
    else
      snprintf(landmark, sizeof(landmark), "%s", filename);
  }

  if (last_message[0]
      && different_error_count <= 10
      && (!message
          || strcmp(message, last_message) != 0
          || strcmp(landmark, last_landmark) != 0)) {
    const char* etype = last_is_error ? "read error: " : "";
    void (*f)(const char*, const char*, ...) = last_is_error ? lerror : lwarning;
    if (gfi && gfi->user_flags)
      /* error already reported */;
    else if (same_error_count == 1)
      f(last_landmark, "%s%s", etype, last_message);
    else if (same_error_count > 0)
      f(last_landmark, "%s%s (%d times)", etype, last_message, same_error_count);
    same_error_count = 0;
    last_message[0] = 0;
  }

  if (message) {
    if (last_message[0] == 0)
      different_error_count++;
    same_error_count++;
    strncpy(last_message, message, sizeof(last_message));
    last_message[sizeof(last_message) - 1] = 0;
    strncpy(last_landmark, landmark, sizeof(last_landmark));
    last_landmark[sizeof(last_landmark) - 1] = 0;
    last_is_error = is_error;
    if (different_error_count == 11) {
      if (!(gfi && gfi->user_flags))
        error(0, "(plus more errors; is this GIF corrupt?)");
      different_error_count++;
    }
  } else
    last_message[0] = 0;

  {
    unsigned long missing;
    if (message && sscanf(message, "missing %lu pixel", &missing) == 1
        && missing > 10000 && no_ignore_errors) {
      gifread_error(gfs, 0, -1, 0);
      lerror(landmark, "fatal error: too many missing pixels, giving up");
      exit(1);
    }
  }

  if (gfi && is_error < 0)
    gfi->user_flags |= 1;
}

struct StoredFile {
  FILE *f;
  struct StoredFile *next;
  char name[1];
};

static struct StoredFile *stored_files = 0;

static FILE *
open_giffile(const char *name)
{
  struct StoredFile *sf;
  FILE *f;

  if (name == 0 || strcmp(name, "-") == 0) {
#ifndef OUTPUT_GIF_TO_TERMINAL
    if (isatty(fileno(stdin))) {
      lerror("<stdin>", "Is a terminal");
      return NULL;
    }
#endif
#if defined(_MSDOS) || defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
#elif defined(__DJGPP__)
    setmode(fileno(stdin), O_BINARY);
#elif defined(__EMX__)
    _fsetmode(stdin, "b");
#endif
    return stdin;
  }

  if (nextfile)
    for (sf = stored_files; sf; sf = sf->next)
      if (strcmp(name, sf->name) == 0)
        return sf->f;

  f = fopen(name, "rb");

  if (f && nextfile) {
    sf = (struct StoredFile *) malloc(sizeof(struct StoredFile) + strlen(name));
    sf->f = f;
    sf->next = stored_files;
    stored_files = sf;
    strcpy(sf->name, name);
  } else if (!f)
    lerror(name, "%s", strerror(errno));

  return f;
}

static void
close_giffile(FILE *f, int final)
{
  struct StoredFile **sf_pprev, *sf;

  if (!final && nextfile) {
    int c = getc(f);
    if (c == EOF)
      final = 1;
    else
      ungetc(c, f);
  }

  for (sf_pprev = &stored_files; (sf = *sf_pprev); sf_pprev = &sf->next)
    if (sf->f == f) {
      if (final) {
        fclose(f);
        *sf_pprev = sf->next;
        free((void *) sf);
      }
      return;
    }

  if (f != stdin)
    fclose(f);
}

void
input_stream(const char *name)
{
  char* component_namebuf;
  FILE *f;
  Gif_Stream *gfs;
  int i;
  int saved_next_frame = next_frame;
  int componentno = 0;
  const char *main_name = 0;
  Gt_Frame old_def_frame;

  input = 0;
  input_name = name;
  frames_done = 0;
  next_frame = 0;
  next_input = 0;
  if (next_output)
    combine_output_options();
  files_given++;

  set_mode(BLANK_MODE);

  f = open_giffile(name);
  if (!f)
    return;
  if (f == stdin) {
    name = "<stdin>";
    input_name = 0;
  }
  main_name = name;

 retry_file:
  /* change filename for component files */
  componentno++;
  if (componentno > 1) {
    component_namebuf = (char*) malloc(strlen(main_name) + 10);
    sprintf(component_namebuf, "%s~%d", main_name, componentno);
    name = component_namebuf;
  } else
    component_namebuf = 0;

  /* check for empty file */
  i = getc(f);
  if (i == EOF) {
    if (!(gif_read_flags & GIF_READ_TRAILING_GARBAGE_OK))
      lerror(name, "empty file");
    else if (nextfile)
      lerror(name, "no more images in file");
    goto error;
  }
  ungetc(i, f);

  if (verbosing)
    verbose_open('<', name);

  /* read file */
  {
    int old_error_count = error_count;
    gfs = Gif_FullReadFile(f, gif_read_flags | GIF_READ_COMPRESSED,
                           name, gifread_error);
    if ((!gfs || (Gif_ImageCount(gfs) == 0 && gfs->errors > 0))
        && componentno != 1)
      lerror(name, "trailing garbage ignored");
    if (!no_ignore_errors)
      error_count = old_error_count;
  }

  if (!gfs || (Gif_ImageCount(gfs) == 0 && gfs->errors > 0)) {
    if (componentno == 1)
      lerror(name, "file not in GIF format");
    Gif_DeleteStream(gfs);
    if (verbosing)
      verbose_close('>');
    goto error;
  }

  /* special processing for components after the first */
  if (componentno > 1) {
    if (mode == BATCHING || mode == INSERTING)
      fatal_error("%s: %<--multifile%> is useful only in merge mode", name);
    input_done();
  }

  input = gfs;

  /* Processing when we've got a new input frame */
  set_mode(BLANK_MODE);

  if (active_output_data.output_name == 0) {
    /* Don't override explicit output names.
       This code works 'cause output_name is reset to 0 after each output. */
    if (mode == BATCHING)
      active_output_data.output_name = input_name;
    else if (mode == EXPLODING) {
      /* Explode into current directory. */
      const char *explode_name = (input_name ? input_name : "#stdin#");
      const char *slash = strrchr(explode_name, PATHNAME_SEPARATOR);
      if (slash)
        active_output_data.output_name = slash + 1;
      else
        active_output_data.output_name = explode_name;
    }
  }

  /* This code rather sucks. Here's the problem: Since we consider options
     strictly sequentially, one at a time, we can't tell the difference
     between these:

     --name=X g.gif             h.gif   // name on g.gif #0
     --name=X g.gif          #2 h.gif   // name on g.gif #2
              g.gif --name=X #2 h.gif   // name on g.gif #2
              g.gif --name=X    h.gif   // name on h.gif #0 !!!

     Here's the solution. Mark when we CHANGE an option. After processing
     an input GIF, mark all the options as 'unchanged' -- but leave the
     VALUES as is. Then when we read the next frame, CLEAR the unchanged
     options. So it's like so: (* means changed, . means not.)

     [-.] --name=X [X*] g.gif [X.] #2 [-.] h.gif   == name on g.gif #2
     [-.] g.gif [-.] --name=X [X*] #2 [-.] h.gif  == name on g.gif #2
     [-.] --name=X [X*] g.gif [X.|-.] h.gif  == name on g.gif #0
     [-.] g.gif [-.] --name=X [X*] h.gif  == name on h.gif #0 */

  /* Clear old options from the last input stream */
  if (!CHANGED(saved_next_frame, CH_NAME))
    def_frame.name = 0;
  if (!CHANGED(saved_next_frame, CH_COMMENT))
    def_frame.comment = 0;
  if (!CHANGED(saved_next_frame, CH_EXTENSION))
    def_frame.extensions = 0;
  def_frame.input_filename = input_name;

  old_def_frame = def_frame;
  first_input_frame = frames->count;
  def_frame.position_is_offset = 1;
  for (i = 0; i < gfs->nimages; i++)
    add_frame(frames, gfs, gfs->images[i]);
  def_frame = old_def_frame;

  if (unoptimizing)
    if (!Gif_FullUnoptimize(gfs, GIF_UNOPTIMIZE_SIMPLEST_DISPOSAL)) {
      static int context = 0;
      if (!context) {
        lwarning(name, "GIF too complex to unoptimize\n"
                 "  (The reason was local color tables or complex transparency.\n"
                 "  Try running the GIF through %<gifsicle --colors=255%> first.)");
        context = 1;
      } else
        lwarning(name, "GIF too complex to unoptimize");
    }

  apply_color_transforms(input_transforms, gfs);
  gfs->refcount++;

  /* Read more files. */
  free(component_namebuf);
  if ((gif_read_flags & GIF_READ_TRAILING_GARBAGE_OK) && !nextfile)
    goto retry_file;
  close_giffile(f, 0);
  return;

 error:
  free(component_namebuf);
  close_giffile(f, 1);
}

void
input_done(void)
{
  if (!input) return;

  if (verbosing) verbose_close('>');

  Gif_DeleteStream(input);
  input = 0;

  if (mode == DELETING)
    frame_change_done();
  if (mode == BATCHING || mode == EXPLODING)
    output_frames();
}


/*****
 * colormap stuff
 **/

static void
set_new_fixed_colormap(const char *name)
{
  int i;
  if (name && strcmp(name, "web") == 0) {
    Gif_Colormap *cm = Gif_NewFullColormap(216, 256);
    Gif_Color *col = cm->col;
    for (i = 0; i < 216; i++) {
      col[i].gfc_red = (i / 36) * 0x33;
      col[i].gfc_green = ((i / 6) % 6) * 0x33;
      col[i].gfc_blue = (i % 6) * 0x33;
    }
    def_output_data.colormap_fixed = cm;

  } else if (name && (strcmp(name, "gray") == 0
                      || strcmp(name, "grey") == 0)) {
    Gif_Colormap *cm = Gif_NewFullColormap(256, 256);
    Gif_Color *col = cm->col;
    for (i = 0; i < 256; i++)
      col[i].gfc_red = col[i].gfc_green = col[i].gfc_blue = i;
    def_output_data.colormap_fixed = cm;

  } else if (name && strcmp(name, "bw") == 0) {
    Gif_Colormap *cm = Gif_NewFullColormap(2, 256);
    cm->col[0].gfc_red = cm->col[0].gfc_green = cm->col[0].gfc_blue = 0;
    cm->col[1].gfc_red = cm->col[1].gfc_green = cm->col[1].gfc_blue = 255;
    def_output_data.colormap_fixed = cm;

  } else
    def_output_data.colormap_fixed = read_colormap_file(name, 0);
}

static void
do_colormap_change(Gif_Stream *gfs)
{
  if (active_output_data.colormap_fixed || active_output_data.colormap_size > 0)
    kc_set_gamma(active_output_data.colormap_gamma_type,
                 active_output_data.colormap_gamma);

  if (active_output_data.colormap_fixed)
    colormap_stream(gfs, active_output_data.colormap_fixed,
                    &active_output_data);

  if (active_output_data.colormap_size > 0) {
    kchist kch;
    Gif_Colormap* (*adapt_func)(kchist*, Gt_OutputData*);
    Gif_Colormap *new_cm;

    /* set up the histogram */
    {
      uint32_t ntransp;
      int i, any_locals = 0;
      for (i = 0; i < gfs->nimages; i++)
        if (gfs->images[i]->local)
          any_locals = 1;
      kchist_make(&kch, gfs, &ntransp);
      if (kch.n <= active_output_data.colormap_size
          && !any_locals
          && !active_output_data.colormap_fixed) {
        warning(1, "trivial adaptive palette (only %d colors in source)", kch.n);
        kchist_cleanup(&kch);
        return;
      }
      active_output_data.colormap_needs_transparency = ntransp > 0;
    }

    switch (active_output_data.colormap_algorithm) {
     case COLORMAP_DIVERSITY:
      adapt_func = &colormap_flat_diversity;
      break;
     case COLORMAP_BLEND_DIVERSITY:
      adapt_func = &colormap_blend_diversity;
      break;
     case COLORMAP_MEDIAN_CUT:
      adapt_func = &colormap_median_cut;
      break;
     default:
      fatal_error("can't happen");
    }

    new_cm = (*adapt_func)(&kch, &active_output_data);
    colormap_stream(gfs, new_cm, &active_output_data);

    Gif_DeleteColormap(new_cm);
    kchist_cleanup(&kch);
  }
}


/*****
 * output GIF images
 **/

static void
write_stream(const char *output_name, Gif_Stream *gfs)
{
  FILE *f;

  if (output_name)
    f = fopen(output_name, "wb");
  else {
#ifndef OUTPUT_GIF_TO_TERMINAL
    if (isatty(fileno(stdout))) {
      lerror("<stdout>", "Is a terminal: try `-o OUTPUTFILE`");
      return;
    }
#endif
#if defined(_MSDOS) || defined(_WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#elif defined(__DJGPP__)
    setmode(fileno(stdout), O_BINARY);
#elif defined(__EMX__)
    _fsetmode(stdout, "b");
#endif
    f = stdout;
    output_name = "<stdout>";
  }

  if (f) {
    Gif_FullWriteFile(gfs, &gif_write_info, f);
    fclose(f);
    any_output_successful = 1;
  } else
    lerror(output_name, "%s", strerror(errno));
}

static void
merge_and_write_frames(const char *outfile, int f1, int f2)
{
  Gif_Stream *out;
  int compress_immediately;
  int colormap_change;
  int huge_stream;
  assert(!nested_mode);
  if (verbosing)
    verbose_open('[', outfile ? outfile : "#stdout#");
  active_output_data.active_output_name = outfile;

  colormap_change = active_output_data.colormap_size > 0
    || active_output_data.colormap_fixed;
  warn_local_colormaps = !colormap_change;

  if (!(active_output_data.scaling
        || (active_output_data.optimizing & GT_OPT_MASK)
        || colormap_change))
    compress_immediately = 1;
  else
    compress_immediately = active_output_data.conserve_memory;

  out = merge_frame_interval(frames, f1, f2, &active_output_data,
                             compress_immediately, &huge_stream);

  if (out) {
    double w, h;
    if (active_output_data.scaling == GT_SCALING_SCALE) {
      w = active_output_data.scale_x * out->screen_width;
      h = active_output_data.scale_y * out->screen_height;
    } else {
      w = active_output_data.resize_width;
      h = active_output_data.resize_height;
    }
    if (active_output_data.scaling != GT_SCALING_NONE)
      resize_stream(out, w, h, active_output_data.resize_flags,
                    active_output_data.scale_method,
                    active_output_data.scale_colors);
    if (colormap_change)
      do_colormap_change(out);
    if (output_transforms)
      apply_color_transforms(output_transforms, out);
    if (active_output_data.optimizing & GT_OPT_MASK)
      optimize_fragments(out, active_output_data.optimizing, huge_stream);
    write_stream(outfile, out);
    Gif_DeleteStream(out);
  }

  if (verbosing)
    verbose_close(']');
  active_output_data.active_output_name = 0;
}

static void
output_information(const char *outfile)
{
  FILE *f;
  int i, j;
  Gt_Frame *fr;
  Gif_Stream *gfs;

  if (infoing == 2)
    f = stderr;
  else if (outfile == 0)
    f = stdout;
  else {
    f = fopen(outfile, "w");
    if (!f) {
      lerror(outfile, "%s", strerror(errno));
      return;
    }
  }

  for (i = 0; i < frames->count; i++)
    FRAME(frames, i).stream->user_flags = 97;

  for (i = 0; i < frames->count; i++)
    if (FRAME(frames, i).stream->user_flags == 97) {
      fr = &FRAME(frames, i);
      gfs = fr->stream;
      gfs->user_flags = 0;
      stream_info(f, gfs, fr->input_filename, fr->info_flags);
      for (j = i; j < frames->count; j++)
        if (FRAME(frames, j).stream == gfs) {
          fr = &FRAME(frames, j);
          image_info(f, gfs, fr->image, fr->info_flags);
        }
    }

  if (f != stderr && f != stdout)
    fclose(f);
}

void
output_frames(void)
{
  /* Use the current output name, not the stored output name.
     This supports 'gifsicle a.gif -o xxx'.
     It's not like any other option, but seems right: it fits the natural
     order -- input, then output. */
  int i;
  const char *outfile = active_output_data.output_name;
  active_output_data.output_name = 0;

  /* Output information only now. */
  if (infoing)
    output_information(outfile);

  if (infoing != 1 && frames->count > 0)
    switch (mode) {

     case MERGING:
     case BATCHING:
     case INFOING:
      merge_and_write_frames(outfile, 0, -1);
      break;

     case EXPLODING: {
       /* Use the current output name for consistency, even though that means
          we can't explode different frames to different names. Not a big deal
          anyway; they can always repeat the gif on the cmd line. */
       int max_nimages = 0;
       for (i = 0; i < frames->count; i++) {
         Gt_Frame *fr = &FRAME(frames, i);
         if (fr->stream->nimages > max_nimages)
           max_nimages = fr->stream->nimages;
       }

       if (!outfile) /* Watch out! */
         outfile = "-";

       for (i = 0; i < frames->count; i++) {
         Gt_Frame *fr = &FRAME(frames, i);
         int imagenumber = Gif_ImageNumber(fr->stream, fr->image);
         char *explodename;

         const char *imagename = 0;
         if (fr->explode_by_name)
           imagename = fr->name ? fr->name : fr->image->identifier;

         explodename = explode_filename(outfile, imagenumber, imagename,
                                        max_nimages);
         merge_and_write_frames(explodename, i, i);
       }
       break;
     }

     case INSERTING:
      /* do nothing */
      break;

    }

  active_next_output = 0;
  clear_frameset(frames, 0);

  /* cropping: clear the 'crop->ready' information, which depended on the last
     input image. */
  if (def_frame.crop)
    def_frame.crop->ready = 0;
}


/*****
 * parsing arguments
 **/

int
frame_argument(Clp_Parser *clp, const char *arg)
{
  /* Returns 0 iff you should try a file named 'arg'. */
  int val = parse_frame_spec(clp, arg, -1, 0);
  if (val == -97)
    return 0;
  else if (val > 0) {
    int i, delta = (frame_spec_1 <= frame_spec_2 ? 1 : -1);
    for (i = frame_spec_1; i != frame_spec_2 + delta; i += delta)
      show_frame(i, frame_spec_name != 0);
    if (next_output)
      combine_output_options();
    return 1;
  } else
    return 1;
}

static int
handle_extension(Clp_Parser *clp, int is_app)
{
  Gif_Extension *gfex;
  const char *extension_type = clp->vstr;
  const char *extension_body = Clp_Shift(clp, 1);
  if (!extension_body) {
    Clp_OptionError(clp, "%O requires two arguments");
    return 0;
  }

  UNCHECKED_MARK_CH(frame, CH_EXTENSION);
  if (is_app)
    gfex = Gif_NewExtension(255, extension_type, 11);
  else if (!isdigit(extension_type[0]) && extension_type[1] == 0)
    gfex = Gif_NewExtension(extension_type[0], 0, 0);
  else {
    long l = strtol(extension_type, (char **)&extension_type, 0);
    if (*extension_type != 0 || l < 0 || l >= 256)
      fatal_error("bad extension type: must be a number between 0 and 255");
    gfex = Gif_NewExtension(l, 0, 0);
  }

  gfex->data = (uint8_t *)extension_body;
  gfex->length = strlen(extension_body);
  gfex->next = def_frame.extensions;
  def_frame.extensions = gfex;

  return 1;
}


/*****
 * option processing
 **/

static void
initialize_def_frame(void)
{
  /* frame defaults */
  def_frame.stream = 0;
  def_frame.image = 0;
  def_frame.use = 1;

  def_frame.name = 0;
  def_frame.no_name = 0;
  def_frame.comment = 0;
  def_frame.no_comments = 0;

  def_frame.interlacing = -1;
  def_frame.transparent.haspixel = 0;
  def_frame.left = -1;
  def_frame.top = -1;
  def_frame.position_is_offset = 0;

  def_frame.crop = 0;

  def_frame.delay = -1;
  def_frame.disposal = -1;

  def_frame.nest = 0;
  def_frame.explode_by_name = 0;

  def_frame.no_extensions = 0;
  def_frame.no_app_extensions = 0;
  def_frame.extensions = 0;

  def_frame.flip_horizontal = 0;
  def_frame.flip_vertical = 0;
  def_frame.total_crop = 0;

  /* output defaults */
  def_output_data.output_name = 0;

  def_output_data.screen_width = -1;
  def_output_data.screen_height = -1;
  def_output_data.background.haspixel = 0;
  def_output_data.loopcount = -2;

  def_output_data.colormap_size = 0;
  def_output_data.colormap_fixed = 0;
  def_output_data.colormap_algorithm = COLORMAP_DIVERSITY;
  def_output_data.dither_type = dither_none;
  def_output_data.dither_name = "none";
  def_output_data.colormap_gamma_type = KC_GAMMA_SRGB;
  def_output_data.colormap_gamma = 2.2;

  def_output_data.optimizing = 0;
  def_output_data.scaling = GT_SCALING_NONE;
  def_output_data.scale_method = SCALE_METHOD_MIX;
  def_output_data.scale_colors = 0;

  def_output_data.conserve_memory = 0;

  active_output_data = def_output_data;
}

static void
combine_output_options(void)
{
  int recent = next_output;
  next_output = active_next_output;
#define COMBINE_ONE_OUTPUT_OPTION(value, field)         \
  if (CHANGED(recent, value)) {                         \
    MARK_CH(output, value);                             \
    active_output_data.field = def_output_data.field;   \
  }

  COMBINE_ONE_OUTPUT_OPTION(CH_OUTPUT, output_name);

  if (CHANGED(recent, CH_LOGICAL_SCREEN)) {
    MARK_CH(output, CH_LOGICAL_SCREEN);
    active_output_data.screen_width = def_output_data.screen_width;
    active_output_data.screen_height = def_output_data.screen_height;
  }
  COMBINE_ONE_OUTPUT_OPTION(CH_BACKGROUND, background);
  COMBINE_ONE_OUTPUT_OPTION(CH_LOOPCOUNT, loopcount);

  COMBINE_ONE_OUTPUT_OPTION(CH_OPTIMIZE, optimizing);
  COMBINE_ONE_OUTPUT_OPTION(CH_COLORMAP, colormap_size);
  COMBINE_ONE_OUTPUT_OPTION(CH_COLORMAP_METHOD, colormap_algorithm);
  if (CHANGED(recent, CH_USE_COLORMAP)) {
    MARK_CH(output, CH_USE_COLORMAP);
    if (def_output_data.colormap_fixed)
      def_output_data.colormap_fixed->refcount++;
    Gif_DeleteColormap(active_output_data.colormap_fixed);
    active_output_data.colormap_fixed = def_output_data.colormap_fixed;
  }
  if (CHANGED(recent, CH_DITHER)) {
    MARK_CH(output, CH_DITHER);
    active_output_data.dither_type = def_output_data.dither_type;
    active_output_data.dither_data = def_output_data.dither_data;
  }
  if (CHANGED(recent, CH_GAMMA)) {
    MARK_CH(output, CH_GAMMA);
    active_output_data.colormap_gamma_type = def_output_data.colormap_gamma_type;
    active_output_data.colormap_gamma = def_output_data.colormap_gamma;
  }

  if (CHANGED(recent, CH_RESIZE)) {
    MARK_CH(output, CH_RESIZE);
    active_output_data.scaling = def_output_data.scaling;
    active_output_data.resize_width = def_output_data.resize_width;
    active_output_data.resize_height = def_output_data.resize_height;
    active_output_data.resize_flags = def_output_data.resize_flags;
    active_output_data.scale_x = def_output_data.scale_x;
    active_output_data.scale_y = def_output_data.scale_y;
  }

  if (CHANGED(recent, CH_RESIZE_METHOD)) {
    MARK_CH(output, CH_RESIZE_METHOD);
    active_output_data.scale_method = def_output_data.scale_method;
  }

  if (CHANGED(recent, CH_SCALE_COLORS)) {
    MARK_CH(output, CH_SCALE_COLORS);
    active_output_data.scale_colors = def_output_data.scale_colors;
  }

  COMBINE_ONE_OUTPUT_OPTION(CH_MEMORY, conserve_memory);

  def_output_data.colormap_fixed = 0;
  def_output_data.output_name = 0;

  active_next_output |= next_output;
  next_output = 0;
}

static void
redundant_option_warning(const char* opttype)
{
  static int context = 0;
  if (!context) {
    warning(0, "redundant %s option\n"
            "  (The %s option was overridden by another %s option\n"
            "  before it had any effect.)", opttype, opttype, opttype);
    context = 1;
  } else
    warning(0, "redundant %s option", opttype);
}

static void
print_useless_options(const char *type_name, int value, const char *names[])
{
  int explanation_printed = 0;
  int i;
  if (!value) return;
  for (i = 0; i < 32; i++)
    if (CHANGED(value, i)) {
      if (!explanation_printed) {
        warning(0, "useless %s-related %s option\n"
                "  (It didn%,t affect any %s.)", names[i], type_name, type_name);
        explanation_printed = 1;
      } else
        warning(0, "useless %s-related %s option", names[i], type_name);
    }
}

static Gt_Crop *
copy_crop(Gt_Crop *oc)
{
  Gt_Crop *nc = Gif_New(Gt_Crop);
  /* Memory leak on crops, but this just is NOT a problem. */
  if (oc)
    *nc = *oc;
  else
    memset(nc, 0, sizeof(Gt_Crop));
  nc->ready = 0;
  return nc;
}

static void
parse_resize_geometry_opt(Gt_OutputData* odata, const char* str, Clp_Parser* clp)
{
    double x, y;
    int flags = GT_RESIZE_FIT, scale = 0;

    if (*str == '_' || *str == 'x') {
        x = 0;
        str += (*str == '_');
    } else if (isdigit((unsigned char) *str))
        x = strtol(str, (char**) &str, 10);
    else
        goto error;

    if (*str == 'x') {
        ++str;
        if (*str == '_' || !isdigit((unsigned char) *str)) {
            y = 0;
            str += (*str == '_');
        } else
            y = strtol(str, (char**) &str, 10);
    } else
        y = x;

    for (; *str != 0; ++str)
        if (*str == '%')
            scale = 1;
        else if (*str == '!')
            flags = 0;
        else if (*str == '^')
            flags |= GT_RESIZE_FIT | GT_RESIZE_MIN_DIMEN;
        else if (*str == '<')
            flags |= GT_RESIZE_FIT | GT_RESIZE_FIT_UP;
        else if (*str == '>')
            flags |= GT_RESIZE_FIT | GT_RESIZE_FIT_DOWN;
        else
            goto error;

    if (scale) {
        odata->scaling = GT_SCALING_SCALE;
        odata->scale_x = x / 100.0;
        odata->scale_y = y / 100.0;
    } else {
        odata->scaling = GT_SCALING_RESIZE;
        odata->resize_width = x;
        odata->resize_height = y;
    }
    odata->resize_flags = flags;
    return;

error:
    Clp_OptionError(clp, "argument to %O must be a valid geometry specification");
}



/*****
 * main
 **/

int
main(int argc, char *argv[])
{
  /* Check SIZEOF constants (useful for Windows). If these assertions fail,
     you've used the wrong Makefile. You should've used Makefile.w32 for
     32-bit Windows and Makefile.w64 for 64-bit Windows. */
  static_assert(sizeof(unsigned int) == SIZEOF_UNSIGNED_INT, "unsigned int has the wrong size.");
  static_assert(sizeof(unsigned long) == SIZEOF_UNSIGNED_LONG, "unsigned long has the wrong size.");
  static_assert(sizeof(void*) == SIZEOF_VOID_P, "void* has the wrong size.");

  clp = Clp_NewParser(argc, (const char * const *)argv, sizeof(options) / sizeof(options[0]), options);

  Clp_AddStringListType
    (clp, LOOP_TYPE, Clp_AllowNumbers,
     "infinite", 0, "forever", 0,
     (const char*) 0);
  Clp_AddStringListType
    (clp, DISPOSAL_TYPE, Clp_AllowNumbers,
     "none", GIF_DISPOSAL_NONE,
     "asis", GIF_DISPOSAL_ASIS,
     "background", GIF_DISPOSAL_BACKGROUND,
     "bg", GIF_DISPOSAL_BACKGROUND,
     "previous", GIF_DISPOSAL_PREVIOUS,
     (const char*) 0);
  Clp_AddStringListType
    (clp, COLORMAP_ALG_TYPE, 0,
     "diversity", COLORMAP_DIVERSITY,
     "blend-diversity", COLORMAP_BLEND_DIVERSITY,
     "median-cut", COLORMAP_MEDIAN_CUT,
     (const char*) 0);
  Clp_AddStringListType
    (clp, OPTIMIZE_TYPE, Clp_AllowNumbers,
     "keep-empty", GT_OPT_KEEPEMPTY + 1,
     "no-keep-empty", GT_OPT_KEEPEMPTY,
     "drop-empty", GT_OPT_KEEPEMPTY,
     "no-drop-empty", GT_OPT_KEEPEMPTY + 1,
     (const char*) 0);
  Clp_AddStringListType
    (clp, RESIZE_METHOD_TYPE, 0,
     "point", SCALE_METHOD_POINT,
     "sample", SCALE_METHOD_POINT,
     "mix", SCALE_METHOD_MIX,
     "box", SCALE_METHOD_BOX,
     "catrom", SCALE_METHOD_CATROM,
     "lanczos", SCALE_METHOD_LANCZOS3,
     "lanczos2", SCALE_METHOD_LANCZOS2,
     "lanczos3", SCALE_METHOD_LANCZOS3,
     "mitchell", SCALE_METHOD_MITCHELL,
     "fast", SCALE_METHOD_POINT,
     "good", SCALE_METHOD_MIX,
     (const char*) 0);
  Clp_AddType(clp, DIMENSIONS_TYPE, 0, parse_dimensions, 0);
  Clp_AddType(clp, POSITION_TYPE, 0, parse_position, 0);
  Clp_AddType(clp, SCALE_FACTOR_TYPE, 0, parse_scale_factor, 0);
  Clp_AddType(clp, FRAME_SPEC_TYPE, 0, parse_frame_spec, 0);
  Clp_AddType(clp, COLOR_TYPE, Clp_DisallowOptions, parse_color, 0);
  Clp_AddType(clp, RECTANGLE_TYPE, 0, parse_rectangle, 0);
  Clp_AddType(clp, TWO_COLORS_TYPE, Clp_DisallowOptions, parse_two_colors, 0);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  Clp_SetErrorHandler(clp, clp_error_handler);

  program_name = Clp_ProgramName(clp);

  frames = new_frameset(16);
  initialize_def_frame();
  Gif_InitCompressInfo(&gif_write_info);
  Gif_SetErrorHandler(gifread_error);

#if ENABLE_THREADS
  pthread_mutex_init(&kd3_sort_lock, 0);
#endif

  /* Yep, I'm an idiot.
     GIF dimensions are unsigned 16-bit integers. I assume that these
     numbers will fit in an 'int'. This assertion tests that assumption.
     Really I should go through & change everything over, but it doesn't
     seem worth my time. */
  {
    uint16_t m = 0xFFFFU;
    int i = m;
    assert(i > 0 && "configuration/lameness failure! bug the author!");
  }

  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {

      /* MODE OPTIONS */

     case 'b':
      set_mode(BATCHING);
      break;

     case 'm':
      set_mode(MERGING);
      break;

     case 'e':
      set_mode(EXPLODING);
      def_frame.explode_by_name = 0;
      break;

     case 'E':
      set_mode(EXPLODING);
      def_frame.explode_by_name = 1;
      break;

      /* INFORMATION OPTIONS */

     case INFO_OPT:
      if (clp->negated)
        infoing = 0;
      else
        /* switch between infoing == 1 (suppress regular output) and 2 (don't
           suppress) */
        infoing = (infoing == 1 ? 2 : 1);
      break;

     case COLOR_INFO_OPT:
      if (clp->negated)
        def_frame.info_flags &= ~INFO_COLORMAPS;
      else {
        def_frame.info_flags |= INFO_COLORMAPS;
        if (!infoing)
          infoing = 1;
      }
      break;

     case EXTENSION_INFO_OPT:
      if (clp->negated)
        def_frame.info_flags &= ~INFO_EXTENSIONS;
      else {
        def_frame.info_flags |= INFO_EXTENSIONS;
        if (!infoing)
          infoing = 1;
      }
      break;

     case SIZE_INFO_OPT:
      if (clp->negated)
        def_frame.info_flags &= ~INFO_SIZES;
      else {
        def_frame.info_flags |= INFO_SIZES;
        if (!infoing)
          infoing = 1;
      }
      break;

     case VERBOSE_OPT:
      verbosing = clp->negated ? 0 : 1;
      break;

      /* FRAME CHANGE OPTIONS */

     case DELETE_OPT:
     case REPLACE_OPT:
     case INSERT_OPT:
     case APPEND_OPT:
      frame_change_done();
      set_frame_change(opt);
      break;

     case ALTER_DONE_OPT:
      frame_change_done();
      break;

      /* IMAGE OPTIONS */

     case NAME_OPT:
      if (clp->negated) goto no_names;
      MARK_CH(frame, CH_NAME);
      def_frame.name = clp->vstr;
      break;

     no_names:
     case NO_NAME_OPT:
      MARK_CH(frame, CH_NAME);
      def_frame.no_name = 1;
      def_frame.name = 0;
      break;

     case SAME_NAME_OPT:
      def_frame.no_name = 0;
      def_frame.name = 0;
      break;

     case COMMENT_OPT:
      if (clp->negated) goto no_comments;
      MARK_CH(frame, CH_COMMENT);
      if (!def_frame.comment) def_frame.comment = Gif_NewComment();
      Gif_AddComment(def_frame.comment, clp->vstr, -1);
      break;

     no_comments:
     case NO_COMMENTS_OPT:
      Gif_DeleteComment(def_frame.comment);
      def_frame.comment = 0;
      def_frame.no_comments = 1;
      break;

     case SAME_COMMENTS_OPT:
      def_frame.no_comments = 0;
      break;

     case 'i':
      MARK_CH(frame, CH_INTERLACE);
      def_frame.interlacing = clp->negated ? 0 : 1;
      break;

     case SAME_INTERLACE_OPT:
      def_frame.interlacing = -1;
      break;

     case POSITION_OPT:
      MARK_CH(frame, CH_POSITION);
      def_frame.left = clp->negated ? 0 : position_x;
      def_frame.top = clp->negated ? 0 : position_y;
      break;

     case SAME_POSITION_OPT:
      def_frame.left = -1;
      def_frame.top = -1;
      break;

     case 't':
      MARK_CH(frame, CH_TRANSPARENT);
      if (clp->negated)
        def_frame.transparent.haspixel = 255;
      else {
        def_frame.transparent = parsed_color;
        def_frame.transparent.haspixel = parsed_color.haspixel ? 2 : 1;
      }
      break;

     case SAME_TRANSPARENT_OPT:
      def_frame.transparent.haspixel = 0;
      break;

     case BACKGROUND_OPT:
      MARK_CH(output, CH_BACKGROUND);
      if (clp->negated) {
        def_output_data.background.haspixel = 2;
        def_output_data.background.pixel = 0;
      } else {
        def_output_data.background = parsed_color;
        def_output_data.background.haspixel = parsed_color.haspixel ? 2 : 1;
      }
      break;

     case SAME_BACKGROUND_OPT:
      MARK_CH(output, CH_BACKGROUND);
      def_output_data.background.haspixel = 0;
      break;

     case LOGICAL_SCREEN_OPT:
      MARK_CH(output, CH_LOGICAL_SCREEN);
      if (clp->negated)
        def_output_data.screen_width = def_output_data.screen_height = 0;
      else {
        def_output_data.screen_width = dimensions_x;
        def_output_data.screen_height = dimensions_y;
      }
      break;

     case SAME_LOGICAL_SCREEN_OPT:
      MARK_CH(output, CH_LOGICAL_SCREEN);
      def_output_data.screen_width = def_output_data.screen_height = -1;
      break;

     case CROP_OPT:
      if (clp->negated) goto no_crop;
      MARK_CH(frame, CH_CROP);
      {
        Gt_Crop *crop = copy_crop(def_frame.crop);
        /* Memory leak on crops, but this just is NOT a problem. */
        crop->spec_x = position_x;
        crop->spec_y = position_y;
        crop->spec_w = dimensions_x;
        crop->spec_h = dimensions_y;
        def_frame.crop = crop;
      }
      break;

     no_crop:
     case SAME_CROP_OPT:
      def_frame.crop = 0;
      break;

     case CROP_TRANSPARENCY_OPT:
      if (clp->negated)
        goto no_crop_transparency;
      def_frame.crop = copy_crop(def_frame.crop);
      def_frame.crop->transparent_edges = 1;
      break;

     no_crop_transparency:
      if (def_frame.crop && def_frame.crop->transparent_edges) {
        def_frame.crop = copy_crop(def_frame.crop);
        def_frame.crop->transparent_edges = 0;
      }
      break;

      /* extensions options */

     case NO_EXTENSIONS_OPT:
      def_frame.no_extensions = 1;
      break;

    case NO_APP_EXTENSIONS_OPT:
      def_frame.no_app_extensions = 1;
      break;

     case SAME_EXTENSIONS_OPT:
      def_frame.no_extensions = 0;
      break;

    case SAME_APP_EXTENSIONS_OPT:
      def_frame.no_app_extensions = 0;
      break;

     case EXTENSION_OPT:
      if (!handle_extension(clp, 0))
        goto bad_option;
      break;

     case APP_EXTENSION_OPT:
      if (!handle_extension(clp, 1))
        goto bad_option;
      break;

      /* IMAGE DATA OPTIONS */

     case FLIP_HORIZ_OPT:
      MARK_CH(frame, CH_FLIP);
      def_frame.flip_horizontal = !clp->negated;
      break;

     case FLIP_VERT_OPT:
      MARK_CH(frame, CH_FLIP);
      def_frame.flip_vertical = !clp->negated;
      break;

     case NO_FLIP_OPT:
      def_frame.flip_horizontal = def_frame.flip_vertical = 0;
      break;

     case NO_ROTATE_OPT:
      def_frame.rotation = 0;
      break;

     case ROTATE_90_OPT:
      MARK_CH(frame, CH_ROTATE);
      def_frame.rotation = 1;
      break;

     case ROTATE_180_OPT:
      MARK_CH(frame, CH_ROTATE);
      def_frame.rotation = 2;
      break;

     case ROTATE_270_OPT:
      MARK_CH(frame, CH_ROTATE);
      def_frame.rotation = 3;
      break;

      /* ANIMATION OPTIONS */

     case 'd':
      MARK_CH(frame, CH_DELAY);
      def_frame.delay = clp->negated ? 0 : clp->val.i;
      break;

     case SAME_DELAY_OPT:
      def_frame.delay = -1;
      break;

     case DISPOSAL_OPT:
      MARK_CH(frame, CH_DISPOSAL);
      if (clp->negated)
        def_frame.disposal = GIF_DISPOSAL_NONE;
      else if (clp->val.i < 0 || clp->val.i > 7)
        error(0, "disposal must be between 0 and 7");
      else
        def_frame.disposal = clp->val.i;
      break;

     case SAME_DISPOSAL_OPT:
      def_frame.disposal = -1;
      break;

     case 'l':
      MARK_CH(output, CH_LOOPCOUNT);
      if (clp->negated)
        def_output_data.loopcount = -1;
      else
        def_output_data.loopcount = (clp->have_val ? clp->val.i : 0);
      break;

     case SAME_LOOPCOUNT_OPT:
      MARK_CH(output, CH_LOOPCOUNT);
      def_output_data.loopcount = -2;
      break;

    case OPTIMIZE_OPT: {
      int o;
      UNCHECKED_MARK_CH(output, CH_OPTIMIZE);
      if (clp->negated || (clp->have_val && clp->val.i < 0))
        o = 0;
      else
        o = (clp->have_val ? clp->val.i : 1);
      if (o > GT_OPT_MASK && (o & 1))
        def_output_data.optimizing |= o - 1;
      else if (o > GT_OPT_MASK)
        def_output_data.optimizing &= ~o;
      else
        def_output_data.optimizing = (def_output_data.optimizing & ~GT_OPT_MASK) | o;
      break;
    }

     case UNOPTIMIZE_OPT:
      UNCHECKED_MARK_CH(input, CH_UNOPTIMIZE);
      unoptimizing = clp->negated ? 0 : 1;
      break;

     case THREADS_OPT:
      if (clp->negated)
          thread_count = 0;
      else if (clp->have_val)
          thread_count = clp->val.i;
      else
          thread_count = GIFSICLE_DEFAULT_THREAD_COUNT;
      break;

      /* WHOLE-GIF OPTIONS */

     case CAREFUL_OPT: {
       if (clp->negated)
         gif_write_info.flags = 0;
       else
         gif_write_info.flags = GIF_WRITE_CAREFUL_MIN_CODE_SIZE | GIF_WRITE_EAGER_CLEAR;
       break;
     }

     case CHANGE_COLOR_OPT: {
       next_input |= 1 << CH_CHANGE_COLOR;
       if (clp->negated)
         input_transforms = delete_color_transforms
           (input_transforms, &color_change_transformer);
       else if (parsed_color2.haspixel)
         error(0, "COLOR2 must be in RGB format in %<--change-color COLOR1 COLOR2%>");
       else
         input_transforms = append_color_change
           (input_transforms, parsed_color, parsed_color2);
       break;
     }

    case COLOR_TRANSFORM_OPT:
      next_output |= 1 << CH_COLOR_TRANSFORM;
      if (clp->negated)
        output_transforms = delete_color_transforms
          (output_transforms, &pipe_color_transformer);
      else
        output_transforms = append_color_transform
          (output_transforms, &pipe_color_transformer, (void *)clp->vstr);
      break;

     case COLORMAP_OPT:
      MARK_CH(output, CH_COLORMAP);
      if (clp->negated)
        def_output_data.colormap_size = 0;
      else {
        def_output_data.colormap_size = clp->val.i;
        if (def_output_data.colormap_size < 2
            || def_output_data.colormap_size > 256) {
          Clp_OptionError(clp, "argument to %O must be between 2 and 256");
          def_output_data.colormap_size = 0;
        }
      }
      break;

     case GRAY_OPT:
      MARK_CH(output, CH_USE_COLORMAP);
      Gif_DeleteColormap(def_output_data.colormap_fixed);
      set_new_fixed_colormap("gray");
      break;

    case USE_COLORMAP_OPT:
      MARK_CH(output, CH_USE_COLORMAP);
      Gif_DeleteColormap(def_output_data.colormap_fixed);
      if (clp->negated)
        def_output_data.colormap_fixed = 0;
      else
        set_new_fixed_colormap(clp->vstr);
      break;

     case COLORMAP_ALGORITHM_OPT:
      MARK_CH(output, CH_COLORMAP_METHOD);
      def_output_data.colormap_algorithm = clp->val.i;
      break;

    case DITHER_OPT: {
      const char* name;
      if (clp->negated)
        name = "none";
      else if (!clp->have_val)
        name = "default";
      else
        name = clp->val.s;
      if (strcmp(name, "posterize") == 0)
        name = "none";
      if (strcmp(name, def_output_data.dither_name) != 0
          && (strcmp(name, "none") == 0
              || strcmp(def_output_data.dither_name, "default") != 0))
        MARK_CH(output, CH_DITHER);
      UNCHECKED_MARK_CH(output, CH_DITHER);
      if (set_dither_type(&def_output_data, name) < 0)
        Clp_OptionError(clp, "%<%s%> is not a valid dither", name);
      def_output_data.dither_name = name;
      break;
    }

    case GAMMA_OPT: {
#if HAVE_POW
      char* ends;
      MARK_CH(output, CH_GAMMA);
      if (clp->negated) {
        def_output_data.colormap_gamma_type = KC_GAMMA_NUMERIC;
        def_output_data.colormap_gamma = 1;
      } else if (strcmp(clp->val.s, "sRGB") == 0
                 || strcmp(clp->val.s, "srgb") == 0)
        def_output_data.colormap_gamma_type = KC_GAMMA_SRGB;
      else {
        double gamma = strtod(clp->val.s, &ends);
        if (*clp->val.s && !*ends && !isspace((unsigned char) *clp->val.s)) {
          def_output_data.colormap_gamma_type = KC_GAMMA_NUMERIC;
          def_output_data.colormap_gamma = gamma;
        } else
          Clp_OptionError(clp, "%O should be a number or %<srgb%>");
      }
#else
      Clp_OptionError(clp, "this version of Gifsicle does not support %O");
#endif
      break;
    }

    case RESIZE_OPT:
    case RESIZE_FIT_OPT:
    case RESIZE_TOUCH_OPT:
      MARK_CH(output, CH_RESIZE);
      if (clp->negated)
        def_output_data.scaling = GT_SCALING_NONE;
      else if (dimensions_x <= 0 && dimensions_y <= 0) {
        error(0, "one of W and H must be positive in %<%s WxH%>", Clp_CurOptionName(clp));
        def_output_data.scaling = GT_SCALING_NONE;
      } else {
        def_output_data.scaling = GT_SCALING_RESIZE;
        def_output_data.resize_width = dimensions_x;
        def_output_data.resize_height = dimensions_y;
        def_output_data.resize_flags = 0;
        if (opt != RESIZE_OPT)
            def_output_data.resize_flags |= GT_RESIZE_FIT;
        if (opt == RESIZE_FIT_OPT)
            def_output_data.resize_flags |= GT_RESIZE_FIT_DOWN;
      }
      break;

    case RESIZE_WIDTH_OPT:
    case RESIZE_HEIGHT_OPT:
    case RESIZE_FIT_WIDTH_OPT:
    case RESIZE_FIT_HEIGHT_OPT:
    case RESIZE_TOUCH_WIDTH_OPT:
    case RESIZE_TOUCH_HEIGHT_OPT:
      MARK_CH(output, CH_RESIZE);
      if (clp->negated)
        def_output_data.scaling = GT_SCALING_NONE;
      else if (clp->val.u == 0) {
        error(0, "%s argument must be positive", Clp_CurOptionName(clp));
        def_output_data.scaling = GT_SCALING_NONE;
      } else {
        unsigned dimen[2] = {0, 0};
        dimen[(opt == RESIZE_HEIGHT_OPT || opt == RESIZE_FIT_HEIGHT_OPT)] = clp->val.u;
        def_output_data.scaling = GT_SCALING_RESIZE;
        def_output_data.resize_width = dimen[0];
        def_output_data.resize_height = dimen[1];
        def_output_data.resize_flags = 0;
        if (opt != RESIZE_WIDTH_OPT && opt != RESIZE_HEIGHT_OPT)
            def_output_data.resize_flags |= GT_RESIZE_FIT;
        if (opt == RESIZE_FIT_WIDTH_OPT || opt == RESIZE_FIT_HEIGHT_OPT)
            def_output_data.resize_flags |= GT_RESIZE_FIT_DOWN;
      }
      break;

     case SCALE_OPT:
      MARK_CH(output, CH_RESIZE);
      if (clp->negated)
        def_output_data.scaling = GT_SCALING_NONE;
      else if (parsed_scale_factor_x <= 0 || parsed_scale_factor_y <= 0) {
        error(0, "%s X and Y factors must be positive", Clp_CurOptionName(clp));
        def_output_data.scaling = GT_SCALING_NONE;
      } else {
        def_output_data.scaling = GT_SCALING_SCALE;
        def_output_data.scale_x = parsed_scale_factor_x;
        def_output_data.scale_y = parsed_scale_factor_y;
        def_output_data.resize_flags = 0;
      }
      break;

    case RESIZE_GEOMETRY_OPT:
      MARK_CH(output, CH_RESIZE);
      if (clp->negated)
        def_output_data.scaling = GT_SCALING_NONE;
      else
        parse_resize_geometry_opt(&def_output_data, clp->val.s, clp);
      break;

    case RESIZE_METHOD_OPT:
      MARK_CH(output, CH_RESIZE_METHOD);
      def_output_data.scale_method = clp->val.i;
      break;

    case RESIZE_COLORS_OPT:
      MARK_CH(output, CH_SCALE_COLORS);
      def_output_data.scale_colors = clp->negated ? 0 : clp->val.i;
      if (def_output_data.scale_colors > 256) {
        error(0, "%s can be at most 256", Clp_CurOptionName(clp));
        def_output_data.scale_colors = 256;
      }
      break;

    case LOSSY_OPT:
      if (clp->have_val)
        gif_write_info.loss = clp->val.i;
      else
        gif_write_info.loss = 20;
      break;

      /* RANDOM OPTIONS */

     case NO_WARNINGS_OPT:
      no_warnings = !clp->negated;
      break;

     case WARNINGS_OPT:
      no_warnings = clp->negated;
      break;

     case IGNORE_ERRORS_OPT:
      no_ignore_errors = clp->negated;
      break;

     case CONSERVE_MEMORY_OPT:
      MARK_CH(output, CH_MEMORY);
      def_output_data.conserve_memory = clp->negated ? -1 : 1;
      break;

     case MULTIFILE_OPT:
      if (clp->negated)
        gif_read_flags &= ~GIF_READ_TRAILING_GARBAGE_OK;
      else {
        gif_read_flags |= GIF_READ_TRAILING_GARBAGE_OK;
        nextfile = 0;
      }
      break;

     case NEXTFILE_OPT:
      if (clp->negated)
        gif_read_flags &= ~GIF_READ_TRAILING_GARBAGE_OK;
      else {
        gif_read_flags |= GIF_READ_TRAILING_GARBAGE_OK;
        nextfile = 1;
      }
      break;

     case VERSION_OPT:
#ifdef GIF_UNGIF
      printf("LCDF Gifsicle %s (ungif)\n", VERSION);
#else
      printf("LCDF Gifsicle %s\n", VERSION);
#endif
      printf("Copyright (C) 1997-2019 Eddie Kohler\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(EXIT_OK);
      break;

     case HELP_OPT:
      usage();
      exit(EXIT_OK);
      break;

     case OUTPUT_OPT:
      MARK_CH(output, CH_OUTPUT);
      if (strcmp(clp->vstr, "-") == 0)
        def_output_data.output_name = 0;
      else
        def_output_data.output_name = clp->vstr;
      break;

      /* NONOPTIONS */

     case Clp_NotOption:
      if (clp->vstr[0] != '#' || !frame_argument(clp, clp->vstr)) {
        input_done();
        input_stream(clp->vstr);
      }
      break;

     case Clp_Done:
      goto done;

     bad_option:
     case Clp_BadOption:
      short_usage();
      exit(EXIT_USER_ERR);
      break;

     default:
      break;

    }
  }

 done:

  if (next_output)
    combine_output_options();
  if (!files_given)
    input_stream(0);

  frame_change_done();
  input_done();
  if ((mode == MERGING && !error_count) || mode == INFOING)
    output_frames();

  verbose_endline();
  print_useless_options("frame", next_frame, frame_option_types);
  print_useless_options("input", next_input, input_option_types);
  if (any_output_successful)
    print_useless_options("output", active_next_output, output_option_types);
  blank_frameset(frames, 0, 0, 1);
  Clp_DeleteParser(clp);
  return (error_count ? EXIT_ERR : EXIT_OK);
}
