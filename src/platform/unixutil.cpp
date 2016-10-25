//-----------------------------------------------------------------------------
// Utility functions that run on Unix. Notably, our memory allocation;
// we use two separate allocators, one for long-lived stuff and one for
// stuff that gets freed after every regeneration of the model, to save us
// the trouble of freeing the latter explicitly.
//
// Copyright 2016 whitequark@whitequark.org.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

void dbp(const char *str, ...)
{
    va_list f;
    static char buf[1024*50];
    va_start(f, str);
    vsnprintf(buf, sizeof(buf), str, f);
    va_end(f);

    fputs(buf, stderr);
    fputc('\n', stderr);
}

void GetAbsoluteFilename(char *file)
{
}

//-----------------------------------------------------------------------------
// A separate heap, on which we allocate expressions. Maybe a bit faster,
// since no fragmentation issues whatsoever, and it also makes it possible
// to be sloppy with our memory management, and just free everything at once
// at the end.
//-----------------------------------------------------------------------------
struct HeapList {
    HeapList *next;
};

static HeapList *TempHeap;

void *AllocTemporary(int n)
{
    HeapList *l = (HeapList*)malloc(n + sizeof(HeapList));
    if(!l) oops();
    l->next = TempHeap;
    TempHeap = l;
    return (void*)((intptr_t)l + sizeof(HeapList));
}
void FreeTemporary(void *p) {
}
void FreeAllTemporary(void)
{
    while(TempHeap) {
        HeapList *next = TempHeap->next;
        free(TempHeap);
        TempHeap = next;
    }
}

void *MemRealloc(void *p, int n) {
    if(!p) {
        return MemAlloc(n);
    }

    p = realloc(p, n);
    if(!p) oops();
    return p;
}
void *MemAlloc(int n) {
    void *p = malloc(n);
    if(!p) oops();
    return p;
}
void MemFree(void *p) {
    free(p);
}

void vl(void) {
}

void InitHeaps(void) {
}

