//-----------------------------------------------------------------------------
// Utility functions that depend on Win32.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

// Include after solvespace.h to avoid identifier clashes.
#include <windows.h>
#include <shellapi.h>

namespace SolveSpace {
static HANDLE TempHeap;

void dbp(const char *str, ...)
{
    va_list f;
    static char buf[1024*50];
    va_start(f, str);
    _vsnprintf(buf, sizeof(buf), str, f);
    va_end(f);

    // The native version of OutputDebugString, unlike most others,
    // is OutputDebugStringA.
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");

#ifndef NDEBUG
    // Duplicate to stderr in debug builds, but not in release; this is slow.
    fputs(buf, stderr);
    fputc('\n', stderr);
#endif
}

//-----------------------------------------------------------------------------
// A separate heap, on which we allocate expressions. Maybe a bit faster,
// since no fragmentation issues whatsoever, and it also makes it possible
// to be sloppy with our memory management, and just free everything at once
// at the end.
//-----------------------------------------------------------------------------
void *AllocTemporary(size_t n)
{
    void *v = HeapAlloc(TempHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, n);
    ssassert(v != NULL, "Cannot allocate memory");
    return v;
}
void FreeAllTemporary()
{
    if(TempHeap) HeapDestroy(TempHeap);
    TempHeap = HeapCreate(HEAP_NO_SERIALIZE, 1024*1024*20, 0);
}

std::vector<std::string> InitPlatform(int argc, char **argv) {
#if !defined(LIBRARY) && defined(_MSC_VER)
    // We display our own message on abort; just call ReportFault.
    _set_abort_behavior(_CALL_REPORTFAULT, _WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    int crtReportTypes[] = {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT};
    for(int crtReportType : crtReportTypes) {
        _CrtSetReportMode(crtReportType, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
        _CrtSetReportFile(crtReportType, _CRTDBG_FILE_STDERR);
    }
#endif

    // Create the heap that we use to store Exprs and other temp stuff.
    FreeAllTemporary();

    // Extract the command-line arguments; the ones from main() are ignored,
    // since they are in the OEM encoding.
    int argcW;
    LPWSTR *argvW = CommandLineToArgvW(GetCommandLineW(), &argcW);
    std::vector<std::string> args;
    for(int i = 0; i < argcW; i++) {
        args.push_back(Platform::Narrow(argvW[i]));
    }
    LocalFree(argvW);
    return args;
}

}
