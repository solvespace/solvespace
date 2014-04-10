## src/Makefile.am

AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = $(FLTK_CXXFLAGS) $(LIBSPNAV_CPPFLAGS)

if WIN32
AM_CPPFLAGS += \
	-I$(top_srcdir)/extlib/libpng \
	-I$(top_srcdir)/extlib/si \
	-I$(top_srcdir)/extlib/zlib
endif

bin_PROGRAMS = solvespace

built_src = \
	bitmapextra.table.h \
	bitmapfont.table.h \
	icons-proto.h \
	icons.h

BUILT_SOURCES = $(built_src)

solvespace_SOURCES = \
	$(built_src) \
	bsp.cpp \
	clipboard.cpp \
	confscreen.cpp \
	constraint.cpp \
	constrainteq.cpp \
	describescreen.cpp \
	draw.cpp \
	drawconstraint.cpp \
	drawentity.cpp \
	dsc.h \
	entity.cpp \
	export.cpp \
	exportstep.cpp \
	exportvector.cpp \
	expr.h \
	expr.cpp \
	file.cpp \
	font.table.h \
	generate.cpp \
	glhelper.cpp \
	graphicswin.cpp \
	group.cpp \
	groupmesh.cpp \
	mesh.cpp \
	modify.cpp \
	mouse.cpp \
	polygon.h \
	polygon.cpp \
	request.cpp \
	sketch.h \
	solvespace.h \
	solvespace.cpp \
	style.cpp \
	system.cpp \
	textscreens.cpp \
	textwin.cpp \
	toolbar.cpp \
	ttf.cpp \
	ui.h \
	undoredo.cpp \
	util.cpp \
	view.cpp \
	srf/boolean.cpp \
	srf/curve.cpp \
	srf/merge.cpp \
	srf/ratpoly.cpp \
	srf/raycast.cpp \
	srf/surface.h \
	srf/surface.cpp \
	srf/surfinter.cpp \
	srf/triangulate.cpp

if HAVE_FLTK
solvespace_SOURCES += \
	fltk/xFl_Gl_Window_Group.H \
	fltk/xFl_Gl_Window_Group.cxx \
	fltk/fltkmain.cpp \
	fltk/fltkutil.cpp

solvespace_LDADD = $(FLTK_LDSTATICFLAGS) -lGLU $(LIBSPNAV_LIBS)
endif

if WIN32
solvespace_SOURCES += \
	win32/freeze.h \
	win32/freeze.cpp \
	win32/w32main.cpp \
	win32/w32util.cpp

if MINGW
solvespace_LDFLAGS = \
	-llibpng \
	-lzlib \
	-luser32 -lgdi32 -lcomctl32 -ladvapi32 -lshell32 \
	-lopengl32 -lglu32
else ! MINGW
solvespace_LDFLAGS = -link \
	-libpath:$(top_srcdir)/extlib/libpng libpng.lib \
	-libpath:$(top_srcdir)/extlib/zlib zlib.lib \
	user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib \
	opengl32.lib glu32.lib
endif ! MINGW
endif WIN32

icons = \
	icons/angle.png \
	icons/arc.png \
	icons/assemble.png \
	icons/bezier.png \
	icons/char-0-check-false.png \
	icons/char-1-check-true.png \
	icons/char-2-radio-false.png \
	icons/char-3-radio-true.png \
	icons/circle.png \
	icons/constraint.png \
	icons/construction.png \
	icons/edges.png \
	icons/equal.png \
	icons/extrude.png \
	icons/faces.png \
	icons/hidden-lines.png \
	icons/horiz.png \
	icons/in3d.png \
	icons/length.png \
	icons/line.png \
	icons/mesh.png \
	icons/normal.png \
	icons/ontoworkplane.png \
	icons/other-supp.png \
	icons/parallel.png \
	icons/perpendicular.png \
	icons/point.png \
	icons/pointonx.png \
	icons/rectangle.png \
	icons/ref.png \
	icons/same-orientation.png \
	icons/shaded.png \
	icons/sketch-in-3d.png \
	icons/sketch-in-plane.png \
	icons/step-rotate.png \
	icons/step-translate.png \
	icons/symmetric.png \
	icons/tangent-arc.png \
	icons/text.png \
	icons/trim.png \
	icons/vert.png \
	icons/workplane.png

EXTRA_DIST = \
	$(icons) \
	png2c.pl \
	pngchar2c.pl \
	win32/icon.ico \
	win32/manifest.xml \
	win32/resource.rc

if MAINTAINER_MODE

icons.h: $(icons) $(srcdir)/png2c.pl
	$(PERL) $(srcdir)/png2c.pl icons.h icons-proto.h $(srcdir)

icons-proto.h: icons.h
	@exit 0

bitmapextra.table.h: $(icons) $(srcdir)/pngchar2c.pl
	$(PERL) $(srcdir)/pngchar2c.pl $(srcdir) >bitmapextra.table.tmp
	mv -f bitmapextra.table.tmp bitmapextra.table.h

endif MAINTAINER_MODE

if HAVE_PREBUILT_SOURCE
if ! MAINTAINER_MODE

icons.h: $(srcdir)/built/icons.h
	cp $(srcdir)/built/icons.h icons.h

icons-proto.h: $(srcdir)/built/icons-proto.h
	cp $(srcdir)/built/icons-proto.h icons-proto.h

bitmapextra.table.h: $(srcdir)/built/bitmapextra.table.h
	cp $(srcdir)/built/bitmapextra.table.h bitmapextra.table.h

endif ! MAINTAINER_MODE

bitmapfont.table.h: $(srcdir)/built/bitmapfont.table.h
	cp $(srcdir)/built/bitmapfont.table.h bitmapfont.table.h

endif HAVE_PREBUILT_SOURCE

run-valgrind: solvespace$(EXEEXT)
	@test -z "$$VALGRIND_OPTS" || echo VALGRIND_OPTS = $$VALGRIND_OPTS
	valgrind \
		--tool=memcheck \
		--verbose \
		--track-fds=yes \
		--log-file=vg.%p.out \
		--num-callers=50 \
		--error-limit=no \
		--read-var-info=yes \
		--leak-check=full \
		--leak-resolution=high \
		--show-reachable=yes \
		--track-origins=yes \
		--malloc-fill=0xac \
		--free-fill=0xde \
		./solvespace$(EXEEXT)

## end src/Makefile.am
