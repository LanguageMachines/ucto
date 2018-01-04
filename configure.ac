#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([ucto], [0.12], [lamasoftware@science.ru.nl])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([config.h])

SYSCONFDIR=$sysconfdir
AC_SUBST([SYSCONFDIR])

if test x"${CXXFLAGS+set}" = xset; then
  # the user set CXXFLAGS; don't override it.
  cxx_flags_were_set=true
else
  cxx_flags_were_set=false
fi

# Checks for programs.
AC_PROG_CXX( [c++] )

if $cxx_flags_were_set; then
  CXXFLAGS=$CXXFLAGS
fi

# use libtool
AC_PROG_LIBTOOL
LT_INIT

# when running tests, use CXX
AC_LANG([C++])

# Checks for libraries.

if test $prefix = "NONE"; then
   prefix="$ac_default_prefix"
fi

# Checks for header files.
AC_HEADER_STDBOOL

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_INT32_T

# Checks for library functions.

AX_LIB_READLINE

#AC_CHECK_HEADERS([libexttextcat/textcat.h],
#	         [CXXFLAGS="$CXXFLAGS -I$prefix/include"],
#	         [AC_CHECK_HEADERS([libtextcat/textcat.h],
#                                   [CXXFLAGS="$CXXFLAGS -I$prefix/include"],
#		                   [AC_CHECK_HEADERS([textcat.h],
#						     [],
#		                                     [AC_MSG_NOTICE([textcat support not available])])])])
#
#AC_SEARCH_LIBS([textcat_Init],[exttextcat-2.0 exttextcat textcat],[AC_DEFINE(HAVE_TEXTCAT_LIB, 1, textcat_lib)],
#  [AC_MSG_NOTICE([Unable to find textcat library. textcat support not available])])


if test $prefix = "NONE"; then
   prefix="$ac_default_prefix"
fi

PKG_PROG_PKG_CONFIG

if test "x$prefix" = "xNONE"; then
   prefix="/usr/local"
fi

if test "x$PKG_CONFIG_PATH" = x; then
    export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
else
    export PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"
fi

AC_OSX_PKG( [icu4c] )
AC_MSG_NOTICE([pkg-config: PATH=$PKG_CONFIG_PATH])

AC_ARG_WITH(icu,
       [  --with-icu=DIR       use icu installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [])


PKG_CHECK_MODULES( [TEXTCAT],
  [libexttextcat],
  [TEXTCAT_FOUND=1
   CXXFLAGS="$CXXFLAGS $TEXTCAT_CFLAGS"
   LIBS="$TEXTCAT_LIBS $LIBS"],
  [PKG_CHECK_MODULES( [TEXTCAT],
   [libtextcat],
   [TEXTCAT_FOUND=1
    CXXFLAGS="$CXXFLAGS $TEXTCAT_CFLAGS"
    LIBS="$TEXTCAT_LIBS $LIBS"],
   [TEXTCAT_FOUND=0]
   )]
 )

if test $TEXTCAT_FOUND = 1; then
   AC_DEFINE([HAVE_TEXTCAT], [1], [textcat])
else
# final resort... for OSX at least
   AC_CHECK_HEADERS([libtextcat/textcat.h],
	[CXXFLAGS="$CXXFLAGS $prefix/include/libtextcat"
	 AC_SEARCH_LIBS([textcat_Init],
	  [libtextcat],
	  [AC_DEFINE([HAVE_TEXTCAT], [1], [textcat])],
          [AC_MSG_NOTICE([textcat library not available])])],
        [AC_MSG_NOTICE([textcat include file not available])])
fi

PKG_CHECK_MODULES([ICU], [icu-uc >= 50 icu-io] )
CXXFLAGS="$CXXFLAGS $ICU_CFLAGS"
LIBS="$ICU_LIBS $LIBS"

PKG_CHECK_MODULES([XML2], [libxml-2.0 >= 2.6.16] )
CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
LIBS="$LIBS $XML2_LIBS"

PKG_CHECK_MODULES([folia], [folia >= 1.10] )
CXXFLAGS="$folia_CFLAGS $CXXFLAGS"
LIBS="$folia_LIBS $LIBS"

PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.17] )
CXXFLAGS="$CXXFLAGS $ticcutils_CFLAGS"
LIBS="$LIBS $ticcutils_LIBS"

PKG_CHECK_MODULES(
	[uctodata],
	[uctodata],
	[PKG_CHECK_MODULES(
		[uctodata],
		[uctodata >= 0.4],
		[],
		[AC_MSG_NOTICE([ATTENTION:
	Your ucto datafiles are are outdated
	You should consider installing a newer version of the uctodata package!])])

	],
	[AC_MSG_NOTICE([ATTENTION:
	ucto datafiles are not installed!
	ucto will work with only a minimal default configuration.
	You should consider installing the uctodata package!] )] )
# Checks for library functions.

AC_OUTPUT([
  Makefile
  ucto.pc
  m4/Makefile
  config/Makefile
  docs/Makefile
  src/Makefile
  tests/Makefile
  include/Makefile
  include/ucto/Makefile
])
