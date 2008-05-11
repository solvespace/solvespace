#include "solvespace.h"

char *Entity::DescriptionString(void) {
    if(h.isFromRequest()) {
        Request *r = SS.GetRequest(h.request());
        return r->DescriptionString();
    } else {
        Group *g = SS.GetGroup(h.group());
        return g->DescriptionString();
    }
}

bool Entity::HasVector(void) {
    switch(type) {
        case LINE_SEGMENT:
        case NORMAL_IN_3D:
        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
            return true;

        default:
            return false;
    }
}

ExprVector Entity::VectorGetExprs(void) {
    switch(type) {
        case LINE_SEGMENT:
            return (SS.GetEntity(point[0])->PointGetExprs()).Minus(
                    SS.GetEntity(point[1])->PointGetExprs());

        case NORMAL_IN_3D:
        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
            return NormalExprsN();

        default: oops();
    }
}

Vector Entity::VectorGetNum(void) {
    switch(type) {
        case LINE_SEGMENT:
            return (SS.GetEntity(point[0])->PointGetNum()).Minus(
                    SS.GetEntity(point[1])->PointGetNum());

        case NORMAL_IN_3D:
        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
            return NormalN();

        default: oops();
    }
}

Vector Entity::VectorGetRefPoint(void) {
    switch(type) {
        case LINE_SEGMENT:
            return ((SS.GetEntity(point[0])->PointGetNum()).Plus(
                     SS.GetEntity(point[1])->PointGetNum())).ScaledBy(0.5);

        case NORMAL_IN_3D:
        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
            return SS.GetEntity(point[0])->PointGetNum();

        default: oops();
    }
}

bool Entity::IsCircle(void) {
    return (type == CIRCLE);
}

bool Entity::IsWorkplane(void) {
    return (type == WORKPLANE);
}

ExprVector Entity::WorkplaneGetOffsetExprs(void) {
    return SS.GetEntity(point[0])->PointGetExprs();
}

Vector Entity::WorkplaneGetOffset(void) {
    return SS.GetEntity(point[0])->PointGetNum();
}

void Entity::WorkplaneGetPlaneExprs(ExprVector *n, Expr **dn) {
    if(type == WORKPLANE) {
        *n = Normal()->NormalExprsN();

        ExprVector p0 = SS.GetEntity(point[0])->PointGetExprs();
        // The plane is n dot (p - p0) = 0, or
        //              n dot p - n dot p0 = 0
        // so dn = n dot p0
        *dn = p0.Dot(*n);
    } else {
        oops();
    }
}

double Entity::DistanceGetNum(void) {
    if(type == DISTANCE) {
        return SS.GetParam(param[0])->val;
    } else if(type == DISTANCE_N_COPY) {
        return numDistance;
    } else oops();
}
Expr *Entity::DistanceGetExpr(void) {
    if(type == DISTANCE) {
        return Expr::FromParam(param[0]);
    } else if(type == DISTANCE_N_COPY) {
        return Expr::FromConstant(numDistance);
    } else oops();
}
void Entity::DistanceForceTo(double v) {
    if(type == DISTANCE) {
        (SS.GetParam(param[0]))->val = v;
    } else if(type == DISTANCE_N_COPY) {
        // do nothing, it's locked
    } else oops();
}

Entity *Entity::Normal(void) {
    return SS.GetEntity(normal);
}

bool Entity::IsPoint(void) {
    switch(type) {
        case POINT_IN_3D:
        case POINT_IN_2D:
        case POINT_N_COPY:
        case POINT_N_TRANS:
        case POINT_N_ROT_TRANS:
            return true;

        default:
            return false;
    }
}

bool Entity::IsNormal(void) {
    switch(type) {
        case NORMAL_IN_3D:
        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
            return true;

        default:           return false;
    }
}

Quaternion Entity::NormalGetNum(void) {
    Quaternion q;
    switch(type) {
        case NORMAL_IN_3D:
            q.w  = SS.GetParam(param[0])->val;
            q.vx = SS.GetParam(param[1])->val;
            q.vy = SS.GetParam(param[2])->val;
            q.vz = SS.GetParam(param[3])->val;
            break;

        case NORMAL_IN_2D: {
            Entity *wrkpl = SS.GetEntity(workplane);
            Entity *norm = SS.GetEntity(wrkpl->normal);
            q = norm->NormalGetNum();
            break;
        }
        case NORMAL_N_COPY:
            q = numNormal;
            break;

        case NORMAL_N_ROT:
            q.w  = SS.GetParam(param[0])->val;
            q.vx = SS.GetParam(param[1])->val;
            q.vy = SS.GetParam(param[2])->val;
            q.vz = SS.GetParam(param[3])->val;
            q = q.Times(numNormal);
            break;

        default: oops();
    }
    return q;
}

