//-----------------------------------------------------------------------------
// Anything involving surfaces and sets of surfaces (i.e., shells); except
// for the real math, which is in ratpoly.cpp.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

SSurface SSurface::FromExtrusionOf(SBezier *sb, Vector t0, Vector t1) {
    SSurface ret = {};

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

bool SSurface::IsExtrusion(SBezier *of, Vector *alongp) const {
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
                            Vector *start, Vector *finish) const
{
    SBezier sb;
    if(!IsExtrusion(&sb, axis)) return false;
    if(!sb.IsCircle(*axis, center, r)) return false;

    *start = sb.ctrl[0];
    *finish = sb.ctrl[2];
    return true;
}

// Create a surface patch by revolving and possibly translating a curve.
// Works for sections up to but not including 180 degrees.
SSurface SSurface::FromRevolutionOf(SBezier *sb, Vector pt, Vector axis, double thetas,
                                    double thetaf, double dists,
                                    double distf) { // s is start, f is finish
    SSurface ret = {};
    ret.degm = sb->deg;
    ret.degn = 2;

    double dtheta = fabs(WRAP_SYMMETRIC(thetaf - thetas, 2*PI));
    double w      = cos(dtheta / 2);

    // Revolve the curve about the z axis
    int i;
    for(i = 0; i <= ret.degm; i++) {
        Vector p = sb->ctrl[i];

        Vector ps = p.RotatedAbout(pt, axis, thetas),
               pf = p.RotatedAbout(pt, axis, thetaf);

        // The middle control point should be at the intersection of the tangents at ps and pf.
        // This is equivalent but works for 0 <= angle < 180 degrees.
        Vector mid = ps.Plus(pf).ScaledBy(0.5);
        Vector c   = ps.ClosestPointOnLine(pt, axis);
        Vector ct  = mid.Minus(c).ScaledBy(1 / (w * w)).Plus(c);

        // not sure this is needed
        if(ps.Equals(pf)) {
            ps = c;
            ct = c;
            pf = c;
        }
        // moving along the axis can create hilical surfaces (or straight extrusion if
        // thetas==thetaf)
        ret.ctrl[i][0] = ps.Plus(axis.ScaledBy(dists));
        ret.ctrl[i][1] = ct.Plus(axis.ScaledBy((dists + distf) / 2));
        ret.ctrl[i][2] = pf.Plus(axis.ScaledBy(distf));

        ret.weight[i][0] = sb->weight[i];
        ret.weight[i][1] = sb->weight[i] * w;
        ret.weight[i][2] = sb->weight[i];
    }

    return ret;
}

SSurface SSurface::FromPlane(Vector pt, Vector u, Vector v) {
    SSurface ret = {};

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

SSurface SSurface::FromTransformationOf(SSurface *a, Vector t, Quaternion q, double scale,
                                        bool includingTrims)
{
    bool needRotate    = !EXACT(q.vx == 0.0 && q.vy == 0.0 && q.vz == 0.0 && q.w == 1.0);
    bool needTranslate = !EXACT(t.x  == 0.0 && t.y  == 0.0 && t.z  == 0.0);
    bool needScale     = !EXACT(scale == 1.0);

    SSurface ret = {};
    ret.h = a->h;
    ret.color = a->color;
    ret.face = a->face;

    ret.degm = a->degm;
    ret.degn = a->degn;
    int i, j;
    for(i = 0; i <= 3; i++) {
        for(j = 0; j <= 3; j++) {
            Vector ctrl = a->ctrl[i][j];
            if(needScale) {
                ctrl = ctrl.ScaledBy(scale);
            }
            if(needRotate) {
                ctrl = q.Rotate(ctrl);
            }
            if(needTranslate) {
                ctrl = ctrl.Plus(t);
            }
            ret.ctrl[i][j] = ctrl;
            ret.weight[i][j] = a->weight[i][j];
        }
    }

    if(includingTrims) {
        STrimBy *stb;
        ret.trim.ReserveMore(a->trim.n);
        for(stb = a->trim.First(); stb; stb = a->trim.NextAfter(stb)) {
            STrimBy n = *stb;
            if(needScale) {
                n.start  = n.start.ScaledBy(scale);
                n.finish = n.finish.ScaledBy(scale);
            }
            if(needRotate) {
                n.start  = q.Rotate(n.start);
                n.finish = q.Rotate(n.finish);
            }
            if(needTranslate) {
                n.start  = n.start.Plus(t);
                n.finish = n.finish.Plus(t);
            }
            ret.trim.Add(&n);
        }
    }

    if(scale < 0) {
        // If we mirror every surface of a shell, then it will end up inside
        // out. So fix that here.
        ret.Reverse();
    }

    return ret;
}

void SSurface::GetAxisAlignedBounding(Vector *ptMax, Vector *ptMin) const {
    *ptMax = Vector::From(VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE);
    *ptMin = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);

    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            (ctrl[i][j]).MakeMaxMin(ptMax, ptMin);
        }
    }
}

