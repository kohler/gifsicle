/* gifsicle.c - gifsicle's main loop.
   Copyright (C) 1997-8 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software; you can copy, distribute, or alter it at will, as
   long as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include "gifsicle.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>


Gt_Frame def_frame;

Gt_Frameset *frames = 0;
int first_input_frame = 0;
Gt_Frameset *nested_frames = 0;

static int next_frame = 0;
static int next_input = 0;


Gif_Stream *input = 0;
char *input_name = 0;
static int unoptimizing = 0;
static int netscape_workaround = 0;

static int frames_done = 0;
static int files_given = 0;

static int optimizing = 0;
static Gt_ColorChange *color_changes = 0;

#define BLANK_MODE	0
#define MERGING		1
#define BATCHING	2
#define EXPLODING	3
#define DELETING	4
#define INSERTING	5
static int mode = BLANK_MODE;
static int nested_mode = 0;

static int infoing = 0;
static int colormap_infoing = 0;
static int outputing = 1;
static int verbosing = 0;

static int colormerging = 0;
static char *colormerge_name = 0;


#define SAME_INTERLACE_OPT	300
#define INFO_OPT		301
#define DISPOSAL_OPT		302
#define SAME_LOOPCOUNT_OPT	303
#define SAME_DISPOSAL_OPT	304
#define SAME_DELAY_OPT		305
#define SAME_TRANSPARENT_OPT	308
#define LOGICAL_SCREEN_OPT	309
#define COMMENT_OPT		310
#define UNOPTIMIZE_OPT		311
#define NETSCAPE_WORK_OPT	312
#define OPTIMIZE_OPT		313
#define SAME_LOGICAL_SCREEN_OPT 314
#define DELETE_OPT		315
#define REPLACE_OPT		316
#define INSERT_OPT		317
#define ALTER_DONE_OPT		318
#define APPEND_OPT		319
#define COLOR_INFO_OPT		320
#define VERBOSE_OPT		321
#define NO_COMMENTS_OPT		322
#define SAME_COMMENTS_OPT	323
#define NAME_OPT		324
#define SAME_NAME_OPT		325
#define NO_NAME_OPT		326
#define POSITION_OPT		327
#define SAME_POSITION_OPT	328
#define VERSION_OPT		329
#define HELP_OPT		330
#define OUTPUT_OPT		331
#define CROP_OPT		332
#define SAME_CROP_OPT		333
#define CHANGE_COLOR_OPT	334

#define LOOP_TYPE		(Clp_MaxDefaultType + 1)
#define DISPOSAL_TYPE		(Clp_MaxDefaultType + 2)
#define DIMENSIONS_TYPE		(Clp_MaxDefaultType + 3)
#define FRAME_SPEC_TYPE		(Clp_MaxDefaultType + 4)
#define COLOR_TYPE		(Clp_MaxDefaultType + 5)
#define POSITION_TYPE		(Clp_MaxDefaultType + 6)
#define RECTANGLE_TYPE		(Clp_MaxDefaultType + 7)
#define TWO_COLORS_TYPE		(Clp_MaxDefaultType + 8)

Clp_Option options[] = {
  { "append", 0, APPEND_OPT, 0, 0 },
  { "batch", 'b', 'b', 0, 0 },
  { "cc", 0, CHANGE_COLOR_OPT, TWO_COLORS_TYPE, Clp_Negate },
  { "change-color", 0, CHANGE_COLOR_OPT, TWO_COLORS_TYPE, Clp_Negate },
  { "cinfo", 0, COLOR_INFO_OPT, 0, Clp_Negate },
  { "clip", 0, CROP_OPT, RECTANGLE_TYPE, Clp_Negate },
  { "color-info", 0, COLOR_INFO_OPT, 0, Clp_Negate },
  { "comment", 'c', COMMENT_OPT, Clp_ArgString, Clp_Negate },
  { "no-comments", 0, NO_COMMENTS_OPT, 0, Clp_LongMinMatch, 6 }, /****/
  { "crop", 0, CROP_OPT, RECTANGLE_TYPE, Clp_Negate },
  { "delay", 'd', 'd', Clp_ArgInt, 0 },
  { "delete", 0, DELETE_OPT, 0, 0 },
  { "disposal", 'D', DISPOSAL_OPT, DISPOSAL_TYPE, 0 },
  { "done", 0, ALTER_DONE_OPT, 0, 0 },
  { "explode", 'e', 'e', 0, Clp_LongMinMatch, 1 }, /****/
  { "explode-by-name", 'E', 'E', 0, 0 },
  { "help", 'h', HELP_OPT, 0, 0 },
  { "info", 'I', INFO_OPT, 0, Clp_Negate },
  { "insert-before", 0, INSERT_OPT, FRAME_SPEC_TYPE, 0 },
  { "interlace", 'i', 'i', 0, Clp_Negate },
  { "logical-screen", 'S', LOGICAL_SCREEN_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "loopcount", 'l', 'l', LOOP_TYPE, Clp_Optional | Clp_Negate },
  { "merge", 'm', 'm', 0, 0 },
  { "name", 'n', NAME_OPT, Clp_ArgString, Clp_Negate },
  { "no-names", 0, NO_NAME_OPT, 0, Clp_LongMinMatch, 5 }, /****/
  { "netscape-workaround", 0, NETSCAPE_WORK_OPT, 0, Clp_Negate },
  { "optimize", 'O', OPTIMIZE_OPT, Clp_ArgInt, Clp_Negate | Clp_Optional },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "position", 'p', POSITION_OPT, POSITION_TYPE, Clp_Negate },
  { "replace", 0, REPLACE_OPT, FRAME_SPEC_TYPE, 0 },
  { "same-clip", 0, SAME_CROP_OPT, 0, 0 },
  { "same-comments", 0, SAME_COMMENTS_OPT, 0, 0 },
  { "same-crop", 0, SAME_CROP_OPT, 0, 0 },
  { "same-interlace", 0, SAME_INTERLACE_OPT, 0, 0 },
  { "same-logical-screen", 0, SAME_LOGICAL_SCREEN_OPT, 0, 0 },
  { "same-loopcount", 0, SAME_LOOPCOUNT_OPT, 0, 0 },
  { "same-disposal", 0, SAME_DISPOSAL_OPT, 0, 0 },
  { "same-delay", 0, SAME_DELAY_OPT, 0, 0 },
  { "same-names", 0, SAME_NAME_OPT, 0, 0 },
  { "same-position", 0, SAME_POSITION_OPT, 0, 0 },
  { "same-screen", 0, SAME_LOGICAL_SCREEN_OPT, 0, 0 },
  { "same-transparent", 0, SAME_TRANSPARENT_OPT, 0, 0 },
  { "screen", 0, LOGICAL_SCREEN_OPT, DIMENSIONS_TYPE, Clp_Negate },
  { "transparent", 't', 't', COLOR_TYPE, Clp_Negate },
  { "unoptimize", 'U', UNOPTIMIZE_OPT, 0, Clp_Negate },
  { "verbose", 'v', VERBOSE_OPT, 0, Clp_Negate },
  { "version", 0, VERSION_OPT, 0, 0 },
};


