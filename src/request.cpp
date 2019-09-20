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

struct EntReqMapping {
    Request::Type  reqType;
    Entity::Type   entType;
    int            points;
    bool           useExtraPoints;
    bool           hasNormal;
    bool           hasDistance;
};
static const EntReqMapping EntReqMap[] = {
// request type                   entity type                 pts   xtra?   norml   dist
{ Request::Type::WORKPLANE,       Entity::Type::WORKPLANE,      1,  false,  true,   false },
{ Request::Type::DATUM_POINT,     (Entity::Type)0,              1,  false,  false,  false },
{ Request::Type::LINE_SEGMENT,    Entity::Type::LINE_SEGMENT,   2,  false,  false,  false },
{ Request::Type::CUBIC,           Entity::Type::CUBIC,          4,  true,   false,  false },
{ Request::Type::CUBIC_PERIODIC,  Entity::Type::CUBIC_PERIODIC, 3,  true,   false,  false },
{ Request::Type::CIRCLE,          Entity::Type::CIRCLE,         1,  false,  true,   true  },
{ Request::Type::ARC_OF_CIRCLE,   Entity::Type::ARC_OF_CIRCLE,  3,  false,  true,   false },
{ Request::Type::TTF_TEXT,        Entity::Type::TTF_TEXT,       4,  false,  true,   false },
{ Request::Type::IMAGE,           Entity::Type::IMAGE,          4,  false,  true,   false },
};

static void CopyEntityInfo(const EntReqMapping *te, int extraPoints,
                           Entity::Type *ent, Request::Type *req,
                           int *pts, bool *hasNormal, bool *hasDistance)
{
    int points = te->points;
    if(te->useExtraPoints) points += extraPoints;

    if(ent)         *ent         = te->entType;
    if(req)         *req         = te->reqType;
    if(pts)         *pts         = points;
    if(hasNormal)   *hasNormal   = te->hasNormal;
    if(hasDistance) *hasDistance = te->hasDistance;
}

bool EntReqTable::GetRequestInfo(Request::Type req, int extraPoints,
                                 Entity::Type *ent, int *pts, bool *hasNormal, bool *hasDistance)
{
    for(const EntReqMapping &te : EntReqMap) {
        if(req == te.reqType) {
            CopyEntityInfo(&te, extraPoints, ent, NULL, pts, hasNormal, hasDistance);
            return true;
        }
    }
    return false;
}

bool EntReqTable::GetEntityInfo(Entity::Type ent, int extraPoints,
                                Request::Type *req, int *pts, bool *hasNormal, bool *hasDistance)
{
    for(const EntReqMapping &te : EntReqMap) {
        if(ent == te.entType) {
            CopyEntityInfo(&te, extraPoints, NULL, req, pts, hasNormal, hasDistance);
            return true;
        }
    }
    return false;
}

Request::Type EntReqTable::GetRequestForEntity(Entity::Type ent) {
    Request::Type req;
    ssassert(GetEntityInfo(ent, 0, &req, NULL, NULL, NULL),
             "No entity for request");
    return req;
}

