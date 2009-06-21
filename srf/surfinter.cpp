#include "solvespace.h"

// Dot product tolerance for perpendicular.
const double SShell::DOTP_TOL = 1e-3;

extern int FLAG;

void SSurface::AddExactIntersectionCurve(SBezier *sb, SSurface *srfB,
                            SShell *agnstA, SShell *agnstB, SShell *into)
{
    SCurve sc;
    ZERO(&sc);
    // Important to keep the order of (surfA, surfB) consistent; when we later
    // rewrite the identifiers, we rewrite surfA from A and surfB from B.
    sc.surfA = h;
    sc.surfB = srfB->h;
    sc.exact = *sb;
    sc.isExact = true;

    // Now we have to piecewise linearize the curve. If there's already an
    // identical curve in the shell, then follow that pwl exactly, otherwise
    // calculate from scratch.
    SCurve split, *existing = NULL, *se;
    SBezier sbrev = *sb;
    sbrev.Reverse();
    bool backwards = false;
    for(se = into->curve.First(); se; se = into->curve.NextAfter(se)) {
        if(se->isExact) {
            if(sb->Equals(&(se->exact))) {
                existing = se;
                break;
            }
            if(sbrev.Equals(&(se->exact))) {
                existing = se;
                backwards = true;
                break;
            }
        }
    }
    if(existing) {
        SCurvePt *v;
        for(v = existing->pts.First(); v; v = existing->pts.NextAfter(v)) {
            sc.pts.Add(v);
        }
        if(backwards) sc.pts.Reverse();
        split = sc;
        ZERO(&sc);
    } else {
        sb->MakePwlInto(&(sc.pts));
        // and split the line where it intersects our existing surfaces
        split = sc.MakeCopySplitAgainst(agnstA, agnstB, this, srfB);
        sc.Clear();
    }

    if(0 && sb->deg == 1) {
        dbp(" ");
        SCurvePt *prev = NULL, *v;
        dbp("split.pts.n =%d", split.pts.n);
        for(v = split.pts.First(); v; v = split.pts.NextAfter(v)) {
            if(prev) {
                Vector e = (prev->p).Minus(v->p).WithMagnitude(-1);
                SS.nakedEdges.AddEdge((prev->p).Plus(e), (v->p).Minus(e));
            }
            prev = v;
        }
    }
    // Nothing should be generating zero-len edges.
    if((sb->Start()).Equals(sb->Finish())) oops();

    split.source = SCurve::FROM_INTERSECTION;
    into->curve.AddAndAssignId(&split);
}

