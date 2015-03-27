//-----------------------------------------------------------------------------
// Implementation of our Request class; a request is a user-created thing
// that will generate an entity (line, curve) when the sketch is generated,
// in the same way that other entities are generated automatically, like
// by an extrude or a step and repeat.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

const hRequest Request::HREQUEST_REFERENCE_XY = { 1 };
const hRequest Request::HREQUEST_REFERENCE_YZ = { 2 };
const hRequest Request::HREQUEST_REFERENCE_ZX = { 3 };

const EntReqTable::TableEntry EntReqTable::Table[] = {
//   request type               entity type       pts   xtra?   norml   dist    description
{ Request::WORKPLANE,       Entity::WORKPLANE,      1,  false,  true,   false, "workplane"      },
{ Request::DATUM_POINT,     0,                      1,  false,  false,  false, "datum-point"    },
{ Request::LINE_SEGMENT,    Entity::LINE_SEGMENT,   2,  false,  false,  false, "line-segment"   },
{ Request::CUBIC,           Entity::CUBIC,          4,  true,   false,  false, "cubic-bezier"   },
{ Request::CUBIC_PERIODIC,  Entity::CUBIC_PERIODIC, 3,  true,   false,  false, "periodic-cubic" },
{ Request::CIRCLE,          Entity::CIRCLE,         1,  false,  true,   true,  "circle"         },
{ Request::ARC_OF_CIRCLE,   Entity::ARC_OF_CIRCLE,  3,  false,  true,   false, "arc-of-circle"  },
{ Request::TTF_TEXT,        Entity::TTF_TEXT,       2,  false,  true,   false, "ttf-text"       },
{ 0, 0, 0, false, false, false, 0 },
};

const char *EntReqTable::DescriptionForRequest(int req) {
    for(int i = 0; Table[i].reqType; i++) {
        if(req == Table[i].reqType) {
            return Table[i].description;
        }
    }
    return "???";
}

void EntReqTable::CopyEntityInfo(const TableEntry *te, int extraPoints,
           int *ent, int *req, int *pts, bool *hasNormal, bool *hasDistance)
{
    int points = te->points;
    if(te->useExtraPoints) points += extraPoints;

    if(ent)         *ent         = te->entType;
    if(req)         *req         = te->reqType;
    if(pts)         *pts         = points;
    if(hasNormal)   *hasNormal   = te->hasNormal;
    if(hasDistance) *hasDistance = te->hasDistance;
}

bool EntReqTable::GetRequestInfo(int req, int extraPoints,
                     int *ent, int *pts, bool *hasNormal, bool *hasDistance)
{
    for(int i = 0; Table[i].reqType; i++) {
        const TableEntry *te = &(Table[i]);
        if(req == te->reqType) {
            CopyEntityInfo(te, extraPoints,
                ent, NULL, pts, hasNormal, hasDistance);
            return true;
        }
    }
    return false;
}

bool EntReqTable::GetEntityInfo(int ent, int extraPoints,
                     int *req, int *pts, bool *hasNormal, bool *hasDistance)
{
    for(int i = 0; Table[i].reqType; i++) {
        const TableEntry *te = &(Table[i]);
        if(ent == te->entType) {
            CopyEntityInfo(te, extraPoints,
                NULL, req, pts, hasNormal, hasDistance);
            return true;
        }
    }
    return false;
}

int EntReqTable::GetRequestForEntity(int ent) {
    int req = 0;
    GetEntityInfo(ent, 0, &req, NULL, NULL, NULL);
    return req;
}


void Request::Generate(IdList<Entity,hEntity> *entity,
                       IdList<Param,hParam> *param)
{
    int points = 0;
    int et = 0;
    bool hasNormal = false;
    bool hasDistance = false;
    int i;

    Entity e = {};
    EntReqTable::GetRequestInfo(type, extraPoints,
                    &et, &points, &hasNormal, &hasDistance);

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
        Entity p = {};
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
        Entity n = {};
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
        Entity d = {};
        d.workplane = workplane;
        d.h = h.entity(64);
        d.group = group;
        d.style = style;
        d.type = Entity::DISTANCE;
        d.param[0] = AddParam(param, h.param(64));
        entity->Add(&d);
        e.distance = d.h;
    }

    if(et) entity->Add(&e);
}

char *Request::DescriptionString(void) {
    const char *s;
    if(h.v == Request::HREQUEST_REFERENCE_XY.v) {
        s = "#XY";
    } else if(h.v == Request::HREQUEST_REFERENCE_YZ.v) {
        s = "#YZ";
    } else if(h.v == Request::HREQUEST_REFERENCE_ZX.v) {
        s = "#ZX";
    } else {
        s = EntReqTable::DescriptionForRequest(type);
    }
    static char ret[100];
    sprintf(ret, "r%03x-%s", h.v, s);
    return ret;
}

hParam Request::AddParam(IdList<Param,hParam> *param, hParam hp) {
    Param pa = {};
    pa.h = hp;
    param->Add(&pa);
    return hp;
}

