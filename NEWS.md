Gifsicle NEWS
=============

## Version 1.92 – 18.Apr.2019

* Add `--lossy` option from Kornel Lipiński.

* Remove an assertion failure possible with `--conserve-memory` + `--colors` +
  `--careful`.


## Version 1.91 – 5.Jan.2018

* Several security bug fixes with malicious GIFs.


## Version 1.90 – 14.Aug.2017

* Kill a use-after-free error reported by @junxzm1990.


## Version 1.89 - 11.Jul.2017

* Add SIMD support for resizing. When enabled this improves resize
  performance enormously for complex resize methods.

* Add thread support for resizing. `-j[NTHREADS]` tells gifsicle to use
  up to NTHREADS threads to resize an input image. There are several
  caveats---multiple threads can be only used on unoptimized images.
  Thanks to Github user @wilkesybear.

* Quashed several crashes and undefined behaviors. Thanks to Github users
  including @pornel, @strazzere, and @b0b0505.

* Minor bug fixes.


## Version 1.88 - 1.Jul.2015

* Fix bug where long comments were read incorrectly. Reported by
  kazarny.

* Add --no-ignore-errors option.


## Version 1.87 - 9.Dec.2014

* Always optimize as if the background is transparent. This fixes some
  rare bugs reported by Lars Dieckow.

* Fix --crop issue with must-be-preserved frames that are out of the
  crop window.


## Version 1.86 - 14.Oct.2014

* Further fix --rotate + --crop.


## Version 1.85 - 14.Oct.2014

* Greatly improve optimization time for images with many colors.

* Add --no-extensions (with the s) and document those options more.

* Fix bug in interaction of --resize and --rotate reported by Michał
  Ziemba.


## Version 1.84 - 29.Jun.2014

* Correct optimizer bug that affected GIFs with 65535 or more total
  colors. Reported by Jernej Simončič.


## Version 1.83 - 21.Apr.2014

* Correct bug in custom gamma values reported by Kornel Lesiński.

* Update Windows build.


## Version 1.82 - 27.Mar.2014

* Correct bug in `mix` sampling method reported by Bryan Stillwell.


## Version 1.81 - 24.Mar.2014

* Correct bug in `mix` sampling method reported by Bryan Stillwell.


## Version 1.80 - 18.Mar.2014

* Bug fixes and improved error messages.


## Version 1.79 - 17.Mar.2014

* Major improvements in image scaling. Work sponsored by Tumblr via
  Mike Hurwitz.

  * Add new resize sampling methods `mix`, `box`, `catrom`, `mitchell`,
    `lanczos2`, and `lanczos3`, selectable by `--resize-method`. The
    `catrom` filter often gives good results; the slightly faster `mix`
    method (a bilinear interpolator) is now the default. These new
    sampling methods consider all of the image's input colors when
    shrinking the image, producing better, less noisy output for most
    images.

  * Add `--resize-colors`, which allows Gifsicle to enlarge the palette
    when resizing images. This is particularly important when shrinking
    images with small colormaps---e.g., shrinking a black-and-white
    image should probably introduce shades of gray.

* Support extensions such as XMP4 in which extension packet boundaries
  matter. Reported by `ata4`.

* Many bug fixes, especially to cropping. Thanks to Tumblr and to
  Bryan Stillwell, Tal Lev-Ami, "Marco," and others.


## Version 1.78 - 9.Dec.2013

* Correct an optimization bug introduced in 1.76. Reported by Tom
  Roostan.


## Version 1.77 - 26.Nov.2013

* Major improvements to color selection (important when reducing
  colormap size). Use gamma-corrected colors in selection and
  dithering; this makes image quality much better. Also, when reducing
  colors with dithering, prefer to select colors that dithering can't
  approximate.

* Add ordered dithering modes, which avoid animation artifacts. The
  default ordered dithering mode (`--dither=ordered`) is a novel mode
  that combines some of the visual advantages of error diffusion with
  the artifact avoidance of ordered dithering.

* Add halftone dithering (`--dither=halftone`).

* gifview: Improved cache memory management for better animations.
  Collect memory for old frames based on an explicit --memory-limit
  (default 40MB).

* gifview: Add `--fallback-delay` option, to specify a fallback delay
  for frames with delay 0. Thanks to Sung Pae.


## Version 1.76 - 20.Nov.2013

* Fix `-O2` crashes introduced with `--resize` improvements. Reported
  by Bryan Stillwell.


## Version 1.75 - 18.Nov.2013

* Improve `--careful` (fewer crashes). Reported by Bryan Stillwell.

* Improve `-O2`: again, don't refuse to optimize images with local
  color tables. Reported by Bryan Stillwell.

