#include "../solvespace.h"

// Converge it to better than LENGTH_EPS; we want two points, each
// independently projected into uv and back, to end up equal with the
// LENGTH_EPS. Best case that requires LENGTH_EPS/2, but more is better
// and convergence should be fast by now.
#define RATPOLY_EPS (LENGTH_EPS/(1e2))

static double Bernstein(int k, int deg, double t)
{
    if(k > deg || k < 0) return 0;

    switch(deg) {
        case 0:
            return 1;
            break;

        case 1:
            if(k == 0) {
                return (1 - t);
            } else if(k = 1) {
                return t;
            }
            break;

        case 2:
            if(k == 0) {
                return (1 - t)*(1 - t);
            } else if(k == 1) {
                return 2*(1 - t)*t;
            } else if(k == 2) {
                return t*t;
            }
            break;

        case 3:
            if(k == 0) {
                return (1 - t)*(1 - t)*(1 - t);
            } else if(k == 1) {
                return 3*(1 - t)*(1 - t)*t;
            } else if(k == 2) {
                return 3*(1 - t)*t*t;
            } else if(k == 3) {
                return t*t*t;
            }
            break;
    }
    oops();
}

double BernsteinDerivative(int k, int deg, double t)
{
    switch(deg) {
        case 0:
            return 0;
            break;

        case 1:
            if(k == 0) {
                return -1;
            } else if(k = 1) {
                return 1;
            }
            break;

        case 2:
            if(k == 0) {
                return -2 + 2*t;
            } else if(k == 1) {
                return 2 - 4*t;
            } else if(k == 2) {
                return 2*t;
            }
            break;

        case 3:
            if(k == 0) {
                return -3 + 6*t - 3*t*t;
            } else if(k == 1) {
                return 3 - 12*t + 9*t*t;
            } else if(k == 2) {
                return 6*t - 9*t*t;
            } else if(k == 3) {
                return 3*t*t;
            }
            break;
    }
    oops();
}

SBezier SBezier::From(Vector p0, Vector p1) {
    SBezier ret;
    ZERO(&ret);
    ret.deg = 1;
    ret.weight[0] = ret.weight[1] = 1;
    ret.ctrl[0] = p0;
    ret.ctrl[1] = p1;
    return ret;
}

SBezier SBezier::From(Vector p0, Vector p1, Vector p2) {
    SBezier ret;
    ZERO(&ret);
    ret.deg = 2;
    ret.weight[0] = ret.weight[1] = ret.weight[2] = 1;
    ret.ctrl[0] = p0;
    ret.ctrl[1] = p1;
    ret.ctrl[2] = p2;
    return ret;
}

SBezier SBezier::From(Vector p0, Vector p1, Vector p2, Vector p3) {
    SBezier ret;
    ZERO(&ret);
    ret.deg = 3;
    ret.weight[0] = ret.weight[1] = ret.weight[2] = ret.weight[3] = 1;
    ret.ctrl[0] = p0;
    ret.ctrl[1] = p1;
    ret.ctrl[2] = p2;
    ret.ctrl[3] = p3;
    return ret;
}

Vector SBezier::Start(void) {
    return ctrl[0];
}

Vector SBezier::Finish(void) {
    return ctrl[deg];
}

Vector SBezier::PointAt(double t) {
    Vector pt = Vector::From(0, 0, 0);
    double d = 0;

    int i;
    for(i = 0; i <= deg; i++) {
        double B = Bernstein(i, deg, t);
        pt = pt.Plus(ctrl[i].ScaledBy(B*weight[i]));
        d += weight[i]*B;
    }
    pt = pt.ScaledBy(1.0/d);
    return pt;
}

