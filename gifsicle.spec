Summary: Powerful program for manipulating GIF images and animations

Name: gifsicle
Version: 1.6
Release: 1
Source: http://www.lcdf.org/gifsicle/gifsicle-1.6.tar.gz

Icon: logo1.gif
URL: http://www.lcdf.org/gifsicle/

Group: Applications/Graphics
Vendor: Little Cambridgeport Design Factory
Packager: Eddie Kohler <eddietwo@lcs.mit.edu>
Copyright: GPL

BuildRoot: /tmp/gifsicle-build

%description
Gifsicle manipulates GIF image files on the
command line. It supports merging several GIFs
into a GIF animation; exploding an animation into
its component frames; changing individual frames
in an animation; turning interlacing on and off;
adding transparency; adding delays, disposals, and
looping to animations; adding or removing
comments; optimizing animations for space; and
changing images' colormaps, among other things.

The gifsicle package contains two other programs:
gifview, a lightweight GIF viewer for X, can show
animations as slideshows or in real time, and
gifdiff compares two GIFs for identical visual
appearance.

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --enable-gifview --enable-gifdiff
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin $RPM_BUILD_ROOT/usr/man/man1
mkdir -p $RPM_BUILD_ROOT/usr/X11R6/bin $RPM_BUILD_ROOT/usr/X11R6/man/man1
install -c -s gifsicle $RPM_BUILD_ROOT/usr/bin/gifsicle
install -c -s gifview $RPM_BUILD_ROOT/usr/X11R6/bin/gifview
install -c -s gifdiff $RPM_BUILD_ROOT/usr/bin/gifdiff
install -c -m 644 gifsicle.man $RPM_BUILD_ROOT/usr/man/man1/gifsicle.1
install -c -m 644 gifview.man $RPM_BUILD_ROOT/usr/X11R6/man/man1/gifview.1
install -c -m 644 gifdiff.man $RPM_BUILD_ROOT/usr/man/man1/gifdiff.1

%clean
rm -rf $RPM_BUILD_ROOT

%post

%files
%doc NEWS README
%attr(0755,root,root) /usr/bin/gifsicle
%attr(0755,root,root) /usr/bin/gifdiff
%attr(0755,root,root) /usr/X11R6/bin/gifview
%attr(0644,root,root) /usr/man/man1/gifsicle.1
%attr(0644,root,root) /usr/man/man1/gifdiff.1
%attr(0644,root,root) /usr/X11R6/man/man1/gifview.1
