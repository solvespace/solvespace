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

static int IsInit = 0;

void Group::GenerateEquations(IdList<Equation,hEquation> *l) {
    // Nothing to do for now.
}

void SolveSpace::CnfFreezeInt(uint32_t v, const char *name)
{
    abort();
}

uint32_t SolveSpace::CnfThawInt(uint32_t v, const char *name)
{
    abort();
    return 0;
}

void SolveSpace::DoMessageBox(const char *str, int rows, int cols, bool error)
{
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
    if(!IsInit) {
        InitHeaps();
        IsInit = 1;
    }

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
case SLVS_E_POINT_IN_3D:        e.type = Entity::POINT_IN_3D; break;
case SLVS_E_POINT_IN_2D:        e.type = Entity::POINT_IN_2D; break;
case SLVS_E_NORMAL_IN_3D:       e.type = Entity::NORMAL_IN_3D; break;
case SLVS_E_NORMAL_IN_2D:       e.type = Entity::NORMAL_IN_2D; break;
case SLVS_E_DISTANCE:           e.type = Entity::DISTANCE; break;
case SLVS_E_WORKPLANE:          e.type = Entity::WORKPLANE; break;
case SLVS_E_LINE_SEGMENT:       e.type = Entity::LINE_SEGMENT; break;
case SLVS_E_CUBIC:              e.type = Entity::CUBIC; break;
case SLVS_E_CIRCLE:             e.type = Entity::CIRCLE; break;
case SLVS_E_ARC_OF_CIRCLE:      e.type = Entity::ARC_OF_CIRCLE; break;

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

    for(i = 0; i < ssys->constraints; i++) {
        Slvs_Constraint *sc = &(ssys->constraint[i]);
        ConstraintBase c = {};

        int t;
        switch(sc->type) {
case SLVS_C_POINTS_COINCIDENT:  t = Constraint::POINTS_COINCIDENT; break;
case SLVS_C_PT_PT_DISTANCE:     t = Constraint::PT_PT_DISTANCE; break;
case SLVS_C_PT_PLANE_DISTANCE:  t = Constraint::PT_PLANE_DISTANCE; break;
case SLVS_C_PT_LINE_DISTANCE:   t = Constraint::PT_LINE_DISTANCE; break;
case SLVS_C_PT_FACE_DISTANCE:   t = Constraint::PT_FACE_DISTANCE; break;
case SLVS_C_PT_IN_PLANE:        t = Constraint::PT_IN_PLANE; break;
case SLVS_C_PT_ON_LINE:         t = Constraint::PT_ON_LINE; break;
case SLVS_C_PT_ON_FACE:         t = Constraint::PT_ON_FACE; break;
case SLVS_C_EQUAL_LENGTH_LINES: t = Constraint::EQUAL_LENGTH_LINES; break;
case SLVS_C_LENGTH_RATIO:       t = Constraint::LENGTH_RATIO; break;
case SLVS_C_EQ_LEN_PT_LINE_D:   t = Constraint::EQ_LEN_PT_LINE_D; break;
case SLVS_C_EQ_PT_LN_DISTANCES: t = Constraint::EQ_PT_LN_DISTANCES; break;
case SLVS_C_EQUAL_ANGLE:        t = Constraint::EQUAL_ANGLE; break;
case SLVS_C_EQUAL_LINE_ARC_LEN: t = Constraint::EQUAL_LINE_ARC_LEN; break;
case SLVS_C_LENGTH_DIFFERENCE:  t = Constraint::LENGTH_DIFFERENCE; break;
case SLVS_C_SYMMETRIC:          t = Constraint::SYMMETRIC; break;
case SLVS_C_SYMMETRIC_HORIZ:    t = Constraint::SYMMETRIC_HORIZ; break;
case SLVS_C_SYMMETRIC_VERT:     t = Constraint::SYMMETRIC_VERT; break;
case SLVS_C_SYMMETRIC_LINE:     t = Constraint::SYMMETRIC_LINE; break;
case SLVS_C_AT_MIDPOINT:        t = Constraint::AT_MIDPOINT; break;
case SLVS_C_HORIZONTAL:         t = Constraint::HORIZONTAL; break;
case SLVS_C_VERTICAL:           t = Constraint::VERTICAL; break;
case SLVS_C_DIAMETER:           t = Constraint::DIAMETER; break;
case SLVS_C_PT_ON_CIRCLE:       t = Constraint::PT_ON_CIRCLE; break;
case SLVS_C_SAME_ORIENTATION:   t = Constraint::SAME_ORIENTATION; break;
case SLVS_C_ANGLE:              t = Constraint::ANGLE; break;
case SLVS_C_PARALLEL:           t = Constraint::PARALLEL; break;
case SLVS_C_PERPENDICULAR:      t = Constraint::PERPENDICULAR; break;
case SLVS_C_ARC_LINE_TANGENT:   t = Constraint::ARC_LINE_TANGENT; break;
case SLVS_C_CUBIC_LINE_TANGENT: t = Constraint::CUBIC_LINE_TANGENT; break;
case SLVS_C_EQUAL_RADIUS:       t = Constraint::EQUAL_RADIUS; break;
case SLVS_C_PROJ_PT_DISTANCE:   t = Constraint::PROJ_PT_DISTANCE; break;
case SLVS_C_WHERE_DRAGGED:      t = Constraint::WHERE_DRAGGED; break;
case SLVS_C_CURVE_CURVE_TANGENT:t = Constraint::CURVE_CURVE_TANGENT; break;

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
    int how = SYS.Solve(&g, &(ssys->dof), &bad, andFindBad, false);

    switch(how) {
        case System::SOLVED_OKAY:
            ssys->result = SLVS_RESULT_OKAY;
            break;

        case System::DIDNT_CONVERGE:
            ssys->result = SLVS_RESULT_DIDNT_CONVERGE;
            break;

        case System::SINGULAR_JACOBIAN:
            ssys->result = SLVS_RESULT_INCONSISTENT;
            break;

        case System::TOO_MANY_UNKNOWNS:
            ssys->result = SLVS_RESULT_TOO_MANY_UNKNOWNS;
            break;

        default: oops();
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
            ssys->failed[i] = bad.elem[i].v;
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
