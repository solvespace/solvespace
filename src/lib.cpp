//-----------------------------------------------------------------------------
// A library wrapper around SolveSpace, to permit someone to use its constraint
// solver without coupling their program too much to SolveSpace's internals.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#define EXPORT_DLL
#include <slvs.h>

Sketch SolveSpace::SK = {};
static System SYS;

void SolveSpace::Platform::FatalError(const std::string &message) {
    fprintf(stderr, "%s", message.c_str());
    abort();
}

void Group::GenerateEquations(IdList<Equation,hEquation> *) {
    // Nothing to do for now.
}

extern "C" {

Slvs_hParam Slvs_AddParam(double val) {
    Param pa = {};
    pa.val   = val;
    SK.param.AddAndAssignId(&pa);
    return pa.h.v;
}

// entities
Slvs_hEntity Slvs_AddPoint2D(Slvs_hGroup grouph, double u, double v, Slvs_hEntity workplaneh) {
    Slvs_hParam uph      = Slvs_AddParam(u);
    Slvs_hParam vph      = Slvs_AddParam(v);
    EntityBase e  = {};
    e.type        = EntityBase::Type::POINT_IN_2D;
    e.group.v     = grouph;
    e.workplane.v = workplaneh;
    e.param[0].v  = uph;
    e.param[1].v  = vph;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_AddPoint3D(Slvs_hGroup grouph, double x, double y, double z) {
    Slvs_hParam xph      = Slvs_AddParam(x);
    Slvs_hParam yph      = Slvs_AddParam(y);
    Slvs_hParam zph      = Slvs_AddParam(z);
    EntityBase e  = {};
    e.type        = EntityBase::Type::POINT_IN_3D;
    e.group.v     = grouph;
    e.workplane.v = EntityBase::FREE_IN_3D.v;
    e.param[0].v  = xph;
    e.param[1].v  = yph;
    e.param[2].v  = zph;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_AddNormal2D(Slvs_hGroup grouph, Slvs_hEntity workplaneh) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    if(!workplane->IsWorkplane()) {
        ssassert(false, "workplane argument is not a workplane");
    }
    EntityBase e  = {};
    e.type        = EntityBase::Type::NORMAL_IN_2D;
    e.group.v     = grouph;
    e.workplane.v = workplaneh;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_AddNormal3D(Slvs_hGroup grouph, double qw, double qx, double qy, double qz) {
    Slvs_hParam wph      = Slvs_AddParam(qw);
    Slvs_hParam xph      = Slvs_AddParam(qx);
    Slvs_hParam yph      = Slvs_AddParam(qy);
    Slvs_hParam zph      = Slvs_AddParam(qz);
    EntityBase e  = {};
    e.type        = EntityBase::Type::NORMAL_IN_3D;
    e.group.v     = grouph;
    e.workplane.v = EntityBase::FREE_IN_3D.v;
    e.param[0].v  = wph;
    e.param[1].v  = xph;
    e.param[2].v  = yph;
    e.param[3].v  = zph;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_AddDistance(Slvs_hGroup grouph, double value, Slvs_hEntity workplaneh) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    if(!workplane->IsWorkplane()) {
        ssassert(false, "workplane argument is not a workplane");
    }
    Slvs_hParam valueph  = Slvs_AddParam(value);
    EntityBase e  = {};
    e.type        = EntityBase::Type::DISTANCE;
    e.group.v     = grouph;
    e.workplane.v = workplaneh;
    e.param[0].v  = valueph;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_AddLine2D(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh, Slvs_hEntity workplaneh) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* ptA = SK.entity.FindByHandleV(ptAh);
    EntityBase* ptB = SK.entity.FindByHandleV(ptBh);
    if(!workplane->IsWorkplane()) {
        ssassert(false, "workplane argument is not a workplane");
    } else if(!ptA->IsPoint2D()) {
        ssassert(false, "ptA argument is not a 2d point");
    } else if(!ptB->IsPoint2D()) {
        ssassert(false, "ptB argument is not a 2d point");
    }
    EntityBase e  = {};
    e.type        = EntityBase::Type::LINE_SEGMENT;
    e.group.v     = grouph;
    e.workplane.v = workplaneh;
    e.point[0].v  = ptAh;
    e.point[1].v  = ptBh;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_AddLine3D(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh) {
    EntityBase* ptA = SK.entity.FindByHandleV(ptAh);
    EntityBase* ptB = SK.entity.FindByHandleV(ptBh);
    if(!ptA->IsPoint3D()) {
        ssassert(false, "ptA argument is not a 3d point");
    } else if(!ptB->IsPoint3D()) {
        ssassert(false, "ptB argument is not a 3d point");
    }
    EntityBase e  = {};
    e.type        = EntityBase::Type::LINE_SEGMENT;
    e.group.v     = grouph;
    e.workplane.v = EntityBase::FREE_IN_3D.v;
    e.point[0].v  = ptAh;
    e.point[1].v  = ptBh;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_AddCubic(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh, Slvs_hEntity ptCh, Slvs_hEntity ptDh, Slvs_hEntity workplaneh) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* ptA = SK.entity.FindByHandleV(ptAh);
    EntityBase* ptB = SK.entity.FindByHandleV(ptBh);
    EntityBase* ptC = SK.entity.FindByHandleV(ptCh);
    EntityBase* ptD = SK.entity.FindByHandleV(ptDh);
    if(!workplane->IsWorkplane()) {
        ssassert(false, "workplane argument is not a workplane");
    } else if(!ptA->IsPoint2D()) {
        ssassert(false, "ptA argument is not a 2d point");
    } else if(!ptB->IsPoint2D()) {
        ssassert(false, "ptB argument is not a 2d point");
    } else if(!ptC->IsPoint2D()) {
        ssassert(false, "ptC argument is not a 2d point");
    } else if(!ptD->IsPoint2D()) {
        ssassert(false, "ptD argument is not a 2d point");
    }
    EntityBase e  = {};
    e.type        = EntityBase::Type::CUBIC;
    e.group.v     = grouph;
    e.workplane.v = workplaneh;
    e.point[0].v  = ptAh;
    e.point[1].v  = ptBh;
    e.point[2].v  = ptCh;
    e.point[3].v  = ptDh;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}


Slvs_hEntity Slvs_AddArc(Slvs_hGroup grouph, Slvs_hEntity normalh, Slvs_hEntity centerh, Slvs_hEntity starth, Slvs_hEntity endh,
                            Slvs_hEntity workplaneh) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* normal = SK.entity.FindByHandleV(normalh);
    EntityBase* center = SK.entity.FindByHandleV(centerh);
    EntityBase* start = SK.entity.FindByHandleV(starth);
    EntityBase* end = SK.entity.FindByHandleV(endh);
    if(!workplane->IsWorkplane()) {
        ssassert(false, "workplane argument is not a workplane");
    } else if(!normal->IsNormal3D()) {
        ssassert(false, "normal argument is not a 3d normal");
    } else if(!center->IsPoint2D()) {
        ssassert(false, "center argument is not a 2d point");
    } else if(!start->IsPoint2D()) {
        ssassert(false, "start argument is not a 2d point");
    } else if(!end->IsPoint2D()) {
        ssassert(false, "end argument is not a 2d point");
    }
    EntityBase e  = {};
    e.type        = EntityBase::Type::ARC_OF_CIRCLE;
    e.group.v     = grouph;
    e.workplane.v = workplaneh;
    e.normal.v    = normalh;
    e.point[0].v  = centerh;
    e.point[1].v  = starth;
    e.point[2].v  = endh;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_AddCircle(Slvs_hGroup grouph, Slvs_hEntity normalh, Slvs_hEntity centerh, Slvs_hEntity radiush,
                            Slvs_hEntity workplaneh) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* normal = SK.entity.FindByHandleV(normalh);
    EntityBase* center = SK.entity.FindByHandleV(centerh);
    EntityBase* radius = SK.entity.FindByHandleV(radiush);
   if(!workplane->IsWorkplane()) {
        ssassert(false, "workplane argument is not a workplane");
    } else if(!normal->IsNormal3D()) {
        ssassert(false, "normal argument is not a 3d normal");
    } else if(!center->IsPoint2D()) {
        ssassert(false, "center argument is not a 2d point");
    } else if(!radius->IsDistance()) {
        ssassert(false, "radius argument is not a distance");
    }
    EntityBase e  = {};
    e.type        = EntityBase::Type::CIRCLE;
    e.group.v     = grouph;
    e.workplane.v = workplaneh;
    e.normal.v    = normalh;
    e.point[0].v  = centerh;
    e.distance.v  = radiush;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_AddWorkplane(Slvs_hGroup grouph, Slvs_hEntity originh, Slvs_hEntity nmh) {
    EntityBase e  = {};
    e.type        = EntityBase::Type::WORKPLANE;
    e.group.v     = grouph;
    e.workplane.v = SLVS_FREE_IN_3D;
    e.point[0].v  = originh;
    e.normal.v    = nmh;
    SK.entity.AddAndAssignId(&e);
    return e.h.v;
}

Slvs_hEntity Slvs_Add2DBase(Slvs_hGroup grouph) {
    Vector u      = Vector::From(1, 0, 0);
    Vector v      = Vector::From(0, 1, 0);
    Quaternion q  = Quaternion::From(u, v);
    Slvs_hEntity nm = Slvs_AddNormal3D(grouph, q.w, q.vx, q.vy, q.vz);
    return Slvs_AddWorkplane(grouph, Slvs_AddPoint3D(grouph, 0, 0, 0), nm);
}

// constraints

Slvs_hConstraint Slvs_AddConstraint(Slvs_hGroup grouph,
    int type, Slvs_hEntity workplaneh, double val, Slvs_hEntity ptAh,
    Slvs_hEntity ptBh = SLVS_NO_ENTITY, Slvs_hEntity entityAh = SLVS_NO_ENTITY,
    Slvs_hEntity entityBh = SLVS_NO_ENTITY, Slvs_hEntity entityCh = SLVS_NO_ENTITY,
    Slvs_hEntity entityDh = SLVS_NO_ENTITY, int other = 0, int other2 = 0) {
    ConstraintBase c = {};
    ConstraintBase::Type t;
    switch(type) {
        case SLVS_C_POINTS_COINCIDENT:  t = ConstraintBase::Type::POINTS_COINCIDENT; break;
        case SLVS_C_PT_PT_DISTANCE:     t = ConstraintBase::Type::PT_PT_DISTANCE; break;
        case SLVS_C_PT_PLANE_DISTANCE:  t = ConstraintBase::Type::PT_PLANE_DISTANCE; break;
        case SLVS_C_PT_LINE_DISTANCE:   t = ConstraintBase::Type::PT_LINE_DISTANCE; break;
        case SLVS_C_PT_FACE_DISTANCE:   t = ConstraintBase::Type::PT_FACE_DISTANCE; break;
        case SLVS_C_PT_IN_PLANE:        t = ConstraintBase::Type::PT_IN_PLANE; break;
        case SLVS_C_PT_ON_LINE:         t = ConstraintBase::Type::PT_ON_LINE; break;
        case SLVS_C_PT_ON_FACE:         t = ConstraintBase::Type::PT_ON_FACE; break;
        case SLVS_C_EQUAL_LENGTH_LINES: t = ConstraintBase::Type::EQUAL_LENGTH_LINES; break;
        case SLVS_C_LENGTH_RATIO:       t = ConstraintBase::Type::LENGTH_RATIO; break;
        case SLVS_C_ARC_ARC_LEN_RATIO:  t = ConstraintBase::Type::ARC_ARC_LEN_RATIO; break;
        case SLVS_C_ARC_LINE_LEN_RATIO: t = ConstraintBase::Type::ARC_LINE_LEN_RATIO; break;
        case SLVS_C_EQ_LEN_PT_LINE_D:   t = ConstraintBase::Type::EQ_LEN_PT_LINE_D; break;
        case SLVS_C_EQ_PT_LN_DISTANCES: t = ConstraintBase::Type::EQ_PT_LN_DISTANCES; break;
        case SLVS_C_EQUAL_ANGLE:        t = ConstraintBase::Type::EQUAL_ANGLE; break;
        case SLVS_C_EQUAL_LINE_ARC_LEN: t = ConstraintBase::Type::EQUAL_LINE_ARC_LEN; break;
        case SLVS_C_LENGTH_DIFFERENCE:  t = ConstraintBase::Type::LENGTH_DIFFERENCE; break;
        case SLVS_C_ARC_ARC_DIFFERENCE: t = ConstraintBase::Type::ARC_ARC_DIFFERENCE; break;
        case SLVS_C_ARC_LINE_DIFFERENCE:t = ConstraintBase::Type::ARC_LINE_DIFFERENCE; break;
        case SLVS_C_SYMMETRIC:          t = ConstraintBase::Type::SYMMETRIC; break;
        case SLVS_C_SYMMETRIC_HORIZ:    t = ConstraintBase::Type::SYMMETRIC_HORIZ; break;
        case SLVS_C_SYMMETRIC_VERT:     t = ConstraintBase::Type::SYMMETRIC_VERT; break;
        case SLVS_C_SYMMETRIC_LINE:     t = ConstraintBase::Type::SYMMETRIC_LINE; break;
        case SLVS_C_AT_MIDPOINT:        t = ConstraintBase::Type::AT_MIDPOINT; break;
        case SLVS_C_HORIZONTAL:         t = ConstraintBase::Type::HORIZONTAL; break;
        case SLVS_C_VERTICAL:           t = ConstraintBase::Type::VERTICAL; break;
        case SLVS_C_DIAMETER:           t = ConstraintBase::Type::DIAMETER; break;
        case SLVS_C_PT_ON_CIRCLE:       t = ConstraintBase::Type::PT_ON_CIRCLE; break;
        case SLVS_C_SAME_ORIENTATION:   t = ConstraintBase::Type::SAME_ORIENTATION; break;
        case SLVS_C_ANGLE:              t = ConstraintBase::Type::ANGLE; break;
        case SLVS_C_PARALLEL:           t = ConstraintBase::Type::PARALLEL; break;
        case SLVS_C_PERPENDICULAR:      t = ConstraintBase::Type::PERPENDICULAR; break;
        case SLVS_C_ARC_LINE_TANGENT:   t = ConstraintBase::Type::ARC_LINE_TANGENT; break;
        case SLVS_C_CUBIC_LINE_TANGENT: t = ConstraintBase::Type::CUBIC_LINE_TANGENT; break;
        case SLVS_C_EQUAL_RADIUS:       t = ConstraintBase::Type::EQUAL_RADIUS; break;
        case SLVS_C_PROJ_PT_DISTANCE:   t = ConstraintBase::Type::PROJ_PT_DISTANCE; break;
        case SLVS_C_WHERE_DRAGGED:      t = ConstraintBase::Type::WHERE_DRAGGED; break;
        case SLVS_C_CURVE_CURVE_TANGENT:t = ConstraintBase::Type::CURVE_CURVE_TANGENT; break;
    }
    c.type           = t;
    c.group.v        = grouph;
    c.workplane.v    = workplaneh;
    c.valA           = val;
    c.ptA.v          = ptAh;
    c.ptB.v          = ptBh;
    c.entityA.v      = entityAh;
    c.entityB.v      = entityBh;
    c.entityC.v      = entityCh;
    c.entityD.v      = entityDh;
    c.other          = other ? true : false;
    c.other2         = other2 ? true : false;
    SK.constraint.AddAndAssignId(&c);
    return c.h.v;
}

Slvs_hConstraint Slvs_Coincident(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                    Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsPoint() && entityB->IsPoint()) {
        return Slvs_AddConstraint(grouph, SLVS_C_POINTS_COINCIDENT, workplaneh, 0., entityAh, entityBh);
    } else if(entityA->IsPoint() && entityB->IsWorkplane()) {
        return Slvs_AddConstraint(grouph, SLVS_C_PT_IN_PLANE, SLVS_FREE_IN_3D, 0., entityAh, SLVS_NO_ENTITY, entityBh);
    } else if(entityA->IsPoint() && entityB->IsLine()) {
        return Slvs_AddConstraint(grouph, SLVS_C_PT_ON_LINE, workplaneh, 0., entityAh, SLVS_NO_ENTITY, entityBh);
    } else if(entityA->IsPoint() && (entityB->IsCircle() || entityB->IsArc())) {
        return Slvs_AddConstraint(grouph, SLVS_C_PT_ON_CIRCLE, workplaneh, 0., entityAh, SLVS_NO_ENTITY, entityBh);
    }
    ssassert(false, "Invalid arguments for coincident constraint");
}

Slvs_hConstraint Slvs_Distance(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, double value,
                                Slvs_hEntity workplaneh) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsPoint() && entityB->IsPoint()) {
        return Slvs_AddConstraint(grouph, SLVS_C_PT_PT_DISTANCE, workplaneh, value, entityAh,
                                entityBh);
    } else if(entityA->IsPoint() && entityB->IsWorkplane() && workplane->Is3D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_PT_PLANE_DISTANCE, entityBh, value, entityAh,
                                SLVS_NO_ENTITY, entityBh);
    } else if(entityA->IsPoint() && entityB->IsLine()) {
        return Slvs_AddConstraint(grouph, SLVS_C_PT_LINE_DISTANCE, workplaneh, value, entityAh,
                                SLVS_NO_ENTITY, entityBh);
    }
    ssassert(false, "Invalid arguments for distance constraint");
}