* Greatly improve `--dither` speed.


## Version 1.74 - 16.Nov.2013

* Improve `--resize` behavior: avoid animation artifacts due to
  different rounding decisions. Also speed it up. Reported by Bryan
  Stillwell.


## Version 1.73 - 15.Nov.2013

* Fix bug where `-O2` would refuse to optimize some images with local
  color tables, claiming that "more than 256 colors were required".
  What was really required is previous disposal. Reported by Bryan
  Stillwell.


## Version 1.72 - 9.Nov.2013

* Fix crash bugs on some combinations of `--crop` and `--resize`
  (prevalent on images whose first frame didn't cover the whole
  logical screen). Reported by Bryan Stillwell.


## Version 1.71 - 15.Jun.2013

* Avoid rounding errors in `--resize`. Reported by Paul Kane.

* Report error when `-I` is combined with `-b`. Reported by Frank
  Dana.

* Frame selections also apply in batch mode. Reported by LOU Yu Hong
  Leo.


## Version 1.70 - 31.Jan.2013

* Fix `--crop` bug introduced in version 1.68, visible in images
  containing local color tables. Bug reported by Alberto Nannini.


## Version 1.69 - 31.Jan.2013

* Minor bug fix release.


## Version 1.68 - 24.Nov.2012

* gifsicle: Alberto Nannini reported some images that are optimized
  beyond what Gifsicle can do. In fact, Gifsicle's GIF writer was
  limited enough that even when running without optimization, it
  seriously expanded the input images. Some improvements to Gifsicle's
  writing procedure avoid this problem; now `gifsicle IMAGE` will
  produce large results less often, and when it does generate larger
  results, they're larger by hundreds of bytes, not hundreds of
  thousands. However, due to restrictions in the current optimizer (and
  on my time), I was unable to improve `gifsicle -O3`'s handling of
  these images. The optimizer that produced the images must be doing
  something pretty clever. (Is it SuperGIF?)


## Version 1.67 - 5.May.2012

* gifsicle: Frame specifications like "#2-0" are allowed; they insert
  frames in reverse order. Feature request from Leon Arnott.

* gifview: Add --min-delay option.


## Version 1.66 - 2.Apr.2012

* gifsicle: Add -Okeep-empty for Gerald Johanson.


## Version 1.65 - 2.Apr.2012

* gifsicle: Several users (Kornel Lesiński and others) reported "bugs" with
  Gifsicle-optimized images and Mac programs like Safari. The bug is in
  Safari, but add --careful to work around it.

* Improve -O3 a bit (although for some images, the new -O3 is bigger than
  the old).


## Version 1.64 - 22.Nov.2011

* gifsicle: Add --resize-fit options.  Tom Glenne request.


## Version 1.63 - 17.Jul.2011

* gifsicle: Avoid crash on frame selections where frames are repeated.
  Werner Lemberg report.


## Version 1.62 - 4.Apr.2011

* gifsicle: -O3 optimization level tries even harder, so that now gifsicle
  -O3 should never produce larger results than gifsicle-1.60 -O2.  Jernej
  Simončič report.


## Version 1.61 - 25.Feb.2011

* gifsicle: Add new -O3 optimization level, which applies several
  transparency heuristics and picks the one with the best result.  David
  Jamroga pointed out a file that gifsicle made larger; -O3 shrinks it.

* gifsicle: Correct some optimizer bugs introduced in 1.59 that could lead
  to visually different output.

* gifdiff: Add -w/--ignore-redundancy option.

* gifview: Correct crash bug.

* gifsicle/gifdiff: Correct occasionally odd error messages.


## Version 1.60 - 12.Apr.2010

* GIF reading library: Correct error that could corrupt the reading of
  certain large images.  Reported by Jernej Simončič.


## Version 1.59 - 11.Mar.2010

* gifsicle -O2: Optimize away entirely-transparent frames when possible.
  Requested by Gerald Johanson.


## Version 1.58 - 14.Jan.2010

* gifsicle: Fix optimizer bug reported by Dion Mendel.


## Version 1.57 - 11.Nov.2009

* gifsicle: Don't throw away totally-cropped frames with 0 delay, since
  most browsers treat 0-delay frames as 100ms-delay frames.  Reported by
  Calle Kabo.


## Version 1.56 - 18.Oct.2009

* gifsicle: Fix --crop-transparency for animated images; previous versions
  could lose frames.  Problem reported by Daniel v. Dombrowski.

* gifview: Make --disposal=background behavior look like Firefox.


## Version 1.55 - 3.Apr.2009

