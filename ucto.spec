# $Id$
# $URL$

Summary: Unicode Tokenizer
Name: ucto
Version: 0.4.1
Release: 1
License: GPL
Group: Applications/System
URL: http://ilk.uvt.nl/

Packager: Joost van Baal <joostvb-ilk@ad1810.com>
Vendor: ILK, http://ilk.uvt.nl/

Source: http://ilk.uvt.nl/downloads/pub/software/ucto-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

Requires: libicu
BuildRequires: gcc-c++, libicu-devel

%description
Ucto can tokenize UTF-8 encoded text files (i.e. separate words from
punctuation, split sentences, generate n-grams), and  offers several other
basic preprocessing steps (change case, count words/characters and reverse
lines) that make your text suited for further processing such as indexing,
part-of-speech tagging, or machine translation.

Ucto is a product of the ILK Research Group, Tilburg University (The
Netherlands).

If you are interested in machine parsing of UTF-8 encoded text files, e.g. to
do scientific research in natural language processing, ucto will likely be of
use to you.

%prep
%setup

%build
%configure

%install
%{__rm} -rf %{buildroot}
%makeinstall

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%doc AUTHORS ChangeLog NEWS README TODO
%{_libdir}/libucto*
%{_bindir}/ucto
%{_includedir}/%{name}/*.h
%{_libdir}/pkgconfig/ucto*.pc
%{_mandir}/man*/ucto*
%{_sysconfdir}/%{name}/*

%changelog
* Sat Mar 12 2011 Joost van Baal <joostvb-ilk@ad1810.com> - 0.4.1
- New upstream.
* Sun Feb 27 2011 Joost van Baal <joostvb-ilk@ad1810.com> - 0.3.6-1
- Initial release.