void SSurface::IntersectAgainst(SSurface *b, SShell *agnstA, SShell *agnstB, 
                                SShell *into)
{
    Vector amax, amin, bmax, bmin;
    GetAxisAlignedBounding(&amax, &amin);
    b->GetAxisAlignedBounding(&bmax, &bmin);

    if(Vector::BoundingBoxesDisjoint(amax, amin, bmax, bmin)) {
        // They cannot possibly intersect, no curves to generate
        return;
    }

    Vector alongt, alongb;
    SBezier oft, ofb;
    bool isExtdt = this->IsExtrusion(&oft, &alongt),
         isExtdb =    b->IsExtrusion(&ofb, &alongb);

    if(degm == 1 && degn == 1 && b->degm == 1 && b->degn == 1) {
        // Line-line intersection; it's a plane or nothing.
        Vector na = NormalAt(0, 0).WithMagnitude(1),
               nb = b->NormalAt(0, 0).WithMagnitude(1);
        double da = na.Dot(PointAt(0, 0)),
               db = nb.Dot(b->PointAt(0, 0));

        Vector dl = na.Cross(nb);
        if(dl.Magnitude() < LENGTH_EPS) return; // parallel planes
        dl = dl.WithMagnitude(1);
        Vector p = Vector::AtIntersectionOfPlanes(na, da, nb, db);

        // Trim it to the region 0 <= {u,v} <= 1 for each plane; not strictly
        // necessary, since line will be split and excess edges culled, but
        // this improves speed and robustness.
        int i;
        double tmax = VERY_POSITIVE, tmin = VERY_NEGATIVE;
        for(i = 0; i < 2; i++) {
            SSurface *s = (i == 0) ? this : b;
            Vector tu, tv;
            s->TangentsAt(0, 0, &tu, &tv);

            double up, vp, ud, vd;
            s->ClosestPointTo(p, &up, &vp);
            ud = (dl.Dot(tu)) / tu.MagSquared();
            vd = (dl.Dot(tv)) / tv.MagSquared();

            // so u = up + t*ud
            //    v = vp + t*vd
            if(ud > LENGTH_EPS) {
                tmin = max(tmin, -up/ud);
                tmax = min(tmax, (1 - up)/ud);
            } else if(ud < -LENGTH_EPS) {
                tmax = min(tmax, -up/ud);
                tmin = max(tmin, (1 - up)/ud);
            } else {
                if(up < -LENGTH_EPS || up > 1 + LENGTH_EPS) {
                    // u is constant, and outside [0, 1]
                    tmax = VERY_NEGATIVE;
                }
            }
            if(vd > LENGTH_EPS) {
                tmin = max(tmin, -vp/vd);
                tmax = min(tmax, (1 - vp)/vd);
            } else if(vd < -LENGTH_EPS) {
                tmax = min(tmax, -vp/vd);
                tmin = max(tmin, (1 - vp)/vd);
            } else {
                if(vp < -LENGTH_EPS || vp > 1 + LENGTH_EPS) {
                    // v is constant, and outside [0, 1]
                    tmax = VERY_NEGATIVE;
                }
            }
        }

        if(tmax > tmin + LENGTH_EPS) {
            SBezier bezier = SBezier::From(p.Plus(dl.ScaledBy(tmin)),
                                           p.Plus(dl.ScaledBy(tmax)));
            AddExactIntersectionCurve(&bezier, b, agnstA, agnstB, into);
        }
    } else if((degm == 1 && degn == 1 && isExtdb) ||
              (b->degm == 1 && b->degn == 1 && isExtdt))
    {
        // The intersection between a plane and a surface of extrusion
        SSurface *splane, *sext;
        if(degm == 1 && degn == 1) {
            splane = this;
            sext = b;
        } else {
            splane = b;
            sext = this;
        }

        Vector n = splane->NormalAt(0, 0).WithMagnitude(1), along;
        double d = n.Dot(splane->PointAt(0, 0));
        SBezier bezier;
        (void)sext->IsExtrusion(&bezier, &along);

        if(fabs(n.Dot(along)) < LENGTH_EPS) {
            // Direction of extrusion is parallel to plane; so intersection
            // is zero or more lines. Build a line within the plane, and
            // normal to the direction of extrusion, and intersect that line
            // against the surface; each intersection point corresponds to
            // a line.
            Vector pm, alu, p0, dp;
            // a point halfway along the extrusion
            pm = ((sext->ctrl[0][0]).Plus(sext->ctrl[0][1])).ScaledBy(0.5);
            alu = along.WithMagnitude(1);
            dp = (n.Cross(along)).WithMagnitude(1);
            // n, alu, and dp form an orthogonal csys; set n component to
            // place it on the plane, alu component to lie halfway along
            // extrusion, and dp component doesn't matter so zero
            p0 = n.ScaledBy(d).Plus(alu.ScaledBy(pm.Dot(alu)));

            List<SInter> inters;
            ZERO(&inters);
            sext->AllPointsIntersecting(
                p0, p0.Plus(dp), &inters, false, false, true);
    
            SInter *si;
            for(si = inters.First(); si; si = inters.NextAfter(si)) {
                Vector al = along.ScaledBy(0.5);
                SBezier bezier;
                bezier = SBezier::From((si->p).Minus(al), (si->p).Plus(al));
                AddExactIntersectionCurve(&bezier, b, agnstA, agnstB, into);
            }

            inters.Clear();
        } else {
            // Direction of extrusion is not parallel to plane; so
            // intersection is projection of extruded curve into our plane.
            int i;
            for(i = 0; i <= bezier.deg; i++) {
                Vector p0 = bezier.ctrl[i],
                       p1 = p0.Plus(along);

                bezier.ctrl[i] =
                    Vector::AtIntersectionOfPlaneAndLine(n, d, p0, p1, NULL);
            }

            AddExactIntersectionCurve(&bezier, b, agnstA, agnstB, into);
        }
    } else if(isExtdt && isExtdb && 
                sqrt(fabs(alongt.Dot(alongb))) > 
                sqrt(alongt.Magnitude() * alongb.Magnitude()) - LENGTH_EPS)
    {
        // Two surfaces of extrusion along the same axis. So they might
        // intersect along some number of lines parallel to the axis.
        Vector axis = alongt.WithMagnitude(1);
        
        List<SInter> inters;
        ZERO(&inters);
        List<Vector> lv;
        ZERO(&lv);

        double a_axis0 = (   ctrl[0][0]).Dot(axis),
               a_axis1 = (   ctrl[0][1]).Dot(axis),
               b_axis0 = (b->ctrl[0][0]).Dot(axis),
               b_axis1 = (b->ctrl[0][1]).Dot(axis);

        if(a_axis0 > a_axis1) SWAP(double, a_axis0, a_axis1);
        if(b_axis0 > b_axis1) SWAP(double, b_axis0, b_axis1);

        double ab_axis0 = max(a_axis0, b_axis0),
               ab_axis1 = min(a_axis1, b_axis1);

        if(fabs(ab_axis0 - ab_axis1) < LENGTH_EPS) {
            // The line would be zero-length
            return;
        }

        Vector axis0 = axis.ScaledBy(ab_axis0),
               axis1 = axis.ScaledBy(ab_axis1),
               axisc = (axis0.Plus(axis1)).ScaledBy(0.5);

        oft.MakePwlInto(&lv);

        int i;
        for(i = 0; i < lv.n - 1; i++) {
            Vector pa = lv.elem[i], pb = lv.elem[i+1];
            pa = pa.Minus(axis.ScaledBy(pa.Dot(axis)));
            pb = pb.Minus(axis.ScaledBy(pb.Dot(axis)));
            pa = pa.Plus(axisc);
            pb = pb.Plus(axisc);

            b->AllPointsIntersecting(pa, pb, &inters, true, false, false);
        }

        SInter *si;
        for(si = inters.First(); si; si = inters.NextAfter(si)) {
            Vector p = (si->p).Minus(axis.ScaledBy((si->p).Dot(axis)));
            double ub, vb;
            b->ClosestPointTo(p, &ub, &vb, true);
            SSurface plane;
            plane = SSurface::FromPlane(p, axis.Normal(0), axis.Normal(1));

            b->PointOnSurfaces(this, &plane, &ub, &vb);

            p = b->PointAt(ub, vb);

            SBezier bezier;
            bezier = SBezier::From(p.Plus(axis0), p.Plus(axis1));
            AddExactIntersectionCurve(&bezier, b, agnstA, agnstB, into);
        }

        inters.Clear();
        lv.Clear();
    } else {
        // Try intersecting the surfaces numerically, by a marching algorithm.
        // First, we find all the intersections between a surface and the
        // boundary of the other surface.
        SPointList spl;
        ZERO(&spl);
        int a;
        for(a = 0; a < 2; a++) {
            SShell   *shA  = (a == 0) ? agnstA : agnstB,
                     *shB  = (a == 0) ? agnstB : agnstA;
            SSurface *srfA = (a == 0) ? this : b,
                     *srfB = (a == 0) ? b : this;

            SEdgeList el;
            ZERO(&el);
            srfA->MakeEdgesInto(shA, &el, false, NULL);

            SEdge *se;
            for(se = el.l.First(); se; se = el.l.NextAfter(se)) {
                List<SInter> lsi;
                ZERO(&lsi);

                srfB->AllPointsIntersecting(se->a, se->b, &lsi,
                    true, true, false);
                if(lsi.n == 0) continue;

                // Find the other surface that this curve trims.
                hSCurve hsc = { se->auxA };
                SCurve *sc = shA->curve.FindById(hsc);
                hSSurface hother = (sc->surfA.v == srfA->h.v) ?
                                                    sc->surfB : sc->surfA;
                SSurface *other = shA->surface.FindById(hother);

                SInter *si;
                for(si = lsi.First(); si; si = lsi.NextAfter(si)) {
                    Vector p = si->p;
                    double u, v;
                    srfB->ClosestPointTo(p, &u, &v);
                    srfB->PointOnSurfaces(srfA, other, &u, &v);
                    p = srfB->PointAt(u, v);
                    if(!spl.ContainsPoint(p)) {
                        SPoint sp;
                        sp.p = p;
                        // We also need the edge normal, so that we know in
                        // which direction to march.
                        srfA->ClosestPointTo(p, &u, &v);
                        Vector n = srfA->NormalAt(u, v);
                        sp.auxv = n.Cross((se->b).Minus(se->a));
                        sp.auxv = (sp.auxv).WithMagnitude(1);

                        spl.l.Add(&sp);
                    }
                }
                lsi.Clear();
            }

            el.Clear();
        }

        while(spl.l.n >= 2) {
            SCurve sc;
            ZERO(&sc);
            sc.surfA = h;
            sc.surfB = b->h;
            sc.isExact = false;
            sc.source = SCurve::FROM_INTERSECTION;

            Vector start  = spl.l.elem[0].p,
                   startv = spl.l.elem[0].auxv;
            spl.l.ClearTags();
            spl.l.elem[0].tag = 1;
            spl.l.RemoveTagged();
            SCurvePt padd;
            ZERO(&padd);
            padd.vertex = true;
            padd.p = start;
            sc.pts.Add(&padd);

            Point2d pa, pb;
            Vector np, npc;
            // Better to start with a too-small step, so that we don't miss
            // features of the curve entirely.
            bool fwd;
            double tol, step = SS.ChordTolMm();
            for(a = 0; a < 100; a++) {
                ClosestPointTo(start, &pa);
                b->ClosestPointTo(start, &pb);

                Vector na =    NormalAt(pa).WithMagnitude(1), 
                       nb = b->NormalAt(pb).WithMagnitude(1);

                if(a == 0) {
                    Vector dp = nb.Cross(na);
                    if(dp.Dot(startv) < 0) {
                        // We want to march in the more inward direction.
                        fwd = true;
                    } else {
                        fwd = false;
                    }
                }

                int i = 0;
                do {
                    if(i++ > 20) break;

                    Vector dp = nb.Cross(na);
                    if(!fwd) dp = dp.ScaledBy(-1);
                    dp = dp.WithMagnitude(step);

                    np = start.Plus(dp);
                    npc = ClosestPointOnThisAndSurface(b, np);
                    tol = (npc.Minus(np)).Magnitude();

                    if(tol > SS.ChordTolMm()*0.8) {
                        step *= 0.95;
                    } else {
                        step /= 0.95;
                    }
                } while(tol > SS.ChordTolMm() || tol < SS.ChordTolMm()/2);

                SPoint *sp;
                for(sp = spl.l.First(); sp; sp = spl.l.NextAfter(sp)) {
                    if((sp->p).OnLineSegment(start, npc, 2*SS.ChordTolMm())) {
                        sp->tag = 1;
                        a = 1000;
                        npc = sp->p;
                    }
                }

                padd.p = npc;
                padd.vertex = (a == 1000);
                sc.pts.Add(&padd);

                start = npc;
            }

            spl.l.RemoveTagged();

            // And now we split and insert the curve
            SCurve split = sc.MakeCopySplitAgainst(agnstA, agnstB, this, b);
            sc.Clear();
            into->curve.AddAndAssignId(&split);
            break;
        }
        spl.Clear();
    }
}


