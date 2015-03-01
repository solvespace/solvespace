# ===========================================================================
#	http://www.gnu.org/software/autoconf-archive/ax_fltk.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_FLTK(API-VERSION[,
#           USE-OPTIONS[,
#           ACTION-IF-FOUND[,
#           ACTION-IF-NOT-FOUND]]])
#
# DESCRIPTION
#
#   This macro checks for the presence of an installed copy of FLTK
#   matching the specified API-VERSION (which can be "1.1", "1.3" and so
#   on). If found, the following variables are set and AC_SUBST'ed with
#   values obtained from the fltk-config script:
#
#	FLTK_VERSION
#	FLTK_API_VERSION
#	FLTK_CC
#	FLTK_CXX
#	FLTK_OPTIM
#	FLTK_CFLAGS
#	FLTK_CXXFLAGS
#	FLTK_LDFLAGS
#	FLTK_LDSTATICFLAGS
#	FLTK_LIBS
#	FLTK_PREFIX
#	FLTK_INCLUDEDIR
#
#   USE-OPTIONS is a space-separated set of --use-* options to pass to the
#   fltk-config script when populating the above variables.
#
#   If you are using FLTK extensions (e.g. OpenGL support, extra image
#   libraries, Forms compatibility), then you can specify a set of
#   space-separated options like "--use-gl", "--use-images" etc. for
#   USE-OPTIONS.
#
#   ACTION-IF-FOUND is a list of shell commands to run if a matching
#   version of FLTK is found, and ACTION-IF-NOT-FOUND is a list of commands
#   to run it if it is not found. If ACTION-IF-FOUND is not specified, the
#   default action will define HAVE_FLTK.
#
#   Please let the author(s) know if this macro fails on any platform, or
#   if you have any other suggestions or comments.
#
# LICENSE
#
#   Copyright (c) 2013 Daniel Richard G. <skunk@iSKUNK.ORG>
#
#   This program is free software: you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation, either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 0

AC_DEFUN([AX_FLTK], [
AC_LANG_PUSH([C++])

m4_bmatch([$1], [^[1-9]\.[0-9]$], [],
	[m4_fatal([invalid FLTK API version "$1"])])
m4_bmatch([$2], [^\(\(--use-[a-z]+\)\( +--use-[a-z]+\)*\)?$], [],
	[m4_fatal([invalid fltk-config use-options string "$2"])])

AC_ARG_WITH([fltk],
	[AS_HELP_STRING([--with-fltk=PREFIX],
		[use FLTK $1 libraries installed in PREFIX])])

case "_$with_fltk" in
	_no)
	FLTK_CONFIG=
	;;

	_|_yes)
	AC_PATH_PROG([FLTK_CONFIG], [fltk-config])
	;;

	*)
	AC_PATH_PROG([FLTK_CONFIG], [fltk-config], , [$with_fltk/bin])
	;;
esac

FLTK_VERSION=
FLTK_API_VERSION=

FLTK_CC=
FLTK_CXX=
FLTK_OPTIM=
FLTK_CFLAGS=
FLTK_CXXFLAGS=
FLTK_LDFLAGS=
FLTK_LDSTATICFLAGS=
FLTK_LIBS=
FLTK_PREFIX=
FLTK_INCLUDEDIR=

have_fltk=no

if test -n "$FLTK_CONFIG"
then
	FLTK_VERSION=`$FLTK_CONFIG --version`
	FLTK_API_VERSION=`$FLTK_CONFIG --api-version`

	AC_MSG_CHECKING([for FLTK API version $1])

	if test "_$FLTK_API_VERSION" = "_$1"
	then
		have_fltk=yes
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
	fi

	fl_opt="$2"

	FLTK_CC=`$FLTK_CONFIG $fl_opt --cc`
	FLTK_CXX=`$FLTK_CONFIG $fl_opt --cxx`
	FLTK_OPTIM=`$FLTK_CONFIG $fl_opt --optim`
	FLTK_CFLAGS=`$FLTK_CONFIG $fl_opt --cflags`
	FLTK_CXXFLAGS=`$FLTK_CONFIG $fl_opt --cxxflags`
	FLTK_LDFLAGS=`$FLTK_CONFIG $fl_opt --ldflags`
	FLTK_LDSTATICFLAGS=`$FLTK_CONFIG $fl_opt --ldstaticflags`
	FLTK_LIBS=`$FLTK_CONFIG $fl_opt --libs`
	FLTK_PREFIX=`$FLTK_CONFIG $fl_opt --prefix`
	FLTK_INCLUDEDIR=`$FLTK_CONFIG $fl_opt --includedir`
fi

# Finally, execute ACTION-IF-FOUND / ACTION-IF-NOT-FOUND:
#
if test "$have_fltk" = yes
then
:
ifelse([$3], ,
	[AC_DEFINE([HAVE_FLTK], [1], [Define if you have the FLTK libraries.])],
	[$3])
else
:
$4
fi

AC_SUBST([FLTK_VERSION])
AC_SUBST([FLTK_API_VERSION])

AC_SUBST([FLTK_CC])
AC_SUBST([FLTK_CXX])
AC_SUBST([FLTK_OPTIM])
AC_SUBST([FLTK_CFLAGS])
AC_SUBST([FLTK_CXXFLAGS])
AC_SUBST([FLTK_LDFLAGS])
AC_SUBST([FLTK_LDSTATICFLAGS])
AC_SUBST([FLTK_LIBS])
AC_SUBST([FLTK_PREFIX])
AC_SUBST([FLTK_INCLUDEDIR])

AC_LANG_POP
])dnl AX_FLTK
