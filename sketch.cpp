#include "solvespace.h"

const hEntity  Entity::NONE = { 0 };

const hGroup Group::HGROUP_REFERENCES = { 1 };
const hRequest Request::HREQUEST_REFERENCE_XY = { 1 };
const hRequest Request::HREQUEST_REFERENCE_YZ = { 2 };
const hRequest Request::HREQUEST_REFERENCE_ZX = { 3 };

void Request::GenerateEntities(IdList<Entity,hEntity> *l, 
                               IdList<Group,hGroup> *gl)
{
    Entity e;
    memset(&e, 0, sizeof(e));

    Group *g = gl->FindById(group);
    e.csys = csys = g->csys;

    switch(type) {
        case TWO_D_CSYS:
            e.type = Entity::TWO_D_CSYS;
            l->AddById(&e, h.entity(group, 0));
            break;

        default:
            oops();
    }
}

void Entity::GeneratePointsAndParams(IdList<Point,hPoint> *lpt, 
                                     IdList<Param,hParam> *lpa)
{
    int ptc, pac;

    switch(type) {
        case TWO_D_CSYS: ptc = 1;    pac = 4;   break;

        default:
            oops();
    }

    Point pt;
    memset(&pt, 0, sizeof(pt));

    pt.csys = csys;

    Param pa;
    memset(&pa, 0, sizeof(pa));
    int i;
    for(i = 0; i < ptc; i++) {
        lpt->AddById(&pt, h.point3(i));
    }
    for(i = 0; i < pac; i++) {
        lpa->AddById(&pa, h.param(i));
    }
}

void Point::GenerateParams(IdList<Param,hParam> *l) {
    Param pa;
    memset(&pa, 0, sizeof(pa));

    switch(h.v & 7) {
        case 0:
            // A point in 3-space; three parameters x, y, z.
            l->AddById(&pa, h.param(0));
            l->AddById(&pa, h.param(1));
            l->AddById(&pa, h.param(2));
            break;
            
        default:
            oops();
    }
}
