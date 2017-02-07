AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -DSYSCONF_PATH=\"$(datadir)\" -std=c++0x -W -Wall -pedantic -O3 -g

bin_PROGRAMS = ucto

LDADD = libucto.la

ucto_SOURCES = ucto.cxx

lib_LTLIBRARIES = libucto.la
libucto_la_LDFLAGS = -version-info 2:0:0

libucto_la_SOURCES = textcat.cxx unicode.cxx setting.cxx tokenize.cxx

TESTS = tst.sh

EXTRA_DIST = tst.sh
CLEANFILES = tst.out
