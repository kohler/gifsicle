/* gifview.c - gifview's main loop.
   Copyright (C) 1997 Eddie Kohler, eddietwo@lcs.mit.edu
   This file is part of gifview.

   Gifview is free software; you can copy, distribute, or alter it at will, as
   long as this notice is kept intact and this source code is made available.
   Hypo(pa)thetical commerical developers are asked to write the author a note,
   which might make his day. There is no warranty, express or implied. */

#include "gifx.h"
#include "clp.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>


char *programname = "gifview";

char *display_name = 0;
Display *display;
Window window = None;

static Gif_Stream *input = 0;
static int frames_done = 0;
static int files_given = 0;
static char *input_name = 0;

static Gif_Colormap *use_colormap = 0;
static char *use_colormap_name = 0;
static int use_colormap_number = -1;

static int unoptimizing = 0;


#define COLORMAP_OPT	'c'
#define UNOPTIMIZE_OPT	'U'

Clp_Option options[] = {
  { "colormap", 'c', COLORMAP_OPT, Clp_ArgString, Clp_Negate },
  { "display", 'd', 'd', Clp_ArgString, 0 },
  { "use-colormap", 'c', COLORMAP_OPT, Clp_ArgString, Clp_Negate },
  { "unoptimize", 'U', UNOPTIMIZE_OPT, 0, Clp_Negate },
};


void
fatalerror(char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: ", programname);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
  exit(1);
}


void
error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: ", programname);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
}


void
warning(char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: warning: ", programname);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
}


void
createwindow(int w, int h)
{
  char *stringlist[2];
  XTextProperty windowname, iconname;
  XClassHint classh;
  
  /* Open the display and create the window */
  display = XOpenDisplay(display_name);
  if (!display) fatalerror("can't open display");
  window = XCreateSimpleWindow
    (display, DefaultRootWindow(display),
     0, 0, w, h, 0, 1, 0);

  /* Set the window's title and class (for window manager resources) */
  stringlist[0] = "gifview";
  stringlist[1] = NULL;
  XStringListToTextProperty(stringlist, 1, &windowname);
  XStringListToTextProperty(stringlist, 1, &iconname);
  classh.res_name = "gifview";
  classh.res_class = "Gifview";
  XSetWMProperties(display, window, &windowname, &iconname,
		   NULL, 0, NULL, NULL, &classh);
  
  /* We can hear buttons and that's it */
  XSelectInput(display, window, ButtonPressMask);
}


void
makewindow(Gif_Stream *gfs, Gif_Image *gfi)
{
  static int owidth = -1;
  static int oheight = -1;
  int width = Gif_ImageWidth(gfi);
  int height = Gif_ImageHeight(gfi);
  
  if (!window)
    createwindow(width, height);
  else {
    if (owidth != width || oheight != height) {
      XWindowChanges winch;
      winch.width = width;
      winch.height = height;
      XReconfigureWMWindow
	(display, window, DefaultScreen(display),
	 CWWidth | CWHeight, &winch);
    }
  }
  
  owidth = width;
  oheight = height;
}


static void
get_use_colormap()
{
  if (!input) return;
  use_colormap = 0;
  if (use_colormap_name) {
    use_colormap = Gif_GetNamedColormap(input, use_colormap_name);
    if (!use_colormap)
      error("can't find colormap `%s'", use_colormap_name);
  } else if (use_colormap_number >= 0) {
    use_colormap = Gif_GetColormap(input, use_colormap_number);
    if (!use_colormap)
      error("can't find colormap %d", use_colormap_number);
  }
}


static void
get_input_stream(char *name)
{
  FILE *f;
  Gif_Stream *gfs;
  
  input = 0;
  frames_done = 0;
  input_name = 0;
  files_given++;
  
  if (!name || strcmp(name, "-") == 0)
    f = stdin;
  else
    f = fopen(name, "rb");
  if (!f) {
    error("can't open `%s'", name);
    return;
  }
  
  gfs = Gif_ReadFile(f);
  fclose(f);
  
  if (!gfs || Gif_ImageCount(gfs) == 0) {
    error("`%s' doesn't seem to contain a GIF", name ? name : "stdin");
    return;
  }
  
  input_name = name;
  input = gfs;
  if (unoptimizing)
    Gif_Unoptimize(gfs);
  get_use_colormap();
}