double SSurface::DepartureFromCoplanar(void) {
    int i, j;
    int ia, ja, ib, jb, ic, jc;
    double best;

    // Grab three points to define a plane; first choose (0, 0) arbitrarily.
    ia = ja = 0;
    // Then the point farthest from pt a.
    best = VERY_NEGATIVE;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            if(i == ia && j == ja) continue;

            double dist = (ctrl[i][j]).Minus(ctrl[ia][ja]).Magnitude();
            if(dist > best) {
                best = dist;
                ib = i;
                jb = j;
            }
        }
    }
    // Then biggest magnitude of ab cross ac.
    best = VERY_NEGATIVE;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            if(i == ia && j == ja) continue;
            if(i == ib && j == jb) continue;

            double mag =
                ((ctrl[ia][ja].Minus(ctrl[ib][jb]))).Cross(
                 (ctrl[ia][ja].Minus(ctrl[i ][j ]))).Magnitude();
            if(mag > best) {
                best = mag;
                ic = i;
                jc = j;
            }
        }
    }

    Vector n = ((ctrl[ia][ja].Minus(ctrl[ib][jb]))).Cross(
                (ctrl[ia][ja].Minus(ctrl[ic][jc])));
    n = n.WithMagnitude(1);
    double d = (ctrl[ia][ja]).Dot(n);

    // Finally, calculate the deviation from each point to the plane.
    double farthest = VERY_NEGATIVE;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            double dist = fabs(n.Dot(ctrl[i][j]) - d);
            if(dist > farthest) {
                farthest = dist;
            }
        }
    }
    return farthest;
}

