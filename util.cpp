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

Quaternion Quaternion::MakeFrom(double a, double b, double c, double d) {
    Quaternion q;
    q.a = a;
    q.b = b;
    q.c = c;
    q.d = d;
    return q;
}

Quaternion Quaternion::MakeFrom(Vector u, Vector v)
{
    Vector n = u.Cross(v);

    Quaternion q;
    q.a = 0.5*sqrt(1 + u.x + v.y + n.z);
    q.b = (1/(4*(q.a)))*(v.z - n.y);
    q.c = (1/(4*(q.a)))*(n.x - u.z);
    q.d = (1/(4*(q.a)))*(u.y - v.x);
    return q;
}

Quaternion Quaternion::Plus(Quaternion y) {
    Quaternion q;
    q.a = a + y.a;
    q.b = b + y.b;
    q.c = c + y.c;
    q.d = d + y.d;
    return q;
}

Quaternion Quaternion::Minus(Quaternion y) {
    Quaternion q;
    q.a = a - y.a;
    q.b = b - y.b;
    q.c = c - y.c;
    q.d = d - y.d;
    return q;
}

Quaternion Quaternion::ScaledBy(double s) {
    Quaternion q;
    q.a = a*s;
    q.b = b*s;
    q.c = c*s;
    q.d = d*s;
    return q;
}

double Quaternion::Magnitude(void) {
    return sqrt(a*a + b*b + c*c + d*d);
}

Quaternion Quaternion::WithMagnitude(double s) {
    return ScaledBy(s/Magnitude());
}

Vector Quaternion::RotationU(void) {
    Vector v;
    v.x = a*a + b*b - c*c - d*d;
    v.y = 2*a*d + 2*b*c;
    v.z = 2*b*d - 2*a*c;
    return v;
}

Vector Quaternion::RotationV(void) {
    Vector v;
    v.x = 2*b*c - 2*a*d;
    v.y = a*a - b*b + c*c - d*d;
    v.z = 2*a*b + 2*c*d;
    return v;
}


Vector Vector::MakeFrom(double x, double y, double z) {
    Vector v;
    v.x = x; v.y = y; v.z = z;
    return v;
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
    } else {
        oops();
    }

    if(which == 0) {
        // That's the vector we return.
    } else if(which == 1) {
        n = this->Cross(n);
    } else {
        oops();
    }

    n = n.WithMagnitude(1);

    return n;
}

Vector Vector::RotatedAbout(Vector axis, double theta) {
    double c = cos(theta);
    double s = sin(theta);

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

