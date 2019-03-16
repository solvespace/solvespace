//-----------------------------------------------------------------------------
// Operations on polygons (planar, of line segment edges).
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

Vector STriangle::Normal() const {
    const Vector ab = b.Minus(a);
    const Vector bc = c.Minus(b);
    return ab.Cross(bc);
}

double STriangle::MinAltitude() const {
    const double altA = a.DistanceToLine(b, c.Minus(b));
    const double altB = b.DistanceToLine(c, a.Minus(c));
    const double altC = c.DistanceToLine(a, b.Minus(a));

    return min(altA, min(altB, altC));
}

bool STriangle::ContainsPoint(const Vector& p) const {
    const Vector n = Normal();
    if(MinAltitude() < LENGTH_EPS) {
        // shouldn't happen; zero-area triangle
        return false;
    }
    return ContainsPointProjd(n.WithMagnitude(1), p);
}

bool STriangle::ContainsPointProjd(const Vector& n, const Vector& p) const {
    const Vector ab = b.Minus(a);
    const Vector bc = c.Minus(b);
    const Vector ca = a.Minus(c);

    const Vector no_ab = n.Cross(ab);
    if(no_ab.Dot(p) < no_ab.Dot(a) - LENGTH_EPS) {
        return false;
    }

    const Vector no_bc = n.Cross(bc);
    if(no_bc.Dot(p) < no_bc.Dot(b) - LENGTH_EPS) {
        return false;
    }

    const Vector no_ca = n.Cross(ca);
    if(no_ca.Dot(p) < no_ca.Dot(c) - LENGTH_EPS) {
        return false;
    }

    return true;
}

bool STriangle::Raytrace(const Vector &rayPoint, const Vector &rayDir,
                         double *t, Vector *inters) const {
    // Algorithm from: "Fast, Minimum Storage Ray/Triangle Intersection" by
    // Tomas Moeller and Ben Trumbore.

    // Find vectors for two edges sharing vertex A.
    const Vector edge1 = b.Minus(a);
    const Vector edge2 = c.Minus(a);

    // Begin calculating determinant - also used to calculate U parameter.
    const Vector pvec = rayDir.Cross(edge2);

    // If determinant is near zero, ray lies in plane of triangle.
    // Also, cull back facing triangles here.
    const double det = edge1.Dot(pvec);
    if(-det < LENGTH_EPS) {
        return false;
    }
    const double inv_det = 1.0f / det;

    // Calculate distance from vertex A to ray origin.
    const Vector tvec = rayPoint.Minus(a);

    // Calculate U parameter and test bounds.
    const double u = tvec.Dot(pvec) * inv_det;
    if(u < 0.0f || u > 1.0f) {
        return false;
    }

    // Prepare to test V parameter.
    const Vector qvec = tvec.Cross(edge1);

    // Calculate V parameter and test bounds.
    const double v = rayDir.Dot(qvec) * inv_det;
    if(v < 0.0f || u + v > 1.0f) {
        return false;
    }

    // Calculate t, ray intersects triangle.
    *t = edge2.Dot(qvec) * inv_det;

    // Calculate intersection point.
    if(inters != NULL) {
        *inters = rayPoint.Plus(rayDir.ScaledBy(*t));
    }

    return true;
}

double STriangle::SignedVolume() const {
    return a.Dot(b.Cross(c)) / 6.0;
}

bool STriangle::IsDegenerate() const {
    return a.OnLineSegment(b, c) ||
           b.OnLineSegment(a, c) ||
           c.OnLineSegment(a, b);
}

void STriangle::FlipNormal() {
    swap(a, b);
    swap(an, bn);
}

STriangle STriangle::Transform(const Vector& u, const Vector& v, const Vector& n) const {
    STriangle tr = *this;
    tr.a  = tr.a.ScaleOutOfCsys(u, v, n);
    tr.an = tr.an.ScaleOutOfCsys(u, v, n);
    tr.b  = tr.b.ScaleOutOfCsys(u, v, n);
    tr.bn = tr.bn.ScaleOutOfCsys(u, v, n);
    tr.c  = tr.c.ScaleOutOfCsys(u, v, n);
    tr.cn = tr.cn.ScaleOutOfCsys(u, v, n);
    return tr;
}

