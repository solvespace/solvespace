#include "solvespace.h"

const hEntity  Entity::NO_CSYS = { 0 };

const hGroup Group::HGROUP_REFERENCES = { 1 };
const hRequest Request::HREQUEST_REFERENCE_XY = { 1 };
const hRequest Request::HREQUEST_REFERENCE_YZ = { 2 };
const hRequest Request::HREQUEST_REFERENCE_ZX = { 3 };

char *Group::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "g%03x-%s", h.v, name.str);
    } else {
        sprintf(ret, "g%03x-(unnamed)", h.v);
    }
    return ret;
}

void Request::AddParam(Entity *e, int index) {
    Param pa;
    memset(&pa, 0, sizeof(pa));
    pa.h = e->param(index);
    SS.param.Add(&pa);
}

void Request::Generate(void) {
    int points = 0;
    int params = 0;
    int type = 0;
    int i;

    Group *g = SS.group.FindById(group);

    Entity e;
    memset(&e, 0, sizeof(e));
    e.h = this->entity(0);

    switch(type) {
        case Request::CSYS_2D:
            type = Entity::CSYS_2D;          points = 1; params = 4; goto c;
        case Request::LINE_SEGMENT:
            type = Entity::LINE_SEGMENT;     points = 2; params = 0; goto c;
c: {
            // Common routines, for all the requests that generate a single
            // entity that's defined by a simple combination of pts and params.
            for(i = 0; i < params; i++) {
                AddParam(&e, i);
            }
            for(i = 0; i < points; i++) {
                Point pt;
                memset(&pt, 0, sizeof(pt));
                pt.h = e.point(16 + 3*i);
                if(g->csys.v == Entity::NO_CSYS.v) {
                    pt.type = Point::IN_FREE_SPACE;
                    // params for x y z
                    AddParam(&e, 16 + 3*i + 0);
                    AddParam(&e, 16 + 3*i + 1);
                    AddParam(&e, 16 + 3*i + 2);
                } else {
                    pt.type = Point::IN_2D_CSYS;
                    pt.csys = g->csys;
                    // params for u v
                    AddParam(&e, 16 + 3*i + 0);
                    AddParam(&e, 16 + 3*i + 1);
                }
                SS.point.Add(&pt);
            }
    
            e.type = type;
            SS.entity.Add(&e);
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

void Point::ForceTo(Vector v) {
    switch(type) {
        case IN_FREE_SPACE:
            SS.param.FindById(param(0))->ForceTo(v.x);
            SS.param.FindById(param(1))->ForceTo(v.y);
            SS.param.FindById(param(2))->ForceTo(v.z);
            break;

        default:
            oops();
    }
}

Vector Point::GetCoords(void) {
    Vector v;
    switch(type) {
        case IN_FREE_SPACE:
            v.x = SS.param.FindById(param(0))->val;
            v.y = SS.param.FindById(param(1))->val;
            v.z = SS.param.FindById(param(2))->val;
            break;

        default:
            oops();
    }

    return v;
}

void Point::Draw(void) {
    Vector v = GetCoords();

    double s = 4;
    Vector r = SS.GW.projRight.ScaledBy(4/SS.GW.scale);
    Vector d = SS.GW.projDown.ScaledBy(4/SS.GW.scale);

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

