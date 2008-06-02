#include "solvespace.h"

const hConstraint Constraint::NO_CONSTRAINT = { 0 };

char *Constraint::DescriptionString(void) {
    static char ret[1024];

    char *s;
    switch(type) {
        case POINTS_COINCIDENT: s = "pts-coincident"; break;
        case PT_PT_DISTANCE:    s = "pt-pt-distance"; break;
        case PT_LINE_DISTANCE:  s = "pt-line-distance"; break;
        case PT_PLANE_DISTANCE: s = "pt-plane-distance"; break;
        case PT_IN_PLANE:       s = "pt-in-plane"; break;
        case PT_ON_LINE:        s = "pt-on-line"; break;
        case PT_ON_FACE:        s = "pt-on-face"; break;
        case EQUAL_LENGTH_LINES:s = "eq-length"; break;
        case LENGTH_RATIO:      s = "length-ratio"; break;
        case SYMMETRIC:         s = "symmetric"; break;
        case SYMMETRIC_HORIZ:   s = "symmetric-h"; break;
        case SYMMETRIC_VERT:    s = "symmetric-v"; break;
        case AT_MIDPOINT:       s = "at-midpoint"; break;
        case HORIZONTAL:        s = "horizontal"; break;
        case VERTICAL:          s = "vertical"; break;
        case DIAMETER:          s = "diameter"; break;
        case PT_ON_CIRCLE:      s = "pt-on-circle"; break;
        case SAME_ORIENTATION:  s = "same-orientation"; break;
        case ANGLE:             s = "angle"; break;
        case PARALLEL:          s = "parallel"; break;
        case EQUAL_RADIUS:      s = "eq-radius"; break;
        default:                s = "???"; break;
    }

    sprintf(ret, "c%03x-%s", h.v, s);
    return ret;
}

void Constraint::AddConstraint(Constraint *c) {
    SS.constraint.AddAndAssignId(c);

    SS.GenerateAll();
}

void Constraint::Constrain(int type, hEntity ptA, hEntity ptB, hEntity entityA)
{
    Constraint c;
    memset(&c, 0, sizeof(c));
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.type = type;
    c.ptA = ptA;
    c.ptB = ptB;
    c.entityA = entityA;
    AddConstraint(&c);
}

void Constraint::ConstrainCoincident(hEntity ptA, hEntity ptB) {
    Constrain(POINTS_COINCIDENT, ptA, ptB, Entity::NO_ENTITY);
}

