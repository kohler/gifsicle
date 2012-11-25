GIFSICLE 1.68
=============

ABOUT GIFSICLE
--------------
   Gifsicle twaddles GIF image files in a variety of ways. It is better
than many of the freely available GIF twaddlers on the market -- for one
thing, it has more options.

   It supports merging several GIFs into a GIF animation; exploding an
animation into its component frames; changing individual frames in an
animation; turning interlacing on and off; adding transparency; adding
delays, disposals, and looping to animations; adding or removing comments;
flipping and rotation; optimizing animations for space; and changing
images' colormaps, among other things. Extensive command-line options
control which, if any, of these occur.

   Gifview, a companion program requiring X11, displays GIF images and
animations on an X display. It can display multi-frame GIFs either as
slideshows, displaying one frame at a time, or as real-time animations.

   Gifdiff, another companion program, checks two GIF files for identical
visual appearance. This is probably most useful for testing
GIF-manipulating software.

   Each of these programs has a manpage, 'PROGRAMNAME.1'.

   The Gifsicle package comes with NO WARRANTY, express or implied,
including, but not limited to, the implied warranties of merchantability
and fitness for a particular purpose.

MAKING GIFSICLE ON UNIX
-----------------------
   You need an ANSI C compiler, such as gcc.

   Just type './configure', then 'make'.  'make install' will build and
install gifsicle and its manual page (under /usr/local by default).

   './configure' accepts the usual options. See 'INSTALL' for more
details.

   To build without gifview (for example, if you don't have X11), give the
'--disable-gifview' option. To build without gifdiff, give the
'--disable-gifdiff' option. To have Gifsicle create unGIFs instead of GIFs,
give the '--enable-ungif' option.

MAKING GIFSICLE ON 32-BIT WINDOWS
---------------------------------
   Emil Mikulic <darkmoon@connexus.net.au> has provided a Makefile for
building gifsicle out of the box on 32-bit Windows. It has since been
updated by Eddie Kohler and Steven Marthouse <comments@vrml3d.com>. Just
unpack the distribution, change into its "src" directory, and type

	nmake -f Makefile.w32

Gifview will not be built. 

   Stephen Schnipsel <schnipsel@m4f.net> provided an altered Makefile for
building gifsicle on Windows using Borland C++. Unpack the distribution,
change into its "src" directory, and type

	nmake -f Makefile.bcc

   You will need to edit one of these Makefiles if you are building
ungifsicle, or if you are compiling with a different compiler. You can edit
it with any text editor (like Notepad). See the file for more information.

CHANGES
-------
   See 'NEWS' in this directory for a detailed listing of changes.

BUGS, SUGGESTIONS, ETC.
-----------------------
   Please write me if you have trouble building or running gifsicle, or if
you have suggestions or patches.

	Eddie Kohler
	ekohler@gmail.com
	http://www.read.seas.harvard.edu/~kohler/

GIFSICLE HOME PAGE
------------------
   For latest distributions, bug reports, and other stuff, see the Gifsicle
home page at

	http://www.lcdf.org/gifsicle/

THE GIF PATENTS AND UNGIFS
--------------------------
   Unisys formerly held patents on the Lempel-Ziv-Welch compression
algorithm used in GIFs, which means that GIF-manipulating software could
not be truly free.  As of October 1, 2006, however, it is believed (by the
Software Freedom Law Center and the Free Software Foundation, among others)
that there are no significant patent claims interfering with employment of
the GIF format.  For that reason, gifsicle is completely free software.

   Nonetheless, Gifsicle can be configured to write run-length-encoded
GIFs, rather than LZW-compressed GIFs, avoiding these obsolete patents.
This idea was first implemented independently by Toshio Kuratomi
<badger@prtr-13.ucsc.edu> and Hutchison Avenue Software Corporation
(http://www.hasc.com/, <info@hasc.com>). Turn this on by giving
'./configure' the '--enable-ungif' switch. Now that the patents have
expired there is no good reason to turn on this switch, which can make
GIFs a factor of 2 larger or more. If your copy of gifsicle says '(ungif)'
when you run 'gifsicle --version', it is writing run-length-encoded GIFs.

COPYRIGHT/LICENSE
-----------------
   All source code is Copyright (C) 1997-2011 Eddie Kohler.

   IF YOU PLAN TO USE GIFSICLE ONLY TO CREATE OR MODIFY GIF IMAGES, DON'T
WORRY ABOUT THE REST OF THIS SECTION. Anyone can use Gifsicle however they
wish; the license applies only to those who plan to copy, distribute, or
alter its code. If you use Gifsicle for an organizational or commercial Web
site, I would appreciate a link to the Gifsicle home page on any 'About
This Server' page, but it's not required.

   This code is distributed under the GNU General Public License, Version 2
(and only Version 2). The GNU General Public License is available via the
Web at <http://www.gnu.org/licenses/gpl.html> or in the 'COPYING' file in
this directory.

   The following alternative license may be used at your discretion.

   Permission is granted to copy, distribute, or alter gifsicle, whole or
in part, as long as source code copyright notices are kept intact, with the
following restriction: Developers or distributors who plan to use Gifsicle
code, whole or in part, in a product whose source code will not be made
available to the end user -- more precisely, in a context which would
violate the GPL -- MUST contact the author and obtain permission before
doing so.

   As of October 1, 2006, it is believed that there are no significant
patent claims interfering with employment of the GIF format, so previous
language about obtaining a license from Unisys has been removed.

AUTHORS
-------
Eddie Kohler <ekohler@gmail.com>
http://www.read.seas.harvard.edu/~kohler/
He wrote it.

Anne Dudfield <anne@mazunetworks.com>
http://www.frii.com/~annied/
She named it.

David Hedbor <david@hedbor.org>
Many bug reports and constructive whining about the optimizer.

Emil Mikulic <darkmoon@connexus.net.au>
Win32 port help.

Hans Dinsen-Hansen <dino@danbbs.dk>
http://www.danbbs.dk/~dino/
Adaptive tree method for GIF writing.
