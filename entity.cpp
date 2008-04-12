#include "solvespace.h"

void Entity::LineDrawOrGetDistance(Vector a, Vector b) {
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

void Entity::Draw(void) {
    dogd.drawing = true;
    DrawOrGetDistance();
}

double Entity::GetDistance(Point2d mp) {
    dogd.drawing = false;
    dogd.mp = mp;
    dogd.dmin = 1e12;

    DrawOrGetDistance();
    
    return dogd.dmin;
}

void Entity::DrawOrGetDistance(void) {
    switch(type) {
        case CSYS_2D: {
            if(!SS.GW.show2dCsyss) break;

            Vector p;
            p = SS.point.FindById(point(16))->GetCoords();

            double q[4];
            for(int i = 0; i < 4; i++) {
                q[i] = SS.param.FindById(param(i))->val;
            }

            Vector u = Vector::RotationU(q[0], q[1], q[2], q[3]);
            Vector v = Vector::RotationV(q[0], q[1], q[2], q[3]);

            double s = (min(SS.GW.width, SS.GW.height))*0.4;

            Vector pp = p.Plus (u).Plus (v);
            Vector pm = p.Plus (u).Minus(v);
            Vector mm = p.Minus(u).Minus(v);
            Vector mp = p.Minus(u).Plus (v);
            pp = pp.ScaledBy(s);
            pm = pm.ScaledBy(s);
            mm = mm.ScaledBy(s);
            mp = mp.ScaledBy(s);

            LineDrawOrGetDistance(pp, pm);
            LineDrawOrGetDistance(pm, mm);
            LineDrawOrGetDistance(mm, mp);
            LineDrawOrGetDistance(mp, pp);

            if(dogd.drawing) {
                Request *r = SS.request.FindById(this->request());
                glPushMatrix();
                    glxTranslatev(mm);
                    glxOntoCsys(u, v);
                    glxWriteText(r->DescriptionString());
                glPopMatrix();
            }
            break;
        }
        default:
            oops();
    }
}

