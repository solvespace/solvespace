#include "solvespace.h"

void MakePathRelative(char *basep, char *pathp)
{
    int i;
    char *p;
    char base[MAX_PATH], path[MAX_PATH], out[MAX_PATH];

    // Convert everything to lowercase
    p = basep;
    for(i = 0; *p; p++) {
        base[i++] = tolower(*p);
    }
    base[i++] = '\0';
    p = pathp;
    for(i = 0; *p; p++) {
        path[i++] = tolower(*p);
    }
    path[i++] = '\0';

    // Find the length of the common prefix
    int com;
    for(com = 0; base[com] && path[com]; com++) {
        if(base[com] != path[com]) break;
    }
    if(!(base[com] && path[com])) return; // weird, prefix is entire string
    if(com == 0) return; // maybe on different drive letters?

    // Align the common prefix to the nearest slash; otherwise would break
    // on subdirectories or filenames that shared a prefix.
    while(com >= 1 && base[com-1] != '/' && base[com-1] != '\\') {
        com--;
    }
    if(com == 0) return;

    int sections = 0;
    int secLen = 0, secStart = 0;
    for(i = com; base[i]; i++) {
        if(base[i] == '/' || base[i] == '\\') {
            if(secLen == 2 && memcmp(base+secStart, "..", 2)==0) return;
            if(secLen == 1 && memcmp(base+secStart, ".", 1)==0) return;

            sections++;
            secLen = 0;
            secStart = i+1;
        } else {
            secLen++;
        }
    }

    // For every directory in the prefix of the base, we must go down a
    // directory in the relative path name
    strcpy(out, "");
    for(i = 0; i < sections; i++) {
        strcat(out, "../");
    }
    strcat(out, path+com);

    strcpy(pathp, out);
}

void MakePathAbsolute(char *basep, char *pathp) {
    char out[MAX_PATH];
    strcpy(out, basep);

    // Chop off the filename
    int i;
    for(i = strlen(out) - 1; i >= 0; i--) {
        if(out[i] == '\\' || out[i] == '/') break;
    }
    if(i < 0) return; // base is not an absolute path, or something?
    out[i+1] = '\0';

    strcat(out, pathp);
    GetAbsoluteFilename(out);

    strcpy(pathp, out);
}

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

Quaternion Quaternion::From(double w, double vx, double vy, double vz) {
    Quaternion q;
    q.w  = w;
    q.vx = vx;
    q.vy = vy;
    q.vz = vz;
    return q;
}

Quaternion Quaternion::From(hParam w, hParam vx, hParam vy, hParam vz) {
    Quaternion q;
    q.w  = SS.GetParam(w )->val;
    q.vx = SS.GetParam(vx)->val;
    q.vy = SS.GetParam(vy)->val;
    q.vz = SS.GetParam(vz)->val;
    return q;
}

Quaternion Quaternion::From(Vector u, Vector v)
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
        if(u.x > v.y && u.x > n.z) {
            s = 2*sqrt(1 + u.x - v.y - n.z);
            q.w  = (v.z - n.y)/s;
            q.vx = s/4;
            q.vy = (u.y + v.x)/s;
            q.vz = (n.x + u.z)/s;
        } else if(v.y > n.z) {
            s = 2*sqrt(1 - u.x + v.y - n.z);
            q.w  = (n.x - u.z)/s;
            q.vx = (u.y + v.x)/s;
            q.vy = s/4;
            q.vz = (v.z + n.y)/s;
        } else {
            s = 2*sqrt(1 - u.x - v.y + n.z);
            q.w  = (u.y - v.x)/s;
            q.vx = (n.x + u.z)/s;
            q.vy = (v.z + n.y)/s;
            q.vz = s/4;
        }
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
    Vector v;
    v.x = 2*w*vy + 2*vx*vz;
    v.y = 2*vy*vz - 2*w*vx;
    v.z = w*w - vx*vx - vy*vy + vz*vz;
    return v;
}

Vector Quaternion::Rotate(Vector p) {
    // Express the point in the new basis
    return (RotationU().ScaledBy(p.x)).Plus(
            RotationV().ScaledBy(p.y)).Plus(
            RotationN().ScaledBy(p.z));
}

Quaternion Quaternion::Inverse(void) {
    Quaternion r;
    r.w = w;
    r.vx = -vx;
    r.vy = -vy;
    r.vz = -vz;
    return r.WithMagnitude(1); // not that the normalize should be reqd
}

Quaternion Quaternion::ToThe(double p) {
    // Avoid division by zero, or arccos of something not in its domain
    if(w >= (1 - 1e-6)) {
        return From(1, 0, 0, 0);
    } else if(w <= (-1 + 1e-6)) {
        return From(-1, 0, 0, 0);
    }

    Quaternion r;
    Vector axis = Vector::From(vx, vy, vz);
    double theta = acos(w); // okay, since magnitude is 1, so -1 <= w <= 1
    theta *= p;
    r.w = cos(theta);
    axis = axis.WithMagnitude(sin(theta));
    r.vx = axis.x;
    r.vy = axis.y;
    r.vz = axis.z;
    return r;
}

