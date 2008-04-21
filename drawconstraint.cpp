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

void Constraint::DrawOrGetDistance(void) {
    if(!SS.GW.showConstraints) return;

    // Unit vectors that describe our current view of the scene.
    Vector gr = SS.GW.projRight;
    Vector gu = SS.GW.projUp;
    Vector gn = gr.Cross(gu);

    glxColor(1, 0.3, 1);
    switch(type) {
        case PT_PT_DISTANCE: {
            Vector ap = SS.GetEntity(ptA)->PointGetCoords();
            Vector bp = SS.GetEntity(ptB)->PointGetCoords();

            Vector ref = ((ap.Plus(bp)).ScaledBy(0.5)).Plus(disp.offset);

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
            // It's impossible to select this constraint on the drawing;
            // have to do it from the text window.
            if(!dogd.drawing) break;
            double s = 2;
            Vector r = SS.GW.projRight.ScaledBy(s/SS.GW.scale);
            Vector d = SS.GW.projUp.ScaledBy(s/SS.GW.scale);
            for(int i = 0; i < 2; i++) {
                Vector p = SS.GetEntity(i == 0 ? ptA : ptB)->PointGetCoords();
                glxColor(0.4, 0, 0.4);
                glBegin(GL_QUADS);
                    glxVertex3v(p.Plus (r).Plus (d));
                    glxVertex3v(p.Plus (r).Minus(d));
                    glxVertex3v(p.Minus(r).Minus(d));
                    glxVertex3v(p.Minus(r).Plus (d));
                glEnd();
            }
            break;
        }

        case PT_IN_PLANE: {
            double s = 6;
            Vector r = SS.GW.projRight.ScaledBy(s/SS.GW.scale);
            Vector d = SS.GW.projUp.ScaledBy(s/SS.GW.scale);
            Vector p = SS.GetEntity(ptA)->PointGetCoords();
            LineDrawOrGetDistance(p.Plus (r).Plus (d), p.Plus (r).Minus(d));
            LineDrawOrGetDistance(p.Plus (r).Minus(d), p.Minus(r).Minus(d));
            LineDrawOrGetDistance(p.Minus(r).Minus(d), p.Minus(r).Plus (d));
            LineDrawOrGetDistance(p.Minus(r).Plus (d), p.Plus (r).Plus (d));
            break;
        }

        default: oops();
    }
}

void Constraint::Draw(void) {
    dogd.drawing = true;
    DrawOrGetDistance();
}

double Constraint::GetDistance(Point2d mp) {
    dogd.drawing = false;
    dogd.mp = mp;
    dogd.dmin = 1e12;

    DrawOrGetDistance();
    
    return dogd.dmin;
}

