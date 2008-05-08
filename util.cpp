#include "solvespace.h"

void MakeMatrix(double *mat, double a11, double a12, double a13, double a14,
                             double a21, double a22, double a23, double a24,
                             double a31, double a32, double a33, double a34,
                             double a41, double a42, double a43, double a44)
{
    mat[ 0] = a11;
    mat[ 1] = a21;
    mat[ 2] = a31;
    mat[ 3] = a41;
    mat[ 4] = a12;
    mat[ 5] = a22;
    mat[ 6] = a32;
    mat[ 7] = a42;
    mat[ 8] = a13;
    mat[ 9] = a23;
    mat[10] = a33;
    mat[11] = a43;
    mat[12] = a14;
    mat[13] = a24;
    mat[14] = a34;
    mat[15] = a44;
}

Quaternion Quaternion::MakeFrom(double w, double vx, double vy, double vz) {
    Quaternion q;
    q.w  = w;
    q.vx = vx;
    q.vy = vy;
    q.vz = vz;
    return q;
}

Quaternion Quaternion::MakeFrom(Vector u, Vector v)
{
    Vector n = u.Cross(v);

    Quaternion q;
    double s, tr = 1 + u.x + v.y + n.z;
    if(tr > 1e-4) {
        s = 2*sqrt(tr);
        q.w  = s/4;
        q.vx = (v.z - n.y)/s;
        q.vy = (n.x - u.z)/s;
        q.vz = (u.y - v.x)/s;
    } else {
        double m = max(u.x, max(v.y, n.z));
        if(m == u.x) {
            s = 2*sqrt(1 + u.x - v.y - n.z);
            q.w  = (v.z - n.y)/s;
            q.vx = s/4;
            q.vy = (u.y + v.x)/s;
            q.vz = (n.x + u.z)/s;
        } else if(m == v.y) {
            s = 2*sqrt(1 - u.x + v.y - n.z);
            q.w  = (n.x - u.z)/s;
            q.vx = (u.y + v.x)/s;
            q.vy = s/4;
            q.vz = (v.z + n.y)/s;
        } else if(m == n.z) {
            s = 2*sqrt(1 - u.x - v.y + n.z);
            q.w  = (u.y - v.x)/s;
            q.vx = (n.x + u.z)/s;
            q.vy = (v.z + n.y)/s;
            q.vz = s/4;
        } else oops();
    }

    return q.WithMagnitude(1);
}

Quaternion Quaternion::Plus(Quaternion b) {
    Quaternion q;
    q.w  = w  + b.w;
    q.vx = vx + b.vx;
    q.vy = vy + b.vy;
    q.vz = vz + b.vz;
    return q;
}

Quaternion Quaternion::Minus(Quaternion b) {
    Quaternion q;
    q.w  = w  - b.w;
    q.vx = vx - b.vx;
    q.vy = vy - b.vy;
    q.vz = vz - b.vz;
    return q;
}

Quaternion Quaternion::ScaledBy(double s) {
    Quaternion q;
    q.w  = w*s;
    q.vx = vx*s;
    q.vy = vy*s;
    q.vz = vz*s;
    return q;
}

double Quaternion::Magnitude(void) {
    return sqrt(w*w + vx*vx + vy*vy + vz*vz);
}

Quaternion Quaternion::WithMagnitude(double s) {
    return ScaledBy(s/Magnitude());
}

Vector Quaternion::RotationU(void) {
    Vector v;
    v.x = w*w + vx*vx - vy*vy - vz*vz;
    v.y = 2*w *vz + 2*vx*vy;
    v.z = 2*vx*vz - 2*w *vy;
    return v;
}

Vector Quaternion::RotationV(void) {
    Vector v;
    v.x = 2*vx*vy - 2*w*vz;
    v.y = w*w - vx*vx + vy*vy - vz*vz;
    v.z = 2*w*vx + 2*vy*vz;
    return v;
}

Vector Quaternion::RotationN(void) {
    return RotationU().Cross(RotationV());
}


Vector Vector::MakeFrom(double x, double y, double z) {
    Vector v;
    v.x = x; v.y = y; v.z = z;
    return v;
}

bool Vector::Equals(Vector v) {
    double tol = 0.1;
    if(fabs(x - v.x) > tol) return false;
    if(fabs(y - v.y) > tol) return false;
    if(fabs(z - v.z) > tol) return false;
    return true;
}

Vector Vector::Plus(Vector b) {
    Vector r;

    r.x = x + b.x;
    r.y = y + b.y;
    r.z = z + b.z;

    return r;
}

Vector Vector::Minus(Vector b) {
    Vector r;

    r.x = x - b.x;
    r.y = y - b.y;
    r.z = z - b.z;

    return r;
}

Vector Vector::Negated(void) {
    Vector r;

    r.x = -x;
    r.y = -y;
    r.z = -z;

    return r;
}