Quaternion Quaternion::Times(Quaternion b) {
    double sa = w, sb = b.w;
    Vector va = { vx, vy, vz };
    Vector vb = { b.vx, b.vy, b.vz };

    Quaternion r;
    r.w = sa*sb - va.Dot(vb);
    Vector vr = vb.ScaledBy(sa).Plus(
                va.ScaledBy(sb).Plus(
                va.Cross(vb)));
    r.vx = vr.x;
    r.vy = vr.y;
    r.vz = vr.z;
    return r;
}


Vector Vector::From(double x, double y, double z) {
    Vector v;
    v.x = x; v.y = y; v.z = z;
    return v;
}

Vector Vector::From(hParam x, hParam y, hParam z) {
    Vector v;
    v.x = SS.GetParam(x)->val;
    v.y = SS.GetParam(y)->val;
    v.z = SS.GetParam(z)->val;
    return v;
}

double Vector::Element(int i) {
    switch(i) {
        case 0: return x;
        case 1: return y;
        case 2: return z;
        default: oops();
    }
}

bool Vector::Equals(Vector v, double tol) {
    // Quick axis-aligned tests before going further
    double dx = v.x - x; if(dx < -tol || dx > tol) return false;
    double dy = v.y - y; if(dy < -tol || dy > tol) return false;
    double dz = v.z - z; if(dz < -tol || dz > tol) return false;

    return (this->Minus(v)).MagSquared() < tol*tol;
}

bool Vector::EqualsExactly(Vector v) {
    return (x == v.x) &&
           (y == v.y) &&
           (z == v.z);
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
    if(this->Equals(Vector::From(0, 0, 1))) {
        // Make DXFs exported in the XY plane work nicely...
        n = Vector::From(1, 0, 0);
    } else if(xa < ya && xa < za) {
        n.x = 0;
        n.y = z;
        n.z = -y;
    } else if(ya < za) {
        n.x = -z;
        n.y = 0;
        n.z = x;
    } else {
        n.x = y;
        n.y = -x;
        n.z = 0;
    }

    if(which == 0) {
        // That's the vector we return.
    } else if(which == 1) {
        n = this->Cross(n);
    } else oops();

    n = n.WithMagnitude(1);

    return n;
}

