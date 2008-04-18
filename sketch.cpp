#include "solvespace.h"

const hEntity  Entity::NO_CSYS = { 0 };

const hGroup Group::HGROUP_REFERENCES = { 1 };
const hRequest Request::HREQUEST_REFERENCE_XY = { 1 };
const hRequest Request::HREQUEST_REFERENCE_YZ = { 2 };
const hRequest Request::HREQUEST_REFERENCE_ZX = { 3 };

char *Group::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "g%04x-%s", h.v, name.str);
    } else {
        sprintf(ret, "g%04x-(unnamed)", h.v);
    }
    return ret;
}

void Request::AddParam(IdList<Param,hParam> *param, Entity *e, int index) {
    Param pa;
    memset(&pa, 0, sizeof(pa));
    pa.h = e->param(index);
    param->Add(&pa);
}

void Request::Generate(IdList<Entity,hEntity> *entity,
                       IdList<Point,hPoint> *point,
                       IdList<Param,hParam> *param)
{
    int points = 0;
    int params = 0;
    int et = 0;
    int i;

    Group *g = SS.group.FindById(group);

    Entity e;
    memset(&e, 0, sizeof(e));
    e.h = this->entity(0);

    bool shown = true;
    switch(type) {
        case Request::CSYS_2D:
            et = Entity::CSYS_2D;          points = 1; params = 4; goto c;

        case Request::DATUM_POINT:
            et = Entity::DATUM_POINT;      points = 1; params = 0; goto c;

        case Request::LINE_SEGMENT:
            et = Entity::LINE_SEGMENT;     points = 2; params = 0; goto c;
c: {
            // Common routines, for all the requests that generate a single
            // entity that's defined by a simple combination of pts and params.
            for(i = 0; i < params; i++) {
                AddParam(param, &e, i);
            }
            for(i = 0; i < points; i++) {
                Point pt;
                memset(&pt, 0, sizeof(pt));
                pt.csys = csys;
                pt.h = e.point(16 + 3*i);
                if(csys.v == Entity::NO_CSYS.v) {
                    pt.type = Point::IN_FREE_SPACE;
                    // params for x y z
                    AddParam(param, &e, 16 + 3*i + 0);
                    AddParam(param, &e, 16 + 3*i + 1);
                    AddParam(param, &e, 16 + 3*i + 2);
                } else {
                    pt.type = Point::IN_2D_CSYS;
                    // params for u v
                    AddParam(param, &e, 16 + 3*i + 0);
                    AddParam(param, &e, 16 + 3*i + 1);
                }
                point->Add(&pt);
            }
    
            e.type = et;
            entity->Add(&e);
            break;
        }

        default:
            oops();
    }
}

char *Request::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "r%03x-%s", h.v, name.str);
    } else {
        sprintf(ret, "r%03x-(unnamed)", h.v);
    }
    return ret;
}

void Param::ForceTo(double v) {
    val = v;
    known = true;
}

void Point::ForceTo(Vector p) {
    switch(type) {
        case IN_FREE_SPACE:
            SS.GetParam(param(0))->ForceTo(p.x);
            SS.GetParam(param(1))->ForceTo(p.y);
            SS.GetParam(param(2))->ForceTo(p.z);
            break;

        case IN_2D_CSYS: {
            Entity *c = SS.GetEntity(csys);
            Vector u, v;
            c->Get2dCsysBasisVectors(&u, &v);
            SS.GetParam(param(0))->ForceTo(p.Dot(u));
            SS.GetParam(param(1))->ForceTo(p.Dot(v));
            break;
        }

        default:
            oops();
    }
}

Vector Point::GetCoords(void) {
    Vector p;
    switch(type) {
        case IN_FREE_SPACE:
            p.x = SS.GetParam(param(0))->val;
            p.y = SS.GetParam(param(1))->val;
            p.z = SS.GetParam(param(2))->val;
            break;

        case IN_2D_CSYS: {
            Entity *c = SS.GetEntity(csys);
            Vector u, v;
            c->Get2dCsysBasisVectors(&u, &v);
            p =        u.ScaledBy(SS.GetParam(param(0))->val);
            p = p.Plus(v.ScaledBy(SS.GetParam(param(1))->val));
            break;
        }

        default:
            oops();
    }

    return p;
}

void Point::Draw(void) {
    Vector v = GetCoords();

    double s = 4;
    Vector r = SS.GW.projRight.ScaledBy(4/SS.GW.scale);
    Vector d = SS.GW.projUp.ScaledBy(4/SS.GW.scale);

    glBegin(GL_QUADS);
        glxVertex3v(v.Plus (r).Plus (d));
        glxVertex3v(v.Plus (r).Minus(d));
        glxVertex3v(v.Minus(r).Minus(d));
        glxVertex3v(v.Minus(r).Plus (d));
    glEnd();
}

double Point::GetDistance(Point2d mp) {
    Vector v = GetCoords();
    Point2d pp = SS.GW.ProjectPoint(v);

    return pp.DistanceTo(mp);
}

