/* gifview.c - gifview's main loop.
   Copyright (C) 1997-2019 Eddie Kohler, ekohler@gmail.com
   This file is part of gifview, in the gifsicle package.

   Gifview is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#ifdef X_DISPLAY_MISSING
#error "You can't compile gifview without X."
#endif

#include <lcdfgif/gifx.h>
#include <lcdf/clp.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif

/*****
 * TIME STUFF (from xwrits)
 **/

#define MICRO_PER_SEC 1000000

#define xwADDTIME(result, a, b) do { \
	(result).tv_sec = (a).tv_sec + (b).tv_sec; \
	if (((result).tv_usec = (a).tv_usec + (b).tv_usec) >= MICRO_PER_SEC) { \
		(result).tv_sec++; \
		(result).tv_usec -= MICRO_PER_SEC; \
	} } while (0)

#define xwSUBTIME(result, a, b) do { \
	(result).tv_sec = (a).tv_sec - (b).tv_sec; \
	if (((result).tv_usec = (a).tv_usec - (b).tv_usec) < 0) { \
		(result).tv_sec--; \
		(result).tv_usec += MICRO_PER_SEC; \
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
# define xwGETTIMEOFDAY(a) X_GETTIMEOFDAY(a)
#elif GETTIMEOFDAY_PROTO == 0
EXTERN int gettimeofday(struct timeval *, struct timezone *);
# define xwGETTIMEOFDAY(a) gettimeofday((a), 0)
#elif GETTIMEOFDAY_PROTO == 1
# define xwGETTIMEOFDAY(a) gettimeofday((a))
#else
# define xwGETTIMEOFDAY(a) gettimeofday((a), 0)
#endif

#define xwGETTIME(a) do { xwGETTIMEOFDAY(&(a)); xwSUBTIME((a), (a), genesis_time); } while (0)
struct timeval genesis_time;


/*****
 * THE VIEWER STRUCTURE
 **/

static unsigned pixel_memory_limit_kb = 40000;
static unsigned pixel_memory_kb;

typedef struct Gt_Viewer {

  Display *display;
  int screen_number;
  Visual *visual;
  int depth;
  Colormap colormap;
  Gif_XContext *gfx;
  Cursor arrow_cursor;
  Cursor wait_cursor;

  Window parent;
  int top_level;

  Window window;
  int use_window;
  int width;
  int height;
  int resizable;
  int being_deleted;

  Gif_Stream *gfs;
  const char *name;
  const char *title;

  Gif_Image **im;
  int nim;

  Pixmap pixmap;
  int im_pos;
  int was_unoptimized;

  Gif_XFrame *unoptimized_frames;
  int n_unoptimized_frames;

  struct Gt_Viewer *next;

  int can_animate;
  int animating;
  int unoptimizing;
  int scheduled;
  int preparing;
  struct Gt_Viewer *anim_next;
  struct timeval timer;
  int anim_loop;

} Gt_Viewer;

const char *program_name = "gifview";
static Clp_Parser* clp;

static const char *cur_display_name = 0;
static Display *cur_display = 0;
static const char *cur_geometry_spec = 0;
static Cursor cur_arrow_cursor = 0;
static Cursor cur_wait_cursor = 0;
static const char *cur_resource_name;
static const char *cur_window_title = 0;
static Window cur_use_window = None;
static int cur_use_window_new = 0;
static const char *cur_background_color = "black";

static Gt_Viewer *viewers;
static Gt_Viewer *animations;
static int animating = 0;
static int unoptimizing = 0;
static int install_colormap = 0;
static int interactive = 1;
static int min_delay = 0;
static int fallback_delay = 0;

static struct timeval preparation_time;


#define DISPLAY_OPT		300
#define UNOPTIMIZE_OPT		301
#define VERSION_OPT		302
#define ANIMATE_OPT		303
#define GEOMETRY_OPT		304
#define NAME_OPT		305
#define HELP_OPT		306
#define WINDOW_OPT		307
#define INSTALL_COLORMAP_OPT	308
#define INTERACTIVE_OPT		309
#define BACKGROUND_OPT		310
#define NEW_WINDOW_OPT		311
#define TITLE_OPT		312
#define MIN_DELAY_OPT		313
#define FALLBACK_DELAY_OPT	314
#define MEMORY_LIMIT_OPT	315

#define WINDOW_TYPE		(Clp_ValFirstUser)

const Clp_Option options[] = {
  { "animate", 'a', ANIMATE_OPT, 0, Clp_Negate },
  { "background", 'b', BACKGROUND_OPT, Clp_ValString, 0 },
  { "bg", 't', BACKGROUND_OPT, Clp_ValString, 0 },
  { "display", 'd', DISPLAY_OPT, Clp_ValStringNotOption, 0 },
  { "geometry", 'g', GEOMETRY_OPT, Clp_ValString, 0 },
  { "install-colormap", 'i', INSTALL_COLORMAP_OPT, 0, Clp_Negate },
  { "interactive", 'e', INTERACTIVE_OPT, 0, Clp_Negate },
  { "help", 0, HELP_OPT, 0, 0 },
  { "memory-limit", 0, MEMORY_LIMIT_OPT, Clp_ValUnsigned, Clp_Negate },
  { "min-delay", 0, MIN_DELAY_OPT, Clp_ValInt, Clp_Negate },
  { "fallback-delay", 0, FALLBACK_DELAY_OPT, Clp_ValInt, Clp_Negate },
  { "name", 0, NAME_OPT, Clp_ValString, 0 },
  { "title", 'T', TITLE_OPT, Clp_ValString, 0 },
  { "unoptimize", 'U', UNOPTIMIZE_OPT, 0, Clp_Negate },
  { "version", 0, VERSION_OPT, 0, 0 },
  { "window", 'w', WINDOW_OPT, WINDOW_TYPE, 0 },
  { "new-window", 0, NEW_WINDOW_OPT, WINDOW_TYPE, 0 }
};


/*****
 * Diagnostics
 **/

void fatal_error(const char* format, ...) {
    char buf[BUFSIZ];
    int n = snprintf(buf, BUFSIZ, "%s: ", program_name);
    va_list val;
    va_start(val, format);
    Clp_vsnprintf(clp, buf + n, BUFSIZ - n, format, val);
    va_end(val);
    fputs(buf, stderr);
    exit(1);
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

void warning(const char* format, ...) {
    char buf[BUFSIZ];
    int n = snprintf(buf, BUFSIZ, "%s: warning: ", program_name);
    va_list val;
    va_start(val, format);
    Clp_vsnprintf(clp, buf + n, BUFSIZ - n, format, val);
    va_end(val);
    fputs(buf, stderr);
}

void short_usage(void) {
    Clp_fprintf(clp, stderr, "\
Usage: %s [--display DISPLAY] [OPTION]... [FILE | FRAME]...\n\
Try %<%s --help%> for more information.\n",
                program_name, program_name);
}

void usage(void) {
    Clp_fprintf(clp, stdout, "\
%<Gifview%> is a lightweight GIF viewer for X. It can display animated GIFs as\n\
slideshows, one frame at a time, or as animations.\n\
\n\
Usage: %s [--display DISPLAY] [OPTION]... [FILE | FRAME]...\n\n", program_name);
    Clp_fprintf(clp, stdout, "\
Options are:\n\
  -a, --animate                 Animate multiframe GIFs.\n\
  -U, --unoptimize              Unoptimize displayed GIFs.\n\
  -d, --display DISPLAY         Set display to DISPLAY.\n\
      --name NAME               Set application resource name to NAME.\n\
  -g, --geometry GEOMETRY       Set window geometry.\n\
  -T, --title TITLE             Set window title.\n");
    Clp_fprintf(clp, stdout, "\
  -w, --window WINDOW           Show GIF in existing WINDOW.\n\
      --new-window WINDOW       Show GIF in new child of existing WINDOW.\n\
  -i, --install-colormap        Use a private colormap.\n\
  --bg, --background COLOR      Use COLOR for transparent pixels.\n\
      --min-delay DELAY         Set minimum frame delay to DELAY/100 sec.\n\
      --fallback-delay DELAY    Set fallback frame delay to DELAY/100 sec.\n\
  +e, --no-interactive          Ignore buttons and keystrokes.\n");
    Clp_fprintf(clp, stdout, "\
      --memory-limit LIM        Cache at most LIM megabytes of animation.\n\
      --help                    Print this message and exit.\n\
      --version                 Print version number and exit.\n\
\n\
Frame selections:               #num, #num1-num2, #num1-, #name\n\n");
    Clp_fprintf(clp, stdout, "\
Keystrokes:\n\
  [N]/[Space] Go to next frame.         [P]/[B] Go to previous frame.\n\
  [R]/[<] Go to first frame.            [>] Go to last frame.\n\
  [ESC] Stop animation.                 [S]/[A] Toggle animation.\n\
  [U] Toggle unoptimization.            [Backspace]/[W] Delete window.\n\
  [Q] Quit.\n\
\n\
Left mouse button goes to next frame, right mouse button deletes window.\n\
\n\
Report bugs to <ekohler@gmail.com>.\n");
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
  if (!install_colormap)
    for (trav = viewers; trav; trav = trav->next)
      if (trav != viewer && trav->display == display
	  && trav->screen_number == screen_number) {
	viewer->visual = trav->visual;
	viewer->depth = trav->depth;
	viewer->colormap = trav->colormap;
	viewer->gfx = trav->gfx;
	viewer->gfx->refcount++;
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
    if (best_v->visualid != default_visualid
	|| (best_v->VISUAL_CLASS == PseudoColor && install_colormap))
      viewer->colormap =
	XCreateColormap(display, RootWindow(display, screen_number),
			viewer->visual, AllocNone);
    else
      viewer->colormap = DefaultColormap(display, screen_number);

  }

  viewer->gfx = Gif_NewXContextFromVisual
    (display, screen_number, viewer->visual, viewer->depth, viewer->colormap);
  viewer->gfx->refcount++;

  if (v) XFree(v);
}


Gt_Viewer *
new_viewer(Display *display, Gif_Stream *gfs, const char *name)
{
  Gt_Viewer *viewer;
  int i;

  /* Make the Gt_Viewer structure */
  viewer = Gif_New(Gt_Viewer);
  viewer->display = display;

  if (cur_use_window) {
    XWindowAttributes attr;

    if (cur_use_window == (Window)(-1)) {	/* means use root window */
      viewer->screen_number = DefaultScreen(display);
      cur_use_window = RootWindow(display, viewer->screen_number);
    }

    XGetWindowAttributes(display, cur_use_window, &attr);

    viewer->screen_number = -1;
    for (i = 0; i < ScreenCount(display); i++)
      if (ScreenOfDisplay(display, i) == attr.screen)
	viewer->screen_number = i;
    assert(viewer->screen_number >= 0);

    viewer->visual = attr.visual;
    viewer->depth = attr.depth;
    viewer->colormap = attr.colormap;

    viewer->gfx = Gif_NewXContextFromVisual
      (display, viewer->screen_number, viewer->visual, viewer->depth,
       viewer->colormap);
    viewer->gfx->refcount++;

    /* Before -- use root window, if that's what we were given; otherwise,
       create a child of the window we were given */
    /* 13.Nov.2001 - don't make a child of the window we were given! */
    if (cur_use_window_new) {
      viewer->window = None;
      viewer->use_window = 0;
    } else {
      viewer->window = cur_use_window;
      viewer->use_window = 1;
    }
    viewer->parent = cur_use_window;
    viewer->top_level = 0;
    viewer->resizable = 0;

  } else {
    viewer->screen_number = DefaultScreen(display);
    choose_visual(viewer);
    viewer->window = None;
    viewer->parent = RootWindow(display, viewer->screen_number);
    viewer->use_window = 0;
    viewer->top_level = 1;
    viewer->resizable = 1;
  }

  /* assign background color */
  if (cur_background_color) {
    XColor color;
    if (!XParseColor(viewer->display, viewer->colormap, cur_background_color,
		     &color)) {
      error("invalid background color %<%s%>\n", cur_background_color);
      cur_background_color = 0;
    } else if (!XAllocColor(viewer->display, viewer->colormap, &color))
      warning("can%,t allocate background color\n");
    else {
      unsigned long pixel = color.pixel;
      Gif_XContext *gfx = viewer->gfx;
      if (pixel != gfx->transparent_pixel && gfx->refcount > 1) {
	/* copy X context */
	viewer->gfx = Gif_NewXContextFromVisual
	  (gfx->display, gfx->screen_number, gfx->visual, gfx->depth,
	   gfx->colormap);
	viewer->gfx->refcount++;
	gfx->refcount--;
      }
      viewer->gfx->transparent_pixel = pixel;
    }
  }

  if (!cur_arrow_cursor) {
    cur_arrow_cursor = XCreateFontCursor(display, XC_left_ptr);
    cur_wait_cursor = XCreateFontCursor(display, XC_watch);
  }
  viewer->arrow_cursor = cur_arrow_cursor;
  viewer->wait_cursor = cur_wait_cursor;

  viewer->being_deleted = 0;
  viewer->gfs = gfs;
  gfs->refcount++;
  viewer->name = name;
  viewer->title = cur_window_title;
  viewer->nim = Gif_ImageCount(gfs);
  viewer->im = Gif_NewArray(Gif_Image *, viewer->nim);
  for (i = 0; i < viewer->nim; i++)
    viewer->im[i] = gfs->images[i];
  viewer->pixmap = None;
  viewer->im_pos = -1;
  viewer->was_unoptimized = 0;
  viewer->unoptimized_frames = Gif_NewXFrames(gfs);
  viewer->n_unoptimized_frames = 0;
  viewer->next = viewers;
  viewers = viewer;
  viewer->animating = 0;
  viewer->unoptimizing = unoptimizing;
  viewer->scheduled = 0;
  viewer->preparing = 0;
  viewer->anim_next = 0;
  viewer->anim_loop = 0;
  viewer->timer.tv_sec = viewer->timer.tv_usec = 0;

  return viewer;
}


void
delete_viewer(Gt_Viewer *viewer)
{
  Gt_Viewer *prev = 0, *trav;
  if (viewer->pixmap && !viewer->was_unoptimized)
    XFreePixmap(viewer->display, viewer->pixmap);

  for (trav = viewers; trav != viewer; prev = trav, trav = trav->next)
    ;
  if (prev) prev->next = viewer->next;
  else viewers = viewer->next;

  Gif_DeleteXFrames(viewer->gfx, viewer->gfs, viewer->unoptimized_frames);
  Gif_DeleteStream(viewer->gfs);
  Gif_DeleteArray(viewer->im);
  Gif_DeleteXContext(viewer->gfx);
  Gif_Delete(viewer);
}


static Gt_Viewer *
next_viewer(Gif_Stream *gfs, const char *name)
{
  Gt_Viewer *viewer = new_viewer(cur_display, gfs, name);
  cur_use_window = None;
  return viewer;
}

static Gt_Viewer *
get_input_stream(const char *name)
{
  FILE *f;
  Gif_Stream *gfs = 0;

  if (name == 0 || strcmp(name, "-") == 0) {
#ifndef OUTPUT_GIF_TO_TERMINAL
    if (isatty(fileno(stdin))) {
      error("<stdin>: is a terminal\n");
      return NULL;
    }
#endif
    f = stdin;
#if defined(_MSDOS) || defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
#elif defined(__DJGPP__)
    setmode(fileno(stdin), O_BINARY);
#elif defined(__EMX__)
    _fsetmode(stdin, "b");
#endif
    name = "<stdin>";
  } else
    f = fopen(name, "rb");
  if (!f) {
    error("%s: %s\n", name, strerror(errno));
    return 0;
  }

  gfs = Gif_FullReadFile(f, GIF_READ_COMPRESSED, 0, 0);
  fclose(f);

  if (!gfs || Gif_ImageCount(gfs) == 0) {
    error("%s: file not in GIF format\n", name);
    Gif_DeleteStream(gfs);
    return 0;
  }

  if (!cur_display) {
    cur_display = XOpenDisplay(cur_display_name);
    if (!cur_display) {
      error("can%,t open display\n");
      return 0;
    }
  }

  return next_viewer(gfs, name);
}


/*****
 * Schedule stuff
 **/

void
switch_animating(Gt_Viewer *viewer, int animating)
{
  int i;
  Gif_Stream *gfs = viewer->gfs;
  if (animating == viewer->animating || !viewer->can_animate)
    return;
  for (i = 0; i < gfs->nimages; i++)
    viewer->im[i] = gfs->images[i];
  viewer->animating = animating;
  if (!animating)
    viewer->timer.tv_sec = viewer->timer.tv_usec = 0;
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
  viewer->timer.tv_sec = viewer->timer.tv_usec = 0;
}

void
schedule(Gt_Viewer *viewer)
{
  Gt_Viewer *prev, *trav;

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

void
schedule_next_frame(Gt_Viewer *viewer)
{
  struct timeval interval;
  int delay = viewer->im[viewer->im_pos]->delay;
  int next_pos = viewer->im_pos + 1;
  if (delay < 1)
    delay = fallback_delay;
  if (delay < min_delay)
    delay = min_delay;
  if (next_pos == viewer->nim)
    next_pos = 0;

  if (viewer->timer.tv_sec == 0 && viewer->timer.tv_usec == 0)
    xwGETTIME(viewer->timer);

  interval.tv_sec = delay / 100;
  interval.tv_usec = (delay % 100) * (MICRO_PER_SEC / 100);
  if (delay == 0)
    interval.tv_usec = 2000;
  xwADDTIME(viewer->timer, viewer->timer, interval);

  /* 1.Aug.2002 - leave some time to prepare the frame if necessary */
  if (viewer->unoptimized_frames[next_pos].pixmap) {
    xwSUBTIME(viewer->timer, viewer->timer, preparation_time);
    viewer->preparing = 1;
  }

  schedule(viewer);
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
  warning("bad geometry specification\n");
  sh->flags = 0;
  return 0;
}


static Atom wm_delete_window_atom;
static Atom wm_protocols_atom;

void
create_viewer_window(Gt_Viewer *viewer, int w, int h)
{
  Display *display = viewer->display;
  char *stringlist[2];
  XTextProperty window_name, icon_name;
  XClassHint classh;
  XSizeHints *sizeh = XAllocSizeHints(); /* sets all fields to 0 */

  /* Set the window's geometry */
  sizeh->width = w ? w : 1;
  sizeh->height = h ? h : 1;
  if (cur_geometry_spec) {
    int scr_width = DisplayWidth(viewer->display, viewer->screen_number);
    int scr_height = DisplayHeight(viewer->display, viewer->screen_number);
    parse_geometry(cur_geometry_spec, sizeh, scr_width, scr_height);
  }

  /* Open the display and create the window */
  if (!viewer->window) {
    XSetWindowAttributes x_set_attr;
    unsigned long x_set_attr_mask;
    x_set_attr.colormap = viewer->colormap;
    x_set_attr.backing_store = NotUseful;
    x_set_attr.save_under = False;
    x_set_attr.border_pixel = 0;
    x_set_attr.background_pixel = 0;
    x_set_attr_mask = CWColormap | CWBorderPixel | CWBackPixel
      | CWBackingStore | CWSaveUnder;

    viewer->window = XCreateWindow
      (display, viewer->parent,
       sizeh->x, sizeh->y, sizeh->width, sizeh->height, 0,
       viewer->depth, InputOutput, viewer->visual,
       x_set_attr_mask, &x_set_attr);
    XDefineCursor(display, viewer->window, viewer->arrow_cursor);
  }

  /* If user gave us geometry, don't change the size later */
  if (sizeh->flags & USSize)
    viewer->resizable = 0;
  viewer->width = w;
  viewer->height = h;

  /* Set the window's title and class (for window manager resources) */
  if (viewer->top_level) {
    stringlist[0] = "gifview";
    stringlist[1] = 0;
    XStringListToTextProperty(stringlist, 1, &window_name);
    XStringListToTextProperty(stringlist, 1, &icon_name);
    classh.res_name = (char *)cur_resource_name;
    classh.res_class = "Gifview";
    XSetWMProperties(display, viewer->window, &window_name, &icon_name,
		     NULL, 0, sizeh, NULL, &classh);
    XFree(window_name.value);
    XFree(icon_name.value);

    if (!wm_delete_window_atom) {
      wm_delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);
      wm_protocols_atom = XInternAtom(display, "WM_PROTOCOLS", False);
    }
    XSetWMProtocols(display, viewer->window, &wm_delete_window_atom, 1);
  }

  if (interactive)
    XSelectInput(display, viewer->window, ButtonPressMask | KeyPressMask
		 | StructureNotifyMask);
  else
    XSelectInput(display, viewer->window, StructureNotifyMask);

  XFree(sizeh);
}


void
pre_delete_viewer(Gt_Viewer *viewer)
{
  if (viewer->being_deleted) return;
  viewer->being_deleted = 1;

  if (viewer->scheduled) unschedule(viewer);

  if (viewer->window && !viewer->use_window)
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
set_viewer_name(Gt_Viewer *viewer, int slow_number)
{
  Gif_Image *gfi;
  char *strs[2];
  char *identifier;
  XTextProperty name_prop;
  int im_pos = (slow_number >= 0 ? slow_number : viewer->im_pos);
  int len;

  if (!viewer->top_level || im_pos >= viewer->nim || viewer->being_deleted)
    return;

  gfi = viewer->im[im_pos];
  len = strlen(viewer->title) + strlen(viewer->name) + 14;
  identifier = (slow_number >= 0 ? (char *)0 : gfi->identifier);
  if (identifier)
    len += 2 + strlen(identifier);

  strs[0] = Gif_NewArray(char, len);
  if (strcmp(viewer->title, "gifview") != 0)
    strcpy(strs[0], viewer->title);
  else if (slow_number >= 0)
    sprintf(strs[0], "gifview: %s [#%d]", viewer->name, im_pos);
  else if (viewer->nim == 1 && identifier)
    sprintf(strs[0], "gifview: %s #%s", viewer->name, identifier);
  else if (viewer->animating || viewer->nim == 1)
    sprintf(strs[0], "gifview: %s", viewer->name);
  else if (!identifier)
    sprintf(strs[0], "gifview: %s #%d", viewer->name, im_pos);
  else
    sprintf(strs[0], "gifview: %s #%d #%s", viewer->name, im_pos, identifier);
  strs[1] = 0;

  XStringListToTextProperty(strs, 1, &name_prop);
  XSetWMName(viewer->display, viewer->window, &name_prop);
  XSetWMIconName(viewer->display, viewer->window, &name_prop);

  XFree(name_prop.value);
  Gif_DeleteArray(strs[0]);
}

static unsigned screen_memory_kb(const Gt_Viewer* viewer) {
  return 1 + ((unsigned) (viewer->gfs->screen_width
                          * viewer->gfs->screen_height) / 334);
}

static Pixmap
unoptimized_frame(Gt_Viewer *viewer, int frame, int slow)
{
  /* create a new unoptimized frame if necessary */
  if (!viewer->unoptimized_frames[frame].pixmap) {
    (void) Gif_XNextImage(viewer->gfx, viewer->gfs, frame,
			  viewer->unoptimized_frames);
    pixel_memory_kb += screen_memory_kb(viewer);
    viewer->unoptimized_frames[viewer->n_unoptimized_frames].user_data =
      frame;
    ++viewer->n_unoptimized_frames;
    if (slow) {
      set_viewer_name(viewer, frame);
      XFlush(viewer->display);
    }
  }

  /* kill some old frames if over the memory limit */
  while (pixel_memory_limit_kb != (unsigned) -1
         && pixel_memory_limit_kb < pixel_memory_kb
         && viewer->n_unoptimized_frames > 1) {
    int killidx, killframe, i = 0;
    do {
      killidx = random() % viewer->n_unoptimized_frames;
      killframe = viewer->unoptimized_frames[killidx].user_data;
      ++i;
    } while (killframe == frame
             || (i < 10 && killframe > frame && killframe < frame + 5)
             || (i < 10 && (killframe % 50) == 0));
    XFreePixmap(viewer->display, viewer->unoptimized_frames[killframe].pixmap);
    viewer->unoptimized_frames[killframe].pixmap = None;
    --viewer->n_unoptimized_frames;
    viewer->unoptimized_frames[killidx].user_data =
      viewer->unoptimized_frames[viewer->n_unoptimized_frames].user_data;
    pixel_memory_kb -= screen_memory_kb(viewer);
  }

  return viewer->unoptimized_frames[frame].pixmap;
}

void
prepare_frame(Gt_Viewer *viewer, int frame)
{
  Display *display = viewer->display;
  Window window = viewer->window;
  int changed_cursor = 0;

  if (viewer->being_deleted || !viewer->animating)
    return;

  if (frame < 0 || frame > viewer->nim - 1)
    frame = 0;

  /* Change cursor if we need to wait. */
  if ((viewer->animating || viewer->unoptimizing)
      && !viewer->unoptimized_frames[frame].pixmap) {
    if (frame > viewer->im_pos + 10 || frame < viewer->im_pos) {
      changed_cursor = 1;
      XDefineCursor(display, window, viewer->wait_cursor);
      XFlush(display);
    }
  }

  /* Prepare the frame */
  (void) unoptimized_frame(viewer, frame, changed_cursor && !viewer->animating);

  /* Restore cursor */
  if (changed_cursor)
    XDefineCursor(display, window, viewer->arrow_cursor);

  /* schedule actual view of window */
  xwADDTIME(viewer->timer, viewer->timer, preparation_time);
  viewer->preparing = 0;
  schedule(viewer);
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
    int width, height, changed_cursor = 0;

    /* Change cursor if we need to wait. */
    if ((viewer->animating || viewer->unoptimizing)
	&& !viewer->unoptimized_frames[frame].pixmap) {
      if (frame > viewer->im_pos + 10 || frame < viewer->im_pos) {
	changed_cursor = 1;
	XDefineCursor(display, window, viewer->wait_cursor);
	XFlush(display);
      }
    }

    /* 5/26/98 Do some noodling around to try and use memory most effectively.
       If animating, keep the uncompressed frame; otherwise, throw it away. */
    if (viewer->animating || viewer->unoptimizing)
      viewer->pixmap = unoptimized_frame(viewer, frame,
                                         changed_cursor && !viewer->animating);
    else
      viewer->pixmap = Gif_XImage(viewer->gfx, viewer->gfs, gfi);

    /* put the image on the window */
    if (viewer->animating || viewer->unoptimizing)
      width = viewer->gfs->screen_width, height = viewer->gfs->screen_height;
    else
      width = Gif_ImageWidth(gfi), height = Gif_ImageHeight(gfi);
    if (!window) {
      create_viewer_window(viewer, width, height);
      window = viewer->window;
    }
    XSetWindowBackgroundPixmap(display, window, viewer->pixmap);
    if (old_pixmap || viewer->use_window) /* clear existing window */
      XClearWindow(display, window);
    /* Only change size after changing pixmap. */
    if ((viewer->width != width || viewer->height != height)
	&& viewer->resizable) {
      XWindowChanges winch;
      winch.width = viewer->width = width;
      winch.height = viewer->height = height;
      XReconfigureWMWindow
	(display, window, viewer->screen_number,
	 CWWidth | CWHeight, &winch);
    }

    /* Get rid of old pixmaps */
    if (!viewer->was_unoptimized && old_pixmap)
      XFreePixmap(display, old_pixmap);
    viewer->was_unoptimized = viewer->animating || viewer->unoptimizing;

    /* Restore cursor */
    if (changed_cursor)
      XDefineCursor(display, window, viewer->arrow_cursor);

    /* Do we need a new name? */
    if ((!viewer->animating && Gif_ImageCount(viewer->gfs) > 1)
	|| old_pixmap == None)
      need_set_name = 1;
  }

  viewer->im_pos = frame;
  viewer->preparing = 0;

  if (need_set_name)
    set_viewer_name(viewer, -1);

  if (!old_pixmap && !viewer->use_window)
    /* first image; map the window */
    XMapRaised(display, window);
  else if (viewer->animating)
    /* only schedule next frame if image is already mapped */
    schedule_next_frame(viewer);
}


/*****
 * Command line arguments: marking frames, being done with streams
 **/

int
frame_argument(Gt_Viewer *viewer, const char *arg)
{
  const char *c = arg;
  int n1 = 0;

  /* Get a number range (#x, #x-y, #x-, or #-y). First, read x. */
  if (isdigit(c[0]))
    n1 = strtol(arg, (char **)&c, 10);

  /* It really was a number range only if c is now at the end of the
     argument. */
  if (c[0] != 0) {
    Gif_Image *gfi = Gif_GetNamedImage(viewer->gfs, c);
    if (!gfi)
      error("no frame named %<%s%>\n", c);
    else
      n1 = Gif_ImageNumber(viewer->gfs, gfi);
  } else {
    if (n1 < 0 || n1 >= viewer->nim) {
      error("no frame number %d\n", n1);
      n1 = 0;
    }
  }

  return n1;
}


void
input_stream_done(Gt_Viewer *viewer, int first_frame)
{
  viewer->can_animate = Gif_ImageCount(viewer->gfs) > 1;
  switch_animating(viewer, animating && viewer->can_animate);
  if (first_frame < 0)
    first_frame = 0;
  view_frame(viewer, first_frame);
}


void
key_press(Gt_Viewer *viewer, XKeyEvent *e)
{
  char buf[32];
  KeySym key;
  int nbuf = XLookupString(e, buf, 32, &key, 0);
  if (nbuf > 1) buf[0] = 0;	/* ignore multikey sequences */

  if (key == XK_space || key == XK_F || key == XK_f || key == XK_N || key == XK_n)
    /* space, N or F: one frame ahead */
    view_frame(viewer, viewer->im_pos + 1);

  else if (key == XK_B || key == XK_b || key == XK_P || key == XK_p)
    /* B or P: one frame back */
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
      int pos = viewer->im_pos;
      if (viewer->im_pos >= viewer->nim - 1) {
	pos = 0;
	viewer->anim_loop = 0;
      }
      view_frame(viewer, pos);

    } else
      unschedule(viewer);

    set_viewer_name(viewer, -1);

  } else if (key == XK_U || key == XK_u) {
    /* U: toggle unoptimizing */
    int pos = viewer->im_pos;
    viewer->unoptimizing = !viewer->unoptimizing;
    if (!viewer->animating) {
      viewer->im_pos = -1;
      view_frame(viewer, pos);
      set_viewer_name(viewer, -1);
    }

  } else if (key == XK_R || key == XK_r
	     || (nbuf == 1 && buf[0] == '<')) {
    /* R or <: reset to first frame */
    unschedule(viewer);
    viewer->anim_loop = 0;
    view_frame(viewer, 0);

  } else if (nbuf == 1 && buf[0] == '>') {
    /* >: reset to last frame */
    unschedule(viewer);
    viewer->anim_loop = 0;
    view_frame(viewer, viewer->nim - 1);

  } else if (key == XK_Escape && viewer->animating) {
    /* Escape: stop animation */
    switch_animating(viewer, 0);
    unschedule(viewer);
    set_viewer_name(viewer, -1);

  } else if (key == XK_Z || key == XK_z) {
    /* Z: trigger resizability */
    viewer->resizable = !viewer->resizable;
  }
}