Vector Vector::RotatedAbout(Vector orig, Vector axis, double theta) {
    Vector r = this->Minus(orig);
    r = r.RotatedAbout(axis, theta);
    return r.Plus(orig);
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

Vector Vector::DotInToCsys(Vector u, Vector v, Vector n) {
    Vector r = {
        this->Dot(u),
        this->Dot(v),
        this->Dot(n)
    };
    return r;
}

Vector Vector::ScaleOutOfCsys(Vector u, Vector v, Vector n) {
    Vector r = u.ScaledBy(x).Plus(
               v.ScaledBy(y).Plus(
               n.ScaledBy(z)));
    return r;
}

double Vector::DistanceToLine(Vector p0, Vector dp) {
    double m = dp.Magnitude();
    return ((this->Minus(p0)).Cross(dp)).Magnitude() / m;
}

bool Vector::OnLineSegment(Vector a, Vector b) {
    Vector d = b.Minus(a);

    double m = d.MagSquared();
    double distsq = ((this->Minus(a)).Cross(d)).MagSquared() / m;

    if(distsq >= LENGTH_EPS*LENGTH_EPS) return false;

    double t = (this->Minus(a)).DivPivoting(d);
    // On-endpoint must be tested for separately.
    if(t < 0 || t > 1) return false;
    return true;
}

Vector Vector::ClosestPointOnLine(Vector p0, Vector dp) {
    dp = dp.WithMagnitude(1);
    // this, p0, and (p0+dp) define a plane; the min distance is in
    // that plane, so calculate its normal
    Vector pn = (this->Minus(p0)).Cross(dp);
    // The minimum distance line is in that plane, perpendicular
    // to the line
    Vector n = pn.Cross(dp);

    // Calculate the actual distance
    double d = (dp.Cross(p0.Minus(*this))).Magnitude();
    return this->Plus(n.WithMagnitude(d));
}

double Vector::MagSquared(void) {
    return x*x + y*y + z*z;
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
    if(m == 0) {
        return From(v, 0, 0);
    } else {
        return ScaledBy(v/m);
    }
}

Vector Vector::ProjectVectorInto(hEntity wrkpl) {
    Entity *w = SS.GetEntity(wrkpl);
    Vector u = w->Normal()->NormalU();
    Vector v = w->Normal()->NormalV();

    double up = this->Dot(u);
    double vp = this->Dot(v);

    return (u.ScaledBy(up)).Plus(v.ScaledBy(vp));
}

Vector Vector::ProjectInto(hEntity wrkpl) {
    Entity *w = SS.GetEntity(wrkpl);
    Vector p0 = w->WorkplaneGetOffset();

    Vector f = this->Minus(p0);

    return p0.Plus(f.ProjectVectorInto(wrkpl));
}

Point2d Vector::Project2d(Vector u, Vector v) {
    Point2d p;
    p.x = this->Dot(u);
    p.y = this->Dot(v);
    return p;
}

Point2d Vector::ProjectXy(void) {
    Point2d p;
    p.x = x;
    p.y = y;
    return p;
}

double Vector::DivPivoting(Vector delta) {
    double mx = fabs(delta.x), my = fabs(delta.y), mz = fabs(delta.z);

    if(mx > my && mx > mz) {
        return x/delta.x;
    } else if(my > mz) {
        return y/delta.y;
    } else {
        return z/delta.z;
    }
}

Vector Vector::ClosestOrtho(void) {
    double mx = fabs(x), my = fabs(y), mz = fabs(z);

    if(mx > my && mx > mz) {
        return From((x > 0) ? 1 : -1, 0, 0);
    } else if(my > mz) {
        return From(0, (y > 0) ? 1 : -1, 0);
    } else {
        return From(0, 0, (z > 0) ? 1 : -1);
    }
}

void Vector::MakeMaxMin(Vector *maxv, Vector *minv) {
    maxv->x = max(maxv->x, x);
    maxv->y = max(maxv->y, y);
    maxv->z = max(maxv->z, z);

    minv->x = min(minv->x, x);
    minv->y = min(minv->y, y);
    minv->z = min(minv->z, z);
}

bool Vector::OutsideAndNotOn(Vector maxv, Vector minv) {
    return (x > maxv.x + LENGTH_EPS) || (x < minv.x - LENGTH_EPS) ||
           (y > maxv.y + LENGTH_EPS) || (y < minv.y - LENGTH_EPS) ||
           (z > maxv.z + LENGTH_EPS) || (z < minv.z - LENGTH_EPS);
}

bool Vector::BoundingBoxesDisjoint(Vector amax, Vector amin,
                                   Vector bmax, Vector bmin)
{
    int i;
    for(i = 0; i < 3; i++) {
        if(amax.Element(i) < bmin.Element(i) - LENGTH_EPS) return true;
        if(amin.Element(i) > bmax.Element(i) + LENGTH_EPS) return true;
    }
    return false;
}

Vector Vector::AtIntersectionOfPlanes(Vector n1, double d1,
                                      Vector n2, double d2)
{
    double det = (n1.Dot(n1))*(n2.Dot(n2)) - 
                 (n1.Dot(n2))*(n1.Dot(n2));
    double c1 = (d1*n2.Dot(n2) - d2*n1.Dot(n2))/det;
    double c2 = (d2*n1.Dot(n1) - d1*n1.Dot(n2))/det;
    
    return (n1.ScaledBy(c1)).Plus(n2.ScaledBy(c2));
}

Vector Vector::AtIntersectionOfLines(Vector a0, Vector a1,
                                     Vector b0, Vector b1,
                                     bool *skew,
                                     double *parama, double *paramb)
{
    Vector da = a1.Minus(a0), db = b1.Minus(b0);

    // Make an orthogonal coordinate system from those directions
    Vector dn = da.Cross(db); // normal to both
    Vector dna = dn.Cross(da); // normal to da
    Vector dnb = dn.Cross(db); // normal to db

    // At the intersection of the lines
    //    a0 + pa*da = b0 + pb*db (where pa, pb are scalar params)
    // So dot this equation against dna and dnb to get two equations
    // to solve for da and db
    double pb =  ((a0.Minus(b0)).Dot(dna))/(db.Dot(dna));
    double pa = -((a0.Minus(b0)).Dot(dnb))/(da.Dot(dnb));
    if(parama) *parama = pa;
    if(paramb) *paramb = pb;

    // And from either of those, we get the intersection point.
    Vector pi = a0.Plus(da.ScaledBy(pa));

    if(skew) {
        // Check if the intersection points on each line are actually
        // coincident...
        if(pi.Equals(b0.Plus(db.ScaledBy(pb)))) {
            *skew = false;
        } else {
            *skew = true;
        }
    }
    return pi;
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

double Point2d::Dot(Point2d p) {
    return x*p.x + y*p.y;
}

double Point2d::DistanceToLine(Point2d p0, Point2d dp, bool segment) {
    double m = dp.x*dp.x + dp.y*dp.y;
    if(m < LENGTH_EPS*LENGTH_EPS) return VERY_POSITIVE;
   
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

Point2d Point2d::Normal(void) {
    Point2d ret;
    ret.x = y;
    ret.y = -x;
    return ret;
}

