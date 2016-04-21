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

    // The native version of OutputDebugString, unlike most others,
    // is OutputDebugStringA.
    OutputDebugStringA(buf);
}

std::string Narrow(const wchar_t *in)
{
    std::string out;
    DWORD len = WideCharToMultiByte(CP_UTF8, 0, in, -1, NULL, 0, NULL, NULL);
    out.resize(len - 1);
    if(!WideCharToMultiByte(CP_UTF8, 0, in, -1, &out[0], len, NULL, NULL))
        oops();
    return out;
}

std::string Narrow(const std::wstring &in)
{
    if(in == L"") return "";

    std::string out;
    out.resize(WideCharToMultiByte(CP_UTF8, 0, &in[0], in.length(), NULL, 0, NULL, NULL));
    if(!WideCharToMultiByte(CP_UTF8, 0, &in[0], in.length(),
                            &out[0], out.length(), NULL, NULL))
        oops();
    return out;
}

std::wstring Widen(const char *in)
{
    std::wstring out;
    DWORD len = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);
    out.resize(len - 1);
    if(!MultiByteToWideChar(CP_UTF8, 0, in, -1, &out[0], len))
        oops();
    return out;
}

std::wstring Widen(const std::string &in)
{
    if(in == "") return L"";

    std::wstring out;
    out.resize(MultiByteToWideChar(CP_UTF8, 0, &in[0], in.length(), NULL, 0));
    if(!MultiByteToWideChar(CP_UTF8, 0, &in[0], in.length(), &out[0], out.length()))
        oops();
    return out;
}

FILE *ssfopen(const std::string &filename, const char *mode)
{
    // Prepend \\?\ UNC prefix unless already an UNC path.
    // We never try to fopen paths that are not absolute or
    // contain separators inappropriate for the platform;
    // thus, it is always safe to prepend this prefix.
    std::string uncFilename = filename;
    if(uncFilename.substr(0, 2) != "\\\\")
        uncFilename = "\\\\?\\" + uncFilename;

    if(filename.length() != strlen(filename.c_str())) oops();
    return _wfopen(Widen(uncFilename).c_str(), Widen(mode).c_str());
}

void ssremove(const std::string &filename)
{
    if(filename.length() != strlen(filename.c_str())) oops();
    _wremove(Widen(filename).c_str());
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
