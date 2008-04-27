#include "solvespace.h"

char *Constraint::DescriptionString(void) {
    static char ret[1024];
    sprintf(ret, "c%04x", h.v);
    return ret;
}

hConstraint Constraint::AddConstraint(Constraint *c) {
    SS.constraint.AddAndAssignId(c);

    SS.GenerateAll(SS.GW.solving == GraphicsWindow::SOLVE_ALWAYS);
    return c->h;
}

void Constraint::ConstrainCoincident(hEntity ptA, hEntity ptB) {
    Constraint c;
    memset(&c, 0, sizeof(c));
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.activeWorkplane;
    c.type = Constraint::POINTS_COINCIDENT;
    c.ptA = ptA;
    c.ptB = ptB;
    AddConstraint(&c);
}

void Constraint::MenuConstrain(int id) {
    Constraint c;
    memset(&c, 0, sizeof(c));
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.activeWorkplane;

    SS.GW.GroupSelection();
#define gs (SS.GW.gs)

    switch(id) {
        case GraphicsWindow::MNU_DISTANCE_DIA: {
            if(gs.points == 2 && gs.n == 2) {
                c.type = PT_PT_DISTANCE;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 1 && gs.n == 1) {
                c.type = PT_PT_DISTANCE;
                Entity *e = SS.GetEntity(gs.entity[0]);
                c.ptA = e->point[0];
                c.ptB = e->point[1];
            } else {
                Error("Bad selection for distance / diameter constraint.");
                return;
            }
            Vector n = SS.GW.projRight.Cross(SS.GW.projUp);
            Vector a = SS.GetEntity(c.ptA)->PointGetCoords();
            Vector b = SS.GetEntity(c.ptB)->PointGetCoords();

            c.disp.offset = n.Cross(a.Minus(b)).WithMagnitude(50);
            c.exprA = Expr::FromString("0")->DeepCopyKeep();
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;
        }

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
            if(c.workplane.v == Entity::FREE_IN_3D.v) {
                Error("Select workplane before constraining horiz/vert.");
                return;
            }
            if(gs.lineSegments == 1 && gs.n == 1) {
                c.entityA = gs.entity[0];
                Entity *e = SS.GetEntity(c.entityA);
                ha = e->point[0];
                hb = e->point[1];
            } else if(gs.points == 2 && gs.n == 2) {
                ha = c.ptA = gs.point[0];
                hb = c.ptB = gs.point[1];
            } else {
                Error("Bad selection for horizontal / vertical constraint.");
                return;
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
            SS.GenerateAll(true);
            return;

        case GraphicsWindow::MNU_SOLVE_AUTO:
            if(SS.GW.solving == GraphicsWindow::SOLVE_ALWAYS) {
                SS.GW.solving = GraphicsWindow::DONT_SOLVE;
            } else {
                SS.GW.solving = GraphicsWindow::SOLVE_ALWAYS;
            }
            SS.GW.EnsureValidActives();
            break;

        default: oops();
    }

    SS.GW.ClearSelection();
    InvalidateGraphics();
}

Expr *Constraint::Distance(hEntity wrkpl, hEntity hpa, hEntity hpb) {
    Entity *pa = SS.GetEntity(hpa);
    Entity *pb = SS.GetEntity(hpb);
    if(!(pa->IsPoint() && pb->IsPoint())) oops();

    if(wrkpl.v == Entity::FREE_IN_3D.v) {
        // This is true distance
        ExprVector ea, eb, eab;
        ea = pa->PointGetExprs();
        eb = pb->PointGetExprs();
        eab = ea.Minus(eb);

        return eab.Magnitude();
    } else {
        // This is projected distance, in the given workplane.
        Expr *au, *av, *bu, *bv;

        pa->PointGetExprsInWorkplane(wrkpl, &au, &av);
        pb->PointGetExprsInWorkplane(wrkpl, &bu, &bv);

        Expr *du = au->Minus(bu);
        Expr *dv = av->Minus(bv);

        return ((du->Square())->Plus(dv->Square()))->Sqrt();
    }
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
            AddEq(l, Distance(workplane, ptA, ptB)->Minus(exprA), 0);
            break;

        case EQUAL_LENGTH_LINES: {
            Entity *a = SS.GetEntity(entityA);
            Entity *b = SS.GetEntity(entityB);
            AddEq(l, Distance(workplane, a->point[0], a->point[1])->Minus(
                     Distance(workplane, b->point[0], b->point[1])), 0);
            break;
        }

        case POINTS_COINCIDENT: {
            Entity *a = SS.GetEntity(ptA);
            Entity *b = SS.GetEntity(ptB);
            if(workplane.v == Entity::FREE_IN_3D.v) {
                ExprVector pa = a->PointGetExprs();
                ExprVector pb = b->PointGetExprs();
                AddEq(l, pa.x->Minus(pb.x), 0);
                AddEq(l, pa.y->Minus(pb.y), 1);
                AddEq(l, pa.z->Minus(pb.z), 2);
            } else {
                Expr *au, *av;
                Expr *bu, *bv;
                a->PointGetExprsInWorkplane(workplane, &au, &av);
                b->PointGetExprsInWorkplane(workplane, &bu, &bv);
                AddEq(l, au->Minus(bu), 0);
                AddEq(l, av->Minus(bv), 1);
            }
            break;
        }

        case PT_IN_PLANE: {
            // This one works the same, whether projected or not.
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
                ha = e->point[0];
                hb = e->point[1];
            } else {
                ha = ptA;
                hb = ptB;
            }
            Entity *a = SS.GetEntity(ha);
            Entity *b = SS.GetEntity(hb);

            Expr *au, *av, *bu, *bv;
            a->PointGetExprsInWorkplane(workplane, &au, &av);
            b->PointGetExprsInWorkplane(workplane, &bu, &bv);

            AddEq(l, (type == HORIZONTAL) ? av->Minus(bv) : au->Minus(bu), 0);
            break;
        }

        default: oops();
    }
}
