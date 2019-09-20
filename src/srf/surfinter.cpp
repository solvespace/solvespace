//-----------------------------------------------------------------------------
// How to intersect two surfaces, to get some type of curve. This is either
// an exact special case (e.g., two planes to make a line), or a numerical
// thing.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

extern int FLAG;

void SSurface::AddExactIntersectionCurve(SBezier *sb, SSurface *srfB,
                                         SShell *agnstA, SShell *agnstB, SShell *into)
{
    SCurve sc = {};
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
        sc = {};
    } else {
        sb->MakePwlInto(&(sc.pts));
        // and split the line where it intersects our existing surfaces
        split = sc.MakeCopySplitAgainst(agnstA, agnstB, this, srfB);
        sc.Clear();
    }

    // Test if the curve lies entirely outside one of the
    SCurvePt *scpt;
    bool withinA = false, withinB = false;
    for(scpt = split.pts.First(); scpt; scpt = split.pts.NextAfter(scpt)) {
        double tol = 0.01;
        Point2d puv;
        ClosestPointTo(scpt->p, &puv);
        if(puv.x > -tol && puv.x < 1 + tol &&
           puv.y > -tol && puv.y < 1 + tol)
        {
            withinA = true;
        }
        srfB->ClosestPointTo(scpt->p, &puv);
        if(puv.x > -tol && puv.x < 1 + tol &&
           puv.y > -tol && puv.y < 1 + tol)
        {
            withinB = true;
        }
        // Break out early, no sense wasting time if we already have the answer.
        if(withinA && withinB) break;
    }
    if(!(withinA && withinB)) {
        // Intersection curve lies entirely outside one of the surfaces, so
        // it's fake.
        split.Clear();
        return;
    }

#if 0
    if(sb->deg == 2) {
        dbp(" ");
        SCurvePt *prev = NULL, *v;
        dbp("split.pts.n = %d", split.pts.n);
        for(v = split.pts.First(); v; v = split.pts.NextAfter(v)) {
            if(prev) {
                Vector e = (prev->p).Minus(v->p).WithMagnitude(0);
                SS.nakedEdges.AddEdge((prev->p).Plus(e), (v->p).Minus(e));
            }
            prev = v;
        }
    }
