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

SBezier SBezier::TransformedBy(Vector t, Quaternion q) {
    SBezier ret = *this;
    int i;
    for(i = 0; i <= deg; i++) {
        ret.ctrl[i] = (q.Rotate(ret.ctrl[i])).Plus(t);
    }
    return ret;
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
    SBezier ret = this->TransformedBy(q.Rotate(origin).ScaledBy(-1), q);
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

SCurve SCurve::FromTransformationOf(SCurve *a, Vector t, Quaternion q) {
    SCurve ret;
    ZERO(&ret);

    ret.h = a->h;
    ret.isExact = a->isExact;
    ret.exact = (a->exact).TransformedBy(t, q);
    ret.surfA = a->surfA;
    ret.surfB = a->surfB;
    
    Vector *p;
    for(p = a->pts.First(); p; p = a->pts.NextAfter(p)) {
        Vector pp = (q.Rotate(*p)).Plus(t);
        ret.pts.Add(&pp);
    }
    return ret;
}

void SCurve::Clear(void) {
    pts.Clear();
}

STrimBy STrimBy::EntireCurve(SShell *shell, hSCurve hsc, bool backwards) {
    STrimBy stb;
    ZERO(&stb);
    stb.curve = hsc;
    SCurve *sc = shell->curve.FindById(hsc);

    if(backwards) {
        stb.finish = sc->pts.elem[0];
        stb.start = sc->pts.elem[sc->pts.n - 1];
        stb.backwards = true;
    } else {
        stb.start = sc->pts.elem[0];
        stb.finish = sc->pts.elem[sc->pts.n - 1];
        stb.backwards = false;
    }

    return stb;
}

