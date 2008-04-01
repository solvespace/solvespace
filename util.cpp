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

    n = n.ScaledBy(1/n.Magnitude());

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

void glVertex3v(Vector u)
{
    glVertex3f((GLfloat)u.x, (GLfloat)u.y, (GLfloat)u.z);
}
