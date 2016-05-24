#
# spec file for package waon (Version 0.9)
#
# Copyright (C) 2007 Kengo Ichiki <kichiki@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
Name:         waon
License:      GPL
Group:        Productivity/Multimedia/Sound/Utilities
Summary:      A Wave-to-Notes Transcriber and Some Utility Tools
Version:      0.9
Release:      0
URL:          http://waon.sourceforge.net/
Source:       %{name}-%{version}.tar.gz
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
Packager:     Kengo Ichiki
# Distribution: openSUSE

Requires: fftw3, fftw3-devel
Requires: libsndfile, libsndfile-devel
Requires: libao, libao-devel
Requires: libsamplerate, libsamplerate-devel
Requires: gtk2, gtk2-devel

%description
WaoN is a Wave-to-Notes transcriber (converts audio file into midi file)
and some utility tools such as gWaoN, graphical visualization of the
spectra, and phase vocoder for time-stretching and pitch-shifting.

%prep
# extract the source and go into the source directory
%setup -q

%build
make

%install
mkdir $RPM_BUILD_ROOT/usr
mkdir $RPM_BUILD_ROOT/usr/bin
mkdir $RPM_BUILD_ROOT/usr/share
mkdir $RPM_BUILD_ROOT/usr/share/man
mkdir $RPM_BUILD_ROOT/usr/share/man/man1
install -s waon  $RPM_BUILD_ROOT/usr/bin
install -s pv    $RPM_BUILD_ROOT/usr/bin
install -s gwaon $RPM_BUILD_ROOT/usr/bin
install waon.1  $RPM_BUILD_ROOT/usr/share/man/man1
install pv.1    $RPM_BUILD_ROOT/usr/share/man/man1
install gwaon.1 $RPM_BUILD_ROOT/usr/share/man/man1
gzip $RPM_BUILD_ROOT/usr/share/man/man1/waon.1
gzip $RPM_BUILD_ROOT/usr/share/man/man1/pv.1
gzip $RPM_BUILD_ROOT/usr/share/man/man1/gwaon.1

%clean
# clean up the hard disc after build
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
/usr/bin/waon
/usr/bin/pv
/usr/bin/gwaon
/usr/share/man/man1/waon.1.gz
/usr/share/man/man1/pv.1.gz
/usr/share/man/man1/gwaon.1.gz

%changelog
* Mon Nov  5 2007 Kengo Ichiki  <kichiki@users.sourceforge.net>
- update for waon-0.9.
* Wed Oct 10 2007 Kengo Ichiki  <kichiki@users.sourceforge.net>
- start writing spec file.
