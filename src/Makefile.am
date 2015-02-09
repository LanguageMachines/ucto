# $Id$
# $URL $

AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -DSYSCONF_PATH=\"$(SYSCONFDIR)\" -std=c++11

bin_PROGRAMS = ucto

LDADD = libucto.la

ucto_SOURCES = ucto.cxx

lib_LTLIBRARIES = libucto.la
libucto_la_LDFLAGS = -version-info 2:0:0

libucto_la_SOURCES =  tokenize.cxx unicode.cxx

TESTS = tst.sh

EXTRA_DIST = tst.sh
CLEANFILES = tst.out
