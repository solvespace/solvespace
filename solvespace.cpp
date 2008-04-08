#include "solvespace.h"

template IdList<Request,hRequest>;
template IdList<Entity,hEntity>;
template IdList<Point,hPoint>;

SolveSpace SS;

void SolveSpace::Init(void) {
    TW.Init();
    GW.Init();

    request.Clear();
    entity.Clear();
    point.Clear();
    param.Clear();
    group.Clear();

    // Our initial group, that contains the references.
    Group g;
    memset(&g, 0, sizeof(g));
    g.csys = Entity::NONE;
    g.name.strcpy("__references");
    group.AddById(&g, Group::HGROUP_REFERENCES);

    g.csys = Entity::NONE;
    g.name.strcpy("");
    group.AddAndAssignId(&g);
    

    // Let's create three two-d coordinate systems, for the coordinate
    // planes; these are our references, present in every sketch.
    Request r;
    memset(&r, 0, sizeof(r));
    r.type = Request::TWO_D_CSYS;
    r.group = Group::HGROUP_REFERENCES;

    r.name.strcpy("__xy_plane");
    request.AddById(&r, Request::HREQUEST_REFERENCE_XY);
    r.name.strcpy("__yz_plane");
    request.AddById(&r, Request::HREQUEST_REFERENCE_YZ);
    r.name.strcpy("__zx_plane");
    request.AddById(&r, Request::HREQUEST_REFERENCE_ZX);

    TW.ShowGroupList();
    TW.ShowRequestList();

    TW.ClearScreen();
    Solve();
}

void SolveSpace::Solve(void) {
    int i;

    entity.Clear();
    for(i = 0; i < request.elems; i++) {
        request.elem[i].v.GenerateEntities(&entity, &group);
    }

    point.Clear();
    param.Clear();
    for(i = 0; i < entity.elems; i++) {
        entity.elem[i].v.GeneratePointsAndParams(&point, &param);
    }
    for(i = 0; i < point.elems; i++) {
        point.elem[i].v.GenerateParams(&param);
    }

    TW.Printf("entities=%d", entity.elems);
    TW.Printf("points=%d", point.elems);
    TW.Printf("params=%d", param.elems);

}

