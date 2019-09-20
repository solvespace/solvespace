//-----------------------------------------------------------------------------
// Anything involving curves and sets of curves (except for the real math,
// which is in ratpoly.cpp).
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

SBezier SBezier::From(Vector4 p0, Vector4 p1) {
    SBezier ret = {};
    ret.deg = 1;
    ret.weight[0] = p0.w;
    ret.ctrl  [0] = p0.PerspectiveProject();
    ret.weight[1] = p1.w;
    ret.ctrl  [1] = p1.PerspectiveProject();
    return ret;
}

SBezier SBezier::From(Vector4 p0, Vector4 p1, Vector4 p2) {
    SBezier ret = {};
    ret.deg = 2;
    ret.weight[0] = p0.w;
    ret.ctrl  [0] = p0.PerspectiveProject();
    ret.weight[1] = p1.w;
    ret.ctrl  [1] = p1.PerspectiveProject();
    ret.weight[2] = p2.w;
    ret.ctrl  [2] = p2.PerspectiveProject();
    return ret;
}

SBezier SBezier::From(Vector4 p0, Vector4 p1, Vector4 p2, Vector4 p3) {
    SBezier ret = {};
    ret.deg = 3;
    ret.weight[0] = p0.w;
    ret.ctrl  [0] = p0.PerspectiveProject();
    ret.weight[1] = p1.w;
    ret.ctrl  [1] = p1.PerspectiveProject();
    ret.weight[2] = p2.w;
    ret.ctrl  [2] = p2.PerspectiveProject();
    ret.weight[3] = p3.w;
    ret.ctrl  [3] = p3.PerspectiveProject();
    return ret;
}

SBezier SBezier::From(Vector p0, Vector p1) {
    return SBezier::From(p0.Project4d(),
                         p1.Project4d());
}

SBezier SBezier::From(Vector p0, Vector p1, Vector p2) {
    return SBezier::From(p0.Project4d(),
                         p1.Project4d(),
                         p2.Project4d());
}

SBezier SBezier::From(Vector p0, Vector p1, Vector p2, Vector p3) {
    return SBezier::From(p0.Project4d(),
                         p1.Project4d(),
                         p2.Project4d(),
                         p3.Project4d());
}

Vector SBezier::Start() const {
    return ctrl[0];
}

Vector SBezier::Finish() const {
    return ctrl[deg];
}

void SBezier::Reverse() {
    int i;
    for(i = 0; i < (deg+1)/2; i++) {
        swap(ctrl[i], ctrl[deg-i]);
        swap(weight[i], weight[deg-i]);
    }
}

void SBezier::ScaleSelfBy(double s) {
    int i;
    for(i = 0; i <= deg; i++) {
        ctrl[i] = ctrl[i].ScaledBy(s);
    }
}

void SBezier::GetBoundingProjd(Vector u, Vector orig,
                               double *umin, double *umax) const
{
    int i;
    for(i = 0; i <= deg; i++) {
        double ut = ((ctrl[i]).Minus(orig)).Dot(u);
        if(ut < *umin) *umin = ut;
        if(ut > *umax) *umax = ut;
    }
}

SBezier SBezier::TransformedBy(Vector t, Quaternion q, double scale) const {
    SBezier ret = *this;
    int i;
    for(i = 0; i <= deg; i++) {
        ret.ctrl[i] = (ret.ctrl[i]).ScaledBy(scale);
        ret.ctrl[i] = (q.Rotate(ret.ctrl[i])).Plus(t);
    }
    return ret;
}