void SSurface::WeightControlPoints(void) {
    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            ctrl[i][j] = (ctrl[i][j]).ScaledBy(weight[i][j]);
        }
    }
}
void SSurface::UnWeightControlPoints(void) {
    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            ctrl[i][j] = (ctrl[i][j]).ScaledBy(1.0/weight[i][j]);
        }
    }
}
void SSurface::CopyRowOrCol(bool row, int this_ij, SSurface *src, int src_ij) {
    if(row) {
        int j;
        for(j = 0; j <= degn; j++) {
            ctrl  [this_ij][j] = src->ctrl  [src_ij][j];
            weight[this_ij][j] = src->weight[src_ij][j];
        }
    } else {
        int i;
        for(i = 0; i <= degm; i++) {
            ctrl  [i][this_ij] = src->ctrl  [i][src_ij];
            weight[i][this_ij] = src->weight[i][src_ij];
        }
    }
}
void SSurface::BlendRowOrCol(bool row, int this_ij, SSurface *a, int a_ij,
                                                    SSurface *b, int b_ij)
{
    if(row) {
        int j;
        for(j = 0; j <= degn; j++) {
            Vector c = (a->ctrl  [a_ij][j]).Plus(b->ctrl  [b_ij][j]);
            double w = (a->weight[a_ij][j]   +   b->weight[b_ij][j]);
            ctrl  [this_ij][j] = c.ScaledBy(0.5);
            weight[this_ij][j] = w / 2;
        }
    } else {
        int i;
        for(i = 0; i <= degm; i++) {
            Vector c = (a->ctrl  [i][a_ij]).Plus(b->ctrl  [i][b_ij]);
            double w = (a->weight[i][a_ij]   +   b->weight[i][b_ij]);
            ctrl  [i][this_ij] = c.ScaledBy(0.5);
            weight[i][this_ij] = w / 2;
        }
    }
}
void SSurface::SplitInHalf(bool byU, SSurface *sa, SSurface *sb) {
    sa->degm = sb->degm = degm;
    sa->degn = sb->degn = degn;

    // by de Casteljau's algorithm in a projective space; so we must work
    // on points (w*x, w*y, w*z, w)
    WeightControlPoints();

    switch(byU ? degm : degn) {
        case 1:
            sa->CopyRowOrCol (byU, 0, this, 0);
            sb->CopyRowOrCol (byU, 1, this, 1);

            sa->BlendRowOrCol(byU, 1, this, 0, this, 1);
            sb->BlendRowOrCol(byU, 0, this, 0, this, 1);
            break;

        case 2:
            sa->CopyRowOrCol (byU, 0, this, 0);
            sb->CopyRowOrCol (byU, 2, this, 2);

            sa->BlendRowOrCol(byU, 1, this, 0, this, 1);
            sb->BlendRowOrCol(byU, 1, this, 1, this, 2);

            sa->BlendRowOrCol(byU, 2, sa,   1, sb,   1);
            sb->BlendRowOrCol(byU, 0, sa,   1, sb,   1);
            break;

        case 3: {
            SSurface st;
            st.degm = degm; st.degn = degn;

            sa->CopyRowOrCol (byU, 0, this, 0);
            sb->CopyRowOrCol (byU, 3, this, 3);

            sa->BlendRowOrCol(byU, 1, this, 0, this, 1);
            sb->BlendRowOrCol(byU, 2, this, 2, this, 3);
            st. BlendRowOrCol(byU, 0, this, 1, this, 2); // scratch var

            sa->BlendRowOrCol(byU, 2, sa,   1, &st,  0);
            sb->BlendRowOrCol(byU, 1, sb,   2, &st,  0);

            sa->BlendRowOrCol(byU, 3, sa,   2, sb,   1);
            sb->BlendRowOrCol(byU, 0, sa,   2, sb,   1);
            break;
        }

        default: oops();
    }

    sa->UnWeightControlPoints();
    sb->UnWeightControlPoints();
    UnWeightControlPoints();
}