STriangle STriangle::From(const STriMeta& meta, const Vector& a, const Vector& b,
                          const Vector& c) {
    STriangle tr = {};
    tr.meta = meta;
    tr.a = a;
    tr.b = b;
    tr.c = c;
    return tr;
}

SEdge SEdge::From(const Vector& a, const Vector& b) {
    SEdge se = {};
    se.a = a;
    se.b = b;
    return se;
}

bool SEdge::EdgeCrosses(const Vector& ea, const Vector& eb, Vector *ppi, SPointList *spl) const {
    const Vector d = eb.Minus(ea);
    const double t_eps = LENGTH_EPS/d.Magnitude();


    const Vector dthis = b.Minus(a);
    const double tthis_eps = LENGTH_EPS/dthis.Magnitude();

    if((ea.Equals(a) && eb.Equals(b)) ||
       (eb.Equals(a) && ea.Equals(b)))
    {
        if(ppi) {
            *ppi = a;
        }
        if(spl) {
            spl->Add(a);
        }
        return true;
    }

    // Can't just test if distance between d and a equals distance between d and b;
    // they could be on opposite sides, since we don't have the sign.
    const double m = sqrt(d.Magnitude()*dthis.Magnitude());
    if(sqrt(fabs(d.Dot(dthis))) > (m - LENGTH_EPS)) {
        // The edges are parallel.
        if(fabs(a.DistanceToLine(ea, d)) > LENGTH_EPS) {
            // and not coincident, so can't be intersecting
            return false;
        }
        // The edges are coincident. Make sure that neither endpoint lies
        // on the other
        bool inters = false;
        double t;
        t = a.Minus(ea).DivPivoting(d);
        if(t > t_eps && t < (1 - t_eps)) {
            inters = true;
        }
        t = b.Minus(ea).DivPivoting(d);
        if(t > t_eps && t < (1 - t_eps)) {
            inters = true;
        }
        t = ea.Minus(a).DivPivoting(dthis);
        if(t > tthis_eps && t < (1 - tthis_eps)) {
            inters = true;
        }
        t = eb.Minus(a).DivPivoting(dthis);
        if(t > tthis_eps && t < (1 - tthis_eps)) {
            inters = true;
        }

        if(inters) {
            if(ppi) {
                *ppi = a;
            }
            if(spl) {
                spl->Add(a);
            }
            return true;
        } else {
            // So coincident but disjoint, okay.
            return false;
        }
    }

    // Lines are not parallel, so look for an intersection.
    bool skew;
    double tthis;
    double t;
    const Vector pi = Vector::AtIntersectionOfLines(ea, eb, a, b,
                                       &skew,
                                       &t, &tthis);
    if(skew) {
        return false;
    }

    const bool inOrEdge0 = (t     > -t_eps)     && (t     < (1 + t_eps));
    const bool inOrEdge1 = (tthis > -tthis_eps) && (tthis < (1 + tthis_eps));

    if(inOrEdge0 && inOrEdge1) {
        if(a.Equals(ea) || b.Equals(ea) ||
           a.Equals(eb) || b.Equals(eb)) {
            // Not an intersection if we share an endpoint with an edge
            return false;
        }
        // But it's an intersection if a vertex of one edge lies on the
        // inside of the other (or if they cross away from either's
        // vertex).
        if(ppi) {
            *ppi = pi;
        }
        if(spl) {
            spl->Add(pi);
        }
        return true;
    }
    return false;
}

void SEdgeList::Clear() {
    l.Clear();
}
                                                                                                   
void SEdgeList::AddEdge(const Vector& a, const Vector& b, const int auxA, const int auxB,
                        const int tag) {
    SEdge e = {};
    e.tag = tag;
    e.a = a;
    e.b = b;
    e.auxA = auxA;
    e.auxB = auxB;
    l.Add(&e);
}