void
loop(void)
{
  struct timeval now, stop_loop, stop_delta;
  fd_set xfds;
  XEvent e;
  int pending;
  Gt_Viewer *v;
  Display *display = viewers->display;
  int x_socket = ConnectionNumber(display);
  stop_delta.tv_sec = 0;
  stop_delta.tv_usec = 200000;

  xwGETTIME(now);
  FD_ZERO(&xfds);

  while (viewers) {

    /* Check for any animations */
    /* 13.Feb.2001 - Use the 'pending' counter to avoid a tight loop
       if all the frames in an animation have delay 0. Reported by
       Franc,ois Petitjean. */
    /* 1.Aug.2002 - Switch to running the loop for max 0.2s. */
    xwADDTIME(stop_loop, now, stop_delta);
    while (animations && xwTIMEGEQ(now, animations->timer)
	   && xwTIMEGEQ(stop_loop, now)) {
      v = animations;
      animations = v->anim_next;
      v->scheduled = 0;
      if (v->preparing)
	prepare_frame(v, v->im_pos + 1);
      else {
	if (xwTIMEGEQ(now, v->timer))
	  v->timer = now;
	view_frame(v, v->im_pos + 1);
      }
      xwGETTIME(now);
    }

    pending = XPending(display);
    if (!pending) {
      /* select() until event arrives */
      struct timeval timeout, *timeout_ptr;
      int retval;

      if (animations) {
	xwSUBTIME(timeout, animations->timer, now);
	timeout_ptr = &timeout;
      } else
	timeout_ptr = 0;

      FD_SET(x_socket, &xfds);
      retval = select(x_socket + 1, &xfds, 0, 0, timeout_ptr);
      pending = (retval <= 0 ? 0 : FD_ISSET(x_socket, &xfds));
    }

    if (pending)
      while (XPending(display)) {
	XNextEvent(display, &e);
	v = find_viewer(e.xany.display, e.xany.window);
	if (v) {
	  if (interactive) {
	    if (e.type == ButtonPress && e.xbutton.button == 1)
	      /* Left mouse button: go to next frame */
	      view_frame(v, v->im_pos + 1);

      else if (e.type == ButtonPress && e.xbutton.button == 4)
        /* mousewheel forward */
        view_frame(v, v->im_pos + 1);

      else if (e.type == ButtonPress && e.xbutton.button == 5)
        /* mousewheel backward */
        view_frame(v, v->im_pos - 1);

	    else if (e.type == ButtonPress && e.xbutton.button == 3)
	      /* Right mouse button: delete window */
	      pre_delete_viewer(v);

	    else if (e.type == KeyPress)
	      /* Key press: call function */
	      key_press(v, &e.xkey);
	  }

	  if (e.type == ClientMessage
	      && e.xclient.message_type == wm_protocols_atom
	      && (Atom)(e.xclient.data.l[0]) == wm_delete_window_atom)
	    /* WM_DELETE_WINDOW message: delete window */
	    pre_delete_viewer(v);

	  else if (e.type == MapNotify && v->animating
		   && v->scheduled == 0)
	    /* Window was just mapped; now, start animating it */
	    schedule_next_frame(v);

	  else if (e.type == DestroyNotify)
	    /* Once the window has been destroyed, delete related state */
	    delete_viewer(v);
	}
      }

    xwGETTIME(now);
  }
}