//-----------------------------------------------------------------------------
// Find all points where the indicated finite (if segment) or infinite (if not
// segment) line intersects our surface. Report them in uv space in the list.
// We first do a bounding box check; if the line doesn't intersect, then we're
// done. If it does, then we check how small our surface is. If it's big,
// then we subdivide into quarters and recurse. If it's small, then we refine
// by Newton's method and record the point.
//-----------------------------------------------------------------------------
void SSurface::AllPointsIntersectingUntrimmed(Vector a, Vector b,
                                              int *cnt, int *level,
                                              List<Inter> *l, bool segment,
                                              SSurface *sorig)
{
    // Test if the line intersects our axis-aligned bounding box; if no, then
    // no possibility of an intersection
    if(LineEntirelyOutsideBbox(a, b, segment)) return;
 
    if(*cnt > 2000) {
        dbp("!!! too many subdivisions (level=%d)!", *level);
        dbp("degm = %d degn = %d", degm, degn);
        return;
    }
    (*cnt)++;

    // If we might intersect, and the surface is small, then switch to Newton
    // iterations.
    if(DepartureFromCoplanar() < 0.2*SS.ChordTolMm()) {
        Vector p = (ctrl[0   ][0   ]).Plus(
                    ctrl[0   ][degn]).Plus(
                    ctrl[degm][0   ]).Plus(
                    ctrl[degm][degn]).ScaledBy(0.25);
        Inter inter;
        sorig->ClosestPointTo(p, &(inter.p.x), &(inter.p.y), false);
        if(sorig->PointIntersectingLine(a, b, &(inter.p.x), &(inter.p.y))) {
            Vector p = sorig->PointAt(inter.p.x, inter.p.y);
            // Debug check, verify that the point lies in both surfaces
            // (which it ought to, since the surfaces should be coincident)
            double u, v;
            ClosestPointTo(p, &u, &v);
            l->Add(&inter);
        } else {
            // Might not converge if line is almost tangent to surface...
        }
        return;
    }

    // But the surface is big, so split it, alternating by u and v
    SSurface surf0, surf1;
    SplitInHalf((*level & 1) == 0, &surf0, &surf1);

    int nextLevel = (*level) + 1;
    (*level) = nextLevel;
    surf0.AllPointsIntersectingUntrimmed(a, b, cnt, level, l, segment, sorig);
    (*level) = nextLevel;
    surf1.AllPointsIntersectingUntrimmed(a, b, cnt, level, l, segment, sorig);
}