Slvs_hConstraint Slvs_Equal(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                            Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsLine() && entityB->IsLine()) {
        return Slvs_AddConstraint(grouph, SLVS_C_EQUAL_LENGTH_LINES, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh);
    } else if(entityA->IsLine() && (entityB->IsArc() || entityB->IsCircle())) {
        return Slvs_AddConstraint(grouph, SLVS_C_EQUAL_LINE_ARC_LEN, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh);
    } else if((entityA->IsArc() || entityA->IsCircle()) &&
                (entityB->IsArc() || entityB->IsCircle())) {
        return Slvs_AddConstraint(grouph, SLVS_C_EQUAL_RADIUS, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh);
    }
    ssassert(false, "Invalid arguments for equal constraint");
}

Slvs_hConstraint Slvs_EqualAngle(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, Slvs_hEntity entityCh,
                                    Slvs_hEntity entityDh,
                                    Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    EntityBase* entityC = SK.entity.FindByHandleV(entityCh);
    EntityBase* entityD = SK.entity.FindByHandleV(entityDh);
    if(entityA->IsLine2D() && entityB->IsLine2D() && entityC->IsLine2D() && entityD->IsLine2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_EQUAL_ANGLE, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh,
                                entityCh, entityDh);
    }
    ssassert(false, "Invalid arguments for equal angle constraint");
}

