#include "solvespace.h"

bool Constraint::HasLabel(void) {
    switch(type) {
        case PT_PT_DISTANCE:
            return true;

        default:
            return false;
    }
}

void Constraint::DrawOrGetDistance(void) {
    // Unit vectors that describe our current view of the scene.
    Vector gr = SS.GW.projRight;
    Vector gu = SS.GW.projUp;
    Vector gn = gr.Cross(gu);

    glxColor(1, 0.3, 1);
    switch(type) {
        case PT_PT_DISTANCE: {
            Vector ap = SS.GetEntity(ptA)->GetPointCoords();
            Vector bp = SS.GetEntity(ptB)->GetPointCoords();

            Vector ref = ((ap.Plus(bp)).ScaledBy(0.5)).Plus(disp.offset);

            if(dogd.drawing) {
                Vector ab   = ap.Minus(bp);
                Vector ar   = ap.Minus(ref);
                // Normal to a plan containing the line and the label origin.
                Vector n    = ab.Cross(ar);
                Vector out  = ab.Cross(n).WithMagnitude(1);
                out = out.ScaledBy(-out.Dot(ar));

                glBegin(GL_LINES);
                    glxVertex3v(ap);
                    glxVertex3v(ap.Plus(out));
                    glxVertex3v(bp);
                    glxVertex3v(bp.Plus(out));
                glEnd();

                glPushMatrix();
                    glxTranslatev(ref);
                    glxOntoCsys(gr, gu);
                    glxWriteText(exprA->Print());
                glPopMatrix();
            } else {
                Point2d o = SS.GW.ProjectPoint(ref);
                dogd.dmin = o.DistanceTo(dogd.mp) - 10;
            }

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