* gifsicle: Another optimize fix for --disposal previous.  Reported by
  Gerald Johanson.


## Version 1.54 - 3.Apr.2009

* gifsicle, gifview: Fix several serious bugs with --disposal previous that
  could affect optimized, unoptimized, and displayed images.  Reported by
  Gerald Johanson.


## Version 1.53 - 19.Mar.2009

* gifsicle: Frames in an unoptimized image will use disposal "none" when
  possible.  This leads to fewer surprises later.


## Version 1.52 - 18.May.2008

* gifsicle: Fix bug introduced in 1.51: --crop-transparent works.  Reported by
  dgdd wanadoo fr.


## Version 1.51 - 12.May.2008

* gifsicle: `--crop` preserves the logical screen when it can.  Reported by
  Petio Tonev.


## Version 1.50 - 7.May.2008

* gifsicle, gifview: Refuse to read GIFs from the terminal.  Requested by
  Robert Riebisch.


## Version 1.49 - 2.May.2008

* Just some Makefile updates requested by Robert Riebisch.


## Version 1.48 - 16.Mar.2007

* gifsicle: Avoid crash in `--crop-transparency` when an image's first
  frame is totally transparent.  Reported by Gerald Johanson.


## Version 1.47 - 10.Mar.2007

* gifsicle: Improve `--nextfile` behavior.


## Version 1.46 - 9.Jan.2007

* gifsicle: Add `--nextfile` option, useful for scripts.  Problem reported
  by Jason Young.


## Version 1.45 - 30.Dec.2006

* Do not compile gifview by default when X is not available.

* gifview: Add `--title` option, based on patch supplied by Andres Tello
  Abrego.


## Version 1.44 - 3.Oct.2005

* gifview: Enforce a minimum delay of 0.002 sec.  Requested by Dale Wiles.


## Version 1.43 - 8.Sep.2005

* '-I -b', and other similar combinations, causes a fatal error rather than
  putting information in an unexpected file.  Reported by Michiel de Bondt.


## Version 1.42 - 8.Jan.2005

* Improve manual page.


## Version 1.41 - 19.Aug.2004

* Fix problems with 64-bit machines.  Thanks to Dan Stahlke for pointing
  out the problem and providing a patch.


## Version 1.40 - 28.Aug.2003

* Fix longstanding bug where `--disposal=previous` was mistakenly
  equivalent to `--disposal=asis`.

* Include Makefile.bcc, from Stephen Schnipsel.


## Version 1.39 - 11.Jul.2003

* Fix Makefile.w32 (oops).


## Version 1.38 - 26.Jun.2003

* Include Makefile.w32 (oops).


## Version 1.37 - 11.Feb.2003

* Fix bug where combining `--rotate-X` and `-O` options would cause a
  segmentation fault. Reported by Dan Lasley <Dan_Lasley@hilton.com>.

* Rearrange source tree.


## Version 1.36 - 17.Nov.2002

* Fix subscript-out-of-range error in main.c reported by Andrea Suatoni.


## Version 1.35 - 14.Aug.2002

* Fixed bug where `--crop` could cause a segmentation fault, present since
  1.32 or 1.33. Reported by Tom Schumm <phong@phong.org>.


## Version 1.34 - 13.Aug.2002

* Fixed bug where combining `--crop` and `-O` options could corrupt output.
  Reported by Tom Schumm <phong@phong.org>.


## Version 1.33 - 1.Aug.2002

* Be more careful about time while animating. In particular, prepare frames
  before they are needed, so that they can be displayed exactly when
  required. Problem reported by Walter Harms <WHarms@bfs.de>.

* More warning fixes.


## Version 1.32 - 5.Jul.2002

* Add `--multifile` option handling concatenated GIF files. This is useful
  for scripts. For example, `gifsicle --multifile -` will merge all GIF
  files written to its standard input into a single animation.

* More fixes for spurious background warnings.


## Version 1.31 - 17.Jun.2002

* Changed behavior of `--crop X,Y+WIDTHxHEIGHT` option when WIDTH or HEIGHT
  is negative. Previously, zero or negative WIDTH and HEIGHT referred to
  the image's entire width or height. Thus, the option `--crop 10,0+0x0`
  would always lead to an error, because the crop left position (10) plus
  the crop width (the image width) was 10 pixels beyond the image edge. The
  new behavior measures zero or negative WIDTH and HEIGHT relative to the
  image's bottom-right corner.

* Changed background behavior. Hopefully the only user-visible effect will
  be fewer spurious warnings.

* Fixed a bug that could corrupt output when optimizing images with `-O2`
  that had more than 256 colors.


## Version 1.30 - 27.Jul.2001