Slvs_hConstraint Slvs_EqualPointToLine(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                        Slvs_hEntity entityCh, Slvs_hEntity entityDh,
                                        Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    EntityBase* entityC = SK.entity.FindByHandleV(entityCh);
    EntityBase* entityD = SK.entity.FindByHandleV(entityDh);
    if(entityA->IsPoint2D() && entityB->IsLine2D() && entityC->IsPoint2D() && entityD->IsLine2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_EQ_PT_LN_DISTANCES, workplaneh, 0., entityAh,
                                entityBh, entityCh, entityDh);
    }
    ssassert(false, "Invalid arguments for equal point to line constraint");
}

Slvs_hConstraint Slvs_Ratio(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, double value,
                            Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsLine2D() && entityB->IsLine2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_LENGTH_RATIO, workplaneh, value,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh);
    }
    ssassert(false, "Invalid arguments for ratio constraint");
}

Slvs_hConstraint Slvs_Symmetric(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                Slvs_hEntity entityCh   = SLVS_NO_ENTITY,
                                Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    EntityBase* entityC = SK.entity.FindByHandleV(entityCh);
    if(entityA->IsPoint3D() && entityB->IsPoint3D() && entityC->IsWorkplane() &&
        workplane->IsFreeIn3D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_SYMMETRIC, workplaneh, 0., entityAh, entityBh,
                                entityCh);
    } else if(entityA->IsPoint2D() && entityB->IsPoint2D() && entityC->IsWorkplane() &&
                workplane->IsFreeIn3D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_SYMMETRIC, entityCh, 0., entityAh, entityBh,
                                entityCh);
    } else if(entityA->IsPoint2D() && entityB->IsPoint2D() && entityC->IsLine()) {
        if(workplane->IsFreeIn3D()) {
            ssassert(false, "3d workplane given for a 2d constraint");
        }
        return Slvs_AddConstraint(grouph, SLVS_C_SYMMETRIC_LINE, workplaneh, 0., entityAh,
                                entityBh, entityCh);
    }
    ssassert(false, "Invalid arguments for symmetric constraint");
}

