## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = no-dependencies foreign check-news

bin_PROGRAMS = gifsicle @OTHERPROGRAMS@
EXTRA_PROGRAMS = gifview gifdiff
man_MANS = gifsicle.1 @OTHERMANS@

LDADD = @MALLOC_O@ @LIBOBJS@
gifsicle_LDADD = $(LDADD) @GIFWRITE_O@
gifview_LDADD = $(LDADD) @X_LIBS@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@

gifsicle_DEPENDENCIES = @GIFWRITE_O@ @MALLOC_O@ @LIBOBJS@
gifview_DEPENDENCIES = @MALLOC_O@ @LIBOBJS@
gifdiff_DEPENDENCIES = @MALLOC_O@ @LIBOBJS@

gifsicle_SOURCES = clp.h clp.c \
		gif.h giffunc.c gifread.c gifunopt.c \
		gifsicle.h merge.c optimize.c quantize.c support.c xform.c \
		gifsicle.c

gifview_SOURCES = clp.h clp.c \
		gif.h giffunc.c gifread.c gifx.h gifx.c \
		gifview.c

gifdiff_SOURCES = clp.h clp.c \
		gif.h giffunc.c gifread.c \
		gifdiff.c

INCLUDES = $(X_CFLAGS)

EXTRA_DIST = dmalloc.h dmalloc.c fmalloc.c \
		gifwrite.c ungifwrt.c \
		gifsicle.1 gifview.1 gifdiff.1 \
		logo.gif logo1.gif rpm.spec \
		Makefile.w32 win32cfg.h

clp.o: clp.h clp.c config.h

fmalloc.o: fmalloc.c config.h
dmalloc.o: dmalloc.h dmalloc.c config.h

giffunc.o: gif.h giffunc.c config.h
gifread.o: gif.h gifread.c config.h
gifwrite.o: gif.h gifwrite.c config.h
ungifwrt.o: gif.h ungifwrt.c config.h
gifunopt.o: gif.h gifunopt.c config.h
gifx.o: gif.h gifx.h gifx.c config.h

merge.o: gifsicle.h merge.c config.h
optimize.o: gifsicle.h optimize.c config.h
quantize.o: gifsicle.h quantize.c config.h
support.o: gifsicle.h support.c config.h
xform.o: gifsicle.h xform.c config.h
gifsicle.o: gifsicle.h gifsicle.c config.h

gifview.o: gifview.c config.h
gifdiff.o: gifdiff.c config.h

srclinks:
	for i in clp.c clp.h fmalloc.c strerror.c ; do \
	ln -sf $(HOME)/src/edlib/$$i $$i ; done
	for i in gif.h giffunc.c gifread.c gifwrite.c ungifwrt.c gifunopt.c \
	gifx.h gifx.c giftoc.c ; do \
	ln -sf $(HOME)/src/giflib/$$i $$i ; done

versionize:
	perl -pi -e "s/^\\.ds V.*/.ds V $(VERSION)/;" gifsicle.1 gifview.1 gifdiff.1
	perl -pi -e "s/^Version: .*/Version: $(VERSION)/; s/$(PACKAGE)-[\w.]+\.tar\.gz/$(PACKAGE)-$(VERSION).tar.gz/;" rpm.spec
	perl -pi -e "s/VERSION \"[\w.]+/VERSION \"$(VERSION)/;" win32cfg.h

rpm: dist
	buildarch=`rpm --showrc | awk '/^build arch/ { print $$4; }'` ; \
	mkdir -p /tmp/rpm-gfs/SOURCES /tmp/rpm-gfs/RPMS/$$buildarch \
	/tmp/rpm-gfs/BUILD ; \
	echo 'include: /usr/lib/rpm/rpmrc' > /tmp/rpm-gfs/rc ; \
	echo 'macrofiles: /usr/lib/rpm/macros:/tmp/rpm-gfs/macros' >> /tmp/rpm-gfs/rc ; \
	echo '%_topdir /tmp/rpm-gfs' > /tmp/rpm-gfs/macros ; \
	cp logo1.gif $(PACKAGE)-$(VERSION).tar.gz /tmp/rpm-gfs/SOURCES ; \
	rpm --rcfile /tmp/rpm-gfs/rc -bb rpm.spec ; \
	cp /tmp/rpm-gfs/RPMS/$$buildarch/*.rpm .
	rm -rf /tmp/rpm-gfs

rpm-ungif: dist
	buildarch=`rpm --showrc | awk '/^build arch/ { print $$4; }'` ; \
	mkdir -p /tmp/rpm-ugfs/SOURCES /tmp/rpm-ugfs/RPMS/$$buildarch \
	/tmp/rpm-ugfs/BUILD ; \
	echo 'include: /usr/lib/rpm/rpmrc' > /tmp/rpm-ugfs/rc ; \
	echo 'macrofiles: /usr/lib/rpm/macros:/tmp/rpm-ugfs/macros' >> /tmp/rpm-ugfs/rc ; \
	echo '%_topdir /tmp/rpm-ugfs' > /tmp/rpm-ugfs/macros ; \
	cp logo1.gif $(PACKAGE)-$(VERSION).tar.gz /tmp/rpm-ugfs/SOURCES ; \
	GIFSICLE_UNGIF=yes rpm --rcfile /tmp/rpm-ugfs/rc -bb rpm.spec ; \
	cp /tmp/rpm-ugfs/RPMS/$$buildarch/*.rpm `echo /tmp/rpm-ugfs/RPMS/$$buildarch/*.rpm | sed 's/.*gifsicle/ungifsicle/'`
	rm -rf /tmp/rpm-ugfs

dist-ungif: dist
	tar xzf gifsicle-$(VERSION).tar.gz
	@rm gifsicle-$(VERSION)/gifwrite.c
	rm -rf ungifsicle-$(VERSION)
	mv gifsicle-$(VERSION) ungifsicle-$(VERSION)
	GZIP=--best gtar chozf ungifsicle-$(VERSION).tar.gz ungifsicle-$(VERSION)
	rm -rf ungifsicle-$(VERSION)

.PHONY: srclinks versionize rpm dist-ungif rpm-ungif