void SBezier::MakePwlInto(List<Vector> *l) {
    l->Add(&(ctrl[0]));
    MakePwlWorker(l, 0.0, 1.0);
}
void SBezier::MakePwlWorker(List<Vector> *l, double ta, double tb) {
    Vector pa = PointAt(ta);
    Vector pb = PointAt(tb);

    // Can't test in the middle, or certain cubics would break.
    double tm1 = (2*ta + tb) / 3;
    double tm2 = (ta + 2*tb) / 3;

    Vector pm1 = PointAt(tm1);
    Vector pm2 = PointAt(tm2);

    double d = max(pm1.DistanceToLine(pa, pb.Minus(pa)),
                   pm2.DistanceToLine(pa, pb.Minus(pa)));

    double step = 1.0/SS.maxSegments;
    if((tb - ta) < step || d < SS.ChordTolMm()) {
        // A previous call has already added the beginning of our interval.
        l->Add(&pb);
    } else {
        double tm = (ta + tb) / 2;
        MakePwlWorker(l, ta, tm);
        MakePwlWorker(l, tm, tb);
    }
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

SSurface SSurface::FromExtrusionOf(SBezier *sb, Vector t0, Vector t1) {
    SSurface ret;
    ZERO(&ret);

    ret.degm = sb->deg;
    ret.degn = 1;

    int i;
    for(i = 0; i <= ret.degm; i++) {
        ret.ctrl[i][0] = (sb->ctrl[i]).Plus(t0);
        ret.weight[i][0] = sb->weight[i];

        ret.ctrl[i][1] = (sb->ctrl[i]).Plus(t1);
        ret.weight[i][1] = sb->weight[i];
    }

    return ret;
}

bool SSurface::IsExtrusion(SBezier *of, Vector *alongp) {
    int i;

    if(degn != 1) return false;

    Vector along = (ctrl[0][1]).Minus(ctrl[0][0]);
    for(i = 0; i <= degm; i++) {
        if((fabs(weight[i][1] - weight[i][0]) < LENGTH_EPS) &&
           ((ctrl[i][1]).Minus(ctrl[i][0])).Equals(along))
        {
            continue;
        }
        return false;
    }

    // yes, we are a surface of extrusion; copy the original curve and return
    if(of) {
        for(i = 0; i <= degm; i++) {
            of->weight[i] = weight[i][0];
            of->ctrl[i] = ctrl[i][0];
        }
        of->deg = degm;
        *alongp = along;
    }
    return true;
}

bool SSurface::IsCylinder(Vector *center, Vector *axis, double *r,
                             Vector *start, Vector *finish)
{
    SBezier sb;
    if(!IsExtrusion(&sb, axis)) return false;
    if(sb.deg != 2) return false;

    Vector t0 = (sb.ctrl[0]).Minus(sb.ctrl[1]),
           t2 = (sb.ctrl[2]).Minus(sb.ctrl[1]),
           r0 = axis->Cross(t0),
           r2 = axis->Cross(t2);

    *center = Vector::AtIntersectionOfLines(sb.ctrl[0], (sb.ctrl[0]).Plus(r0),
                                            sb.ctrl[2], (sb.ctrl[2]).Plus(r2),
                                            NULL, NULL, NULL);

    double rd0 = center->Minus(sb.ctrl[0]).Magnitude(),
           rd2 = center->Minus(sb.ctrl[2]).Magnitude();
    if(fabs(rd0 - rd2) > LENGTH_EPS) {
        return false;
    }
    *r = rd0;

    Vector u = r0.WithMagnitude(1),
           v = (axis->Cross(u)).WithMagnitude(1);
    Point2d c2  = center->Project2d(u, v),
            pa2 = (sb.ctrl[0]).Project2d(u, v).Minus(c2),
            pb2 = (sb.ctrl[2]).Project2d(u, v).Minus(c2);
    
    double thetaa = atan2(pa2.y, pa2.x), // in fact always zero due to csys
           thetab = atan2(pb2.y, pb2.x),
           dtheta = WRAP_NOT_0(thetab - thetaa, 2*PI);
    if(dtheta > PI) {
        // Not possible with a second order Bezier arc; so we must have
        // the points backwards.
        dtheta = 2*PI - dtheta;
    }

    if(fabs(sb.weight[1] - cos(dtheta/2)) > LENGTH_EPS) {
        return false;
    }

    *start  = sb.ctrl[0];
    *finish = sb.ctrl[2];

    return true;
}

SSurface SSurface::FromPlane(Vector pt, Vector u, Vector v) {
    SSurface ret;
    ZERO(&ret);

    ret.degm = 1;
    ret.degn = 1;

    ret.weight[0][0] = ret.weight[0][1] = 1; 
    ret.weight[1][0] = ret.weight[1][1] = 1;

    ret.ctrl[0][0] = pt;
    ret.ctrl[0][1] = pt.Plus(u);
    ret.ctrl[1][0] = pt.Plus(v);
    ret.ctrl[1][1] = pt.Plus(v).Plus(u);
    
    return ret;
}

SSurface SSurface::FromTransformationOf(SSurface *a, Vector t, Quaternion q,
                                        bool includingTrims)
{
    SSurface ret;
    ZERO(&ret);

    ret.h = a->h;
    ret.color = a->color;
    ret.face = a->face;

    ret.degm = a->degm;
    ret.degn = a->degn;
    int i, j;
    for(i = 0; i <= 3; i++) {
        for(j = 0; j <= 3; j++) {
            ret.ctrl[i][j] = (q.Rotate(a->ctrl[i][j])).Plus(t);
            ret.weight[i][j] = a->weight[i][j];
        }
    }

    if(includingTrims) {
        STrimBy *stb;
        for(stb = a->trim.First(); stb; stb = a->trim.NextAfter(stb)) {
            STrimBy n = *stb;
            n.start  = (q.Rotate(n.start)) .Plus(t);
            n.finish = (q.Rotate(n.finish)).Plus(t);
            ret.trim.Add(&n);
        }
    }

    return ret;
}

Vector SSurface::PointAt(double u, double v) {
    Vector num = Vector::From(0, 0, 0);
    double den = 0;

    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            double Bi = Bernstein(i, degm, u),
                   Bj = Bernstein(j, degn, v);

            num = num.Plus(ctrl[i][j].ScaledBy(Bi*Bj*weight[i][j]));
            den += weight[i][j]*Bi*Bj;
        }
    }
    num = num.ScaledBy(1.0/den);
    return num;
}