Slvs_hConstraint Slvs_SymmetricH(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh,
                                    Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* ptA = SK.entity.FindByHandleV(ptAh);
    EntityBase* ptB = SK.entity.FindByHandleV(ptBh);
    if(workplane->IsFreeIn3D()) {
        ssassert(false, "3d workplane given for a 2d constraint");
    } else if(ptA->IsPoint2D() && ptB->IsPoint2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_SYMMETRIC_HORIZ, workplaneh, 0., ptAh, ptBh);
    }
    ssassert(false, "Invalid arguments for symmetric horizontal constraint");
}

Slvs_hConstraint Slvs_SymmetricV(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh,
                                    Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* ptA = SK.entity.FindByHandleV(ptAh);
    EntityBase* ptB = SK.entity.FindByHandleV(ptBh);
    if(workplane->IsFreeIn3D()) {
        ssassert(false, "3d workplane given for a 2d constraint");
    } else if(ptA->IsPoint2D() && ptB->IsPoint2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_SYMMETRIC_VERT, workplaneh, 0., ptAh, ptBh);
    }
    ssassert(false, "Invalid arguments for symmetric vertical constraint");
}

Slvs_hConstraint Slvs_Midpoint(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh,
                                Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* ptA = SK.entity.FindByHandleV(ptAh);
    EntityBase* ptB = SK.entity.FindByHandleV(ptBh);
    if(ptA->IsPoint() && ptB->IsLine()) {
        return Slvs_AddConstraint(grouph, SLVS_C_AT_MIDPOINT, workplaneh, 0., ptAh,
                                SLVS_NO_ENTITY, ptBh);
    }
    ssassert(false, "Invalid arguments for midpoint constraint");
}

