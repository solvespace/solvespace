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

extern "C" {

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

void Slvs_Solve(Slvs_System *ssys, Slvs_hGroup shg)
{
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
case SLVS_E_POINT_IN_3D:        e.type = EntityBase::Type::POINT_IN_3D; break;
case SLVS_E_POINT_IN_2D:        e.type = EntityBase::Type::POINT_IN_2D; break;
case SLVS_E_NORMAL_IN_3D:       e.type = EntityBase::Type::NORMAL_IN_3D; break;
case SLVS_E_NORMAL_IN_2D:       e.type = EntityBase::Type::NORMAL_IN_2D; break;
case SLVS_E_DISTANCE:           e.type = EntityBase::Type::DISTANCE; break;
case SLVS_E_WORKPLANE:          e.type = EntityBase::Type::WORKPLANE; break;
case SLVS_E_LINE_SEGMENT:       e.type = EntityBase::Type::LINE_SEGMENT; break;
case SLVS_E_CUBIC:              e.type = EntityBase::Type::CUBIC; break;
case SLVS_E_CIRCLE:             e.type = EntityBase::Type::CIRCLE; break;
case SLVS_E_ARC_OF_CIRCLE:      e.type = EntityBase::Type::ARC_OF_CIRCLE; break;

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

    GroupBase g = {};
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
    SYS.param.Clear();
    SYS.entity.Clear();
    SYS.eq.Clear();
    SYS.dragged.Clear();

    SK.param.Clear();
    SK.entity.Clear();
    SK.constraint.Clear();

    FreeAllTemporary();
}

} /* extern "C" */
