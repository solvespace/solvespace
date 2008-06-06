#include "solvespace.h"

const hEntity  Entity::FREE_IN_3D = { 0 };
const hEntity  Entity::NO_ENTITY = { 0 };

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
        case NORMAL_N_ROT_AA:
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
        case NORMAL_N_ROT_AA:
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
        case NORMAL_N_ROT_AA:
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
        case NORMAL_N_ROT_AA:
            return SS.GetEntity(point[0])->PointGetNum();

        default: oops();
    }
}

bool Entity::IsCircle(void) {
    return (type == CIRCLE) || (type == ARC_OF_CIRCLE);
}

Expr *Entity::CircleGetRadiusExpr(void) {
    if(type == CIRCLE) {
        return SS.GetEntity(distance)->DistanceGetExpr();
    } else if(type == ARC_OF_CIRCLE) {
        return Constraint::Distance(workplane, point[0], point[1]);
    } else oops();
}

double Entity::CircleGetRadiusNum(void) {
    if(type == CIRCLE) {
        return SS.GetEntity(distance)->DistanceGetNum();
    } else if(type == ARC_OF_CIRCLE) {
        Vector c  = SS.GetEntity(point[0])->PointGetNum();
        Vector pa = SS.GetEntity(point[1])->PointGetNum();
        return (pa.Minus(c)).Magnitude();
    } else oops();
}

void Entity::ArcGetAngles(double *thetaa, double *thetab, double *dtheta) {
    if(type != ARC_OF_CIRCLE) oops();

    Quaternion q = Normal()->NormalGetNum();
    Vector u = q.RotationU(), v = q.RotationV();

    Vector c  = SS.GetEntity(point[0])->PointGetNum();
    Vector pa = SS.GetEntity(point[1])->PointGetNum();
    Vector pb = SS.GetEntity(point[2])->PointGetNum();

    Point2d c2  = c.Project2d(u, v);
    Point2d pa2 = (pa.Project2d(u, v)).Minus(c2);
    Point2d pb2 = (pb.Project2d(u, v)).Minus(c2);

    *thetaa = atan2(pa2.y, pa2.x);
    *thetab = atan2(pb2.y, pb2.x);
    *dtheta = *thetab - *thetaa;
    while(*dtheta < 0) *dtheta += 2*PI;
    while(*dtheta > (2*PI)) *dtheta -= 2*PI;
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
        return Expr::From(param[0]);
    } else if(type == DISTANCE_N_COPY) {
        return Expr::From(numDistance);
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
        case POINT_N_ROT_AA:
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
        case NORMAL_N_ROT_AA:
            return true;

        default:           return false;
    }
}

