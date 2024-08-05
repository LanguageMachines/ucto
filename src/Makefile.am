AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -DSYSCONF_PATH=\"$(datadir)\" -std=c++17 -W -Wall -pedantic -g -O3

bin_PROGRAMS = ucto

LDADD = libucto.la

ucto_SOURCES = ucto.cxx

lib_LTLIBRARIES = libucto.la
libucto_la_LDFLAGS = -version-info 6:0:0

libucto_la_SOURCES = my_textcat.cxx setting.cxx tokenize.cxx

TESTS = tst.sh

EXTRA_DIST = tst.sh
CLEANFILES = tst.out
