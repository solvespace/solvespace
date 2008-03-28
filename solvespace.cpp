#include "solvespace.h"

template IdList<Request,hRequest>;
template IdList<Entity,hEntity>;
template IdList<Point,hPoint>;

SolveSpace SS;

void SolveSpace::Init(void) {
    TW.Init();
    GW.Init();

    req.Clear();
    entity.Clear();
    point.Clear();
    param.Clear();

    int i;
    for(i = 0; i < 20; i++) {
        TW.Printf("this is line number %d", i);
    }
}

