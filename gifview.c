/* gifview.c - gifview's main loop.
   Copyright (C) 1997-8 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifview.

   Gifview is free software; you can copy, distribute, or alter it at will, as
   long as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include "gifx.h"
#include "clp.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#ifndef FD_SET
#include <sys/select.h>
#endif

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

/*****
 * TIME STUFF (from xwrits)
 **/

#define MicroPerSec 1000000

#define xwADDTIME(result, a, b) do { \
	(result).tv_sec = (a).tv_sec + (b).tv_sec; \
	if (((result).tv_usec = (a).tv_usec + (b).tv_usec) >= MicroPerSec) { \
		(result).tv_sec++; \
		(result).tv_usec -= MicroPerSec; \
	} } while (0)

#define xwSUBTIME(result, a, b) do { \
	(result).tv_sec = (a).tv_sec - (b).tv_sec; \
	if (((result).tv_usec = (a).tv_usec - (b).tv_usec) < 0) { \
		(result).tv_sec--; \
		(result).tv_usec += MicroPerSec; \
	} } while (0)

#define xwSETMINTIME(a, b) do { \
	if ((b).tv_sec < (a).tv_sec || \
	    ((b).tv_sec == (a).tv_sec && (b).tv_usec < (a).tv_usec)) \
		(a) = (b); \
	} while (0)

#define xwTIMEGEQ(a, b) ((a).tv_sec > (b).tv_sec || \
	((a).tv_sec == (b).tv_sec && (a).tv_usec >= (b).tv_usec))

#define xwTIMEGT(a, b) ((a).tv_sec > (b).tv_sec || \
	((a).tv_sec == (b).tv_sec && (a).tv_usec > (b).tv_usec))

#define xwTIMELEQ0(a) ((a).tv_sec < 0 || ((a).tv_sec == 0 && (a).tv_usec <= 0))

#ifdef X_GETTIMEOFDAY
#define xwGETTIMEOFDAY(a) X_GETTIMEOFDAY(a)
#elif SYSV_GETTIMEOFDAY
#define xwGETTIMEOFDAY(a) gettimeofday((a))
#else
#ifdef NO_GETTIMEOFDAY_PROTO
EXTERN int gettimeofday(struct timeval *, struct timezone *);
#endif
#define xwGETTIMEOFDAY(a) gettimeofday((a), 0)
#endif

#define xwGETTIME(a) do { xwGETTIMEOFDAY(&(a)); xwSUBTIME((a), (a), genesis_time); } while (0)
struct timeval genesis_time;


/*****
 * THE VIEWER STRUCTURE
 **/

typedef struct Gt_Viewer {
  
  Display *display;
  int screen_number;
  Visual *visual;
  int depth;
  Colormap colormap;
  Gif_XContext *gfx;
  
  Window window;
  int width;
  int height;
  Pixmap pixmap;
  int being_deleted;
  
  Gif_Stream *gfs;
  char *name;
  
  Gif_Stream *anim_gfs;
  int can_animate;
  
  Gif_Image **im;
  int *im_number;
  int nim;
  int im_cap;
  int im_pos;
  
  struct Gt_Viewer *next;
  
  int animating;
  int scheduled;
  struct Gt_Viewer *anim_next;
  struct timeval timer;
  int anim_loop;
  
} Gt_Viewer;

const char *program_name = "gifview";

static char *cur_display_name = 0;
static Display *cur_display = 0;
static char *cur_geometry_spec = 0;
static const char *cur_resource_name;

static Gt_Viewer *viewers;
static Gt_Viewer *animations;
static int animating = 0;
static int unoptimizing = 0;


#define DISPLAY_OPT	300
#define UNOPTIMIZE_OPT	301
#define VERSION_OPT	302
#define ANIMATE_OPT	303
#define GEOMETRY_OPT	304
#define NAME_OPT	305
#define HELP_OPT	306

Clp_Option options[] = {
  { "animate", 'a', ANIMATE_OPT, 0, Clp_Negate },
  { "display", 'd', DISPLAY_OPT, Clp_ArgString, 0 },
  { "geometry", 'g', GEOMETRY_OPT, Clp_ArgString, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "name", 0, NAME_OPT, Clp_ArgString, 0 },
  { "unoptimize", 'U', UNOPTIMIZE_OPT, 0, Clp_Negate },
  { "version", 0, VERSION_OPT, 0, 0 },
};