#endif // 0
    ssassert(!(sb->Start()).Equals(sb->Finish()),
             "Unexpected zero-length edge");

    split.source = SCurve::Source::INTERSECTION;
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

            List<SInter> inters = {};
            sext->AllPointsIntersecting(p0, p0.Plus(dp), &inters,
                /*asSegment=*/false, /*trimmed=*/false, /*inclTangent=*/true);

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

        List<SInter> inters = {};
        List<Vector> lv = {};

        double a_axis0 = (   ctrl[0][0]).Dot(axis),
               a_axis1 = (   ctrl[0][1]).Dot(axis),
               b_axis0 = (b->ctrl[0][0]).Dot(axis),
               b_axis1 = (b->ctrl[0][1]).Dot(axis);

        if(a_axis0 > a_axis1) swap(a_axis0, a_axis1);
        if(b_axis0 > b_axis1) swap(b_axis0, b_axis1);

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
            Vector pa = lv[i], pb = lv[i+1];
            pa = pa.Minus(axis.ScaledBy(pa.Dot(axis)));
            pb = pb.Minus(axis.ScaledBy(pb.Dot(axis)));
            pa = pa.Plus(axisc);
            pb = pb.Plus(axisc);

            b->AllPointsIntersecting(pa, pb, &inters,
                /*asSegment=*/true,/*trimmed=*/false, /*inclTangent=*/false);
        }

        SInter *si;
        for(si = inters.First(); si; si = inters.NextAfter(si)) {
            Vector p = (si->p).Minus(axis.ScaledBy((si->p).Dot(axis)));
            double ub, vb;
            b->ClosestPointTo(p, &ub, &vb, /*mustConverge=*/true);
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
        SPointList spl = {};
        int a;
        for(a = 0; a < 2; a++) {
            SShell   *shA  = (a == 0) ? agnstA : agnstB;
            SSurface *srfA = (a == 0) ? this : b,
                     *srfB = (a == 0) ? b : this;

            SEdgeList el = {};
            srfA->MakeEdgesInto(shA, &el, MakeAs::XYZ, NULL);

            SEdge *se;
            for(se = el.l.First(); se; se = el.l.NextAfter(se)) {
                List<SInter> lsi = {};

                srfB->AllPointsIntersecting(se->a, se->b, &lsi,
                    /*asSegment=*/true, /*trimmed=*/true, /*inclTangent=*/false);
                if(lsi.IsEmpty())
                    continue;

                // Find the other surface that this curve trims.
                hSCurve hsc = { (uint32_t)se->auxA };
                SCurve *sc = shA->curve.FindById(hsc);
                hSSurface hother = (sc->surfA == srfA->h) ?
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
            SCurve sc = {};
            sc.surfA = h;
            sc.surfB = b->h;
            sc.isExact = false;
            sc.source = SCurve::Source::INTERSECTION;

            Vector start  = spl.l[0].p,
                   startv = spl.l[0].auxv;
            spl.l.ClearTags();
            spl.l[0].tag = 1;
            spl.l.RemoveTagged();

            // Our chord tolerance is whatever the user specified
            double maxtol = SS.ChordTolMm();
            int maxsteps = max(300, SS.GetMaxSegments()*3);

            // The curve starts at our starting point.
            SCurvePt padd = {};
            padd.vertex = true;
            padd.p = start;
            sc.pts.Add(&padd);

            Point2d pa, pb;
            Vector np, npc = Vector::From(0, 0, 0);
            bool fwd = false;
            // Better to start with a too-small step, so that we don't miss
            // features of the curve entirely.
            double tol, step = maxtol;
            for(a = 0; a < maxsteps; a++) {
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

                int i;
                for(i = 0; i < 20; i++) {
                    Vector dp = nb.Cross(na);
                    if(!fwd) dp = dp.ScaledBy(-1);
                    dp = dp.WithMagnitude(step);

                    np = start.Plus(dp);
                    npc = ClosestPointOnThisAndSurface(b, np);
                    tol = (npc.Minus(np)).Magnitude();

                    if(tol > maxtol*0.8) {
                        step *= 0.90;
                    } else {
                        step /= 0.90;
                    }

                    if((tol < maxtol) && (tol > maxtol/2)) {
                        // If we meet the chord tolerance test, and we're
                        // not too fine, then we break out.
                        break;
                    }
                }

                SPoint *sp;
                for(sp = spl.l.First(); sp; sp = spl.l.NextAfter(sp)) {
                    if((sp->p).OnLineSegment(start, npc, 2*SS.ChordTolMm())) {
                        sp->tag = 1;
                        a = maxsteps;
                        npc = sp->p;
                    }
                }

                padd.p = npc;
                padd.vertex = (a == maxsteps);
                sc.pts.Add(&padd);

                start = npc;
            }

            spl.l.RemoveTagged();

            // And now we split and insert the curve
            SCurve split = sc.MakeCopySplitAgainst(agnstA, agnstB, this, b);
            sc.Clear();
            into->curve.AddAndAssignId(&split);
        }
        spl.Clear();
    }
}

//-----------------------------------------------------------------------------
// Are two surfaces coincident, with the same (or with opposite) normals?
// Currently handles planes only.
//-----------------------------------------------------------------------------
bool SSurface::CoincidentWith(SSurface *ss, bool sameNormal) const {
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

bool SSurface::CoincidentWithPlane(Vector n, double d) const {
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
            ss->MakeEdgesInto(this, el, SSurface::MakeAs::XYZ, useCurvesFrom);
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

