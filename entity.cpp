#include "solvespace.h"

char *Entity::DescriptionString(void) {
    Request *r = SS.GetRequest(h.request());
    return r->DescriptionString();
}

void Entity::Csys2dGetBasisVectors(Vector *u, Vector *v) {
    double q[4];
    for(int i = 0; i < 4; i++) {
        q[i] = SS.GetParam(param.h[i])->val;
    }
    Quaternion quat = Quaternion::MakeFrom(q[0], q[1], q[2], q[3]);

    *u = quat.RotationU();
    *v = quat.RotationV();
}

Vector Entity::Csys2dGetNormalVector(void) {
    Vector u, v;
    Csys2dGetBasisVectors(&u, &v);
    return u.Cross(v);
}

void Entity::Csys2dGetBasisExprs(ExprVector *u, ExprVector *v) {
    Expr *a = Expr::FromParam(param.h[0]);
    Expr *b = Expr::FromParam(param.h[1]);
    Expr *c = Expr::FromParam(param.h[2]);
    Expr *d = Expr::FromParam(param.h[3]);

    Expr *two = Expr::FromConstant(2);

    u->x = a->Square();
    u->x = (u->x)->Plus(b->Square());
    u->x = (u->x)->Minus(c->Square());
    u->x = (u->x)->Minus(d->Square());

    u->y = two->Times(a->Times(d));
    u->y = (u->y)->Plus(two->Times(b->Times(c)));

    u->z = two->Times(b->Times(d));
    u->z = (u->z)->Minus(two->Times(a->Times(c)));

    v->x = two->Times(b->Times(c));
    v->x = (v->x)->Minus(two->Times(a->Times(d)));

    v->y = a->Square();
    v->y = (v->y)->Minus(b->Square());
    v->y = (v->y)->Plus(c->Square());
    v->y = (v->y)->Minus(d->Square());

    v->z = two->Times(a->Times(b));
    v->z = (v->z)->Plus(two->Times(c->Times(d)));
}

ExprVector Entity::Csys2dGetOffsetExprs(void) {
    return SS.GetEntity(assoc[0])->PointGetExprs();
}

bool Entity::HasPlane(void) {
    switch(type) {
        case CSYS_2D:
            return true;
        default:
            return false;
    }
}

void Entity::PlaneGetExprs(ExprVector *n, Expr **dn) {
    if(type == CSYS_2D) {
        Expr *a = Expr::FromParam(param.h[0]);
        Expr *b = Expr::FromParam(param.h[1]);
        Expr *c = Expr::FromParam(param.h[2]);
        Expr *d = Expr::FromParam(param.h[3]);

        Expr *two = Expr::FromConstant(2);

        // Convert the quaternion to our plane's normal vector.
        n->x =               two->Times(a->Times(c));
        n->x = (n->x)->Plus (two->Times(b->Times(d)));
        n->y =               two->Times(c->Times(d));
        n->y = (n->y)->Minus(two->Times(a->Times(b)));
        n->z =               a->Square();
        n->z = (n->z)->Minus(b->Square());
        n->z = (n->z)->Minus(c->Square());
        n->z = (n->z)->Plus (d->Square());

        ExprVector p0 = SS.GetEntity(assoc[0])->PointGetExprs();
        // The plane is n dot (p - p0) = 0, or
        //              n dot p - n dot p0 = 0
        // so dn = n dot p0
        *dn = p0.Dot(*n);
    } else {
        oops();
    }
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

bool Entity::IsPointIn3d(void) {
    switch(type) {
        case POINT_IN_3D:
            return true;
        default:
            return false;
    }
}

bool Entity::PointIsKnown(void) {
    switch(type) {
        case POINT_IN_3D:
            return SS.GetParam(param.h[0])->known &&
                   SS.GetParam(param.h[1])->known &&
                   SS.GetParam(param.h[2])->known;
        case POINT_IN_2D:
            return SS.GetParam(param.h[0])->known &&
                   SS.GetParam(param.h[1])->known;
        default: oops();
    }
}

bool Entity::PointIsFromReferences(void) {
    return h.request().IsFromReferences();
}

void Entity::PointForceTo(Vector p) {
    switch(type) {
        case POINT_IN_3D:
            SS.GetParam(param.h[0])->val = p.x;
            SS.GetParam(param.h[1])->val = p.y;
            SS.GetParam(param.h[2])->val = p.z;
            break;

        case POINT_IN_2D: {
            Entity *c = SS.GetEntity(csys);
            Vector u, v;
            c->Csys2dGetBasisVectors(&u, &v);
            SS.GetParam(param.h[0])->val = p.Dot(u);
            SS.GetParam(param.h[1])->val = p.Dot(v);
            break;
        }
        default: oops();
    }
}

Vector Entity::PointGetCoords(void) {
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
            c->Csys2dGetBasisVectors(&u, &v);
            p =        u.ScaledBy(SS.GetParam(param.h[0])->val);
            p = p.Plus(v.ScaledBy(SS.GetParam(param.h[1])->val));
            break;
        }
        default: oops();
    }
    return p;
}

