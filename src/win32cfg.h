/* Hand-edited file based on config.h.in */
/* config.h.in.  Generated from configure.ac by autoheader.  */

#ifndef GIFSICLE_CONFIG_H
#define GIFSICLE_CONFIG_H

/* Define to the number of arguments to gettimeofday (gifview only). */
/* #undef GETTIMEOFDAY_PROTO */

/* Define if GIF LZW compression is off. */
/* #undef GIF_UNGIF */

/* Define to 1 if you have the <inttypes.h> header file. */
/* #undef HAVE_INTTYPES_H */

/* Define to 1 if you have the <memory.h> header file. */
/* #undef HAVE_MEMORY_H */

/* Define to 1 if you have the `mkstemp' function. */
/* #undef HAVE_MKSTEMP */

/* Define to 1 if you have the <stdint.h> header file. */
/* #undef HAVE_STDINT_H */

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the <sys/select.h> header file. */
/* #undef HAVE_SYS_SELECT_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
/* #undef HAVE_SYS_STAT_H */

/* Define to 1 if you have the <sys/types.h> header file. */
/* #undef HAVE_SYS_TYPES_H */

/* Define to 1 if the system has the type `uintptr_t'. */
/* #undef HAVE_UINTPTR_T */

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Define if you have u_intXX_t types but not uintXX_t types. */
/* #undef HAVE_U_INT_TYPES */

/* Define to write GIFs to stdout even when stdout is a terminal. */
/* #undef OUTPUT_GIF_TO_TERMINAL */

/* Name of package */
#define PACKAGE "gifsicle"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "gifsicle"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "gifsicle 1.82"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "gifsicle"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.82"

/* Pathname separator character ('/' on Unix). */
#define PATHNAME_SEPARATOR '\\'

/* Define to a function that returns a random number. */
#define RANDOM rand

/* The size of `unsigned int', as computed by sizeof. */
#define SIZEOF_UNSIGNED_INT 4

/* The size of `unsigned long', as computed by sizeof. */
#ifdef _WIN64
#define SIZEOF_UNSIGNED_LONG 8
#else
#define SIZEOF_UNSIGNED_LONG 4
#endif

/* The size of `void *', as computed by sizeof. */
#ifdef _WIN64
#define SIZEOF_VOID_P 8
#else
#define SIZEOF_VOID_P 4
#endif

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1.82 (Windows)"

/* Define if X is not available. */
#define X_DISPLAY_MISSING 1

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
# ifndef inline
#  define inline __inline
# endif
#endif

/* Windows doesn't have popen, but it does have _popen. */
#define popen _popen
#define pclose _pclose
#define snprintf _snprintf

/* Fixes redefinition error in giffunc.c */
#define GIF_ALLOCATOR_DEFINED 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Use either the clean-failing malloc library in fmalloc.c, or the debugging
   malloc library in dmalloc.c. */
#define xmalloc(s)		fail_die_malloc((s),__FILE__,__LINE__)
#define xrealloc(p,s)		fail_die_realloc((p),(s),__FILE__,__LINE__)
#define xfree			free
void *fail_die_malloc(size_t, const char *, int);
void *fail_die_realloc(void *, size_t, const char *, int);

/* Prototype strerror if we don't have it. */
#ifndef HAVE_STRERROR
char *strerror(int errno);
#endif

#ifdef __cplusplus
}
/* Get rid of a possible inline macro under C++. */
# define inline inline
#endif

#endif /* GIFSICLE_CONFIG_H */
