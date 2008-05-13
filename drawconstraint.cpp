#include "solvespace.h"

bool Constraint::HasLabel(void) {
    switch(type) {
        case PT_LINE_DISTANCE:
        case PT_PLANE_DISTANCE:
        case PT_PT_DISTANCE:
        case DIAMETER:
        case LENGTH_RATIO:
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
}

double Constraint::EllipticalInterpolation(double rx, double ry, double theta) {
    double ex = rx*cos(theta);
    double ey = ry*sin(theta);
    double v = sqrt(ex*ex + ey*ey);

    return v;
}

void Constraint::DoLabel(Vector ref, Vector *labelPos, Vector gr, Vector gu) {
    char *s = exprA->Print();
    if(labelPos) {
        // labelPos is from the top left corner (for the text box used to
        // edit things), but ref is from the center.
        *labelPos = ref.Minus(gr.WithMagnitude(glxStrWidth(s)/2)).Minus(
                             gu.WithMagnitude(glxStrHeight()/2));
    }

    if(dogd.drawing) {
        glPushMatrix();
            glxTranslatev(ref);
            glxOntoWorkplane(gr, gu);
            glxWriteTextRefCenter(s);
        glPopMatrix();
    } else {
        Point2d o = SS.GW.ProjectPoint(ref);
        dogd.dmin = min(dogd.dmin, o.DistanceTo(dogd.mp) - 10);
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

    glxColor3d(1, 0.2, 1);
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

        case PT_PLANE_DISTANCE: {
            Vector pt = SS.GetEntity(ptA)->PointGetNum();
            Entity *plane = SS.GetEntity(entityA);
            Vector n = plane->Normal()->NormalN();
            Vector p = plane->WorkplaneGetOffset();
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

            Vector lAB = (lA.Minus(lB)).WithMagnitude(1);
            Vector closest;
            // lA, lB, and pt define a plane; the min distance is in
            // that plane, so calculate its normal
            Vector pn = (pt.Minus(lA)).Cross(lAB);
            // The minimum distance line is in that plane, perpendicular
            // to the line
            Vector n = pn.Cross(lAB);

            // Calculate the actual distance
            double d = (lAB.Cross(lA.Minus(pt))).Magnitude();
            closest = pt.Plus(n.WithMagnitude(d));
            
            LineDrawOrGetDistance(pt, closest);
            Vector ref = ((closest.Plus(pt)).ScaledBy(0.5)).Plus(disp.offset);
            DoLabel(ref, labelPos, gr, gu);

            if(workplane.v != Entity::FREE_IN_3D.v) {
                // Draw the projection marker from the closest point on the
                // projected line to the projected point on the real line.
                lAB = (lA.Minus(lB));
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
                glxStrWidth(exprA->Print())/2, glxStrHeight()/2, theta);

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
        case PT_IN_PLANE: {
            double s = 7/SS.GW.scale;
            Vector p = SS.GetEntity(ptA)->PointGetNum();
            Vector r = gr.WithMagnitude(s);
            Vector d = gu.WithMagnitude(s);
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
                Vector m = (a.ScaledBy(1.0/3)).Plus(b.ScaledBy(2.0/3));
                Vector ab = a.Minus(b);
                Vector n = (gn.Cross(ab)).WithMagnitude(10/SS.GW.scale);
                
                LineDrawOrGetDistance(m.Minus(n), m.Plus(n));
            }
            if(type == LENGTH_RATIO) {
                Vector ref = ((a.Plus(b)).ScaledBy(0.5)).Plus(disp.offset);
                DoLabel(ref, labelPos, gr, gu);
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
                    Point2d ref = SS.GW.ProjectPoint(m.Plus(offset));
                    dogd.dmin = min(dogd.dmin, ref.DistanceTo(dogd.mp)-10);
                }
            } else {
                Vector a = SS.GetEntity(ptA)->PointGetNum();
                Vector b = SS.GetEntity(ptB)->PointGetNum();

                Entity *w = SS.GetEntity(SS.GetEntity(ptA)->workplane);
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

        default: oops();
    }
}

void Constraint::Draw(void) {
    dogd.drawing = true;
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

