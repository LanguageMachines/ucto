AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -DSYSCONF_PATH=\"$(datadir)\" -std=c++11 -W -Wall -pedantic -O3 -g

bin_PROGRAMS = ucto

LDADD = libucto.la

ucto_SOURCES = ucto.cxx

lib_LTLIBRARIES = libucto.la
libucto_la_LDFLAGS = -version-info 3:0:0

libucto_la_SOURCES = my_textcat.cxx setting.cxx tokenize.cxx

TESTS = tst.sh

EXTRA_DIST = tst.sh
CLEANFILES = tst.out
