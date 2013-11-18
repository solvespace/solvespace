DEFINES = /D_WIN32_WINNT=0x500 /DISOLATION_AWARE_ENABLED /D_WIN32_IE=0x500 /DWIN32_LEAN_AND_MEAN /DWIN32 /DLIBRARY
# Use the multi-threaded static libc because libpng and zlib do; not sure if anything bad
# happens if those mix, but don't want to risk it.
CFLAGS  = /W3 /nologo -MT -Iextlib -I..\..\common\win32 /D_DEBUG /D_CRT_SECURE_NO_WARNINGS /I. /I.. /Zi /EHs /O2 /GS-

HEADERS = ..\solvespace.h ..\dsc.h ..\sketch.h ..\expr.h slvs.h

OBJDIR = obj

SSOBJS   = $(OBJDIR)\util.obj \
           $(OBJDIR)\entity.obj \
           $(OBJDIR)\expr.obj \
           $(OBJDIR)\constrainteq.obj \
           $(OBJDIR)\system.obj \


W32OBJS  = $(OBJDIR)\w32util.obj \


LIBOBJS  = $(OBJDIR)\lib.obj \


LIBS = user32.lib gdi32.lib comctl32.lib advapi32.lib shell32.lib

all: $(OBJDIR)/CDemo.exe
    @cp $(OBJDIR)/CDemo.exe .
    CDemo.exe

clean:
	rm -f obj/*

$(OBJDIR)/slvs.dll: $(SSOBJS) $(LIBOBJS) $(W32OBJS)
    @$(CC) /LD -Fe$(OBJDIR)/slvs.dll $(SSOBJS) $(LIBOBJS) $(W32OBJS) $(LIBS)
    @cp $(OBJDIR)/slvs.dll .
    @echo slvs.dll

$(OBJDIR)/CDemo.exe: CDemo.c $(OBJDIR)/slvs.dll
    @$(CC) $(CFLAGS) -Fe$(OBJDIR)/CDemo.exe CDemo.c $(OBJDIR)/slvs.lib $(LIBS)
    @echo CDemo.exe

$(SSOBJS): ..\$(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj ..\$(@B).cpp

$(W32OBJS): ..\win32\$(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj ..\win32\$(@B).cpp

$(LIBOBJS): $(@B).cpp $(HEADERS)
    @$(CC) $(CFLAGS) $(DEFINES) -c -Fo$(OBJDIR)/$(@B).obj $(@B).cpp