int
main(int argc, char *argv[])
{
  Gt_Viewer *viewer = 0;
  int viewer_given = 0;
  int any_errors = 0;
  int first_frame = -1;

  clp = Clp_NewParser(argc, (const char * const *)argv,
                      sizeof(options) / sizeof(options[0]), options);
  Clp_SetOptionChar(clp, '+', Clp_ShortNegated);
  Clp_AddStringListType
    (clp, WINDOW_TYPE, Clp_AllowNumbers | Clp_StringListLong,
     "root", (long) -1,
     (const char*) 0);
  program_name = cur_resource_name = cur_window_title = Clp_ProgramName(clp);

  xwGETTIMEOFDAY(&genesis_time);
  preparation_time.tv_sec = 0;
  preparation_time.tv_usec = 200000;

  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {

     case DISPLAY_OPT:
      if (cur_display)
	fatal_error("%<--display%> must come before all other options\n");
      cur_display_name = clp->vstr;
      cur_display = 0;
      cur_arrow_cursor = cur_wait_cursor = None;
      break;

     case TITLE_OPT:
      cur_window_title = clp->vstr;
      break;

     case GEOMETRY_OPT:
      cur_geometry_spec = clp->vstr;
      break;

     case NAME_OPT:
      cur_resource_name = clp->vstr;
      break;

     case UNOPTIMIZE_OPT:
      unoptimizing = clp->negated ? 0 : 1;
      break;

     case BACKGROUND_OPT:
      cur_background_color = clp->vstr;
      break;

     case ANIMATE_OPT:
      animating = clp->negated ? 0 : 1;
      break;

     case INSTALL_COLORMAP_OPT:
      install_colormap = clp->negated ? 0 : 1;
      break;

     case WINDOW_OPT:
      cur_use_window = clp->val.ul;
      cur_use_window_new = 0;
      break;

     case NEW_WINDOW_OPT:
      cur_use_window = clp->val.ul;
      cur_use_window_new = 1;
      break;

     case INTERACTIVE_OPT:
      interactive = clp->negated ? 0 : 1;
      break;

    case MIN_DELAY_OPT:
      min_delay = clp->negated ? 0 : clp->val.i;
      break;

    case FALLBACK_DELAY_OPT:
      fallback_delay = clp->negated ? 0 : clp->val.i;
      break;

    case MEMORY_LIMIT_OPT:
      if (clp->negated || clp->val.u >= ((unsigned) -1 / 1000))
        pixel_memory_limit_kb = (unsigned) -1;
      else
        pixel_memory_limit_kb = clp->val.u * 1000;
      break;

     case VERSION_OPT:
      printf("gifview (LCDF Gifsicle) %s\n", VERSION);
      printf("Copyright (C) 1997-2019 Eddie Kohler\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;

     case HELP_OPT:
      usage();
      exit(0);
      break;

     case Clp_NotOption:
      if (clp->vstr[0] == '#') {
	if (!viewer_given) {
	  viewer = get_input_stream(0);
	  viewer_given = 1;
	}
	if (viewer && first_frame >= 0) {
	  /* copy viewer if 2 frame specs given */
	  input_stream_done(viewer, first_frame);
	  viewer = next_viewer(viewer->gfs, viewer->name);
	}
	if (viewer)
	  first_frame = frame_argument(viewer, clp->vstr + 1);
      } else {
        if (viewer) input_stream_done(viewer, first_frame);
	first_frame = -1;
        viewer = get_input_stream(clp->vstr);
	viewer_given = 1;
      }
      break;

     case Clp_Done:
      goto done;

     case Clp_BadOption:
      any_errors = 1;
      break;

     default:
      break;

    }
  }

 done:

  if (!viewer_given) {
    if (any_errors) {
      short_usage();
      exit(1);
    }
    viewer = get_input_stream(0);
  }
  if (viewer) input_stream_done(viewer, first_frame);

  if (viewers) loop();

  return 0;
}
