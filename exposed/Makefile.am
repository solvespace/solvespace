## exposed/Makefile.am

AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(top_srcdir)/include

AM_LDFLAGS = -L../src

noinst_PROGRAMS  = CDemo

CDemo_SOURCES = CDemo.c

CDemo_LDADD =

if HAVE_FLTK
CDemo_SOURCES += ../src/fltk/fltkutil.cpp
CDemo_LDADD += $(FLTK_LDFLAGS) -lX11 -lGL -lGLU $(LIBSPNAV_LIBS)
endif
if WIN32
CDemo_SOURCES += ../src/win32/w32util.cpp
endif

CDemo_LDADD += -lslvs

EXTRA_DIST = \
	DOC.txt \
	Makefile.msvc \
	VbDemo.vb

## end exposed/Makefile.am
