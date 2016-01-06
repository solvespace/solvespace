//-----------------------------------------------------------------------------
// The implementation of our entities in the symbolic algebra system, methods
// to return a symbolic representation of the entity (line by its endpoints,
// circle by center and radius, etc.).
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

const hEntity  EntityBase::FREE_IN_3D = { 0 };
const hEntity  EntityBase::NO_ENTITY = { 0 };

bool EntityBase::HasVector(void) {
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

ExprVector EntityBase::VectorGetExprs(void) {
    switch(type) {
        case LINE_SEGMENT:
            return (SK.GetEntity(point[0])->PointGetExprs()).Minus(
                    SK.GetEntity(point[1])->PointGetExprs());

        case NORMAL_IN_3D:
        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
        case NORMAL_N_ROT_AA:
            return NormalExprsN();

        default: oops();
    }
}

Vector EntityBase::VectorGetNum(void) {
    switch(type) {
        case LINE_SEGMENT:
            return (SK.GetEntity(point[0])->PointGetNum()).Minus(
                    SK.GetEntity(point[1])->PointGetNum());

        case NORMAL_IN_3D:
        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
        case NORMAL_N_ROT_AA:
            return NormalN();

        default: oops();
    }
}

Vector EntityBase::VectorGetRefPoint(void) {
    switch(type) {
        case LINE_SEGMENT:
            return ((SK.GetEntity(point[0])->PointGetNum()).Plus(
                     SK.GetEntity(point[1])->PointGetNum())).ScaledBy(0.5);

        case NORMAL_IN_3D:
        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
        case NORMAL_N_ROT_AA:
            return SK.GetEntity(point[0])->PointGetNum();

        default: oops();
    }
}

Vector EntityBase::VectorGetStartPoint(void) {
    switch(type) {
        case LINE_SEGMENT:
            return SK.GetEntity(point[1])->PointGetNum();

        case NORMAL_IN_3D:
        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
        case NORMAL_N_ROT:
        case NORMAL_N_ROT_AA:
            return SK.GetEntity(point[0])->PointGetNum();

        default: oops();
    }
}

bool EntityBase::IsCircle(void) {
    return (type == CIRCLE) || (type == ARC_OF_CIRCLE);
}

Expr *EntityBase::CircleGetRadiusExpr(void) {
    if(type == CIRCLE) {
        return SK.GetEntity(distance)->DistanceGetExpr();
    } else if(type == ARC_OF_CIRCLE) {
        return Constraint::Distance(workplane, point[0], point[1]);
    } else oops();
}

double EntityBase::CircleGetRadiusNum(void) {
    if(type == CIRCLE) {
        return SK.GetEntity(distance)->DistanceGetNum();
    } else if(type == ARC_OF_CIRCLE) {
        Vector c  = SK.GetEntity(point[0])->PointGetNum();
        Vector pa = SK.GetEntity(point[1])->PointGetNum();
        return (pa.Minus(c)).Magnitude();
    } else oops();
}

void EntityBase::ArcGetAngles(double *thetaa, double *thetab, double *dtheta) {
    if(type != ARC_OF_CIRCLE) oops();

    Quaternion q = Normal()->NormalGetNum();
    Vector u = q.RotationU(), v = q.RotationV();

    Vector c  = SK.GetEntity(point[0])->PointGetNum();
    Vector pa = SK.GetEntity(point[1])->PointGetNum();
    Vector pb = SK.GetEntity(point[2])->PointGetNum();

    Point2d c2  = c.Project2d(u, v);
    Point2d pa2 = (pa.Project2d(u, v)).Minus(c2);
    Point2d pb2 = (pb.Project2d(u, v)).Minus(c2);

    *thetaa = atan2(pa2.y, pa2.x);
    *thetab = atan2(pb2.y, pb2.x);
    *dtheta = *thetab - *thetaa;
    // If the endpoints are coincident, call it a full arc, not a zero arc;
    // useful concept to have when splitting
    while(*dtheta < 1e-6) *dtheta += 2*PI;
    while(*dtheta > (2*PI)) *dtheta -= 2*PI;
}

Vector EntityBase::CubicGetStartNum(void) {
    return SK.GetEntity(point[0])->PointGetNum();
}
Vector EntityBase::CubicGetFinishNum(void) {
    return SK.GetEntity(point[3+extraPoints])->PointGetNum();
}
ExprVector EntityBase::CubicGetStartTangentExprs(void) {
    ExprVector pon  = SK.GetEntity(point[0])->PointGetExprs(),
               poff = SK.GetEntity(point[1])->PointGetExprs();
    return (pon.Minus(poff));
}
ExprVector EntityBase::CubicGetFinishTangentExprs(void) {
    ExprVector pon  = SK.GetEntity(point[3+extraPoints])->PointGetExprs(),
               poff = SK.GetEntity(point[2+extraPoints])->PointGetExprs();
    return (pon.Minus(poff));
}
Vector EntityBase::CubicGetStartTangentNum(void) {
    Vector pon  = SK.GetEntity(point[0])->PointGetNum(),
           poff = SK.GetEntity(point[1])->PointGetNum();
    return (pon.Minus(poff));
}
Vector EntityBase::CubicGetFinishTangentNum(void) {
    Vector pon  = SK.GetEntity(point[3+extraPoints])->PointGetNum(),
           poff = SK.GetEntity(point[2+extraPoints])->PointGetNum();
    return (pon.Minus(poff));
}

bool EntityBase::IsWorkplane(void) {
    return (type == WORKPLANE);
}

ExprVector EntityBase::WorkplaneGetOffsetExprs(void) {
    return SK.GetEntity(point[0])->PointGetExprs();
}

Vector EntityBase::WorkplaneGetOffset(void) {
    return SK.GetEntity(point[0])->PointGetNum();
}

void EntityBase::WorkplaneGetPlaneExprs(ExprVector *n, Expr **dn) {
    if(type == WORKPLANE) {
        *n = Normal()->NormalExprsN();

        ExprVector p0 = SK.GetEntity(point[0])->PointGetExprs();
        // The plane is n dot (p - p0) = 0, or
        //              n dot p - n dot p0 = 0
        // so dn = n dot p0
        *dn = p0.Dot(*n);
    } else {
        oops();
    }
}

bool EntityBase::IsDistance(void) {
    return (type == DISTANCE) ||
           (type == DISTANCE_N_COPY);
}
double EntityBase::DistanceGetNum(void) {
    if(type == DISTANCE) {
        return SK.GetParam(param[0])->val;
    } else if(type == DISTANCE_N_COPY) {
        return numDistance;
    } else oops();
}
Expr *EntityBase::DistanceGetExpr(void) {
    if(type == DISTANCE) {
        return Expr::From(param[0]);
    } else if(type == DISTANCE_N_COPY) {
        return Expr::From(numDistance);
    } else oops();
}
void EntityBase::DistanceForceTo(double v) {
    if(type == DISTANCE) {
        (SK.GetParam(param[0]))->val = v;
    } else if(type == DISTANCE_N_COPY) {
        // do nothing, it's locked
    } else oops();
}

EntityBase *EntityBase::Normal(void) {
    return SK.GetEntity(normal);
}

bool EntityBase::IsPoint(void) {
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

bool EntityBase::IsNormal(void) {
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

Quaternion EntityBase::NormalGetNum(void) {
    Quaternion q;
    switch(type) {
        case NORMAL_IN_3D:
            q = Quaternion::From(param[0], param[1], param[2], param[3]);
            break;

        case NORMAL_IN_2D: {
            EntityBase *wrkpl = SK.GetEntity(workplane);
            EntityBase *norm = SK.GetEntity(wrkpl->normal);
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
            q = GetAxisAngleQuaternion(0);
            q = q.Times(numNormal);
            break;
        }

        default: oops();
    }
    return q;
}

void EntityBase::NormalForceTo(Quaternion q) {
    switch(type) {
        case NORMAL_IN_3D:
            SK.GetParam(param[0])->val = q.w;
            SK.GetParam(param[1])->val = q.vx;
            SK.GetParam(param[2])->val = q.vy;
            SK.GetParam(param[3])->val = q.vz;
            break;

        case NORMAL_IN_2D:
        case NORMAL_N_COPY:
            // There's absolutely nothing to do; these are locked.
            break;
        case NORMAL_N_ROT: {
            Quaternion qp = q.Times(numNormal.Inverse());

            SK.GetParam(param[0])->val = qp.w;
            SK.GetParam(param[1])->val = qp.vx;
            SK.GetParam(param[2])->val = qp.vy;
            SK.GetParam(param[3])->val = qp.vz;
            break;
        }

        case NORMAL_N_ROT_AA:
            // Not sure if I'll bother implementing this one
            break;

        default: oops();
    }
}

Vector EntityBase::NormalU(void) {
    return NormalGetNum().RotationU();
}
Vector EntityBase::NormalV(void) {
    return NormalGetNum().RotationV();
}
Vector EntityBase::NormalN(void) {
    return NormalGetNum().RotationN();
}

ExprVector EntityBase::NormalExprsU(void) {
    return NormalGetExprs().RotationU();
}
ExprVector EntityBase::NormalExprsV(void) {
    return NormalGetExprs().RotationV();
}
ExprVector EntityBase::NormalExprsN(void) {
    return NormalGetExprs().RotationN();
}

ExprQuaternion EntityBase::NormalGetExprs(void) {
    ExprQuaternion q;
    switch(type) {
        case NORMAL_IN_3D:
            q = ExprQuaternion::From(param[0], param[1], param[2], param[3]);
            break;

        case NORMAL_IN_2D: {
            EntityBase *wrkpl = SK.GetEntity(workplane);
            EntityBase *norm = SK.GetEntity(wrkpl->normal);
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
            q = GetAxisAngleQuaternionExprs(0);
            q = q.Times(orig);
            break;
        }

        default: oops();
    }
    return q;
}

void EntityBase::PointForceTo(Vector p) {
    switch(type) {
        case POINT_IN_3D:
            SK.GetParam(param[0])->val = p.x;
            SK.GetParam(param[1])->val = p.y;
            SK.GetParam(param[2])->val = p.z;
            break;

        case POINT_IN_2D: {
            EntityBase *c = SK.GetEntity(workplane);
            p = p.Minus(c->WorkplaneGetOffset());
            SK.GetParam(param[0])->val = p.Dot(c->Normal()->NormalU());
            SK.GetParam(param[1])->val = p.Dot(c->Normal()->NormalV());
            break;
        }

        case POINT_N_TRANS: {
            if(timesApplied == 0) break;
            Vector trans = (p.Minus(numPoint)).ScaledBy(1.0/timesApplied);
            SK.GetParam(param[0])->val = trans.x;
            SK.GetParam(param[1])->val = trans.y;
            SK.GetParam(param[2])->val = trans.z;
            break;
        }

        case POINT_N_ROT_TRANS: {
            // Force only the translation; leave the rotation unchanged. But
            // remember that we're working with respect to the rotated
            // point.
            Vector trans = p.Minus(PointGetQuaternion().Rotate(numPoint));
            SK.GetParam(param[0])->val = trans.x;
            SK.GetParam(param[1])->val = trans.y;
            SK.GetParam(param[2])->val = trans.z;
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
            double thetai = (SK.GetParam(param[3])->val)*timesApplied*2;
            double dtheta = thetaf - thetai;
            // Take the smallest possible change in the actual step angle,
            // in order to avoid jumps when you cross from +pi to -pi
            while(dtheta < -PI) dtheta += 2*PI;
            while(dtheta > PI) dtheta -= 2*PI;
            SK.GetParam(param[3])->val = (thetai + dtheta)/(timesApplied*2);
            break;
        }

        case POINT_N_COPY:
            // Nothing to do; it's a static copy
            break;

        default: oops();
    }
}

Vector EntityBase::PointGetNum(void) {
    Vector p;
    switch(type) {
        case POINT_IN_3D:
            p = Vector::From(param[0], param[1], param[2]);
            break;

        case POINT_IN_2D: {
            EntityBase *c = SK.GetEntity(workplane);
            Vector u = c->Normal()->NormalU();
            Vector v = c->Normal()->NormalV();
            p =        u.ScaledBy(SK.GetParam(param[0])->val);
            p = p.Plus(v.ScaledBy(SK.GetParam(param[1])->val));
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

ExprVector EntityBase::PointGetExprs(void) {
    ExprVector r;
    switch(type) {
        case POINT_IN_3D:
            r = ExprVector::From(param[0], param[1], param[2]);
            break;

        case POINT_IN_2D: {
            EntityBase *c = SK.GetEntity(workplane);
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
            ExprQuaternion q = GetAxisAngleQuaternionExprs(3);
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

void EntityBase::PointGetExprsInWorkplane(hEntity wrkpl, Expr **u, Expr **v) {
    if(type == POINT_IN_2D && workplane.v == wrkpl.v) {
        // They want our coordinates in the form that we've written them,
        // very nice.
        *u = Expr::From(param[0]);
        *v = Expr::From(param[1]);
    } else {
        // Get the offset and basis vectors for this weird exotic csys.
        EntityBase *w = SK.GetEntity(wrkpl);
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

void EntityBase::PointForceQuaternionTo(Quaternion q) {
    if(type != POINT_N_ROT_TRANS) oops();

    SK.GetParam(param[3])->val = q.w;
    SK.GetParam(param[4])->val = q.vx;
    SK.GetParam(param[5])->val = q.vy;
    SK.GetParam(param[6])->val = q.vz;
}

Quaternion EntityBase::GetAxisAngleQuaternion(int param0) {
    Quaternion q;
    double theta = timesApplied*SK.GetParam(param[param0+0])->val;
    double s = sin(theta), c = cos(theta);
    q.w = c;
    q.vx = s*SK.GetParam(param[param0+1])->val;
    q.vy = s*SK.GetParam(param[param0+2])->val;
    q.vz = s*SK.GetParam(param[param0+3])->val;
    return q;
}

ExprQuaternion EntityBase::GetAxisAngleQuaternionExprs(int param0) {
    ExprQuaternion q;

    Expr *theta = Expr::From(timesApplied)->Times(
                  Expr::From(param[param0+0]));
    Expr *c = theta->Cos(), *s = theta->Sin();
    q.w = c;
    q.vx = s->Times(Expr::From(param[param0+1]));
    q.vy = s->Times(Expr::From(param[param0+2]));
    q.vz = s->Times(Expr::From(param[param0+3]));
    return q;
}

Quaternion EntityBase::PointGetQuaternion(void) {
    Quaternion q;

    if(type == POINT_N_ROT_AA) {
        q = GetAxisAngleQuaternion(3);
    } else if(type == POINT_N_ROT_TRANS) {
        q = Quaternion::From(param[3], param[4], param[5], param[6]);
    } else oops();

    return q;
}

bool EntityBase::IsFace(void) {
    switch(type) {
        case FACE_NORMAL_PT:
        case FACE_XPROD:
        case FACE_N_ROT_TRANS:
        case FACE_N_TRANS:
        case FACE_N_ROT_AA:
            return true;
        default:
            return false;
    }
}

ExprVector EntityBase::FaceGetNormalExprs(void) {
    ExprVector r;
    if(type == FACE_NORMAL_PT) {
        Vector v = Vector::From(numNormal.vx, numNormal.vy, numNormal.vz);
        r = ExprVector::From(v.WithMagnitude(1));
    } else if(type == FACE_XPROD) {
        ExprVector vc = ExprVector::From(param[0], param[1], param[2]);
        ExprVector vn =
            ExprVector::From(numNormal.vx, numNormal.vy, numNormal.vz);
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
    } else if(type == FACE_N_TRANS) {
        r = ExprVector::From(numNormal.vx, numNormal.vy, numNormal.vz);
    } else if(type == FACE_N_ROT_AA) {
        r = ExprVector::From(numNormal.vx, numNormal.vy, numNormal.vz);
        ExprQuaternion q = GetAxisAngleQuaternionExprs(3);
        r = q.Rotate(r);
    } else oops();
    return r;
}

Vector EntityBase::FaceGetNormalNum(void) {
    Vector r;
    if(type == FACE_NORMAL_PT) {
        r = Vector::From(numNormal.vx, numNormal.vy, numNormal.vz);
    } else if(type == FACE_XPROD) {
        Vector vc = Vector::From(param[0], param[1], param[2]);
        Vector vn = Vector::From(numNormal.vx, numNormal.vy, numNormal.vz);
        r = vc.Cross(vn);
    } else if(type == FACE_N_ROT_TRANS) {
        // The numerical normal vector gets the rotation
        r = Vector::From(numNormal.vx, numNormal.vy, numNormal.vz);
        Quaternion q = Quaternion::From(param[3], param[4], param[5], param[6]);
        r = q.Rotate(r);
    } else if(type == FACE_N_TRANS) {
        r = Vector::From(numNormal.vx, numNormal.vy, numNormal.vz);
    } else if(type == FACE_N_ROT_AA) {
        r = Vector::From(numNormal.vx, numNormal.vy, numNormal.vz);
        Quaternion q = GetAxisAngleQuaternion(3);
        r = q.Rotate(r);
    } else oops();
    return r.WithMagnitude(1);
}

ExprVector EntityBase::FaceGetPointExprs(void) {
    ExprVector r;
    if(type == FACE_NORMAL_PT) {
        r = SK.GetEntity(point[0])->PointGetExprs();
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
    } else if(type == FACE_N_TRANS) {
        ExprVector trans = ExprVector::From(param[0], param[1], param[2]);
        r = ExprVector::From(numPoint);
        r = r.Plus(trans.ScaledBy(Expr::From(timesApplied)));
    } else if(type == FACE_N_ROT_AA) {
        ExprVector trans = ExprVector::From(param[0], param[1], param[2]);
        ExprQuaternion q = GetAxisAngleQuaternionExprs(3);
        r = ExprVector::From(numPoint);
        r = r.Minus(trans);
        r = q.Rotate(r);
        r = r.Plus(trans);
    } else oops();
    return r;
}

Vector EntityBase::FaceGetPointNum(void) {
    Vector r;
    if(type == FACE_NORMAL_PT) {
        r = SK.GetEntity(point[0])->PointGetNum();
    } else if(type == FACE_XPROD) {
        r = numPoint;
    } else if(type == FACE_N_ROT_TRANS) {
        // The numerical point gets the rotation and translation.
        Vector trans = Vector::From(param[0], param[1], param[2]);
        Quaternion q = Quaternion::From(param[3], param[4], param[5], param[6]);
        r = q.Rotate(numPoint);
        r = r.Plus(trans);
    } else if(type == FACE_N_TRANS) {
        Vector trans = Vector::From(param[0], param[1], param[2]);
        r = numPoint.Plus(trans.ScaledBy(timesApplied));
    } else if(type == FACE_N_ROT_AA) {
        Vector trans = Vector::From(param[0], param[1], param[2]);
        Quaternion q = GetAxisAngleQuaternion(3);
        r = numPoint.Minus(trans);
        r = q.Rotate(r);
        r = r.Plus(trans);
    } else oops();
    return r;
}

bool EntityBase::HasEndpoints(void) {
    return (type == LINE_SEGMENT) ||
           (type == CUBIC) ||
           (type == ARC_OF_CIRCLE);
}
Vector EntityBase::EndpointStart() {
    if(type == LINE_SEGMENT) {
        return SK.GetEntity(point[0])->PointGetNum();
    } else if(type == CUBIC) {
        return CubicGetStartNum();
    } else if(type == ARC_OF_CIRCLE) {
        return SK.GetEntity(point[1])->PointGetNum();
    } else {
        oops();
    }
}
Vector EntityBase::EndpointFinish() {
    if(type == LINE_SEGMENT) {
        return SK.GetEntity(point[1])->PointGetNum();
    } else if(type == CUBIC) {
        return CubicGetFinishNum();
    } else if(type == ARC_OF_CIRCLE) {
        return SK.GetEntity(point[2])->PointGetNum();
    } else {
        oops();
    }
}

void EntityBase::AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index) {
    Equation eq;
    eq.e = expr;
    eq.h = h.equation(index);
    l->Add(&eq);
}

void EntityBase::GenerateEquations(IdList<Equation,hEquation> *l) {
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
            if(SK.GetEntity(point[0])->type != POINT_IN_2D) break;

            // If the two endpoints of the arc are constrained coincident
            // (to make a complete circle), then our distance constraint
            // would be redundant and therefore overconstrain things.
            int i;
            for(i = 0; i < SK.constraint.n; i++) {
                ConstraintBase *c = &(SK.constraint.elem[i]);
                if(c->group.v != group.v) continue;
                if(c->type != Constraint::POINTS_COINCIDENT) continue;

                if((c->ptA.v == point[1].v && c->ptB.v == point[2].v) ||
                   (c->ptA.v == point[2].v && c->ptB.v == point[1].v))
                {
                    break;
                }
            }
            if(i < SK.constraint.n) break;

            Expr *ra = Constraint::Distance(workplane, point[0], point[1]);
            Expr *rb = Constraint::Distance(workplane, point[0], point[2]);
            AddEq(l, ra->Minus(rb), 0);
            break;
        }
        default:;
            // Most entities do not generate equations.
    }
}

