#include "solvespace.h"

char *Entity::DescriptionString(void) {
    Request *r = SS.GetRequest(h.request());
    return r->DescriptionString();
}

void Entity::Get2dCsysBasisVectors(Vector *u, Vector *v) {
    double q[4];
    for(int i = 0; i < 4; i++) {
        q[i] = SS.GetParam(param.h[i])->val;
    }
    Quaternion quat = Quaternion::MakeFrom(q[0], q[1], q[2], q[3]);

    *u = quat.RotationU();
    *v = quat.RotationV();
}

bool Entity::IsPoint(void) {
    switch(type) {
        case POINT_IN_3D:
        case POINT_IN_2D:
            return true;
        default:
            return false;
    }
}

bool Entity::IsFromReferences(void) {
    hRequest hr = h.request();
    if(hr.v == Request::HREQUEST_REFERENCE_XY.v) return true;
    if(hr.v == Request::HREQUEST_REFERENCE_YZ.v) return true;
    if(hr.v == Request::HREQUEST_REFERENCE_ZX.v) return true;
    return false;
}

void Entity::ForcePointTo(Vector p) {
    switch(type) {
        case POINT_IN_3D:
            SS.GetParam(param.h[0])->ForceTo(p.x);
            SS.GetParam(param.h[1])->ForceTo(p.y);
            SS.GetParam(param.h[2])->ForceTo(p.z);
            break;

        case POINT_IN_2D: {
            Entity *c = SS.GetEntity(csys);
            Vector u, v;
            c->Get2dCsysBasisVectors(&u, &v);
            SS.GetParam(param.h[0])->ForceTo(p.Dot(u));
            SS.GetParam(param.h[1])->ForceTo(p.Dot(v));
            break;
        }
        default: oops();
    }
}

Vector Entity::GetPointCoords(void) {
    Vector p;
    switch(type) {
        case POINT_IN_3D:
            p.x = SS.GetParam(param.h[0])->val;
            p.y = SS.GetParam(param.h[1])->val;
            p.z = SS.GetParam(param.h[2])->val;
            break;

        case POINT_IN_2D: {
            Entity *c = SS.GetEntity(csys);
            Vector u, v;
            c->Get2dCsysBasisVectors(&u, &v);
            p =        u.ScaledBy(SS.GetParam(param.h[0])->val);
            p = p.Plus(v.ScaledBy(SS.GetParam(param.h[1])->val));
            break;
        }
        default: oops();
    }
    return p;
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
    glxColor(1, 1, 1);

    switch(type) {
        case POINT_IN_3D:
        case POINT_IN_2D: {
            Entity *isfor = SS.GetEntity(h.request().entity(0));
            if(!SS.GW.show2dCsyss && isfor->type == Entity::CSYS_2D) break;

            Vector v = GetPointCoords();

            if(dogd.drawing) {
                double s = 4;
                Vector r = SS.GW.projRight.ScaledBy(s/SS.GW.scale);
                Vector d = SS.GW.projUp.ScaledBy(s/SS.GW.scale);

                glxColor(0, 0.8, 0);
                glBegin(GL_QUADS);
                    glxVertex3v(v.Plus (r).Plus (d));
                    glxVertex3v(v.Plus (r).Minus(d));
                    glxVertex3v(v.Minus(r).Minus(d));
                    glxVertex3v(v.Minus(r).Plus (d));
                glEnd();
            } else {
                Point2d pp = SS.GW.ProjectPoint(v);
                dogd.dmin = pp.DistanceTo(dogd.mp) - 8;
            }
            break;
        }

        case CSYS_2D: {
            if(!SS.GW.show2dCsyss) break;

            Vector p;
            p = SS.GetEntity(assoc[0])->GetPointCoords();

            Vector u, v;
            Get2dCsysBasisVectors(&u, &v);

            double s = (min(SS.GW.width, SS.GW.height))*0.4/SS.GW.scale;

            Vector us = u.ScaledBy(s);
            Vector vs = v.ScaledBy(s);

            Vector pp = p.Plus (us).Plus (vs);
            Vector pm = p.Plus (us).Minus(vs);
            Vector mm = p.Minus(us).Minus(vs);
            Vector mp = p.Minus(us).Plus (vs);

            glxColor(0, 0.4, 0.4);
            LineDrawOrGetDistance(pp, pm);
            LineDrawOrGetDistance(pm, mm);
            LineDrawOrGetDistance(mm, mp);
            LineDrawOrGetDistance(mp, pp);

            if(dogd.drawing) {
                glPushMatrix();
                    glxTranslatev(mm);
                    glxOntoCsys(u, v);
                    glxWriteText(DescriptionString());
                glPopMatrix();
            }
            break;
        }

        case LINE_SEGMENT: {
            Vector a = SS.GetEntity(assoc[0])->GetPointCoords();
            Vector b = SS.GetEntity(assoc[1])->GetPointCoords();
            LineDrawOrGetDistance(a, b);
            break;
        }

        default:
            oops();
    }
}

