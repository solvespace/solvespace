#include "solvespace.h"

const hRequest Request::HREQUEST_REFERENCE_XY = { 1 };
const hRequest Request::HREQUEST_REFERENCE_YZ = { 2 };
const hRequest Request::HREQUEST_REFERENCE_ZX = { 3 };


void Request::Generate(IdList<Entity,hEntity> *entity,
                       IdList<Param,hParam> *param)
{
    int points = 0;
    int params = 0;
    int et = 0;
    bool hasNormal = false;
    bool hasDistance = false;
    int i;

    Entity e;
    memset(&e, 0, sizeof(e));
    switch(type) {
        case Request::WORKPLANE:
            et = Entity::WORKPLANE;
            points = 1;
            hasNormal = true;
            break;

        case Request::DATUM_POINT:
            et = 0;
            points = 1;
            break;

        case Request::LINE_SEGMENT:
            et = Entity::LINE_SEGMENT;
            points = 2;
            break;

        case Request::CIRCLE:
            et = Entity::CIRCLE;
            points = 1;
            params = 1;
            hasNormal = true;
            hasDistance = true;
            break;

        case Request::ARC_OF_CIRCLE:
            et = Entity::ARC_OF_CIRCLE;
            points = 3;
            hasNormal = true;
            break;

        case Request::CUBIC:
            et = Entity::CUBIC;
            points = 4 + extraPoints; 
            break;

        case Request::CUBIC_PERIODIC:
            et = Entity::CUBIC_PERIODIC;
            points = 3 + extraPoints;
            break;

        case Request::TTF_TEXT:
            et = Entity::TTF_TEXT;
            points = 2;
            hasNormal = true;
            break;

        default: oops();
    }

    // Generate the entity that's specific to this request.
    e.type = et;
    e.extraPoints = extraPoints;
    e.group = group;
    e.style = style;
    e.workplane = workplane;
    e.construction = construction;
    e.str.strcpy(str.str);
    e.font.strcpy(font.str);
    e.h = h.entity(0);

    // And generate entities for the points
    for(i = 0; i < points; i++) {
        Entity p;
        memset(&p, 0, sizeof(p));
        p.workplane = workplane;
        // points start from entity 1, except for datum point case
        p.h = h.entity(i+(et ? 1 : 0));
        p.group = group;
        p.style = style;

        if(workplane.v == Entity::FREE_IN_3D.v) {
            p.type = Entity::POINT_IN_3D;
            // params for x y z
            p.param[0] = AddParam(param, h.param(16 + 3*i + 0));
            p.param[1] = AddParam(param, h.param(16 + 3*i + 1));
            p.param[2] = AddParam(param, h.param(16 + 3*i + 2));
        } else {
            p.type = Entity::POINT_IN_2D;
            // params for u v
            p.param[0] = AddParam(param, h.param(16 + 3*i + 0));
            p.param[1] = AddParam(param, h.param(16 + 3*i + 1));
        }
        entity->Add(&p);
        e.point[i] = p.h;
    }
    if(hasNormal) {
        Entity n;
        memset(&n, 0, sizeof(n));
        n.workplane = workplane;
        n.h = h.entity(32);
        n.group = group;
        n.style = style;
        if(workplane.v == Entity::FREE_IN_3D.v) {
            n.type = Entity::NORMAL_IN_3D;
            n.param[0] = AddParam(param, h.param(32+0));
            n.param[1] = AddParam(param, h.param(32+1));
            n.param[2] = AddParam(param, h.param(32+2));
            n.param[3] = AddParam(param, h.param(32+3));
        } else {
            n.type = Entity::NORMAL_IN_2D;
            // and this is just a copy of the workplane quaternion,
            // so no params required
        }
        if(points < 1) oops();
        // The point determines where the normal gets displayed on-screen;
        // it's entirely cosmetic.
        n.point[0] = e.point[0];
        entity->Add(&n);
        e.normal = n.h;
    }
    if(hasDistance) {
        Entity d;
        memset(&d, 0, sizeof(d));
        d.workplane = workplane;
        d.h = h.entity(64);
        d.group = group;
        d.style = style;
        d.type = Entity::DISTANCE;
        d.param[0] = AddParam(param, h.param(64));
        entity->Add(&d);
        e.distance = d.h;
    }
    // And generate any params not associated with the point that
    // we happen to need.
    for(i = 0; i < params; i++) {
        e.param[i] = AddParam(param, h.param(i));
    }

    if(et) entity->Add(&e);
}

char *Request::DescriptionString(void) {
    char *s;
    if(h.v == Request::HREQUEST_REFERENCE_XY.v) {
        s = "#XY";
    } else if(h.v == Request::HREQUEST_REFERENCE_YZ.v) {
        s = "#YZ";
    } else if(h.v == Request::HREQUEST_REFERENCE_ZX.v) {
        s = "#ZX";
    } else {
        switch(type) {
            case WORKPLANE:         s = "workplane"; break;
            case DATUM_POINT:       s = "datum-point"; break;
            case LINE_SEGMENT:      s = "line-segment"; break;
            case CUBIC:             s = "cubic-bezier"; break;
            case CUBIC_PERIODIC:    s = "periodic-cubic"; break;
            case CIRCLE:            s = "circle"; break;
            case ARC_OF_CIRCLE:     s = "arc-of-circle"; break;
            case TTF_TEXT:          s = "ttf-text"; break;
            default:                s = "???"; break;
        }
    }
    static char ret[100];
    sprintf(ret, "r%03x-%s", h.v, s);
    return ret;
}

hParam Request::AddParam(IdList<Param,hParam> *param, hParam hp) {
    Param pa;
    memset(&pa, 0, sizeof(pa));
    pa.h = hp;
    param->Add(&pa);
    return hp;
}

