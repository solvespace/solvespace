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
        glBegin(GL_LINE_STRIP);
            glxVertex3v(a);
            glxVertex3v(b);
        glEnd();
    } else {
        Point2d ap = SS.GW.ProjectPoint(a);
        Point2d bp = SS.GW.ProjectPoint(b);

        double d = dogd.mp.DistanceToLine(ap, bp.Minus(ap), true);
        dogd.dmin = min(dogd.dmin, d);
    }
    dogd.refp = (a.Plus(b)).ScaledBy(0.5);
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
        glPushMatrix();
            glxTranslatev(ref);
            glxOntoWorkplane(gr, gu);
            glxWriteTextRefCenter(s);
        glPopMatrix();
    } else {
        double l = swidth/2 - sheight/2;
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

void Constraint::DrawOrGetDistance(Vector *labelPos) {
    if(!SS.GW.showConstraints) return;
    Group *g = SS.GetGroup(group);
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
            Vector ap = SS.GetEntity(ptA)->PointGetNum();
            Vector bp = SS.GetEntity(ptB)->PointGetNum();

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
            Vector pt = SS.GetEntity(ptA)->PointGetNum();
            Entity *enta = SS.GetEntity(entityA);
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
            Vector pt = SS.GetEntity(ptA)->PointGetNum();
            Entity *line = SS.GetEntity(entityA);
            Vector lA = SS.GetEntity(line->point[0])->PointGetNum();
            Vector lB = SS.GetEntity(line->point[1])->PointGetNum();

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

                Vector lA = SS.GetEntity(line->point[0])->PointGetNum();
                Vector lB = SS.GetEntity(line->point[1])->PointGetNum();

                Vector c2 = (lA.ScaledBy(1-t)).Plus(lB.ScaledBy(t));
                DoProjectedPoint(&c2);
            }
            break;
        }

        case DIAMETER: {
            Entity *circle = SS.GetEntity(entityA);
            Vector center = SS.GetEntity(circle->point[0])->PointGetNum();
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
                    Vector p = SS.GetEntity(i == 0 ? ptA : ptB)-> PointGetNum();
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
                    Vector p = SS.GetEntity(i == 0 ? ptA : ptB)-> PointGetNum();
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
            Vector p = SS.GetEntity(ptA)->PointGetNum();
            Vector r, d;
            if(type == PT_ON_FACE) {
                Vector n = SS.GetEntity(entityA)->FaceGetNormalNum();
                r = n.Normal(0);
                d = n.Normal(1);
            } else if(type == PT_IN_PLANE) {
                Entity *n = SS.GetEntity(entityA)->Normal();
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
                Entity *e = SS.GetEntity(i == 0 ? entityA : entityB);
                Quaternion q = e->NormalGetNum();
                Vector n = q.RotationN().WithMagnitude(25/SS.GW.scale);
                Vector u = q.RotationU().WithMagnitude(6/SS.GW.scale);
                Vector p = SS.GetEntity(e->point[0])->PointGetNum();
                p = p.Plus(n.WithMagnitude(10/SS.GW.scale));

                LineDrawOrGetDistance(p.Plus(u), p.Minus(u).Plus(n));
                LineDrawOrGetDistance(p.Minus(u), p.Plus(u).Plus(n));
            }
            break;
        }

        case ANGLE: {
            Entity *a = SS.GetEntity(entityA);
            Entity *b = SS.GetEntity(entityB);
            
            Vector a0 = a->VectorGetRefPoint();
            Vector b0 = b->VectorGetRefPoint();
            Vector da = a->VectorGetNum();
            Vector db = b->VectorGetNum();
            if(otherAngle) da = da.ScaledBy(-1);

            if(workplane.v != Entity::FREE_IN_3D.v) {
                a0 = a0.ProjectInto(workplane);
                b0 = b0.ProjectInto(workplane);
                da = da.ProjectVectorInto(workplane);
                db = db.ProjectVectorInto(workplane);
            }

            // Make an orthogonal coordinate system from those directions
            Vector dn = da.Cross(db); // normal to both
            Vector dna = dn.Cross(da); // normal to da
            Vector dnb = dn.Cross(db); // normal to db
            // At the intersection of the lines
            //    a0 + pa*da = b0 + pb*db (where pa, pb are scalar params)
            // So dot this equation against dna and dnb to get two equations
            // to solve for da and db
            double pb =  ((a0.Minus(b0)).Dot(dna))/(db.Dot(dna));
            double pa = -((a0.Minus(b0)).Dot(dnb))/(da.Dot(dnb));

            Vector pi = a0.Plus(da.ScaledBy(pa));
            Vector ref;
            if(pi.Equals(b0.Plus(db.ScaledBy(pb)))) {
                ref = pi.Plus(disp.offset);
                // We draw in a coordinate system centered at pi, with
                // basis vectors da and dna.
                da = da.WithMagnitude(1); dna = dna.WithMagnitude(1);
                Vector rm = ref.Minus(pi);
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
                ref = ref.Plus(rm.WithMagnitude(adj + 3/SS.GW.scale));
            } else {
                // The lines are skew; no wonderful way to illustrate that.
                ref = a->VectorGetRefPoint().Plus(b->VectorGetRefPoint());
                ref = ref.ScaledBy(0.5).Plus(disp.offset);
                glPushMatrix();
                    gu = gu.WithMagnitude(1);
                    glxTranslatev(ref.Plus(gu.ScaledBy(-1.5*glxStrHeight())));
                    glxOntoWorkplane(gr, gu);
                    glxWriteTextRefCenter("angle between skew lines");
                glPopMatrix();
            }
            
            DoLabel(ref, labelPos, gr, gu);
            break;
        }

        case PARALLEL: {
            for(int i = 0; i < 2; i++) {
                Entity *e = SS.GetEntity(i == 0 ? entityA : entityB);
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
                Entity *circ = SS.GetEntity(i == 0 ? entityA : entityB);
                Vector center = SS.GetEntity(circ->point[0])->PointGetNum();
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
            break;
        }

        case LENGTH_RATIO:
        case EQUAL_LENGTH_LINES: {
            Vector a, b;
            for(int i = 0; i < 2; i++) {
                Entity *e = SS.GetEntity(i == 0 ? entityA : entityB);
                a = SS.GetEntity(e->point[0])->PointGetNum();
                b = SS.GetEntity(e->point[1])->PointGetNum();

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
            Entity *forLen = SS.GetEntity(entityA);
            Vector a = SS.GetEntity(forLen->point[0])->PointGetNum(),
                   b = SS.GetEntity(forLen->point[1])->PointGetNum();
            if(workplane.v != Entity::FREE_IN_3D.v) {
                DoProjectedPoint(&a);
                DoProjectedPoint(&b);
            }
            DoEqualLenTicks(a, b, gn);

            Entity *ln = SS.GetEntity(entityB);
            Vector la = SS.GetEntity(ln->point[0])->PointGetNum(),
                   lb = SS.GetEntity(ln->point[1])->PointGetNum();
            Vector pt = SS.GetEntity(ptA)->PointGetNum();
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
                Entity *ln = SS.GetEntity(i == 0 ? entityA : entityB);
                Vector la = SS.GetEntity(ln->point[0])->PointGetNum(),
                       lb = SS.GetEntity(ln->point[1])->PointGetNum();
                Entity *pte = SS.GetEntity(i == 0 ? ptA : ptB);
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
            n = SS.GetEntity(entityA)->Normal()->NormalN(); goto s;
        case SYMMETRIC_HORIZ:
            n = SS.GetEntity(workplane)->Normal()->NormalU(); goto s;
        case SYMMETRIC_VERT:
            n = SS.GetEntity(workplane)->Normal()->NormalV(); goto s;
s:
            Vector a = SS.GetEntity(ptA)->PointGetNum();
            Vector b = SS.GetEntity(ptB)->PointGetNum();
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
                    r = SS.GetEntity(workplane)->Normal()->NormalU();
                    u = SS.GetEntity(workplane)->Normal()->NormalV();
                    n = r.Cross(u);
                }
                // For "at midpoint", this branch is always taken.
                Entity *e = SS.GetEntity(entityA);
                Vector a = SS.GetEntity(e->point[0])->PointGetNum();
                Vector b = SS.GetEntity(e->point[1])->PointGetNum();
                Vector m = (a.ScaledBy(0.5)).Plus(b.ScaledBy(0.5));
                Vector offset = (a.Minus(b)).Cross(n);
                offset = offset.WithMagnitude(13/SS.GW.scale);
                // Draw midpoint constraint on other side of line, so that
                // a line can be midpoint and horizontal at same time.
                if(type == AT_MIDPOINT) offset = offset.ScaledBy(-1);

                if(dogd.drawing) {
                    glPushMatrix();
                        glxTranslatev(m.Plus(offset));
                        glxOntoWorkplane(r, u);
                        glxWriteTextRefCenter(
                            (type == HORIZONTAL)  ? "H" : (
                            (type == VERTICAL)    ? "V" : (
                            (type == AT_MIDPOINT) ? "M" : NULL)));
                    glPopMatrix();
                } else {
                    dogd.refp = m.Plus(offset);
                    Point2d ref = SS.GW.ProjectPoint(dogd.refp);
                    dogd.dmin = min(dogd.dmin, ref.DistanceTo(dogd.mp)-10);
                }
            } else {
                Vector a = SS.GetEntity(ptA)->PointGetNum();
                Vector b = SS.GetEntity(ptB)->PointGetNum();

                Entity *w = SS.GetEntity(workplane);
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
    glLineWidth(1);
    DrawOrGetDistance(NULL);
}

double Constraint::GetDistance(Point2d mp) {
    dogd.drawing = false;
    dogd.mp = mp;
    dogd.dmin = 1e12;

    DrawOrGetDistance(NULL);
    
    return dogd.dmin;
}

Vector Constraint::GetLabelPos(void) {
    dogd.drawing = false;
    dogd.mp.x = 0; dogd.mp.y = 0;
    dogd.dmin = 1e12;

    Vector p;
    DrawOrGetDistance(&p);
    return p;
}

Vector Constraint::GetReferencePos(void) {
    dogd.drawing = false;

    dogd.refp = SS.GW.offset.ScaledBy(-1);
    DrawOrGetDistance(NULL);

    return dogd.refp;
}

