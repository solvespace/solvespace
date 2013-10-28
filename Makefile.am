## Makefile.am

ACLOCAL_AMFLAGS = -I ac-aux

AM_CPPFLAGS = $(FLTK_CXXFLAGS)

if WIN32
AM_CPPFLAGS += \
	-I$(srcdir)/extlib/libpng \
	-I$(srcdir)/extlib/si \
	-I$(srcdir)/extlib/zlib
endif

bin_PROGRAMS = solvespace

icons_src = \
	icons.h \
	icons-proto.h

BUILT_SOURCES = $(icons_src)

solvespace_SOURCES = \
	$(icons_src) \
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

solvespace_LDADD = $(FLTK_LDSTATICFLAGS) -lGLU
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
else
solvespace_LDFLAGS = -link \
	-libpath:$(srcdir)/extlib/libpng libpng.lib \
	-libpath:$(srcdir)/extlib/zlib zlib.lib \
	user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib \
	opengl32.lib glu32.lib
endif # MINGW
endif # WIN32

icons = \
	icon.ico \
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

tables = \
	bitmapextra.table.h \
	bitmapfont.table.h \
	font.table.h

exposed = \
	exposed/CDemo.c \
	exposed/DOC.txt \
	exposed/Makefile \
	exposed/VbDemo.vb \
	exposed/lib.cpp \
	exposed/slvs.h

EXTRA_DIST = \
	$(icons) \
	$(tables) \
	$(exposed) \
	COPYING.txt \
	Makefile.msvc \
	extlib/build-fltk.sh \
	png2c.pl \
	pngchar2c.pl \
	tools/Makefile \
	tools/ttf2c.cpp \
	wishlist.txt \
	win32/manifest.xml \
	win32/resource.rc

optional_dist = \
	extlib/libpng/png.h \
	extlib/libpng/pngconf.h \
	extlib/libpng/pnglibconf.h \
	extlib/libpng/libpng.lib \
	extlib/si/si.h \
	extlib/si/siSync.h \
	extlib/si/siSyncPriv.h \
	extlib/si/siapp.h \
	extlib/si/spwdata.h \
	extlib/si/spwerror.h \
	extlib/si/spwmacro.h \
	extlib/si/siapp.lib \
	extlib/zlib/zconf.h \
	extlib/zlib/zlib.h \
	extlib/zlib/zutil.h \
	extlib/zlib/zlib.lib \
	extlib/fltk-1.3.2-source.tar.gz

dist-hook:
	for file in $(optional_dist); do \
		test -f $(srcdir)/$$file || continue; \
		dir=`dirname $$file`; \
		test -d $(distdir)/$$dir || mkdir $(distdir)/$$dir || exit; \
		cp -p $(srcdir)/$$file $(distdir)/$$file || exit; \
	done
	@if fgrep '/DPACKAGE_VERSION="\"$(PACKAGE_VERSION)\""' $(srcdir)/Makefile.msvc >/dev/null; \
	then :; \
	else \
		echo 'error: /DPACKAGE_VERSION flag in Makefile.msvc is out-of-date'; \
		echo '(current package version is $(PACKAGE_VERSION))'; \
		exit 1; \
	fi

if MAINTAINER_MODE

icons.h: $(icons) $(srcdir)/png2c.pl
	$(PERL) $(srcdir)/png2c.pl $@ icons-proto.h $(srcdir)

icons-proto.h: icons.h
	@exit 0

bitmapextra.table.h: $(icons) $(srcdir)/pngchar2c.pl
	$(PERL) $(srcdir)/pngchar2c.pl $(srcdir) >$@.tmp
	mv -f $@.tmp $@

endif # MAINTAINER_MODE

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

## end Makefile.am
