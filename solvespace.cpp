#include "solvespace.h"

SolveSpace SS;

void SolveSpace::Init(void) {
    TW.Init();
    GW.Init();

    int i;
    for(i = 0; i < 20; i++) {
        TW.Printf("this is line number %d", i);
    }
}

