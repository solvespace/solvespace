#include "solvespace.h"

Vector STriangle::Normal(void) {
    Vector ab = b.Minus(a), bc = c.Minus(b);
    return ab.Cross(bc);
}

double STriangle::MinAltitude(void) {
    double altA = a.DistanceToLine(b, c.Minus(b)),
           altB = b.DistanceToLine(c, a.Minus(c)),
           altC = c.DistanceToLine(a, b.Minus(a));

    return min(altA, min(altB, altC));
}

bool STriangle::ContainsPoint(Vector p) {
    Vector n = Normal();
    if(MinAltitude() < LENGTH_EPS) {
        // shouldn't happen; zero-area triangle
        return false;
    }
    return ContainsPointProjd(n.WithMagnitude(1), p);
}

bool STriangle::ContainsPointProjd(Vector n, Vector p) {
    Vector ab = b.Minus(a), bc = c.Minus(b), ca = a.Minus(c);

    Vector no_ab = n.Cross(ab);
    if(no_ab.Dot(p) < no_ab.Dot(a) - LENGTH_EPS) return false;

    Vector no_bc = n.Cross(bc);
    if(no_bc.Dot(p) < no_bc.Dot(b) - LENGTH_EPS) return false;

    Vector no_ca = n.Cross(ca);
    if(no_ca.Dot(p) < no_ca.Dot(c) - LENGTH_EPS) return false;

    return true;
}

void STriangle::FlipNormal(void) {
    SWAP(Vector, a, b);
    SWAP(Vector, an, bn);
}

STriangle STriangle::From(STriMeta meta, Vector a, Vector b, Vector c) {
    STriangle tr;
    ZERO(&tr);
    tr.meta = meta;
    tr.a = a;
    tr.b = b;
    tr.c = c;
    return tr;
}

SEdge SEdge::From(Vector a, Vector b) {
    SEdge se = { 0, a, b };
    return se;
}

void SEdgeList::Clear(void) {
    l.Clear();
}

void SEdgeList::AddEdge(Vector a, Vector b) {
    SEdge e; ZERO(&e);
    e.a = a;
    e.b = b;
    l.Add(&e);
}

bool SEdgeList::AssembleContour(Vector first, Vector last, SContour *dest,
                                SEdge *errorAt, bool keepDir)
{
    int i;

    dest->AddPoint(first);
    dest->AddPoint(last);

    do {
        for(i = 0; i < l.n; i++) {
            SEdge *se = &(l.elem[i]);
            if(se->tag) continue;

            if(se->a.Equals(last)) {
                dest->AddPoint(se->b);
                last = se->b;
                se->tag = 1;
                break;
            }
            // Don't allow backwards edges if keepDir is true.
            if(!keepDir && se->b.Equals(last)) {
                dest->AddPoint(se->a);
                last = se->a;
                se->tag = 1;
                break;
            }
        }
        if(i >= l.n) {
            // Couldn't assemble a closed contour; mark where.
            if(errorAt) {
                errorAt->a = first;
                errorAt->b = last;
            }
            return false;
        }

    } while(!last.Equals(first));

    return true;
}

bool SEdgeList::AssemblePolygon(SPolygon *dest, SEdge *errorAt, bool keepDir) {
    dest->Clear();

    bool allClosed = true;
    for(;;) {
        Vector first, last;
        int i;
        for(i = 0; i < l.n; i++) {
            if(!l.elem[i].tag) {
                first = l.elem[i].a;
                last = l.elem[i].b;
                l.elem[i].tag = 1;
                break;
            }
        }
        if(i >= l.n) {
            return allClosed;
        }

        // Create a new empty contour in our polygon, and finish assembling
        // into that contour.
        dest->AddEmptyContour();
        if(!AssembleContour(first, last, &(dest->l.elem[dest->l.n-1]),
                errorAt, keepDir))
        {
            allClosed = false;
        }
        // But continue assembling, even if some of the contours are open
    }
}

