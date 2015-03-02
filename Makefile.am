## Makefile.am

ACLOCAL_AMFLAGS = -I ac-aux

SUBDIRS = src exposed

noinst_HEADERS = include/slvs.h

exposed = \
	exposed/CDemo.c \
	exposed/DOC.txt \
	exposed/Makefile \
	exposed/VbDemo.vb

EXTRA_DIST = \
	$(exposed) \
	COPYING.txt \
	Makefile.msvc \
	tools/Makefile \
	tools/ttf2c.cpp \
	wishlist.txt

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

## end Makefile.am
