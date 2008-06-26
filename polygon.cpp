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

bool SPolygon::IntersectsPolygon(Vector ga, Vector gb) {
    int i, j;
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l.elem[i]);
        for(j = 0; j < sc->l.n; j++) {
            Vector pa = sc->l.elem[j].p,
                   pb = sc->l.elem[WRAP(j+1, (sc->l.n - 1))].p;
            // So do the lines from (ga to gb) and (pa to pb) intersect?
            Vector dp = pb.Minus(pa),    dg = gb.Minus(ga);
            double tp, tg;
            if(!Vector::LinesIntersect(pa, dp, ga, dg, &tp, &tg)) {
                continue;
            }

            // So check if the line segments intersect
            double lp = dp.Magnitude(),  lg = dg.Magnitude();
            tp *= lp;
            tg *= lg;

            if(tg > LENGTH_EPS && tg < (lg - LENGTH_EPS) &&
               tp > LENGTH_EPS && tp < (lp - LENGTH_EPS))
            {
                return true;
            }
        }
    }
    return false;
}

bool SPolygon::VisibleVertices(SContour *outer, SContour *inner,
                               SEdgeList *extras, int *vo, int *vi)
{
    int i, j, k;

    double lmin = 1e12;
    for(i = 0; i < outer->l.n; i++) {
        Vector op = outer->l.elem[i].p;

        for(j = 0; j < inner->l.n; j++) {
            Vector ip = inner->l.elem[j].p;

            if(IntersectsPolygon(op, ip)) goto dontuse;

            for(k = 0; k < extras->l.n; k++) {
                SEdge *se = &(extras->l.elem[k]);

                if(ip.Equals(se->a) || ip.Equals(se->b) ||
                   op.Equals(se->a) || op.Equals(se->b))
                {
                    goto dontuse;
                }
                
                Vector dt = ip.Minus(op), de = (se->b).Minus(se->a);
                double te, tt;
                if(!Vector::LinesIntersect(op, dt, se->a, de, &tt, &te))
                    continue;

                double le = de.Magnitude(), lt = dt.Magnitude();
                tt *= lt;
                te *= le;
                if(tt > LENGTH_EPS && tt < (lt - LENGTH_EPS) &&
                   te > LENGTH_EPS && te < (le - LENGTH_EPS))
                {
                    goto dontuse;
                }
            }

            if((op.Minus(ip)).Magnitude() < lmin) {
                lmin = (op.Minus(ip)).Magnitude();
                *vo = i;
                *vi = j;
            }
dontuse:;
        }
    }
    if(lmin < 1e12) {
        return true;
    } else {
        return false;
    }
}

bool SContour::VertexIsEar(int v, Vector normal) {
    int   va = WRAP(v-1, l.n), vb = WRAP(v  , l.n), vc = WRAP(v+1, l.n);
    Vector a = l.elem[va].p,    b = l.elem[vb].p,    c = l.elem[vc].p;

    STriMeta meta;
    ZERO(&meta);
    STriangle tr = STriangle::From(meta, a, b, c);
    if(normal.Dot(tr.Normal()) > 0) return false;
    
    int i;
    for(i = 0; i < l.n; i++) {
        if(i == va) continue;
        if(i == vb) continue;
        if(i == vc) continue;

        Vector p = l.elem[i].p;
        if(p.Equals(tr.a)) continue;
        if(p.Equals(tr.b)) continue;
        if(p.Equals(tr.c)) continue;

        if(tr.ContainsPoint(p)) return false;
    }
    return true;
}

void SContour::TriangulateInto(SMesh *m, STriMeta meta, Vector normal) {
    int i;

    bool odd = false;
    while(l.n >= 3) {
        int start, end, incr;
        if(odd) {
            start = 0; end = l.n; incr = 1;
        } else {
            start = l.n - 1; end = -1; incr = -1;
        }
        for(i = start; i != end; i += incr) {
            if(VertexIsEar(i, normal)) {
                break;
            }
        }
        if(i == end) {
            dbp("couldn't find ear!");
            return;
        }

        Vector a = l.elem[WRAP(i-1, l.n)].p,
               b = l.elem[WRAP(i  , l.n)].p,
               c = l.elem[WRAP(i+1, l.n)].p;
        m->AddTriangle(meta, c, b, a);
        l.ClearTags();
        l.elem[i].tag = 1;
        l.RemoveTagged();

        odd = !odd;
    }

    l.Clear();
}

void SPolygon::TriangulateInto(SMesh *m) {
    STriMeta meta;
    ZERO(&meta);
    TriangulateInto(m, meta);
}
void SPolygon::TriangulateInto(SMesh *m, STriMeta meta) {
    FixContourDirections();

    int i, j, k;
    bool *used = (bool *)AllocTemporary(l.n*sizeof(bool));
    int *winding = (int *)AllocTemporary(l.n*sizeof(int));
    bool **contained = (bool **)AllocTemporary(l.n*sizeof(bool *));
    for(i = 0; i < l.n; i++) {
        contained[i] = (bool *)AllocTemporary(l.n*sizeof(bool));

        SContour *sci = &(l.elem[i]);
        if(sci->l.n < 1) continue;
        for(j = 0; j < l.n; j++) {
            SContour *scj = &(l.elem[j]);
            if(scj->l.n < 1) continue;
            if(i == j) {
                contained[i][j] = true;
                continue;
            }
            
            if(scj->ContainsPointProjdToNormal(normal, sci->l.elem[0].p)) {
                (winding[i])++;
                contained[i][j] = true;
            }
        }
    }

    for(;;) {
        for(i = 0; i < l.n; i++) {
            if(winding[i] == 0) break;
        }
        if(i >= l.n) {
            // No outer contours left, so we're done
            break;
        }

        SContour *outer = &(l.elem[i]);
        SContour merged;
        ZERO(&merged);

        SEdgeList extras;
        ZERO(&extras);

        for(j = 0; j < outer->l.n - 1; j++) {
            merged.AddPoint(outer->l.elem[j].p);
        }
        // If this polygon has holes, then we must merge them in.
        for(;;) {
            for(j = 0; j < l.n; j++) {
                if(used[j]) continue;
                if(winding[j] != winding[i] + 1) continue;
                if(!contained[j][i]) continue;

                SContour *inner = &(l.elem[j]);

                int vinner, vouter;
                if(VisibleVertices(&merged, inner, &extras, &vouter, &vinner)) {
                    used[j] = true;

                    SEdge se = 
                        { 0, merged.l.elem[vouter].p, inner->l.elem[vinner].p };
                    extras.l.Add(&se);
                    
                    SContour alt;
                    ZERO(&alt);
                    for(k = 0; k <= vouter; k++) {
                        alt.AddPoint(merged.l.elem[k].p);
                    }
                    for(k = 0; k <= inner->l.n - 1; k++) {
                        int v = WRAP(k + vinner, inner->l.n - 1);
                        alt.AddPoint(inner->l.elem[v].p);
                    }
                    for(k = vouter; k < merged.l.n; k++) {
                        alt.AddPoint(merged.l.elem[k].p);
                    }
                    merged.l.Clear();
                    merged = alt;
                    break;
                }
            }
            if(j >= l.n) {
                break;
            }
        }

        merged.TriangulateInto(m, meta, normal);
        merged.l.Clear();
        extras.Clear();

        for(j = 0; j < l.n; j++) {
            if(contained[j][i]) winding[j] -= 2;
        }
        used[i] = true;
    }
}

