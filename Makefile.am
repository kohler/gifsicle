## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = foreign check-news

SUBDIRS = src

man_MANS = gifsicle.1 @OTHERMANS@

EXTRA_DIST = gifsicle.spec \
	include/lcdf/clp.h include/lcdf/inttypes.h \
	include/lcdfgif/gif.h include/lcdfgif/gifx.h \
	gifsicle.1 gifview.1 gifdiff.1 logo.gif logo1.gif

gifsicle:
	@cd src && $(MAKE)

srclinks:
	test -d $(srcdir)/include || mkdir $(srcdir)/include
	test -d $(srcdir)/include/lcdf || mkdir $(srcdir)/include/lcdf
	test -d $(srcdir)/include/lcdfgif || mkdir $(srcdir)/include/lcdfgif
	cd $(srcdir); for i in clp inttypes; do \
	ln -sf ../../../liblcdf/include/lcdf/$$i.h include/lcdf/$$i.h ; done
	cd $(srcdir); for i in gif gifx; do \
	ln -sf ../../../liblcdf/include/lcdfgif/$$i.h include/lcdfgif/$$i.h ; done
	cd $(srcdir); for i in clp fmalloc strerror ; do \
	ln -sf ../../liblcdf/liblcdf/$$i.c src/$$i.c ; done
	cd $(srcdir); for i in giffunc gifread gifwrite ungifwrt gifunopt giftoc gifx ; do \
	ln -sf ../../liblcdf/liblcdfgif/$$i.c src/$$i.c ; done

versionize:
	perl -pi -e "s/^\\.ds V.*/.ds V $(VERSION)/;" gifsicle.1 gifview.1 gifdiff.1
	perl -pi -e "s/^Version:(\s+).*/Version:"'$$'"{1}$(VERSION)/; s/$(PACKAGE)-[\w.]+\.tar\.gz/$(PACKAGE)-$(VERSION).tar.gz/;" gifsicle.spec
	perl -pi -e "s/VERSION \"[\w.]+/VERSION \"$(VERSION)/;" src/win32cfg.h

rpm: dist
	buildarch=`rpm --showrc | awk '/^build arch/ { print $$4; }'` ; \
	mkdir -p /tmp/rpm-gfs/SOURCES /tmp/rpm-gfs/RPMS/$$buildarch \
	/tmp/rpm-gfs/BUILD ; \
	echo 'include: /usr/lib/rpm/rpmrc' > /tmp/rpm-gfs/rc ; \
	echo 'macrofiles: /usr/lib/rpm/macros:/tmp/rpm-gfs/macros' >> /tmp/rpm-gfs/rc ; \
	echo '%_topdir /tmp/rpm-gfs' > /tmp/rpm-gfs/macros ; \
	cp logo1.gif $(PACKAGE)-$(VERSION).tar.gz /tmp/rpm-gfs/SOURCES ; \
	rpmbuild --rcfile /tmp/rpm-gfs/rc -bb gifsicle.spec ; \
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
	GIFSICLE_UNGIF=yes rpmbuild --rcfile /tmp/rpm-ugfs/rc -bb gifsicle.spec ; \
	cp /tmp/rpm-ugfs/RPMS/$$buildarch/*.rpm `echo /tmp/rpm-ugfs/RPMS/$$buildarch/*.rpm | sed 's/.*gifsicle/ungifsicle/'`
	rm -rf /tmp/rpm-ugfs

dist-ungif: dist
	$(AMTAR) xzf gifsicle-$(VERSION).tar.gz
	@rm gifsicle-$(VERSION)/src/gifwrite.c
	rm -rf ungifsicle-$(VERSION)
	mv gifsicle-$(VERSION) ungifsicle-$(VERSION)
	GZIP=$(GZIP_ENV) $(AMTAR) chozf ungifsicle-$(VERSION).tar.gz ungifsicle-$(VERSION)
	rm -rf ungifsicle-$(VERSION)

.PHONY: srclinks versionize rpm dist-ungif rpm-ungif
