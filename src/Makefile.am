## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = no-dependencies foreign check-news

bin_PROGRAMS = gifsicle @OTHERPROGRAMS@
EXTRA_PROGRAMS = gifview gifdiff
man_MANS = gifsicle.1 @OTHERMANS@

LDADD = @MALLOC_O@ @LIBOBJS@
gifview_LDADD = $(LDADD) @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@

gifsicle_DEPENDENCIES = @MALLOC_O@ @LIBOBJS@
gifview_DEPENDENCIES = @MALLOC_O@ @LIBOBJS@
gifdiff_DEPENDENCIES = @MALLOC_O@ @LIBOBJS@

gifsicle_SOURCES = clp.h clp.c \
		gif.h giffunc.c gifread.c gifunopt.c gifwrite.c \
		gifsicle.h merge.c optimize.c quantize.c support.c xform.c \
		gifsicle.c

gifview_SOURCES = clp.h clp.c \
		gif.h giffunc.c gifread.c gifunopt.c gifx.h gifx.c \
		gifview.c

gifdiff_SOURCES = clp.h clp.c \
		gif.h giffunc.c gifread.c \
		gifdiff.c

INCLUDES = $(X_CFLAGS)

EXTRA_DIST = dmalloc.h dmalloc.c fmalloc.c \
		gifsicle.1 gifview.1 gifdiff.1 \
		logo.gif logo1.gif rpm.spec \
		Makefile.w32 win32cfg.h

clp.o: clp.h clp.c

fmalloc.o: fmalloc.c
dmalloc.o: dmalloc.h dmalloc.c

giffunc.o: gif.h giffunc.c
gifread.o: gif.h gifread.c
gifwrite.o: gif.h gifwrite.c
gifunopt.o: gif.h gifunopt.c
gifx.o: gif.h gifx.h gifx.c

merge.o: gifsicle.h merge.c
optimize.o: gifsicle.h optimize.c
quantize.o: gifsicle.h quantize.c
support.o: gifsicle.h support.c
xform.o: gifsicle.h xform.c
gifsicle.o: gifsicle.h gifsicle.c

gifview.o: gifview.c
gifdiff.o: gifdiff.c

srclinks:
	for i in clp.c clp.h fmalloc.c strerror.c ; do \
	ln -sf ~/src/edlib/$$i $$i ; done
	for i in gif.h giffunc.c gifread.c gifwrite.c gifunopt.c \
	gifx.h gifx.c giftoc.c ; do \
	ln -sf ~/src/giflib/$$i $$i ; done

versionize:
	perl -pi -e "s/^\\.ds V.*/.ds V $(VERSION)/;" gifsicle.1 gifview.1 gifdiff.1
	perl -pi -e "s/^Version: .*/Version: $(VERSION)/; s/$(PACKAGE)-[\w.]+\.tar\.gz/$(PACKAGE)-$(VERSION).tar.gz/;" rpm.spec
	perl -pi -e "s/VERSION \"[\w.]+/VERSION \"$(VERSION)/;" win32cfg.h

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
