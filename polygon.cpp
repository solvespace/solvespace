#include "solvespace.h"

Vector STriangle::Normal(void) {
    Vector ab = b.Minus(a), bc = c.Minus(b);
    return ab.Cross(bc);
}

bool STriangle::ContainsPoint(Vector p) {
    Vector ab = b.Minus(a), bc = c.Minus(b), ca = a.Minus(c);
    Vector n = ab.Cross(bc);
    n = n.WithMagnitude(1);

    Vector no_ab = n.Cross(ab);
    if(no_ab.Dot(p) < no_ab.Dot(a) - LENGTH_EPS) return false;

    Vector no_bc = n.Cross(bc);
    if(no_bc.Dot(p) < no_bc.Dot(b) - LENGTH_EPS) return false;

    Vector no_ca = n.Cross(ca);
    if(no_ca.Dot(p) < no_ca.Dot(c) - LENGTH_EPS) return false;

    return true;
}

STriangle STriangle::From(STriMeta meta, Vector a, Vector b, Vector c) {
    STriangle tr = { 0, meta, a, b, c };
    return tr;
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

        dest->AddEmptyContour();
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
    }
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
    Vector n = Vector::MakeFrom(0, 0, 0);

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

void SPolygon::AddPoint(Vector p) {
    if(l.n < 1) oops();

    SPoint sp;
    sp.tag = 0;
    sp.p = p;

    // Add to the last contour in the list
    (l.elem[l.n-1]).l.Add(&sp);
}

void SPolygon::MakeEdgesInto(SEdgeList *el) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).MakeEdgesInto(el);
    }
}

Vector SPolygon::ComputeNormal(void) {
    if(l.n < 1) return Vector::MakeFrom(0, 0, 0);
    return (l.elem[0]).ComputeNormal();
}

bool SPolygon::ContainsPoint(Vector p) {
    bool inside = false;
    int i;
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l.elem[i]);
        if(sc->ContainsPointProjdToNormal(normal, p)) {
            inside = !inside;
        }
    }
    return inside;
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

static int TriMode, TriVertexCount;
static Vector Tri1, TriNMinus1, TriNMinus2;
static Vector TriNormal;
static SMesh *TriMesh;
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
            TriMesh->AddTriangle(TriNormal, TriNMinus2, TriNMinus1, *triN);
        }
    } else if(TriMode == GL_TRIANGLE_FAN) {
        if(TriVertexCount >= 2) {
            TriMesh->AddTriangle(TriNormal, Tri1, TriNMinus1, *triN);
        }
    } else if(TriMode == GL_TRIANGLE_STRIP) {
        if(TriVertexCount >= 2) {
            TriMesh->AddTriangle(TriNormal, TriNMinus2, TriNMinus1, *triN);
        }
    } else oops();
            
    TriNMinus2 = TriNMinus1;
    TriNMinus1 = *triN;
    TriVertexCount++;
}
void SPolygon::TriangulateInto(SMesh *m) {
    TriMesh = m;
    TriNormal = normal;

    GLUtesselator *gt = gluNewTess();
    gluTessCallback(gt, GLU_TESS_BEGIN, (glxCallbackFptr *)TriBegin);
    gluTessCallback(gt, GLU_TESS_END, (glxCallbackFptr *)TriEnd);
    gluTessCallback(gt, GLU_TESS_VERTEX, (glxCallbackFptr *)TriVertex);

    glxTesselatePolygon(gt, this);

    gluDeleteTess(gt);
}