void SSurface::TangentsAt(double u, double v, Vector *tu, Vector *tv) {
    Vector num   = Vector::From(0, 0, 0),
           num_u = Vector::From(0, 0, 0),
           num_v = Vector::From(0, 0, 0);
    double den   = 0,
           den_u = 0,
           den_v = 0;

    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            double Bi  = Bernstein(i, degm, u),
                   Bj  = Bernstein(j, degn, v),
                   Bip = BernsteinDerivative(i, degm, u),
                   Bjp = BernsteinDerivative(j, degn, v);

            num = num.Plus(ctrl[i][j].ScaledBy(Bi*Bj*weight[i][j]));
            den += weight[i][j]*Bi*Bj;

            num_u = num_u.Plus(ctrl[i][j].ScaledBy(Bip*Bj*weight[i][j]));
            den_u += weight[i][j]*Bip*Bj;

            num_v = num_v.Plus(ctrl[i][j].ScaledBy(Bi*Bjp*weight[i][j]));
            den_v += weight[i][j]*Bi*Bjp;
        }
    }
    // Quotient rule; f(t) = n(t)/d(t), so f' = (n'*d - n*d')/(d^2)
    *tu = ((num_u.ScaledBy(den)).Minus(num.ScaledBy(den_u)));
    *tu = tu->ScaledBy(1.0/(den*den));

    *tv = ((num_v.ScaledBy(den)).Minus(num.ScaledBy(den_v)));
    *tv = tv->ScaledBy(1.0/(den*den));
}

Vector SSurface::NormalAt(double u, double v) {
    Vector tu, tv;
    TangentsAt(u, v, &tu, &tv);
    return tu.Cross(tv);
}