void Entity::NormalForceTo(Quaternion q) {
    switch(type) {
        case NORMAL_IN_3D:
            SS.GetParam(param[0])->val = q.w;
            SS.GetParam(param[1])->val = q.vx;
            SS.GetParam(param[2])->val = q.vy;
            SS.GetParam(param[3])->val = q.vz;
            break;

        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
            // There's absolutely nothing to do; these are locked.
            break;

        case NORMAL_N_ROT:
            break;

        default: oops();
    }
}

Vector Entity::NormalU(void) {
    return NormalGetNum().RotationU();
}
Vector Entity::NormalV(void) {
    return NormalGetNum().RotationV();
}
Vector Entity::NormalN(void) {
    return NormalGetNum().RotationN();
}

ExprVector Entity::NormalExprsU(void) {
    return NormalGetExprs().RotationU();
}
ExprVector Entity::NormalExprsV(void) {
    return NormalGetExprs().RotationV();
}
ExprVector Entity::NormalExprsN(void) {
    return NormalGetExprs().RotationN();
}

ExprQuaternion Entity::NormalGetExprs(void) {
    ExprQuaternion q;
    switch(type) {
        case NORMAL_IN_3D:
            q.w  = Expr::FromParam(param[0]);
            q.vx = Expr::FromParam(param[1]);
            q.vy = Expr::FromParam(param[2]);
            q.vz = Expr::FromParam(param[3]);
            break;

        case NORMAL_IN_2D: {
            Entity *wrkpl = SS.GetEntity(workplane);
            Entity *norm = SS.GetEntity(wrkpl->normal);
            q = norm->NormalGetExprs();
            break;
        }
        case NORMAL_N_COPY:
            q.w  = Expr::FromConstant(numNormal.w);
            q.vx = Expr::FromConstant(numNormal.vx);
            q.vy = Expr::FromConstant(numNormal.vy);
            q.vz = Expr::FromConstant(numNormal.vz);
            break;

        case NORMAL_N_ROT: {
            ExprQuaternion orig;
            orig.w  = Expr::FromConstant(numNormal.w);
            orig.vx = Expr::FromConstant(numNormal.vx);
            orig.vy = Expr::FromConstant(numNormal.vy);
            orig.vz = Expr::FromConstant(numNormal.vz);

            q.w  = Expr::FromParam(param[0]);
            q.vx = Expr::FromParam(param[1]);
            q.vy = Expr::FromParam(param[2]);
            q.vz = Expr::FromParam(param[3]);

            q = q.Times(orig);
            break;
        }

        default: oops();
    }
    return q;
}

bool Entity::PointIsFromReferences(void) {
    return h.request().IsFromReferences();
}

void Entity::PointForceTo(Vector p) {
    switch(type) {
        case POINT_IN_3D:
            SS.GetParam(param[0])->val = p.x;
            SS.GetParam(param[1])->val = p.y;
            SS.GetParam(param[2])->val = p.z;
            break;

        case POINT_IN_2D: {
            Entity *c = SS.GetEntity(workplane);
            p = p.Minus(c->WorkplaneGetOffset());
            SS.GetParam(param[0])->val = p.Dot(c->Normal()->NormalU());
            SS.GetParam(param[1])->val = p.Dot(c->Normal()->NormalV());
            break;
        }

        case POINT_N_TRANS: {
            Vector trans = p.Minus(numPoint);
            SS.GetParam(param[0])->val = trans.x;
            SS.GetParam(param[1])->val = trans.y;
            SS.GetParam(param[2])->val = trans.z;
            break;
        }

        case POINT_N_ROT_TRANS: {
            // Force only the translation; leave the rotation unchanged. But
            // remember that we're working with respect to the rotated
            // point.
            Vector trans = p.Minus(PointGetQuaternion().Rotate(numPoint));
            SS.GetParam(param[0])->val = trans.x;
            SS.GetParam(param[1])->val = trans.y;
            SS.GetParam(param[2])->val = trans.z;
            break;
        }

        case POINT_N_COPY:
            // Nothing to do; it's a static copy
            break;

        default: oops();
    }
}