//-----------------------------------------------------------------------------
// Test if the specified edge crosses any of the edges in our list. Two edges
// are not considered to cross if they share an endpoint (within LENGTH_EPS),
// but they are considered to cross if they are coincident and overlapping.
// If pi is not NULL, then a crossing is returned in that.
//-----------------------------------------------------------------------------
int SEdgeList::AnyEdgeCrossings(Vector a, Vector b, Vector *ppi) {
    Vector d = b.Minus(a);
    double t_eps = LENGTH_EPS/d.Magnitude();

    int cnt = 0;
    SEdge *se;
    for(se = l.First(); se; se = l.NextAfter(se)) {
        double dist_a, dist_b;
        double t, tse; 
        bool skew;
        Vector pi;
        bool inOrEdge0, inOrEdge1;

        Vector dse = (se->b).Minus(se->a);
        double tse_eps = LENGTH_EPS/dse.Magnitude();

        if(a.Equals(se->a) && b.Equals(se->b)) goto intersects;
        if(b.Equals(se->a) && a.Equals(se->b)) goto intersects;

        dist_a = (se->a).DistanceToLine(a, d),
        dist_b = (se->b).DistanceToLine(a, d);

        if(fabs(dist_a - dist_b) < LENGTH_EPS) {
            // The edges are parallel.
            if(fabs(dist_a) > LENGTH_EPS) {
                // and not coincident, so can't be interesecting
                continue;
            }
            // The edges are coincident. Make sure that neither endpoint lies
            // on the other
            double t;
            t = ((se->a).Minus(a)).DivPivoting(d);
            if(t > t_eps && t < (1 - t_eps)) goto intersects;
            t = ((se->b).Minus(a)).DivPivoting(d);
            if(t > t_eps && t < (1 - t_eps)) goto intersects;
            t = a.Minus(se->a).DivPivoting(dse);
            if(t > tse_eps && t < (1 - tse_eps)) goto intersects;
            t = b.Minus(se->a).DivPivoting(dse);
            if(t > tse_eps && t < (1 - tse_eps)) goto intersects;
            // So coincident but disjoint, okay.
            continue;
        }

        // Lines are not parallel, so look for an intersection.
        pi = Vector::AtIntersectionOfLines(a, b, se->a, se->b,
                                           &skew,
                                           &t, &tse);
        if(skew) continue;

        inOrEdge0 = (t   >   -t_eps) && (t <   (1 +   t_eps));
        inOrEdge1 = (tse > -tse_eps) && (tse < (1 + tse_eps));

        if(inOrEdge0 && inOrEdge1) {
            if((se->a).Equals(a) || (se->b).Equals(a) ||
               (se->a).Equals(b) || (se->b).Equals(b))
            {
                // Not an intersection if we share an endpoint with an edge
                continue;
            }
            // But it's an intersection if a vertex of one edge lies on the
            // inside of the other (or if they cross away from either's
            // vertex).
            if(ppi) *ppi = pi;
            goto intersects;
        }
        continue;

intersects:
        cnt++;
        // and continue with the loop
    }
    return cnt;
}

void SContour::AddPoint(Vector p) {
    SPoint sp;
    sp.tag = 0;
    sp.p = p;

    l.Add(&sp);
}

void SContour::MakeEdgesInto(SEdgeList *el) {
    int i;
    for(i = 0; i < (l.n-1); i++) {
        SEdge e;
        e.tag = 0;
        e.a = l.elem[i].p;
        e.b = l.elem[i+1].p;
        el->l.Add(&e);
    }
}

void SContour::CopyInto(SContour *dest) {
    SPoint *sp;
    for(sp = l.First(); sp; sp = l.NextAfter(sp)) {
        dest->AddPoint(sp->p);
    }
}

void SContour::FindPointWithMinX(void) {
    SPoint *sp;
    xminPt = Vector::From(1e10, 1e10, 1e10);
    for(sp = l.First(); sp; sp = l.NextAfter(sp)) {
        if(sp->p.x < xminPt.x) {
            xminPt = sp->p;
        }
    }
}

Vector SContour::ComputeNormal(void) {
    Vector n = Vector::From(0, 0, 0);

    for(int i = 0; i < l.n - 2; i++) {
        Vector u = (l.elem[i+1].p).Minus(l.elem[i+0].p).WithMagnitude(1);
        Vector v = (l.elem[i+2].p).Minus(l.elem[i+1].p).WithMagnitude(1);
        Vector nt = u.Cross(v);
        if(nt.Magnitude() > n.Magnitude()) {
            n = nt;
        }
    }
    return n.WithMagnitude(1);
}

