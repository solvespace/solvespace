//-----------------------------------------------------------------------------
// Anything involving surfaces and sets of surfaces (i.e., shells); except
// for the real math, which is in ratpoly.cpp.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

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

bool SSurface::IsCylinder(Vector *axis, Vector *center, double *r,
                            Vector *start, Vector *finish)
{
    SBezier sb;
    if(!IsExtrusion(&sb, axis)) return false;
    if(!sb.IsCircle(*axis, center, r)) return false;

    *start = sb.ctrl[0];
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

//-----------------------------------------------------------------------------
// Generate the piecewise linear approximation of the trim stb, which applies
// to the curve sc.
//-----------------------------------------------------------------------------
void SSurface::MakeTrimEdgesInto(SEdgeList *sel, bool asUv,
                                 SCurve *sc, STrimBy *stb)
{
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

//-----------------------------------------------------------------------------
// Generate all of our trim curves, in piecewise linear form. We can do
// so in either uv or xyz coordinates. And if requested, then we can use
// the split curves from useCurvesFrom instead of the curves in our own
// shell.
//-----------------------------------------------------------------------------
void SSurface::MakeEdgesInto(SShell *shell, SEdgeList *sel, bool asUv,
                             SShell *useCurvesFrom)
{
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

        MakeTrimEdgesInto(sel, asUv, sc, stb);
    }
}

//-----------------------------------------------------------------------------
// Report our trim curves. If a trim curve is exact and sbl is not null, then
// add its exact form to sbl. Otherwise, add its piecewise linearization to
// sel.
//-----------------------------------------------------------------------------
void SSurface::MakeSectionEdgesInto(SShell *shell,
                                    SEdgeList *sel, SBezierList *sbl)
{
    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        SCurve *sc = shell->curve.FindById(stb->curve);
        SBezier *sb = &(sc->exact);

        if(sbl && sc->isExact && sb->deg != 1) {
            double ts, tf;
            if(stb->backwards) {
                sb->ClosestPointTo(stb->start,  &tf);
                sb->ClosestPointTo(stb->finish, &ts);
            } else {
                sb->ClosestPointTo(stb->start,  &ts);
                sb->ClosestPointTo(stb->finish, &tf);
            }
            SBezier junk_bef, keep_aft;
            sb->SplitAt(ts, &junk_bef, &keep_aft);
            // In the kept piece, the range that used to go from ts to 1
            // now goes from 0 to 1; so rescale tf appropriately.
            tf = (tf - ts)/(1 - ts);

            SBezier keep_bef, junk_aft;
            keep_aft.SplitAt(tf, &keep_bef, &junk_aft);

            sbl->l.Add(&keep_bef);
        } else {
            MakeTrimEdgesInto(sel, false, sc, stb);
        }
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

void SShell::MakeSectionEdgesInto(Vector n, double d,
                                 SEdgeList *sel, SBezierList *sbl)
{
    SSurface *s;
    for(s = surface.First(); s; s = surface.NextAfter(s)) {
        if(s->CoincidentWithPlane(n, d)) {
            s->MakeSectionEdgesInto(this, sel, sbl);
        }
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

