## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = no-dependencies foreign check-news

bin_PROGRAMS = gifsicle @OTHERPROGRAMS@
EXTRA_PROGRAMS = gifview gifdiff
man_MANS = gifsicle.1 @OTHERMANS@

LDADD = @MALLOC_O@ @STRERROR_O@
gifview_LDADD = $(LDADD) @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@

gifsicle_DEPENDENCIES = @MALLOC_O@ @STRERROR_O@
gifview_DEPENDENCIES = @MALLOC_O@ @STRERROR_O@
gifdiff_DEPENDENCIES = @MALLOC_O@ @STRERROR_O@

gifsicle_SOURCES = clp.c clp.h \
		giffunc.c gifread.c gifunopt.c gifwrite.c \
		merge.c optimize.c quantize.c support.c xform.c \
		gifsicle.c gifsicle.h

gifview_SOURCES = clp.c clp.h \
		giffunc.c gifread.c gifunopt.c gifx.c gifx.h \
		gifview.c

gifdiff_SOURCES = clp.c clp.h \
		giffunc.c gifread.c \
		gifdiff.c

INCLUDES = $(X_CFLAGS)

EXTRA_DIST = README.win \
		dmalloc.c dmalloc.h fmalloc.c strerror.c \
		gifsicle.1 gifview.1 gifdiff.1 \
		logo.gif logo1.gif rpm.spec

clp.o: clp.c clp.h

giffunc.o: giffunc.c gif.h
gifread.o: gifread.c gif.h
gifwrite.o: gifwrite.c gif.h
gifunopt.o: gifunopt.c gif.h
gifx.o: gifx.c gifx.h gif.h

gifsicle.o: gifsicle.c gifsicle.h
merge.o: merge.c gifsicle.h
support.o: support.c gifsicle.h
quantize.o: quantize.c gifsicle.h
optimize.o: optimize.c gifsicle.h

gifview.o: gifview.c
gifdiff.o: gifdiff.c

GIF_MEMORY_DEFINES =
INTEGER_TYPES =

srclinks:
	for i in clp.c clp.h ; do \
	ln -sf ~/src/edlib/$$i $$i ; done
	for i in gif.h.in giffunc.c gifread.c gifwrite.c gifunopt.c \
	gifx.h gifx.c giftoc.c ; do \
	ln -sf ~/src/giflib/$$i $$i ; done

versionize:
	perl -pi~ -e "s/^\\.ds V.*/.ds V $(VERSION)/;" gifsicle.1 gifview.1 gifdiff.1
	perl -pi~ -e "s/^Version: .*/Version: $(VERSION)/; s/$(PACKAGE)-[\w.]+\.tar\.gz/$(PACKAGE)-$(VERSION).tar.gz/;" rpm.spec

rpm: dist
	buildarch=`rpm --showrc | awk '/^build arch/ { print $$4; }'` ; \
	mkdir -p /tmp/rpmb/SOURCES /tmp/rpmb/RPMS/$$buildarch \
	/tmp/rpmb/BUILD ; \
	echo 'topdir: /tmp/rpmb' > /tmp/rpmb/rc ; \
	cp logo1.gif $(PACKAGE)-$(VERSION).tar.gz /tmp/rpmb/SOURCES ; \
	rpm --rcfile /tmp/rpmb/rc -bb rpm.spec ; \
	cp /tmp/rpmb/RPMS/$$buildarch/*.rpm .
	rm -rf /tmp/rpmb

.PHONY: srclinks versionize rpm