Slvs_hConstraint Slvs_Horizontal(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity workplaneh,
                                    Slvs_hEntity entityBh = SLVS_NO_ENTITY) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(workplane->IsFreeIn3D()) {
        ssassert(false, "Horizontal constraint is not supported in 3D");
    } else if(entityA->IsLine2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_HORIZONTAL, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh);
    } else if(entityA->IsPoint2D() && entityB->IsPoint2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_HORIZONTAL, workplaneh, 0., entityAh, entityBh);
    }
    ssassert(false, "Invalid arguments for horizontal constraint");
}

Slvs_hConstraint Slvs_Vertical(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity workplaneh,
                                Slvs_hEntity entityBh = SLVS_NO_ENTITY) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(workplane->IsFreeIn3D()) {
        ssassert(false, "Vertical constraint is not supported in 3D");
    } else if(entityA->IsLine2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_VERTICAL, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh);
    } else if(entityA->IsPoint2D() && entityB->IsPoint2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_VERTICAL, workplaneh, 0., entityAh, entityBh);
    }
    ssassert(false, "Invalid arguments for horizontal constraint");
}

Slvs_hConstraint Slvs_Diameter(Slvs_hGroup grouph, Slvs_hEntity entityAh, double value) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    if(entityA->IsArc() || entityA->IsCircle()) {
        return Slvs_AddConstraint(grouph, SLVS_C_DIAMETER, SLVS_FREE_IN_3D, value,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh);
    }
    ssassert(false, "Invalid arguments for diameter constraint");
}

Slvs_hConstraint Slvs_SameOrientation(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsNormal3D() && entityB->IsNormal3D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_SAME_ORIENTATION, SLVS_FREE_IN_3D,
                                0., SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh,
                                entityBh);
    }
    ssassert(false, "Invalid arguments for same orientation constraint");
}

Slvs_hConstraint Slvs_Angle(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, double value,
                            Slvs_hEntity workplaneh = SLVS_FREE_IN_3D,
                            int inverse         = 0) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsLine2D() && entityB->IsLine2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_ANGLE, workplaneh, value,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh, inverse);
    }
    ssassert(false, "Invalid arguments for angle constraint");
}

Slvs_hConstraint Slvs_Perpendicular(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                    Slvs_hEntity workplaneh = SLVS_FREE_IN_3D,
                                    int inverse         = 0) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsLine2D() && entityB->IsLine2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_PERPENDICULAR, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, inverse);
    }
    ssassert(false, "Invalid arguments for perpendicular constraint");
}

Slvs_hConstraint Slvs_Parallel(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsLine2D() && entityB->IsLine2D()) {
        return Slvs_AddConstraint(grouph, SLVS_C_PARALLEL, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh);
    }
    ssassert(false, "Invalid arguments for parallel constraint");
}

Slvs_hConstraint Slvs_Tangent(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh,
                                Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* workplane = SK.entity.FindByHandleV(workplaneh);
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsArc() && entityB->IsLine2D()) {
        if(workplane->IsFreeIn3D()) {
            ssassert(false, "3d workplane given for a 2d constraint");
        }
        Vector a1 = SK.GetEntity(entityA->point[1])->PointGetNum(),
                a2 = SK.GetEntity(entityA->point[2])->PointGetNum();
        Vector l0 = SK.GetEntity(entityB->point[0])->PointGetNum(),
                l1 = SK.GetEntity(entityB->point[1])->PointGetNum();
        bool other;
        if(l0.Equals(a1) || l1.Equals(a1)) {
            other = false;
        } else if(l0.Equals(a2) || l1.Equals(a2)) {
            other = true;
        } else {
            ssassert(false, "The tangent arc and line segment must share an "
                                        "endpoint. Constrain them with Constrain -> "
                                        "On Point before constraining tangent.");
        }
        return Slvs_AddConstraint(grouph, SLVS_C_ARC_LINE_TANGENT, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, other);
    } else if(entityA->IsCubic() && entityB->IsLine2D() && workplane->IsFreeIn3D()) {
        Vector as = entityA->CubicGetStartNum(), af = entityA->CubicGetFinishNum();
        Vector l0 = SK.GetEntity(entityB->point[0])->PointGetNum(),
                l1 = SK.GetEntity(entityB->point[1])->PointGetNum();
        bool other;
        if(l0.Equals(as) || l1.Equals(as)) {
            other = false;
        } else if(l0.Equals(af) || l1.Equals(af)) {
            other = true;
        } else {
            ssassert(false, "The tangent cubic and line segment must share an "
                                        "endpoint. Constrain them with Constrain -> "
                                        "On Point before constraining tangent.");
        }
        return Slvs_AddConstraint(grouph, SLVS_C_CUBIC_LINE_TANGENT, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, other);
    } else if((entityA->IsArc() || entityA->IsCubic()) &&
                (entityB->IsArc() || entityB->IsCubic())) {
        if(workplane->IsFreeIn3D()) {
            ssassert(false, "3d workplane given for a 2d constraint");
        }
        Vector as = entityA->EndpointStart(), af = entityA->EndpointFinish(),
                bs = entityB->EndpointStart(), bf = entityB->EndpointFinish();
        bool other;
        bool other2;
        if(as.Equals(bs)) {
            other  = false;
            other2 = false;
        } else if(as.Equals(bf)) {
            other  = false;
            other2 = true;
        } else if(af.Equals(bs)) {
            other  = true;
            other2 = false;
        } else if(af.Equals(bf)) {
            other  = true;
            other2 = true;
        } else {
            ssassert(false, "The curves must share an endpoint. Constrain them "
                                        "with Constrain -> On Point before constraining "
                                        "tangent.");
        }
        return Slvs_AddConstraint(grouph, SLVS_C_CURVE_CURVE_TANGENT, workplaneh, 0.,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh, other, other2);
    }
    ssassert(false, "Invalid arguments for tangent constraint");
}