* Fixed bug in ungif code: Writing a large ungif could corrupt memory,
  leading eventually to bad output. This bug has been present since
  Gifsicle could write ungifs! Bad files and assistance provided by Jeff
  Brown <jabrown@ipn.caida.org>.


## Version 1.29 - 22.May.2001

* Fixed an optimization bug that could produce bad images. Again, reported
  by Dale Wiles <dwiles@buffalo.veridian.com>. It turns out that 1.26 was a
  bad release! I've reconstructed and run some regression tests; hopefully
  Dale and I have exhausted the bug stream.


## Version 1.28 - 13.May.2001

* `--colors` option still didn't work; it produced bad images. Again,
  thanks to Dale Wiles <dwiles@buffalo.veridian.com> for reporting the bug.

* Added explicit `--conserve-memory` option.


## Version 1.27 - 10.May.2001

* Fixed `--colors` option, which caused segmentation faults. Thanks to Dale
  Wiles <dwiles@buffalo.veridian.com> for reporting the bug.


## Version 1.26 - 22.Apr.2001

* Added `--crop-transparency` option, which crops any transparent edges off
  the image. Requested by Gre7g Luterman <gre7glut@earthlink.com>.

* Try to conserve memory in gifsicle when working with huge images (> 20
  megabytes of uncompressed data). May make gifsicle slower if you actually
  had enough memory to deal with the uncompressed data.


## Version 1.25 - 13.Feb.2001

* Fixed gifview bug: If every frame in an animation had a small or
  zero delay, then gifview would previously enter a infinite loop and
  become noninteractive. Reported by Franc,ois Petitjean
  <francois.petitjean@bureauveritas.com>.


## Version 1.24 - 11.Feb.2001

* Delete `unoptimized_pixmaps` array when deleting a viewer in gifview.
  Reported by Franc,ois Petitjean <francois.petitjean@bureauveritas.com>.

* Added `--resize-width` and `--resize-height` options.

* Why is it that Frenchmen are always telling me to delete the Clp_Parser
  immediately before calling exit()? Don't they realize that exit() frees
  application memory just as well as free()? Are things different in
  France? More French, perhaps?


## Version 1.23 - 12.Dec.2000

* In Houston on a layover because of stupid Continental Airlines: allow
  GIFs without terminators. Problem reported by Matt Olech
  <Matt.Olech@airliance.com>.


## Version 1.22 - 23.Nov.2000

* Handle time more carefully when displaying animations. Should result in
  more precise timings. Problem reported by Iris Baye <ibaye@gwdg.de>.


## Version 1.21 - 9.Sep.2000

* Fixed `--careful -O2`, which could create bad GIFs when there were bad
  interactions with transparency. Bug reported by Manfred Schwarb
  <schwarb@geo.umnw.ethz.ch>.


## Version 1.20 - 23.Jun.2000

* Added `--careful` option, because some bad GIF implementations (Java,
  Internet Explorer) don't support Gifsicle's super-optimized, but legal,
  GIFs. Problem reported by Andrea Beiser <abb9@columbia.edu>.

* Gifdiff will work on very large images (better memory usage).


## Version 1.19 - 25.Apr.2000

* Fixed memory corruption bug: `gifsicle --use-colormap=FILE.gif` would
  formerly cause a segmentation fault. Bug reported by Alan Watts
  <alan@16color.com>.


## Version 1.18 - 3.Apr.2000

* Gifview now uses X server memory more parsimoniously. This should make it
  more robust, and nicer to the X server, on large animations. Requested by
  Vladimir Eltsov <ve@boojum.hut.fi>. Appears to solve bug reported by
  Vince McIntyre <vjm@Physics.usyd.edu.au>.

* Gifview treats frame selection more sensibly. A frame selection just
  tells it which frame to start on.

* Added a message to the manual page warning people to quote the `#`
  character in frame selections. The interactive bash shell, for example,
  interprets it as a comment.


## Version 1.17 - 15.Mar.2000

* Added `--resize _xH` and `--resize Wx_`. Requested by Edwin Piekart
  <edwin@ckz.com.pl>.

* Changed behavior of `--no-logical-screen`. Now gifsicle will include all
  visible GIFs when calculating the size of the logical screen.

* Got rid of spurious redundant-option warnings.


## Version 1.16.1 - 10.Sep.1999

* Gifview can put a GIF (or an animated GIF) on the root window; use
  `gifview -w root`. Requested by Roland Blais <roland@greatbasin.com>.


## Version 1.16 - 31.Aug.1999

* Optimization improvement: More colormap manipulation. Version 1.16
  produces smaller GIFs than 1.15 in the large majority of cases (but not
  all).