bool SContour::IsClockwiseProjdToNormal(Vector n) {
    // Degenerate things might happen as we draw; doesn't really matter
    // what we do then.
    if(n.Magnitude() < 0.01) return true;

    // An arbitrary 2d coordinate system that has n as its normal
    Vector u = n.Normal(0);
    Vector v = n.Normal(1);

    double area = 0;
    for(int i = 0; i < (l.n - 1); i++) {
        double u0 = (l.elem[i  ].p).Dot(u);
        double v0 = (l.elem[i  ].p).Dot(v);
        double u1 = (l.elem[i+1].p).Dot(u);
        double v1 = (l.elem[i+1].p).Dot(v);

        area += ((v0 + v1)/2)*(u1 - u0);
    }
    return (area < 0);
}

bool SContour::ContainsPointProjdToNormal(Vector n, Vector p) {
    Vector u = n.Normal(0);
    Vector v = n.Normal(1);

    double up = p.Dot(u);
    double vp = p.Dot(v);

    bool inside = false;
    for(int i = 0; i < (l.n - 1); i++) {
        double ua = (l.elem[i  ].p).Dot(u);
        double va = (l.elem[i  ].p).Dot(v);
        // The curve needs to be exactly closed; approximation is death.
        double ub = (l.elem[(i+1)%(l.n-1)].p).Dot(u);
        double vb = (l.elem[(i+1)%(l.n-1)].p).Dot(v);

        if ((((va <= vp) && (vp < vb)) ||
             ((vb <= vp) && (vp < va))) &&
            (up < (ub - ua) * (vp - va) / (vb - va) + ua))
        {
          inside = !inside;
        }
    }

    return inside;
}

bool SContour::AllPointsInPlane(Vector n, double d, Vector *notCoplanarAt) {
    for(int i = 0; i < l.n; i++) {
        Vector p = l.elem[i].p;
        double dd = n.Dot(p) - d;
        if(fabs(dd) > 10*LENGTH_EPS) {
            *notCoplanarAt = p;
            return false;
        }
    }
    return true;
}

void SContour::Reverse(void) {
    l.Reverse();
}


void SPolygon::Clear(void) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).l.Clear();
    }
    l.Clear();
}

void SPolygon::AddEmptyContour(void) {
    SContour c;
    memset(&c, 0, sizeof(c));
    l.Add(&c);
}

void SPolygon::MakeEdgesInto(SEdgeList *el) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).MakeEdgesInto(el);
    }
}

Vector SPolygon::ComputeNormal(void) {
    if(l.n < 1) return Vector::From(0, 0, 0);
    return (l.elem[0]).ComputeNormal();
}

bool SPolygon::ContainsPoint(Vector p) {
    return (WindingNumberForPoint(p) % 2) == 1;
}

int SPolygon::WindingNumberForPoint(Vector p) {
    int winding = 0;
    int i;
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l.elem[i]);
        if(sc->ContainsPointProjdToNormal(normal, p)) {
            winding++;
        }
    }
    return winding;
}

void SPolygon::FixContourDirections(void) {
    // At output, the contour's tag will be 1 if we reversed it, else 0.
    l.ClearTags();

    // Outside curve looks counterclockwise, projected against our normal.
    int i, j;
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l.elem[i]);
        if(sc->l.n < 1) continue;
        Vector pt = (sc->l.elem[0]).p;

        sc->timesEnclosed = 0;
        bool outer = true;
        for(j = 0; j < l.n; j++) {
            if(i == j) continue;
            SContour *sct = &(l.elem[j]);
            if(sct->ContainsPointProjdToNormal(normal, pt)) {
                outer = !outer;
                (sc->timesEnclosed)++;
            }
        }
   
        bool clockwise = sc->IsClockwiseProjdToNormal(normal);
        if(clockwise && outer || (!clockwise && !outer)) {
            sc->Reverse();
            sc->tag = 1;
        }
    }
}

bool SPolygon::IsEmpty(void) {
    if(l.n == 0 || l.elem[0].l.n == 0) return true;
    return false;
}

