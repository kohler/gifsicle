/* gifsicle.h - Function declarations for gifsicle.
   Copyright (C) 1997-2017 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#ifndef GIFSICLE_H
#define GIFSICLE_H
#include <lcdfgif/gif.h>
#include <lcdf/clp.h>
#ifdef __GNUC__
#define NORETURN __attribute__ ((noreturn))
#define USED_ATTR __attribute__ ((used))
#else
#define NORETURN
#define USED_ATTR
#endif

typedef struct Gt_Frameset Gt_Frameset;
typedef struct Gt_Crop Gt_Crop;
typedef struct Gt_ColorTransform Gt_ColorTransform;

#if ENABLE_THREADS
#include <pthread.h>
extern pthread_mutex_t kd3_sort_lock;
#endif

typedef struct Gt_Frame {

  Gif_Stream *stream;
  Gif_Image *image;
  int use;

  const char *name;
  int no_name;
  Gif_Comment *comment;
  int no_comments;

  Gif_Color transparent;        /* also background */
  int interlacing;
  int left;
  int top;

  Gt_Crop *crop;
  int left_offset;
  int top_offset;

  int delay;
  int disposal;

  Gt_Frameset *nest;
  int explode_by_name;

  int no_extensions;
  int no_app_extensions;
  Gif_Extension *extensions;

  unsigned flip_horizontal: 1;
  unsigned flip_vertical: 1;
  unsigned info_flags: 3;
  unsigned position_is_offset: 1;
  unsigned total_crop: 1;
  unsigned rotation;

  const char *input_filename;

} Gt_Frame;


struct Gt_Frameset {
  int count;
  int cap;
  Gt_Frame *f;
};


struct Gt_Crop {
  int ready;
  int transparent_edges;
  int spec_x;
  int spec_y;
  int spec_w;
  int spec_h;
  int x;
  int y;
  int w;
  int h;
  int left_offset;
  int top_offset;
};


typedef void (*colormap_transform_func)(Gif_Colormap *, void *);

struct Gt_ColorTransform {
  Gt_ColorTransform *prev;
  Gt_ColorTransform *next;
  colormap_transform_func func;
  void *data;
};


typedef struct {

  const char *output_name;
  const char *active_output_name;

  int screen_width;
  int screen_height;

  Gif_Color background;
  int loopcount;

  int colormap_size;
  Gif_Colormap *colormap_fixed;
  int colormap_algorithm;
  int colormap_needs_transparency;
  int dither_type;
  const uint8_t* dither_data;
  const char* dither_name;
  int colormap_gamma_type;
  double colormap_gamma;

  int optimizing;

  int scaling;
  int resize_width;
  int resize_height;
  int resize_flags;
  double scale_x;
  double scale_y;
  int scale_method;
  int scale_colors;

  int conserve_memory;

} Gt_OutputData;

extern Gt_OutputData active_output_data;
extern Clp_Parser* clp;

#define GT_SCALING_NONE         0
#define GT_SCALING_RESIZE       1
#define GT_SCALING_SCALE        2

#define GT_RESIZE_FIT           1
#define GT_RESIZE_FIT_DOWN      2
#define GT_RESIZE_FIT_UP        4
#define GT_RESIZE_MIN_DIMEN     8

#define SCALE_METHOD_POINT      0
#define SCALE_METHOD_BOX        1
#define SCALE_METHOD_MIX        2
#define SCALE_METHOD_CATROM     3
#define SCALE_METHOD_LANCZOS2   4
#define SCALE_METHOD_LANCZOS3   5
#define SCALE_METHOD_MITCHELL   6

#define GT_OPT_MASK             0xFFFF
#define GT_OPT_KEEPEMPTY        0x10000


/*****
 * helper
 **/

static inline int
constrain(int low, int x, int high)
{
  return x < low ? low : (x < high ? x : high);
}


/*****
 * error & verbose
 **/
extern const char *program_name;
extern int verbosing;
extern int error_count;
extern int no_warnings;
extern int thread_count;
extern Gif_CompressInfo gif_write_info;

void fatal_error(const char* format, ...) NORETURN;
void warning(int need_file, const char* format, ...);
void lwarning(const char* landmark, const char* format, ...);
void error(int need_file, const char* format, ...);
void lerror(const char* landmark, const char* format, ...);
void clp_error_handler(Clp_Parser *clp, const char *clp_message);
void usage(void);
void short_usage(void);

void verbose_open(char, const char *);
void verbose_close(char);
void verbose_endline(void);

const char* debug_color_str(const Gif_Color* gfc);

#define EXIT_OK         0
#define EXIT_ERR        1
#define EXIT_USER_ERR   1

/*****
 * info &c
 **/
#define INFO_COLORMAPS  1
#define INFO_EXTENSIONS 2
#define INFO_SIZES      4
void stream_info(FILE *f, Gif_Stream *gfs, const char *filename, int flags);
void image_info(FILE *f, Gif_Stream *gfs, Gif_Image *gfi, int flags);

