//-----------------------------------------------------------------------------
// Utility functions that depend on Win32. Notably, our memory allocation;
// we use two separate allocators, one for long-lived stuff and one for
// stuff that gets freed after every regeneration of the model, to save us
// the trouble of freeing the latter explicitly.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {
static HANDLE PermHeap, TempHeap;

void dbp(const char *str, ...)
{
    va_list f;
    static char buf[1024*50];
    va_start(f, str);
    _vsnprintf(buf, sizeof(buf), str, f);
    va_end(f);

    OutputDebugString(buf);
}

void GetAbsoluteFilename(char *file)
{
    char absoluteFile[MAX_PATH];
    GetFullPathName(file, sizeof(absoluteFile), absoluteFile, NULL);
    strcpy(file, absoluteFile);
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
    if(!v) oops();
    return v;
}
void FreeTemporary(void *p) {
    HeapFree(TempHeap, HEAP_NO_SERIALIZE, p);
}
void FreeAllTemporary(void)
{
    if(TempHeap) HeapDestroy(TempHeap);
    TempHeap = HeapCreate(HEAP_NO_SERIALIZE, 1024*1024*20, 0);
    // This is a good place to validate, because it gets called fairly
    // often.
    vl();
}

void *MemAlloc(size_t n) {
    void *p = HeapAlloc(PermHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, n);
    if(!p) oops();
    return p;
}
void MemFree(void *p) {
    HeapFree(PermHeap, HEAP_NO_SERIALIZE, p);
}

void vl(void) {
    if(!HeapValidate(TempHeap, HEAP_NO_SERIALIZE, NULL)) oops();
    if(!HeapValidate(PermHeap, HEAP_NO_SERIALIZE, NULL)) oops();
}

void InitHeaps(void) {
    // Create the heap used for long-lived stuff (that gets freed piecewise).
    PermHeap = HeapCreate(HEAP_NO_SERIALIZE, 1024*1024*20, 0);
    // Create the heap that we use to store Exprs and other temp stuff.
    FreeAllTemporary();
}
}