Slvs_hConstraint Slvs_DistanceProj(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity ptBh, double value) {
    EntityBase* ptA = SK.entity.FindByHandleV(ptAh);
    EntityBase* ptB = SK.entity.FindByHandleV(ptBh);
    if(ptA->IsPoint() && ptB->IsPoint()) {
        return Slvs_AddConstraint(grouph, SLVS_C_PROJ_PT_DISTANCE, SLVS_FREE_IN_3D,
                                value, ptAh, ptBh);
    }
    ssassert(false, "Invalid arguments for projected distance constraint");
}

Slvs_hConstraint Slvs_LengthDiff(Slvs_hGroup grouph, Slvs_hEntity entityAh, Slvs_hEntity entityBh, double value,
                                    Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* entityA = SK.entity.FindByHandleV(entityAh);
    EntityBase* entityB = SK.entity.FindByHandleV(entityBh);
    if(entityA->IsLine() && entityB->IsLine()) {
        return Slvs_AddConstraint(grouph, SLVS_C_LENGTH_DIFFERENCE, workplaneh, value,
                                SLVS_NO_ENTITY, SLVS_NO_ENTITY, entityAh, entityBh);
    }
    ssassert(false, "Invalid arguments for length difference constraint");
}

Slvs_hConstraint Slvs_Dragged(Slvs_hGroup grouph, Slvs_hEntity ptAh, Slvs_hEntity workplaneh = SLVS_FREE_IN_3D) {
    EntityBase* ptA = SK.entity.FindByHandleV(ptAh);
    if(ptA->IsPoint()) {
        return Slvs_AddConstraint(grouph, SLVS_C_WHERE_DRAGGED, workplaneh, 0., ptAh);
    }
    ssassert(false, "Invalid arguments for dragged constraint");
}

void Slvs_QuaternionU(double qw, double qx, double qy, double qz,
                         double *x, double *y, double *z)
{
    Quaternion q = Quaternion::From(qw, qx, qy, qz);
    Vector v = q.RotationU();
    *x = v.x;
    *y = v.y;
    *z = v.z;
}

void Slvs_QuaternionV(double qw, double qx, double qy, double qz,
                         double *x, double *y, double *z)
{
    Quaternion q = Quaternion::From(qw, qx, qy, qz);
    Vector v = q.RotationV();
    *x = v.x;
    *y = v.y;
    *z = v.z;
}

void Slvs_QuaternionN(double qw, double qx, double qy, double qz,
                         double *x, double *y, double *z)
{
    Quaternion q = Quaternion::From(qw, qx, qy, qz);
    Vector v = q.RotationN();
    *x = v.x;
    *y = v.y;
    *z = v.z;
}

void Slvs_MakeQuaternion(double ux, double uy, double uz,
                         double vx, double vy, double vz,
                         double *qw, double *qx, double *qy, double *qz)
{
    Vector u = Vector::From(ux, uy, uz),
           v = Vector::From(vx, vy, vz);
    Quaternion q = Quaternion::From(u, v);
    *qw = q.w;
    *qx = q.vx;
    *qy = q.vy;
    *qz = q.vz;
}

int Slvs_SolveSketch(Slvs_hGroup shg, int* rank, int* dof, int* badCount, int calculateFaileds = 0)
{
    SYS.Clear();

    Group g;
    g.h.v = shg;

    for(EntityBase &ent : SK.entity) {
        EntityBase *e = &ent;
        if (e->group.v != shg) {
            continue;
        }
        for (hParam &parh : e->param) {
            if (parh.v != 0) {
                Param *p = SK.GetParam(parh);
                p->known = false;
                SYS.param.Add(p);
            }
        }
    }

    IdList<Param, hParam> constraintParams = {};
    for(ConstraintBase &con : SK.constraint) {
        ConstraintBase *c = &con;
        if(c->group.v != shg)
            continue;
        c->Generate(&constraintParams);
        if(!constraintParams.IsEmpty()) {
            for(Param &p : constraintParams) {
                p.h    = SK.param.AddAndAssignId(&p);
                c->valP = p.h;
                SYS.param.Add(&p);
            }
            constraintParams.Clear();
            c->ModifyToSatisfy();
        }
    }

    for(ConstraintBase &con : SK.constraint) {
        ConstraintBase *c = &con;
        if(c->type == ConstraintBase::Type::WHERE_DRAGGED) {
            EntityBase *e = SK.GetEntity(c->ptA);
            SYS.dragged.Add(&(e->param[0]));
            SYS.dragged.Add(&(e->param[1]));
            if (e->type == EntityBase::Type::POINT_IN_3D) {
                SYS.dragged.Add(&(e->param[2]));
            }
        }
    }

    List<hConstraint> badList;
    bool andFindBad = calculateFaileds ? true : false;
    SolveResult status = SYS.Solve(&g, rank, dof, &badList, andFindBad, false, false);

    *badCount = badList.n;

    switch(status) {
        case SolveResult::OKAY:
            return SLVS_RESULT_OKAY;
        case SolveResult::DIDNT_CONVERGE:
            return SLVS_RESULT_DIDNT_CONVERGE;
        case SolveResult::REDUNDANT_DIDNT_CONVERGE:
        case SolveResult::REDUNDANT_OKAY:
            return SLVS_RESULT_INCONSISTENT;
        case SolveResult::TOO_MANY_UNKNOWNS:
            return SLVS_RESULT_TOO_MANY_UNKNOWNS;
    }
}