Vector SPolygon::AnyPoint(void) {
    if(IsEmpty()) oops();
    return l.elem[0].l.elem[0].p;
}

bool SPolygon::AllPointsInPlane(Vector *notCoplanarAt) {
    if(IsEmpty()) return true;

    Vector p0 = AnyPoint();
    double d = normal.Dot(p0);

    for(int i = 0; i < l.n; i++) {
        if(!(l.elem[i]).AllPointsInPlane(normal, d, notCoplanarAt)) {
            return false;
        }
    }
    return true;
}

bool SPolygon::SelfIntersecting(Vector *intersectsAt) {
    SEdgeList el;
    ZERO(&el);
    MakeEdgesInto(&el);

    SEdge *se;
    for(se = el.l.First(); se; se = el.l.NextAfter(se)) {
        int inters = el.AnyEdgeCrossings(se->a, se->b, intersectsAt);
        if(inters != 1) return true;
    }
    return false;
}

static int TriMode, TriVertexCount;
static Vector Tri1, TriNMinus1, TriNMinus2;
static Vector TriNormal;
static SMesh *TriMesh;
static STriMeta TriMeta;
static void GLX_CALLBACK TriBegin(int mode) 
{
    TriMode = mode;
    TriVertexCount = 0;
}
static void GLX_CALLBACK TriEnd(void)
{
}
static void GLX_CALLBACK TriVertex(Vector *triN)
{
    if(TriVertexCount == 0) {
        Tri1 = *triN;
    }
    if(TriMode == GL_TRIANGLES) {
        if((TriVertexCount % 3) == 2) {
            TriMesh->AddTriangle(
                TriMeta, TriNormal, TriNMinus2, TriNMinus1, *triN);
        }
    } else if(TriMode == GL_TRIANGLE_FAN) {
        if(TriVertexCount >= 2) {
            TriMesh->AddTriangle(
                TriMeta, TriNormal, Tri1, TriNMinus1, *triN);
        }
    } else if(TriMode == GL_TRIANGLE_STRIP) {
        if(TriVertexCount >= 2) {
            TriMesh->AddTriangle(
                TriMeta, TriNormal, TriNMinus2, TriNMinus1, *triN);
        }
    } else oops();
            
    TriNMinus2 = TriNMinus1;
    TriNMinus1 = *triN;
    TriVertexCount++;
}
void SPolygon::TriangulateInto(SMesh *m) {
    STriMeta meta;
    ZERO(&meta);
    TriangulateInto(m, meta);
}
void SPolygon::TriangulateInto(SMesh *m, STriMeta meta) {
    TriMesh = m;
    TriNormal = normal;
    TriMeta = meta;

    GLUtesselator *gt = gluNewTess();
    gluTessCallback(gt, GLU_TESS_BEGIN, (glxCallbackFptr *)TriBegin);
    gluTessCallback(gt, GLU_TESS_END, (glxCallbackFptr *)TriEnd);
    gluTessCallback(gt, GLU_TESS_VERTEX, (glxCallbackFptr *)TriVertex);

    glxTesselatePolygon(gt, this);

    gluDeleteTess(gt);
}