void Constraint::MenuConstrain(int id) {
    Constraint c;
    memset(&c, 0, sizeof(c));
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();

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
            } else if(gs.workplanes == 1 && gs.points == 1 && gs.n == 2) {
                c.type = PT_PLANE_DISTANCE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) {
                c.type = PT_LINE_DISTANCE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.circlesOrArcs == 1 && gs.n == 1) {
                c.type = DIAMETER;
                c.entityA = gs.entity[0];
            } else {
                Error("Bad selection for distance / diameter constraint.");
                return;
            }
            if(c.type == PT_PT_DISTANCE) {
                Vector n = SS.GW.projRight.Cross(SS.GW.projUp);
                Vector a = SS.GetEntity(c.ptA)->PointGetNum();
                Vector b = SS.GetEntity(c.ptB)->PointGetNum();
                c.disp.offset = n.Cross(a.Minus(b));
                c.disp.offset = (c.disp.offset).WithMagnitude(50/SS.GW.scale);
            } else {
                c.disp.offset = Vector::From(0, 0, 0);
            }

            c.exprA = Expr::From("0")->DeepCopyKeep();
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;
        }

        case GraphicsWindow::MNU_ON_ENTITY:
            if(gs.points == 2 && gs.n == 2) {
                c.type = POINTS_COINCIDENT;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.points == 1 && gs.workplanes == 1 && gs.n == 2) {
                c.type = PT_IN_PLANE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.points == 1 && gs.lineSegments == 1 && gs.n == 2) {
                c.type = PT_ON_LINE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.points == 1 && gs.circlesOrArcs == 1 && gs.n == 2) {
                c.type = PT_ON_CIRCLE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.points == 1 && gs.faces == 1 && gs.n == 2) {
                c.type = PT_ON_FACE;
                c.ptA = gs.point[0];
                c.entityA = gs.face[0];
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
            } else if(gs.circlesOrArcs == 2 && gs.n == 2) {
                c.type = EQUAL_RADIUS;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            } else {
                Error("Bad selection for equal length / radius constraint.");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_RATIO:
            if(gs.lineSegments == 2 && gs.n == 2) {
                c.type = LENGTH_RATIO;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            } else {
                Error("Bad selection for length ratio constraint.");
                return;
            }

            c.exprA = Expr::From("0")->DeepCopyKeep();
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_AT_MIDPOINT:
            if(gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) {
                c.type = AT_MIDPOINT;
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];
            } else if(gs.lineSegments == 1 && gs.workplanes == 1 && gs.n == 2) {
                c.type = AT_MIDPOINT;
                int i = SS.GetEntity(gs.entity[0])->IsWorkplane() ? 1 : 0;
                c.entityA = gs.entity[i];
                c.entityB = gs.entity[1-i];
            } else {
                Error("Bad selection for at midpoint constraint.");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_SYMMETRIC:
            if(gs.points == 2 &&
                                ((gs.workplanes == 1 && gs.n == 3) ||
                                 (gs.n == 2)))
            {
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 1 && 
                                ((gs.workplanes == 1 && gs.n == 2) ||
                                 (gs.n == 1)))
            {
                int i = SS.GetEntity(gs.entity[0])->IsWorkplane() ? 1 : 0;
                Entity *line = SS.GetEntity(gs.entity[i]);
                c.entityA = gs.entity[1-i];
                c.ptA = line->point[0];
                c.ptB = line->point[1];
            } else {
                Error("Bad selection for symmetric constraint.");
                return;
            }
            if(c.entityA.v == Entity::NO_ENTITY.v) {
                if(c.workplane.v == Entity::FREE_IN_3D.v) {
                    Error("Must be locked in to workplane when constraining "
                          "symmetric without an explicit symmetry plane.");
                    return;
                }
                Vector pa = SS.GetEntity(c.ptA)->PointGetNum();
                Vector pb = SS.GetEntity(c.ptB)->PointGetNum();
                Vector dp = pa.Minus(pb);
                Entity *norm = SS.GetEntity(c.workplane)->Normal();;
                Vector u = norm->NormalU(), v = norm->NormalV();
                if(fabs(dp.Dot(u)) > fabs(dp.Dot(v))) {
                    c.type = SYMMETRIC_HORIZ;
                } else {
                    c.type = SYMMETRIC_VERT;
                }
            } else {
                // Symmetry with a symmetry plane specified explicitly.
                c.type = SYMMETRIC;
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

        case GraphicsWindow::MNU_ORIENTED_SAME:
            if(gs.anyNormals == 2 && gs.n == 2) {
                c.type = SAME_ORIENTATION;
                c.entityA = gs.anyNormal[0];
                c.entityB = gs.anyNormal[1];
            } else {
                Error("Bad selection for same orientation constraint.");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_OTHER_ANGLE:
            if(gs.constraints == 1 && gs.n == 0) {
                Constraint *c = SS.GetConstraint(gs.constraint[0]);
                if(c->type == ANGLE) {
                    c->otherAngle = !(c->otherAngle);
                    c->ModifyToSatisfy();
                    break;
                }
            }
            Error("Must select an angle constraint.");
            break;

        case GraphicsWindow::MNU_ANGLE:
            if(gs.vectors == 2 && gs.n == 2) {
                c.type = ANGLE;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
                c.exprA = Expr::From(0.0)->DeepCopyKeep();
                c.otherAngle = true;
            } else {
                Error("Bad selection for angle constraint.");
                return;
            }
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_PARALLEL:
            if(gs.vectors == 2 && gs.n == 2) {
                c.type = PARALLEL;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
            } else {
                Error("Bad selection for parallel constraint.");
                return;
            }
            AddConstraint(&c);
            break;

        case GraphicsWindow::MNU_SOLVE_NOW:
            SS.GenerateAll(0, INT_MAX);
            return;

        default: oops();
    }

    SS.GW.ClearSelection();
    InvalidateGraphics();
}

Expr *Constraint::VectorsParallel(int eq, ExprVector a, ExprVector b) {
    ExprVector r = a.Cross(b);
    // Hairy ball theorem screws me here. There's no clean solution that I
    // know, so let's pivot on the initial numerical guess.
    double mx = fabs((a.x)->Eval()) + fabs((b.x)->Eval());
    double my = fabs((a.y)->Eval()) + fabs((b.y)->Eval());
    double mz = fabs((a.z)->Eval()) + fabs((b.z)->Eval());
    // The basis vector in which the vectors have the LEAST energy is the
    // one that we should look at most (e.g. if both vectors lie in the xy
    // plane, then the z component of the cross product is most important).
    // So find the strongest component of a and b, and that's the component
    // of the cross product to ignore.
    double m = max(mx, max(my, mz));
    Expr *e0, *e1;
         if(m == mx) { e0 = r.y; e1 = r.z; }
    else if(m == my) { e0 = r.z; e1 = r.x; }
    else if(m == mz) { e0 = r.x; e1 = r.y; }
    else oops();

    if(eq == 0) return e0;
    if(eq == 1) return e1;
    oops();
}

Expr *Constraint::PointLineDistance(hEntity wrkpl, hEntity hpt, hEntity hln) {
    Entity *ln = SS.GetEntity(hln);
    Entity *a = SS.GetEntity(ln->point[0]);
    Entity *b = SS.GetEntity(ln->point[1]);

    Entity *p = SS.GetEntity(hpt);

    if(wrkpl.v == Entity::FREE_IN_3D.v) {
        ExprVector ep = p->PointGetExprs();

        ExprVector ea = a->PointGetExprs();
        ExprVector eb = b->PointGetExprs();
        ExprVector eab = ea.Minus(eb);
        Expr *m = eab.Magnitude();

        return ((eab.Cross(ea.Minus(ep))).Magnitude())->Div(m);
    } else {
        Expr *ua, *va, *ub, *vb;
        a->PointGetExprsInWorkplane(wrkpl, &ua, &va);
        b->PointGetExprsInWorkplane(wrkpl, &ub, &vb);

        Expr *du = ua->Minus(ub);
        Expr *dv = va->Minus(vb);

        Expr *u, *v;
        p->PointGetExprsInWorkplane(wrkpl, &u, &v);

        Expr *m = ((du->Square())->Plus(dv->Square()))->Sqrt();

        Expr *proj = (dv->Times(ua->Minus(u)))->Minus(
                     (du->Times(va->Minus(v))));

        return proj->Div(m);
    }
}

Expr *Constraint::PointPlaneDistance(ExprVector p, hEntity hpl) {
    ExprVector n;
    Expr *d;
    SS.GetEntity(hpl)->WorkplaneGetPlaneExprs(&n, &d);
    return (p.Dot(n))->Minus(d);
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

ExprVector Constraint::PointInThreeSpace(hEntity workplane, Expr *u, Expr *v) {
    Entity *w = SS.GetEntity(workplane);

    ExprVector ub = w->Normal()->NormalExprsU();
    ExprVector vb = w->Normal()->NormalExprsV();
    ExprVector ob = w->WorkplaneGetOffsetExprs();

    return (ub.ScaledBy(u)).Plus(vb.ScaledBy(v)).Plus(ob);
}

void Constraint::ModifyToSatisfy(void) {
    if(type == ANGLE) {
        Vector a = SS.GetEntity(entityA)->VectorGetNum();
        Vector b = SS.GetEntity(entityB)->VectorGetNum();
        if(otherAngle) a = a.ScaledBy(-1);
        if(workplane.v != Entity::FREE_IN_3D.v) {
            a = a.ProjectVectorInto(workplane);
            b = b.ProjectVectorInto(workplane);
        }
        double c = (a.Dot(b))/(a.Magnitude() * b.Magnitude());
        double theta = acos(c)*180/PI;
        Expr::FreeKeep(&exprA);
        exprA = Expr::From(theta)->DeepCopyKeep();
    } else {
        // We'll fix these ones up by looking at their symbolic equation;
        // that means no extra work.
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
        exprA = Expr::From(nd)->DeepCopyKeep();

        l.Clear();
    }
}

void Constraint::AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index) {
    Equation eq;
    eq.e = expr;
    eq.h = h.equation(index);
    l->Add(&eq);
}

void Constraint::Generate(IdList<Equation,hEquation> *l) {
    Expr *exA = NULL;
    if(exprA) exA = exprA->DeepCopy();

    switch(type) {
        case PT_PT_DISTANCE:
            AddEq(l, Distance(workplane, ptA, ptB)->Minus(exA), 0);
            break;

        case PT_LINE_DISTANCE:
            AddEq(l,
                PointLineDistance(workplane, ptA, entityA)->Minus(exA), 0);
            break;

        case PT_PLANE_DISTANCE: {
            ExprVector pt = SS.GetEntity(ptA)->PointGetExprs();
            AddEq(l, (PointPlaneDistance(pt, entityA))->Minus(exA), 0);
            break;
        }

        case EQUAL_LENGTH_LINES: {
            Entity *a = SS.GetEntity(entityA);
            Entity *b = SS.GetEntity(entityB);
            AddEq(l, Distance(workplane, a->point[0], a->point[1])->Minus(
                     Distance(workplane, b->point[0], b->point[1])), 0);
            break;
        }

        case LENGTH_RATIO: {
            Entity *a = SS.GetEntity(entityA);
            Entity *b = SS.GetEntity(entityB);
            Expr *la = Distance(workplane, a->point[0], a->point[1]);
            Expr *lb = Distance(workplane, b->point[0], b->point[1]);
            AddEq(l, (la->Div(lb))->Minus(exA), 0);
            break;
        }

        case DIAMETER: {
            Entity *circle = SS.GetEntity(entityA);
            Expr *r = circle->CircleGetRadiusExpr();
            AddEq(l, (r->Times(Expr::From(2)))->Minus(exA), 0);
            break;
        }

        case EQUAL_RADIUS: {
            Entity *c1 = SS.GetEntity(entityA);
            Entity *c2 = SS.GetEntity(entityB);
            AddEq(l, (c1->CircleGetRadiusExpr())->Minus(
                      c2->CircleGetRadiusExpr()), 0);
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

        case PT_IN_PLANE:
            // This one works the same, whether projected or not.
            AddEq(l, PointPlaneDistance(
                        SS.GetEntity(ptA)->PointGetExprs(), entityA), 0);
            break;

        case PT_ON_FACE: {
            // a plane, n dot (p - p0) = 0
            ExprVector p = SS.GetEntity(ptA)->PointGetExprs();
            Entity *f = SS.GetEntity(entityA);
            ExprVector p0 = f->FaceGetPointExprs();
            ExprVector n = f->FaceGetNormalExprs();
            AddEq(l, (p.Minus(p0)).Dot(n), 0);
            break;
        }

        case PT_ON_LINE:
            if(workplane.v == Entity::FREE_IN_3D.v) {
                Entity *ln = SS.GetEntity(entityA);
                Entity *a = SS.GetEntity(ln->point[0]);
                Entity *b = SS.GetEntity(ln->point[1]);
                Entity *p = SS.GetEntity(ptA);

                ExprVector ep = p->PointGetExprs();
                ExprVector ea = a->PointGetExprs();
                ExprVector eb = b->PointGetExprs();
                ExprVector eab = ea.Minus(eb);
                ExprVector eap = ea.Minus(ep);

                AddEq(l, VectorsParallel(0, eab, eap), 0);
                AddEq(l, VectorsParallel(1, eab, eap), 1);
            } else {
                AddEq(l, PointLineDistance(workplane, ptA, entityA), 0);
            }
            break;

        case PT_ON_CIRCLE: {
            Entity *circle = SS.GetEntity(entityA);
            hEntity center = circle->point[0];
            Expr *radius = circle->CircleGetRadiusExpr();
            AddEq(l, Distance(workplane, ptA, center)->Minus(radius), 0);
            break;
        }

        case AT_MIDPOINT:
            if(workplane.v == Entity::FREE_IN_3D.v) {
                Entity *ln = SS.GetEntity(entityA);
                ExprVector a = SS.GetEntity(ln->point[0])->PointGetExprs();
                ExprVector b = SS.GetEntity(ln->point[1])->PointGetExprs();
                ExprVector m = (a.Plus(b)).ScaledBy(Expr::From(0.5));

                if(ptA.v) {
                    ExprVector p = SS.GetEntity(ptA)->PointGetExprs();
                    AddEq(l, (m.x)->Minus(p.x), 0);
                    AddEq(l, (m.y)->Minus(p.y), 1);
                    AddEq(l, (m.z)->Minus(p.z), 2);
                } else {
                    AddEq(l, PointPlaneDistance(m, entityB), 0);
                }
            } else {
                Entity *ln = SS.GetEntity(entityA);
                Entity *a = SS.GetEntity(ln->point[0]);
                Entity *b = SS.GetEntity(ln->point[1]);
                
                Expr *au, *av, *bu, *bv;
                a->PointGetExprsInWorkplane(workplane, &au, &av);
                b->PointGetExprsInWorkplane(workplane, &bu, &bv);
                Expr *mu = Expr::From(0.5)->Times(au->Plus(bu));
                Expr *mv = Expr::From(0.5)->Times(av->Plus(bv));

                if(ptA.v) {
                    Entity *p = SS.GetEntity(ptA);
                    Expr *pu, *pv;
                    p->PointGetExprsInWorkplane(workplane, &pu, &pv);
                    AddEq(l, pu->Minus(mu), 0);
                    AddEq(l, pv->Minus(mv), 1);
                } else {
                    ExprVector m = PointInThreeSpace(workplane, mu, mv);
                    AddEq(l, PointPlaneDistance(m, entityB), 0);
                }
            }
            break;

        case SYMMETRIC:
            if(workplane.v == Entity::FREE_IN_3D.v) {
                Entity *plane = SS.GetEntity(entityA);
                Entity *ea = SS.GetEntity(ptA);
                Entity *eb = SS.GetEntity(ptB);
                ExprVector a = ea->PointGetExprs();
                ExprVector b = eb->PointGetExprs();

                // The midpoint of the line connecting the symmetric points
                // lies on the plane of the symmetry.
                ExprVector m = (a.Plus(b)).ScaledBy(Expr::From(0.5));
                AddEq(l, PointPlaneDistance(m, plane->h), 0);

                // And projected into the plane of symmetry, the points are
                // coincident.
                Expr *au, *av, *bu, *bv;
                ea->PointGetExprsInWorkplane(plane->h, &au, &av);
                eb->PointGetExprsInWorkplane(plane->h, &bu, &bv);
                AddEq(l, au->Minus(bu), 0);
                AddEq(l, av->Minus(bv), 1);
            } else {
                Entity *plane = SS.GetEntity(entityA);
                Entity *a = SS.GetEntity(ptA);
                Entity *b = SS.GetEntity(ptB);

                Expr *au, *av, *bu, *bv;
                a->PointGetExprsInWorkplane(workplane, &au, &av);
                b->PointGetExprsInWorkplane(workplane, &bu, &bv);
                Expr *mu = Expr::From(0.5)->Times(au->Plus(bu));
                Expr *mv = Expr::From(0.5)->Times(av->Plus(bv));

                ExprVector m = PointInThreeSpace(workplane, mu, mv);
                AddEq(l, PointPlaneDistance(m, plane->h), 0);

                // Construct a vector within the workplane that is normal
                // to the symmetry pane's normal (i.e., that lies in the
                // plane of symmetry). The line connecting the points is
                // perpendicular to that constructed vector.
                Entity *w = SS.GetEntity(workplane);
                ExprVector u = w->Normal()->NormalExprsU();
                ExprVector v = w->Normal()->NormalExprsV();

                ExprVector pa = a->PointGetExprs();
                ExprVector pb = b->PointGetExprs();
                ExprVector n;
                Expr *d;
                plane->WorkplaneGetPlaneExprs(&n, &d);
                AddEq(l, (n.Cross(u.Cross(v))).Dot(pa.Minus(pb)), 1);
            }
            break;

        case SYMMETRIC_HORIZ:
        case SYMMETRIC_VERT: {
            Entity *a = SS.GetEntity(ptA);
            Entity *b = SS.GetEntity(ptB);

            Expr *au, *av, *bu, *bv;
            a->PointGetExprsInWorkplane(workplane, &au, &av);
            b->PointGetExprsInWorkplane(workplane, &bu, &bv);

            if(type == SYMMETRIC_HORIZ) {
                AddEq(l, av->Minus(bv), 0);
                AddEq(l, au->Plus(bu), 0);
            } else {
                AddEq(l, au->Minus(bu), 0);
                AddEq(l, av->Plus(bv), 0);
            }
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

        case SAME_ORIENTATION: {
            Entity *a = SS.GetEntity(entityA);
            Entity *b = SS.GetEntity(entityB);
            ExprVector au = a->NormalExprsU(),
                       av = a->NormalExprsV(),
                       an = a->NormalExprsN();
            ExprVector bu = b->NormalExprsU(),
                       bv = b->NormalExprsV(),
                       bn = b->NormalExprsN();
            
            AddEq(l, VectorsParallel(0, an, bn), 0);
            AddEq(l, VectorsParallel(1, an, bn), 1);
            Expr *d1 = au.Dot(bv);
            Expr *d2 = au.Dot(bu);
            // Allow either orientation for the coordinate system, depending
            // on how it was drawn.
            if(fabs(d1->Eval()) < fabs(d2->Eval())) {
                AddEq(l, d1, 2);
            } else {
                AddEq(l, d2, 2);
            }
            break;
        }

        case ANGLE: {
            Entity *a = SS.GetEntity(entityA);
            Entity *b = SS.GetEntity(entityB);
            ExprVector ae = a->VectorGetExprs();
            ExprVector be = b->VectorGetExprs();
            if(otherAngle) ae = ae.ScaledBy(Expr::From(-1));
            Expr *c;
            if(workplane.v == Entity::FREE_IN_3D.v) {
                Expr *mags = (ae.Magnitude())->Times(be.Magnitude());
                c = (ae.Dot(be))->Div(mags);
            } else {
                Entity *w = SS.GetEntity(workplane);
                ExprVector u = w->Normal()->NormalExprsU();
                ExprVector v = w->Normal()->NormalExprsV();
                Expr *ua = u.Dot(ae);
                Expr *va = v.Dot(ae);
                Expr *ub = u.Dot(be);
                Expr *vb = v.Dot(be);
                Expr *maga = (ua->Square()->Plus(va->Square()))->Sqrt();
                Expr *magb = (ub->Square()->Plus(vb->Square()))->Sqrt();
                Expr *dot = (ua->Times(ub))->Plus(va->Times(vb));
                c = dot->Div(maga->Times(magb));
            }
            Expr *rads = exA->Times(Expr::From(PI/180));
            AddEq(l, c->Minus(rads->Cos()), 0);
            break;
        }

        case PARALLEL: {
            ExprVector a = SS.GetEntity(entityA)->VectorGetExprs();
            ExprVector b = SS.GetEntity(entityB)->VectorGetExprs();

            if(workplane.v == Entity::FREE_IN_3D.v) {
                AddEq(l, VectorsParallel(0, a, b), 0);
                AddEq(l, VectorsParallel(1, a, b), 1);
            } else {
                Entity *w = SS.GetEntity(workplane);
                ExprVector wn = w->Normal()->NormalExprsN();
                AddEq(l, (a.Cross(b)).Dot(wn), 0);
            }
            break;
        }

        default: oops();
    }
}