double Slvs_GetParamValue(Slvs_hEntity e, int i)
{
    EntityBase* entity = SK.entity.FindByHandleV(e);
    Param* p = SK.param.FindById(entity->param[i]);
    return p->val;
}

Slvs_hEntity Slvs_GetPoint(Slvs_hParam e, int i)
{
    EntityBase* entity = SK.entity.FindByHandleV(e);
    EntityBase* p = SK.entity.FindById(entity->point[i]);
    return p->h.v;
}

void Slvs_Solve(Slvs_System *ssys, Slvs_hGroup shg)
{
    SYS.Clear();
    SK.param.Clear();
    SK.entity.Clear();
    SK.constraint.Clear();
    int i;
    for(i = 0; i < ssys->params; i++) {
        Slvs_Param *sp = &(ssys->param[i]);
        Param p = {};

        p.h.v = sp->h;
        p.val = sp->val;
        SK.param.Add(&p);
        if(sp->group == shg) {
            SYS.param.Add(&p);
        }
    }

    for(i = 0; i < ssys->entities; i++) {
        Slvs_Entity *se = &(ssys->entity[i]);
        EntityBase e = {};

        switch(se->type) {
case SLVS_E_POINT_IN_3D:        e.type = Entity::Type::POINT_IN_3D; break;
case SLVS_E_POINT_IN_2D:        e.type = Entity::Type::POINT_IN_2D; break;
case SLVS_E_NORMAL_IN_3D:       e.type = Entity::Type::NORMAL_IN_3D; break;
case SLVS_E_NORMAL_IN_2D:       e.type = Entity::Type::NORMAL_IN_2D; break;
case SLVS_E_DISTANCE:           e.type = Entity::Type::DISTANCE; break;
case SLVS_E_WORKPLANE:          e.type = Entity::Type::WORKPLANE; break;
case SLVS_E_LINE_SEGMENT:       e.type = Entity::Type::LINE_SEGMENT; break;
case SLVS_E_CUBIC:              e.type = Entity::Type::CUBIC; break;
case SLVS_E_CIRCLE:             e.type = Entity::Type::CIRCLE; break;
case SLVS_E_ARC_OF_CIRCLE:      e.type = Entity::Type::ARC_OF_CIRCLE; break;

default: dbp("bad entity type %d", se->type); return;
        }
        e.h.v           = se->h;
        e.group.v       = se->group;
        e.workplane.v   = se->wrkpl;
        e.point[0].v    = se->point[0];
        e.point[1].v    = se->point[1];
        e.point[2].v    = se->point[2];
        e.point[3].v    = se->point[3];
        e.normal.v      = se->normal;
        e.distance.v    = se->distance;
        e.param[0].v    = se->param[0];
        e.param[1].v    = se->param[1];
        e.param[2].v    = se->param[2];
        e.param[3].v    = se->param[3];

        SK.entity.Add(&e);
    }
    IdList<Param, hParam> params = {};
    for(i = 0; i < ssys->constraints; i++) {
        Slvs_Constraint *sc = &(ssys->constraint[i]);
        ConstraintBase c = {};

        ConstraintBase::Type t;
        switch(sc->type) {
case SLVS_C_POINTS_COINCIDENT:  t = ConstraintBase::Type::POINTS_COINCIDENT; break;
case SLVS_C_PT_PT_DISTANCE:     t = ConstraintBase::Type::PT_PT_DISTANCE; break;
case SLVS_C_PT_PLANE_DISTANCE:  t = ConstraintBase::Type::PT_PLANE_DISTANCE; break;
case SLVS_C_PT_LINE_DISTANCE:   t = ConstraintBase::Type::PT_LINE_DISTANCE; break;
case SLVS_C_PT_FACE_DISTANCE:   t = ConstraintBase::Type::PT_FACE_DISTANCE; break;
case SLVS_C_PT_IN_PLANE:        t = ConstraintBase::Type::PT_IN_PLANE; break;
case SLVS_C_PT_ON_LINE:         t = ConstraintBase::Type::PT_ON_LINE; break;
case SLVS_C_PT_ON_FACE:         t = ConstraintBase::Type::PT_ON_FACE; break;
case SLVS_C_EQUAL_LENGTH_LINES: t = ConstraintBase::Type::EQUAL_LENGTH_LINES; break;
case SLVS_C_LENGTH_RATIO:       t = ConstraintBase::Type::LENGTH_RATIO; break;
case SLVS_C_ARC_ARC_LEN_RATIO:  t = ConstraintBase::Type::ARC_ARC_LEN_RATIO; break;
case SLVS_C_ARC_LINE_LEN_RATIO: t = ConstraintBase::Type::ARC_LINE_LEN_RATIO; break;
case SLVS_C_EQ_LEN_PT_LINE_D:   t = ConstraintBase::Type::EQ_LEN_PT_LINE_D; break;
case SLVS_C_EQ_PT_LN_DISTANCES: t = ConstraintBase::Type::EQ_PT_LN_DISTANCES; break;
case SLVS_C_EQUAL_ANGLE:        t = ConstraintBase::Type::EQUAL_ANGLE; break;
case SLVS_C_EQUAL_LINE_ARC_LEN: t = ConstraintBase::Type::EQUAL_LINE_ARC_LEN; break;
case SLVS_C_LENGTH_DIFFERENCE:  t = ConstraintBase::Type::LENGTH_DIFFERENCE; break;
case SLVS_C_ARC_ARC_DIFFERENCE: t = ConstraintBase::Type::ARC_ARC_DIFFERENCE; break;
case SLVS_C_ARC_LINE_DIFFERENCE:t = ConstraintBase::Type::ARC_LINE_DIFFERENCE; break;
case SLVS_C_SYMMETRIC:          t = ConstraintBase::Type::SYMMETRIC; break;
case SLVS_C_SYMMETRIC_HORIZ:    t = ConstraintBase::Type::SYMMETRIC_HORIZ; break;
case SLVS_C_SYMMETRIC_VERT:     t = ConstraintBase::Type::SYMMETRIC_VERT; break;
case SLVS_C_SYMMETRIC_LINE:     t = ConstraintBase::Type::SYMMETRIC_LINE; break;
case SLVS_C_AT_MIDPOINT:        t = ConstraintBase::Type::AT_MIDPOINT; break;
case SLVS_C_HORIZONTAL:         t = ConstraintBase::Type::HORIZONTAL; break;
case SLVS_C_VERTICAL:           t = ConstraintBase::Type::VERTICAL; break;
case SLVS_C_DIAMETER:           t = ConstraintBase::Type::DIAMETER; break;
case SLVS_C_PT_ON_CIRCLE:       t = ConstraintBase::Type::PT_ON_CIRCLE; break;
case SLVS_C_SAME_ORIENTATION:   t = ConstraintBase::Type::SAME_ORIENTATION; break;
case SLVS_C_ANGLE:              t = ConstraintBase::Type::ANGLE; break;
case SLVS_C_PARALLEL:           t = ConstraintBase::Type::PARALLEL; break;
case SLVS_C_PERPENDICULAR:      t = ConstraintBase::Type::PERPENDICULAR; break;
case SLVS_C_ARC_LINE_TANGENT:   t = ConstraintBase::Type::ARC_LINE_TANGENT; break;
case SLVS_C_CUBIC_LINE_TANGENT: t = ConstraintBase::Type::CUBIC_LINE_TANGENT; break;
case SLVS_C_EQUAL_RADIUS:       t = ConstraintBase::Type::EQUAL_RADIUS; break;
case SLVS_C_PROJ_PT_DISTANCE:   t = ConstraintBase::Type::PROJ_PT_DISTANCE; break;
case SLVS_C_WHERE_DRAGGED:      t = ConstraintBase::Type::WHERE_DRAGGED; break;
case SLVS_C_CURVE_CURVE_TANGENT:t = ConstraintBase::Type::CURVE_CURVE_TANGENT; break;

default: dbp("bad constraint type %d", sc->type); return;
        }

        c.type = t;

        c.h.v           = sc->h;
        c.group.v       = sc->group;
        c.workplane.v   = sc->wrkpl;
        c.valA          = sc->valA;
        c.ptA.v         = sc->ptA;
        c.ptB.v         = sc->ptB;
        c.entityA.v     = sc->entityA;
        c.entityB.v     = sc->entityB;
        c.entityC.v     = sc->entityC;
        c.entityD.v     = sc->entityD;
        c.other         = (sc->other) ? true : false;
        c.other2        = (sc->other2) ? true : false;

        c.Generate(&params);
        if(!params.IsEmpty()) {
            for(Param &p : params) {
                p.h = SK.param.AddAndAssignId(&p);
                c.valP = p.h;
                SYS.param.Add(&p);
            }
            params.Clear();
            c.ModifyToSatisfy();
        }

        SK.constraint.Add(&c);
    }

    for(i = 0; i < (int)arraylen(ssys->dragged); i++) {
        if(ssys->dragged[i]) {
            hParam hp = { ssys->dragged[i] };
            SYS.dragged.Add(&hp);
        }
    }

    Group g = {};
    g.h.v = shg;

    List<hConstraint> bad = {};

    // Now we're finally ready to solve!
    bool andFindBad = ssys->calculateFaileds ? true : false;
    SolveResult how = SYS.Solve(&g, NULL, &(ssys->dof), &bad, andFindBad, /*andFindFree=*/false);

    switch(how) {
        case SolveResult::OKAY:
            ssys->result = SLVS_RESULT_OKAY;
            break;

        case SolveResult::DIDNT_CONVERGE:
            ssys->result = SLVS_RESULT_DIDNT_CONVERGE;
            break;

        case SolveResult::REDUNDANT_DIDNT_CONVERGE:
        case SolveResult::REDUNDANT_OKAY:
            ssys->result = SLVS_RESULT_INCONSISTENT;
            break;

        case SolveResult::TOO_MANY_UNKNOWNS:
            ssys->result = SLVS_RESULT_TOO_MANY_UNKNOWNS;
            break;
    }

    // Write the new parameter values back to our caller.
    for(i = 0; i < ssys->params; i++) {
        Slvs_Param *sp = &(ssys->param[i]);
        hParam hp = { sp->h };
        sp->val = SK.GetParam(hp)->val;
    }

    if(ssys->failed) {
        // Copy over any the list of problematic constraints.
        for(i = 0; i < ssys->faileds && i < bad.n; i++) {
            ssys->failed[i] = bad[i].v;
        }
        ssys->faileds = bad.n;
    }

    bad.Clear();
    SYS.Clear();
    SK.param.Clear();
    SK.entity.Clear();
    SK.constraint.Clear();

    FreeAllTemporary();
}

} /* extern "C" */