void Request::Generate(IdList<Entity,hEntity> *entity,
                       IdList<Param,hParam> *param)
{
    int points = 0;
    Entity::Type et = (Entity::Type)0;
    bool hasNormal = false;
    bool hasDistance = false;
    int i;

    // Request-specific generation.
    switch(type) {
        case Type::TTF_TEXT: {
            double actualAspectRatio = SS.fonts.AspectRatio(font, str);
            if(EXACT(actualAspectRatio != 0.0)) {
                // We could load the font, so use the actual value.
                aspectRatio = actualAspectRatio;
            }
            if(EXACT(aspectRatio == 0.0)) {
                // We couldn't load the font and we don't have anything saved,
                // so just use 1:1, which is valid for the missing font symbol anyhow.
                aspectRatio = 1.0;
            }
            break;
        }

        case Type::IMAGE: {
            auto image = SS.images.find(file);
            if(image != SS.images.end()) {
                std::shared_ptr<Pixmap> pixmap = (*image).second;
                if(pixmap != NULL) {
                    aspectRatio = (double)pixmap->width / (double)pixmap->height;
                }
            }
            if(EXACT(aspectRatio == 0.0)) {
                aspectRatio = 1.0;
            }
            break;
        }

        default: // most requests don't do anything else
            break;
    }

    Entity e = {};
    EntReqTable::GetRequestInfo(type, extraPoints, &et, &points, &hasNormal, &hasDistance);

    // Generate the entity that's specific to this request.
    e.type = et;
    e.extraPoints = extraPoints;
    e.group = group;
    e.style = style;
    e.workplane = workplane;
    e.construction = construction;
    e.str = str;
    e.font = font;
    e.file = file;
    e.aspectRatio = aspectRatio;
    e.h = h.entity(0);

    // And generate entities for the points
    for(i = 0; i < points; i++) {
        Entity p = {};
        p.workplane = workplane;
        // points start from entity 1, except for datum point case
        p.h = h.entity(i+((et != (Entity::Type)0) ? 1 : 0));
        p.group = group;
        p.style = style;
        p.construction = e.construction;
        if(workplane == Entity::FREE_IN_3D) {
            p.type = Entity::Type::POINT_IN_3D;
            // params for x y z
            p.param[0] = AddParam(param, h.param(16 + 3*i + 0));
            p.param[1] = AddParam(param, h.param(16 + 3*i + 1));
            p.param[2] = AddParam(param, h.param(16 + 3*i + 2));
        } else {
            p.type = Entity::Type::POINT_IN_2D;
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
        n.construction = e.construction;
        if(workplane == Entity::FREE_IN_3D) {
            n.type = Entity::Type::NORMAL_IN_3D;
            n.param[0] = AddParam(param, h.param(32+0));
            n.param[1] = AddParam(param, h.param(32+1));
            n.param[2] = AddParam(param, h.param(32+2));
            n.param[3] = AddParam(param, h.param(32+3));
        } else {
            n.type = Entity::Type::NORMAL_IN_2D;
            // and this is just a copy of the workplane quaternion,
            // so no params required
        }
        ssassert(points >= 1, "Positioning a normal requires a point");
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
        d.type = Entity::Type::DISTANCE;
        d.param[0] = AddParam(param, h.param(64));
        entity->Add(&d);
        e.distance = d.h;
    }

    if(et != (Entity::Type)0) entity->Add(&e);
}

std::string Request::DescriptionString() const {
    const char *s = "";
    if(h == Request::HREQUEST_REFERENCE_XY) {
        s = "#XY";
    } else if(h == Request::HREQUEST_REFERENCE_YZ) {
        s = "#YZ";
    } else if(h == Request::HREQUEST_REFERENCE_ZX) {
        s = "#ZX";
    } else {
        switch(type) {
            case Type::WORKPLANE:       s = "workplane";      break;
            case Type::DATUM_POINT:     s = "datum-point";    break;
            case Type::LINE_SEGMENT:    s = "line-segment";   break;
            case Type::CUBIC:           s = "cubic-bezier";   break;
            case Type::CUBIC_PERIODIC:  s = "periodic-cubic"; break;
            case Type::CIRCLE:          s = "circle";         break;
            case Type::ARC_OF_CIRCLE:   s = "arc-of-circle";  break;
            case Type::TTF_TEXT:        s = "ttf-text";       break;
            case Type::IMAGE:           s = "image";          break;
        }
    }
    ssassert(s != NULL, "Unexpected request type");
    return ssprintf("r%03x-%s", h.v, s);
}

int Request::IndexOfPoint(hEntity he) const {
    if(type == Type::DATUM_POINT) {
        return (he == h.entity(0)) ? 0 : -1;
    }
    for(int i = 0; i < MAX_POINTS_IN_ENTITY; i++) {
        if(he == h.entity(i + 1)) {
            return i;
        }
    }
    return -1;
}

hParam Request::AddParam(IdList<Param,hParam> *param, hParam hp) {
    Param pa = {};
    pa.h = hp;
    param->Add(&pa);
    return hp;
}