void SSurface::ClosestPointTo(Vector p, double *u, double *v, bool converge) {
    int i, j;

    if(degm == 1 && degn == 1) {
        *u = *v = 0; // a plane, perfect no matter what the initial guess
    } else {
        double minDist = VERY_POSITIVE;
        double res = (max(degm, degn) == 2) ? 7.0 : 20.0;
        for(i = 0; i < (int)res; i++) {
            for(j = 0; j <= (int)res; j++) {
                double tryu = (i/res), tryv = (j/res);
                
                Vector tryp = PointAt(tryu, tryv);
                double d = (tryp.Minus(p)).Magnitude();
                if(d < minDist) {
                    *u = tryu;
                    *v = tryv;
                    minDist = d;
                }
            }
        }
    }

    // Initial guess is in u, v
    Vector p0;
    for(i = 0; i < (converge ? 15 : 3); i++) {
        p0 = PointAt(*u, *v);
        if(converge) {
            if(p0.Equals(p, RATPOLY_EPS)) {
                return;
            }
        }

        Vector tu, tv;
        TangentsAt(*u, *v, &tu, &tv);
        
        // Project the point into a plane through p0, with basis tu, tv; a
        // second-order thing would converge faster but needs second
        // derivatives.
        Vector dp = p.Minus(p0);
        double du = dp.Dot(tu), dv = dp.Dot(tv);
        *u += du / (tu.MagSquared());
        *v += dv / (tv.MagSquared());
    }

    if(converge) {
        dbp("didn't converge");
        dbp("have %.3f %.3f %.3f", CO(p0));
        dbp("want %.3f %.3f %.3f", CO(p));
        dbp("distance = %g", (p.Minus(p0)).Magnitude());
    }

    if(isnan(*u) || isnan(*v)) {
        *u = *v = 0;
    }
}

bool SSurface::PointIntersectingLine(Vector p0, Vector p1, double *u, double *v)
{
    int i;
    for(i = 0; i < 15; i++) {
        Vector pi, p, tu, tv;
        p = PointAt(*u, *v);
        TangentsAt(*u, *v, &tu, &tv);
        
        Vector n = (tu.Cross(tv)).WithMagnitude(1);
        double d = p.Dot(n);

        bool parallel;
        pi = Vector::AtIntersectionOfPlaneAndLine(n, d, p0, p1, &parallel);
        if(parallel) break;

        // Check for convergence
        if(pi.Equals(p, RATPOLY_EPS)) return true;

        // Adjust our guess and iterate
        Vector dp = pi.Minus(p);
        double du = dp.Dot(tu), dv = dp.Dot(tv);
        *u += du / (tu.MagSquared());
        *v += dv / (tv.MagSquared());
    }
//    dbp("didn't converge (surface intersecting line)");
    return false;
}

void SSurface::PointOnSurfaces(SSurface *s1, SSurface *s2,
                                                double *up, double *vp)
{
    double u[3] = { *up, 0, 0 }, v[3] = { *vp, 0, 0 };
    SSurface *srf[3] = { this, s1, s2 };

    // Get initial guesses for (u, v) in the other surfaces
    Vector p = PointAt(*u, *v);
    (srf[1])->ClosestPointTo(p, &(u[1]), &(v[1]), false);
    (srf[2])->ClosestPointTo(p, &(u[2]), &(v[2]), false);

    int i, j;
    for(i = 0; i < 15; i++) {
        // Approximate each surface by a plane
        Vector p[3], tu[3], tv[3], n[3];
        double d[3];
        for(j = 0; j < 3; j++) {
            p[j] = (srf[j])->PointAt(u[j], v[j]);
            (srf[j])->TangentsAt(u[j], v[j], &(tu[j]), &(tv[j]));
            n[j] = ((tu[j]).Cross(tv[j])).WithMagnitude(1);
            d[j] = (n[j]).Dot(p[j]);
        }

        // If a = b and b = c, then does a = c? No, it doesn't.
        if((p[0]).Equals(p[1], RATPOLY_EPS) && 
           (p[1]).Equals(p[2], RATPOLY_EPS) &&
           (p[2]).Equals(p[0], RATPOLY_EPS))
        {
            *up = u[0];
            *vp = v[0];
            return;
        }

        bool parallel;
        Vector pi = Vector::AtIntersectionOfPlanes(n[0], d[0],
                                                   n[1], d[1],
                                                   n[2], d[2], &parallel);
        if(parallel) break;

        for(j = 0; j < 3; j++) {
            Vector dp = pi.Minus(p[j]);
            double du = dp.Dot(tu[j]), dv = dp.Dot(tv[j]);
            u[j] += du / (tu[j]).MagSquared();
            v[j] += dv / (tv[j]).MagSquared();
        }
    }
    dbp("didn't converge (three surfaces intersecting)");
}

