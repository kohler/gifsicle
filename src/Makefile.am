## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = foreign check-news

bin_PROGRAMS = gifsicle @OTHERPROGRAMS@
EXTRA_PROGRAMS = gifview gifdiff

LDADD = @MALLOC_O@ @LIBOBJS@
gifsicle_LDADD = $(LDADD) @GIFWRITE_O@
gifview_LDADD = $(LDADD) @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@

gifsicle_DEPENDENCIES = @GIFWRITE_O@ @MALLOC_O@ @LIBOBJS@
gifview_DEPENDENCIES = @MALLOC_O@ @LIBOBJS@
gifdiff_DEPENDENCIES = @MALLOC_O@ @LIBOBJS@

gifsicle_SOURCES = clp.c \
		giffunc.c gifread.c gifunopt.c \
		gifsicle.h merge.c optimize.c quantize.c support.c xform.c \
		gifsicle.c

gifview_SOURCES = clp.c \
		giffunc.c gifread.c gifx.c \
		gifview.c

gifdiff_SOURCES = clp.c \
		giffunc.c gifread.c \
		gifdiff.c

INCLUDES = $(X_CFLAGS) -I$(top_srcdir)/include

EXTRA_DIST = dmalloc.h dmalloc.c fmalloc.c gifwrite.c ungifwrt.c
