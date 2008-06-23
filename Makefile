DEFINES = /D_WIN32_WINNT=0x500 /DISOLATION_AWARE_ENABLED /D_WIN32_IE=0x500 /DWIN32_LEAN_AND_MEAN /DWIN32
# Use the multi-threaded static libc because libpng and zlib do; not sure if anything bad
# happens if those mix, but don't want to risk it.
CFLAGS  = /W3 /nologo -MT -Iextlib -I..\common\win32 /D_DEBUG /D_CRT_SECURE_NO_WARNINGS /I. /Zi /EHs

HEADERS = ..\common\win32\freeze.h ui.h solvespace.h dsc.h sketch.h expr.h polygon.h

OBJDIR = obj

FREEZE   = $(OBJDIR)\freeze.obj

W32OBJS  = $(OBJDIR)\w32main.obj \

SSOBJS   = $(OBJDIR)\solvespace.obj \
           $(OBJDIR)\textwin.obj \
           $(OBJDIR)\textscreens.obj \
           $(OBJDIR)\graphicswin.obj \
           $(OBJDIR)\util.obj \
           $(OBJDIR)\entity.obj \
           $(OBJDIR)\drawentity.obj \
           $(OBJDIR)\group.obj \
           $(OBJDIR)\groupmesh.obj \
           $(OBJDIR)\request.obj \
           $(OBJDIR)\glhelper.obj \
           $(OBJDIR)\expr.obj \
           $(OBJDIR)\constraint.obj \
           $(OBJDIR)\draw.obj \
           $(OBJDIR)\drawconstraint.obj \
           $(OBJDIR)\file.obj \
           $(OBJDIR)\undoredo.obj \
           $(OBJDIR)\system.obj \
           $(OBJDIR)\polygon.obj \
           $(OBJDIR)\mesh.obj \
           $(OBJDIR)\bsp.obj \


LIBS = user32.lib gdi32.lib comctl32.lib advapi32.lib opengl32.lib glu32.lib \
       extlib\libpng.lib extlib\zlib.lib

all: $(OBJDIR)/solvespace.exe
    @cp $(OBJDIR)/solvespace.exe .
    solvespace t.slvs

clean:
	rm -f obj/*

$(OBJDIR)/solvespace.exe: $(SSOBJS) $(W32OBJS) $(FREEZE)
    @$(CC) $(DEFINES) $(CFLAGS) -Fe$(OBJDIR)/solvespace.exe $(SSOBJS) $(W32OBJS) $(FREEZE) $(LIBS)
    editbin /nologo /STACK:8388608 $(OBJDIR)/solvespace.exe
    @echo solvespace.exe

$(SSOBJS): $(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj $(@B).cpp

$(W32OBJS): win32/$(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj win32/$(@B).cpp

$(FREEZE): ..\common\win32\$(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj ..\common\win32\$(@B).cpp
