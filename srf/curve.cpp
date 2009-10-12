//-----------------------------------------------------------------------------
// Anything involving curves and sets of curves (except for the real math,
// which is in ratpoly.cpp).
//-----------------------------------------------------------------------------
#include "../solvespace.h"

SBezier SBezier::From(Vector4 p0, Vector4 p1) {
    SBezier ret;
    ZERO(&ret);
    ret.deg = 1;
    ret.weight[0] = p0.w;
    ret.ctrl  [0] = p0.PerspectiveProject();
    ret.weight[1] = p1.w;
    ret.ctrl  [1] = p1.PerspectiveProject();
    return ret;
}

SBezier SBezier::From(Vector4 p0, Vector4 p1, Vector4 p2) {
    SBezier ret;
    ZERO(&ret);
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
    SBezier ret;
    ZERO(&ret);
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

Vector SBezier::Start(void) {
    return ctrl[0];
}

Vector SBezier::Finish(void) {
    return ctrl[deg];
}

void SBezier::Reverse(void) {
    int i;
    for(i = 0; i < (deg+1)/2; i++) {
        SWAP(Vector, ctrl[i], ctrl[deg-i]);
        SWAP(double, weight[i], weight[deg-i]);
    }
}

void SBezier::ScaleSelfBy(double s) {
    int i;
    for(i = 0; i <= deg; i++) {
        ctrl[i] = ctrl[i].ScaledBy(s);
    }
}

void SBezier::GetBoundingProjd(Vector u, Vector orig,
                               double *umin, double *umax)
{
    int i;
    for(i = 0; i <= deg; i++) {
        double ut = ((ctrl[i]).Minus(orig)).Dot(u);
        if(ut < *umin) *umin = ut;
        if(ut > *umax) *umax = ut;
    }
}

SBezier SBezier::TransformedBy(Vector t, Quaternion q, bool mirror) {
    SBezier ret = *this;
    int i;
    for(i = 0; i <= deg; i++) {
        if(mirror) ret.ctrl[i].z *= -1;
        ret.ctrl[i] = (q.Rotate(ret.ctrl[i])).Plus(t);
    }
    return ret;
}

//-----------------------------------------------------------------------------
// Does this curve lie entirely within the specified plane? It does if all
// the control points lie in that plane.
//-----------------------------------------------------------------------------
bool SBezier::IsInPlane(Vector n, double d) {
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
bool SBezier::IsCircle(Vector axis, Vector *center, double *r) {
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

bool SBezier::IsRational(void) {
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
                               Vector origin, double cameraTan)
{
    Quaternion q = Quaternion::From(u, v);
    q = q.Inverse();
    // we want Q*(p - o) = Q*p - Q*o
    SBezier ret = this->TransformedBy(q.Rotate(origin).ScaledBy(-1), q, false);
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

bool SBezier::Equals(SBezier *b) {
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

void SBezierList::Clear(void) {
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
// reverse order), then cull them.
//-----------------------------------------------------------------------------
void SBezierList::CullIdenticalBeziers(void) {
    int i, j;

    l.ClearTags();
    for(i = 0; i < l.n; i++) {
        SBezier *bi = &(l.elem[i]), bir;
        bir = *bi;
        bir.Reverse();

        for(j = i + 1; j < l.n; j++) {
            SBezier *bj = &(l.elem[j]);
            if(bj->Equals(bi) ||
               bj->Equals(&bir))
            {
                bi->tag = 1;
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
void SBezierList::AllIntersectionsWith(SBezierList *sblb, SPointList *spl) {
    SBezier *sba, *sbb;
    for(sba = l.First(); sba; sba = l.NextAfter(sba)) {
        for(sbb = sblb->l.First(); sbb; sbb = sblb->l.NextAfter(sbb)) {
            sbb->AllIntersectionsWith(sba, spl);
        }
    }
}
void SBezier::AllIntersectionsWith(SBezier *sbb, SPointList *spl) {
    SPointList splRaw;
    ZERO(&splRaw);
    SEdgeList sea, seb;
    ZERO(&sea);
    ZERO(&seb);
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

SBezierLoop SBezierLoop::FromCurves(SBezierList *sbl,
                                    bool *allClosed, SEdge *errorAt)
{
    SBezierLoop loop;
    ZERO(&loop);

    if(sbl->l.n < 1) return loop;
    sbl->l.ClearTags();
  
    SBezier *first = &(sbl->l.elem[0]);
    first->tag = 1;
    loop.l.Add(first);
    Vector start = first->Start();
    Vector hanging = first->Finish();

    sbl->l.RemoveTagged();

    while(sbl->l.n > 0 && !hanging.Equals(start)) {
        int i;
        bool foundNext = false;
        for(i = 0; i < sbl->l.n; i++) {
            SBezier *test = &(sbl->l.elem[i]);

            if((test->Finish()).Equals(hanging)) {
                test->Reverse();
                // and let the next test catch it
            }
            if((test->Start()).Equals(hanging)) {
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

void SBezierLoop::Reverse(void) {
    l.Reverse();
    SBezier *sb;
    for(sb = l.First(); sb; sb = l.NextAfter(sb)) {
        // If we didn't reverse each curve, then the next curve in list would
        // share your start, not your finish.
        sb->Reverse();
    }
}

void SBezierLoop::GetBoundingProjd(Vector u, Vector orig,
                                   double *umin, double *umax)
{
    SBezier *sb;
    for(sb = l.First(); sb; sb = l.NextAfter(sb)) {
        sb->GetBoundingProjd(u, orig, umin, umax);
    }
}

void SBezierLoop::MakePwlInto(SContour *sc) {
    List<Vector> lv;
    ZERO(&lv);

    int i, j;
    for(i = 0; i < l.n; i++) {
        SBezier *sb = &(l.elem[i]);
        sb->MakePwlInto(&lv);
        
        // Each curve's piecewise linearization includes its endpoints,
        // which we don't want to duplicate (creating zero-len edges).
        for(j = (i == 0 ? 0 : 1); j < lv.n; j++) {
            sc->AddPoint(lv.elem[j]);
        }
        lv.Clear();
    }
    // Ensure that it's exactly closed, not just within a numerical tolerance.
    sc->l.elem[sc->l.n - 1] = sc->l.elem[0];
}


SBezierLoopSet SBezierLoopSet::From(SBezierList *sbl, SPolygon *poly,
                                    bool *allClosed, SEdge *errorAt)
{
    int i;
    SBezierLoopSet ret;
    ZERO(&ret);

    while(sbl->l.n > 0) {
        bool thisClosed;
        SBezierLoop loop;
        loop = SBezierLoop::FromCurves(sbl, &thisClosed, errorAt);
        if(!thisClosed) {
            ret.Clear();
            *allClosed = false;
            return ret;
        }

        ret.l.Add(&loop);
        poly->AddEmptyContour();
        loop.MakePwlInto(&(poly->l.elem[poly->l.n-1]));
    }

    poly->normal = poly->ComputeNormal();
    ret.normal = poly->normal;
    if(poly->l.n > 0) {
        ret.point = poly->AnyPoint();
    } else {
        ret.point = Vector::From(0, 0, 0);
    }
    poly->FixContourDirections();

    for(i = 0; i < poly->l.n; i++) {
        if(poly->l.elem[i].tag) {
            // We had to reverse this contour in order to fix the poly
            // contour directions; so need to do the same with the curves.
            ret.l.elem[i].Reverse();
        }
    }

    *allClosed = true;
    return ret;
}

void SBezierLoopSet::GetBoundingProjd(Vector u, Vector orig,
                                      double *umin, double *umax)
{
    SBezierLoop *sbl;
    for(sbl = l.First(); sbl; sbl = l.NextAfter(sbl)) {
        sbl->GetBoundingProjd(u, orig, umin, umax);
    }
}

void SBezierLoopSet::Clear(void) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).Clear();
    }
    l.Clear();
}

SCurve SCurve::FromTransformationOf(SCurve *a, Vector t, Quaternion q,
                                               bool mirror)
{
    SCurve ret;
    ZERO(&ret);

    ret.h = a->h;
    ret.isExact = a->isExact;
    ret.exact = (a->exact).TransformedBy(t, q, mirror);
    ret.surfA = a->surfA;
    ret.surfB = a->surfB;
    
    SCurvePt *p;
    for(p = a->pts.First(); p; p = a->pts.NextAfter(p)) {
        SCurvePt pp = *p;
        if(mirror) pp.p.z *= -1;
        pp.p = (q.Rotate(pp.p)).Plus(t);
        ret.pts.Add(&pp);
    }
    return ret;
}

void SCurve::Clear(void) {
    pts.Clear();
}

SSurface *SCurve::GetSurfaceA(SShell *a, SShell *b) {
    if(source == FROM_A) {
        return a->surface.FindById(surfA);
    } else if(source == FROM_B) {
        return b->surface.FindById(surfA);
    } else if(source == FROM_INTERSECTION) {
        return a->surface.FindById(surfA);
    } else oops();
}

SSurface *SCurve::GetSurfaceB(SShell *a, SShell *b) {
    if(source == FROM_A) {
        return a->surface.FindById(surfB);
    } else if(source == FROM_B) {
        return b->surface.FindById(surfB);
    } else if(source == FROM_INTERSECTION) {
        return b->surface.FindById(surfB);
    } else oops();
}

//-----------------------------------------------------------------------------
// When we split line segments wherever they intersect a surface, we introduce
// extra pwl points. This may create very short edges that could be removed
// without violating the chord tolerance. Those are ugly, and also break
// stuff in the Booleans. So remove them.
//-----------------------------------------------------------------------------
void SCurve::RemoveShortSegments(SSurface *srfA, SSurface *srfB) {
    if(pts.n < 2) return;
    pts.ClearTags();

    Vector prev = pts.elem[0].p;
    int i, a;
    for(i = 1; i < pts.n - 1; i++) {
        SCurvePt *sct = &(pts.elem[i]),
                 *scn = &(pts.elem[i+1]);
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
    STrimBy stb;
    ZERO(&stb);
    stb.curve = hsc;
    SCurve *sc = shell->curve.FindById(hsc);

    if(backwards) {
        stb.finish = sc->pts.elem[0].p;
        stb.start = sc->pts.elem[sc->pts.n - 1].p;
        stb.backwards = true;
    } else {
        stb.start = sc->pts.elem[0].p;
        stb.finish = sc->pts.elem[sc->pts.n - 1].p;
        stb.backwards = false;
    }

    return stb;
}

