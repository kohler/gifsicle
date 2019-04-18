## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = foreign check-news

bin_PROGRAMS = gifsicle @OTHERPROGRAMS@
EXTRA_PROGRAMS = gifview gifdiff

LDADD = @LIBOBJS@
gifsicle_LDADD = $(LDADD) @GIFWRITE_O@
gifview_LDADD = $(LDADD) @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@

gifsicle_DEPENDENCIES = @GIFWRITE_O@ @LIBOBJS@
gifview_DEPENDENCIES = @LIBOBJS@
gifdiff_DEPENDENCIES = @LIBOBJS@

WERROR = @WERROR@

gifsicle_SOURCES = clp.c fmalloc.c \
		giffunc.c gifread.c gifunopt.c \
		gifsicle.h kcolor.h \
		merge.c optimize.c quantize.c support.c xform.c \
		gifsicle.c

gifview_SOURCES = clp.c fmalloc.c \
		giffunc.c gifread.c gifx.c \
		gifview.c

gifdiff_SOURCES = clp.c fmalloc.c \
		giffunc.c gifread.c \
		gifdiff.c

AM_CPPFLAGS = $(X_CFLAGS) $(WERROR) -I$(top_srcdir)/include

EXTRA_gifsicle_SOURCES = gifwrite.c ungifwrt.c

EXTRA_DIST = opttemplate.c Makefile.bcc Makefile.w32 win32cfg.h
