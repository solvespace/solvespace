#include "solvespace.h"

char *Entity::DescriptionString(void) {
    Request *r = SS.GetRequest(request());
    return r->DescriptionString();
}

void Entity::Get2dCsysBasisVectors(Vector *u, Vector *v) {
    double q[4];
    for(int i = 0; i < 4; i++) {
        q[i] = SS.param.FindById(param(i))->val;
    }
    Quaternion quat = Quaternion::MakeFrom(q[0], q[1], q[2], q[3]);

    *u = quat.RotationU();
    *v = quat.RotationV();
}

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
            Vector p;
            p = SS.point.FindById(point(16))->GetCoords();

            Vector u, v;
            Get2dCsysBasisVectors(&u, &v);

            double s = (min(SS.GW.width, SS.GW.height))*0.4/SS.GW.scale;

            Vector us = u.ScaledBy(s);
            Vector vs = v.ScaledBy(s);

            Vector pp = p.Plus (us).Plus (vs);
            Vector pm = p.Plus (us).Minus(vs);
            Vector mm = p.Minus(us).Minus(vs);
            Vector mp = p.Minus(us).Plus (vs);

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
        case DATUM_POINT:
            // All display is handled by the generated point.
            break;

        case LINE_SEGMENT: {
            Vector a = SS.point.FindById(point(16))->GetCoords();
            Vector b = SS.point.FindById(point(16+3))->GetCoords();
            LineDrawOrGetDistance(a, b);
            break;
        }

        default:
            oops();
    }
}

