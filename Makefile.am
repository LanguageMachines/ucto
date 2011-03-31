# $Id$
# $URL$

ACLOCAL_AMFLAGS = -I m4 --install

SUBDIRS = src include m4 config docs

EXTRA_DIST = bootstrap AUTHORS TODO NEWS ucto.pc.in ucto-icu.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ucto.pc ucto-icu.pc

ChangeLog: NEWS
	svn update; svn2cl --include-rev