bool SSurface::LineEntirelyOutsideBbox(Vector a, Vector b, bool asSegment) const {
    Vector amax, amin;
    GetAxisAlignedBounding(&amax, &amin);
    if(!Vector::BoundingBoxIntersectsLine(amax, amin, a, b, asSegment)) {
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
void SSurface::MakeTrimEdgesInto(SEdgeList *sel, MakeAs flags,
                                 SCurve *sc, STrimBy *stb)
{
    Vector prev = Vector::From(0, 0, 0);
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
        Vector tpt, *pt = &(sc->pts[i].p);

        if(flags == MakeAs::UV) {
            ClosestPointTo(*pt, &u, &v);
            tpt = Vector::From(u, v, 0);
        } else {
            tpt = *pt;
        }

        if(inCurve) {
            sel->AddEdge(prev, tpt, sc->h.v, stb->backwards);
            empty = false;
        }

        prev = tpt;     // either uv or xyz, depending on flags

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
void SSurface::MakeEdgesInto(SShell *shell, SEdgeList *sel, MakeAs flags,
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

        MakeTrimEdgesInto(sel, flags, sc, stb);
    }
}

//-----------------------------------------------------------------------------
// Compute the exact tangent to the intersection curve between two surfaces,
// by taking the cross product of the surface normals. We choose the direction
// of this tangent so that its dot product with dir is positive.
//-----------------------------------------------------------------------------
Vector SSurface::ExactSurfaceTangentAt(Vector p, SSurface *srfA, SSurface *srfB, Vector dir)
{
    Point2d puva, puvb;
    srfA->ClosestPointTo(p, &puva);
    srfB->ClosestPointTo(p, &puvb);
    Vector ts = (srfA->NormalAt(puva)).Cross(
                (srfB->NormalAt(puvb)));
    ts = ts.WithMagnitude(1);
    if(ts.Dot(dir) < 0) {
        ts = ts.ScaledBy(-1);
    }
    return ts;
}

//-----------------------------------------------------------------------------
// Report our trim curves. If a trim curve is exact and sbl is not null, then
// add its exact form to sbl. Otherwise, add its piecewise linearization to
// sel.
//-----------------------------------------------------------------------------
void SSurface::MakeSectionEdgesInto(SShell *shell, SEdgeList *sel, SBezierList *sbl)
{
    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        SCurve *sc = shell->curve.FindById(stb->curve);
        SBezier *sb = &(sc->exact);

        if(sbl && sc->isExact && (sb->deg != 1 || !sel)) {
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
        } else if(sbl && !sel && !sc->isExact) {
            // We must approximate this trim curve, as piecewise cubic sections.
            SSurface *srfA = shell->surface.FindById(sc->surfA);
            SSurface *srfB = shell->surface.FindById(sc->surfB);

            Vector s = stb->backwards ? stb->finish : stb->start,
                   f = stb->backwards ? stb->start : stb->finish;

            int sp, fp;
            for(sp = 0; sp < sc->pts.n; sp++) {
                if(s.Equals(sc->pts[sp].p)) break;
            }
            if(sp >= sc->pts.n) return;
            for(fp = sp; fp < sc->pts.n; fp++) {
                if(f.Equals(sc->pts[fp].p)) break;
            }
            if(fp >= sc->pts.n) return;
            // So now the curve we want goes from elem[sp] to elem[fp]

            while(sp < fp) {
                // Initially, we'll try approximating the entire trim curve
                // as a single Bezier segment
                int fpt = fp;

                for(;;) {
                    // So construct a cubic Bezier with the correct endpoints
                    // and tangents for the current span.
                    Vector st = sc->pts[sp].p,
                           ft = sc->pts[fpt].p,
                           sf = ft.Minus(st);
                    double m = sf.Magnitude() / 3;

                    Vector stan = ExactSurfaceTangentAt(st, srfA, srfB, sf),
                           ftan = ExactSurfaceTangentAt(ft, srfA, srfB, sf);

                    SBezier sb = SBezier::From(st,
                                               st.Plus (stan.WithMagnitude(m)),
                                               ft.Minus(ftan.WithMagnitude(m)),
                                               ft);

                    // And test how much this curve deviates from the
                    // intermediate points (if any).
                    int i;
                    bool tooFar = false;
                    for(i = sp + 1; i <= (fpt - 1); i++) {
                        Vector p = sc->pts[i].p;
                        double t;
                        sb.ClosestPointTo(p, &t, /*mustConverge=*/false);
                        Vector pp = sb.PointAt(t);
                        if((pp.Minus(p)).Magnitude() > SS.ChordTolMm()/2) {
                            tooFar = true;
                            break;
                        }
                    }

                    if(tooFar) {
                        // Deviates by too much, so try a shorter span
                        fpt--;
                        continue;
                    } else {
                        // Okay, so use this piece and break.
                        sbl->l.Add(&sb);
                        break;
                    }
                }

                // And continue interpolating, starting wherever the curve
                // we just generated finishes.
                sp = fpt;
            }
        } else {
            if(sel) MakeTrimEdgesInto(sel, MakeAs::XYZ, sc, stb);
        }
    }
}

void SSurface::TriangulateInto(SShell *shell, SMesh *sm) {
    SEdgeList el = {};

    MakeEdgesInto(shell, &el, MakeAs::UV);

    SPolygon poly = {};
    if(el.AssemblePolygon(&poly, NULL, /*keepDir=*/true)) {
        int i, start = sm->l.n;
        if(degm == 1 && degn == 1) {
            // A surface with curvature along one direction only; so
            // choose the triangulation with chords that lie as much
            // as possible within the surface. And since the trim curves
            // have been pwl'd to within the desired chord tol, that will
            // produce a surface good to within roughly that tol.
            //
            // If this is just a plane (degree (1, 1)) then the triangulation
            // code will notice that, and not bother checking chord tols.
            poly.UvTriangulateInto(sm, this);
        } else {
            // A surface with compound curvature. So we must overlay a
            // two-dimensional grid, and triangulate around that.
            poly.UvGridTriangulateInto(sm, this);
        }

        STriMeta meta = { face, color };
        for(i = start; i < sm->l.n; i++) {
            STriangle *st = &(sm->l[i]);
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
void SSurface::Reverse() {
    int i, j;
    for(i = 0; i < (degm+1)/2; i++) {
        for(j = 0; j <= degn; j++) {
            swap(ctrl[i][j], ctrl[degm-i][j]);
            swap(weight[i][j], weight[degm-i][j]);
        }
    }

    STrimBy *stb;
    for(stb = trim.First(); stb; stb = trim.NextAfter(stb)) {
        stb->backwards = !stb->backwards;
        swap(stb->start, stb->finish);
    }
}

void SSurface::ScaleSelfBy(double s) {
    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            ctrl[i][j] = ctrl[i][j].ScaledBy(s);
        }
    }
}

void SSurface::Clear() {
    trim.Clear();
}