void SSurface::GetAxisAlignedBounding(Vector *ptMax, Vector *ptMin) {
    *ptMax = Vector::From(VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE);
    *ptMin = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);

    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            (ctrl[i][j]).MakeMaxMin(ptMax, ptMin);
        }
    }
}

bool SSurface::LineEntirelyOutsideBbox(Vector a, Vector b, bool segment) {
    Vector amax, amin;
    GetAxisAlignedBounding(&amax, &amin);
    if(!Vector::BoundingBoxIntersectsLine(amax, amin, a, b, segment)) {
        // The line segment could fail to intersect the bbox, but lie entirely
        // within it and intersect the surface.
        if(a.OutsideAndNotOn(amax, amin) && b.OutsideAndNotOn(amax, amin)) {
            return true;
        }
    }
    return false;
}

void SSurface::MakeEdgesInto(SShell *shell, SEdgeList *sel, bool asUv,
                             SShell *useCurvesFrom) {
    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        SCurve *sc = shell->curve.FindById(stb->curve);

        // We have the option to use the curves from another shell; this
        // is relevant when generating the coincident edges while doing the
        // Booleans, since the curves from the output shell will be split
        // against any intersecting surfaces (and the originals aren't).
        if(useCurvesFrom) {
            sc = useCurvesFrom->curve.FindById(sc->newH);
        }

        Vector prev, prevuv, ptuv;
        bool inCurve = false, empty = true;
        double u = 0, v = 0;
   
        int i, first, last, increment;
        if(stb->backwards) {
            first = sc->pts.n - 1;
            last = 0;
            increment = -1;
        } else {
            first = 0;
            last = sc->pts.n - 1;
            increment = 1;
        }
        for(i = first; i != (last + increment); i += increment) {
            Vector *pt = &(sc->pts.elem[i]);
            if(asUv) {
                ClosestPointTo(*pt, &u, &v);
                ptuv = Vector::From(u, v, 0);
                if(inCurve) {
                    sel->AddEdge(prevuv, ptuv, sc->h.v, stb->backwards);
                    empty = false;
                }
                prevuv = ptuv;
            } else {
                if(inCurve) {
                    sel->AddEdge(prev, *pt, sc->h.v, stb->backwards);
                    empty = false;
                }
                prev = *pt;
            }

            if(pt->Equals(stb->start)) inCurve = true;
            if(pt->Equals(stb->finish)) inCurve = false;
        }
        if(inCurve) dbp("trim was unterminated");
        if(empty)   dbp("trim was empty");
    }
}

void SSurface::TriangulateInto(SShell *shell, SMesh *sm) {
    SEdgeList el;
    ZERO(&el);

    MakeEdgesInto(shell, &el, true);

    SPolygon poly;
    ZERO(&poly);
    if(el.AssemblePolygon(&poly, NULL, true)) {
        int i, start = sm->l.n;
        // Curved surfaces are triangulated in such a way as to minimize
        // deviation between edges and surface; but doesn't matter for planes.
        poly.UvTriangulateInto(sm, (degm == 1 && degn == 1) ? NULL : this);

        STriMeta meta = { face, color };
        for(i = start; i < sm->l.n; i++) {
            STriangle *st = &(sm->l.elem[i]);
            st->meta = meta;
            st->an = NormalAt(st->a.x, st->a.y);
            st->bn = NormalAt(st->b.x, st->b.y);
            st->cn = NormalAt(st->c.x, st->c.y);
            st->a = PointAt(st->a.x, st->a.y);
            st->b = PointAt(st->b.x, st->b.y);
            st->c = PointAt(st->c.x, st->c.y);
            // Works out that my chosen contour direction is inconsistent with
            // the triangle direction, sigh.
            st->FlipNormal();
        }
    } else {
        dbp("failed to assemble polygon to trim nurbs surface in uv space");
    }

    el.Clear();
    poly.Clear();
}

