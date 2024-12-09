#include "solvespace.h"
#include <mimalloc.h>

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Debug output
//-----------------------------------------------------------------------------

void DebugPrint(const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
    va_end(va);
}

//-----------------------------------------------------------------------------
// Temporary arena.
//-----------------------------------------------------------------------------

struct MimallocHeap {
    mi_heap_t *heap = NULL;

    ~MimallocHeap() {
        if(heap != NULL)
            mi_heap_destroy(heap);
    }
};

static thread_local MimallocHeap TempArena;

void *AllocTemporary(size_t size) {
    if(TempArena.heap == NULL) {
        TempArena.heap = mi_heap_new();
        ssassert(TempArena.heap != NULL, "out of memory");
    }
    void *ptr = mi_heap_zalloc(TempArena.heap, size);
    ssassert(ptr != NULL, "out of memory");
    return ptr;
}

void FreeAllTemporary() {
    MimallocHeap temp;
    std::swap(TempArena.heap, temp.heap);
}

}
}