* Fixed memory corruption bug in quantization. Found and fixed by Steven
  Marthouse <comments@vrml3d.com>.


## Version 1.15 - 20.Aug.1999

* Bug fix: no more assertion failures when combining images that require
  local colormaps.

* Fixed serious quantization bug introduced in 1.14: when reducing the
  number of colors, gifsicle would ignore a random portion of the colors in
  the old colormap.

* Optimization bug fix: two adjacent identical frames will no longer crash
  the optimizer. Bug introduced in 1.14. That's two for two: two "speed
  improvements" in 1.14, two bugs introduced!

* Bug fix: Exploding the standard input results in files named
  `#stdin#.NNN`, as the documentation claims, not `<stdin>.NNN`.

* Optimization improvement: the optimizer reorders colormaps in a slightly
  smarter way, which can result in smaller compression.

* If a position option like `-p 0,5` is followed by a multi-frame GIF
  (without frame specifications), then the position option places the
  animation as a whole, not each of its individual frames.

* An unrecognized nonnumeric frame specification is now treated as a file
  name, so you can say `gifsicle #stdin#.000`, for example.

* Gifview improvements: Now even images with local color tables can be
  "unoptimized" or animated.

* `--replace` now preserves the replaced frame's disposal as well as its
  delay.

* `gifsicle -I -` reports "<stdin>" as the input file name.


## Version 1.14.2 - 16.Aug.1999

* Fixed memory bug in Gif_DeleteImage (I freed a block of memory, then
  accessed it). I am a moron!! This bug affected Version 1.14.1 only.


## Version 1.14.1 - 9.Aug.1999

* Fixed configuration bug: you couldn't compile gifsicle from a different
  directory (it told you to try --enable-ungif).

* Steven Marthouse <comments@vrml3d.com> helped fix `Makefile.w32` to
  enable wildcards in filenames under Windows. Requested by Mark Olesen
  <mark_olesen@hotmail.com>.


## Version 1.14 - 2.Aug.1999

* Switched to an adaptive tree strategy for LZW GIF compression, which is
  faster than the old hashing strategy. This method is the brainchild of
  Hans Dinsen-Hansen <dino@danbbs.dk>.

* Bug fix/optimization improvement: Some images would include a transparent
  index that was not a valid color (it was larger than the colormap).
  Gifsicle formerly ignored these transparent indices. It doesn't any
  longer; furthermore, it will generate those kinds of transparent index,
  which can mean smaller colormaps and smaller GIFs.

* Speed improvements in optimization and colormap quantization.

* Added `--no-warnings` (`-w`) option to inhibit warning messages.
  Suggested by Andrea Beiser <abb9@columbia.edu>.

* `--info` output can now be sent to a file with `-o`, just as with any
  other kind of output.

* More specific error messages on invalid GIFs.


## Version 1.13 - 16.Jun.1999

* Added optional support for run-length-encoded GIFs to avoid patent
  problems. Run-length encoding idea based on code by Hutchinson Avenue
  Software Corporation <http://www.hasc.com> found in Thomas Boutell's gd
  library <http://www.boutell.com>.

* `gifsicle --explode` now generates filenames of the form `file.000 ..
  file.100` rather than `file.0 .. file.100`, so you can use shell globbing
  like `file.*` and automatically get the right order.

* Added `--background` option to gifview, so you can set the color used for
  transparent pixels. Suggested by Art Blair <apblair@students.wisc.edu>.

* Gifdiff is built by default on Unix.


## Version 1.12 - 25.Mar.1999

* Added `--window` option to gifview, which lets it display a GIF in an
  arbitrary X window. Thanks to Larry Smith <lcs@zk3.dec.com> for patches.

* Added `--install-colormap` to gifview. Suggested by Yair Lenga
  <yair.lenga@ssmb.com>.

* Gifsicle now exits with status 1 if there were any errors, status 0
  otherwise. Before, it exited with status 1 only if there was a fatal error,
  or nothing was successfully output. The new behavior seems more useful.
  Problem reported by David Kelly <dkelly@nebula.tbe.com>.


## Version 1.11.2 - 10.Mar.1999

* `gifsicle -U` will now unoptimize 1-image GIFs as well as animations.


## Version 1.11.1 - 24.Jan.1999

* Fixed core dump on corrupted GIF files (in particular, bad extensions).
  Problem reported by tenthumbs <tenthumbs@cybernex.net>.


## Version 1.11 - 22.Jan.1999

* Added `--scale XFACTORxYFACTOR` to complement `--resize`.

* Made `--resize` work better on animations (no more off-by-one errors).

