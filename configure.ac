dnl Process this file with autoconf to produce a configure script.
AC_INIT([gifsicle], [1.92])
AC_CONFIG_SRCDIR([src/gifsicle.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
ac_user_cc=${CC+y}

AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CPP
AC_C_CONST
AC_C_INLINE

build_gifview=maybe
AC_ARG_ENABLE([gifview],
    [AS_HELP_STRING([--disable-gifview], [do not build gifview X11 viewer])],
    [if test "x$enableval" != xyes; then build_gifview=no; fi])

build_gifdiff=yes
AC_ARG_ENABLE([gifdiff],
    [AS_HELP_STRING([--disable-gifdiff], [do not build gifdiff comparison utility])],
    [if test "x$enableval" != xyes; then build_gifdiff=no; fi])

AC_ARG_ENABLE([warnings],
    [AS_HELP_STRING([--disable-warnings], [compile without -W -Wall])],
    [:], [enable_warnings=yes])
if test x$enable_warnings = xyes; then
    CC="$CC -W -Wall"
fi

WERROR=
AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror], [compile with -Werror])],
    [if test "x$enableval" = xyes; then WERROR="-Werror"; fi])
AC_SUBST([WERROR])

use_dmalloc=
AC_ARG_ENABLE([dmalloc],
    [AS_HELP_STRING([--enable-dmalloc], [build with debugging malloc library])],
    [if test "x$enableval" = xyes; then use_dmalloc=yes; fi])

ungif=
AC_ARG_ENABLE([ungif],
    [AS_HELP_STRING([--enable-ungif], [build without LZW compression])],
    [if test "x$enableval" = xyes; then ungif=yes; fi])


dnl
dnl Choose programs to build. Always build gifsicle
dnl

OTHERPROGRAMS=""
OTHERMANS=""

if test "x$build_gifview" = xyes -o "x$build_gifview" = xmaybe; then
    AC_PATH_XTRA
    if test "x$no_x" = xyes; then
        if test "x$build_gifview" = xyes; then
            AC_MSG_ERROR([
******************************************************************************
Cannot find X, but you explicitly requested that gifview be built.
You may need to install an X11 development package to get the X headers,
supply a '--with-x' option, or simply '--disable-gifview'.])
	      else
	          AC_MSG_WARN([Not compiling gifview since X is not available.])
	          build_gifview=no
	      fi
    else
	      build_gifview=yes
    fi
fi

if test "x$build_gifview" = xyes ; then
    OTHERPROGRAMS="$OTHERPROGRAMS gifview"'$(EXEEXT)'
    OTHERMANS="$OTHERMANS gifview.1"
    AC_CHECK_HEADERS([time.h sys/time.h])
    AC_CACHE_CHECK([for gettimeofday prototype], [ac_cv_gettimeofday],
        [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
            [[#include <time.h>
#include <sys/time.h>]],
            [[gettimeofday((void*) 0, (void*) 0)]])],
            [ac_cv_gettimeofday=2],
            [AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
                [[#include <time.h>
    #include <sys/time.h>]],
                [[gettimeofday((void*) 0)]])],
                [ac_cv_gettimeofday=1], [ac_cv_gettimeofday=0])])])
    AC_DEFINE_UNQUOTED([GETTIMEOFDAY_PROTO], [$ac_cv_gettimeofday], [Define to the number of arguments to gettimeofday.])
else
    AC_DEFINE_UNQUOTED(X_DISPLAY_MISSING, 1, [Define if X is not available.])
fi

if test "x$build_gifdiff" = xyes ; then
    OTHERPROGRAMS="$OTHERPROGRAMS gifdiff"'$(EXEEXT)'
    OTHERMANS="$OTHERMANS gifdiff.1"
fi

AC_SUBST(OTHERPROGRAMS)
AC_SUBST(OTHERMANS)


dnl
dnl Set up ungif support
dnl

