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

    // The sketch starts with three requests, for three datum planes.
    Request n;
    n.type = Request::FOR_PLANE;
    req.AddById(&n, Request::HREQUEST_DATUM_PLANE_XY);
    req.AddById(&n, Request::HREQUEST_DATUM_PLANE_YZ);
    req.AddById(&n, Request::HREQUEST_DATUM_PLANE_ZX);

    int i;
    for(i = 0; i < 10; i++) {
        TW.Printf("this is line number %d", i);
    }
}