Quaternion Entity::NormalGetNum(void) {
    Quaternion q;
    switch(type) {
        case NORMAL_IN_3D:
            q = Quaternion::From(param[0], param[1], param[2], param[3]);
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
            q = Quaternion::From(param[0], param[1], param[2], param[3]);
            q = q.Times(numNormal);
            break;

        case NORMAL_N_ROT_AA: {
            double theta = timesApplied*SS.GetParam(param[0])->val;
            double s = sin(theta), c = cos(theta);
            q.w = c;
            q.vx = s*SS.GetParam(param[1])->val;
            q.vy = s*SS.GetParam(param[2])->val;
            q.vz = s*SS.GetParam(param[3])->val;
            q = q.Times(numNormal);
            break;
        }

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
        case NORMAL_N_ROT: {
            Quaternion qp = q.Times(numNormal.Inverse());
            
            SS.GetParam(param[0])->val = qp.w;
            SS.GetParam(param[1])->val = qp.vx;
            SS.GetParam(param[2])->val = qp.vy;
            SS.GetParam(param[3])->val = qp.vz;
            break;
        }

        case NORMAL_N_ROT_AA:
            // Not sure if I'll bother implementing this one
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
            q = ExprQuaternion::From(param[0], param[1], param[2], param[3]);
            break;

        case NORMAL_IN_2D: {
            Entity *wrkpl = SS.GetEntity(workplane);
            Entity *norm = SS.GetEntity(wrkpl->normal);
            q = norm->NormalGetExprs();
            break;
        }
        case NORMAL_N_COPY:
            q = ExprQuaternion::From(numNormal);
            break;

        case NORMAL_N_ROT: {
            ExprQuaternion orig = ExprQuaternion::From(numNormal);
            q = ExprQuaternion::From(param[0], param[1], param[2], param[3]);

            q = q.Times(orig);
            break;
        }

        case NORMAL_N_ROT_AA: {
            ExprQuaternion orig = ExprQuaternion::From(numNormal);

            Expr *theta = Expr::From(timesApplied)->Times(
                          Expr::From(param[0]));
            Expr *c = theta->Cos(), *s = theta->Sin();
            q.w = c;
            q.vx = s->Times(Expr::From(param[1]));
            q.vy = s->Times(Expr::From(param[2]));
            q.vz = s->Times(Expr::From(param[3]));

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
            if(timesApplied == 0) break;
            Vector trans = (p.Minus(numPoint)).ScaledBy(1.0/timesApplied);
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

        case POINT_N_ROT_AA: {
            // Force only the angle; the axis and center of rotation stay
            Vector offset = Vector::From(param[0], param[1], param[2]);
            Vector normal = Vector::From(param[4], param[5], param[6]);
            Vector u = normal.Normal(0), v = normal.Normal(1);
            Vector po = p.Minus(offset), numo = numPoint.Minus(offset);
            double thetap = atan2(v.Dot(po), u.Dot(po));
            double thetan = atan2(v.Dot(numo), u.Dot(numo));
            double thetaf = (thetap - thetan);
            double thetai = (SS.GetParam(param[3])->val)*timesApplied*2;
            double dtheta = thetaf - thetai;
            // Take the smallest possible change in the actual step angle,
            // in order to avoid jumps when you cross from +pi to -pi
            while(dtheta < -PI) dtheta += 2*PI;
            while(dtheta > PI) dtheta -= 2*PI;
            SS.GetParam(param[3])->val = (thetai + dtheta)/(timesApplied*2);
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
            p = Vector::From(param[0], param[1], param[2]);
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
            Vector trans = Vector::From(param[0], param[1], param[2]);
            p = numPoint.Plus(trans.ScaledBy(timesApplied));
            break;
        }

        case POINT_N_ROT_TRANS: {
            Vector offset = Vector::From(param[0], param[1], param[2]);
            Quaternion q = PointGetQuaternion();
            p = q.Rotate(numPoint);
            p = p.Plus(offset);
            break;
        }

        case POINT_N_ROT_AA: {
            Vector offset = Vector::From(param[0], param[1], param[2]);
            Quaternion q = PointGetQuaternion();
            p = numPoint.Minus(offset);
            p = q.Rotate(p);
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
            r = ExprVector::From(param[0], param[1], param[2]);
            break;

        case POINT_IN_2D: {
            Entity *c = SS.GetEntity(workplane);
            ExprVector u = c->Normal()->NormalExprsU();
            ExprVector v = c->Normal()->NormalExprsV();
            r = c->WorkplaneGetOffsetExprs();
            r = r.Plus(u.ScaledBy(Expr::From(param[0])));
            r = r.Plus(v.ScaledBy(Expr::From(param[1])));
            break;
        }
        case POINT_N_TRANS: {
            ExprVector orig = ExprVector::From(numPoint);
            ExprVector trans = ExprVector::From(param[0], param[1], param[2]);
            r = orig.Plus(trans.ScaledBy(Expr::From(timesApplied)));
            break;
        }
        case POINT_N_ROT_TRANS: {
            ExprVector orig = ExprVector::From(numPoint);
            ExprVector trans = ExprVector::From(param[0], param[1], param[2]);
            ExprQuaternion q =
                ExprQuaternion::From(param[3], param[4], param[5], param[6]);
            orig = q.Rotate(orig);
            r = orig.Plus(trans);
            break;
        }
        case POINT_N_ROT_AA: {
            ExprVector orig = ExprVector::From(numPoint);
            ExprVector trans = ExprVector::From(param[0], param[1], param[2]);
            Expr *theta = Expr::From(timesApplied)->Times(
                          Expr::From(param[3]));
            Expr *c = theta->Cos(), *s = theta->Sin();
            ExprQuaternion q = { 
                c,
                s->Times(Expr::From(param[4])),
                s->Times(Expr::From(param[5])),
                s->Times(Expr::From(param[6])) };
            orig = orig.Minus(trans);
            orig = q.Rotate(orig);
            r = orig.Plus(trans);
            break;
        }
        case POINT_N_COPY:
            r = ExprVector::From(numPoint);
            break;

        default: oops();
    }
    return r;
}

void Entity::PointGetExprsInWorkplane(hEntity wrkpl, Expr **u, Expr **v) {
    if(type == POINT_IN_2D && workplane.v == wrkpl.v) {
        // They want our coordinates in the form that we've written them,
        // very nice.
        *u = Expr::From(param[0]);
        *v = Expr::From(param[1]);
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
    Quaternion q;

    if(type == POINT_N_ROT_AA) {
        double theta = timesApplied*SS.GetParam(param[3])->val;
        double s = sin(theta), c = cos(theta);
        q.w = c;
        q.vx = s*SS.GetParam(param[4])->val;
        q.vy = s*SS.GetParam(param[5])->val;
        q.vz = s*SS.GetParam(param[6])->val;
    } else if(type == POINT_N_ROT_TRANS) {
        q = Quaternion::From(param[3], param[4], param[5], param[6]);
    } else oops();

    return q;
}

bool Entity::IsFace(void) {
    switch(type) {
        case FACE_NORMAL_PT:
        case FACE_XPROD:
        case FACE_N_ROT_TRANS:
            return true;
        default:
            return false;
    }
}

ExprVector Entity::FaceGetNormalExprs(void) {
    ExprVector r;
    if(type == FACE_NORMAL_PT) {
        Vector v = Vector::From(numNormal.vx, numNormal.vy, numNormal.vz);
        r = ExprVector::From(v.WithMagnitude(1));
    } else if(type == FACE_XPROD) {
        ExprVector vc = ExprVector::From(param[0], param[1], param[2]);
        ExprVector vn = ExprVector::From(numVector);
        r = vc.Cross(vn);
        r = r.WithMagnitude(Expr::From(1.0));
    } else if(type == FACE_N_ROT_TRANS) {
        // The numerical normal vector gets the rotation; the numerical
        // normal has magnitude one, and the rotation doesn't change that,
        // so there's no need to fix it up.
        r = ExprVector::From(numNormal.vx, numNormal.vy, numNormal.vz);
        ExprQuaternion q =
            ExprQuaternion::From(param[3], param[4], param[5], param[6]);
        r = q.Rotate(r);
    } else oops();
    return r;
}

Vector Entity::FaceGetNormalNum(void) {
    Vector r;
    if(type == FACE_NORMAL_PT) {
        r = Vector::From(numNormal.vx, numNormal.vy, numNormal.vz);
    } else if(type == FACE_XPROD) {
        Vector vc = Vector::From(param[0], param[1], param[2]);
        r = vc.Cross(numVector);
    } else if(type == FACE_N_ROT_TRANS) {
        // The numerical normal vector gets the rotation
        r = Vector::From(numNormal.vx, numNormal.vy, numNormal.vz);
        Quaternion q = Quaternion::From(param[3], param[4], param[5], param[6]);
        r = q.Rotate(r);
    } else oops();
    return r.WithMagnitude(1);
}

ExprVector Entity::FaceGetPointExprs(void) {
    ExprVector r;
    if(type == FACE_NORMAL_PT) {
        r = SS.GetEntity(point[0])->PointGetExprs();
    } else if(type == FACE_XPROD) {
        r = ExprVector::From(numPoint);
    } else if(type == FACE_N_ROT_TRANS) {
        // The numerical point gets the rotation and translation.
        ExprVector trans = ExprVector::From(param[0], param[1], param[2]);
        ExprQuaternion q =
            ExprQuaternion::From(param[3], param[4], param[5], param[6]);
        r = ExprVector::From(numPoint);
        r = q.Rotate(r);
        r = r.Plus(trans);
    } else oops();
    return r;
}

Vector Entity::FaceGetPointNum(void) {
    Vector r;
    if(type == FACE_NORMAL_PT) {
        r = SS.GetEntity(point[0])->PointGetNum();
    } else if(type == FACE_XPROD) {
        r = numPoint;
    } else if(type == FACE_N_ROT_TRANS) {
        // The numerical point gets the rotation and translation.
        Vector trans = Vector::From(param[0], param[1], param[2]);
        Quaternion q = Quaternion::From(param[3], param[4], param[5], param[6]);
        r = q.Rotate(numPoint);
        r = r.Plus(trans);
    } else oops();
    return r;
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
            AddEq(l, (q.Magnitude())->Minus(Expr::From(1)), 0);
            break;
        }
        case ARC_OF_CIRCLE: {
            // If this is a copied entity, with its point already fixed
            // with respect to each other, then we don't want to generate
            // the distance constraint!
            if(SS.GetEntity(point[0])->type == POINT_IN_2D) {
                Expr *ra = Constraint::Distance(workplane, point[0], point[1]);
                Expr *rb = Constraint::Distance(workplane, point[0], point[2]);
                AddEq(l, ra->Minus(rb), 0);
            }
            break;
        }
        default:;
            // Most entities do not generate equations.
    }
}

void Entity::CalculateNumerical(void) {
    if(IsPoint()) actPoint = PointGetNum();
    if(IsNormal()) actNormal = NormalGetNum();
    if(type == DISTANCE || type == DISTANCE_N_COPY) {
        actDistance = DistanceGetNum();
    }
    if(IsFace()) {
        numPoint  = FaceGetPointNum();
        Vector n = FaceGetNormalNum();
        numNormal = Quaternion::From(0, n.x, n.y, n.z);
    }
    visible = IsVisible();
}

