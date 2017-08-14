## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

man_MANS = gifsicle.1 @OTHERMANS@

EXTRA_DIST = COPYING README.md NEWS.md \
	include/lcdf/clp.h include/lcdf/inttypes.h \
	include/lcdfgif/gif.h include/lcdfgif/gifx.h \
	gifsicle.1 gifview.1 gifdiff.1 logo.gif logo1.gif \
	test/testie \
	test/001-transexpand.testie \
	test/002-cropresize.testie \
	test/003-bgopt.testie \
	test/004-careful.testie \
	test/005-resize.testie \
	test/006-unoptdisposal.testie \
	test/007-alltransp.testie \
	test/008-resizemix.testie \
	test/009-zerowidth.testie \
	test/010-warnings.testie \
	test/011-resizemix.testie

gifsicle: config.h
	@cd src && $(MAKE) gifsicle
gifdiff: config.h
	@cd src && $(MAKE) gifdiff
gifview: config.h
	@cd src && $(MAKE) gifview

srclinks:
	cd $(top_srcdir); sh ./sourcecheckout.sh

versionize:
	perl -pi -e "s/^\\.ds V.*/.ds V $(VERSION)/;" $(top_srcdir)/gifsicle.1 $(top_srcdir)/gifview.1 $(top_srcdir)/gifdiff.1
	perl -pi -e "s/gifsicle [\d.]+/gifsicle $(VERSION)/; s/VERSION \"[\w.]+/VERSION \"$(VERSION)/;" $(top_srcdir)/src/win32cfg.h

dist-ungif: dist
	$(AMTAR) xzf gifsicle-$(VERSION).tar.gz
	@rm gifsicle-$(VERSION)/src/gifwrite.c
	rm -rf ungifsicle-$(VERSION)
	mv gifsicle-$(VERSION) ungifsicle-$(VERSION)
	GZIP=$(GZIP_ENV) $(AMTAR) chozf ungifsicle-$(VERSION).tar.gz ungifsicle-$(VERSION)
	rm -rf ungifsicle-$(VERSION)

check: all
	$(top_srcdir)/test/testie -p $(top_builddir)/src $(top_srcdir)/test

.PHONY: srclinks versionize rpm dist-ungif rpm-ungif check
