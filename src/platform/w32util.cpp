//-----------------------------------------------------------------------------
// Utility functions that depend on Win32. Notably, our memory allocation;
// we use two separate allocators, one for long-lived stuff and one for
// stuff that gets freed after every regeneration of the model, to save us
// the trouble of freeing the latter explicitly.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

// Include after solvespace.h to avoid identifier clashes.
#include <windows.h>

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

void assert_failure(const char *file, unsigned line, const char *function,
                    const char *condition, const char *message) {
    dbp("File %s, line %u, function %s:\n", file, line, function);
    dbp("Assertion '%s' failed: ((%s) == false).\n", message, condition);
#ifdef NDEBUG
    _exit(1);
#else
    abort();
#endif
}

std::string Narrow(const wchar_t *in)
{
    std::string out;
    DWORD len = WideCharToMultiByte(CP_UTF8, 0, in, -1, NULL, 0, NULL, NULL);
    out.resize(len - 1);
    ssassert(WideCharToMultiByte(CP_UTF8, 0, in, -1, &out[0], len, NULL, NULL),
             "Invalid UTF-16");
    return out;
}

std::string Narrow(const std::wstring &in)
{
    if(in == L"") return "";

    std::string out;
    out.resize(WideCharToMultiByte(CP_UTF8, 0, &in[0], in.length(), NULL, 0, NULL, NULL));
    ssassert(WideCharToMultiByte(CP_UTF8, 0, &in[0], in.length(),
                                 &out[0], out.length(), NULL, NULL),
             "Invalid UTF-16");
    return out;
}

std::wstring Widen(const char *in)
{
    std::wstring out;
    DWORD len = MultiByteToWideChar(CP_UTF8, 0, in, -1, NULL, 0);
    out.resize(len - 1);
    ssassert(MultiByteToWideChar(CP_UTF8, 0, in, -1, &out[0], len),
             "Invalid UTF-8");
    return out;
}

std::wstring Widen(const std::string &in)
{
    if(in == "") return L"";

    std::wstring out;
    out.resize(MultiByteToWideChar(CP_UTF8, 0, &in[0], in.length(), NULL, 0));
    ssassert(MultiByteToWideChar(CP_UTF8, 0, &in[0], in.length(), &out[0], out.length()),
             "Invalid UTF-8");
    return out;
}

bool PathEqual(const std::string &a, const std::string &b)
{
    // Case-sensitivity is actually per-volume on Windows,
    // but it is tedious to implement and test for little benefit.
    std::wstring wa = Widen(a), wb = Widen(b);
    return std::equal(wa.begin(), wa.end(), wb.begin(), /*wb.end(),*/
                [](wchar_t wca, wchar_t wcb) { return towlower(wca) == towlower(wcb); });

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

    ssassert(filename.length() == strlen(filename.c_str()),
             "Unexpected null byte in middle of a path");
    return _wfopen(Widen(uncFilename).c_str(), Widen(mode).c_str());
}

void ssremove(const std::string &filename)
{
    ssassert(filename.length() == strlen(filename.c_str()),
             "Unexpected null byte in middle of a path");
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
    ssassert(v != NULL, "Cannot allocate memory");
    return v;
}
void FreeTemporary(void *p) {
    HeapFree(TempHeap, HEAP_NO_SERIALIZE, p);
}
void FreeAllTemporary()
{
    if(TempHeap) HeapDestroy(TempHeap);
    TempHeap = HeapCreate(HEAP_NO_SERIALIZE, 1024*1024*20, 0);
    // This is a good place to validate, because it gets called fairly
    // often.
    vl();
}

void *MemAlloc(size_t n) {
    void *p = HeapAlloc(PermHeap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, n);
    ssassert(p != NULL, "Cannot allocate memory");
    return p;
}
void MemFree(void *p) {
    HeapFree(PermHeap, HEAP_NO_SERIALIZE, p);
}

void vl() {
    ssassert(HeapValidate(TempHeap, HEAP_NO_SERIALIZE, NULL), "Corrupted heap");
    ssassert(HeapValidate(PermHeap, HEAP_NO_SERIALIZE, NULL), "Corrupted heap");
}

void InitHeaps() {
    // Create the heap used for long-lived stuff (that gets freed piecewise).
    PermHeap = HeapCreate(HEAP_NO_SERIALIZE, 1024*1024*20, 0);
    // Create the heap that we use to store Exprs and other temp stuff.
    FreeAllTemporary();
}
}
