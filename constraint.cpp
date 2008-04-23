#include "solvespace.h"

hConstraint Constraint::AddConstraint(Constraint *c) {
    SS.constraint.AddAndAssignId(c);
    return c->h;
}

void Constraint::ConstrainCoincident(hEntity ptA, hEntity ptB) {
    Constraint c;
    memset(&c, 0, sizeof(c));
    c.group = SS.GW.activeGroup;
    c.type = Constraint::POINTS_COINCIDENT;
    c.ptA = ptA;
    c.ptB = ptB;
    SS.constraint.AddAndAssignId(&c);
}

void Constraint::MenuConstrain(int id) {
    Constraint c;
    memset(&c, 0, sizeof(c));
    c.group = SS.GW.activeGroup;

    SS.GW.GroupSelection();
#define gs (SS.GW.gs)

    switch(id) {
        case GraphicsWindow::MNU_DISTANCE_DIA:
            if(gs.points == 2 && gs.n == 2) {
                c.type = PT_PT_DISTANCE;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 1 && gs.n == 1) {
                c.type = PT_PT_DISTANCE;
                Entity *e = SS.GetEntity(gs.entity[0]);
                c.ptA = e->assoc[0];
                c.ptB = e->assoc[1];
            } else {
                Error("Bad selection for distance / diameter constraint.");
                return;
            }
            c.disp.offset = Vector::MakeFrom(50, 50, 50);
            c.exprA = Expr::FromString("0")->DeepCopyKeep();
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_ON_ENTITY:
            if(gs.points == 2 && gs.n == 2) {
                c.type = POINTS_COINCIDENT;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.points == 1 && gs.planes == 1 && gs.n == 2) {
                c.type = PT_IN_PLANE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else {
                Error("Bad selection for on point / curve / plane constraint.");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_EQUAL:
            if(gs.lineSegments == 2 && gs.n == 2) {
                c.type = EQUAL_LENGTH_LINES;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            } else {
                Error("Bad selection for equal length / radius constraint.");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_VERTICAL:
        case GraphicsWindow::MNU_HORIZONTAL: {
            hEntity ha, hb;
            if(gs.lineSegments == 1 && gs.n == 1) {
                c.entityA = gs.entity[0];
                Entity *e = SS.GetEntity(c.entityA);
                ha = e->assoc[0];
                hb = e->assoc[1];
            } else if(gs.points == 2 && gs.n == 2) {
                ha = c.ptA = gs.point[0];
                hb = c.ptB = gs.point[1];
            } else {
                Error("Bad selection for horizontal / vertical constraint.");
                return;
            }
            Entity *ea = SS.GetEntity(ha);
            Entity *eb = SS.GetEntity(hb);
            if(ea->csys.v == Entity::NO_CSYS.v &&
               eb->csys.v == Entity::NO_CSYS.v)
            {
                Error("Horizontal/vertical constraint applies only to "
                      "entities drawn in a 2d coordinate system.");
                return;
            }
            if(eb->csys.v == SS.GW.activeCsys.v) {
                // We are constraining two points in two different csyss; so
                // we have two choices for the definitons of the coordinate
                // directions. ptA's gets chosen, so make sure that's the
                // active csys.
                hEntity t = c.ptA;
                c.ptA = c.ptB;
                c.ptB = t;
            }
            if(id == GraphicsWindow::MNU_HORIZONTAL) {
                c.type = HORIZONTAL;
            } else {
                c.type = VERTICAL;
            }
            AddConstraint(&c);
            break;
        }

        case GraphicsWindow::MNU_SOLVE_NOW:
            SS.Solve();
            return;

        default: oops();
    }

    SS.GW.ClearSelection();
    InvalidateGraphics();
}

Expr *Constraint::Distance(hEntity hpa, hEntity hpb) {
    Entity *pa = SS.GetEntity(hpa);
    Entity *pb = SS.GetEntity(hpb);

    if(!(pa->IsPoint() && pb->IsPoint())) oops();

    if(pa->type == Entity::POINT_IN_2D &&
       pb->type == Entity::POINT_IN_2D &&
       pa->csys.v == pb->csys.v)
    {
        // A nice case; they are both in the same 2d csys, so I can write
        // the equation in terms of the basis vectors in that csys.
        Expr *du = Expr::FromParam(pa->param.h[0])->Minus(
                   Expr::FromParam(pb->param.h[0]));
        Expr *dv = Expr::FromParam(pa->param.h[1])->Minus(
                   Expr::FromParam(pb->param.h[1]));

        return ((du->Square())->Plus(dv->Square()))->Sqrt();
    }

    // The usual case, just write it in terms of the coordinates
    ExprVector ea, eb, eab;
    ea = pa->PointGetExprs();
    eb = pb->PointGetExprs();
    eab = ea.Minus(eb);

    return eab.Magnitude();
}

void Constraint::ModifyToSatisfy(void) {
    IdList<Equation,hEquation> l;
    // An uninit IdList could lead us to free some random address, bad.
    memset(&l, 0, sizeof(l));

    Generate(&l);
    if(l.n != 1) oops();

    // These equations are written in the form f(...) - d = 0, where
    // d is the value of the exprA.
    double v = (l.elem[0].e)->Eval();
    double nd = exprA->Eval() + v;
    Expr::FreeKeep(&exprA);
    exprA = Expr::FromConstant(nd)->DeepCopyKeep();

    l.Clear();
}

void Constraint::AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index) {
    Equation eq;
    eq.e = expr;
    eq.h = h.equation(index);
    l->Add(&eq);
}

void Constraint::Generate(IdList<Equation,hEquation> *l) {
    switch(type) {
        case PT_PT_DISTANCE:
            AddEq(l, Distance(ptA, ptB)->Minus(exprA), 0);
            break;

        case EQUAL_LENGTH_LINES: {
            Entity *a = SS.GetEntity(entityA);
            Entity *b = SS.GetEntity(entityB);
            AddEq(l, Distance(a->assoc[0], a->assoc[1])->Minus(
                     Distance(b->assoc[0], b->assoc[1])), 0);
            break;
        }

        case POINTS_COINCIDENT: {
            Entity *a = SS.GetEntity(ptA);
            Entity *b = SS.GetEntity(ptB);
            if(!a->IsPointIn3d() && b->IsPointIn3d()) {
                Entity *t = a;
                a = b; b = t;
            }
            if(a->IsPointIn3d() && b->IsPointIn3d()) {
                // Easy case: both points have 3 DOF, so write three eqs.
                ExprVector ea, eb, eab;
                ea = a->PointGetExprs();
                eb = b->PointGetExprs();
                eab = ea.Minus(eb);
                AddEq(l, eab.x, 0);
                AddEq(l, eab.y, 1);
                AddEq(l, eab.z, 2);
            } else if(!(a->IsPointIn3d() || b->IsPointIn3d()) && 
                       (a->csys.v == b->csys.v))
            {
                // Both in same csys, nice.
                AddEq(l, Expr::FromParam(a->param.h[0])->Minus(
                         Expr::FromParam(b->param.h[0])), 0);
                AddEq(l, Expr::FromParam(a->param.h[1])->Minus(
                         Expr::FromParam(b->param.h[1])), 1);
            } else {
                // Either two 2 DOF points in different planes, or one
                // 3 DOF point and one 2 DOF point. Either way, write two
                // equations on the projection of a into b's plane.
                ExprVector p3;
                p3 = a->PointGetExprs();
                Entity *csy = SS.GetEntity(b->csys);
                ExprVector offset = csy->Csys2dGetOffsetExprs();
                p3 = p3.Minus(offset);
                ExprVector u, v;
                csy->Csys2dGetBasisExprs(&u, &v);
                AddEq(l, Expr::FromParam(b->param.h[0])->Minus(p3.Dot(u)), 0);
                AddEq(l, Expr::FromParam(b->param.h[1])->Minus(p3.Dot(v)), 1);
            }
            break;
        }

        case PT_IN_PLANE: {
            ExprVector p, n;
            Expr *d;
            p = SS.GetEntity(ptA)->PointGetExprs();
            SS.GetEntity(entityA)->PlaneGetExprs(&n, &d);
            AddEq(l, (p.Dot(n))->Minus(d), 0);
            break;
        }

        case HORIZONTAL:
        case VERTICAL: {
            hEntity ha, hb;
            if(entityA.v) {
                Entity *e = SS.GetEntity(entityA);
                ha = e->assoc[0];
                hb = e->assoc[1];
            } else {
                ha = ptA;
                hb = ptB;
            }
            Entity *a = SS.GetEntity(ha);
            Entity *b = SS.GetEntity(hb);
            if(a->csys.v == Entity::NO_CSYS.v) {
                Entity *t = a;
                a = b;
                b = t;
            }
            
            if(a->csys.v == b->csys.v) {
                int i = (type == HORIZONTAL) ? 1 : 0;
                AddEq(l, Expr::FromParam(a->param.h[i])->Minus(
                         Expr::FromParam(b->param.h[i])), 0);
            } else {
                Entity *csy = SS.GetEntity(a->csys);
                ExprVector u, v;
                csy->Csys2dGetBasisExprs(&u, &v);
                ExprVector norm = (type == HORIZONTAL) ? v : u;
                ExprVector pa = a->PointGetExprs();
                ExprVector pb = b->PointGetExprs();
                AddEq(l, (pa.Minus(pb)).Dot(norm), 0);
            }
            break;
        }

        default: oops();
    }
}
