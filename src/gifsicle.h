/* gifsicle.h - Function declarations for gifsicle.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifsicle.

   Gifsicle is free software; you can copy, distribute, or alter it at will, as
   long as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include "gif.h"
#include "clp.h"
#ifdef __GNUC__
#define NORETURN __attribute__ ((noreturn))
#else
#define NORETURN
#endif


typedef struct Gt_Frameset Gt_Frameset;
typedef struct Gt_Crop Gt_Crop;
typedef struct Gt_ColorChange Gt_ColorChange;

typedef struct Gt_Frame {
  
  Gif_Stream *stream;
  Gif_Image *image;
  int use;
  
  char *name;
  int no_name;
  Gif_Comment *comment;
  int no_comments;
  
  Gif_Color transparent;
  int interlacing;
  int left;
  int top;
  
  Gt_Crop *crop;
  
  int delay;
  int disposal;
  
  Gt_Frameset *nest;
  char *output_name;
  int explode_by_name;
  
  int loopcount;
  int screen_width;
  int screen_height;
  
} Gt_Frame;


struct Gt_Frameset {

  int count;
  int cap;
  Gt_Frame *f;

};


struct Gt_Crop {
  
  int ready;
  int whole_stream;
  int spec_x;
  int spec_y;
  int spec_w;
  int spec_h;
  int x;
  int y;
  int w;
  int h;
  int left_off;
  int right_off;
  
};


struct Gt_ColorChange {
  
  struct Gt_ColorChange *next;
  Gif_Color old_color;
  Gif_Color new_color;

};


/*****
 * error & verbose
 **/
extern const char *program_name;

void fatal_error(char *message, ...) NORETURN;
void warning(char *message, ...);
void error(char *message, ...);
void usage(void);
void short_usage(void);

void verbose_open(char, const char *);
void verbose_close(char);
void verbose_endline(void);

/*****
 * info &c
 **/

void stream_info(Gif_Stream *, char *, int colormaps);
void image_info(Gif_Stream *, Gif_Image *, int colormaps);

char *explode_filename(char *filename, int number, char *name);

/*****
 * merging images
 **/

void unmark_colors(Gif_Colormap *);

int find_image_color(Gif_Stream *gfs, Gif_Image *gfi, Gif_Color *color);

void merge_stream(Gif_Stream *dest, Gif_Stream *src, int no_comments);
void merge_comments(Gif_Comment *destc, Gif_Comment *srcc);
Gif_Image *merge_image(Gif_Stream *dest, Gif_Stream *src, Gif_Image *srci);

void optimize_fragments(Gif_Stream *, int opt_trans);

void crop_image(Gif_Image *, Gt_Crop *);
void apply_color_changes(Gif_Stream *, Gt_ColorChange *);

/*****
 * quantization
 **/

Gif_Color *histogram(Gif_Stream *, int *);

#define ADAPTIVE_ALG_DIVERSITY	0
#define ADAPTIVE_ALG_MEDIAN_CUT	1
Gif_Color *adaptive_palette_median_cut(Gif_Color *, int, int, int *);
Gif_Color *adaptive_palette_diversity(Gif_Color *, int, int, int *);

/*****
 * parsing stuff
 **/
extern int frame_spec_1;
extern int frame_spec_2;
extern char *frame_spec_name;
extern int dimensions_x;
extern int dimensions_y;
extern int position_x;
extern int position_y;
extern Gif_Color parsed_color;
extern Gif_Color parsed_color2;

int parse_frame_spec(Clp_Parser *, const char *, void *, int);
int parse_dimensions(Clp_Parser *, const char *, void *, int);
int parse_position(Clp_Parser *, const char *, void *, int);
int parse_color(Clp_Parser *, const char *, void *, int);
int parse_rectangle(Clp_Parser *, const char *, void *, int);
int parse_two_colors(Clp_Parser *, const char *, void *, int);

extern Gif_Stream *input;
extern char *input_name;

void input_stream(char *);
void input_done(void);
void output_frames(void);

/*****
 * stuff with frames
 **/
extern Gt_Frame def_frame;

Gt_Frameset *new_frameset(int initial_cap);
Gt_Frame *add_frame(Gt_Frameset *, int number, Gif_Stream *, Gif_Image *);
#define FRAME(fs, i) ((fs)->f[i])
Gif_Stream *merge_frame_interval(Gt_Frameset *fset, int f1, int f2);
void clear_frameset(Gt_Frameset *, int from);