void
show_frame(int imagenumber, char *imagename)
{
  Gif_Image *gfi;
  Pixmap pixmap;
  
  static Gif_XContext *gfx = 0;
  static Pixmap oldpixmap = None;
  
  if (!input) return;
  
  if (imagename) {
    gfi = Gif_GetNamedImage(input, imagename);
    if (!gfi) error("no frame named `%s'", imagename);
  } else {
    gfi = Gif_GetImage(input, imagenumber);
    if (!gfi) error("no frame number %d", imagenumber);
  }
  if (!gfi) return;
  
  makewindow(input, gfi);
  
  if (!gfx) {
    gfx = Gif_NewXContext(display, window);
    gfx->transparentvalue = 3;
  }
  pixmap = Gif_XImageColormap(gfx, input, use_colormap, gfi);
  
  XSetWindowBackgroundPixmap(display, window, pixmap);
  if (!oldpixmap) /* first image; map the window */
    XMapRaised(display, window);
  else {
    XClearWindow(display, window);
    XFreePixmap(display, oldpixmap);
  }
  
  while (1) {
    XEvent e;
    XNextEvent(display, &e);
    if (e.type == ButtonPress) break;
  }
  
  oldpixmap = pixmap;
}


void
frame_argument(char *arg)
{
  char *c = arg;
  int n1 = 0;
  int n2 = -1;
  
  if (!input && !input_name) {
    warning("no file specified; assuming stdin");
    get_input_stream(0);
  }
  if (!input) return;
  
  /* Get a number range (#x, #x-y, #x-, or #-y). First, read x. */
  if (isdigit(c[0]))
    n1 = strtol(arg, &c, 10);
  
  /* Then, if the next character is a dash, read y. */
  if (c[0] == '-') {
    c++;
    if (isdigit(c[0]))
      n2 = strtol(c, &c, 10);
    else
      n2 = Gif_ImageCount(input) - 1;
  }
  
  /* It really was a number range only if c is now at the end of the
     argument. */
  if (c[0] != 0)
    show_frame(0, arg);
  
  else if (n2 == -1)
    show_frame(n1, 0);
  
  else
    for (; n1 <= n2; n1++)
      show_frame(n1, 0);
}


void
input_stream_done()
{
  if (!input) return;
  
  if (!frames_done) {
    int i;
    for (i = 0; i < Gif_ImageCount(input); i++)
      show_frame(i, 0);
  }
  
  Gif_DeleteStream(input);
  input = 0;
}


int
main(int argc, char **argv)
{
  int opt;
  
  Clp_Parser *clp =
    Clp_NewParser(argc, argv,
                  sizeof(options) / sizeof(options[0]), options);

  
  while (1) {
    opt = Clp_Next(clp);
    switch (opt) {

     case COLORMAP_OPT:
      if (clp->negated) {
        use_colormap = 0;
        use_colormap_name = 0;
        use_colormap_number = -1;
      } else {
        char *c;
        use_colormap_number = strtol(clp->arg, &c, 10);
        if (*c != 0)
          use_colormap_name = clp->arg;
        get_use_colormap();
      }
      break;

     case 'd':
      display_name = clp->arg;
      break;

     case UNOPTIMIZE_OPT:
      unoptimizing = clp->negated ? 0 : 1;
      break;
      
     case Clp_NotOption:
      if (clp->arg[0] == '#')
        frame_argument(clp->arg + 1);
      else {
        input_stream_done();
        get_input_stream(clp->arg);
      }
      break;
      
     case Clp_Done:
      goto done;
      
     default:
      break;

    }
  }
  
 done:
  
  if (!files_given)
    get_input_stream(0);
  input_stream_done();
  return 0;
}