Vector Vector::Cross(Vector b) {
    Vector r;

    r.x = -(z*b.y) + (y*b.z);
    r.y =  (z*b.x) - (x*b.z);
    r.z = -(y*b.x) + (x*b.y);

    return r;
}

double Vector::Dot(Vector b) {
    return (x*b.x + y*b.y + z*b.z);
}

Vector Vector::Normal(int which) {
    Vector n;

    // Arbitrarily choose one vector that's normal to us, pivoting
    // appropriately.
    double xa = fabs(x), ya = fabs(y), za = fabs(z);
    double minc = min(min(xa, ya), za);
    if(minc == xa) {
        n.x = 0;
        n.y = z;
        n.z = -y;
    } else if(minc == ya) {
        n.y = 0;
        n.z = x;
        n.x = -z;
    } else if(minc == za) {
        n.z = 0;
        n.x = y;
        n.y = -x;
    } else oops();

    if(which == 0) {
        // That's the vector we return.
    } else if(which == 1) {
        n = this->Cross(n);
    } else oops();

    n = n.WithMagnitude(1);

    return n;
}

Vector Vector::RotatedAbout(Vector axis, double theta) {
    double c = cos(theta);
    double s = sin(theta);

    axis = axis.WithMagnitude(1);

    Vector r;

    r.x =   (x)*(c + (1 - c)*(axis.x)*(axis.x)) +
            (y)*((1 - c)*(axis.x)*(axis.y) - s*(axis.z)) +
            (z)*((1 - c)*(axis.x)*(axis.z) + s*(axis.y));

    r.y =   (x)*((1 - c)*(axis.y)*(axis.x) + s*(axis.z)) +
            (y)*(c + (1 - c)*(axis.y)*(axis.y)) +
            (z)*((1 - c)*(axis.y)*(axis.z) - s*(axis.x));

    r.z =   (x)*((1 - c)*(axis.z)*(axis.x) - s*(axis.y)) +
            (y)*((1 - c)*(axis.z)*(axis.y) + s*(axis.x)) +
            (z)*(c + (1 - c)*(axis.z)*(axis.z));

    return r;
}

double Vector::Magnitude(void) {
    return sqrt(x*x + y*y + z*z);
}

Vector Vector::ScaledBy(double v) {
    Vector r;

    r.x = x * v;
    r.y = y * v;
    r.z = z * v;

    return r;
}

Vector Vector::WithMagnitude(double v) {
    double m = Magnitude();
    if(m < 0.001) {
        return MakeFrom(v, 0, 0);
    } else {
        return ScaledBy(v/Magnitude());
    }
}

Vector Vector::ProjectInto(hEntity wrkpl) {
    Entity *w = SS.GetEntity(wrkpl);
    Vector u = w->Normal()->NormalU();
    Vector v = w->Normal()->NormalV();
    Vector p0 = w->WorkplaneGetOffset();

    Vector f = this->Minus(p0);
    double up = f.Dot(u);
    double vp = f.Dot(v);

    return p0.Plus((u.ScaledBy(up)).Plus(v.ScaledBy(vp)));
}

Point2d Point2d::Plus(Point2d b) {
    Point2d r;
    r.x = x + b.x;
    r.y = y + b.y;
    return r;
}

Point2d Point2d::Minus(Point2d b) {
    Point2d r;
    r.x = x - b.x;
    r.y = y - b.y;
    return r;
}

Point2d Point2d::ScaledBy(double s) {
    Point2d r;
    r.x = x*s;
    r.y = y*s;
    return r;
}

double Point2d::Magnitude(void) {
    return sqrt(x*x + y*y);
}

Point2d Point2d::WithMagnitude(double v) {
    double m = Magnitude();
    if(m < 0.001) {
        Point2d r = { v, 0 };
        return r;
    } else {
        return ScaledBy(v/Magnitude());
    }
}

double Point2d::DistanceTo(Point2d p) {
    double dx = x - p.x;
    double dy = y - p.y;
    return sqrt(dx*dx + dy*dy);
}

double Point2d::DistanceToLine(Point2d p0, Point2d dp, bool segment) {
    double m = dp.x*dp.x + dp.y*dp.y;
    if(m < 0.05) return 1e12;
   
    // Let our line be p = p0 + t*dp, for a scalar t from 0 to 1
    double t = (dp.x*(x - p0.x) + dp.y*(y - p0.y))/m;

    if((t < 0 || t > 1) && segment) {
        // The closest point is one of the endpoints; determine which.
        double d0 = DistanceTo(p0);
        double d1 = DistanceTo(p0.Plus(dp));

        return min(d1, d0);
    } else {
        Point2d closest = p0.Plus(dp.ScaledBy(t));
        return DistanceTo(closest);
    }
}


