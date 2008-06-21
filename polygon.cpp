#include "solvespace.h"

Vector STriangle::Normal(void) {
    Vector ab = b.Minus(a), bc = c.Minus(b);
    return ab.Cross(bc);
}

bool STriangle::ContainsPoint(Vector p) {
    Vector n = Normal();
    if(n.Magnitude() < LENGTH_EPS*LENGTH_EPS) {
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
}

STriangle STriangle::From(STriMeta meta, Vector a, Vector b, Vector c) {
    STriangle tr = { 0, meta, a, b, c };
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

bool SEdgeList::AssembleContour(Vector first, Vector last, 
                                    SContour *dest, SEdge *errorAt)
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
            if(se->b.Equals(last)) {
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

bool SEdgeList::AssemblePolygon(SPolygon *dest, SEdge *errorAt) {
    dest->Clear();

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
            return true;
        }

        // Create a new empty contour in our polygon, and finish assembling
        // into that contour.
        dest->AddEmptyContour();
        if(!AssembleContour(first, last, &(dest->l.elem[dest->l.n-1]), errorAt))
            return false;
    }
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
    int i;
    for(i = 0; i < (l.n / 2); i++) {
        int i2 = (l.n - 1) - i;
        SPoint t = l.elem[i2];
        l.elem[i2] = l.elem[i];
        l.elem[i] = t;
    }
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
    // Outside curve looks counterclockwise, projected against our normal.
    int i, j;
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l.elem[i]);
        if(sc->l.n < 1) continue;
        Vector pt = (sc->l.elem[0]).p;

        bool outer = true;
        for(j = 0; j < l.n; j++) {
            if(i == j) continue;
            SContour *sct = &(l.elem[j]);
            if(sct->ContainsPointProjdToNormal(normal, pt)) {
                outer = !outer;
            }
        }
   
        bool clockwise = sc->IsClockwiseProjdToNormal(normal);
        if(clockwise && outer || (!clockwise && !outer)) {
            sc->Reverse();
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