//-----------------------------------------------------------------------------
// Find all points where a line through a and b intersects our surface, and
// add them to the list. If seg is true then report only intersections that
// lie within the finite line segment (not including the endpoints); otherwise
// we work along the infinite line. And we report either just intersections
// inside the trim curve, or any intersection with u, v in [0, 1]. And we
// either disregard or report tangent points.
//-----------------------------------------------------------------------------
void SSurface::AllPointsIntersecting(Vector a, Vector b,
                                     List<SInter> *l,
                                     bool seg, bool trimmed, bool inclTangent)
{
    if(LineEntirelyOutsideBbox(a, b, seg)) return;

    Vector ba = b.Minus(a);
    double bam = ba.Magnitude();

    List<Inter> inters;
    ZERO(&inters);

    // All the intersections between the line and the surface; either special
    // cases that we can quickly solve in closed form, or general numerical.
    Vector center, axis, start, finish;
    double radius;
    if(degm == 1 && degn == 1) {
        // Against a plane, easy.
        Vector n = NormalAt(0, 0).WithMagnitude(1);
        double d = n.Dot(PointAt(0, 0));
        // Trim to line segment now if requested, don't generate points that
        // would just get discarded later.
        if(!seg ||
           (n.Dot(a) > d + LENGTH_EPS && n.Dot(b) < d - LENGTH_EPS) ||
           (n.Dot(b) > d + LENGTH_EPS && n.Dot(a) < d - LENGTH_EPS))
        {
            Vector p = Vector::AtIntersectionOfPlaneAndLine(n, d, a, b, NULL);
            Inter inter;
            ClosestPointTo(p, &(inter.p.x), &(inter.p.y));
            inters.Add(&inter);
        }
    } else if(IsCylinder(&axis, &center, &radius, &start, &finish)) {
        // This one can be solved in closed form too.
        Vector ab = b.Minus(a);
        if(axis.Cross(ab).Magnitude() < LENGTH_EPS) {
            // edge is parallel to axis of cylinder, no intersection points
            return;
        }
        // A coordinate system centered at the center of the circle, with
        // the edge under test horizontal
        Vector u, v, n = axis.WithMagnitude(1);
        u = (ab.Minus(n.ScaledBy(ab.Dot(n)))).WithMagnitude(1);
        v = n.Cross(u);
        Point2d ap = (a.Minus(center)).DotInToCsys(u, v, n).ProjectXy(),
                bp = (b.Minus(center)).DotInToCsys(u, v, n).ProjectXy(),
                sp = (start. Minus(center)).DotInToCsys(u, v, n).ProjectXy(),
                fp = (finish.Minus(center)).DotInToCsys(u, v, n).ProjectXy();

        double thetas = atan2(sp.y, sp.x), thetaf = atan2(fp.y, fp.x);

        Point2d ip[2];
        int ip_n = 0;
        if(fabs(fabs(ap.y) - radius) < LENGTH_EPS) {
            // tangent
            if(inclTangent) {
                ip[0] = Point2d::From(0, ap.y);
                ip_n = 1;
            }
        } else if(fabs(ap.y) < radius) {
            // two intersections
            double xint = sqrt(radius*radius - ap.y*ap.y);
            ip[0] = Point2d::From(-xint, ap.y);
            ip[1] = Point2d::From( xint, ap.y);
            ip_n = 2;
        }
        int i;
        for(i = 0; i < ip_n; i++) {
            double t = (ip[i].Minus(ap)).DivPivoting(bp.Minus(ap));
            // This is a point on the circle; but is it on the arc?
            Point2d pp = ap.Plus((bp.Minus(ap)).ScaledBy(t));
            double theta = atan2(pp.y, pp.x);
            double dp = WRAP_SYMMETRIC(theta  - thetas, 2*PI),
                   df = WRAP_SYMMETRIC(thetaf - thetas, 2*PI);
            double tol = LENGTH_EPS/radius;

            if((df > 0 && ((dp < -tol) || (dp > df + tol))) ||
               (df < 0 && ((dp >  tol) || (dp < df - tol))))
            {
                continue;
            }

            Vector p = a.Plus((b.Minus(a)).ScaledBy(t));

            Inter inter;
            ClosestPointTo(p, &(inter.p.x), &(inter.p.y));
            inters.Add(&inter);
        }
    } else {
        // General numerical solution by subdivision, fallback
        int cnt = 0, level = 0;
        AllPointsIntersectingUntrimmed(a, b, &cnt, &level, &inters, seg, this);
    }

    // Remove duplicate intersection points
    inters.ClearTags();
    int i, j;
    for(i = 0; i < inters.n; i++) {
        for(j = i + 1; j < inters.n; j++) {
            if(inters.elem[i].p.Equals(inters.elem[j].p)) {
                inters.elem[j].tag = 1;
            }
        }
    }
    inters.RemoveTagged();

    for(i = 0; i < inters.n; i++) {
        Point2d puv = inters.elem[i].p;

        // Make sure the point lies within the finite line segment
        Vector pxyz = PointAt(puv.x, puv.y);
        double t = (pxyz.Minus(a)).DivPivoting(ba);
        if(seg && (t > 1 - LENGTH_EPS/bam || t < LENGTH_EPS/bam)) {
            continue;
        }
       
        // And that it lies inside our trim region
        Point2d dummy = { 0, 0 }, ia = { 0, 0 }, ib = { 0, 0 };
        int c = bsp->ClassifyPoint(puv, dummy, &ia, &ib);
        if(trimmed && c == SBspUv::OUTSIDE) {
            continue;
        }

        // It does, so generate the intersection
        SInter si;
        si.p = pxyz;
        si.surfNormal = NormalAt(puv.x, puv.y);
        si.pinter = puv;
        si.srf = this;
        si.onEdge = (c != SBspUv::INSIDE);
        si.edgeA = ia;
        si.edgeB = ib;
        l->Add(&si);
    }

    inters.Clear();
}

void SShell::AllPointsIntersecting(Vector a, Vector b,
                                   List<SInter> *il,
                                   bool seg, bool trimmed, bool inclTangent)
{
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        ss->AllPointsIntersecting(a, b, il, seg, trimmed, inclTangent);
    }
}

int SShell::ClassifyRegion(Vector edge_n, Vector inter_surf_n,
                           Vector edge_surf_n)
{
    double dot = inter_surf_n.Dot(edge_n);
    if(fabs(dot) < DOTP_TOL) {
        // The edge's surface and the edge-on-face surface
        // are coincident. Test the edge's surface normal
        // to see if it's with same or opposite normals.
        if(inter_surf_n.Dot(edge_surf_n) > 0) {
            return COINC_SAME;
        } else {
            return COINC_OPP;
        }
    } else if(dot > 0) {
        return OUTSIDE;
    } else {
        return INSIDE;
    }
}

