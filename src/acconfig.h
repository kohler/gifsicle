/* Process this file with autoheader to produce config.h.in */
#ifndef CONFIG_H
#define CONFIG_H

/* Package and version */
#define PACKAGE "gifsicle"
#define VERSION "97"

/* Are we using the debugging malloc library? */
#undef DMALLOC

/* What form of random() to use? */
#undef RANDOM

/* How many arguments does gettimeofday() take? (gifview only) */
#undef GETTIMEOFDAY_PROTO

/* Get the [u_]int*_t typedefs */
#undef NEED_SYS_TYPES_H
#ifdef NEED_SYS_TYPES_H
# include <sys/types.h>
#endif
#undef u_int16_t
#undef u_int32_t
#undef int32_t

/* Pathname separator character ('/' on Unix) */
#define PATHNAME_SEPARATOR '/'

/* Define this to write GIFs to stdout even when stdout is a terminal */
#undef OUTPUT_GIF_TO_TERMINAL

@TOP@
@BOTTOM@

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
# include <stddef.h>
# define xmalloc(s)		fail_die_malloc((s),__FILE__,__LINE__)
# define xrealloc(p,s)		fail_die_realloc((p),(s),__FILE__,__LINE__)
# define xfree			free
void *fail_die_malloc(size_t, const char *, int);
void *fail_die_realloc(void *, size_t, const char *, int);
#endif

/* Prototype strerror() if we don't have it. */
#ifndef HAVE_STRERROR
char *strerror(int errno);
#endif

#ifdef __cplusplus
}
#endif
#endif
