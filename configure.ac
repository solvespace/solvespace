## configure.ac

AC_PREREQ([2.59])
AC_INIT([SolveSpace], [2.1], [jwesthues@cq.cx], [solvespace])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([solvespace.cpp])
AC_CONFIG_AUX_DIR([ac-aux])
AC_CONFIG_MACRO_DIR([ac-aux])

AM_INIT_AUTOMAKE([1.9.6 foreign tar-ustar])
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_CXX

AC_PROG_INSTALL
dnl AC_PROG_MAKE_SET
AC_HEADER_STDC
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE

# Check for Libtool
#
dnl AC_DISABLE_SHARED
LT_INIT

# Do we have Perl?
#
AC_CHECK_PROG([PERL], [perl], [perl], [false])

# Check for headers that define integer types
#
AC_CHECK_HEADERS([inttypes.h stdint.h sys/socket.h])

# Check for various integer types
#
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_SSIZE_T
AC_DEFINE([HAVE_C99_INTEGER_TYPES], [1], [Define to 1 if (self-explanatory).])

# Check the size of various types
#
AC_CHECK_SIZEOF([char])
AC_CHECK_SIZEOF([short])
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([long])
AC_CHECK_SIZEOF([float])
AC_CHECK_SIZEOF([double])
AC_CHECK_SIZEOF([long double])
AC_CHECK_SIZEOF([void *])
AC_CHECK_SIZEOF([size_t])

##
## Windows support
##

AC_EXEEXT
AC_OBJEXT

win32=no
AC_EGREP_CPP([SOLVESPACE_WIN32],dnl
[#if (defined(_MSC_VER) && defined(_WIN32)) || defined(__MINGW32__)
SOLVESPACE_WIN32
#endif], [win32=yes])

AM_CONDITIONAL([WIN32], [test "$win32" = yes])

mingw=no
AC_EGREP_CPP([SOLVESPACE_MINGW],dnl
[#if defined(__MINGW32__)
SOLVESPACE_MINGW
#endif], [mingw=yes])

AM_CONDITIONAL([MINGW], [test "$mingw" = yes])

AH_VERBATIM([MSVC_FLAGS], [#if defined(_MSC_VER) && defined(_WIN32)
# define _CRT_SECURE_NO_DEPRECATE 1
# define _CRT_SECURE_NO_WARNINGS 1
# define _WIN32_WINNT 0x500
# define _WIN32_IE _WIN32_WINNT
# define ISOLATION_AWARE_ENABLED 1
# define WIN32 1
# define WIN32_LEAN_AND_MEAN 1
#endif])

##
## FLTK
##

AX_FLTK([1.3], [--use-gl --use-images --use-forms])
AM_CONDITIONAL([HAVE_FLTK], [test "$have_fltk" = yes])

if test "$have_fltk" = yes
then
	x=`echo "$FLTK_VERSION" | cut -d. -f1`
	y=`echo "$FLTK_VERSION" | cut -d. -f2`
	z=`echo "$FLTK_VERSION" | cut -d. -f3`

	if test `expr 10000 '*' $x + 100 '*' $y + $z` -ge 10301
	then
		AC_DEFINE([HAVE_FLTK_FULLSCREEN], [1],
			[Define to 1 if your copy of FLTK has proper fullscreen support.])
	fi
fi

##
## Wrap it up
##

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

cat <<EOF

$PACKAGE-$VERSION configuration summary:

    Install prefix ......... : ${prefix}
    C++ preprocessor ....... : ${CXXCPP}
    C++ compiler ........... : ${CXX}
    Linker ................. : ${LD}
    C preprocessor flags ... : ${CPPFLAGS}
    C++ compiler flags ..... : ${CXXFLAGS}
    Linker flags ........... : ${LDFLAGS}
    Extra libraries ........ : ${LIBS}

EOF

## end configure.ac
