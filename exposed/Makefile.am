## exposed/Makefile.am

AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -DLIBRARY -I$(top_srcdir)/src

AM_LDFLAGS = -L.

noinst_PROGRAMS  = CDemo
noinst_LIBRARIES = libslvs.a

CDemo_SOURCES = CDemo.c

libslvs_a_SOURCES = \
	slvs.h \
	lib.cpp \
	../src/util.cpp \
	../src/entity.cpp \
	../src/expr.cpp \
	../src/constraint.cpp \
	../src/constrainteq.cpp \
	../src/system.cpp

if HAVE_FLTK
CDemo_SOURCES += ../src/fltk/fltkutil.cpp
endif
if WIN32
CDemo_SOURCES += ../src/win32/w32util.cpp
endif

CDemo_LDADD = -lslvs $(FLTK_LDSTATICFLAGS)

EXTRA_DIST = \
	DOC.txt \
	Makefile.msvc \
	VbDemo.vb

## end exposed/Makefile.am