* Fixed bug in `--use-colormap`: If transparency was added to the new
  colormap, the transparent color index could later be used (incorrectly)
  as a real image color. This was particularly common if the image was
  dithered.

* `--dither` now tries to mitigate "jumping dither" animation artifacts
  (where adjacent frames in an animation have different dithering patterns,
  so the animation shifts). It does an OK job.

* Improvements to `--color-method=blend-diversity`.


## Version 1.10 - 6.Jan.1999

* Gifview is built by default.

* No other changes from 1.10b1: Emil said it worked on Windows.


## Version 1.10b1 - 31.Dec.1998

* The two malloc packages that come with Gifsicle don't #define `malloc` or
  `realloc`; they #define `xmalloc` and `xrealloc`.

* Changes to Windows port from Emil Mikulic <darkmoon@connexus.net.au>.


## Version 1.9.2 - 28.Dec.1998

* Gifsicle compiles out of the box on Windows! Port maintained by Emil
  Mikulic <darkmoon@connexus.net.au>.

* Moved to config.h-based configuration to simplify GIF library and Windows
  port.

* Some CLP improvements.


## Version 1.9.1 - 16.Dec.1998

* If a reduced-colormap image needs a special transparent color, gifsicle
  will now try to keep the same transparent color value as the input GIF.

* Cropping an image now automatically recalculates the output logical
  screen, rather than retaining the logical screen of the uncropped image.

* Improvement to colormap modification behavior: If it looks like a color
  reserved for transparency will be necessary, gifsicle will reserve a slot
  for it. This will reduce the likelihood that you ask for a colormap of
  size 64, say, and get one of size 128 (because gifsicle added a slot for
  transparency).


## Version 1.9 - 14.Dec.1998

* `--no-background` is now the same as `--background=0` (set the background
  to pixel 0) instead of `--same-background`.


## Version 1.9b3 - 10.Dec.1998

* Fixed a serious bug in merge.c introduced in 1.8: Merging several images
  with global colormaps could corrupt the output colors.

* Fixed a serious bug in optimize.c introduced in 1.9b1: Optimizing GIFs
  which required local color tables could crash the program due to a buffer
  overrun. (I was adding the background to the output colormap without
  making sure there was room for it.)

* Fixed an optimizer bug which could result in incorrect animations:
  background disposal was not handled correctly.

* The optimizer now generates `--disposal=none` instead of
  `--disposal=asis` in certain situations, which can result in smaller
  animations. Suggestion by Markus F.X.J. Oberhumer
  <k3040e4@c210.edvz.uni-linz.ac.at>.

* GIF library changes: cleaner, somewhat faster GIF reading code inspired
  by Patrick J. Naughton <naughton@wind.sun.com>, whose code is distributed
  with XV.


## Version 1.9b2 - 9.Dec.1998

* Fixed `--logical-screen` and `--no-logical-screen`, which had no effect
  in 1.9b1. In a stunning turn of events, this problem was reported by
  Markus F.X.J. Oberhumer <k3040e4@c210.edvz.uni-linz.ac.at>.


## Version 1.9b1 - 5.Dec.1998

* Added `--resize WxH` option, prompted by code from Christian Kumpf
  <kumpf@igd.fhg.de>.