/*****
 * Diagnostics
 **/

void
fatal_error(char *message, ...)
{
  va_list val;
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


void
short_usage(void)
{
  fprintf(stderr, "\
Usage: %s [--display DISPLAY] [options] [filenames and frames] ...\n\
Type %s --help for more information.\n",
	  program_name, program_name);
}


void
usage(void)
{
  fprintf(stderr, "\
Usage: %s [--display DISPLAY] [options] [filenames and frames] ...\n\
Options are:\n\
  --animate, -a                 Animate multiframe GIFs.\n\
  --unoptimize, -U              Unoptimize displayed GIFs.\n\
  --display DISPLAY             Set display to DISPLAY.\n\
  --name NAME                   Set application resource name to NAME.\n\
  --geometry GEOMETRY           Set window geometry.\n\
  --help                        Print this message and exit.\n\
  --version                     Print version number and exit.\n\
Frame selections:               #num, #num1-num2, #num1-, #name\n\
Keystrokes:\n\
  [Space] Go to next frame.             [B] Go to previous frame.\n\
  [R] Go to first frame.                [Backspace]/[W] Delete window.\n\
  [ESC] Stop animation.                 [S] Toggle animation.\n\
  [Q] Quit.\n\
Left mouse button goes to next frame, right mouse button deletes window.\n\
",
	  program_name);
}


/*****
 * Window creation
 **/

#if defined(__cplusplus) || defined(c_plusplus)
#define VISUAL_CLASS c_class
#else
#define VISUAL_CLASS class
#endif

static void
choose_visual(Gt_Viewer *viewer)
{
  Display *display = viewer->display;
  int screen_number = viewer->screen_number;
  VisualID default_visualid = DefaultVisual(display, screen_number)->visualid;
  
  XVisualInfo visi_template;
  int nv, i;
  XVisualInfo *v, *best_v = 0;
  Gt_Viewer *trav;
  
  /* Look for an existing Gt_Viewer with the same display and screen number */
  for (trav = viewers; trav; trav = trav->next)
    if (trav != viewer && trav->display == display
	&& trav->screen_number == screen_number) {
      viewer->visual = trav->visual;
      viewer->depth = trav->depth;
      viewer->colormap = trav->colormap;
      viewer->gfx = trav->gfx;
      return;
    }
  
  /* Find the default visual's XVisualInfo & put it in best_v */
  visi_template.screen = screen_number;
  v = XGetVisualInfo(display, VisualScreenMask, &visi_template, &nv);
  for (i = 0; i < nv && !best_v; i++)
    if (v[i].visualid == default_visualid)
      best_v = &v[i];
  
  if (!best_v) {
    
    /* This should never happen. If we can't find the default visual's
       XVisualInfo, we just use the default visual */
    viewer->visual = DefaultVisual(display, screen_number);
    viewer->depth = DefaultDepth(display, screen_number);
    viewer->colormap = DefaultColormap(display, screen_number);
    
  } else {
    
    /* Which visual to choose? This isn't exactly a simple decision, since
       we want to avoid colormap flashing while choosing a nice visual. So
       here's the algorithm: Prefer the default visual, or take a TrueColor
       visual with strictly greater depth. */
    for (i = 0; i < nv; i++)
      if (v[i].depth > best_v->depth && v[i].VISUAL_CLASS == TrueColor)
	best_v = &v[i];
    
    viewer->visual = best_v->visual;
    viewer->depth = best_v->depth;
    if (best_v->visualid != default_visualid)
      viewer->colormap =
	XCreateColormap(display, RootWindow(display, screen_number),
			viewer->visual, AllocNone);
    else
      viewer->colormap = DefaultColormap(display, screen_number);
    
  }

  viewer->gfx = Gif_NewXContextFromVisual
    (display, screen_number, viewer->visual, viewer->depth, viewer->colormap);
  
  if (v) XFree(v);
}


Gt_Viewer *
new_viewer(Display *display, Gif_Stream *gfs, char *name)
{
  Gt_Viewer *viewer;
  
  /* Make the Gt_Viewer structure */
  viewer = Gif_New(Gt_Viewer);
  viewer->display = display;
  viewer->screen_number = DefaultScreen(display);
  choose_visual(viewer);
  viewer->window = None;
  viewer->being_deleted = 0;
  viewer->pixmap = None;
  viewer->gfs = gfs;
  viewer->name = name;
  viewer->anim_gfs = 0;
  viewer->im_cap = Gif_ImageCount(gfs);
  viewer->im = Gif_NewArray(Gif_Image *, viewer->im_cap);
  viewer->im_number = Gif_NewArray(int, viewer->im_cap);
  viewer->nim = 0;
  viewer->im_pos = -1;
  viewer->next = viewers;
  viewers = viewer;
  viewer->animating = 0;
  viewer->scheduled = 0;
  viewer->anim_next = 0;
  viewer->anim_loop = 0;
  
  return viewer;
}


void
delete_viewer(Gt_Viewer *viewer)
{
  Gt_Viewer *prev = 0, *trav;
  if (viewer->pixmap) XFreePixmap(viewer->display, viewer->pixmap);
  
  for (trav = viewers; trav != viewer; prev = trav, trav = trav->next)
    ;
  if (prev) prev->next = viewer->next;
  else viewers = viewer->next;
  
  Gif_DeleteStream(viewer->gfs);
  if (viewer->anim_gfs != viewer->gfs)
    Gif_DeleteStream(viewer->anim_gfs);
  Gif_DeleteArray(viewer->im);
  Gif_DeleteArray(viewer->im_number);
  Gif_Delete(viewer);
}


static Gt_Viewer *
get_input_stream(char *name)
{
  FILE *f;
  Gif_Stream *gfs = 0;
  Gt_Viewer *viewer;
  
  if (name == 0 || strcmp(name, "-") == 0) {
    f = stdin;
    name = "<stdin>";
  } else
    f = fopen(name, "rb");
  if (!f) {
    error("can't open `%s' for reading", name);
    return 0;
  }
  
  gfs = Gif_FullReadFile(f, GIF_READ_COMPRESSED);
  fclose(f);
  
  if (!gfs || Gif_ImageCount(gfs) == 0) {
    error("`%s' doesn't seem to contain a GIF", name);
    Gif_DeleteStream(gfs);
    return 0;
  }
  
  if (!cur_display) {
    cur_display = XOpenDisplay(cur_display_name);
    if (!cur_display) {
      error("can't open display");
      return 0;
    }
  }
  
  if (unoptimizing)
    Gif_Unoptimize(gfs);
  
  viewer = new_viewer(cur_display, gfs, name);
  if (unoptimizing)
    viewer->anim_gfs = gfs;
  return viewer;
}


/*****
 * Schedule stuff
 **/

void
switch_animating(Gt_Viewer *viewer, int animating)
{
  int i;
  Gif_Stream *gfs;
  
  if (animating == viewer->animating || !viewer->can_animate)
    return;
  
  if (animating && !viewer->anim_gfs) {
    viewer->anim_gfs = Gif_CopyStreamImages(viewer->gfs);
    Gif_Unoptimize(viewer->anim_gfs);
  }
  gfs = animating ? viewer->anim_gfs : viewer->gfs;
  for (i = 0; i < gfs->nimages; i++)
    viewer->im[i] = gfs->images[i];
  
  viewer->animating = animating;
}


void
unschedule(Gt_Viewer *viewer)
{
  Gt_Viewer *prev, *trav;
  if (!viewer->scheduled) return;
  for (prev = 0, trav = animations; trav; prev = trav, trav = trav->anim_next)
    if (trav == viewer)
      break;
  if (trav) {
    if (prev) prev->anim_next = viewer->anim_next;
    else animations = viewer->anim_next;
  }
  viewer->scheduled = 0;
}


void
schedule_next_frame(Gt_Viewer *viewer)
{
  struct timeval now, interval;
  Gt_Viewer *prev, *trav;
  int delay = viewer->im[viewer->im_pos]->delay;
  
  xwGETTIME(now);
  interval.tv_sec = delay / 100;
  interval.tv_usec = (delay % 100) * (MicroPerSec / 100);
  xwADDTIME(viewer->timer, now, interval);

  if (viewer->scheduled)
    unschedule(viewer);
  
  prev = 0;
  for (trav = animations; trav; prev = trav, trav = trav->anim_next)
    if (xwTIMEGEQ(trav->timer, viewer->timer))
      break;
  if (prev) {
    viewer->anim_next = trav;
    prev->anim_next = viewer;
  } else {
    viewer->anim_next = animations;
    animations = viewer;
  }
  viewer->scheduled = 1;
}


/*****
 * X stuff
 **/

int
parse_geometry(const char *const_g, XSizeHints *sh, int screen_width,
	       int screen_height)
{
  char *g = (char *)const_g;
  sh->flags = 0;
  
  if (isdigit(*g)) {
    sh->flags |= USSize;
    sh->width = strtol(g, &g, 10);
    if (g[0] == 'x' && isdigit(g[1]))
      sh->height = strtol(g + 1, &g, 10);
    else
      goto error;
  } else if (!*g)
    goto error;
  
  if (*g == '+' || *g == '-') {
    int x_minus, y_minus;
    sh->flags |= USPosition | PWinGravity;
    x_minus = *g == '-';
    sh->x = strtol(g + 1, &g, 10);
    if (x_minus) sh->x = screen_width - sh->x - sh->width;
    
    y_minus = *g == '-';
    if (*g == '-' || *g == '+')
      sh->y = strtol(g + 1, &g, 10);
    else
      goto error;
    if (y_minus) sh->y = screen_height - sh->y - sh->height;
    
    if (x_minus)
      sh->win_gravity = y_minus ? SouthEastGravity : NorthEastGravity;
    else
      sh->win_gravity = y_minus ? SouthWestGravity : NorthWestGravity;
    
  } else if (*g)
    goto error;
  
  return 1;
  
 error:
  warning("bad geometry specification");
  sh->flags = 0;
  return 0;
}


static Atom wm_delete_window_atom;
static Atom wm_protocols_atom;

void
create_viewer_window(Gt_Viewer *viewer, int w, int h)
{
  Display *display = viewer->display;
  Window window;
  char *stringlist[2];
  XTextProperty window_name, icon_name;
  XClassHint classh;
  XSizeHints *sizeh = XAllocSizeHints(); /* sets all fields to 0 */
  
  /* Set the window's geometry */
  sizeh->width = w;
  sizeh->height = h;
  if (cur_geometry_spec) {
    int scr_width = DisplayWidth(viewer->display, viewer->screen_number);
    int scr_height = DisplayHeight(viewer->display, viewer->screen_number);
    parse_geometry(cur_geometry_spec, sizeh, scr_width, scr_height);
  }
  
  /* Open the display and create the window */
  {
    XSetWindowAttributes x_set_attr;
    unsigned long x_set_attr_mask;
    x_set_attr.colormap = viewer->colormap;
    x_set_attr.backing_store = NotUseful;
    x_set_attr.save_under = False;
    x_set_attr.border_pixel = 0;
    x_set_attr.background_pixel = 0;
    x_set_attr_mask = CWColormap | CWBorderPixel | CWBackPixel
      | CWBackingStore | CWSaveUnder;
  
    viewer->window = window = XCreateWindow
      (display, RootWindow(display, viewer->screen_number),
       sizeh->x, sizeh->y, sizeh->width, sizeh->height, 0,
       viewer->depth, InputOutput, viewer->visual,
       x_set_attr_mask, &x_set_attr);
  }
  
  if (sizeh->flags & USSize)
    /* Setting sizes to -1 says user gave us geometry, don't change the size
       later */
    viewer->width = viewer->height = -1;
  else {
    viewer->width = w;
    viewer->height = h;
  }
  
  /* Set the window's title and class (for window manager resources) */
  stringlist[0] = "gifview";
  stringlist[1] = 0;
  XStringListToTextProperty(stringlist, 1, &window_name);
  XStringListToTextProperty(stringlist, 1, &icon_name);
  classh.res_name = (char *)cur_resource_name;
  classh.res_class = "Gifview";
  XSetWMProperties(display, window, &window_name, &icon_name,
		   NULL, 0, sizeh, NULL, &classh);
  
  if (!wm_delete_window_atom) {
    wm_delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
    wm_protocols_atom = XInternAtom(display, "WM_PROTOCOLS", False);
  }
  XSetWMProtocols(display, window, &wm_delete_window_atom, 1);
  
  XSelectInput(display, window, ButtonPressMask | KeyPressMask
	       | StructureNotifyMask);
  
  XFree(sizeh);
}


void
pre_delete_viewer(Gt_Viewer *viewer)
{
  if (viewer->being_deleted) return;
  viewer->being_deleted = 1;
  
  if (viewer->scheduled) unschedule(viewer);
  
  if (viewer->window)
    XDestroyWindow(viewer->display, viewer->window);
  else
    delete_viewer(viewer);
}


Gt_Viewer *
find_viewer(Display *display, Window window)
{
  Gt_Viewer *v;
  for (v = viewers; v; v = v->next)
    if (v->display == display && v->window == window)
      return v;
  return 0;
}


void
set_viewer_name(Gt_Viewer *viewer)
{
  Gif_Image *gfi;
  char *strs[2];
  XTextProperty name_prop;
  int len, image_number;
  
  if (viewer->im_pos >= viewer->nim || viewer->being_deleted)
    return;
  
  gfi = viewer->im[ viewer->im_pos ];
  image_number = viewer->im_number[ viewer->im_pos ];
  len = strlen(viewer->name) + 9;
  if (image_number == -1)
    len += strlen(gfi->identifier) + 1;
  else
    len += 10;
  
  strs[0] = Gif_NewArray(char, len);
  if (Gif_ImageCount(viewer->gfs) == 1 || viewer->animating)
    sprintf(strs[0], "gifview: %s", viewer->name);
  else if (image_number == -1)
    sprintf(strs[0], "gifview: %s #%s", viewer->name, gfi->identifier);
  else
    sprintf(strs[0], "gifview: %s #%d", viewer->name, image_number);
  strs[1] = 0;
  
  XStringListToTextProperty(strs, 1, &name_prop);
  XSetWMName(viewer->display, viewer->window, &name_prop);
  XSetWMIconName(viewer->display, viewer->window, &name_prop);
  
  XFree(name_prop.value);
  Gif_DeleteArray(strs[0]);
}


void
view_frame(Gt_Viewer *viewer, int frame)
{
  Display *display = viewer->display;
  Window window = viewer->window;
  Pixmap old_pixmap = viewer->pixmap;
  int need_set_name = 0;
  
  if (viewer->being_deleted)
    return;
  
  if (frame < 0)
    frame = 0;
  if (frame > viewer->nim - 1 && viewer->animating) {
    int loopcount = viewer->gfs->loopcount;
    if (loopcount == 0 || loopcount > viewer->anim_loop) {
      viewer->anim_loop++;
      frame = 0;
    } else {
      switch_animating(viewer, 0);
      need_set_name = 1;
    }
  }
  if (frame > viewer->nim - 1)
    frame = viewer->nim - 1;
  
  if (frame != viewer->im_pos) {
    Gif_Image *gfi = viewer->im[frame];
    int width = Gif_ImageWidth(gfi);
    int height = Gif_ImageHeight(gfi);
    
    viewer->im_pos = frame;
    
    if (!window) {
      create_viewer_window(viewer, width, height);
      window = viewer->window;
    } else if ((viewer->width != width || viewer->height != height)
	       && viewer->width > 0) {
      /* If viewer->width < 0, user gave geometry; don't change window size */
      XWindowChanges winch;
      winch.width = viewer->width = width;
      winch.height = viewer->height = height;
      XReconfigureWMWindow
	(display, window, viewer->screen_number,
	 CWWidth | CWHeight, &winch);
    }
    
    if ((!viewer->animating && Gif_ImageCount(viewer->gfs) > 1)
	|| old_pixmap == None)
      need_set_name = 1;
    
    /* 5/26/98 Do some noodling around to try and use memory most effectively.
       If animating, keep the uncompressed frame; otherwise, throw it away. */
    Gif_UncompressImage(gfi);
    viewer->pixmap = Gif_XImage(viewer->gfx, viewer->gfs, gfi);
    if (!viewer->animating && gfi->compressed)
      Gif_ReleaseUncompressedImage(gfi);
    
    XSetWindowBackgroundPixmap(display, window, viewer->pixmap);
    if (old_pixmap) {
      XClearWindow(display, window);
      XFreePixmap(display, old_pixmap);
    }
  }
  
  if (need_set_name)
    set_viewer_name(viewer);
  
  if (!old_pixmap)
    /* first image; map the window */
    XMapRaised(display, window);
  else if (viewer->animating)
    /* only schedule next frame if image is already mapped */
    schedule_next_frame(viewer);
}


/*****
 * Command line arguments: marking frames, being done with streams
 **/

void
mark_frame(Gt_Viewer *viewer, int f_num, char *f_name)
{
  Gif_Image *gfi;
  
  if (f_name) {
    gfi = Gif_GetNamedImage(viewer->gfs, f_name);
    if (!gfi) error("no frame named `%s'", f_name);
  } else {
    gfi = Gif_GetImage(viewer->gfs, f_num);
    if (!gfi) error("no frame number %d", f_num);
  }
  
  if (viewer->nim >= viewer->im_cap) {
    viewer->im_cap *= 2;
    Gif_ReArray(viewer->im, Gif_Image *, viewer->im_cap);
    Gif_ReArray(viewer->im_number, int, viewer->im_cap);
  }
  if (gfi) {
    viewer->im[ viewer->nim ] = gfi;
    viewer->im_number[ viewer->nim ] = f_name ? -1 : f_num;
    viewer->nim++;
  }
}


void
frame_argument(Gt_Viewer *viewer, char *arg)
{
  char *c = arg;
  int n1 = 0;
  int n2 = -1;
  
  /* Get a number range (#x, #x-y, #x-, or #-y). First, read x. */
  if (isdigit(c[0]))
    n1 = strtol(arg, &c, 10);
  
  /* Then, if the next character is a dash, read y. */
  if (c[0] == '-') {
    c++;
    if (isdigit(c[0]))
      n2 = strtol(c, &c, 10);
    else
      n2 = Gif_ImageCount(viewer->gfs) - 1;
  }
  
  /* It really was a number range only if c is now at the end of the
     argument. */
  if (c[0] != 0)
    mark_frame(viewer, 0, arg);
  
  else if (n2 == -1)
    mark_frame(viewer, n1, 0);
  
  else
    for (; n1 <= n2; n1++)
      mark_frame(viewer, n1, 0);
}


void
input_stream_done(Gt_Viewer *viewer)
{
  int i;
  viewer->can_animate = Gif_ImageCount(viewer->gfs) > 1;
  
  if (!viewer->nim) {
    for (i = 0; i < Gif_ImageCount(viewer->gfs); i++)
      mark_frame(viewer, i, 0);
  
  } else {
    for (i = 0; i < Gif_ImageCount(viewer->gfs); i++)
      if (viewer->im_number[i] != i)
	viewer->can_animate = 0;
  }
  
  switch_animating(viewer, animating && viewer->can_animate);
  view_frame(viewer, 0);
}


void
key_press(Gt_Viewer *viewer, KeySym key, unsigned state)
{
  if (key == XK_space || key == XK_F || key == XK_f)
    /* space or F: one frame ahead */
    view_frame(viewer, viewer->im_pos + 1);
  
  else if (key == XK_B || key == XK_b)
    /* B: one frame back */
    view_frame(viewer, viewer->im_pos - 1);
  
  else if (key == XK_W || key == XK_w || key == XK_BackSpace)
    /* backspace: delete viewer */
    pre_delete_viewer(viewer);
  
  else if (key == XK_Q || key == XK_q)
    /* Q: quit applicaton */
    exit(0);
  
  else if (key == XK_S || key == XK_s || key == XK_a || key == XK_A) {
    /* S or A: toggle animation */
    switch_animating(viewer, !viewer->animating);
    
    if (viewer->animating) {
      if (viewer->im_pos >= viewer->nim - 1) {
	viewer->im_pos = 0;
	viewer->anim_loop = 0;
      }
      view_frame(viewer, viewer->im_pos);
      
    } else
      unschedule(viewer);
    
    set_viewer_name(viewer);
    
  } else if (key == XK_R || key == XK_r) {
    /* R: reset to first frame */
    unschedule(viewer);
    viewer->anim_loop = 0;
    view_frame(viewer, 0);
    
  } else if (key == XK_Escape && viewer->animating) {
    /* Escape: stop animation */
    switch_animating(viewer, 0);
    unschedule(viewer);
    set_viewer_name(viewer);
  }
}


void
loop(void)
{
  struct timeval timeout, now, *timeout_ptr;
  fd_set xfds;
  XEvent e;
  int pending;
  Gt_Viewer *v;
  Display *display = viewers->display;
  int x_socket = ConnectionNumber(display);
  
  xwGETTIME(now);
  FD_ZERO(&xfds);
  
  while (viewers) {
    /* Check for any animations */
    while (animations && xwTIMEGEQ(now, animations->timer)) {
      v = animations;
      animations = v->anim_next;
      v->scheduled = 0;
      view_frame(v, v->im_pos + 1);
    }
    
    if (animations) {
      xwSUBTIME(timeout, animations->timer, now);
      timeout_ptr = &timeout;
    } else
      timeout_ptr = 0;
    
    pending = XPending(display);
    if (!pending) {
      FD_SET(x_socket, &xfds);
      select(x_socket + 1, &xfds, 0, 0, timeout_ptr);
      pending = FD_ISSET(x_socket, &xfds);
    }
    
    if (pending)
      while (XPending(display)) {
	XNextEvent(display, &e);
	v = find_viewer(e.xany.display, e.xany.window);
	
	if (v && e.type == ButtonPress && e.xbutton.button == 1)
	  /* Left mouse button: go to next frame */
	  view_frame(v, v->im_pos + 1);
	
	else if (v && e.type == ButtonPress && e.xbutton.button == 3)
	  /* Right mouse button: delete window */
	  pre_delete_viewer(v);
	
	else if (v && e.type == KeyPress)
	  /* Key press: call function */
	  key_press(v, XKeycodeToKeysym(e.xany.display, e.xkey.keycode, 0),
		    e.xkey.state);
	
	else if (v && e.type == ClientMessage
		 && e.xclient.message_type == wm_protocols_atom
		 && (Atom)(e.xclient.data.l[0]) == wm_delete_window_atom)
	  /* WM_DELETE_WINDOW message: delete window */
	  pre_delete_viewer(v);

	else if (v && e.type == MapNotify && v->animating
		 && v->scheduled == 0)
	  /* Window was just mapped; now, start animating it */
	  schedule_next_frame(v);
	  
	else if (v && e.type == DestroyNotify)
	  /* Once the window has been destroyed, delete related state */
	  delete_viewer(v);
	
      }
    
    xwGETTIME(now);
  }
}


int
main(int argc, char **argv)
{
  Gt_Viewer *viewer = 0;
  int viewer_given = 0;
  int any_errors = 0;
  
  Clp_Parser *clp =
    Clp_NewParser(argc, argv,
                  sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  program_name = cur_resource_name = Clp_ProgramName(clp);
  
  xwGETTIMEOFDAY(&genesis_time);
  
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case DISPLAY_OPT:
      if (cur_display)
	fatal_error("`--display' must come before all other options");
      cur_display_name = clp->arg;
      cur_display = 0;
      break;
      
     case GEOMETRY_OPT:
      cur_geometry_spec = clp->arg;
      break;
      
     case NAME_OPT:
      cur_resource_name = clp->arg;
      break;
      
     case UNOPTIMIZE_OPT:
      unoptimizing = clp->negated ? 0 : 1;
      break;
      
     case ANIMATE_OPT:
      animating = clp->negated ? 0 : 1;
      break;
      
     case VERSION_OPT:
      printf("Gifview version %s\n", VERSION);
      printf("Copyright (C) 1997-8 Eddie Kohler\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose. That's right: you're on your own!\n");
      exit(0);
      break;
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case Clp_NotOption:
      if (clp->arg[0] == '#') {
	if (!viewer_given) {
	  viewer = get_input_stream(0);
	  viewer_given = 1;
	}
	if (viewer) frame_argument(viewer, clp->arg + 1);
      } else {
        if (viewer) input_stream_done(viewer);
        viewer = get_input_stream(clp->arg);
	viewer_given = 1;
      }
      break;
      
     case Clp_Done:
      goto done;
      
     case Clp_BadOption:
      short_usage();
      any_errors = 1;
      break;
      
     default:
      break;
      
    }
  }
  
 done:
  
  if (!viewer_given) {
    if (any_errors) exit(1);
    viewer = get_input_stream(0);
  }
  if (viewer) input_stream_done(viewer);
  
  if (viewers) loop();
  
#ifdef DMALLOC
  dmalloc_report();
#endif
  return 0;
}


#ifndef DMALLOC
/* be careful about memory allocation */
#undef malloc
#undef realloc
#ifdef __cplusplus
extern "C" {
#endif

void *
gt_malloc(int size, const char *file, int line)
{
  void *p = malloc(size);
  if (!p && size)
    fatal_error("out of memory (wanted %d at %s:%d)", size, file, line);
  return p;
}

void *
gt_realloc(void *p, int size, const char *file, int line)
{
  if (!p)
    return gt_malloc(size, file, line);
  p = realloc(p, size);
  if (!p && size)
    fatal_error("out of memory (wanted %d at %s:%d)", size, file, line);
  return p;
}

#ifdef __cplusplus
}
#endif
#endif
