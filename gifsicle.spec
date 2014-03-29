Summary:	GIF image and animation manipulator

Name:		gifsicle
Version:	1.82
Release:	1
Source:		http://www.lcdf.org/gifsicle/gifsicle-1.82.tar.gz

Icon:		logo1.gif
URL:		http://www.lcdf.org/gifsicle/

Group:		Applications/Graphics
Vendor:		Little Cambridgeport Design Factory
Packager:	Eddie Kohler <ekohler@gmail.com>
License:	GPL

BuildRoot:	/tmp/gifsicle-build

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
%configure
make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT

%post

%files
%attr(-,root,root) %doc NEWS README
%attr(0755,root,root) %{_bindir}/gifsicle
%attr(0755,root,root) %{_bindir}/gifdiff
%attr(0755,root,root) %{_bindir}/gifview
%attr(0644,root,root) %{_mandir}/man1/gifsicle.1*
%attr(0644,root,root) %{_mandir}/man1/gifdiff.1*
%attr(0644,root,root) %{_mandir}/man1/gifview.1*
