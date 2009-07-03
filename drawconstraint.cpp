#include "solvespace.h"

bool Constraint::HasLabel(void) {
    switch(type) {
        case PT_LINE_DISTANCE:
        case PT_PLANE_DISTANCE:
        case PT_FACE_DISTANCE:
        case PT_PT_DISTANCE:
        case DIAMETER:
        case LENGTH_RATIO:
        case ANGLE:
        case COMMENT:
            return true;

        default:
            return false;
    }
}

void Constraint::LineDrawOrGetDistance(Vector a, Vector b) {
    if(dogd.drawing) {
        if(dogd.sel) {
            dogd.sel->AddEdge(a, b);
        } else {
            glBegin(GL_LINE_STRIP);
                glxVertex3v(a);
                glxVertex3v(b);
            glEnd();
        }
    } else {
        Point2d ap = SS.GW.ProjectPoint(a);
        Point2d bp = SS.GW.ProjectPoint(b);

        double d = dogd.mp.DistanceToLine(ap, bp.Minus(ap), true);
        dogd.dmin = min(dogd.dmin, d);
    }
    dogd.refp = (a.Plus(b)).ScaledBy(0.5);
}

static void LineCallback(void *fndata, Vector a, Vector b)
{
    Constraint *c = (Constraint *)fndata;
    c->LineDrawOrGetDistance(a, b);
}

double Constraint::EllipticalInterpolation(double rx, double ry, double theta) {
    double ex = rx*cos(theta);
    double ey = ry*sin(theta);
    double v = sqrt(ex*ex + ey*ey);

    return v;
}

char *Constraint::Label(void) {
    static char Ret[1024];
    if(type == ANGLE) {
        sprintf(Ret, "%.2f", valA);
    } else if(type == LENGTH_RATIO) {
        sprintf(Ret, "%.3f:1", valA);
    } else if(type == COMMENT) {
        strcpy(Ret, comment.str);
    } else {
        // valA has units of distance
        strcpy(Ret, SS.MmToString(fabs(valA)));
    }
    if(reference) {
        strcat(Ret, " REF");
    }
    return Ret;
}

void Constraint::DoLabel(Vector ref, Vector *labelPos, Vector gr, Vector gu) {
    char *s = Label();
    double swidth = glxStrWidth(s), sheight = glxStrHeight();
    if(labelPos) {
        // labelPos is from the top left corner (for the text box used to
        // edit things), but ref is from the center.
        *labelPos = ref.Minus(gr.WithMagnitude(swidth/2)).Minus(
                              gu.WithMagnitude(sheight/2));
    }

    if(dogd.drawing) {
        glxWriteTextRefCenter(s, ref, gr, gu, LineCallback, this);
    } else {
        double l = swidth/2 - sheight/2;
        l = max(l, 5/SS.GW.scale);
        Point2d a = SS.GW.ProjectPoint(ref.Minus(gr.WithMagnitude(l)));
        Point2d b = SS.GW.ProjectPoint(ref.Plus (gr.WithMagnitude(l)));
        double d = dogd.mp.DistanceToLine(a, b.Minus(a), true);

        dogd.dmin = min(dogd.dmin, d - 3);
        dogd.refp = ref;
    }
}

void Constraint::DoProjectedPoint(Vector *r) {
    Vector p = r->ProjectInto(workplane);
    glLineStipple(4, 0x5555);
    glEnable(GL_LINE_STIPPLE);
    LineDrawOrGetDistance(p, *r);
    glDisable(GL_LINE_STIPPLE);
    *r = p;
}

void Constraint::DoEqualLenTicks(Vector a, Vector b, Vector gn) {
    Vector m = (a.ScaledBy(1.0/3)).Plus(b.ScaledBy(2.0/3));
    Vector ab = a.Minus(b);
    Vector n = (gn.Cross(ab)).WithMagnitude(10/SS.GW.scale);
    
    LineDrawOrGetDistance(m.Minus(n), m.Plus(n));
}

