#include "../solvespace.h"

double Bernstein(int k, int deg, double t)
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
    return deg*(Bernstein(k-1, deg-1, t) - Bernstein(k, deg-1, t));
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
    MakePwlInto(l, Vector::From(0, 0, 0));
}
void SBezier::MakePwlInto(List<Vector> *l, Vector offset) {
    Vector p = (ctrl[0]).Plus(offset);
    l->Add(&p);

    MakePwlWorker(l, 0.0, 1.0, offset);
}
void SBezier::MakePwlWorker(List<Vector> *l, double ta, double tb, Vector off) {
    Vector pa = PointAt(ta);
    Vector pb = PointAt(tb);

    // Can't test in the middle, or certain cubics would break.
    double tm1 = (2*ta + tb) / 3;
    double tm2 = (ta + 2*tb) / 3;

    Vector pm1 = PointAt(tm1);
    Vector pm2 = PointAt(tm2);

    double d = max(pm1.DistanceToLine(pa, pb.Minus(pa)),
                   pm2.DistanceToLine(pa, pb.Minus(pa)));

    double tol = SS.chordTol/SS.GW.scale;

    double step = 1.0/SS.maxSegments;
    if((tb - ta) < step || d < tol) {
        // A previous call has already added the beginning of our interval.
        pb = pb.Plus(off);
        l->Add(&pb);
    } else {
        double tm = (ta + tb) / 2;
        MakePwlWorker(l, ta, tm, off);
        MakePwlWorker(l, tm, tb, off);
    }
}

void SBezier::Reverse(void) {
    int i;
    for(i = 0; i < (deg+1)/2; i++) {
        SWAP(Vector, ctrl[i], ctrl[deg-i]);
        SWAP(double, weight[i], weight[deg-i]);
    }
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

void SBezierLoopSet::Clear(void) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).Clear();
    }
    l.Clear();
}

void SCurve::Clear(void) {
    pts.Clear();
}