//-----------------------------------------------------------------------------
// Does this curve lie entirely within the specified plane? It does if all
// the control points lie in that plane.
//-----------------------------------------------------------------------------
bool SBezier::IsInPlane(Vector n, double d) const {
    int i;
    for(i = 0; i <= deg; i++) {
        if(fabs((ctrl[i]).Dot(n) - d) > LENGTH_EPS) {
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
// Is this Bezier exactly the arc of a circle, projected along the specified
// axis? If yes, return that circle's center and radius.
//-----------------------------------------------------------------------------
bool SBezier::IsCircle(Vector axis, Vector *center, double *r) const {
    if(deg != 2) return false;

    if(ctrl[1].DistanceToLine(ctrl[0], ctrl[2].Minus(ctrl[0])) < LENGTH_EPS) {
        // This is almost a line segment. So it's a circle with very large
        // radius, which is likely to make code that tries to handle circles
        // blow up. So return false.
        return false;
    }

    Vector t0 = (ctrl[0]).Minus(ctrl[1]),
           t2 = (ctrl[2]).Minus(ctrl[1]),
           r0 = axis.Cross(t0),
           r2 = axis.Cross(t2);

    *center = Vector::AtIntersectionOfLines(ctrl[0], (ctrl[0]).Plus(r0),
                                            ctrl[2], (ctrl[2]).Plus(r2),
                                            NULL, NULL, NULL);

    double rd0 = center->Minus(ctrl[0]).Magnitude(),
           rd2 = center->Minus(ctrl[2]).Magnitude();
    if(fabs(rd0 - rd2) > LENGTH_EPS) {
        return false;
    }
    *r = rd0;

    Vector u = r0.WithMagnitude(1),
           v = (axis.Cross(u)).WithMagnitude(1);
    Point2d c2  = center->Project2d(u, v),
            pa2 = (ctrl[0]).Project2d(u, v).Minus(c2),
            pb2 = (ctrl[2]).Project2d(u, v).Minus(c2);

    double thetaa = atan2(pa2.y, pa2.x), // in fact always zero due to csys
           thetab = atan2(pb2.y, pb2.x),
           dtheta = WRAP_NOT_0(thetab - thetaa, 2*PI);
    if(dtheta > PI) {
        // Not possible with a second order Bezier arc; so we must have
        // the points backwards.
        dtheta = 2*PI - dtheta;
    }

    if(fabs(weight[1] - cos(dtheta/2)) > LENGTH_EPS) {
        return false;
    }

    return true;
}

bool SBezier::IsRational() const {
    int i;
    for(i = 0; i <= deg; i++) {
        if(fabs(weight[i] - 1) > LENGTH_EPS) return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// Apply a perspective transformation to a rational Bezier curve, calculating
// the new weights as required.
//-----------------------------------------------------------------------------
SBezier SBezier::InPerspective(Vector u, Vector v, Vector n,
                               Vector origin, double cameraTan) const
{
    Quaternion q = Quaternion::From(u, v);
    q = q.Inverse();
    // we want Q*(p - o) = Q*p - Q*o
    SBezier ret = this->TransformedBy(q.Rotate(origin).ScaledBy(-1), q, 1.0);
    int i;
    for(i = 0; i <= deg; i++) {
        Vector4 ct = Vector4::From(ret.weight[i], ret.ctrl[i]);
        // so the desired curve, before perspective, is
        //    (x/w, y/w, z/w)
        // and after perspective is
        //    ((x/w)/(1 - (z/w)*cameraTan, ...
        //  = (x/(w - z*cameraTan), ...
        // so we want to let w' = w - z*cameraTan
        ct.w = ct.w - ct.z*cameraTan;

        ret.ctrl[i] = ct.PerspectiveProject();
        ret.weight[i] = ct.w;
    }
    return ret;
}

bool SBezier::Equals(SBezier *b) const {
    // We just test of identical degree and control points, even though two
    // curves could still be coincident (even sharing endpoints).
    if(deg != b->deg) return false;
    int i;
    for(i = 0; i <= deg; i++) {
        if(!(ctrl[i]).Equals(b->ctrl[i])) return false;
        if(fabs(weight[i] - b->weight[i]) > LENGTH_EPS) return false;
    }
    return true;
}

void SBezierList::Clear() {
    l.Clear();
}

void SBezierList::ScaleSelfBy(double s) {
    SBezier *sb;
    for(sb = l.First(); sb; sb = l.NextAfter(sb)) {
        sb->ScaleSelfBy(s);
    }
}

//-----------------------------------------------------------------------------
// If our list contains multiple identical Beziers (in either forward or
// reverse order), then cull them. If both is true, both beziers are removed.
// Otherwise only one of them is removed.
//-----------------------------------------------------------------------------
void SBezierList::CullIdenticalBeziers(bool both) {
    int i, j;

    l.ClearTags();
    for(i = 0; i < l.n; i++) {
        SBezier *bi = &(l[i]), bir;
        bir = *bi;
        bir.Reverse();

        for(j = i + 1; j < l.n; j++) {
            SBezier *bj = &(l[j]);
            if(bj->Equals(bi) ||
               bj->Equals(&bir))
            {
                if (both) bi->tag = 1;
                bj->tag = 1;
            }
        }
    }
    l.RemoveTagged();
}

//-----------------------------------------------------------------------------
// Find all the points where a list of Bezier curves intersects another list
// of Bezier curves. We do this by intersecting their piecewise linearizations,
// and then refining any intersections that we find to lie exactly on the
// curves. So this will screw up on tangencies and stuff, but otherwise should
// be fine.
//-----------------------------------------------------------------------------
void SBezierList::AllIntersectionsWith(SBezierList *sblb, SPointList *spl) const {
    for(const SBezier *sba = l.First(); sba; sba = l.NextAfter(sba)) {
        for(const SBezier *sbb = sblb->l.First(); sbb; sbb = sblb->l.NextAfter(sbb)) {
            sbb->AllIntersectionsWith(sba, spl);
        }
    }
}
void SBezier::AllIntersectionsWith(const SBezier *sbb, SPointList *spl) const {
    SPointList splRaw = {};
    SEdgeList sea, seb;
    sea = {};
    seb = {};
    this->MakePwlInto(&sea);
    sbb ->MakePwlInto(&seb);
    SEdge *se;
    for(se = sea.l.First(); se; se = sea.l.NextAfter(se)) {
        // This isn't quite correct, since AnyEdgeCrossings doesn't count
        // the case where two pairs of line segments intersect at their
        // vertices. So this isn't robust, although that case isn't very
        // likely.
        seb.AnyEdgeCrossings(se->a, se->b, NULL, &splRaw);
    }
    SPoint *sp;
    for(sp = splRaw.l.First(); sp; sp = splRaw.l.NextAfter(sp)) {
        Vector p = sp->p;
        if(PointOnThisAndCurve(sbb, &p)) {
            if(!spl->ContainsPoint(p)) spl->Add(p);
        }
    }
    sea.Clear();
    seb.Clear();
    splRaw.Clear();
}

//-----------------------------------------------------------------------------
// Find a plane that contains all of the curves in this list. If the curves
// are all colinear (or coincident, or empty), then that plane is not exactly
// determined but we choose the additional degree(s) of freedom arbitrarily.
// Returns true if all the curves are coplanar, otherwise false.
//-----------------------------------------------------------------------------
bool SBezierList::GetPlaneContainingBeziers(Vector *p, Vector *u, Vector *v,
                        Vector *notCoplanarAt) const
{
    Vector pt, ptFar, ptOffLine, dp, n;
    double farMax, offLineMax;
    int i;

    // Get any point on any Bezier; or an arbitrary point if list is empty.
    if(!l.IsEmpty()) {
        pt = l[0].Start();
    } else {
        pt = Vector::From(0, 0, 0);
    }
    ptFar = ptOffLine = pt;

    // Get the point farthest from our arbitrary point.
    farMax = VERY_NEGATIVE;
    for(const SBezier *sb = l.First(); sb; sb = l.NextAfter(sb)) {
        for(i = 0; i <= sb->deg; i++) {
            double m = (pt.Minus(sb->ctrl[i])).Magnitude();
            if(m > farMax) {
                ptFar = sb->ctrl[i];
                farMax = m;
            }
        }
    }
    if(ptFar.Equals(pt)) {
        // The points are all coincident. So neither basis vector matters.
        *p = pt;
        *u = Vector::From(1, 0, 0);
        *v = Vector::From(0, 1, 0);
        return true;
    }

    // Get the point farthest from the line between pt and ptFar
    dp = ptFar.Minus(pt);
    offLineMax = VERY_NEGATIVE;
    for(const SBezier *sb = l.First(); sb; sb = l.NextAfter(sb)) {
        for(i = 0; i <= sb->deg; i++) {
            double m = (sb->ctrl[i]).DistanceToLine(pt, dp);
            if(m > offLineMax) {
                ptOffLine = sb->ctrl[i];
                offLineMax = m;
            }
        }
    }

    *p = pt;
    if(offLineMax < LENGTH_EPS) {
        // The points are all colinear; so choose the second basis vector
        // arbitrarily.
        *u = (ptFar.Minus(pt)).WithMagnitude(1);
        *v = (u->Normal(0)).WithMagnitude(1);
    } else {
        // The points actually define a plane.
        n = (ptFar.Minus(pt)).Cross(ptOffLine.Minus(pt));
        *u = (n.Normal(0)).WithMagnitude(1);
        *v = (n.Normal(1)).WithMagnitude(1);
    }

    // So we have a plane; but check whether all of the points lie in that
    // plane.
    n = u->Cross(*v);
    n = n.WithMagnitude(1);
    double d = p->Dot(n);
    for(const SBezier *sb = l.First(); sb; sb = l.NextAfter(sb)) {
        for(i = 0; i <= sb->deg; i++) {
            if(fabs(n.Dot(sb->ctrl[i]) - d) > LENGTH_EPS) {
                if(notCoplanarAt) *notCoplanarAt = sb->ctrl[i];
                return false;
            }
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
// Assemble curves in sbl into a single loop. The curves may appear in any
// direction (start to finish, or finish to start), and will be reversed if
// necessary. The curves in the returned loop are removed from sbl, even if
// the loop cannot be closed.
//-----------------------------------------------------------------------------
SBezierLoop SBezierLoop::FromCurves(SBezierList *sbl,
                                    bool *allClosed, SEdge *errorAt)
{
    SBezierLoop loop = {};

    if(sbl->l.n < 1) return loop;
    sbl->l.ClearTags();

    SBezier *first = &(sbl->l[0]);
    first->tag = 1;
    loop.l.Add(first);
    Vector start = first->Start();
    Vector hanging = first->Finish();
    int auxA = first->auxA;

    sbl->l.RemoveTagged();

    while(!sbl->l.IsEmpty() && !hanging.Equals(start)) {
        int i;
        bool foundNext = false;
        for(i = 0; i < sbl->l.n; i++) {
            SBezier *test = &(sbl->l[i]);

            if((test->Finish()).Equals(hanging) && test->auxA == auxA) {
                test->Reverse();
                // and let the next test catch it
            }
            if((test->Start()).Equals(hanging) && test->auxA == auxA) {
                test->tag = 1;
                loop.l.Add(test);
                hanging = test->Finish();
                sbl->l.RemoveTagged();
                foundNext = true;
                break;
            }
        }
        if(!foundNext) {
            // The loop completed without finding the hanging edge, so
            // it's an open loop
            errorAt->a = hanging;
            errorAt->b = start;
            *allClosed = false;
            return loop;
        }
    }
    if(hanging.Equals(start)) {
        *allClosed = true;
    } else {
        // We ran out of edges without forming a closed loop.
        errorAt->a = hanging;
        errorAt->b = start;
        *allClosed = false;
    }

    return loop;
}

void SBezierLoop::Reverse() {
    l.Reverse();
    SBezier *sb;
    for(sb = l.First(); sb; sb = l.NextAfter(sb)) {
        // If we didn't reverse each curve, then the next curve in list would
        // share your start, not your finish.
        sb->Reverse();
    }
}

void SBezierLoop::GetBoundingProjd(Vector u, Vector orig,
                                   double *umin, double *umax) const
{
    for(const SBezier *sb = l.First(); sb; sb = l.NextAfter(sb)) {
        sb->GetBoundingProjd(u, orig, umin, umax);
    }
}

void SBezierLoop::MakePwlInto(SContour *sc, double chordTol) const {
    for(const SBezier *sb = l.First(); sb; sb = l.NextAfter(sb)) {
        sb->MakePwlInto(sc, chordTol);
        // Avoid double points at join between Beziers; except that
        // first and last points should be identical.
        if(l.NextAfter(sb) != NULL) {
            sc->l.RemoveLast(1);
        }
    }
    // Ensure that it's exactly closed, not just within a numerical tolerance.
    if((sc->l.Last()->p).Equals(sc->l.First()->p)) {
        *sc->l.Last() = *sc->l.First();
    }
}

bool SBezierLoop::IsClosed() const {
    if(l.IsEmpty()) return false;
    Vector s = l.First()->Start(),
           f = l.Last()->Finish();
    return s.Equals(f);
}


//-----------------------------------------------------------------------------
// Assemble the curves in sbl into multiple loops, and piecewise linearize the
// curves into poly. If we can't close a contour, then we add it to
// openContours (if that isn't NULL) and keep going; so this works even if the
// input contains a mix of open and closed curves.
//-----------------------------------------------------------------------------
SBezierLoopSet SBezierLoopSet::From(SBezierList *sbl, SPolygon *poly,
                                    double chordTol,
                                    bool *allClosed, SEdge *errorAt,
                                    SBezierLoopSet *openContours)
{
    SBezierLoopSet ret = {};

    *allClosed = true;
    while(!sbl->l.IsEmpty()) {
        bool thisClosed;
        SBezierLoop loop;
        loop = SBezierLoop::FromCurves(sbl, &thisClosed, errorAt);
        if(!thisClosed) {
            // Record open loops in a separate list, if requested.
            *allClosed = false;
            if(openContours) {
                openContours->l.Add(&loop);
            } else {
                loop.Clear();
            }
        } else {
            ret.l.Add(&loop);
            poly->AddEmptyContour();
            loop.MakePwlInto(poly->l.Last(), chordTol);
        }
    }

    poly->normal = poly->ComputeNormal();
    ret.normal = poly->normal;
    if(poly->l.n > 0) {
        ret.point = poly->AnyPoint();
    } else {
        ret.point = Vector::From(0, 0, 0);
    }

    return ret;
}

void SBezierLoopSet::GetBoundingProjd(Vector u, Vector orig,
                                      double *umin, double *umax) const
{
    for(const SBezierLoop *sbl = l.First(); sbl; sbl = l.NextAfter(sbl)) {
        sbl->GetBoundingProjd(u, orig, umin, umax);
    }
}

double SBezierLoopSet::SignedArea() {
    if(EXACT(area == 0.0)) {
        SPolygon sp = {};
        MakePwlInto(&sp);
        sp.normal = sp.ComputeNormal();
        area = sp.SignedArea();
        sp.Clear();
    }
    return area;
}

//-----------------------------------------------------------------------------
// Convert all the Beziers into piecewise linear form, and assemble that into
// a polygon, one contour per loop.
//-----------------------------------------------------------------------------
void SBezierLoopSet::MakePwlInto(SPolygon *sp) const {
    for(const SBezierLoop *sbl = l.First(); sbl; sbl = l.NextAfter(sbl)) {
        sp->AddEmptyContour();
        sbl->MakePwlInto(sp->l.Last());
    }
}

void SBezierLoopSet::Clear() {
    int i;
    for(i = 0; i < l.n; i++) {
        (l[i]).Clear();
    }
    l.Clear();
}

//-----------------------------------------------------------------------------
// An export helper function. We start with a list of Bezier curves, and
// assemble them into loops. We find the outer loops, and find the outer loops'
// inner loops, and group them accordingly.
//-----------------------------------------------------------------------------
void SBezierLoopSetSet::FindOuterFacesFrom(SBezierList *sbl, SPolygon *spxyz,
                                   SSurface *srfuv,
                                   double chordTol,
                                   bool *allClosed, SEdge *notClosedAt,
                                   bool *allCoplanar, Vector *notCoplanarAt,
                                   SBezierLoopSet *openContours)
{
    SSurface srfPlane;
    if(!srfuv) {
        Vector p, u, v;
        *allCoplanar =
            sbl->GetPlaneContainingBeziers(&p, &u, &v, notCoplanarAt);
        if(!*allCoplanar) {
            // Don't even try to assemble them into loops if they're not
            // all coplanar.
            if(openContours) {
                SBezier *sb;
                for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
                    SBezierLoop sbl={};
                    sbl.l.Add(sb);
                    openContours->l.Add(&sbl);
                }
            }
            return;
        }
        // All the curves lie in a plane through p with basis vectors u and v.
        srfPlane = SSurface::FromPlane(p, u, v);
        srfuv = &srfPlane;
    }

    int i, j;
    // Assemble the Bezier trim curves into closed loops; we also get the
    // piecewise linearization of the curves (in the SPolygon spxyz), as a
    // calculation aid for the loop direction.
    SBezierLoopSet sbls = SBezierLoopSet::From(sbl, spxyz, chordTol,
                                               allClosed, notClosedAt,
                                               openContours);
    if(sbls.l.n != spxyz->l.n) return;

    // Convert the xyz piecewise linear to uv piecewise linear.
    SPolygon spuv = {};
    SContour *sc;
    for(sc = spxyz->l.First(); sc; sc = spxyz->l.NextAfter(sc)) {
        spuv.AddEmptyContour();
        SPoint *pt;
        for(pt = sc->l.First(); pt; pt = sc->l.NextAfter(pt)) {
            double u, v;
            srfuv->ClosestPointTo(pt->p, &u, &v);
            spuv.l.Last()->AddPoint(Vector::From(u, v, 0));
        }
    }
    spuv.normal = Vector::From(0, 0, 1); // must be, since it's in xy plane now

    static const int OUTER_LOOP = 10;
    static const int INNER_LOOP = 20;
    static const int USED_LOOP  = 30;
    // Fix the contour directions; we do this properly, in uv space, so it
    // works for curved surfaces too (important for STEP export).
    spuv.FixContourDirections();
    for(i = 0; i < spuv.l.n; i++) {
        SContour    *contour = &(spuv.l[i]);
        SBezierLoop *bl = &(sbls.l[i]);
        if(contour->tag) {
            // This contour got reversed in the polygon to make the directions
            // consistent, so the same must be necessary for the Bezier loop.
            bl->Reverse();
        }
        if(contour->IsClockwiseProjdToNormal(spuv.normal)) {
            bl->tag = INNER_LOOP;
        } else {
            bl->tag = OUTER_LOOP;
        }
    }

    bool loopsRemaining = true;
    while(loopsRemaining) {
        loopsRemaining = false;
        for(i = 0; i < sbls.l.n; i++) {
            SBezierLoop *loop = &(sbls.l[i]);
            if(loop->tag != OUTER_LOOP) continue;

            // Check if this contour contains any outer loops; if it does, then
            // we should do those "inner outer loops" first; otherwise we
            // will steal their holes, since their holes also lie inside this
            // contour.
            for(j = 0; j < sbls.l.n; j++) {
                SBezierLoop *outer = &(sbls.l[j]);
                if(i == j) continue;
                if(outer->tag != OUTER_LOOP) continue;

                Vector p = spuv.l[j].AnyEdgeMidpoint();
                if(spuv.l[i].ContainsPointProjdToNormal(spuv.normal, p)) {
                    break;
                }
            }
            if(j < sbls.l.n) {
                // It does, can't do this one yet.
                continue;
            }

            SBezierLoopSet outerAndInners = {};
            loopsRemaining = true;
            loop->tag = USED_LOOP;
            outerAndInners.l.Add(loop);
            int auxA = 0;
            if(loop->l.n > 0) auxA = loop->l[0].auxA;

            for(j = 0; j < sbls.l.n; j++) {
                SBezierLoop *inner = &(sbls.l[j]);
                if(inner->tag != INNER_LOOP) continue;
                if(inner->l.n < 1) continue;
                if(inner->l[0].auxA != auxA) continue;

                Vector p = spuv.l[j].AnyEdgeMidpoint();
                if(spuv.l[i].ContainsPointProjdToNormal(spuv.normal, p)) {
                    outerAndInners.l.Add(inner);
                    inner->tag = USED_LOOP;
                }
            }

            outerAndInners.point  = srfuv->PointAt(0, 0);
            outerAndInners.normal = srfuv->NormalAt(0, 0);
            l.Add(&outerAndInners);
        }
    }

    // If we have poorly-formed loops--for example, overlapping zero-area
    // stuff--then we can end up with leftovers. We use this function to
    // group stuff into closed paths for export when possible, so it's bad
    // to screw up on that stuff. So just add them onto the open curve list.
    // Very ugly, but better than losing curves.
    for(i = 0; i < sbls.l.n; i++) {
        SBezierLoop *loop = &(sbls.l[i]);
        if(loop->tag == USED_LOOP) continue;

        if(openContours) {
            openContours->l.Add(loop);
        } else {
            loop->Clear();
        }
        // but don't free the used loops, since we shallow-copied them to
        // ourself
    }

    sbls.l.Clear(); // not sbls.Clear(), since that would deep-clear
    spuv.Clear();
}

void SBezierLoopSetSet::AddOpenPath(SBezier *sb) {
    SBezierLoop sbl = {};
    sbl.l.Add(sb);

    SBezierLoopSet sbls = {};
    sbls.l.Add(&sbl);

    l.Add(&sbls);
}

void SBezierLoopSetSet::Clear() {
    SBezierLoopSet *sbls;
    for(sbls = l.First(); sbls; sbls = l.NextAfter(sbls)) {
        sbls->Clear();
    }
    l.Clear();
}

SCurve SCurve::FromTransformationOf(SCurve *a, Vector t,
                                    Quaternion q, double scale)
{
    bool needRotate    = !EXACT(q.vx == 0.0 && q.vy == 0.0 && q.vz == 0.0 && q.w == 1.0);
    bool needTranslate = !EXACT(t.x  == 0.0 && t.y  == 0.0 && t.z  == 0.0);
    bool needScale     = !EXACT(scale == 1.0);

    SCurve ret = {};
    ret.h = a->h;
    ret.isExact = a->isExact;
    ret.exact = (a->exact).TransformedBy(t, q, scale);
    ret.surfA = a->surfA;
    ret.surfB = a->surfB;

    SCurvePt *p;
    ret.pts.ReserveMore(a->pts.n);
    for(p = a->pts.First(); p; p = a->pts.NextAfter(p)) {
        SCurvePt pp = *p;
        if(needScale) {
            pp.p = (pp.p).ScaledBy(scale);
        }
        if(needRotate) {
            pp.p = q.Rotate(pp.p);
        }
        if(needTranslate) {
            pp.p = pp.p.Plus(t);
        }
        ret.pts.Add(&pp);
    }
    return ret;
}

void SCurve::Clear() {
    pts.Clear();
}

SSurface *SCurve::GetSurfaceA(SShell *a, SShell *b) const {
    if(source == Source::A) {
        return a->surface.FindById(surfA);
    } else if(source == Source::B) {
        return b->surface.FindById(surfA);
    } else if(source == Source::INTERSECTION) {
        return a->surface.FindById(surfA);
    } else ssassert(false, "Unexpected curve source");
}

SSurface *SCurve::GetSurfaceB(SShell *a, SShell *b) const {
    if(source == Source::A) {
        return a->surface.FindById(surfB);
    } else if(source == Source::B) {
        return b->surface.FindById(surfB);
    } else if(source == Source::INTERSECTION) {
        return b->surface.FindById(surfB);
    } else ssassert(false, "Unexpected curve source");
}

//-----------------------------------------------------------------------------
// When we split line segments wherever they intersect a surface, we introduce
// extra pwl points. This may create very short edges that could be removed
// without violating the chord tolerance. Those are ugly, and also break
// stuff in the Booleans. So remove them.
//-----------------------------------------------------------------------------
void SCurve::RemoveShortSegments(SSurface *srfA, SSurface *srfB) {
    // Three, not two; curves are pwl'd to at least two edges (three points)
    // even if not necessary, to avoid square holes.
    if(pts.n <= 3) return;
    pts.ClearTags();

    Vector prev = pts[0].p;
    int i, a;
    for(i = 1; i < pts.n - 1; i++) {
        SCurvePt *sct = &(pts[i]),
                 *scn = &(pts[i+1]);
        if(sct->vertex) {
            prev = sct->p;
            continue;
        }
        bool mustKeep = false;

        // We must check against both surfaces; the piecewise linear edge
        // may have a different chord tolerance in the two surfaces. (For
        // example, a circle in the surface of a cylinder is just a straight
        // line, so it always has perfect chord tol, but that circle in
        // a plane is a circle so it doesn't).
        for(a = 0; a < 2; a++) {
            SSurface *srf = (a == 0) ? srfA : srfB;
            Vector puv, nuv;
            srf->ClosestPointTo(prev,   &(puv.x), &(puv.y));
            srf->ClosestPointTo(scn->p, &(nuv.x), &(nuv.y));

            if(srf->ChordToleranceForEdge(nuv, puv) > SS.ChordTolMm()) {
                mustKeep = true;
            }
        }

        if(mustKeep) {
            prev = sct->p;
        } else {
            sct->tag = 1;
            // and prev is unchanged, since there's no longer any point
            // in between
        }
    }

    pts.RemoveTagged();
}

STrimBy STrimBy::EntireCurve(SShell *shell, hSCurve hsc, bool backwards) {
    STrimBy stb = {};
    stb.curve = hsc;
    SCurve *sc = shell->curve.FindById(hsc);

    if(backwards) {
        stb.finish = sc->pts[0].p;
        stb.start = sc->pts.Last()->p;
        stb.backwards = true;
    } else {
        stb.start = sc->pts[0].p;
        stb.finish = sc->pts.Last()->p;
        stb.backwards = false;
    }

    return stb;
}