//-----------------------------------------------------------------------------
// Does the given point lie on our shell? There are many cases; inside and
// outside are obvious, but then there's all the edge-on-edge and edge-on-face
// possibilities.
//
// To calculate, we intersect a ray through p with our shell, and classify
// using the closest intersection point. If the ray hits a surface on edge,
// then just reattempt in a different random direction.
//-----------------------------------------------------------------------------
bool SShell::ClassifyEdge(int *indir, int *outdir,
                          Vector ea, Vector eb,
                          Vector p, 
                          Vector edge_n_in, Vector edge_n_out, Vector surf_n)
{
    List<SInter> l;
    ZERO(&l);

    srand(0);

    // First, check for edge-on-edge
    int edge_inters = 0;
    Vector inter_surf_n[2], inter_edge_n[2];
    SSurface *srf;
    for(srf = surface.First(); srf; srf = surface.NextAfter(srf)) {
        if(srf->LineEntirelyOutsideBbox(ea, eb, true)) continue;

        SEdgeList *sel = &(srf->edges);
        SEdge *se;
        for(se = sel->l.First(); se; se = sel->l.NextAfter(se)) {
            if((ea.Equals(se->a) && eb.Equals(se->b)) ||
               (eb.Equals(se->a) && ea.Equals(se->b)) ||
                p.OnLineSegment(se->a, se->b))
            {
                if(edge_inters < 2) {
                    // Edge-on-edge case
                    Point2d pm;
                    srf->ClosestPointTo(p,  &pm, false);
                    // A vector normal to the surface, at the intersection point
                    inter_surf_n[edge_inters] = srf->NormalAt(pm);
                    // A vector normal to the intersecting edge (but within the
                    // intersecting surface) at the intersection point, pointing
                    // out.
                    inter_edge_n[edge_inters] =
                      (inter_surf_n[edge_inters]).Cross((se->b).Minus((se->a)));
                }

                edge_inters++;
            }
        }
    }

    if(edge_inters == 2) {
        // TODO, make this use the appropriate curved normals
        double dotp[2];
        for(int i = 0; i < 2; i++) {
            dotp[i] = edge_n_out.Dot(inter_surf_n[i]);
        }

        if(fabs(dotp[1]) < DOTP_TOL) {
            SWAP(double, dotp[0],         dotp[1]);
            SWAP(Vector, inter_surf_n[0], inter_surf_n[1]);
            SWAP(Vector, inter_edge_n[0], inter_edge_n[1]);
        }

        int coinc = (surf_n.Dot(inter_surf_n[0])) > 0 ? COINC_SAME : COINC_OPP;

        if(fabs(dotp[0]) < DOTP_TOL && fabs(dotp[1]) < DOTP_TOL) {
            // This is actually an edge on face case, just that the face
            // is split into two pieces joining at our edge.
            *indir  = coinc;
            *outdir = coinc;
        } else if(fabs(dotp[0]) < DOTP_TOL && dotp[1] > DOTP_TOL) {
            if(edge_n_out.Dot(inter_edge_n[0]) > 0) {
                *indir  = coinc;
                *outdir = OUTSIDE;
            } else {
                *indir  = INSIDE;
                *outdir = coinc;
            }
        } else if(fabs(dotp[0]) < DOTP_TOL && dotp[1] < -DOTP_TOL) {
            if(edge_n_out.Dot(inter_edge_n[0]) > 0) {
                *indir  = coinc;
                *outdir = INSIDE;
            } else {
                *indir  = OUTSIDE;
                *outdir = coinc;
            }
        } else if(dotp[0] > DOTP_TOL && dotp[1] > DOTP_TOL) {
            *indir  = INSIDE;
            *outdir = OUTSIDE;
        } else if(dotp[0] < -DOTP_TOL && dotp[1] < -DOTP_TOL) {
            *indir  = OUTSIDE;
            *outdir = INSIDE;
        } else {
            // Edge is tangent to the shell at shell's edge, so can't be
            // a boundary of the surface.
            return false;
        }
        return true;
    }

    if(edge_inters != 0) dbp("bad, edge_inters=%d", edge_inters);

    // Next, check for edge-on-surface. The ray-casting for edge-inside-shell
    // would catch this too, but test separately, for speed (since many edges
    // are on surface) and for numerical stability, so we don't pick up
    // the additional error from the line intersection.

    for(srf = surface.First(); srf; srf = surface.NextAfter(srf)) {
        if(srf->LineEntirelyOutsideBbox(ea, eb, true)) continue;

        Point2d puv;
        srf->ClosestPointTo(p, &(puv.x), &(puv.y), false);
        Vector pp = srf->PointAt(puv);

        if((pp.Minus(p)).Magnitude() > LENGTH_EPS) continue;
        Point2d dummy = { 0, 0 }, ia = { 0, 0 }, ib = { 0, 0 };
        int c = srf->bsp->ClassifyPoint(puv, dummy, &ia, &ib);
        if(c == SBspUv::OUTSIDE) continue;

        // Edge-on-face (unless edge-on-edge above superceded)
        Point2d pin, pout;
        srf->ClosestPointTo(p.Plus(edge_n_in),  &pin,  false);
        srf->ClosestPointTo(p.Plus(edge_n_out), &pout, false);

        Vector surf_n_in  = srf->NormalAt(pin),
               surf_n_out = srf->NormalAt(pout);

        *indir  = ClassifyRegion(edge_n_in,  surf_n_in,  surf_n);
        *outdir = ClassifyRegion(edge_n_out, surf_n_out, surf_n);
        return true;
    }

    // Edge is not on face or on edge; so it's either inside or outside
    // the shell, and we'll determine which by raycasting.
    int cnt = 0;
    for(;;) {
        // Cast a ray in a random direction (two-sided so that we test if
        // the point lies on a surface, but use only one side for in/out
        // testing)
        Vector ray = Vector::From(Random(1), Random(1), Random(1));

        AllPointsIntersecting(
            p.Minus(ray), p.Plus(ray), &l, false, true, false);
        
        // no intersections means it's outside
        *indir  = OUTSIDE;
        *outdir = OUTSIDE;
        double dmin = VERY_POSITIVE;
        bool onEdge = false;
        edge_inters = 0;

        SInter *si;
        for(si = l.First(); si; si = l.NextAfter(si)) {
            double t = ((si->p).Minus(p)).DivPivoting(ray);
            if(t*ray.Magnitude() < -LENGTH_EPS) {
                // wrong side, doesn't count
                continue;
            }

            double d = ((si->p).Minus(p)).Magnitude();

            // We actually should never hit this case; it should have been
            // handled above.
            if(d < LENGTH_EPS && si->onEdge) {
                edge_inters++;
            }

            if(d < dmin) {
                dmin = d;
                // Edge does not lie on surface; either strictly inside
                // or strictly outside
                if((si->surfNormal).Dot(ray) > 0) {
                    *indir  = INSIDE;
                    *outdir = INSIDE;
                } else {
                    *indir  = OUTSIDE;
                    *outdir = OUTSIDE;
                }
                onEdge = si->onEdge;
            }
        }
        l.Clear();

        // If the point being tested lies exactly on an edge of the shell,
        // then our ray always lies on edge, and that's okay. Otherwise
        // try again in a different random direction.
        if(!onEdge) break;
        if(cnt++ > 5) {
            dbp("can't find a ray that doesn't hit on edge!");
            dbp("on edge = %d, edge_inters = %d", onEdge, edge_inters);
            SS.nakedEdges.AddEdge(ea, eb);
            break;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
// Are two surfaces coincident, with the same (or with opposite) normals?
// Currently handles planes only.
//-----------------------------------------------------------------------------
bool SSurface::CoincidentWith(SSurface *ss, bool sameNormal) {
    if(degm != 1 || degn != 1) return false;
    if(ss->degm != 1 || ss->degn != 1) return false;

    Vector p = ctrl[0][0];
    Vector n = NormalAt(0, 0).WithMagnitude(1);
    double d = n.Dot(p);

    if(!ss->CoincidentWithPlane(n, d)) return false;

    Vector n2 = ss->NormalAt(0, 0);
    if(sameNormal) {
        if(n2.Dot(n) < 0) return false;
    } else {
        if(n2.Dot(n) > 0) return false;
    }

    return true;
}

bool SSurface::CoincidentWithPlane(Vector n, double d) {
    if(degm != 1 || degn != 1) return false;
    if(fabs(n.Dot(ctrl[0][0]) - d) > LENGTH_EPS) return false;
    if(fabs(n.Dot(ctrl[0][1]) - d) > LENGTH_EPS) return false;
    if(fabs(n.Dot(ctrl[1][0]) - d) > LENGTH_EPS) return false;
    if(fabs(n.Dot(ctrl[1][1]) - d) > LENGTH_EPS) return false;
    
    return true;
}

//-----------------------------------------------------------------------------
// In our shell, find all surfaces that are coincident with the prototype
// surface (with same or opposite normal, as specified), and copy all of
// their trim polygons into el. The edges are returned in uv coordinates for
// the prototype surface.
//-----------------------------------------------------------------------------
void SShell::MakeCoincidentEdgesInto(SSurface *proto, bool sameNormal,
                                     SEdgeList *el, SShell *useCurvesFrom)
{
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        if(proto->CoincidentWith(ss, sameNormal)) {
            ss->MakeEdgesInto(this, el, false, useCurvesFrom);
        }
    }

    SEdge *se;
    for(se = el->l.First(); se; se = el->l.NextAfter(se)) {
        double ua, va, ub, vb;
        proto->ClosestPointTo(se->a, &ua, &va);
        proto->ClosestPointTo(se->b, &ub, &vb);

        if(sameNormal) {
            se->a = Vector::From(ua, va, 0);
            se->b = Vector::From(ub, vb, 0);
        } else {
            // Flip normal, so flip all edge directions
            se->b = Vector::From(ua, va, 0);
            se->a = Vector::From(ub, vb, 0);
        }
    }
}