//-----------------------------------------------------------------------------
// Reverse the parametrisation of one of our dimensions, which flips the
// normal. We therefore must reverse all our trim curves too. The uv
// coordinates change, but trim curves are stored as xyz so nothing happens
//-----------------------------------------------------------------------------
void SSurface::Reverse(void) {
    int i, j;
    for(i = 0; i < (degm+1)/2; i++) {
        for(j = 0; j <= degn; j++) {
            SWAP(Vector, ctrl[i][j], ctrl[degm-i][j]);
            SWAP(double, weight[i][j], weight[degm-i][j]);
        }
    }

    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        stb->backwards = !stb->backwards;
        SWAP(Vector, stb->start, stb->finish);
    }
}

void SSurface::Clear(void) {
    trim.Clear();
}

void SShell::MakeFromExtrusionOf(SBezierLoopSet *sbls, Vector t0, Vector t1,
                                 int color)
{
    ZERO(this);

    // Make the extrusion direction consistent with respect to the normal
    // of the sketch we're extruding.
    if((t0.Minus(t1)).Dot(sbls->normal) < 0) {
        SWAP(Vector, t0, t1);
    }

    // Define a coordinate system to contain the original sketch, and get
    // a bounding box in that csys
    Vector n = sbls->normal.ScaledBy(-1);
    Vector u = n.Normal(0), v = n.Normal(1);
    Vector orig = sbls->point;
    double umax = 1e-10, umin = 1e10;
    sbls->GetBoundingProjd(u, orig, &umin, &umax);
    double vmax = 1e-10, vmin = 1e10;
    sbls->GetBoundingProjd(v, orig, &vmin, &vmax);
    // and now fix things up so that all u and v lie between 0 and 1
    orig = orig.Plus(u.ScaledBy(umin));
    orig = orig.Plus(v.ScaledBy(vmin));
    u = u.ScaledBy(umax - umin);
    v = v.ScaledBy(vmax - vmin);

    // So we can now generate the top and bottom surfaces of the extrusion,
    // planes within a translated (and maybe mirrored) version of that csys.
    SSurface s0, s1;
    s0 = SSurface::FromPlane(orig.Plus(t0), u, v);
    s0.color = color;
    s1 = SSurface::FromPlane(orig.Plus(t1).Plus(u), u.ScaledBy(-1), v);
    s1.color = color;
    hSSurface hs0 = surface.AddAndAssignId(&s0),
              hs1 = surface.AddAndAssignId(&s1);
    
    // Now go through the input curves. For each one, generate its surface
    // of extrusion, its two translated trim curves, and one trim line. We
    // go through by loops so that we can assign the lines correctly.
    SBezierLoop *sbl;
    for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
        SBezier *sb;

        typedef struct {
            hSCurve     hc;
            hSSurface   hs;
        } TrimLine;
        List<TrimLine> trimLines;
        ZERO(&trimLines);

        for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
            // Generate the surface of extrusion of this curve, and add
            // it to the list
            SSurface ss = SSurface::FromExtrusionOf(sb, t0, t1);
            ss.color = color;
            hSSurface hsext = surface.AddAndAssignId(&ss);

            // Translate the curve by t0 and t1 to produce two trim curves
            SCurve sc;
            ZERO(&sc);
            sc.isExact = true;
            sc.exact = sb->TransformedBy(t0, Quaternion::IDENTITY);
            (sc.exact).MakePwlInto(&(sc.pts));
            sc.surfA = hs0;
            sc.surfB = hsext;
            hSCurve hc0 = curve.AddAndAssignId(&sc);

            ZERO(&sc);
            sc.isExact = true;
            sc.exact = sb->TransformedBy(t1, Quaternion::IDENTITY);
            (sc.exact).MakePwlInto(&(sc.pts));
            sc.surfA = hs1;
            sc.surfB = hsext;
            hSCurve hc1 = curve.AddAndAssignId(&sc);

            STrimBy stb0, stb1;
            // The translated curves trim the flat top and bottom surfaces.
            stb0 = STrimBy::EntireCurve(this, hc0, false);
            stb1 = STrimBy::EntireCurve(this, hc1, true);
            (surface.FindById(hs0))->trim.Add(&stb0);
            (surface.FindById(hs1))->trim.Add(&stb1);

            // The translated curves also trim the surface of extrusion.
            stb0 = STrimBy::EntireCurve(this, hc0, true);
            stb1 = STrimBy::EntireCurve(this, hc1, false);
            (surface.FindById(hsext))->trim.Add(&stb0);
            (surface.FindById(hsext))->trim.Add(&stb1);

            // And form the trim line
            Vector pt = sb->Finish();
            ZERO(&sc);
            sc.isExact = true;
            sc.exact = SBezier::From(pt.Plus(t0), pt.Plus(t1));
            (sc.exact).MakePwlInto(&(sc.pts));
            hSCurve hl = curve.AddAndAssignId(&sc);
            // save this for later
            TrimLine tl;
            tl.hc = hl;
            tl.hs = hsext;
            trimLines.Add(&tl);
        }

        int i;
        for(i = 0; i < trimLines.n; i++) {
            TrimLine *tl = &(trimLines.elem[i]);
            SSurface *ss = surface.FindById(tl->hs);

            TrimLine *tlp = &(trimLines.elem[WRAP(i-1, trimLines.n)]);

            STrimBy stb;
            stb = STrimBy::EntireCurve(this, tl->hc, true);
            ss->trim.Add(&stb);
            stb = STrimBy::EntireCurve(this, tlp->hc, false);
            ss->trim.Add(&stb);

            (curve.FindById(tl->hc))->surfA = ss->h;
            (curve.FindById(tlp->hc))->surfB = ss->h;
        }
        trimLines.Clear();
    }
}