//-----------------------------------------------------------------------------
// Low-quality routines to cutter radius compensate a polygon. Assumes the
// polygon is in the xy plane, and the contours all go in the right direction
// with respect to normal (0, 0, -1).
//-----------------------------------------------------------------------------
void SPolygon::OffsetInto(SPolygon *dest, double r) {
    int i;
    dest->Clear();
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l.elem[i]);
        dest->AddEmptyContour();
        sc->OffsetInto(&(dest->l.elem[dest->l.n-1]), r);
    }
}
//-----------------------------------------------------------------------------
// Calculate the intersection point of two lines. Returns true for success,
// false if they're parallel.
//-----------------------------------------------------------------------------
static bool IntersectionOfLines(double x0A, double y0A, double dxA, double dyA,
                                double x0B, double y0B, double dxB, double dyB,
                                double *xi, double *yi)
{
    double A[2][2];
    double b[2];

    // The line is given to us in the form:
    //    (x(t), y(t)) = (x0, y0) + t*(dx, dy)
    // so first rewrite it as
    //    (x - x0, y - y0) dot (dy, -dx) = 0
    //    x*dy - x0*dy - y*dx + y0*dx = 0
    //    x*dy - y*dx = (x0*dy - y0*dx)

    // So write the matrix, pre-pivoted.
    if(fabs(dyA) > fabs(dyB)) {
        A[0][0] = dyA;  A[0][1] = -dxA;  b[0] = x0A*dyA - y0A*dxA;
        A[1][0] = dyB;  A[1][1] = -dxB;  b[1] = x0B*dyB - y0B*dxB;
    } else {
        A[1][0] = dyA;  A[1][1] = -dxA;  b[1] = x0A*dyA - y0A*dxA;
        A[0][0] = dyB;  A[0][1] = -dxB;  b[0] = x0B*dyB - y0B*dxB;
    }

    // Check the determinant; if it's zero then no solution.
    if(fabs(A[0][0]*A[1][1] - A[0][1]*A[1][0]) < LENGTH_EPS) {
        return false;
    }
    
    // Solve
    double v = A[1][0] / A[0][0];
    A[1][0] -= A[0][0]*v;
    A[1][1] -= A[0][1]*v;
    b[1] -= b[0]*v;

    // Back-substitute.
    *yi = b[1] / A[1][1];
    *xi = (b[0] - A[0][1]*(*yi)) / A[0][0];

    return true;
}
void SContour::OffsetInto(SContour *dest, double r) {
    int i;

    for(i = 0; i < l.n; i++) {
        Vector a, b, c;
        Vector dp, dn;
        double thetan, thetap;

        a = l.elem[WRAP(i-1, (l.n-1))].p;
        b = l.elem[WRAP(i,   (l.n-1))].p;
        c = l.elem[WRAP(i+1, (l.n-1))].p;

        dp = a.Minus(b);
        thetap = atan2(dp.y, dp.x);

        dn = b.Minus(c);
        thetan = atan2(dn.y, dn.x);

        // A short line segment in a badly-generated polygon might look
        // okay but screw up our sense of direction.
        if(dp.Magnitude() < LENGTH_EPS || dn.Magnitude() < LENGTH_EPS) {
            continue;
        }

        if(thetan > thetap && (thetan - thetap) > PI) {
            thetap += 2*PI;
        }
        if(thetan < thetap && (thetap - thetan) > PI) {
            thetan += 2*PI;
        }
    
        if(fabs(thetan - thetap) < (1*PI)/180) {
            Vector p = { b.x - r*sin(thetap), b.y + r*cos(thetap) };
            dest->AddPoint(p);
        } else if(thetan < thetap) {
            // This is an inside corner. We have two edges, Ep and En. Move
            // out from their intersection by radius, normal to En, and
            // then draw a line parallel to En. Move out from their
            // intersection by radius, normal to Ep, and then draw a second
            // line parallel to Ep. The point that we want to generate is
            // the intersection of these two lines--it removes as much
            // material as we can without removing any that we shouldn't.
            double px0, py0, pdx, pdy;
            double nx0, ny0, ndx, ndy;
            double x, y;

            px0 = b.x - r*sin(thetap);
            py0 = b.y + r*cos(thetap);
            pdx = cos(thetap);
            pdy = sin(thetap);

            nx0 = b.x - r*sin(thetan);
            ny0 = b.y + r*cos(thetan);
            ndx = cos(thetan);
            ndy = sin(thetan);

            IntersectionOfLines(px0, py0, pdx, pdy, 
                                nx0, ny0, ndx, ndy,
                                &x, &y);

            dest->AddPoint(Vector::From(x, y, 0));
        } else {
            if(fabs(thetap - thetan) < (6*PI)/180) {
                Vector pp = { b.x - r*sin(thetap),
                              b.y + r*cos(thetap), 0 };
                dest->AddPoint(pp);

                Vector pn = { b.x - r*sin(thetan),
                              b.y + r*cos(thetan), 0 };
                dest->AddPoint(pn);
            } else {
                double theta;
                for(theta = thetap; theta <= thetan; theta += (6*PI)/180) {
                    Vector p = { b.x - r*sin(theta),
                                 b.y + r*cos(theta), 0 };
                    dest->AddPoint(p);
                }
            }
        }
    }
}