Vector Entity::PointGetNum(void) {
    Vector p;
    switch(type) {
        case POINT_IN_3D:
            p.x = SS.GetParam(param[0])->val;
            p.y = SS.GetParam(param[1])->val;
            p.z = SS.GetParam(param[2])->val;
            break;

        case POINT_IN_2D: {
            Entity *c = SS.GetEntity(workplane);
            Vector u = c->Normal()->NormalU();
            Vector v = c->Normal()->NormalV();
            p =        u.ScaledBy(SS.GetParam(param[0])->val);
            p = p.Plus(v.ScaledBy(SS.GetParam(param[1])->val));
            p = p.Plus(c->WorkplaneGetOffset());
            break;
        }

        case POINT_N_TRANS: {
            p = numPoint; 
            p.x += SS.GetParam(param[0])->val;
            p.y += SS.GetParam(param[1])->val;
            p.z += SS.GetParam(param[2])->val;
            break;
        }

        case POINT_N_ROT_TRANS: {
            Vector offset = Vector::MakeFrom(
                SS.GetParam(param[0])->val,
                SS.GetParam(param[1])->val,
                SS.GetParam(param[2])->val);
            Quaternion q = PointGetQuaternion();
            p = q.Rotate(numPoint);
            p = p.Plus(offset);
            break;
        }

        case POINT_N_COPY:
            p = numPoint;
            break;

        default: oops();
    }
    return p;
}

ExprVector Entity::PointGetExprs(void) {
    ExprVector r;
    switch(type) {
        case POINT_IN_3D:
            r.x = Expr::FromParam(param[0]);
            r.y = Expr::FromParam(param[1]);
            r.z = Expr::FromParam(param[2]);
            break;

        case POINT_IN_2D: {
            Entity *c = SS.GetEntity(workplane);
            ExprVector u = c->Normal()->NormalExprsU();
            ExprVector v = c->Normal()->NormalExprsV();
            r = c->WorkplaneGetOffsetExprs();
            r = r.Plus(u.ScaledBy(Expr::FromParam(param[0])));
            r = r.Plus(v.ScaledBy(Expr::FromParam(param[1])));
            break;
        }
        case POINT_N_TRANS: {
            ExprVector orig = {
                Expr::FromConstant(numPoint.x),
                Expr::FromConstant(numPoint.y),
                Expr::FromConstant(numPoint.z) };
            ExprVector trans;
            trans.x = Expr::FromParam(param[0]);
            trans.y = Expr::FromParam(param[1]);
            trans.z = Expr::FromParam(param[2]);
            r = orig.Plus(trans);
            break;
        }
        case POINT_N_ROT_TRANS: {
            ExprVector orig = {
                Expr::FromConstant(numPoint.x),
                Expr::FromConstant(numPoint.y),
                Expr::FromConstant(numPoint.z) };
            ExprVector trans = {
                Expr::FromParam(param[0]),
                Expr::FromParam(param[1]),
                Expr::FromParam(param[2]) };
            ExprQuaternion q = { 
                Expr::FromParam(param[3]),
                Expr::FromParam(param[4]),
                Expr::FromParam(param[5]),
                Expr::FromParam(param[6]) };
            orig = q.Rotate(orig);
            r = orig.Plus(trans);
            break;
        }
        case POINT_N_COPY:
            r.x = Expr::FromConstant(numPoint.x);
            r.y = Expr::FromConstant(numPoint.y);
            r.z = Expr::FromConstant(numPoint.z);
            break;

        default: oops();
    }
    return r;
}

void Entity::PointGetExprsInWorkplane(hEntity wrkpl, Expr **u, Expr **v) {
    if(type == POINT_IN_2D && workplane.v == wrkpl.v) {
        // They want our coordinates in the form that we've written them,
        // very nice.
        *u = Expr::FromParam(param[0]);
        *v = Expr::FromParam(param[1]);
    } else {
        // Get the offset and basis vectors for this weird exotic csys.
        Entity *w = SS.GetEntity(wrkpl);
        ExprVector wp = w->WorkplaneGetOffsetExprs();
        ExprVector wu = w->Normal()->NormalExprsU();
        ExprVector wv = w->Normal()->NormalExprsV();

        // Get our coordinates in three-space, and project them into that
        // coordinate system.
        ExprVector ev = PointGetExprs();
        ev = ev.Minus(wp);
        *u = ev.Dot(wu);
        *v = ev.Dot(wv);
    }
}