void SShell::MakeFromCopyOf(SShell *a) {
    MakeFromTransformationOf(a, Vector::From(0, 0, 0), Quaternion::IDENTITY);
}

void SShell::MakeFromTransformationOf(SShell *a, Vector t, Quaternion q) {
    SSurface *s;
    for(s = a->surface.First(); s; s = a->surface.NextAfter(s)) {
        SSurface n;
        n = SSurface::FromTransformationOf(s, t, q, true);
        surface.Add(&n); // keeping the old ID
    }

    SCurve *c;
    for(c = a->curve.First(); c; c = a->curve.NextAfter(c)) {
        SCurve n;
        n = SCurve::FromTransformationOf(c, t, q);
        curve.Add(&n); // keeping the old ID
    }
}

void SShell::MakeEdgesInto(SEdgeList *sel) {
    SSurface *s;
    for(s = surface.First(); s; s = surface.NextAfter(s)) {
        s->MakeEdgesInto(this, sel, false);
    }
}

void SShell::TriangulateInto(SMesh *sm) {
    SSurface *s;
    for(s = surface.First(); s; s = surface.NextAfter(s)) {
        s->TriangulateInto(this, sm);
    }
}

void SShell::Clear(void) {
    SSurface *s;
    for(s = surface.First(); s; s = surface.NextAfter(s)) {
        s->Clear();
    }
    surface.Clear();

    SCurve *c;
    for(c = curve.First(); c; c = curve.NextAfter(c)) {
        c->Clear();
    }
    curve.Clear();
}