void Constraint::DoEqualRadiusTicks(hEntity he) {
    Entity *circ = SK.GetEntity(he);

    Vector center = SK.GetEntity(circ->point[0])->PointGetNum();
    double r = circ->CircleGetRadiusNum();
    Quaternion q = circ->Normal()->NormalGetNum();
    Vector u = q.RotationU(), v = q.RotationV();

    double theta;
    if(circ->type == Entity::CIRCLE) {
        theta = PI/2;
    } else if(circ->type == Entity::ARC_OF_CIRCLE) {
        double thetaa, thetab, dtheta;
        circ->ArcGetAngles(&thetaa, &thetab, &dtheta);
        theta = thetaa + dtheta/2;
    } else oops();

    Vector d = u.ScaledBy(cos(theta)).Plus(v.ScaledBy(sin(theta)));
    d = d.ScaledBy(r);
    Vector p = center.Plus(d);
    Vector tick = d.WithMagnitude(10/SS.GW.scale);
    LineDrawOrGetDistance(p.Plus(tick), p.Minus(tick));
}

void Constraint::DoArcForAngle(Vector a0, Vector da, Vector b0, Vector db,
                                   Vector offset, Vector *ref)
{
    Vector gr = SS.GW.projRight.ScaledBy(1/SS.GW.scale);
    Vector gu = SS.GW.projUp.ScaledBy(1/SS.GW.scale);

    if(workplane.v != Entity::FREE_IN_3D.v) {
        a0 = a0.ProjectInto(workplane);
        b0 = b0.ProjectInto(workplane);
        da = da.ProjectVectorInto(workplane);
        db = db.ProjectVectorInto(workplane);
    }

    bool skew;
    Vector pi = Vector::AtIntersectionOfLines(a0, a0.Plus(da), 
                                              b0, b0.Plus(db), &skew);

    if(!skew) {
        *ref = pi.Plus(offset);
        // We draw in a coordinate system centered at the intersection point.
        // One basis vector is da, and the other is normal to da and in
        // the plane that contains our lines (so normal to its normal).
        Vector dna = (da.Cross(db)).Cross(da);
        da = da.WithMagnitude(1); dna = dna.WithMagnitude(1);

        Vector rm = (*ref).Minus(pi);
        double rda = rm.Dot(da), rdna = rm.Dot(dna);
        double r = sqrt(rda*rda + rdna*rdna);
        double c = (da.Dot(db))/(da.Magnitude()*db.Magnitude());
        double thetaf = acos(c);

        Vector m = da.ScaledBy(cos(thetaf/2)).Plus(
                   dna.ScaledBy(sin(thetaf/2)));
        if(m.Dot(rm) < 0) {
            da = da.ScaledBy(-1); dna = dna.ScaledBy(-1);
        }

        Vector prev = da.ScaledBy(r).Plus(pi);
        int i, n = 30;
        for(i = 0; i <= n; i++) {
            double theta = (i*thetaf)/n;
            Vector p = da. ScaledBy(r*cos(theta)).Plus(
                       dna.ScaledBy(r*sin(theta))).Plus(pi);
            LineDrawOrGetDistance(prev, p);
            prev = p;
        }

        double tl = atan2(rm.Dot(gu), rm.Dot(gr));
        double adj = EllipticalInterpolation(
            glxStrWidth(Label())/2, glxStrHeight()/2, tl);
        *ref = (*ref).Plus(rm.WithMagnitude(adj + 3/SS.GW.scale));
    } else {
        // The lines are skew; no wonderful way to illustrate that.
        *ref = a0.Plus(b0);
        *ref = (*ref).ScaledBy(0.5).Plus(disp.offset);
        gu = gu.WithMagnitude(1);
        Vector trans = (*ref).Plus(gu.ScaledBy(-1.5*glxStrHeight()));
        glxWriteTextRefCenter("angle between skew lines", 
            trans, gr, gu, LineCallback, this);
    }
}