static void
initialize_def_frame(void)
{
  frames = new_frameset(16);
  
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

  def_frame.crop = 0;
  
  def_frame.delay = -1;
  def_frame.disposal = -1;
  
  def_frame.nest = 0;
  def_frame.output_name = 0;
  def_frame.explode_by_name = 0;
  
  def_frame.loopcount = -2;
  def_frame.screen_width = -1;
  def_frame.screen_height = -1;
}


static void
set_mode(int newmode)
{
  if (mode == BLANK_MODE)
    mode = newmode;
  else if (mode == newmode)
    ;
  else
    error("too late to change modes");
}


void
set_frame_change(int kind)
{
  int i;
  Gt_Frameset *fset;
  
  if (mode == BLANK_MODE)
    set_mode(MERGING);
  if (mode < DELETING && frames_done) {
    error("frame selection and frame changes don't mix");
    return;
  }
  assert(!nested_mode);
  nested_mode = mode;
  
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


void
frame_change_done(void)
{
  if (nested_mode)
    mode = nested_mode;
  if (nested_frames)
    frames = nested_frames;
  nested_mode = 0;
  nested_frames = 0;
}


void
show_frame(int imagenumber, int usename)
{
  Gif_Image *gfi;
  Gt_Frame *frame;
  
  if (!input) return;
  gfi = Gif_GetImage(input, imagenumber);
  if (!gfi) return;

  switch (mode) {
    
   case MERGING:
   case EXPLODING:
   case INSERTING:
    if (!frames_done)
      clear_frameset(frames, first_input_frame);
    frame = add_frame(frames, -1, input, gfi);
    if (usename) frame->explode_by_name = 1;
    break;
    
   case BATCHING:
    add_frame(frames, first_input_frame + imagenumber, input, gfi);
    break;
    
   case DELETING:
    frame = &FRAME(frames, first_input_frame + imagenumber);
    frame->use = 0;
    break;
    
  }
  
  next_frame = 0;
  frames_done = 1;
}


void
input_stream(char *name)
{
  FILE *f;
  Gif_Stream *gfs;
  int i;
  
  input = 0;
  input_name = name;
  frames_done = 0;
  next_frame = 0;
  next_input = 0;
  files_given++;
  
  if (name && strcmp(name, "-") == 0)
    name = 0;
  if (!name)
    f = stdin;
  else
    f = fopen(name, "rb");
  if (!f) {
    error("can't open `%s'", name);
    return;
  }
  
  name = name ? name : "<stdin>";
  if (verbosing) verbose_open('<', name);
  gfs = Gif_FullReadFile(f, netscape_workaround);
  fclose(f);
  
  if (!gfs || Gif_ImageCount(gfs) == 0) {
    error("`%s' doesn't seem to contain a GIF", name);
    Gif_DeleteStream(gfs);
    if (verbosing) verbose_close('>');
    return;
  }
  
  input = gfs;
  gfs->userflags = 97; /* to indicate no output done */
  
  if (gfs->errors)
    warning("there were errors reading `%s'", name);
  if (gfs->odd_extensions)
    warning("ignored extensions in `%s'", name);
  if (gfs->odd_app_extensions)
    warning("ignored application extensions in `%s'", name);
  
  /* Processing when we've got a new input frame */
  if (mode == BLANK_MODE)
    set_mode(MERGING);
  
  if (def_frame.output_name == 0) {
    /* Don't override explicit output names.
       This code works 'cause output_name is reset to 0 after each output. */
    if (mode == BATCHING)
      def_frame.output_name = input_name;
    else if (mode == EXPLODING) {
      /* Use name instead of input_name: it's #stdin# if required. */
      char *slash = strrchr(name, '/');
      if (slash)
	def_frame.output_name = slash + 1;
      else
	def_frame.output_name = name;
    }
  }
  
  first_input_frame = frames->count;
  for (i = 0; i < gfs->nimages; i++)
    add_frame(frames, -1, gfs, gfs->images[i]);
  
  if (unoptimizing)
    Gif_Unoptimize(gfs);
  if (color_changes)
    apply_color_changes(gfs, color_changes);
  if (infoing)
    stream_info(gfs, name, colormap_infoing);
  gfs->refcount++;
}


void
input_done(void)
{
  if (!input) return;
  
  if (verbosing) verbose_close('>');
  if (infoing) {
    int i;
    if (input->userflags == 97)	/* no stream info produced yet */
      stream_info(input, input_name, colormap_infoing);
    for (i = first_input_frame; i < frames->count; i++)
      if (FRAME(frames, i).stream == input && FRAME(frames, i).use)
	image_info(input, FRAME(frames, i).image, colormap_infoing);
  }
  
  input->refcount--;
  input = 0;
  
  if (mode == DELETING)
    frame_change_done();
  if (mode == BATCHING || mode == EXPLODING)
    output_frames();
}


void
output_stream(char *output_name, Gif_Stream *gfs)
{
  FILE *f;
  
  if (output_name)
    f = fopen(output_name, "wb");
  else
    f = stdout;
  
  if (f) {
    Gif_WriteFile(gfs, f);
    fclose(f);
  } else
    error("couldn't open %s for writing", output_name);
}


static void
do_frames_output(char *outfile, int f1, int f2)
{
  Gif_Stream *out;
  assert(!nested_mode);
  if (verbosing) verbose_open('[', outfile ? outfile : "#stdout#");
  
  out = merge_frame_interval(frames, f1, f2);
  if (out) {
    if (optimizing > 0)
      optimize_fragments(out, optimizing > 1);
    output_stream(outfile, out);
    Gif_DeleteStream(out);
  }
  
  if (verbosing) verbose_close(']');
}


void
output_frames(void)
{
  /* Use the current output name, not the stored output name.
     This supports `gifsicle a.gif -o xxx'.
     It's not like any other option, but seems right: it fits the natural
     order -- input, then output. */
  char *outfile = def_frame.output_name;
  int i;
  
  if (outputing && frames->count > 0)
    switch (mode) {
      
     case MERGING:
     case BATCHING:
      do_frames_output(outfile, 0, -1);
      break;
      
     case EXPLODING:
      /* Use the current output name for consistency, even though that means we
	 can't explode different frames to different names. Not a big deal
	 anyway; they can always repeat the gif on the cmd line. */
      if (!outfile) /* Watch out! */
	outfile = "-";
      
      for (i = 0; i < frames->count; i++) {
	Gt_Frame *fr = &FRAME(frames, i);
	int imagenumber = Gif_ImageNumber(fr->stream, fr->image);
	char *explodename;
	
	char *imagename = 0;
	if (fr->explode_by_name)
	  imagename = fr->name ? fr->name : fr->image->identifier;
	
	explodename = explode_filename(outfile, imagenumber, imagename);
	do_frames_output(explodename, i, i);
      }
      break;
      
     case INSERTING:
      /* do nothing */
      break;
      
    }
  
  clear_frameset(frames, 0);
  def_frame.output_name = 0;
  
  /* cropping: make sure that each input image is cropped according to its own
     dimensions. */
  if (def_frame.crop)
    def_frame.crop->ready = 0;
}


void
frame_argument(Clp_Parser *clp, char *arg)
{
  if (parse_frame_spec(clp, arg, 0, 1) > 0) {
    int i;
    for (i = frame_spec_1; i <= frame_spec_2; i++)
      show_frame(i, frame_spec_name != 0);
  }
}


int
main(int argc, char **argv)
{
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  
  Clp_AddStringListType
    (clp, LOOP_TYPE, Clp_AllowNumbers,
     "infinite", 0, "forever", 0,
     0);
  Clp_AddStringListType
    (clp, DISPOSAL_TYPE, Clp_AllowNumbers,
     "none", 0,
     "asis", 1,
     "background", 2,
     "bg", 2,
     "previous", 3,
     0);
  Clp_AddType(clp, DIMENSIONS_TYPE, parse_dimensions, 0);
  Clp_AddType(clp, POSITION_TYPE, parse_position, 0);
  Clp_AddType(clp, FRAME_SPEC_TYPE, parse_frame_spec, 0);
  Clp_AddType(clp, COLOR_TYPE, parse_color, 0);
  Clp_AddType(clp, RECTANGLE_TYPE, parse_rectangle, 0);
  Clp_AddType(clp, TWO_COLORS_TYPE, parse_two_colors, 0);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  
  program_name = Clp_ProgramName(clp);
  initialize_def_frame();
  
  /* Yep, I'm an idiot.
     GIF dimensions are unsigned 16-bit integers. I assume that these
     numbers will fit in an `int'. This assertion tests that assumption.
     Really I should go through & change everything over, but it doesn't
     seem worth my time. */
  {
    u_int16_t m = 0xFFFFU;
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
      next_frame = 1;
      def_frame.explode_by_name = 0;
      break;
      
     case 'E':
      set_mode(EXPLODING);
      next_frame = 1;
      def_frame.explode_by_name = 1;
      break;
      
      /* INFORMATION OPTIONS */
      
     case INFO_OPT:
      if (clp->negated) {
	infoing = 0;
	outputing = 1;
      } else {
	infoing = 1;
	outputing = !outputing;
      }
      break;
      
     case COLOR_INFO_OPT:
      if (clp->negated)
	colormap_infoing = 0;
      else {
	colormap_infoing = 1;
	if (!infoing) {
	  infoing = 1;
	  outputing = 0;
	}
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
      
      /* FRAME OPTIONS */
      
     case NAME_OPT:
      if (clp->negated) goto no_names;
      next_frame = 1;
      def_frame.name = clp->arg;
      break;
      
     no_names:
     case NO_NAME_OPT:
      next_frame = 1;
      def_frame.no_name = 1;
      def_frame.name = 0;
      break;
      
     case SAME_NAME_OPT:
      next_frame = 1;
      def_frame.no_name = 0;
      def_frame.name = 0;
      break;
      
     case COMMENT_OPT:
      if (clp->negated) goto no_comments;
      next_frame = 1;
      if (!def_frame.comment) def_frame.comment = Gif_NewComment();
      Gif_AddComment(def_frame.comment, Gif_CopyString(clp->arg), -1);
      break;
      
     no_comments:
     case NO_COMMENTS_OPT:
      next_frame = 1;
      Gif_DeleteComment(def_frame.comment);
      def_frame.comment = 0;
      def_frame.no_comments = 1;
      break;
      
     case SAME_COMMENTS_OPT:
      def_frame.no_comments = 0;
      break;
      
     case 'i':
      next_frame = 1;
      def_frame.interlacing = clp->negated ? 0 : 1;
      break;
      
     case SAME_INTERLACE_OPT:
      next_frame = 1;
      def_frame.interlacing = -1;
      break;
      
     case POSITION_OPT:
      next_frame = 1;
      def_frame.left = clp->negated ? 0 : position_x;
      def_frame.top = clp->negated ? 0 : position_y;
      break;
      
     case SAME_POSITION_OPT:
      next_frame = 1;
      def_frame.left = -1;
      def_frame.top = -1;
      break;
      
     case 't':
      next_frame = 1;
      if (clp->negated)
	def_frame.transparent.haspixel = 255;
      else {
	def_frame.transparent = parsed_color;
	def_frame.transparent.haspixel =
	  parsed_color.haspixel ? 2 : 1;
      }
      break;
      
     case SAME_TRANSPARENT_OPT:
      next_frame = 1;
      def_frame.transparent.haspixel = 0;
      break;
      
     case CROP_OPT:
      if (clp->negated) goto no_crop;
      next_frame = 1;
      {
	Gt_Crop *crop = malloc(sizeof(Gt_Crop));
	/* Memory leak on crops, but this just is NOT a problem. */
	crop->ready = 0;
	crop->whole_stream = 0;
	crop->spec_x = position_x;
	crop->spec_y = position_y;
	crop->spec_w = dimensions_x;
	crop->spec_h = dimensions_y;
	def_frame.crop = crop;
      }
      break;
      
     no_crop:
     case SAME_CROP_OPT:
      next_frame = 1;
      def_frame.crop = 0;
      break;
      
     case CHANGE_COLOR_OPT:
      next_input = 1;
      if (clp->negated)
	color_changes = 0;
      else {
	Gt_ColorChange *cc = malloc(sizeof(Gt_ColorChange));
	/* Memory leak on color changes; again, this ain't a problem. */
	cc->old_color = parsed_color;
	cc->new_color = parsed_color2;
	cc->next = color_changes;
	color_changes = cc;
      }
      break;
      
      /* ANIMATION OPTIONS */
      
     case 'd':
      next_frame = 1;
      def_frame.delay = clp->val.i;
      break;
      
     case SAME_DELAY_OPT:
      def_frame.delay = -1;
      break;
      
     case DISPOSAL_OPT:
      if (clp->val.i < 0 || clp->val.i > 7)
	error("disposal must be between 0 and 7");
      else {
	next_frame = 1;
	def_frame.disposal = clp->val.i;
      }
      break;
      
     case SAME_DISPOSAL_OPT:
      def_frame.disposal = -1;
      break;
      
     case 'l':
      if (clp->negated)
	def_frame.loopcount = -1;
      else
	def_frame.loopcount = clp->hadarg ? clp->val.i : 0;
      break;
      
     case SAME_LOOPCOUNT_OPT:
      def_frame.loopcount = -2;
      break;
      
     case OPTIMIZE_OPT:
      if (clp->negated)
	optimizing = 0;
      else if (clp->hadarg)
	optimizing = clp->val.i;
      else
	optimizing = 1;
      break;
      
     case UNOPTIMIZE_OPT:
      next_input = 1;
      unoptimizing = clp->negated ? 0 : 1;
      break;
      
      /* STREAM OPTIONS */
      
     case LOGICAL_SCREEN_OPT:
      if (clp->negated)
	def_frame.screen_width = def_frame.screen_height = 0;
      else {
	def_frame.screen_width = dimensions_x;
	def_frame.screen_height = dimensions_y;
      }
      break;
      
     case SAME_LOGICAL_SCREEN_OPT:
      def_frame.screen_width = def_frame.screen_height = -1;
      break;
      
     case NETSCAPE_WORK_OPT:
      netscape_workaround = clp->negated ? 0 : 1;
      break;
      
     case OUTPUT_OPT:
      if (strcmp(clp->arg, "-") == 0)
	def_frame.output_name = 0;
      else
	def_frame.output_name = clp->arg;
      break;
      
      /* RANDOM OPTIONS */
      
     case VERSION_OPT:
      printf("Gifsicle version %s\n", VERSION);
      printf("Copyright (C) 1997 Eddie Kohler\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose. That's right: you're on your own!\n");
      exit(0);
      break;
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
      /* NONOPTIONS */
      
     case Clp_NotOption:
      if (clp->arg[0] == '#')
	frame_argument(clp, clp->arg);
      else {
	input_done();
	input_stream(clp->arg);
      }
      break;
      
     case Clp_Done:
      goto done;
      
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;
      
     default:
      break;
      
    }
  }
  
 done:
  
  if (!files_given)
    input_stream(0);
  else if (next_frame || next_input)
    warning("some options didn't affect anything");
  
  frame_change_done();
  input_done();
  if (mode == MERGING)
    output_frames();
  
  verbose_endline();
  return 0;
}
