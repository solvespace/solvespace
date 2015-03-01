## exposed/Makefile.am

AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -DLIBRARY -I$(top_srcdir)/src

AM_LDFLAGS = -L.

noinst_PROGRAMS  = CDemo
noinst_LIBRARIES = libslvs.a

CDemo_SOURCES = CDemo.c

# Set explicit CPPFLAGS to force renaming of the object files.
# Otherwise, the object files for the GUI application will be overwritten.
libslvs_a_CPPFLAGS = $(AM_CPPFLAGS)

libslvs_a_SOURCES = \
	slvs.h \
	lib.cpp \
	../src/util.cpp \
	../src/entity.cpp \
	../src/expr.cpp \
	../src/constraint.cpp \
	../src/constrainteq.cpp \
	../src/system.cpp

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
