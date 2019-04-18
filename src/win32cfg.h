/* Hand-edited file based on config.h.in */
/* config.h.in.  Generated from configure.ac by autoheader.  */

#ifndef GIFSICLE_CONFIG_H
#define GIFSICLE_CONFIG_H

/* Define to 1 if multithreading support is available. */
/* #undef ENABLE_THREADS */

/* Define to the number of arguments to gettimeofday (gifview only). */
/* #undef GETTIMEOFDAY_PROTO */

/* Define if GIF LZW compression is off. */
/* #undef GIF_UNGIF */

/* Define to 1 if `ext_vector_type' vector types are usable. */
/* #undef HAVE_EXT_VECTOR_TYPE_VECTOR_TYPES */

/* Define to 1 if the system has the type `int64_t'. */
/* #undef HAVE_INT64_T */

/* Define to 1 if you have the <inttypes.h> header file. */
#if defined(_MSC_VER) && _MSC_VER >= 1900
# define HAVE_INTTYPES_H 1
#endif

/* Define to 1 if you have the <memory.h> header file. */
/* #undef HAVE_MEMORY_H */

/* Define to 1 if you have the `mkstemp' function. */
/* #undef HAVE_MKSTEMP */

/* Define to 1 if you have the `pow' function. */
#define HAVE_POW 1

/* Define to 1 if SIMD types should be used. */
/* #undef HAVE_SIMD */

/* Define to 1 if you have the <stdint.h> header file. */
#if defined(_MSC_VER) && _MSC_VER >= 1900
# define HAVE_STDINT_H 1
#endif

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

/* Define to 1 if you have the <sys/time.h> header file. */
/* #undef HAVE_SYS_TIME_H */

/* Define to 1 if you have the <sys/types.h> header file. */
/* #undef HAVE_SYS_TYPES_H */

/* Define to 1 if you have the <time.h> header file. */
/* #undef HAVE_TIME_H */

/* Define to 1 if the system has the type `uint64_t'. */
/* #undef HAVE_UINT64_T */

/* Define to 1 if the system has the type `uintptr_t'. */
#if defined(_MSC_VER) && _MSC_VER >= 1900
# define HAVE_UINTPTR_T 1
#endif

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Define if you have u_intXX_t types but not uintXX_t types. */
/* #undef HAVE_U_INT_TYPES */

/* Define to 1 if `vector_size' vector types are usable. */
/* #undef HAVE_VECTOR_SIZE_VECTOR_TYPES */

/* Define to 1 if you have the `__builtin_shufflevector' function. */
/* #undef HAVE___BUILTIN_SHUFFLEVECTOR */

/* Define to 1 if you have the `__sync_add_and_fetch' function. */
/* #undef HAVE___SYNC_ADD_AND_FETCH */

/* Define to write GIFs to stdout even when stdout is a terminal. */
/* #undef OUTPUT_GIF_TO_TERMINAL */

/* Name of package */
#define PACKAGE "gifsicle"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "gifsicle"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "gifsicle 1.92"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "gifsicle"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.92"

/* Pathname separator character ('/' on Unix). */
#define PATHNAME_SEPARATOR '\\'

/* Define to a function that returns a random number. */
#define RANDOM rand

/* The size of `float', as computed by sizeof. */
#define SIZEOF_FLOAT 4

/* The size of `unsigned int', as computed by sizeof. */
#define SIZEOF_UNSIGNED_INT 4

/* The size of `unsigned long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG 4

/* The size of `void *', as computed by sizeof. */
#ifdef _WIN64
#define SIZEOF_VOID_P 8
#else
#define SIZEOF_VOID_P 4
#endif

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "1.92 (Windows)"

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

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Use the clean-failing malloc library in fmalloc.c. */
#define GIF_ALLOCATOR_DEFINED   1
#define Gif_Free free

/* Prototype strerror if we don't have it. */
#ifndef HAVE_STRERROR
char *strerror(int errno);
#endif

#ifdef __cplusplus
}
/* Get rid of a possible inline macro under C++. */
# define inline inline
#endif

/* Need _setmode under MS-DOS, to set stdin/stdout to binary mode */
/* Need _fsetmode under OS/2 for the same reason */
/* Windows has _isatty and _snprintf, not the normal versions */
#if defined(_MSDOS) || defined(_WIN32) || defined(__EMX__) || defined(__DJGPP__)
# include <fcntl.h>
# include <io.h>
# define isatty _isatty
# if defined(_MSC_VER) && _MSC_VER < 1900
#  define snprintf _snprintf
# endif
#endif

#endif /* GIFSICLE_CONFIG_H */