void Entity::PointForceQuaternionTo(Quaternion q) {
    if(type != POINT_N_ROT_TRANS) oops();

    SS.GetParam(param[3])->val = q.w;
    SS.GetParam(param[4])->val = q.vx;
    SS.GetParam(param[5])->val = q.vy;
    SS.GetParam(param[6])->val = q.vz;
}

Quaternion Entity::PointGetQuaternion(void) {
    if(type != POINT_N_ROT_TRANS) oops();

    Quaternion q;
    q.w  = SS.GetParam(param[3])->val;
    q.vx = SS.GetParam(param[4])->val;
    q.vy = SS.GetParam(param[5])->val;
    q.vz = SS.GetParam(param[6])->val;
    return q;
}

void Entity::LineDrawOrGetDistance(Vector a, Vector b) {
    if(dogd.drawing) {
        // This fudge guarantees that the line will get drawn in front of
        // anything else at the "same" depth in the z-buffer, so that it
        // goes in front of the shaded stuff.
        Vector n = SS.GW.projRight.Cross(SS.GW.projUp);
        n = n.WithMagnitude(3/SS.GW.scale);
        glBegin(GL_LINE_STRIP);
            glxVertex3v(a.Plus(n));
            glxVertex3v(b.Plus(n));
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
    if(dogd.edges && !construction) {
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
    Group *g = SS.GetGroup(group);
    // If an entity is invisible, then it doesn't get shown, and it doesn't
    // contribute a distance for the selection, but it still generates edges.
    if(!(g->visible) && !dogd.edges) return;

    if(group.v != SS.GW.activeGroup.v) {
        glxColor3d(0.5, 0.3, 0.0);
    } else if(construction) {
        glxColor3d(0.1, 0.7, 0.1);
    } else {
        glxColor3d(1, 1, 1);
    }

    switch(type) {
        case POINT_N_COPY:
        case POINT_N_TRANS:
        case POINT_N_ROT_TRANS:
        case POINT_IN_3D:
        case POINT_IN_2D: {
            if(order >= 0 && order != 2) break;
            if(!SS.GW.showPoints) break;

            if(h.isFromRequest()) {
                Entity *isfor = SS.GetEntity(h.request().entity(0));
                if(!SS.GW.showWorkplanes && isfor->type == Entity::WORKPLANE) {
                    break;
                }
            }

            Vector v = PointGetNum();

            if(dogd.drawing) {
                double s = 3;
                Vector r = SS.GW.projRight.ScaledBy(s/SS.GW.scale);
                Vector d = SS.GW.projUp.ScaledBy(s/SS.GW.scale);

                // The usual fudge, to make this appear in front.
                Vector gn = SS.GW.projRight.Cross(SS.GW.projUp);
                v = v.Plus(gn.ScaledBy(4/SS.GW.scale));

                glxColor3d(0, 0.8, 0);
                glBegin(GL_QUADS);
                    glxVertex3v(v.Plus (r).Plus (d));
                    glxVertex3v(v.Plus (r).Minus(d));
                    glxVertex3v(v.Minus(r).Minus(d));
                    glxVertex3v(v.Minus(r).Plus (d));
                glEnd();
            } else {
                Point2d pp = SS.GW.ProjectPoint(v);
                // Make a free point slightly easier to select, so that with
                // coincident points, we select the free one.
                dogd.dmin = pp.DistanceTo(dogd.mp) - 4;
            }
            break;
        }

        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
        case NORMAL_IN_3D:
        case NORMAL_IN_2D: {
            if(order >= 0 && order != 2) break;
            if(!SS.GW.showNormals) break;

            hRequest hr = h.request();
            double f = 0.5;
            if(hr.v == Request::HREQUEST_REFERENCE_XY.v) {
                glxColor3d(0, 0, f);
            } else if(hr.v == Request::HREQUEST_REFERENCE_YZ.v) {
                glxColor3d(f, 0, 0);
            } else if(hr.v == Request::HREQUEST_REFERENCE_ZX.v) {
                glxColor3d(0, f, 0);
            } else {
                glxColor3d(0, 0.4, 0.4);
            }
            Quaternion q = NormalGetNum();
            Vector tail = SS.GetEntity(point[0])->PointGetNum();
            Vector v = (q.RotationN()).WithMagnitude(50/SS.GW.scale);
            Vector tip = tail.Plus(v);
            LineDrawOrGetDistance(tail, tip);

            v = v.WithMagnitude(12/SS.GW.scale);
            Vector axis = q.RotationV();
            LineDrawOrGetDistance(tip, tip.Minus(v.RotatedAbout(axis,  0.6)));
            LineDrawOrGetDistance(tip, tip.Minus(v.RotatedAbout(axis, -0.6)));
            break;
        }

        case DISTANCE:
        case DISTANCE_N_COPY:
            // These are used only as data structures, nothing to display.
            break;

        case WORKPLANE: {
            if(order >= 0 && order != 0) break;
            if(!SS.GW.showWorkplanes) break;

            Vector p;
            p = SS.GetEntity(point[0])->PointGetNum();

            Vector u = Normal()->NormalU();
            Vector v = Normal()->NormalV();

            double s = (min(SS.GW.width, SS.GW.height))*0.4/SS.GW.scale;

            Vector us = u.ScaledBy(s);
            Vector vs = v.ScaledBy(s);

            Vector pp = p.Plus (us).Plus (vs);
            Vector pm = p.Plus (us).Minus(vs);
            Vector mm = p.Minus(us).Minus(vs);
            Vector mp = p.Minus(us).Plus (vs);

            glxColor3d(0, 0.3, 0.3);
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(3, 0x1111);
            LineDrawOrGetDistance(pp, pm);
            LineDrawOrGetDistance(pm, mm);
            LineDrawOrGetDistance(mm, mp);
            LineDrawOrGetDistance(mp, pp);
            glDisable(GL_LINE_STIPPLE);

            if(dogd.drawing) {
                glPushMatrix();
                    glxTranslatev(mm);
                    glxOntoWorkplane(u, v);
                    glxWriteText(DescriptionString());
                glPopMatrix();
            } else {
                // If a line lies in a plane, then select the line, not
                // the plane.
                dogd.dmin += 3;
            }
            break;
        }

        case LINE_SEGMENT: {
            if(order >= 0 && order != 1) break;
            Vector a = SS.GetEntity(point[0])->PointGetNum();
            Vector b = SS.GetEntity(point[1])->PointGetNum();
            LineDrawOrGetDistanceOrEdge(a, b);
            break;
        }

        case CUBIC: {
            if(order >= 0 && order != 1) break;
            Vector p0 = SS.GetEntity(point[0])->PointGetNum();
            Vector p1 = SS.GetEntity(point[1])->PointGetNum();
            Vector p2 = SS.GetEntity(point[2])->PointGetNum();
            Vector p3 = SS.GetEntity(point[3])->PointGetNum();
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

        case CIRCLE: {
            if(order >= 0 && order != 1) break;

            Quaternion q = SS.GetEntity(normal)->NormalGetNum();
            double r = SS.GetEntity(distance)->DistanceGetNum();
            Vector center = SS.GetEntity(point[0])->PointGetNum();
            Vector u = q.RotationU(), v = q.RotationV();

            int i, c = 40;
            Vector prev = u.ScaledBy(r).Plus(center);
            for(i = 1; i <= c; i++) {
                double phi = (2*PI*i)/c;
                Vector p = (u.ScaledBy(r*cos(phi))).Plus(
                            v.ScaledBy(r*sin(phi)));
                p = p.Plus(center);
                LineDrawOrGetDistanceOrEdge(prev, p);
                prev = p;
            }
            break;
        }

        default:
            oops();
    }
}

void Entity::AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index) {
    Equation eq;
    eq.e = expr;
    eq.h = h.equation(index);
    l->Add(&eq);
}

void Entity::GenerateEquations(IdList<Equation,hEquation> *l) {
    switch(type) {
        case NORMAL_IN_3D: {
            ExprQuaternion q = NormalGetExprs();
            AddEq(l, (q.Magnitude())->Minus(Expr::FromConstant(1)), 0);
            break;
        }
        default:;
            // Most entities do not generate equations.
    }
}