* Made `--change-color PIXELINDEX color` work, and `--change-color color
  PIXELINDEX' illegal.

* Many behavior fixes relating to background and transparency. Gifsicle
  would tend to create GIFs which looked as expected, but lost information
  internally; for example, a color value which was only used for
  transparency would be changed to black, no matter what the input color
  was. In some cases, gifsicle simply reports a warning now where it
  ignored information before. Problems reported by Markus F.X.J. Oberhumer
  <k3040e4@c210.edvz.uni-linz.ac.at>.

* Better error messages are now given on redundant, ambiguous, or useless
  command-line options.

* Options that affect GIFs on output (such as `--output`, `--optimize`) now
  behave more like other options: you should put them before the files they
  affect. (You may still put them at the end of the argument list if they
  should affect the last output file.) This should only affect people who
  used batch or explode mode in complex ways.

* Gifsicle will not write a GIF to stdout if it's connected to a terminal.


## Version 1.8 - 3.Dec.1998

* Fixed strange behavior when changing transparency: a new black entry was
  added to the colormap in almost all situations. Problem reported by
  Markus F.X.J. Oberhumer <k3040e4@c210.edvz.uni-linz.ac.at>.

* Fixed `--transform-colormap` bug: commands like "thing 2" (ending in an
  integer) will work now.

* Small fixes (in a gifsicle pipeline, error messages won't be interleaved).


## Version 1.7.1 - 2.Dec.1998

* Added configuration check for `strerror`. Problem reported by Mario
  Gallotta <Mario_Gallotta@optilink.dsccc.com>.

* Added colormap canonicalization: If you pipe a GIF through `gifsicle -O`
  or `-O2`, the output colormaps will be arranged in a predictable order.
  Feature suggested by Markus F.X.J. Oberhumer
  <k3040e4@c210.edvz.uni-linz.ac.at>.


## Version 1.7 - 28.Nov.1998

* Added `--use-colormap=gray` and `--use-colormap=bw`. Idea and some code
  thanks to Christian Kumpf <kumpf@igd.fhg.de>.

* Added `--transform-colormap`, which allows you to plug programs into
  gifsicle that arbitrarily change GIF files' colormaps.

* All `--change-color` options now happen simultaneously, so you can safely
  swap two colors with `--change-color C1 C2 --change-color C2 C1`.

* Colormap modifications are slightly more powerful (they don't reserve a
  color for transparency if they don't need one, and if a subset of the
  modified colormap is used, only that subset is output).

* Fixed `--use-colormap` bug: the last color in a used colormap would be
  ignored (it was reserved for transparency). Symptom: if you put a
  partially transparent image into the Web-safe palette, the result would
  never contain white.

* When changing to a grayscale colormap, gifsicle now uses luminance
  difference to find the closest color. This gives better results for
  `--use-colormap=gray` and the like.

* Added Introduction and Concept Index sections to the gifsicle manpage.

* The gifsicle package now uses automake.


## Version 1.6 - 23.Nov.1998

* A frame extracted from the middle of an animation will now have the
  animation's screen size rather than 640x480. Problem reported by Mr.
  Moose <mrmoose@execpc.com>.


## Version 1.5 - 27.Sep.1998

* `--help` now prints on stdout, as according to the GNU standards.

* Changes to support Win32 port thanks to Emil Mikulic
  <darkmoon@connexus.net.au>.

* Makefiles: Added `make uninstall` target, enabled `./configure`'s
  program name transformations, made VPATH builds possible.


## Version 1.4.1 - 16.Sep.1998

* Fixed `--unoptimize` bug: a frame's transparency could disappear under rare
  circumstances. Bug reported by Rodney Brown <rodneybrown@pmsc.com>


## Version 1.4 - 12.Sep.1998

* Added `--extension` and `--app-extension`.


## Version 1.3.4 - 7.Sep.1998

* More configuration changes.

* Fixed bug in gifview `--geometry` option processing: `--geometry -0-0`
  wasn't recognized.


## Version 1.3.2 - 5.Sep.1998

* Fixed configuration bugs reported by Alexander Mai
  <st002279@hrzpub.tu-darmstadt.de> (OS/2 build didn't work).


## Version 1.3.1 - 4.Sep.1998

* Fixed configuration bug: int32_t could be improperly redefined. Reported by
  Anne "Idiot" Dudfield <anne@lvld.hp.com> and Dax Kelson
  <dkelson@inconnect.com>.


## Version 1.3 - 3.Sep.1998

* Added `--flip-*` and `--rotate-*` options.

* Fixed rare bug in GIF writing code: the last pixel in a frame could
  previously become corrupted.


## Version 1.3b1 - 2.Aug.1998

* Optimization has been completely overhauled. All optimization functions
  are in a separate file, optimize.c. Some bugs have been caught.
  Optimization is now quite powerful, and rarely expands any input file.
  (An expansion of 1-30 bytes usually means that the input file cheated by
  leaving off 'end-of-image' codes, which the standard says must be used.)
  People interested in making optimization even better should contact me,
  or just go ahead and hack the code. Thanks again to David Hedbor
  <david@hedbor.org> for providing test cases.

  Specific changes:

  - The optimizer can handle any input file, even one with local colormaps or
    more than 256 colors. If there are more than 256 colors, an optimal subset
    is placed in the global colormap.

  - The global colormap is reordered so that some images can use a small
    initial portion of it, which lets them compress smaller.

  - Bug fix: Images relying on background disposal are optimized correctly.

  - `-O2` never does worse than `-O1`.

  - Made the `-O2` heuristic better. Some other small changes and
    improvements here and there.

  - The optimizer has been pretty extensively tested using gifdiff.

* New program: gifdiff compares two GIFs for identical appearance. It is
  not built unless you give the `--enable-gifdiff` option to `./configure`.
  It's probably not useful unless you're testing a GIF optimizer.

* Robustness fixes in GIF library.


## Version 1.2.1 - 8.Jun.1998

* `--info` now sends information to standard output. `--info --info` still
  sends information to standard error.

* `--transparent` didn't work. It does now.

* Added `--background` option. Fixed background processing so that an input
  background is reflected in the output GIF.

* Re-fixed small bug with the interaction between per-frame options and
  frame selections.


## Version 1.2 - 28.May.1998

* Fixed small bug with the interaction between per-frame options (`--name`,
  `--comment`) and frame selections.

* Fixed bad bug in merge: If a global colormap from the source needed to be
  dumped into a local colormap in the destination, the pixel map was wrong,
  resulting in bad colors in the destination.

* Fixed memory leaks. Changed memory usage pattern in gifsicle to
  release source image memory as soon as possible, resulting in a better
  memory profile. Both programs now keep images in compressed form for
  better performance.

* Fixed bugs in gifread.c that corrupted memory when uncompressing some bad
  images.

* Added debugging malloc library (`./configure --enable-dmalloc` to use it).

* Added `--no-delay` and `--no-disposal` options, which should have been there
  all along.

* `--crop` will not generate an image with 0 width or height, even if a
  frame is cropped out of existence.

* Other performance improvements.


## Version 1.2b6 - 25.May.1998

* License clarification in README.

* GIF comments are now printed out more carefully in `--info`: binary
  characters appear as backslash escapes. (Literal backslashes are printed
  as `\\`.)

* New `--extension-info/--xinfo` option.

* Removed spurious warning about local colormaps when a `--colors` option
  is in effect.

* Replacing a named frame with another image now keeps the name by default.

* Many changes to gifview. Added `--animate`, `--display`, `--geometry`,
  `--name`, `--help` options, keystrokes, multiple GIFs on the command
  line, and a manual page.


## Version 1.2b5 - 12.May.1998

* Longstanding bug fix in gifunopt.c: an interlaced replacement frame
  caused strange behavior when later optimizing, as the underlying data
  wasn't deinterlaced.

* Further improvements in optimizer; specifically, `-O2` works better now
  -- it uses actual image data to look for an unused pixel value when
  there's no transparency specified.


## Version 1.2b4 - 12.May.1998

* Improved `--optimize`'s performance on images where successive frames had
  different transparent color indexes.


## Version 1.2b3 - 11.May.1998

* Fixed bugs in `--unoptimize`: it would try to unoptimize images with
  local color tables and some transparency manipulations could cause a
  silent failure (example: 2 colors; frame 0 = [*11], frame 1 = [*0*],
  where * is a transparent pixel. Unoptimized, frame 1 should contain [*01]
  -- but this can't be expressed with only 2 colors.)

* Greatly improved `--optimize`'s performance on some images by changing the
  way frames were switched to `--disposal=background`. Problem noticed by
  David Hedbor <david@hedbor.org>

* Documentation improvements.

* Unrecognized extensions are now passed through without change. The
  `--no-extensions` option (currently undocumented) removes all
  unrecognized extensions.

* Bug fix in gifview: frame selections now prevent display of entire GIF.


## Version 1.2b2 - 9.May.1998

* Fixed bad dumb bug in gif.h: lack of parens around macro argument

* Changed heuristic for diversity algorithm: never blend if there are 3 colors
  or less

* Some optimizations to Floyd-Steinberg code


## Version 1.2b1 - 8.May.1998

* Added colormap options: `--colors`, `--color-method`, `--dither`, and
  `--use-colormap`. All this code is in quantize.c.

* Renamed `--color-change` to `--change-color`.


## Version 1.1.2

* Added `--color-change` option.

* Added negative frame numbers. `#-N` no longer means `#0-N`; it means the
  `N`th frame from the end (where `#-1` is the last frame). You can use
  negative frame numbers in ranges as well (e.g., `#-5--1`).


## Version 1.1.1 - 22.Nov.1997

* Fixed bug in CLP which segmentation-faulted on `-` arguments.


## Version 1.1 - 20.Nov.1997

* Added `--crop` option.

* Changed usage behavior on bad command lines.


## Version 1.0 - 21.Jun.1997

* Added `--output/-o` option to specify output filename.

* Slight behavior change: Now, if you replace a frame (say frame #2), its
  delay value will be preserved in the replacement frame, unless you give
  an explicit delay. (Before, the replacement frame's delay would be used,
  which was probably 0.)


## Version 0.91 - 15.Jun.1997

* Bug fix: `--optimize` didn't handle the bottom row of an image's changed
  area correctly. This could lead to incorrect animations. Specific change:
  merge.c line 744: was y < fbt, now y <= fbt.

* Added `-S` as synonym for `--logical-screen`.


## Version 0.9 - 12.Jun.1997

* First public release.
