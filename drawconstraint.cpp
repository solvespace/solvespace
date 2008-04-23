#include "solvespace.h"

bool Constraint::HasLabel(void) {
    switch(type) {
        case PT_PT_DISTANCE:
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

void Constraint::DrawOrGetDistance(Vector *labelPos) {
    if(!SS.GW.showConstraints) return;

    // Unit vectors that describe our current view of the scene.
    Vector gr = SS.GW.projRight;
    Vector gu = SS.GW.projUp;
    Vector gn = gr.Cross(gu);

    glxColor(1, 0.2, 1);
    switch(type) {
        case PT_PT_DISTANCE: {
            Vector ap = SS.GetEntity(ptA)->PointGetCoords();
            Vector bp = SS.GetEntity(ptB)->PointGetCoords();

            Vector ref = ((ap.Plus(bp)).ScaledBy(0.5)).Plus(disp.offset);
            if(labelPos) *labelPos = ref;

            Vector ab   = ap.Minus(bp);
            Vector ar   = ap.Minus(ref);
            // Normal to a plan containing the line and the label origin.
            Vector n    = ab.Cross(ar);
            Vector out  = ab.Cross(n).WithMagnitude(1);
            out = out.ScaledBy(-out.Dot(ar));

            LineDrawOrGetDistance(ap, ap.Plus(out));
            LineDrawOrGetDistance(bp, bp.Plus(out));

            if(dogd.drawing) {
                glPushMatrix();
                    glxTranslatev(ref);
                    glxOntoCsys(gr, gu);
                    glxWriteText(exprA->Print());
                glPopMatrix();
            } else {
                Point2d o = SS.GW.ProjectPoint(ref);
                dogd.dmin = min(dogd.dmin, o.DistanceTo(dogd.mp) - 10);
            }

            break;
        }

        case POINTS_COINCIDENT: {
            if(!dogd.drawing) {
                for(int i = 0; i < 2; i++) {
                    Vector p = SS.GetEntity(i == 0 ? ptA : ptB)->
                                                            PointGetCoords();
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
                    Vector p = SS.GetEntity(i == 0 ? ptA : ptB)->
                                                            PointGetCoords();
                    glxColor(0.4, 0, 0.4);
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

        case PT_IN_PLANE: {
            double s = 5;
            Vector r = SS.GW.projRight.ScaledBy(s/SS.GW.scale);
            Vector d = SS.GW.projUp.ScaledBy(s/SS.GW.scale);
            Vector p = SS.GetEntity(ptA)->PointGetCoords();
            LineDrawOrGetDistance(p.Plus (r).Plus (d), p.Plus (r).Minus(d));
            LineDrawOrGetDistance(p.Plus (r).Minus(d), p.Minus(r).Minus(d));
            LineDrawOrGetDistance(p.Minus(r).Minus(d), p.Minus(r).Plus (d));
            LineDrawOrGetDistance(p.Minus(r).Plus (d), p.Plus (r).Plus (d));
            break;
        }

        case EQUAL_LENGTH_LINES: {
            for(int i = 0; i < 2; i++) {
                Entity *e = SS.GetEntity(i == 0 ? entityA : entityB);
                Vector a = SS.GetEntity(e->assoc[0])->PointGetCoords();
                Vector b = SS.GetEntity(e->assoc[1])->PointGetCoords();
                Vector m = (a.ScaledBy(1.0/3)).Plus(b.ScaledBy(2.0/3));
                Vector ab = a.Minus(b);
                Vector n = (gn.Cross(ab)).WithMagnitude(10/SS.GW.scale);
                
                LineDrawOrGetDistance(m.Minus(n), m.Plus(n));
            }
            break;
        }

        case HORIZONTAL:
        case VERTICAL:
            if(entityA.v) {
                Entity *e = SS.GetEntity(entityA);
                Vector a = SS.GetEntity(e->assoc[0])->PointGetCoords();
                Vector b = SS.GetEntity(e->assoc[1])->PointGetCoords();
                Vector m = (a.ScaledBy(0.5)).Plus(b.ScaledBy(0.5));

                if(dogd.drawing) {
                    glPushMatrix();
                        glxTranslatev(m);
                        glxOntoCsys(gr, gu);
                        glxWriteText(type == HORIZONTAL ? "H" : "V");
                    glPopMatrix();
                } else {
                    Point2d ref = SS.GW.ProjectPoint(m);
                    dogd.dmin = min(dogd.dmin, ref.DistanceTo(dogd.mp)-10);
                }
            } else {
                Vector a = SS.GetEntity(ptA)->PointGetCoords();
                Vector b = SS.GetEntity(ptB)->PointGetCoords();

                Entity *csy = SS.GetEntity(SS.GetEntity(ptA)->csys);
                Vector cu, cv, cn;
                csy->Csys2dGetBasisVectors(&cu, &cv);
                cn = csy->Csys2dGetNormalVector();

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

