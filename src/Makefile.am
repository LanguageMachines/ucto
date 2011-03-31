# $Id$
# $URL $

AM_CPPFLAGS = -I@top_srcdir@/include

bin_PROGRAMS = ucto

LDADD = libucto.la

ucto_SOURCES = ucto.cxx

lib_LTLIBRARIES = libucto.la
libucto_la_LDFLAGS = -version-info 1:0:0

libucto_la_SOURCES =  tokenize.cxx unicode.cxx