void Constraint::DrawOrGetDistance(Vector *labelPos) {
    if(!SS.GW.showConstraints) return;
    Group *g = SK.GetGroup(group);
    // If the group is hidden, then the constraints are hidden and not
    // able to be selected.
    if(!(g->visible)) return;
    // And likewise if the group is not the active group.
    if(g->h.v != SS.GW.activeGroup.v) return;

    // Unit vectors that describe our current view of the scene. One pixel
    // long, not one actual unit.
    Vector gr = SS.GW.projRight.ScaledBy(1/SS.GW.scale);
    Vector gu = SS.GW.projUp.ScaledBy(1/SS.GW.scale);
    Vector gn = (gr.Cross(gu)).WithMagnitude(1/SS.GW.scale);

    glxColor3d(1, 0.1, 1);
    switch(type) {
        case PT_PT_DISTANCE: {
            Vector ap = SK.GetEntity(ptA)->PointGetNum();
            Vector bp = SK.GetEntity(ptB)->PointGetNum();

            if(workplane.v != Entity::FREE_IN_3D.v) {
                DoProjectedPoint(&ap);
                DoProjectedPoint(&bp);
            }

            Vector ref = ((ap.Plus(bp)).ScaledBy(0.5)).Plus(disp.offset);

            Vector ab   = ap.Minus(bp);
            Vector ar   = ap.Minus(ref);
            // Normal to a plan containing the line and the label origin.
            Vector n    = ab.Cross(ar);
            Vector out  = ab.Cross(n).WithMagnitude(1);
            out = out.ScaledBy(-out.Dot(ar));

            LineDrawOrGetDistance(ap, ap.Plus(out));
            LineDrawOrGetDistance(bp, bp.Plus(out));

            DoLabel(ref, labelPos, gr, gu);
            break;
        }

        case PT_FACE_DISTANCE:
        case PT_PLANE_DISTANCE: {
            Vector pt = SK.GetEntity(ptA)->PointGetNum();
            Entity *enta = SK.GetEntity(entityA);
            Vector n, p;
            if(type == PT_PLANE_DISTANCE) {
                n = enta->Normal()->NormalN();
                p = enta->WorkplaneGetOffset();
            } else {
                n = enta->FaceGetNormalNum();
                p = enta->FaceGetPointNum();
            }

            double d = (p.Minus(pt)).Dot(n);

            Vector closest = pt.Plus(n.WithMagnitude(d));
            LineDrawOrGetDistance(pt, closest);

            Vector ref = ((closest.Plus(pt)).ScaledBy(0.5)).Plus(disp.offset);
            DoLabel(ref, labelPos, gr, gu);
            break;
        }

        case PT_LINE_DISTANCE: {
            Vector pt = SK.GetEntity(ptA)->PointGetNum();
            Entity *line = SK.GetEntity(entityA);
            Vector lA = SK.GetEntity(line->point[0])->PointGetNum();
            Vector lB = SK.GetEntity(line->point[1])->PointGetNum();

            if(workplane.v != Entity::FREE_IN_3D.v) {
                lA = lA.ProjectInto(workplane);
                lB = lB.ProjectInto(workplane);
                DoProjectedPoint(&pt);
            }

            // Find the closest point on the line
            Vector closest = pt.ClosestPointOnLine(lA, (lA.Minus(lB)));

            LineDrawOrGetDistance(pt, closest);
            Vector ref = ((closest.Plus(pt)).ScaledBy(0.5)).Plus(disp.offset);
            DoLabel(ref, labelPos, gr, gu);

            if(workplane.v != Entity::FREE_IN_3D.v) {
                // Draw the projection marker from the closest point on the
                // projected line to the projected point on the real line.
                Vector lAB = (lA.Minus(lB));
                double t = (lA.Minus(closest)).DivPivoting(lAB);

                Vector lA = SK.GetEntity(line->point[0])->PointGetNum();
                Vector lB = SK.GetEntity(line->point[1])->PointGetNum();

                Vector c2 = (lA.ScaledBy(1-t)).Plus(lB.ScaledBy(t));
                DoProjectedPoint(&c2);
            }
            break;
        }

        case DIAMETER: {
            Entity *circle = SK.GetEntity(entityA);
            Vector center = SK.GetEntity(circle->point[0])->PointGetNum();
            double r = circle->CircleGetRadiusNum();
            Vector ref = center.Plus(disp.offset);

            double theta = atan2(disp.offset.Dot(gu), disp.offset.Dot(gr));
            double adj = EllipticalInterpolation(
                glxStrWidth(Label())/2, glxStrHeight()/2, theta);

            Vector mark = ref.Minus(center);
            mark = mark.WithMagnitude(mark.Magnitude()-r);
            LineDrawOrGetDistance(ref.Minus(mark.WithMagnitude(adj)),
                                  ref.Minus(mark));

            DoLabel(ref, labelPos, gr, gu);
            break;
        }

        case POINTS_COINCIDENT: {
            if(!dogd.drawing) {
                for(int i = 0; i < 2; i++) {
                    Vector p = SK.GetEntity(i == 0 ? ptA : ptB)-> PointGetNum();
                    Point2d pp = SS.GW.ProjectPoint(p);
                    // The point is selected within a radius of 7, from the
                    // same center; so if the point is visible, then this
                    // constraint cannot be selected. But that's okay.
                    dogd.dmin = min(dogd.dmin, pp.DistanceTo(dogd.mp) - 3);
                    dogd.refp = p;
                }
                break;
            }

            for(int a = 0; a < 2; a++) {
                Vector r = SS.GW.projRight.ScaledBy((a+1)/SS.GW.scale);
                Vector d = SS.GW.projUp.ScaledBy((2-a)/SS.GW.scale);
                for(int i = 0; i < 2; i++) {
                    Vector p = SK.GetEntity(i == 0 ? ptA : ptB)-> PointGetNum();
                    glxColor3d(0.4, 0, 0.4);
                    glBegin(GL_QUADS);
                        glxVertex3v(p.Plus (r).Plus (d));
                        glxVertex3v(p.Plus (r).Minus(d));
                        glxVertex3v(p.Minus(r).Minus(d));
                        glxVertex3v(p.Minus(r).Plus (d));
                    glEnd();
                }

            }
            break;
        }

        case PT_ON_CIRCLE:
        case PT_ON_LINE:
        case PT_ON_FACE:
        case PT_IN_PLANE: {
            double s = 8/SS.GW.scale;
            Vector p = SK.GetEntity(ptA)->PointGetNum();
            Vector r, d;
            if(type == PT_ON_FACE) {
                Vector n = SK.GetEntity(entityA)->FaceGetNormalNum();
                r = n.Normal(0);
                d = n.Normal(1);
            } else if(type == PT_IN_PLANE) {
                EntityBase *n = SK.GetEntity(entityA)->Normal();
                r = n->NormalU();
                d = n->NormalV();
            } else {
                r = gr;
                d = gu;
                s *= (6.0/8); // draw these a little smaller
            }
            r = r.WithMagnitude(s); d = d.WithMagnitude(s);
            LineDrawOrGetDistance(p.Plus (r).Plus (d), p.Plus (r).Minus(d));
            LineDrawOrGetDistance(p.Plus (r).Minus(d), p.Minus(r).Minus(d));
            LineDrawOrGetDistance(p.Minus(r).Minus(d), p.Minus(r).Plus (d));
            LineDrawOrGetDistance(p.Minus(r).Plus (d), p.Plus (r).Plus (d));
            break;
        }

        case SAME_ORIENTATION: {
            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);
                Quaternion q = e->NormalGetNum();
                Vector n = q.RotationN().WithMagnitude(25/SS.GW.scale);
                Vector u = q.RotationU().WithMagnitude(6/SS.GW.scale);
                Vector p = SK.GetEntity(e->point[0])->PointGetNum();
                p = p.Plus(n.WithMagnitude(10/SS.GW.scale));

                LineDrawOrGetDistance(p.Plus(u), p.Minus(u).Plus(n));
                LineDrawOrGetDistance(p.Minus(u), p.Plus(u).Plus(n));
            }
            break;
        }

        case EQUAL_ANGLE: {
            Vector ref;
            Entity *a = SK.GetEntity(entityA);
            Entity *b = SK.GetEntity(entityB);
            Entity *c = SK.GetEntity(entityC);
            Entity *d = SK.GetEntity(entityD);

            Vector a0 = a->VectorGetRefPoint();
            Vector b0 = b->VectorGetRefPoint();
            Vector c0 = c->VectorGetRefPoint();
            Vector d0 = d->VectorGetRefPoint();
            Vector da = a->VectorGetNum();
            Vector db = b->VectorGetNum();
            Vector dc = c->VectorGetNum();
            Vector dd = d->VectorGetNum();

            if(other) da = da.ScaledBy(-1);

            DoArcForAngle(a0, da, b0, db, 
                da.WithMagnitude(40/SS.GW.scale), &ref);
            DoArcForAngle(c0, dc, d0, dd, 
                dc.WithMagnitude(40/SS.GW.scale), &ref);

            break;
        }

        case ANGLE: {
            Entity *a = SK.GetEntity(entityA);
            Entity *b = SK.GetEntity(entityB);
            
            Vector a0 = a->VectorGetRefPoint();
            Vector b0 = b->VectorGetRefPoint();
            Vector da = a->VectorGetNum();
            Vector db = b->VectorGetNum();
            if(other) da = da.ScaledBy(-1);

            Vector ref;
            DoArcForAngle(a0, da, b0, db, disp.offset, &ref);
            DoLabel(ref, labelPos, gr, gu);
            break;
        }

        case PERPENDICULAR: {
            Vector u, v;
            Vector rn, ru;
            if(workplane.v == Entity::FREE_IN_3D.v) {
                rn = gn;
                ru = gu;
            } else {
                EntityBase *normal = SK.GetEntity(workplane)->Normal();
                rn = normal->NormalN();
                ru = normal->NormalV(); // ru meaning r_up, not u/v
            }

            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);

                if(i == 0) {
                    // Calculate orientation of perpendicular sign only
                    // once, so that it's the same both times it's drawn
                    u = e->VectorGetNum();
                    u = u.WithMagnitude(16/SS.GW.scale);
                    v = (rn.Cross(u)).WithMagnitude(16/SS.GW.scale);
                    if(fabs(u.Dot(ru)) < fabs(v.Dot(ru))) {
                        SWAP(Vector, u, v);
                    }
                    if(u.Dot(ru) < 0) u = u.ScaledBy(-1);
                }

                Vector p = e->VectorGetRefPoint();
                Vector s = p.Plus(u).Plus(v);
                LineDrawOrGetDistance(s, s.Plus(v));

                Vector m = s.Plus(v.ScaledBy(0.5));
                LineDrawOrGetDistance(m, m.Plus(u));
            }
            break;
        }

        case CUBIC_LINE_TANGENT:
        case ARC_LINE_TANGENT: {
            Vector textAt, u, v;

            if(type == ARC_LINE_TANGENT) {
                Entity *arc = SK.GetEntity(entityA);
                Entity *norm = SK.GetEntity(arc->normal);
                Vector c = SK.GetEntity(arc->point[0])->PointGetNum();
                Vector p = 
                    SK.GetEntity(arc->point[other ? 2 : 1])->PointGetNum();
                Vector r = p.Minus(c);
                textAt = p.Plus(r.WithMagnitude(14/SS.GW.scale));
                u = norm->NormalU();
                v = norm->NormalV();
            } else {
                Vector n;
                if(workplane.v == Entity::FREE_IN_3D.v) {
                    u = gr;
                    v = gu;
                    n = gn;
                } else {
                    EntityBase *wn = SK.GetEntity(workplane)->Normal();
                    u = wn->NormalU();
                    v = wn->NormalV();
                    n = wn->NormalN();
                }

                Entity *cubic = SK.GetEntity(entityA);
                Vector p =
                    SK.GetEntity(cubic->point[other ? 3 : 0])->PointGetNum();
                Vector dir = SK.GetEntity(entityB)->VectorGetNum();
                Vector out = n.Cross(dir);
                textAt = p.Plus(out.WithMagnitude(14/SS.GW.scale));
            }

            if(dogd.drawing) {
                glxWriteTextRefCenter("T", textAt, u, v, LineCallback, this);
            } else {
                dogd.refp = textAt;
                Point2d ref = SS.GW.ProjectPoint(dogd.refp);
                dogd.dmin = min(dogd.dmin, ref.DistanceTo(dogd.mp)-10);
            }
            break;
        }

        case PARALLEL: {
            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);
                Vector n = e->VectorGetNum();
                n = n.WithMagnitude(25/SS.GW.scale);
                Vector u = (gn.Cross(n)).WithMagnitude(4/SS.GW.scale);
                Vector p = e->VectorGetRefPoint();

                LineDrawOrGetDistance(p.Plus(u), p.Plus(u).Plus(n));
                LineDrawOrGetDistance(p.Minus(u), p.Minus(u).Plus(n));
            }
            break;
        }

        case EQUAL_RADIUS: {
            for(int i = 0; i < 2; i++) {
                DoEqualRadiusTicks(i == 0 ? entityA : entityB);
            }
            break;
        }

        case EQUAL_LINE_ARC_LEN: {
            Entity *line = SK.GetEntity(entityA);
            DoEqualLenTicks(
                SK.GetEntity(line->point[0])->PointGetNum(),
                SK.GetEntity(line->point[1])->PointGetNum(),
                gn);

            DoEqualRadiusTicks(entityB);
            break;
        }

        case LENGTH_RATIO:
        case EQUAL_LENGTH_LINES: {
            Vector a, b;
            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);
                a = SK.GetEntity(e->point[0])->PointGetNum();
                b = SK.GetEntity(e->point[1])->PointGetNum();

                if(workplane.v != Entity::FREE_IN_3D.v) {
                    DoProjectedPoint(&a);
                    DoProjectedPoint(&b);
                }

                DoEqualLenTicks(a, b, gn);
            }
            if(type == LENGTH_RATIO) {
                Vector ref = ((a.Plus(b)).ScaledBy(0.5)).Plus(disp.offset);
                DoLabel(ref, labelPos, gr, gu);
            }
            break;
        }

        case EQ_LEN_PT_LINE_D: {
            Entity *forLen = SK.GetEntity(entityA);
            Vector a = SK.GetEntity(forLen->point[0])->PointGetNum(),
                   b = SK.GetEntity(forLen->point[1])->PointGetNum();
            if(workplane.v != Entity::FREE_IN_3D.v) {
                DoProjectedPoint(&a);
                DoProjectedPoint(&b);
            }
            DoEqualLenTicks(a, b, gn);

            Entity *ln = SK.GetEntity(entityB);
            Vector la = SK.GetEntity(ln->point[0])->PointGetNum(),
                   lb = SK.GetEntity(ln->point[1])->PointGetNum();
            Vector pt = SK.GetEntity(ptA)->PointGetNum();
            if(workplane.v != Entity::FREE_IN_3D.v) {
                DoProjectedPoint(&pt);
                la = la.ProjectInto(workplane);
                lb = lb.ProjectInto(workplane);
            }

            Vector closest = pt.ClosestPointOnLine(la, lb.Minus(la));
            LineDrawOrGetDistance(pt, closest);
            DoEqualLenTicks(pt, closest, gn);
            break;
        }

        case EQ_PT_LN_DISTANCES: {
            for(int i = 0; i < 2; i++) {
                Entity *ln = SK.GetEntity(i == 0 ? entityA : entityB);
                Vector la = SK.GetEntity(ln->point[0])->PointGetNum(),
                       lb = SK.GetEntity(ln->point[1])->PointGetNum();
                Entity *pte = SK.GetEntity(i == 0 ? ptA : ptB);
                Vector pt = pte->PointGetNum();

                if(workplane.v != Entity::FREE_IN_3D.v) {
                    DoProjectedPoint(&pt);
                    la = la.ProjectInto(workplane);
                    lb = lb.ProjectInto(workplane);
                }

                Vector closest = pt.ClosestPointOnLine(la, lb.Minus(la));

                LineDrawOrGetDistance(pt, closest);
                DoEqualLenTicks(pt, closest, gn);
            }
            break;
        }

        {
        Vector n; 
        case SYMMETRIC:
            n = SK.GetEntity(entityA)->Normal()->NormalN(); goto s;
        case SYMMETRIC_HORIZ:
            n = SK.GetEntity(workplane)->Normal()->NormalU(); goto s;
        case SYMMETRIC_VERT:
            n = SK.GetEntity(workplane)->Normal()->NormalV(); goto s;
        case SYMMETRIC_LINE: {
            Entity *ln = SK.GetEntity(entityA);
            Vector la = SK.GetEntity(ln->point[0])->PointGetNum(),
                   lb = SK.GetEntity(ln->point[1])->PointGetNum();
            la = la.ProjectInto(workplane);
            lb = lb.ProjectInto(workplane);
            n = lb.Minus(la);
            Vector nw = SK.GetEntity(workplane)->Normal()->NormalN();
            n = n.RotatedAbout(nw, PI/2);
            goto s;
        }
s:
            Vector a = SK.GetEntity(ptA)->PointGetNum();
            Vector b = SK.GetEntity(ptB)->PointGetNum();

            for(int i = 0; i < 2; i++) {
                Vector tail = (i == 0) ? a : b;
                Vector d = (i == 0) ? b : a;
                d = d.Minus(tail);
                // Project the direction in which the arrow is drawn normal
                // to the symmetry plane; for projected symmetry constraints,
                // they might not be in the same direction, even when the
                // constraint is fully solved.
                d = n.ScaledBy(d.Dot(n));
                d = d.WithMagnitude(20/SS.GW.scale);
                Vector tip = tail.Plus(d);

                LineDrawOrGetDistance(tail, tip);
                d = d.WithMagnitude(9/SS.GW.scale);
                LineDrawOrGetDistance(tip, tip.Minus(d.RotatedAbout(gn,  0.6)));
                LineDrawOrGetDistance(tip, tip.Minus(d.RotatedAbout(gn, -0.6)));
            }
            break;
        }

        case AT_MIDPOINT:
        case HORIZONTAL:
        case VERTICAL:
            if(entityA.v) {
                Vector r, u, n;
                if(workplane.v == Entity::FREE_IN_3D.v) {
                    r = gr; u = gu; n = gn;
                } else {
                    r = SK.GetEntity(workplane)->Normal()->NormalU();
                    u = SK.GetEntity(workplane)->Normal()->NormalV();
                    n = r.Cross(u);
                }
                // For "at midpoint", this branch is always taken.
                Entity *e = SK.GetEntity(entityA);
                Vector a = SK.GetEntity(e->point[0])->PointGetNum();
                Vector b = SK.GetEntity(e->point[1])->PointGetNum();
                Vector m = (a.ScaledBy(0.5)).Plus(b.ScaledBy(0.5));
                Vector offset = (a.Minus(b)).Cross(n);
                offset = offset.WithMagnitude(13/SS.GW.scale);
                // Draw midpoint constraint on other side of line, so that
                // a line can be midpoint and horizontal at same time.
                if(type == AT_MIDPOINT) offset = offset.ScaledBy(-1);

                if(dogd.drawing) {
                    char *s =   (type == HORIZONTAL)  ? "H" : (
                                (type == VERTICAL)    ? "V" : (
                                (type == AT_MIDPOINT) ? "M" : NULL));

                    glxWriteTextRefCenter(s, m.Plus(offset), r, u,
                        LineCallback, this);
                } else {
                    dogd.refp = m.Plus(offset);
                    Point2d ref = SS.GW.ProjectPoint(dogd.refp);
                    dogd.dmin = min(dogd.dmin, ref.DistanceTo(dogd.mp)-10);
                }
            } else {
                Vector a = SK.GetEntity(ptA)->PointGetNum();
                Vector b = SK.GetEntity(ptB)->PointGetNum();

                Entity *w = SK.GetEntity(workplane);
                Vector cu = w->Normal()->NormalU();
                Vector cv = w->Normal()->NormalV();
                Vector cn = w->Normal()->NormalN();

                int i;
                for(i = 0; i < 2; i++) {
                    Vector o = (i == 0) ? a : b;
                    Vector oo = (i == 0) ? a.Minus(b) : b.Minus(a);
                    Vector d = (type == HORIZONTAL) ? cu : cv;
                    if(oo.Dot(d) < 0) d = d.ScaledBy(-1);

                    Vector dp = cn.Cross(d);
                    d = d.WithMagnitude(14/SS.GW.scale);
                    Vector c = o.Minus(d);
                    LineDrawOrGetDistance(o, c);
                    d = d.WithMagnitude(3/SS.GW.scale);
                    dp = dp.WithMagnitude(2/SS.GW.scale);
                    if(dogd.drawing) {
                        glBegin(GL_QUADS);
                            glxVertex3v((c.Plus(d)).Plus(dp));
                            glxVertex3v((c.Minus(d)).Plus(dp));
                            glxVertex3v((c.Minus(d)).Minus(dp));
                            glxVertex3v((c.Plus(d)).Minus(dp));
                        glEnd();
                    } else {
                        Point2d ref = SS.GW.ProjectPoint(c);
                        dogd.dmin = min(dogd.dmin, ref.DistanceTo(dogd.mp)-6);
                    }
                }
            }
            break;

        case COMMENT:
            DoLabel(disp.offset, labelPos, gr, gu);
            break;

        default: oops();
    }
}

void Constraint::Draw(void) {
    dogd.drawing = true;
    dogd.sel = NULL;
    glLineWidth(1);
    DrawOrGetDistance(NULL);
}

double Constraint::GetDistance(Point2d mp) {
    dogd.drawing = false;
    dogd.sel = NULL;
    dogd.mp = mp;
    dogd.dmin = 1e12;

    DrawOrGetDistance(NULL);
    
    return dogd.dmin;
}

Vector Constraint::GetLabelPos(void) {
    dogd.drawing = false;
    dogd.sel = NULL;
    dogd.mp.x = 0; dogd.mp.y = 0;
    dogd.dmin = 1e12;

    Vector p;
    DrawOrGetDistance(&p);
    return p;
}

Vector Constraint::GetReferencePos(void) {
    dogd.drawing = false;
    dogd.sel = NULL;

    dogd.refp = SS.GW.offset.ScaledBy(-1);
    DrawOrGetDistance(NULL);

    return dogd.refp;
}

void Constraint::GetEdges(SEdgeList *sel) {
    dogd.drawing = true;
    dogd.sel = sel;
    DrawOrGetDistance(NULL);
    dogd.sel = NULL;
}