if test "x$ungif" = xyes -o "x$GIFSICLE_UNGIF" = xyes ; then
  GIFWRITE_O=ungifwrt.o
  AC_DEFINE(GIF_UNGIF, 1, [Define if GIF LZW compression is off.])
elif test -r $srcdir/src/gifwrite.c ; then
  GIFWRITE_O=gifwrite.o
else
  AC_MSG_ERROR(This version of gifsicle cannot create compressed GIFs.
*** Either reconfigure with the \`--enable-ungif' option or download the
*** version with compression from http://www.lcdf.org/gifsicle/)
fi
AC_SUBST(GIFWRITE_O)

dnl
dnl random or rand, strerror, strtoul, mkstemp, sys/select.h
dnl

AC_CHECK_FUNC([random], [random_func=random], [random_func=rand])
AC_DEFINE_UNQUOTED(RANDOM, ${random_func}, [Define to a function that returns a random number.])

AC_REPLACE_FUNCS([strerror])
AC_CHECK_FUNCS([strtoul mkstemp])
AC_SEARCH_LIBS([pow], [m], [AC_DEFINE([HAVE_POW], [1], [Define to 1 if you have the `pow' function.])])

AC_CHECK_HEADERS([sys/select.h sys/stat.h inttypes.h unistd.h])


dnl
dnl integer types
dnl

AC_CHECK_HEADERS(inttypes.h, have_inttypes_h=yes, have_inttypes_h=no)
AC_CHECK_HEADERS(sys/types.h, have_sys_types_h=yes, have_sys_types_h=no)

if test $have_inttypes_h = no -a $have_sys_types_h = yes; then
    AC_CACHE_CHECK([for uintXX_t typedefs], ac_cv_uint_t,
    [AC_EGREP_HEADER(dnl
changequote(<<,>>)<<(^|[^a-zA-Z_0-9])uint32_t[^a-zA-Z_0-9]>>changequote([,]),
    sys/types.h, ac_cv_uint_t=yes, ac_cv_uint_t=no)])
fi
if test $have_inttypes_h = no -a $have_sys_types_h = yes -a "$ac_cv_uint_t" = no; then
    AC_CACHE_CHECK([for u_intXX_t typedefs], ac_cv_u_int_t,
    [AC_EGREP_HEADER(dnl
changequote(<<,>>)<<(^|[^a-zA-Z_0-9])u_int32_t[^a-zA-Z_0-9]>>changequote([,]),
    sys/types.h, ac_cv_u_int_t=yes, ac_cv_u_int_t=no)])
fi
if test $have_inttypes_h = yes -o "$ac_cv_uint_t" = yes; then
    :
elif test "$ac_cv_u_int_t" = yes; then
    AC_DEFINE([HAVE_U_INT_TYPES], [1], [Define if you have u_intXX_t types but not uintXX_t types.])
fi

AC_CHECK_TYPES([uintptr_t, int64_t, uint64_t], [], [],
[#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
])

AC_CHECK_SIZEOF([float])
AC_CHECK_SIZEOF([unsigned int])
AC_CHECK_SIZEOF([unsigned long])
AC_CHECK_SIZEOF([void *])


dnl
dnl vector types
dnl

AC_MSG_CHECKING([for usable vector_size vector types])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [AC_INCLUDES_DEFAULT
    [typedef float float4 __attribute__((vector_size (4 * sizeof(float))));]],
    [[float4 a;
    a[0] = a[1] = a[2] = a[3] = (float) rand();
    a *= (float) 3;
    printf("%f\n", a[0]);]])],
    [AC_DEFINE([HAVE_VECTOR_SIZE_VECTOR_TYPES], 1, [Define to 1 if `vector_size' vector types are usable.])
    ac_cv_vector_size_vector_types=yes], [ac_cv_vector_size_vector_types=no])
AC_MSG_RESULT([$ac_cv_vector_size_vector_types])

AC_MSG_CHECKING([for usable ext_vector_type vector types])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [AC_INCLUDES_DEFAULT
    [typedef float float4 __attribute__((ext_vector_type (4)));]],
    [[float4 a;
    a[0] = a[1] = a[2] = a[3] = (float) rand();
    a *= (float) 3;
    printf("%f\n", a[0]);]])],
    [AC_DEFINE([HAVE_EXT_VECTOR_TYPE_VECTOR_TYPES], 1, [Define to 1 if `ext_vector_type' vector types are usable.])
    ac_cv_ext_vector_type_vector_types=yes], [ac_cv_ext_vector_type_vector_types=no])
AC_MSG_RESULT([$ac_cv_ext_vector_type_vector_types])

AC_MSG_CHECKING([for __builtin_shufflevector])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [AC_INCLUDES_DEFAULT
    [typedef float float4 __attribute__((ext_vector_type (4)));]],
    [[float4 a;
    a[0] = a[1] = a[2] = a[3] = (float) rand();
    a = __builtin_shufflevector(a, a, 3, 2, 1, 0);
    printf("%f\n", a[0]);]])],
    [AC_DEFINE([HAVE___BUILTIN_SHUFFLEVECTOR], 1, [Define to 1 if you have the `__builtin_shufflevector' function.])
    ac_cv___builtin_shufflevector=yes], [ac_cv___builtin_shufflevector=no])
AC_MSG_RESULT([$ac_cv___builtin_shufflevector])

AC_ARG_ENABLE([simd],
    [AS_HELP_STRING([--disable-simd], [do not use SIMD instructions])],
    [], [enable_simd=maybe])

if test $ac_cv_vector_size_vector_types = no -a $ac_cv_ext_vector_type_vector_types = no; then
    if test x$enable_simd = xmaybe; then
        AC_MSG_WARN([
Not using SIMD since your compiler's support for SIMD types is inadequate.
])
    elif test x$enable_simd != xno; then
        AC_MSG_ERROR([
******************************************************************************
`--enable-simd' was supplied, but this compiler does not support SIMD types.])
    fi
    enable_simd=no
fi
if test x"$enable_simd" != xno; then
    AC_DEFINE([HAVE_SIMD], [1], [Define to 1 if SIMD types should be used.])
fi


dnl
dnl threads
dnl

AC_SEARCH_LIBS([pthread_create], [pthread], [have_threads=yes], [have_threads=no])
AC_MSG_CHECKING([for __sync_add_and_fetch])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [AC_INCLUDES_DEFAULT
    extern volatile int x;],
    [[printf("%d", __sync_add_and_fetch(&x, 1));]])],
    [AC_DEFINE([HAVE___SYNC_ADD_AND_FETCH], 1, [Define to 1 if you have the `__sync_add_and_fetch' function.])
    result=yes], [result=no])
AC_MSG_RESULT([$result])

AC_ARG_ENABLE([threads],
    [AS_HELP_STRING([--disable-threads], [do not include multithreading])],
    [], [enable_threads=maybe])

if test "x$have_threads" = xno -a "x$enable_threads" = xyes; then
    AC_MSG_ERROR([
******************************************************************************
Threading support is not available on this platform.])
elif test "x$have_threads" = xyes -a "x$enable_threads" != xno; then
    AC_DEFINE([ENABLE_THREADS], [1], [Define to 1 if multithreading support is available.])
fi


dnl
dnl verbatim portions of the header
dnl

AC_DEFINE_UNQUOTED(PATHNAME_SEPARATOR, ['/'], [Pathname separator character ('/' on Unix).])
if false; then
  AC_DEFINE(OUTPUT_GIF_TO_TERMINAL, 1, [Define to write GIFs to stdout even when stdout is a terminal.])
fi

AH_TOP([#ifndef GIFSICLE_CONFIG_H
#define GIFSICLE_CONFIG_H])

AH_BOTTOM([#include <stddef.h>

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
# define snprintf _snprintf
#endif

#endif /* GIFSICLE_CONFIG_H */])


dnl
dnl Output
dnl

AC_OUTPUT(Makefile src/Makefile)
