//-----------------------------------------------------------------------------
// Utility functions used by the Unix port. Notably, our memory allocation;
// we use two separate allocators, one for long-lived stuff and one for
// stuff that gets freed after every regeneration of the model, to save us
// the trouble of freeing the latter explicitly.
//
// Copyright 2008-2013 Jonathan Westhues.
// Copyright 2013 Daniel Richard G. <skunk@iSKUNK.ORG>
//-----------------------------------------------------------------------------
#include "config.h"
#include "solvespace.h"
#if defined(HAVE_BACKTRACE)
#  include BACKTRACE_HEADER
#endif

namespace SolveSpace {

void dbp(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stdout, fmt, va);
    fputc('\n', stdout);
    va_end(va);

    fflush(stdout);
}

//-----------------------------------------------------------------------------
// A separate heap, on which we allocate expressions. Maybe a bit faster,
// since fragmentation is less of a concern, and it also makes it possible
// to be sloppy with our memory management, and just free everything at once
// at the end.
//-----------------------------------------------------------------------------

typedef struct _AllocTempHeader AllocTempHeader;

typedef struct _AllocTempHeader {
    AllocTempHeader *prev;
    AllocTempHeader *next;
} AllocTempHeader;

static AllocTempHeader *Head = NULL;

void *AllocTemporary(size_t n)
{
    AllocTempHeader *h =
        (AllocTempHeader *)malloc(n + sizeof(AllocTempHeader));
    h->prev = NULL;
    h->next = Head;
    if(Head) Head->prev = h;
    Head = h;
    memset(&h[1], 0, n);
    return (void *)&h[1];
}

void FreeAllTemporary() {
    AllocTempHeader *h = Head;
    while(h) {
        AllocTempHeader *f = h;
        h = h->next;
        free(f);
    }
    Head = NULL;
}

void *MemAlloc(size_t n) {
    void *p = malloc(n);
    ssassert(p != NULL, "Cannot allocate memory");
    return p;
}

void MemFree(void *p) {
    free(p);
}

std::vector<std::string> InitPlatform(int argc, char **argv) {
    std::vector<std::string> args;
    args.reserve(argc);
    for(int i = 0; i < argc; i++) {
        args.emplace_back(argv[i]);
    }
    return args;
}

};