STrimBy STrimBy::EntireCurve(SShell *shell, hSCurve hsc) {
    STrimBy stb;
    ZERO(&stb);
    stb.curve = hsc;
    SCurve *sc = shell->curve.FindById(hsc);
    stb.start = sc->pts.elem[0];
    stb.finish = sc->pts.elem[sc->pts.n - 1];

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

SSurface SSurface::FromPlane(Vector pt, Vector n) {
    SSurface ret;
    ZERO(&ret);

    ret.degm = 1;
    ret.degn = 1;

    Vector u = n.Normal(0), v = n.Normal(1);

    ret.weight[0][0] = ret.weight[0][1] = 1; 
    ret.weight[1][0] = ret.weight[1][1] = 1;

    ret.ctrl[0][0] = pt;
    ret.ctrl[0][1] = pt.Plus(u);
    ret.ctrl[1][0] = pt.Plus(v);
    ret.ctrl[1][1] = pt.Plus(v).Plus(u);
    
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

void SSurface::ClosestPointTo(Vector p, double *u, double *v) {
    int i, j;
    double minDist = 1e10;
    double res = 7.0;
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

    // Initial guess is in u, v
    Vector p0;
    for(i = 0; i < 50; i++) {
        p0 = PointAt(*u, *v);
        if(p0.Equals(p)) {
            return;
        }

        Vector tu, tv;
        TangentsAt(*u, *v, &tu, &tv);
        
        // Project the point into a plane through p0, with basis tu, tv; a
        // second-order thing would converge faster but needs second
        // derivatives.
        Vector dp = p.Minus(p0);
        double du = dp.Dot(tu), dv = dp.Dot(tv);
        *u += du / (tu.MagSquared());
        *v += dv / (tu.MagSquared());
    }
    dbp("didn't converge");
    dbp("have %.3f %.3f %.3f", CO(p0));
    dbp("want %.3f %.3f %.3f", CO(p));
    if(isnan(*u) || isnan(*v)) {
        *u = *v = 0;
    }
}

void SSurface::TriangulateInto(SShell *shell, SMesh *sm) {
    SEdgeList el;
    ZERO(&el);

    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        SCurve *sc = shell->curve.FindById(stb->curve);

        Vector prevuv, ptuv;
        bool inCurve = false;
        Vector *pt;
        double u = 0, v = 0;
        for(pt = sc->pts.First(); pt; pt = sc->pts.NextAfter(pt)) {
            ClosestPointTo(*pt, &u, &v);
            ptuv = Vector::From(u, v, 0);

            if(inCurve) {
                el.AddEdge(prevuv, ptuv);
            }
            prevuv = ptuv;

            if(pt->EqualsExactly(stb->start)) inCurve = true;
            if(pt->EqualsExactly(stb->finish)) inCurve = false;
        }
    }
    
    SPolygon poly;
    ZERO(&poly);
    if(!el.AssemblePolygon(&poly, NULL)) {
        dbp("failed to assemble polygon to trim nurbs surface in uv space");
    }

    int i, start = sm->l.n;
    poly.UvTriangulateInto(sm);

    STriMeta meta = { 0, 0x888888 };
    for(i = start; i < sm->l.n; i++) {
        STriangle *st = &(sm->l.elem[i]);
        st->meta = meta;
        st->an = NormalAt(st->a.x, st->a.y);
        st->bn = NormalAt(st->b.x, st->b.y);
        st->cn = NormalAt(st->c.x, st->c.y);
        st->a = PointAt(st->a.x, st->a.y);
        st->b = PointAt(st->b.x, st->b.y);
        st->c = PointAt(st->c.x, st->c.y);
        if((st->Normal()).Dot(st->an) < 0) {
            // Have to get the vertices in the right order
            st->FlipNormal();
        }
    }

    el.Clear();
    poly.Clear();
}

void SSurface::Clear(void) {
    trim.Clear();
}

SShell SShell::FromExtrusionOf(SBezierLoopSet *sbls, Vector t0, Vector t1) {
    SShell ret;
    ZERO(&ret);

    // Make the extrusion direction consistent with respect to the normal
    // of the sketch we're extruding.
    if((t0.Minus(t1)).Dot(sbls->normal) < 0) {
        SWAP(Vector, t0, t1);
    }

    // First, generate the top and bottom surfaces of the extrusion; just
    // planes.
    SSurface s0, s1;
    s0 = SSurface::FromPlane(sbls->point.Plus(t0), sbls->normal.ScaledBy(-1));
    s1 = SSurface::FromPlane(sbls->point.Plus(t1), sbls->normal.ScaledBy( 1));
    hSSurface hs0 = ret.surface.AddAndAssignId(&s0),
              hs1 = ret.surface.AddAndAssignId(&s1);
    
    // Now go through the input curves. For each one, generate its surface
    // of extrusion, its two translated trim curves, and one trim line. We
    // go through by loops so that we can assign the lines correctly.
    SBezierLoop *sbl;
    for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
        SBezier *sb;

        typedef struct {
            STrimBy     trim;
            hSSurface   hs;
        } TrimLine;
        List<TrimLine> trimLines;
        ZERO(&trimLines);

        for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
            // Generate the surface of extrusion of this curve, and add
            // it to the list
            SSurface ss = SSurface::FromExtrusionOf(sb, t0, t1);
            hSSurface hsext = ret.surface.AddAndAssignId(&ss);

            // Translate the curve by t0 and t1 to produce two trim curves
            SCurve sc;
            ZERO(&sc);
            sb->MakePwlInto(&(sc.pts), t0);
            hSCurve hc0 = ret.curve.AddAndAssignId(&sc);
            STrimBy stb0 = STrimBy::EntireCurve(&ret, hc0);

            ZERO(&sc);
            sb->MakePwlInto(&(sc.pts), t1);
            hSCurve hc1 = ret.curve.AddAndAssignId(&sc);
            STrimBy stb1 = STrimBy::EntireCurve(&ret, hc1);

            // The translated curves trim the flat top and bottom surfaces.
//            (ret.surface.FindById(hs0))->trim.Add(&stb0);
            (ret.surface.FindById(hs1))->trim.Add(&stb1);

            // The translated curves also trim the surface of extrusion.
//            (ret.surface.FindById(hsext))->trim.Add(&stb0);
//            (ret.surface.FindById(hsext))->trim.Add(&stb1);

            // And form the trim line
            Vector pt = sb->Finish();
            Vector p0 = pt.Plus(t0), p1 = pt.Plus(t1);
            ZERO(&sc);
            sc.pts.Add(&p0);
            sc.pts.Add(&p1);
            hSCurve hl = ret.curve.AddAndAssignId(&sc);
            // save this for later
            TrimLine tl;
            tl.trim = STrimBy::EntireCurve(&ret, hl);
            tl.hs = hsext;
            trimLines.Add(&tl);
        }

        int i;
        for(i = 0; i < trimLines.n; i++) {
            TrimLine *tl = &(trimLines.elem[i]);
            SSurface *ss = ret.surface.FindById(tl->hs);

            TrimLine *tlp = &(trimLines.elem[WRAP(i-1, trimLines.n)]);

//            ss->trim.Add(&(tl->trim));
//            ss->trim.Add(&(tlp->trim));
        }
        trimLines.Clear();
    }

    return ret;
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

