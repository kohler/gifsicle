![gifsicle-logo](https://raw.githubusercontent.com/kohler/gifsicle/master/logo.gif)

Gifsicle
========
[![TravisCI status](https://travis-ci.org/kohler/gifsicle.svg?branch=master)](https://travis-ci.org/kohler/gifsicle)

Gifsicle manipulates GIF image files. Depending on command line
options, it can merge several GIFs into a GIF animation; explode an
animation into its component frames; change individual frames in an
animation; turn interlacing on and off; add transparency; add delays,
disposals, and looping to animations; add and remove comments; flip
and rotate; optimize animations for space; change images' colormaps;
and other things.

Gifview, a companion program, displays GIF images and animations on an
X display. It can display multi-frame GIFs either as slideshows,
displaying one frame at a time, or as real-time animations.

Gifdiff, another companion program, checks two GIF files for identical
visual appearance. This is probably most useful for testing
GIF-manipulating software.

Each of these programs has a manpage, `PROGRAMNAME.1`.

The Gifsicle package comes with NO WARRANTY, express or implied,
including, but not limited to, the implied warranties of
merchantability and fitness for a particular purpose.

See `NEWS.md` in this directory for changes in recent versions. The Gifsicle
home page is:

http://www.lcdf.org/gifsicle/


Building Gifsicle on UNIX
-------------------------

Type `./configure`, then `make`.

If `./configure` does not exist (you downloaded from Github), run
`autoreconf -i` first.

`./configure` accepts the usual options; see `INSTALL` for details.
To build without gifview (for example, if you don't have X11), use
`./configure --disable-gifview`. To build without gifdiff,
use `./configure --disable-gifdiff`.

`make install` will build and install Gifsicle and its manual page
(under /usr/local by default).


Building Gifsicle on Windows
----------------------------

To build Gifsicle on Windows using Visual C, change into the `src`
directory and run

    nmake -f Makefile.w32

Gifview will not be built. The makefile is from Emil Mikulic
<darkmoon@connexus.net.au> with updates by Steven Marthouse
<comments@vrml3d.com>.

To build Gifsicle on Windows using Borland C++, change into the `src`
directory and run

    nmake -f Makefile.bcc

Stephen Schnipsel <schnipsel@m4f.net> provided `Makefile.bcc`. You
will need to edit one of these Makefiles to use a different compiler.
You can edit it with any text editor (like Notepad). See the file for
more information.


Contact
-------

If you have trouble building or running Gifsicle, try GitHub issues:
https://github.com/kohler/gifsicle

Eddie Kohler, ekohler@gmail.com  
http://www.read.seas.harvard.edu/~kohler/  
Please note that I do not provide support for Windows.


Copyright/License
-----------------

All source code is Copyright (C) 1997-2019 Eddie Kohler.

IF YOU PLAN TO USE GIFSICLE ONLY TO CREATE OR MODIFY GIF IMAGES, DON'T
WORRY ABOUT THE REST OF THIS SECTION. Anyone can use Gifsicle however
they wish; the license applies only to those who plan to copy,
distribute, or alter its code. If you use Gifsicle for an
organizational or commercial Web site, I would appreciate a link to
the Gifsicle home page on any 'About This Server' page, but it's not
required.

This code is distributed under the GNU General Public License, Version
2 (and only Version 2). The GNU General Public License is available
via the Web at <http://www.gnu.org/licenses/gpl.html> or in the
'COPYING' file in this directory.

The following alternative license may be used at your discretion.

Permission is granted to copy, distribute, or alter Gifsicle, whole or
in part, as long as source code copyright notices are kept intact,
with the following restriction: Developers or distributors who plan to
use Gifsicle code, whole or in part, in a product whose source code
will not be made available to the end user -- more precisely, in a
context which would violate the GPL -- MUST contact the author and
obtain permission before doing so.


Authors
-------

Eddie Kohler <ekohler@gmail.com> \
http://www.read.seas.harvard.edu/~kohler/ \
He wrote it.

Anne Dudfield <anne@mazunetworks.com> \
http://www.frii.com/~annied/ \
She named it.

David Hedbor <david@hedbor.org> \
Many bug reports and constructive whining about the optimizer.

Emil Mikulic <darkmoon@connexus.net.au> \
Win32 port help.

Hans Dinsen-Hansen <dino@danbbs.dk> \
http://www.danbbs.dk/~dino/ \
Adaptive tree method for GIF writing.

Kornel Lesiński \
`--lossy` option.