char *explode_filename(const char *filename, int number,
                       const char *name, int max_nimg);

/*****
 * merging images
 **/
void    unmark_colors(Gif_Colormap *);
void    unmark_colors_2(Gif_Colormap *);
void    mark_used_colors(Gif_Stream *gfs, Gif_Image *gfi, Gt_Crop *crop,
                         int compress_immediately);
int     find_color_index(Gif_Color *c, int nc, Gif_Color *);
int     merge_colormap_if_possible(Gif_Colormap *, Gif_Colormap *);

extern int warn_local_colormaps;
void    merge_stream(Gif_Stream *dest, Gif_Stream *src, int no_comments);
void    merge_comments(Gif_Comment *destc, Gif_Comment *srcc);
Gif_Image* merge_image(Gif_Stream* dest, Gif_Stream* src, Gif_Image* srci,
                       Gt_Frame* srcfr, int same_compressed_ok);

void    optimize_fragments(Gif_Stream *, int optimizeness, int huge_stream);

/*****
 * image/colormap transformations
 **/
Gif_Colormap *read_colormap_file(const char *, FILE *);
void    apply_color_transforms(Gt_ColorTransform *, Gif_Stream *);

typedef void (*color_transform_func)(Gif_Colormap *, void *);
Gt_ColorTransform *append_color_transform
        (Gt_ColorTransform *list, color_transform_func, void *);
Gt_ColorTransform *delete_color_transforms
        (Gt_ColorTransform *list, color_transform_func);

void    color_change_transformer(Gif_Colormap *, void *);
Gt_ColorTransform *append_color_change
        (Gt_ColorTransform *list, Gif_Color, Gif_Color);

void    pipe_color_transformer(Gif_Colormap *, void *);

void    combine_crop(Gt_Crop *dstcrop, const Gt_Crop *srccrop, const Gif_Image *gfi);
int     crop_image(Gif_Image* gfi, Gt_Frame* fr, int preserve_total_crop);

void    flip_image(Gif_Image* gfi, Gt_Frame* fr, int is_vert);
void    rotate_image(Gif_Image* gfi, Gt_Frame* fr, int rotation);
void    resize_dimensions(int* w, int* h, double new_width, double new_height,
                          int flags);
void    resize_stream(Gif_Stream* gfs, double new_width, double new_height,
                      int flags, int method, int scale_colors);

/*****
 * quantization
 **/
#define KC_GAMMA_SRGB                   0
#define KC_GAMMA_NUMERIC                1
void    kc_set_gamma(int type, double gamma);

#define COLORMAP_DIVERSITY              0
#define COLORMAP_BLEND_DIVERSITY        1
#define COLORMAP_MEDIAN_CUT             2

enum {
    dither_none = 0, dither_default, dither_floyd_steinberg,
    dither_ordered, dither_ordered_new
};
int     set_dither_type(Gt_OutputData* od, const char* name);
void    colormap_stream(Gif_Stream*, Gif_Colormap*, Gt_OutputData*);

/*****
 * parsing stuff
 **/
extern int      frame_spec_1;
extern int      frame_spec_2;
extern char *   frame_spec_name;
extern int      dimensions_x;
extern int      dimensions_y;
extern int      position_x;
extern int      position_y;
extern Gif_Color parsed_color;
extern Gif_Color parsed_color2;
extern double   parsed_scale_factor_x;
extern double   parsed_scale_factor_y;

int             parse_frame_spec(Clp_Parser *, const char *, int, void *);
int             parse_dimensions(Clp_Parser *, const char *, int, void *);
int             parse_position(Clp_Parser *, const char *, int, void *);
int             parse_scale_factor(Clp_Parser *, const char *, int, void *);
int             parse_color(Clp_Parser *, const char *, int, void *);
int             parse_rectangle(Clp_Parser *, const char *, int, void *);
int             parse_two_colors(Clp_Parser *, const char *, int, void *);

extern Gif_Stream *input;
extern const char *input_name;

void            input_stream(const char *);
void            input_done(void);
void            output_frames(void);

/*****
 * stuff with frames
 **/
extern Gt_Frame def_frame;
#define         FRAME(fs, i)    ((fs)->f[i])

Gt_Frameset *   new_frameset(int initial_cap);
Gt_Frame*       add_frame(Gt_Frameset*, Gif_Stream*, Gif_Image*);
void            clear_def_frame_once_options(void);

Gif_Stream *    merge_frame_interval(Gt_Frameset *, int f1, int f2,
                                     Gt_OutputData *, int compress, int *huge);
void            clear_frameset(Gt_Frameset *, int from);
void            blank_frameset(Gt_Frameset *, int from, int to, int delete_ob);

/*****
 * mode
 **/
#define BLANK_MODE      0
#define MERGING         1
#define BATCHING        2
#define EXPLODING       3
#define INFOING         4
#define DELETING        5
#define INSERTING       6

extern int mode;
extern int nested_mode;

#endif
