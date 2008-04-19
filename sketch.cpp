#include "solvespace.h"

const hEntity  Entity::NO_CSYS = { 0 };

const hGroup Group::HGROUP_REFERENCES = { 1 };
const hRequest Request::HREQUEST_REFERENCE_XY = { 1 };
const hRequest Request::HREQUEST_REFERENCE_YZ = { 2 };
const hRequest Request::HREQUEST_REFERENCE_ZX = { 3 };

char *Group::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "g%04x-%s", h.v, name.str);
    } else {
        sprintf(ret, "g%04x-(unnamed)", h.v);
    }
    return ret;
}

hParam Request::AddParam(IdList<Param,hParam> *param, hParam hp) {
    Param pa;
    memset(&pa, 0, sizeof(pa));
    pa.h = hp;
    param->Add(&pa);
    return hp;
}

void Request::Generate(IdList<Entity,hEntity> *entity,
                       IdList<Param,hParam> *param)
{
    int points = 0;
    int params = 0;
    int et = 0;
    int i;

    Group *g = SS.group.FindById(group);

    Entity e;
    memset(&e, 0, sizeof(e));
    switch(type) {
        case Request::CSYS_2D:
            et = Entity::CSYS_2D;          points = 1; params = 4; goto c;

        case Request::DATUM_POINT:
            et = 0;                        points = 1; params = 0; goto c;

        case Request::LINE_SEGMENT:
            et = Entity::LINE_SEGMENT;     points = 2; params = 0; goto c;
c: {
            // Generate the entity that's specific to this request.
            e.type = et;
            e.h = h.entity(0);

            // And generate entities for the points
            for(i = 0; i < points; i++) {
                Entity p;
                memset(&p, 0, sizeof(p));
                p.csys = csys;
                // points start from entity 1, except for datum point case
                p.h = h.entity(i+(et ? 1 : 0));
                p.symbolic = true;
                if(csys.v == Entity::NO_CSYS.v) {
                    p.type = Entity::POINT_IN_3D;
                    // params for x y z
                    p.param.h[0] = AddParam(param, h.param(16 + 3*i + 0));
                    p.param.h[1] = AddParam(param, h.param(16 + 3*i + 1));
                    p.param.h[2] = AddParam(param, h.param(16 + 3*i + 2));
                } else {
                    p.type = Entity::POINT_IN_2D;
                    // params for u v
                    p.param.h[0] = AddParam(param, h.param(16 + 3*i + 0));
                    p.param.h[1] = AddParam(param, h.param(16 + 3*i + 1));
                }
                entity->Add(&p);
                e.assoc[i] = p.h;
            }
            // And generate any params not associated with the point that
            // we happen to need.
            for(i = 0; i < params; i++) {
                e.param.h[i] = AddParam(param, h.param(i));
            }

            if(et) entity->Add(&e);
            break;
        }

        default:
            oops();
    }
}

char *Request::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "r%03x-%s", h.v, name.str);
    } else {
        sprintf(ret, "r%03x-(unnamed)", h.v);
    }
    return ret;
}

void Param::ForceTo(double v) {
    val = v;
    known = true;
}


