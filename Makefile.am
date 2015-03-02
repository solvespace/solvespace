## Makefile.am

ACLOCAL_AMFLAGS = -I ac-aux

SUBDIRS = src exposed

noinst_HEADERS = include/slvs.h

EXTRA_DIST = \
	COPYING.txt \
	Makefile.msvc \
	tools/Makefile \
	tools/ttf2c.cpp \
	wishlist.txt \
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
	extlib/zlib/zlib.lib

dist-hook:
	@if fgrep '/DPACKAGE_VERSION="\"$(PACKAGE_VERSION)\""' $(srcdir)/Makefile.msvc >/dev/null; \
	then :; \
	else \
		echo 'error: /DPACKAGE_VERSION flag in Makefile.msvc is out-of-date'; \
		echo '(current package version is $(PACKAGE_VERSION))'; \
		exit 1; \
	fi

## end Makefile.am