ExprVector Entity::PointGetExprs(void) {
    ExprVector r;
    switch(type) {
        case POINT_IN_3D:
            r.x = Expr::FromParam(param.h[0]);
            r.y = Expr::FromParam(param.h[1]);
            r.z = Expr::FromParam(param.h[2]);
            break;

        case POINT_IN_2D: {
            Entity *c = SS.GetEntity(csys);
            ExprVector u, v;
            c->Csys2dGetBasisExprs(&u, &v);

            r =        u.ScaledBy(Expr::FromParam(param.h[0]));
            r = r.Plus(v.ScaledBy(Expr::FromParam(param.h[1])));
            break;
        }
        default: oops();
    }
    return r;
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

void Entity::LineDrawOrGetDistanceOrEdge(Vector a, Vector b) {
    LineDrawOrGetDistance(a, b);
    if(dogd.edges) {
        SEdge edge;
        edge.a = a; edge.b = b;
        dogd.edges->l.Add(&edge);
    }
}

void Entity::Draw(int order) {
    dogd.drawing = true;
    dogd.edges = NULL;
    DrawOrGetDistance(order);
}

void Entity::GenerateEdges(SEdgeList *el) {
    dogd.drawing = false;
    dogd.edges = el;
    DrawOrGetDistance(-1);
    dogd.edges = NULL;
}

double Entity::GetDistance(Point2d mp) {
    dogd.drawing = false;
    dogd.edges = NULL;
    dogd.mp = mp;
    dogd.dmin = 1e12;

    DrawOrGetDistance(-1);
    
    return dogd.dmin;
}

void Entity::DrawOrGetDistance(int order) {  
    glxColor3d(1, 1, 1);

    switch(type) {
        case POINT_IN_3D:
        case POINT_IN_2D: {
            if(order >= 0 && order != 2) break;
            if(!SS.GW.showPoints) break;

            Entity *isfor = SS.GetEntity(h.request().entity(0));
            if(!SS.GW.show2dCsyss && isfor->type == Entity::CSYS_2D) break;

            Vector v = PointGetCoords();

            if(dogd.drawing) {
                double s = 3;
                Vector r = SS.GW.projRight.ScaledBy(s/SS.GW.scale);
                Vector d = SS.GW.projUp.ScaledBy(s/SS.GW.scale);

                glxColor3d(0, 0.8, 0);
                glBegin(GL_QUADS);
                    glxVertex3v(v.Plus (r).Plus (d));
                    glxVertex3v(v.Plus (r).Minus(d));
                    glxVertex3v(v.Minus(r).Minus(d));
                    glxVertex3v(v.Minus(r).Plus (d));
                glEnd();
            } else {
                Point2d pp = SS.GW.ProjectPoint(v);
                dogd.dmin = pp.DistanceTo(dogd.mp) - 5;
            }
            break;
        }

        case CSYS_2D: {
            if(order >= 0 && order != 0) break;
            if(!SS.GW.show2dCsyss) break;

            Vector p;
            p = SS.GetEntity(assoc[0])->PointGetCoords();

            Vector u, v;
            Csys2dGetBasisVectors(&u, &v);

            double s = (min(SS.GW.width, SS.GW.height))*0.4/SS.GW.scale;

            Vector us = u.ScaledBy(s);
            Vector vs = v.ScaledBy(s);

            Vector pp = p.Plus (us).Plus (vs);
            Vector pm = p.Plus (us).Minus(vs);
            Vector mm = p.Minus(us).Minus(vs);
            Vector mp = p.Minus(us).Plus (vs);

            glxColor3d(0, 0.4, 0.4);
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
            if(order >= 0 && order != 1) break;
            Vector a = SS.GetEntity(assoc[0])->PointGetCoords();
            Vector b = SS.GetEntity(assoc[1])->PointGetCoords();
            LineDrawOrGetDistanceOrEdge(a, b);
            break;
        }

        case CUBIC: {
            Vector p0 = SS.GetEntity(assoc[0])->PointGetCoords();
            Vector p1 = SS.GetEntity(assoc[1])->PointGetCoords();
            Vector p2 = SS.GetEntity(assoc[2])->PointGetCoords();
            Vector p3 = SS.GetEntity(assoc[3])->PointGetCoords();
            int i, n = 20;
            Vector prev = p0;
            for(i = 1; i <= n; i++) {
                double t = ((double)i)/n;
                Vector p =
                    (p0.ScaledBy((1 - t)*(1 - t)*(1 - t))).Plus(
                    (p1.ScaledBy(3*t*(1 - t)*(1 - t))).Plus(
                    (p2.ScaledBy(3*t*t*(1 - t))).Plus(
                    (p3.ScaledBy(t*t*t)))));
                LineDrawOrGetDistanceOrEdge(prev, p);
                prev = p;
            }
            break;
        }

        default:
            oops();
    }
}

