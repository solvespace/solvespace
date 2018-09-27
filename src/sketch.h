//-----------------------------------------------------------------------------
// The parametric structure of our sketch, in multiple groups, that generate
// geometric entities and surfaces.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_SKETCH_H
#define SOLVESPACE_SKETCH_H

#include "smesh.h"
#include "sedge.h"
#include "group.h"
#include "param.h"
#include "entity.h"
#include "request.h"
#include "equation.h"
#include "render/render.h"
#include "platform/gui.h"
#include "constraint.h"
#include "style.h"

namespace SolveSpace {

enum class Command : uint32_t;

class EntReqTable {
public:
    static bool GetRequestInfo(Request::Type req, int extraPoints,
                               EntityBase::Type *ent, int *pts, bool *hasNormal, bool *hasDistance);
    static bool GetEntityInfo(EntityBase::Type ent, int extraPoints,
                              Request::Type *req, int *pts, bool *hasNormal, bool *hasDistance);
    static Request::Type GetRequestForEntity(EntityBase::Type ent);
};

#ifdef LIBRARY
#   define ENTITY EntityBase
#   define CONSTRAINT ConstraintBase
#else
#   define ENTITY Entity
#   define CONSTRAINT Constraint
#endif
class Sketch {
public:
    // These are user-editable, and define the sketch.
    IdList<Group,hGroup>            group;
    List<hGroup>                    groupOrder;
    IdList<CONSTRAINT,hConstraint>  constraint;
    IdList<Request,hRequest>        request;
    IdList<Style,hStyle>            style;

    // These are generated from the above.
    IdList<ENTITY,hEntity>          entity;
    IdList<Param,hParam>            param;

    inline CONSTRAINT *GetConstraint(hConstraint h)
    { return constraint.FindById(h); }
    inline ENTITY  *GetEntity (hEntity  h) { return entity. FindById(h); }
    inline Param   *GetParam  (hParam   h) { return param.  FindById(h); }
    inline Request *GetRequest(hRequest h) { return request.FindById(h); }
    inline Group   *GetGroup  (hGroup   h) { return group.  FindById(h); }
    // Styles are handled a bit differently.

    void Clear();

    BBox CalculateEntityBBox(bool includingInvisible);
    Group *GetRunningMeshGroupFor(hGroup h);
};
#undef ENTITY
#undef CONSTRAINT

}

#endif
