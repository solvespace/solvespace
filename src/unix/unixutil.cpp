//-----------------------------------------------------------------------------
// Utility functions used by the Unix port. Notably, our memory allocation;
// we use two separate allocators, one for long-lived stuff and one for
// stuff that gets freed after every regeneration of the model, to save us
// the trouble of freeing the latter explicitly.
//
// Copyright 2008-2013 Jonathan Westhues.
// Copyright 2013 Daniel Richard G. <skunk@iSKUNK.ORG>
//-----------------------------------------------------------------------------
#include <config.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "solvespace.h"

namespace SolveSpace {

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

std::string GetAbsoluteFilename(const std::string &filename)
{
    // We rely on the POSIX.1-2008 version of realpath that allocates
    // storage for itself; the POSIX.1-2001 version is broken by design.
    char *expanded = realpath(filename.c_str(), NULL);
    if(expanded) {
        std::string result(expanded);
        free(expanded);
        return result;
    } else {
        return filename;
    }
}

int64_t GetUnixTime(void)
{
    time_t ret;
    time(&ret);
    return (int64_t)ret;
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

void FreeTemporary(void *p)
{
    AllocTempHeader *h = (AllocTempHeader *)p - 1;
    if(h->prev) {
        h->prev->next = h->next;
    } else {
        Head = h->next;
    }
    if(h->next) h->next->prev = h->prev;
    free(h);
}

void FreeAllTemporary(void)
{
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
    if(!p) oops();
    return p;
}

void MemFree(void *p) {
    free(p);
}

void InitHeaps(void) {
    /* nothing to do */
}

};