bool SEdgeList::AssembleContour(const Vector& first, Vector last, const bool keepDir,
                                SContour *dest, SEdge *errorAt) const
{
    dest->AddPoint(first);
    dest->AddPoint(last);

    size_t i;
    do {
        for(i = 0; i < l.n; ++i) {
            SEdge * const se = &(l.elem[i]);
            if(se->tag) {
                continue;
            }

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

bool SEdgeList::AssemblePolygon(SPolygon *dest, SEdge *errorAt, const bool keepDir) const {
    dest->Clear();

    bool allClosed = true;
    for(;;) {
        Vector first = Vector::From(0, 0, 0);
        Vector last  = Vector::From(0, 0, 0);
        size_t i;
        for(i = 0; i < l.n; ++i) {
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
        if(!AssembleContour(first, last, keepDir, &(dest->l.elem[dest->l.n-1]), errorAt))
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
size_t SEdgeList::AnyEdgeCrossings(const Vector& a, const Vector& b,
                                Vector *ppi, SPointList *spl) const
{
    size_t cnt = 0;
    for(const SEdge *se = l.First(); se; se = l.NextAfter(se)) {
        if(se->EdgeCrosses(a, b, ppi, spl)) {
            ++cnt;
        }
    }
    return cnt;
}

//-----------------------------------------------------------------------------
// Returns true if the intersecting edge list contains an edge that shares
// an endpoint with one of our edges.
//-----------------------------------------------------------------------------
bool SEdgeList::ContainsEdgeFrom(const SEdgeList * const sel) const {
    for(const SEdge *se = l.First(); se; se = l.NextAfter(se)) {
        if(sel->ContainsEdge(se)) {
            return true;
        }
    }
    return false;
}
bool SEdgeList::ContainsEdge(const SEdge * const set) const {
    for(const SEdge *se = l.First(); se; se = l.NextAfter(se)) {
        if((se->a).Equals(set->a) && (se->b).Equals(set->b)) {
            return true;
        }
        if((se->b).Equals(set->a) && (se->a).Equals(set->b)) {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
// Remove unnecessary edges: if two are anti-parallel then remove both, and if
// two are parallel then remove one.
//-----------------------------------------------------------------------------
void SEdgeList::CullExtraneousEdges() {
    l.ClearTags();
    size_t i, j;
    for(i = 0; i < l.n; ++i) {
        SEdge * const se = &(l.elem[i]);
        for(j = i+1; j < l.n; ++j) {
            SEdge * const set = &(l.elem[j]);
            if((set->a).Equals(se->a) && (set->b).Equals(se->b)) {
                // Two parallel edges exist; so keep only the first one.
                set->tag = 1;
            }
            if((set->a).Equals(se->b) && (set->b).Equals(se->a)) {
                // Two anti-parallel edges exist; so keep neither.
                se->tag = 1;
                set->tag = 1;
            }
        }
    }
    l.RemoveTagged();
}

//-----------------------------------------------------------------------------
// Make a kd-tree of edges. This is used for O(log(n)) implementations of stuff
// that would naively be O(n).
//-----------------------------------------------------------------------------
SKdNodeEdges *SKdNodeEdges::Alloc() {
    SKdNodeEdges * const ne = (SKdNodeEdges *)AllocTemporary(sizeof(SKdNodeEdges));
    *ne = {};
    return ne;
}
SEdgeLl *SEdgeLl::Alloc() {
    SEdgeLl * const sell = (SEdgeLl *)AllocTemporary(sizeof(SEdgeLl));
    *sell = {};
    return sell;
}
SKdNodeEdges *SKdNodeEdges::From(SEdgeList * const sel) {
    SEdgeLl *sell = nullptr;
    SEdge *se;
    for(se = sel->l.First(); se; se = sel->l.NextAfter(se)) {
        SEdgeLl * const n = SEdgeLl::Alloc();
        n->se = se;
        n->next = sell;
        sell = n;
    }
    return SKdNodeEdges::From(sell);
}
SKdNodeEdges *SKdNodeEdges::From(SEdgeLl * const sell) {
    SKdNodeEdges *n = SKdNodeEdges::Alloc();

    // Compute the midpoints (just mean, though median would be better) of
    // each component.
    Vector ptAve = Vector::From(0, 0, 0);
    SEdgeLl *flip;
    size_t totaln = 0;
    for(flip = sell; flip; flip = flip->next) {
        ptAve = ptAve.Plus(flip->se->a);
        ptAve = ptAve.Plus(flip->se->b);
        ++totaln;
    }
    ptAve = ptAve.ScaledBy(1.0 / (2*totaln));

    // For each component, see how it splits.
    size_t ltln[3] = { 0, 0, 0 }, gtln[3] = { 0, 0, 0 };
    double badness[3];
    for(flip = sell; flip; flip = flip->next) {
        for(size_t i = 0; i < 3; ++i) {
            if(flip->se->a.Element(i) < ptAve.Element(i) + KDTREE_EPS ||
               flip->se->b.Element(i) < ptAve.Element(i) + KDTREE_EPS)
            {
                ++ltln[i];
            }
            if(flip->se->a.Element(i) > ptAve.Element(i) - KDTREE_EPS ||
               flip->se->b.Element(i) > ptAve.Element(i) - KDTREE_EPS)
            {
                ++gtln[i];
            }
        }
    }
    for(size_t i = 0; i < 3; ++i) {
        badness[i] = pow((double)ltln[i], 4) + pow((double)gtln[i], 4);
    }

    // Choose the least bad coordinate to split along.
    if(badness[0] < badness[1] && badness[0] < badness[2]) {
        n->which = 0;
    } else if(badness[1] < badness[2]) {
        n->which = 1;
    } else {
        n->which = 2;
    }
    n->c = ptAve.Element(n->which);

    if(totaln < 3 || totaln == gtln[n->which] || totaln == ltln[n->which]) {
        n->edges = sell;
        // and we're a leaf node
        return n;
    }

    // Sort the edges according to which side(s) of the split plane they're on.
    SEdgeLl *gtl = nullptr, *ltl = nullptr;
    for(flip = sell; flip; flip = flip->next) {
        if(flip->se->a.Element(n->which) < n->c + KDTREE_EPS ||
           flip->se->b.Element(n->which) < n->c + KDTREE_EPS)
        {
            SEdgeLl * const selln = SEdgeLl::Alloc();
            selln->se = flip->se;
            selln->next = ltl;
            ltl = selln;
        }
        if(flip->se->a.Element(n->which) > n->c - KDTREE_EPS ||
           flip->se->b.Element(n->which) > n->c - KDTREE_EPS)
        {
            SEdgeLl * const selln = SEdgeLl::Alloc();
            selln->se = flip->se;
            selln->next = gtl;
            gtl = selln;
        }
    }

    n->lt = SKdNodeEdges::From(ltl);
    n->gt = SKdNodeEdges::From(gtl);
    return n;
}

size_t SKdNodeEdges::AnyEdgeCrossings(const Vector& a, const Vector& b, const size_t cnt,
        Vector *pi, SPointList *spl) const
{
    size_t inters = 0;
    if(gt && lt) {
        if(a.Element(which) < c + KDTREE_EPS ||
           b.Element(which) < c + KDTREE_EPS)
        {
            inters += lt->AnyEdgeCrossings(a, b, cnt, pi, spl);
        }
        if(a.Element(which) > c - KDTREE_EPS ||
           b.Element(which) > c - KDTREE_EPS)
        {
            inters += gt->AnyEdgeCrossings(a, b, cnt, pi, spl);
        }
    } else {
        SEdgeLl *sell;
        for(sell = edges; sell; sell = sell->next) {
            SEdge * const se = sell->se;
            if(se->tag == cnt) {
                continue;
            }
            if(se->EdgeCrosses(a, b, pi, spl)) {
                ++inters;
            }
            se->tag = cnt;
        }
    }
    return inters;
}

//-----------------------------------------------------------------------------
// We have an edge list that contains only collinear edges, maybe with more
// splits than necessary. Merge any collinear segments that join.
//-----------------------------------------------------------------------------
static Vector LineStart, LineDirection;
static int ByTAlongLine(const void *av, const void *bv)
{
    SEdge * const a = (SEdge *)av,
          * const b = (SEdge *)bv;

    const double ta = (a->a.Minus(LineStart)).DivPivoting(LineDirection);
    const double tb = (b->a.Minus(LineStart)).DivPivoting(LineDirection);

    return (ta > tb) ? 1 : -1;
}
void SEdgeList::MergeCollinearSegments(const Vector& a, const Vector& b) {
    LineStart = a;
    LineDirection = b.Minus(a);
    qsort(l.elem, l.n, sizeof(l.elem[0]), ByTAlongLine);

    l.ClearTags();
    size_t i;
    for(i = 1; i < l.n; ++i) {
        SEdge * const prev = &(l.elem[i - 1]);
        SEdge * const now = &(l.elem[i]);

        if((prev->b).Equals(now->a) && prev->auxA == now->auxA) {
            // The previous segment joins up to us; so merge it into us.
            prev->tag = 1;
            now->a = prev->a;
        }
    }
    l.RemoveTagged();
}

void SPointList::Clear() {
    l.Clear();
}

bool SPointList::ContainsPoint(const Vector& pt) const {
    return (IndexForPoint(pt) >= 0);
}

int SPointList::IndexForPoint(const Vector& pt) const {
    size_t i;
    for(i = 0; i < l.n; ++i) {
        SPoint *p = &(l.elem[i]);
        if(pt.Equals(p->p)) {
            return i;
        }
    }
    // Not found, so return negative to indicate that.
    return -1;
}

void SPointList::IncrementTagFor(const Vector& pt) {
    SPoint *p;
    for(p = l.First(); p; p = l.NextAfter(p)) {
        if(pt.Equals(p->p)) {
            ++(p->tag);
            return;
        }
    }
    SPoint pa;
    pa.p = pt;
    pa.tag = 1;
    l.Add(&pa);
}

void SPointList::Add(const Vector& pt) {
    SPoint p = {};
    p.p = pt;
    l.Add(&p);
}

void SContour::AddPoint(const Vector& p) {
    SPoint sp;
    sp.tag = 0;
    sp.p = p;

    l.Add(&sp);
}

void SContour::MakeEdgesInto(SEdgeList * const el) const {
    size_t i;
    for(i = 0; (i + 1) < l.n; ++i) {
        el->AddEdge(l.elem[i].p, l.elem[i+1].p);
    }
}

void SContour::CopyInto(SContour * const dest) const {
    for(const SPoint * sp = l.First(); sp; sp = l.NextAfter(sp)) {
        dest->AddPoint(sp->p);
    }
}

void SContour::FindPointWithMinX() {
    xminPt = Vector::From(1e10, 1e10, 1e10);
    for(const SPoint * sp = l.First(); sp; sp = l.NextAfter(sp)) {
        if(sp->p.x < xminPt.x) {
            xminPt = sp->p;
        }
    }
}

Vector SContour::ComputeNormal() const {
    Vector n = Vector::From(0, 0, 0);

    for(size_t i = 0; (i + 2) < l.n; ++i) {
        const Vector u = (l.elem[i+1].p).Minus(l.elem[i+0].p).WithMagnitude(1);
        const Vector v = (l.elem[i+2].p).Minus(l.elem[i+1].p).WithMagnitude(1);
        const Vector nt = u.Cross(v);
        if(nt.Magnitude() > n.Magnitude()) {
            n = nt;
        }
    }
    return n.WithMagnitude(1);
}

Vector SContour::AnyEdgeMidpoint() const {
    ssassert(l.n >= 2, "Need two points to find a midpoint");
    return ((l.elem[0].p).Plus(l.elem[1].p)).ScaledBy(0.5);
}

bool SContour::IsClockwiseProjdToNormal(const Vector& n) const {
    // Degenerate things might happen as we draw; doesn't really matter
    // what we do then.
    if(n.Magnitude() < 0.01) {
        return true;
    }

    return (SignedAreaProjdToNormal(n) < 0);
}

double SContour::SignedAreaProjdToNormal(const Vector& n) const {
    // An arbitrary 2d coordinate system that has n as its normal
    const Vector u = n.Normal(0);
    const Vector v = n.Normal(1);

    double area = 0;
    for(size_t i = 0; (i + 1) < l.n; ++i) {
        const double u0 = (l.elem[i  ].p).Dot(u);
        const double v0 = (l.elem[i  ].p).Dot(v);
        const double u1 = (l.elem[i+1].p).Dot(u);
        const double v1 = (l.elem[i+1].p).Dot(v);

        area += ((v0 + v1)/2)*(u1 - u0);
    }
    return area;
}

bool SContour::ContainsPointProjdToNormal(const Vector& n, const Vector& p) const {
    const Vector u = n.Normal(0);
    const Vector v = n.Normal(1);

    const double up = p.Dot(u);
    const double vp = p.Dot(v);

    bool inside = false;
    for(size_t i = 0; (i + 1) < l.n; ++i) {
        const double ua = (l.elem[i].p).Dot(u);
        const double va = (l.elem[i].p).Dot(v);
        // The curve needs to be exactly closed; approximation is death.
        const double ub = (l.elem[(i+1)%(l.n-1)].p).Dot(u);
        const double vb = (l.elem[(i+1)%(l.n-1)].p).Dot(v);

        if((((va <= vp) && (vp < vb)) ||
             ((vb <= vp) && (vp < va))) &&
            (up < (ub - ua) * (vp - va) / (vb - va) + ua))
        {
          inside = !inside;
        }
    }

    return inside;
}

void SContour::Reverse() {
    l.Reverse();
}


void SPolygon::Clear() {
    size_t i;
    for(i = 0; i < l.n; ++i) {
        (l.elem[i]).l.Clear();
    }
    l.Clear();
}

void SPolygon::AddEmptyContour() {
    const SContour c = {};
    l.Add(&c);
}

void SPolygon::MakeEdgesInto(SEdgeList * const el) const {
    size_t i;
    for(i = 0; i < l.n; ++i) {
        (l.elem[i]).MakeEdgesInto(el);
    }
}

Vector SPolygon::ComputeNormal() const {
    if(l.n < 1) {
        return Vector::From(0, 0, 0);
    }
    return (l.elem[0]).ComputeNormal();
}

double SPolygon::SignedArea() const {
    double area = 0;
    // This returns the true area only if the contours are all oriented
    // correctly, with the holes backwards from the outer contours.
    for(const SContour *sc = l.First(); sc; sc = l.NextAfter(sc)) {
        area += sc->SignedAreaProjdToNormal(normal);
    }
    return area;
}

bool SPolygon::ContainsPoint(const Vector& p) const {
    return (WindingNumberForPoint(p) % 2) == 1;
}

size_t SPolygon::WindingNumberForPoint(const Vector& p) const {
    size_t winding = 0;
    size_t i;
    for(i = 0; i < l.n; ++i) {
        const SContour * const sc = &(l.elem[i]);
        if(sc->ContainsPointProjdToNormal(normal, p)) {
            ++winding;
        }
    }
    return winding;
}

void SPolygon::FixContourDirections() {
    // At output, the contour's tag will be 1 if we reversed it, else 0.
    l.ClearTags();

    // Outside curve looks counterclockwise, projected against our normal.
    size_t i, j;
    for(i = 0; i < l.n; ++i) {
        SContour * const sc = &(l.elem[i]);
        if(sc->l.n < 2) {
            continue;
        }
        // The contours may not intersect, but they may share vertices; so
        // testing a vertex for point-in-polygon may fail, but the midpoint
        // of an edge is okay.
        const Vector pt = (((sc->l.elem[0]).p).Plus(sc->l.elem[1].p)).ScaledBy(0.5);

        sc->timesEnclosed = 0;
        bool outer = true;
        for(j = 0; j < l.n; ++j) {
            if(i == j) {
                continue;
            }
            const SContour * const sct = &(l.elem[j]);
            if(sct->ContainsPointProjdToNormal(normal, pt)) {
                outer = !outer;
                ++(sc->timesEnclosed);
            }
        }

        const bool clockwise = sc->IsClockwiseProjdToNormal(normal);
        if((clockwise && outer) || (!clockwise && !outer)) {
            sc->Reverse();
            sc->tag = 1;
        }
    }
}

bool SPolygon::IsEmpty() const {
    if(l.n == 0 || l.elem[0].l.n == 0) {
        return true;
    }
    return false;
}

Vector SPolygon::AnyPoint() const {
    ssassert(!IsEmpty(), "Need at least one point");
    return l.elem[0].l.elem[0].p;
}

bool SPolygon::SelfIntersecting(Vector *intersectsAt) const {
    SEdgeList el = {};
    MakeEdgesInto(&el);
    const SKdNodeEdges * const kdtree = SKdNodeEdges::From(&el);

    size_t cnt = 1;
    el.l.ClearTags();

    bool ret = false;
    const SEdge *se;
    for(se = el.l.First(); se; se = el.l.NextAfter(se)) {
        const size_t inters = kdtree->AnyEdgeCrossings(se->a, se->b, cnt, intersectsAt);
        if(inters != 1) {
            ret = true;
            break;
        }
        ++cnt;
    }

    el.Clear();
    return ret;
}

void SPolygon::InverseTransformInto(SPolygon *sp, const Vector& u, const Vector& v,
                                    const Vector& n) const {
    for(const SContour &sc : l) {
        SContour tsc = {};
        tsc.timesEnclosed = sc.timesEnclosed;
        for(const SPoint &sp : sc.l) {
            tsc.AddPoint(sp.p.DotInToCsys(u, v, n));
        }
        sp->l.Add(&tsc);
    }
}

//-----------------------------------------------------------------------------
// Low-quality routines to cutter radius compensate a polygon. Assumes the
// polygon is in the xy plane, and the contours all go in the right direction
// with respect to normal (0, 0, -1).
//-----------------------------------------------------------------------------
void SPolygon::OffsetInto(SPolygon *dest, const double r) const {
    size_t i;
    dest->Clear();
    for(i = 0; i < l.n; ++i) {
        SContour * const sc = &(l.elem[i]);
        dest->AddEmptyContour();
        sc->OffsetInto(&(dest->l.elem[dest->l.n-1]), r);
    }
}
//-----------------------------------------------------------------------------
// Calculate the intersection point of two lines. Returns true for success,
// false if they're parallel.
//-----------------------------------------------------------------------------
static bool IntersectionOfLines(const double x0A, const double y0A, const double dxA, 
                                const double dyA, const double x0B, const double y0B,
                                const double dxB, const double dyB, double * const xi,
                                double * const yi)
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
    const double v = A[1][0] / A[0][0];
    A[1][0] -= A[0][0]*v;
    A[1][1] -= A[0][1]*v;
    b[1] -= b[0]*v;

    // Back-substitute.
    *yi = b[1] / A[1][1];
    *xi = (b[0] - A[0][1]*(*yi)) / A[0][0];

    return true;
}
void SContour::OffsetInto(SContour *dest, const double r) const {
    size_t i;

    for(i = 0; i < l.n; ++i) {
        double thetan, thetap;

        const Vector a = l.elem[WRAP(i-1, (l.n-1))].p;
        const Vector b = l.elem[WRAP(i,   (l.n-1))].p;
        const Vector c = l.elem[WRAP(i+1, (l.n-1))].p;

        const Vector dp = a.Minus(b);
        thetap = atan2(dp.y, dp.x);

        const Vector dn = b.Minus(c);
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
             const Vector p = { b.x - r*sin(thetap), b.y + r*cos(thetap), 0 };
            dest->AddPoint(p);
        } else if(thetan < thetap) {
            // This is an inside corner. We have two edges, Ep and En. Move
            // out from their intersection by radius, normal to En, and
            // then draw a line parallel to En. Move out from their
            // intersection by radius, normal to Ep, and then draw a second
            // line parallel to Ep. The point that we want to generate is
            // the intersection of these two lines--it removes as much
            // material as we can without removing any that we shouldn't.

            const double px0 = b.x - r*sin(thetap);
            const double py0 = b.y + r*cos(thetap);
            const double pdx = cos(thetap);
            const double pdy = sin(thetap);

            const double nx0 = b.x - r*sin(thetan);
            const double ny0 = b.y + r*cos(thetan);
            const double ndx = cos(thetan);
            const double ndy = sin(thetan);

            double x = 0.0, y = 0.0;
            IntersectionOfLines(px0, py0, pdx, pdy,
                                nx0, ny0, ndx, ndy,
                                &x, &y);

            dest->AddPoint(Vector::From(x, y, 0.0));
        } else {
            if(fabs(thetap - thetan) < (6.0*PI)/180.0) {
                const Vector pp{ b.x - r*sin(thetap),
                              b.y + r*cos(thetap), 0.0 };
                dest->AddPoint(pp);

                const Vector pn{ b.x - r*sin(thetan),
                              b.y + r*cos(thetan), 0.0 };
                dest->AddPoint(pn);
            } else {
                double theta;
                for(theta = thetap; theta <= thetan; theta += (6*PI)/180) {
                    const Vector p = { b.x - r*sin(theta),
                                 b.y + r*cos(theta), 0 };
                    dest->AddPoint(p);
                }
            }
        }
    }
}
