DEFINES = /D_WIN32_WINNT=0x500 /DISOLATION_AWARE_ENABLED /D_WIN32_IE=0x500 /DWIN32_LEAN_AND_MEAN /DWIN32
# Use the multi-threaded static libc because libpng and zlib do; not sure if anything bad
# happens if those mix, but don't want to risk it.
CFLAGS  = /W3 /nologo -MT -Iextlib /D_DEBUG /D_CRT_SECURE_NO_WARNINGS /I. /Zi /EHs  # /O2

HEADERS = win32\freeze.h ui.h solvespace.h dsc.h sketch.h expr.h polygon.h srf\surface.h

OBJDIR = obj

W32OBJS  = $(OBJDIR)\w32main.obj \
           $(OBJDIR)\w32util.obj \
           $(OBJDIR)\freeze.obj \

SSOBJS   = $(OBJDIR)\solvespace.obj \
           $(OBJDIR)\textwin.obj \
           $(OBJDIR)\textscreens.obj \
           $(OBJDIR)\confscreen.obj \
           $(OBJDIR)\describescreen.obj \
           $(OBJDIR)\graphicswin.obj \
           $(OBJDIR)\modify.obj \
           $(OBJDIR)\clipboard.obj \
           $(OBJDIR)\view.obj \
           $(OBJDIR)\util.obj \
           $(OBJDIR)\style.obj \
           $(OBJDIR)\entity.obj \
           $(OBJDIR)\drawentity.obj \
           $(OBJDIR)\group.obj \
           $(OBJDIR)\groupmesh.obj \
           $(OBJDIR)\request.obj \
           $(OBJDIR)\glhelper.obj \
           $(OBJDIR)\expr.obj \
           $(OBJDIR)\constraint.obj \
           $(OBJDIR)\constrainteq.obj \
           $(OBJDIR)\mouse.obj \
           $(OBJDIR)\draw.obj \
           $(OBJDIR)\toolbar.obj \
           $(OBJDIR)\drawconstraint.obj \
           $(OBJDIR)\file.obj \
           $(OBJDIR)\undoredo.obj \
           $(OBJDIR)\system.obj \
           $(OBJDIR)\polygon.obj \
           $(OBJDIR)\mesh.obj \
           $(OBJDIR)\bsp.obj \
           $(OBJDIR)\ttf.obj \
           $(OBJDIR)\generate.obj \
           $(OBJDIR)\export.obj \
           $(OBJDIR)\exportvector.obj \
           $(OBJDIR)\exportstep.obj \

SRFOBJS =  $(OBJDIR)\ratpoly.obj \
           $(OBJDIR)\curve.obj \
           $(OBJDIR)\surface.obj \
           $(OBJDIR)\triangulate.obj \
           $(OBJDIR)\boolean.obj \
           $(OBJDIR)\surfinter.obj \
           $(OBJDIR)\raycast.obj \
           $(OBJDIR)\merge.obj \


RES = $(OBJDIR)\resource.res


LIBS = user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib opengl32.lib glu32.lib \
       extlib\libpng.lib extlib\zlib.lib extlib\si\siapp.lib

all: $(OBJDIR)/solvespace.exe
    @cp $(OBJDIR)/solvespace.exe .

clean:
	rm -f obj/*

$(OBJDIR)/solvespace.exe: $(SRFOBJS) $(SSOBJS) $(W32OBJS) $(FREEZE) $(RES)
    @$(CC) $(DEFINES) $(CFLAGS) -Fe$(OBJDIR)/solvespace.exe $(SSOBJS) $(SRFOBJS) $(W32OBJS) $(FREEZE) $(RES) $(LIBS)
    editbin /nologo /STACK:8388608 $(OBJDIR)/solvespace.exe
    @echo solvespace.exe

$(SSOBJS): $(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj $(@B).cpp

$(SRFOBJS): srf\$(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj srf\$(@B).cpp

$(W32OBJS): win32/$(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj win32/$(@B).cpp

$(RES): win32/$(@B).rc icon.ico
	rc win32/$(@B).rc
	mv win32/$(@B).res $(OBJDIR)/$(@B).res

toolbar.cpp: $(OBJDIR)/icons.h

textwin.cpp: $(OBJDIR)/icons.h

glhelper.cpp: bitmapfont.table font.table bitmapextra.table

$(OBJDIR)/icons.h: icons/* png2c.pl
    perl png2c.pl $(OBJDIR)/icons.h $(OBJDIR)/icons-proto.h

