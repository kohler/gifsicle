/* Hand-edited file based on config.h */
/* config.h was generated automatically from config.h.in by configure */
/* config.h.in was generated automatically from configure.in by autoheader */
#ifndef CONFIG_H
#define CONFIG_H

/* Package and version */
#define PACKAGE "gifsicle"
#define VERSION "1.9.2-Windows"

/* Are we using the debugging malloc library? */
/* #undef DMALLOC */

/* What form of random() to use? */
#define RANDOM rand

/* How many arguments does gettimeofday() take? (gifview only) */
/* #undef GETTIMEOFDAY_PROTO */

/* Get the [u_]int*_t typedefs */
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
typedef signed __int16 int16_t;
typedef signed __int32 int32_t;

/* Pathname separator character ('/' on Unix) */
#define PATHNAME_SEPARATOR '\\'

/* Define this to write GIFs to stdout even when stdout is a terminal */
/* #undef OUTPUT_GIF_TO_TERMINAL */

/* Windows doesn't have popen, but it does have _popen */
#define popen _popen
#define pclose _pclose


/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if the X Window System is missing or not being used.  */
#define X_DISPLAY_MISSING 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

#ifdef __cplusplus
extern "C" {
#endif

/* Use either the clean-failing malloc library in fmalloc.c, or the debugging
   malloc library in dmalloc.c */
#ifdef DMALLOC
# include "dmalloc.h"
# define Gif_DeleteFunc		(&debug_free)
# define Gif_DeleteArrayFunc	(&debug_free)
#else
# define xmalloc(s)		fail_die_malloc((s),__FILE__,__LINE__)
# define xrealloc(p,s)		fail_die_realloc((p),(s),__FILE__,__LINE__)
# define xfree			free
void *fail_die_malloc(int, const char *, int);
void *fail_die_realloc(void *, int, const char *, int);
#endif

/* Prototype strerror() if we don't have it. */
#ifndef HAVE_STRERROR
char *strerror(int errno);
#endif

#ifdef __cplusplus
}
#endif
#endif
