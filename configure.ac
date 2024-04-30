#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([ucto],[0.34],[lamasoftware@science.ru.nl]) #adapt version number in codemeta.json as well
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

SYSCONFDIR=$sysconfdir
AC_SUBST([SYSCONFDIR])

# Checks for programs.
AC_PROG_CXX( [g++ c++] )

# use libtool
LT_INIT
AC_PROG_LN_S

# when running tests, use CXX
AC_LANG([C++])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDBOOL

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_INT8_T
AC_TYPE_INT32_T

# Checks for library functions.

AX_LIB_READLINE

if test $prefix = "NONE"; then
   prefix="$ac_default_prefix"
fi

PKG_PROG_PKG_CONFIG

if test "x$PKG_CONFIG_PATH" = x; then
    export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
else
    export PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"
fi

AC_OSX_PKG( [icu4c libtextcat] )
AC_MSG_NOTICE([pkg-config: PATH=$PKG_CONFIG_PATH])

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

if test $TEXTCAT_FOUND = 0; then
# So, no pkg-config for textcat found.
#     Hopefully an old style version can be found???
  AC_CHECK_OLD_TEXTCAT
  if test $TEXTCAT_FOUND = 1; then
    AC_DEFINE([HAVE_OLD_TEXTCAT], [1], [textcat needs C linkage])
  fi
fi

if test $TEXTCAT_FOUND = 1; then
  AC_SEARCH_LM
fi

if test $TEXTCAT_FOUND = 0; then
   AC_MSG_ERROR( [no working libtextcat or libexttextcat found!] )
else
   AC_DEFINE([HAVE_TEXTCAT], [1], [textcat])
fi

PKG_CHECK_MODULES([ICU], [icu-uc >= 50 icu-io] )
CXXFLAGS="$CXXFLAGS $ICU_CFLAGS"
LIBS="$ICU_LIBS $LIBS"

PKG_CHECK_MODULES([XML2], [libxml-2.0 >= 2.9.4] )
CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
LIBS="$LIBS $XML2_LIBS"

PKG_CHECK_MODULES([folia], [folia >= 2.19] )
CXXFLAGS="$folia_CFLAGS $CXXFLAGS"
LIBS="$folia_LIBS $LIBS"

PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.34] )
CXXFLAGS="$CXXFLAGS $ticcutils_CFLAGS"
LIBS="$LIBS $ticcutils_LIBS"

PKG_CHECK_MODULES(
	[uctodata],
	[uctodata],
	[PKG_CHECK_MODULES(
		[uctodata],
		[uctodata >= 0.9],
		[],
		[AC_MSG_NOTICE([ATTENTION:
	Your ucto datafiles are outdated
	You should consider installing a newer version of the uctodata package!])])

	],
	[AC_MSG_ERROR([ATTENTION:
	ucto datafiles are not installed!. Please install uctodata package.] )] )

UCTODATA_PRE=`pkg-config --variable=prefix uctodata`
UCTODATA_DIR=$UCTODATA_PRE/share/ucto
AC_DEFINE_UNQUOTED( [UCTODATA_DIR],
		    ["$UCTODATA_DIR/"],
		    [location of uctodata files])
UCTODATA_VER=`pkg-config --modversion uctodata`
AC_DEFINE_UNQUOTED( [UCTODATA_VERSION],
	   	    ["${UCTODATA_VER}"],
		    [the uctodata version])

# Checks for library functions.

AC_CONFIG_FILES([
  Makefile
  ucto.pc
  m4/Makefile
  config/Makefile
  config/textcat_normal.cfg
  config/textcat_alt.cfg
  docs/Makefile
  src/Makefile
  tests/Makefile
  include/Makefile
  include/ucto/Makefile
])
AC_OUTPUT
