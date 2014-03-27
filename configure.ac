dnl Process this file with autoconf to produce a configure script.
AC_INIT([gifsicle], [1.82])
AC_CONFIG_SRCDIR([src/gifsicle.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CPP
AC_C_CONST
AC_C_INLINE

build_gifview=maybe
AC_ARG_ENABLE(gifview,
[  --disable-gifview       do not build gifview, a GIF viewer for X11],
if test "x$enableval" != xyes ; then
  build_gifview=no
fi)

build_gifdiff=yes
AC_ARG_ENABLE(gifdiff,
[  --disable-gifdiff       do not build gifdiff, a utility for comparing GIFs],
if test "x$enableval" != xyes ; then
  build_gifdiff=no
fi)

use_dmalloc=
AC_ARG_ENABLE(dmalloc,
[  --enable-dmalloc        build with debugging malloc library],
if test "x$enableval" = xyes ; then
  use_dmalloc=yes
fi)

AC_ARG_ENABLE(warnings,
[  --enable-warnings       compile with -W -Wall],
if test "x$enableval" = xyes ; then
  CC="$CC -W -Wall"
fi)

AC_ARG_ENABLE([werror],
[  --enable-werror         compile with -Werror],
if test "x$enableval" = xyes ; then
  WERROR="-Werror"
fi)
AC_SUBST([WERROR])

AC_ARG_ENABLE(all,
[  --enable-all            --enable-gifview --enable-gifdiff --enable-warnings],
if test "x$enableval" = xyes ; then
  build_gifview=yes
  build_gifdiff=yes
  CC="$CC -W -Wall"
fi)

ungif=
AC_ARG_ENABLE(ungif,
[  --enable-ungif          build without compression],
if test "x$enableval" = xyes ; then
  ungif=yes
fi)

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
  AC_CACHE_CHECK(for gettimeofday prototype, ac_cv_gettimeofday,
[AC_TRY_COMPILE([#include <time.h>
#include <sys/time.h>],
[gettimeofday((void *)0, (void *)0);],
[AC_TRY_COMPILE([#include <time.h>
#include <sys/time.h>],
[gettimeofday((void *)0);],
ac_cv_gettimeofday=0,
ac_cv_gettimeofday=2)],
ac_cv_gettimeofday=1)])
  AC_DEFINE_UNQUOTED(GETTIMEOFDAY_PROTO, $ac_cv_gettimeofday, [Define to the number of arguments to gettimeofday (gifview only).])
else
  AC_DEFINE_UNQUOTED(X_DISPLAY_MISSING, 1, [Define if X is not available.])
fi

if test "x$build_gifdiff" = xyes ; then
  OTHERPROGRAMS="$OTHERPROGRAMS gifdiff"'$(EXEEXT)'
  OTHERMANS="$OTHERMANS gifdiff.1"
fi

AC_SUBST(OTHERPROGRAMS)dnl
AC_SUBST(OTHERMANS)dnl


dnl
dnl Set up `ungif' support
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

AC_CHECK_HEADERS([sys/select.h inttypes.h unistd.h])


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

AC_CHECK_TYPES([uintptr_t], [], [],
[#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
])

AC_CHECK_SIZEOF([unsigned int])
AC_CHECK_SIZEOF([unsigned long])
AC_CHECK_SIZEOF([void *])


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

#endif /* GIFSICLE_CONFIG_H */])


dnl
dnl Output
dnl

AC_OUTPUT(Makefile src/Makefile)
