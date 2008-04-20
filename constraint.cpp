#include "solvespace.h"

hConstraint Constraint::AddConstraint(Constraint *c) {
    SS.constraint.AddAndAssignId(c);
    return c->h;
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
            c.exprA = Expr::FromString("300")->DeepCopyKeep();
            AddConstraint(&c);
            break;

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
    Expr *ax, *ay, *az;
    Expr *bx, *by, *bz;
    pa->PointGetExprs(&ax, &ay, &az);
    pb->PointGetExprs(&bx, &by, &bz);

    Expr *dx2 = (ax->Minus(bx))->Square();
    Expr *dy2 = (ay->Minus(by))->Square();
    Expr *dz2 = (az->Minus(bz))->Square();

    return (dx2->Plus(dy2->Plus(dz2)))->Sqrt();
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

        default: oops();
    }
}
