//-----------------------------------------------------------------------------
// How to intersect two surfaces, to get some type of curve. This is either
// an exact special case (e.g., two planes to make a line), or a numerical
// thing.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

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
    SCurve split, *existing = NULL;
    SBezier sbrev = *sb;
    sbrev.Reverse();
    bool backwards = false;
#pragma omp critical(into)
    {
        for(SCurve &se : into->curve) {
            if(se.isExact) {
                if(sb->Equals(&(se.exact))) {
                    existing = &se;
                    break;
                }
                if(sbrev.Equals(&(se.exact))) {
                    existing = &se;
                    backwards = true;
                    break;
                }
            }
        }
        if(existing) {
            // Copy the pwl points while still in the critical section;
            // existing points into into->curve, which reallocates when
            // another thread adds a curve.
            SCurvePt *v;
            for(v = existing->pts.First(); v; v = existing->pts.NextAfter(v)) {
                sc.pts.Add(v);
            }
        }
    }// end omp critical
    if(existing) {
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
#pragma omp critical(into)
    {
        into->curve.AddAndAssignId(&split);
    }
}

//-----------------------------------------------------------------------------
// Does a vertex of one of our trim edges lie on srf, within its trim region,
// with the trim curve crossing srf there (not tangent to it)? The curve's
// direction comes from its exact form if it has one, since a pwl chord's
// direction can differ from the curve's by a few degrees; dir, the direction
// of the pwl edge, is used otherwise.
//-----------------------------------------------------------------------------
static bool EdgeVertexOnSurface(SSurface *srf, Vector p, SCurve *sc, Vector dir) {
    Point2d puv;
    srf->ClosestPointTo(p, &puv, /*mustConverge=*/false);
    if(!(srf->PointAt(puv)).Equals(p)) return false;

    if(!srf->bsp) return false;
    Point2d dummy = { 0, 0 };
    if(srf->bsp->ClassifyPoint(puv, dummy, srf) == SBspUv::Class::OUTSIDE) {
        return false;
    }

    if(sc->isExact) {
        double t;
        sc->exact.ClosestPointTo(p, &t, /*mustConverge=*/false);
        dir = sc->exact.TangentAt(t);
    }
    Vector n = srf->NormalAt(puv);
    if(fabs((n.WithMagnitude(1)).Dot(dir.WithMagnitude(1))) < 1e-3) {
        // The trim curve is tangent to srf here. The direction in which we
        // would march from this point comes from this edge, and means
        // nothing for a tangent edge; if the surfaces do cross here, an edge
        // that crosses srf will find the point.
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
// Where two surfaces are tangent, their normals are parallel, so the direction
// of the curve where they intersect (the cross product of the normals) isn't
// defined there; but their curvatures tell which way, if any, curves leave.
// Writing each surface as a height above the common tangent plane, to second
// order h = x^T*H*x/2 for its second fundamental form H, the surfaces meet
// where x^T*(Ha - Hb)*x = 0. If Ha - Hb is indefinite, that's two lines
// through the point, so up to four directions to march in; if it's definite,
// the surfaces only touch there, and no curve leaves at all. Otherwise this
// order doesn't settle it.
//
// Returns -1 if the surfaces aren't tangent at p or the curvatures don't
// settle it, and otherwise the number of directions, put in rays, in which a
// curve leaves p into the trim regions of both surfaces. The other points in
// spl, where curves may end, say how close to p we must look.
//-----------------------------------------------------------------------------
static int TangentRays(SSurface *srfA, SSurface *srfB, Vector p, Vector *rays,
                       const SPointList *spl) {
    Point2d pa, pb;
    srfA->ClosestPointTo(p, &pa, /*mustConverge=*/false);
    srfB->ClosestPointTo(p, &pb, /*mustConverge=*/false);
    Vector na = srfA->NormalAt(pa).WithMagnitude(1),
           nb = srfB->NormalAt(pb).WithMagnitude(1);
    if((na.Cross(nb)).Magnitude() >= LENGTH_EPS) return -1;

    // Both forms in one basis of the common tangent plane, as heights along na.
    Vector e1 = na.Normal(0).WithMagnitude(1),
           e2 = na.Cross(e1);
    double a11, a12, a22, b11, b12, b22;
    srfA->CurvatureFormAt(pa.x, pa.y, e1, e2, &a11, &a12, &a22);
    srfB->CurvatureFormAt(pb.x, pb.y, e1, e2, &b11, &b12, &b22);
    if(na.Dot(nb) < 0) {
        b11 = -b11;
        b12 = -b12;
        b22 = -b22;
    }
    double d11 = a11 - b11, d12 = a12 - b12, d22 = a22 - b22;

    // The eigenvalues of [d11 d12; d12 d22], l1 >= l2.
    double mean = (d11 + d22)/2,
           rad  = sqrt((d11 - d22)*(d11 - d22)/4 + d12*d12);
    double l1 = mean + rad, l2 = mean - rad;
    double big = max(fabs(l1), fabs(l2)),
           small = min(fabs(l1), fabs(l2));
    // Zero, or nearly singular: this order doesn't settle it.
    if(big < LENGTH_EPS || small < LENGTH_EPS*big) return -1;
    // Definite: the surfaces only touch here.
    if(l2 > 0 || l1 < 0) return 0;

    // Indefinite, l1 > 0 > l2. The eigenvector for l1 is (d12, l1 - d11) or
    // (l1 - d22, d12); take the longer, for accuracy. The other eigenvector
    // is perpendicular to it.
    double v1x = d12, v1y = l1 - d11;
    if(v1x*v1x + v1y*v1y < (l1 - d22)*(l1 - d22) + d12*d12) {
        v1x = l1 - d22;
        v1y = d12;
    }
    Vector v1 = (e1.ScaledBy(v1x).Plus(e2.ScaledBy(v1y))).WithMagnitude(1),
           v2 = na.Cross(v1);

    // In that basis the form is l1*x*x + l2*y*y, zero where
    // x : y = sqrt(-l2) : +/-sqrt(l1).
    Vector lines[2] = {
        v1.ScaledBy(sqrt(-l2)).Plus (v2.ScaledBy(sqrt(l1))).WithMagnitude(1),
        v1.ScaledBy(sqrt(-l2)).Minus(v2.ScaledBy(sqrt(l1))).WithMagnitude(1),
    };

    // Keep the directions that go into both trimmed surfaces; test a little
    // way along each. Near a point of tangency the surfaces stay within
    // LENGTH_EPS of each other for a while, so the trims there can be much
    // closer together than the chord tolerance; e.g. a curve can leave p and
    // end on a trim 0.01 mm away. So look closer than the nearest place a
    // curve may end, as well as within the chord tolerance.
    int n = 0;
    double dist = SS.ChordTolMm();
    for(const SPoint &sp : spl->l) {
        double d = (sp.p.Minus(p)).Magnitude();
        if(d > LENGTH_EPS) dist = min(dist, d/4);
    }
    for(int i = 0; i < 4; i++) {
        Vector ray = (i < 2) ? lines[i] : lines[i - 2].ScaledBy(-1);
        Vector q = p.Plus(ray.ScaledBy(dist));
        bool inside = true;
        for(int j = 0; j < 2; j++) {
            SSurface *srf = (j == 0) ? srfA : srfB;
            if(!srf->bsp) {
                inside = false;
                break;
            }
            Point2d puv;
            srf->ClosestPointTo(q, &puv, /*mustConverge=*/false);
            Point2d dummy = { 0, 0 };
            if(srf->bsp->ClassifyPoint(puv, dummy, srf) != SBspUv::Class::INSIDE) {
                inside = false;
                break;
            }
        }
        if(inside) rays[n++] = ray;
    }
    return n;
}

// A point where a march along the intersection of two surfaces may start or
// end. Where the surfaces are tangent, there's one of these for each direction
// in which a curve leaves the point, since more than one curve can end there.
class MarchPoint {
public:
    int     tag;
    Vector  p;
    // The direction a curve leaves in, for a point of tangency; otherwise the
    // inward normal to the trim edge on which we found the point.
    Vector  dir;
    bool    tangent;
};

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
        SShell *shext;
        if(degm == 1 && degn == 1) {
            splane = this;
            sext = b;
            shext = agnstB;
        } else {
            splane = b;
            sext = this;
            shext = agnstA;
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

            // The extruded curve might be tangent to the plane, e.g. where
            // a fillet-like surface blends into the flat face it's tangent
            // to (issue #1291). The numerical line-surface intersection
            // below can't converge on such a grazing intersection; but the
            // tangency occurs where the extruded curve ends on the plane,
            // so the line of intersection is the trim curve that the
            // surface shares with its neighbour there, and that curve lies
            // entirely in the plane. Add any such exact curves directly,
            // like the exact-curve case in the general branch below.
            List<SBezier> exacts = {};
            for(SCurve &sc : shext->curve) {
                if(sc.source == SCurve::Source::INTERSECTION) continue;
                if(!sc.isExact) continue;
                if((sc.surfA != sext->h) && (sc.surfB != sext->h)) continue;
                if(splane->ContainsPlaneCurve(&sc)) {
                    SBezier bezier = sc.exact;
                    AddExactIntersectionCurve(&bezier, b, agnstA, agnstB, into);
                    exacts.Add(&bezier);
                }
            }

            List<SInter> inters = {};
            sext->AllPointsIntersecting(p0, p0.Plus(dp), &inters,
                /*asSegment=*/false, /*trimmed=*/false, /*inclTangent=*/true);

            SInter *si;
            for(si = inters.First(); si; si = inters.NextAfter(si)) {
                // If this line was already added exactly above, don't add a
                // numerical (last-digits different) duplicate of it; the
                // duplicates would break the trim polygon assembly.
                bool duplicate = false;
                SBezier *eb;
                for(eb = exacts.First(); eb; eb = exacts.NextAfter(eb)) {
                    double t;
                    eb->ClosestPointTo(si->p, &t, /*mustConverge=*/false);
                    if(((eb->PointAt(t)).Minus(si->p)).Magnitude() < LENGTH_EPS) {
                        duplicate = true;
                        break;
                    }
                }
                if(duplicate) continue;

                Vector al = along.ScaledBy(0.5);
                SBezier bezier;
                bezier = SBezier::From((si->p).Minus(al), (si->p).Plus(al));
                AddExactIntersectionCurve(&bezier, b, agnstA, agnstB, into);
            }

            inters.Clear();
            exacts.Clear();
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
        if((degm == 1 && degn == 1) || (b->degm == 1 && b->degn == 1)) {
            // we should only be here if just one surface is a plane because the
            // plane-plane case was already handled above. Need to check the other
            // nonplanar surface for trim curves that lie in the plane and are not
            // already trimming both surfaces. This happens when we cut a Lathe shell
            // on one of the seams for example.
            // This also seems necessary to merge some coincident surfaces.
            SSurface *splane, *sext;
            SShell *shext;
            if(degm == 1 && degn == 1) { // this and other checks assume coplanar ctrl pts.
                splane = this;
                sext = b;
                shext = agnstB;
            } else {
                splane = b;
                sext = this;
                shext = agnstA;
            }
            bool foundExact = false;
            for(SCurve &sc : shext->curve) {
                if(sc.source == SCurve::Source::INTERSECTION) continue;
                if(!sc.isExact) continue;
                if((sc.surfA != sext->h) && (sc.surfB != sext->h)) continue;
                // we have a curve belonging to the curved surface and not the plane.
                // does it lie completely in the plane?
                if(splane->ContainsPlaneCurve(&sc)) {
                    SBezier bezier = sc.exact;
                    AddExactIntersectionCurve(&bezier, b, agnstA, agnstB, into);
                    foundExact = true;
                }
            }
            // if we found at lest one of these we don't want to do the numerical
            // intersection as well. Sometimes it will also find the same curve but
            // with different PWLs and the polygon will fail to assemble.
            if(foundExact)
                return;
        }

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

                // Include tangent points. The intersection curve can end
                // where the surfaces are tangent, e.g. where two fillets of
                // the same radius meet on the face they both blend into, and
                // the march needs that point to end on. The numerical
                // intersection reports such points anyway; the closed-form
                // one for cylinders drops them unless asked.
                srfB->AllPointsIntersecting(se->a, se->b, &lsi,
                    /*asSegment=*/true, /*trimmed=*/true, /*inclTangent=*/true);
                // Find the other surface that this curve trims.
                hSCurve hsc = { (uint32_t)se->auxA };
                SCurve *sc = shA->curve.FindById(hsc);

                // AllPointsIntersecting skips hits at the ends of the segment,
                // so an intersection that falls exactly on a vertex of our
                // trim curves would be missed, e.g. where the end of an
                // extrusion lies on a face of the other shell (issue #1755).
                if(EdgeVertexOnSurface(srfB, se->a, sc, se->b.Minus(se->a))) {
                    SInter si = {};
                    si.p = se->a;
                    lsi.Add(&si);
                }
                if(EdgeVertexOnSurface(srfB, se->b, sc, se->b.Minus(se->a))) {
                    SInter si = {};
                    si.p = se->b;
                    lsi.Add(&si);
                }
                if(lsi.IsEmpty())
                    continue;

                hSSurface hother = (sc->surfA == srfA->h) ?
                                                    sc->surfB : sc->surfA;
                SSurface *other = shA->surface.FindById(hother);

                SInter *si;
                for(si = lsi.First(); si; si = lsi.NextAfter(si)) {
                    Vector p = si->p;
                    double u, v;
                    srfB->ClosestPointTo(p, &u, &v);
                    if(sc->isExact) {
                        srfB->PointOnCurve(&(sc->exact), &u, &v);
                    } else {
                        srfB->PointOnSurfaces(srfA, other, &u, &v);
                    }
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

        // Where the surfaces are tangent, a point turns into one place to
        // start for each direction in which a curve leaves it; or into none,
        // if the surfaces only touch there.
        List<MarchPoint> mpl = {};
        for(SPoint &sp : spl.l) {
            Vector rays[4];
            int nr = TangentRays(this, b, sp.p, rays, &spl);
            if(nr < 0) {
                MarchPoint mp = { 0, sp.p, sp.auxv, /*tangent=*/false };
                mpl.Add(&mp);
            }
            for(int i = 0; i < nr; i++) {
                MarchPoint mp = { 0, sp.p, rays[i], /*tangent=*/true };
                mpl.Add(&mp);
            }
        }

        while(mpl.n >= 2) {
            SCurve sc = {};
            sc.surfA = h;
            sc.surfB = b->h;
            sc.isExact = false;
            sc.source = SCurve::Source::INTERSECTION;

            Vector start  = mpl[0].p,
                   startv = mpl[0].dir;
            bool fromTangent = mpl[0].tangent;
            Vector origin = start;
            mpl.ClearTags();
            mpl[0].tag = 1;
            mpl.RemoveTagged();

            // Our chord tolerance is whatever the user specified
            double maxtol = SS.ChordTolMm();
            int maxsteps = max(300, SS.GetMaxSegments()*3);

            // The curve starts at our starting point.
            SCurvePt padd = {};
            padd.vertex = true;
            padd.p = start;
            sc.pts.Add(&padd);

            Point2d pa, pb;
            Vector np, npc = {}, prevDir = startv;
            bool fwd = false;
            // Better to start with a too-small step, so that we don't miss
            // features of the curve entirely.
            double tol, step = maxtol;
            for(a = 0; a < maxsteps; a++) {
                ClosestPointTo(start, &pa);
                b->ClosestPointTo(start, &pb);

                Vector na =    NormalAt(pa).WithMagnitude(1),
                       nb = b->NormalAt(pb).WithMagnitude(1);

                Vector dp;
                if(fromTangent) {
                    // From a point of tangency, take the first step in the
                    // direction the curve leaves in; after that, keep going
                    // the same way.
                    if(a == 0) {
                        dp = startv;
                    } else {
                        dp = nb.Cross(na).WithMagnitude(1.0);
                        if(dp.Dot(prevDir) < 0) dp = dp.ScaledBy(-1.0);
                    }
                } else {
                    if(a == 0) {
                        dp = nb.Cross(na);
                        if(dp.Dot(startv) < 0) {
                            // We want to march in the more inward direction.
                            fwd = true;
                        } else {
                            fwd = false;
                        }
                    }
                    dp = nb.Cross(na).WithMagnitude(1.0);
                    if(!fwd) dp = dp.ScaledBy(-1.0);
                }
                int i;
                for(i = 0; i < 20; i++) {
                    np = start.Plus(dp.ScaledBy(step));
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

                // Stop at the first point we reach, other than the one we
                // started from; from a point of tangency, the other curves
                // that leave it start there too.
                MarchPoint *hit = NULL;
                for(MarchPoint &mp : mpl) {
                    if(mp.p.Equals(origin)) continue;
                    if((mp.p).OnLineSegment(start, npc, 2*SS.ChordTolMm())) {
                        // It must be ahead of us, not off to the side; near a
                        // point of tangency the end of another curve can be
                        // much closer than the chord tolerance.
                        Vector toP = (mp.p).Minus(start);
                        if(toP.Dot(dp) < 0.5*toP.Magnitude()) continue;
                        hit = &mp;
                        break;
                    }
                }
                if(hit) {
                    Vector end = hit->p;
                    if(hit->tangent) {
                        // Of the curves that leave this point, we came in
                        // along the one whose direction points back at us.
                        Vector back = start.Minus(end);
                        double best = VERY_NEGATIVE;
                        for(MarchPoint &mp : mpl) {
                            if(!mp.tangent || !mp.p.Equals(end)) continue;
                            if(mp.dir.Dot(back) > best) {
                                best = mp.dir.Dot(back);
                                hit = &mp;
                            }
                        }
                    }
                    hit->tag = 1;
                    a = maxsteps;
                    npc = end;
                }

                prevDir = npc.Minus(start);
                padd.p = npc;
                padd.vertex = (a == maxsteps);
                sc.pts.Add(&padd);

                start = npc;
            }

            mpl.RemoveTagged();

            // And now we split and insert the curve
            SCurve split = sc.MakeCopySplitAgainst(agnstA, agnstB, this, b);
            sc.Clear();
#pragma omp critical(into)
            {
                into->curve.AddAndAssignId(&split);
            }
        }
        mpl.Clear();
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
// Does a planar surface contain a curve? Does the curve lie completely in plane?
//-----------------------------------------------------------------------------
bool SSurface::ContainsPlaneCurve(SCurve *sc) const {
    if(degm != 1 || degn != 1) return false;
    if(!sc->isExact) return false; // we don't handle those (yet?)
    
    Vector p = ctrl[0][0];
    Vector n = NormalAt(0, 0).WithMagnitude(1);
    double d = n.Dot(p);

    // check all control points on the curve
    for(int i=0; i<= sc->exact.deg; i++) {
        if(fabs(n.Dot(sc->exact.ctrl[i]) - d) > LENGTH_EPS) return false;
    }
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
    for(SSurface &ss : surface) {
        if(proto->CoincidentWith(&ss, sameNormal)) {
            ss.MakeEdgesInto(this, el, SSurface::MakeAs::XYZ, useCurvesFrom);
        }
    }

    SEdge *se;
    for(se = el->l.First(); se; se = el->l.NextAfter(se)) {
        double ua, va, ub, vb;
        proto->ClosestPointTo(se->a, &ua, &va);
        proto->ClosestPointTo(se->b, &ub, &vb);

        if(sameNormal) {
            se->a = {ua, va, 0};
            se->b = {ub, vb, 0};
        } else {
            // Flip normal, so flip all edge directions
            se->b = {ua, va, 0};
            se->a = {ub, vb, 0};
        }
    }
}

} // namespace SolveSpace
