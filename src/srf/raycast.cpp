//-----------------------------------------------------------------------------
// Routines for ray-casting: intersecting a line segment or an infinite line
// with a surface or shell. Ray-casting against a shell is used for point-in-
// shell testing, and the intersection of edge line segments against surfaces
// is used to get rough surface-curve intersections, which are later refined
// numerically.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

// Dot product tolerance for perpendicular; this is on the direction cosine,
// so it's about 0.001 degrees.
const double SShell::DOTP_TOL = 1e-5;

extern int FLAG;


double SSurface::DepartureFromCoplanar() const {
    int i, j;
    int ia, ja, ib = 0, jb = 0, ic = 0, jc = 0;
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

void SSurface::WeightControlPoints() {
    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            ctrl[i][j] = (ctrl[i][j]).ScaledBy(weight[i][j]);
        }
    }
}
void SSurface::UnWeightControlPoints() {
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

        default: ssassert(false, "Unexpected degree of spline");
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
                                              List<Inter> *l, bool asSegment,
                                              SSurface *sorig)
{
    // Test if the line intersects our axis-aligned bounding box; if no, then
    // no possibility of an intersection
    if(LineEntirelyOutsideBbox(a, b, asSegment)) return;

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
        sorig->ClosestPointTo(p, &(inter.p.x), &(inter.p.y), /*mustConverge=*/false);
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
    surf0.AllPointsIntersectingUntrimmed(a, b, cnt, level, l, asSegment, sorig);
    (*level) = nextLevel;
    surf1.AllPointsIntersectingUntrimmed(a, b, cnt, level, l, asSegment, sorig);
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
                                     bool asSegment, bool trimmed, bool inclTangent)
{
    if(LineEntirelyOutsideBbox(a, b, asSegment)) return;

    Vector ba = b.Minus(a);
    double bam = ba.Magnitude();

    List<Inter> inters = {};

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
        if(!asSegment ||
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
            double t = (ip[i].Minus(ap)).DivProjected(bp.Minus(ap));
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
        AllPointsIntersectingUntrimmed(a, b, &cnt, &level, &inters, asSegment, this);
    }

    // Remove duplicate intersection points
    inters.ClearTags();
    int i, j;
    for(i = 0; i < inters.n; i++) {
        for(j = i + 1; j < inters.n; j++) {
            if(inters[i].p.Equals(inters[j].p)) {
                inters[j].tag = 1;
            }
        }
    }
    inters.RemoveTagged();

    for(i = 0; i < inters.n; i++) {
        Point2d puv = inters[i].p;

        // Make sure the point lies within the finite line segment
        Vector pxyz = PointAt(puv.x, puv.y);
        double t = (pxyz.Minus(a)).DivProjected(ba);
        if(asSegment && (t > 1 - LENGTH_EPS/bam || t < LENGTH_EPS/bam)) {
            continue;
        }

        // And that it lies inside our trim region
        Point2d dummy = { 0, 0 };
        SBspUv::Class c = (bsp) ? bsp->ClassifyPoint(puv, dummy, this) : SBspUv::Class::OUTSIDE;
        if(trimmed && c == SBspUv::Class::OUTSIDE) {
            continue;
        }

        // It does, so generate the intersection
        SInter si;
        si.p = pxyz;
        si.surfNormal = NormalAt(puv.x, puv.y);
        si.pinter = puv;
        si.srf = this;
        si.onEdge = (c != SBspUv::Class::INSIDE);
        l->Add(&si);
    }

    inters.Clear();
}

void SShell::AllPointsIntersecting(Vector a, Vector b,
                                   List<SInter> *il,
                                   bool asSegment, bool trimmed, bool inclTangent)
{
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        ss->AllPointsIntersecting(a, b, il,
            asSegment, trimmed, inclTangent);
    }
}



SShell::Class SShell::ClassifyRegion(Vector edge_n, Vector inter_surf_n,
                           Vector edge_surf_n) const
{
    double dot = inter_surf_n.DirectionCosineWith(edge_n);
    if(fabs(dot) < DOTP_TOL) {
        // The edge's surface and the edge-on-face surface
        // are coincident. Test the edge's surface normal
        // to see if it's with same or opposite normals.
        if(inter_surf_n.Dot(edge_surf_n) > 0) {
            return Class::COINC_SAME;
        } else {
            return Class::COINC_OPP;
        }
    } else if(dot > 0) {
        return Class::OUTSIDE;
    } else {
        return Class::INSIDE;
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
bool SShell::ClassifyEdge(Class *indir, Class *outdir,
                          Vector ea, Vector eb,
                          Vector p,
                          Vector edge_n_in, Vector edge_n_out, Vector surf_n)
{
    List<SInter> l = {};

    srand(0);

    // First, check for edge-on-edge
    int edge_inters = 0;
    Vector inter_surf_n[2], inter_edge_n[2];
    SSurface *srf;
    for(srf = surface.First(); srf; srf = surface.NextAfter(srf)) {
        if(srf->LineEntirelyOutsideBbox(ea, eb, /*asSegment=*/true)) continue;

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
                    srf->ClosestPointTo(p,  &pm, /*mustConverge=*/false);
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
        //! @todo make this use the appropriate curved normals
        double dotp[2];
        for(int i = 0; i < 2; i++) {
            dotp[i] = edge_n_out.DirectionCosineWith(inter_surf_n[i]);
        }

        if(fabs(dotp[1]) < DOTP_TOL) {
            swap(dotp[0],         dotp[1]);
            swap(inter_surf_n[0], inter_surf_n[1]);
            swap(inter_edge_n[0], inter_edge_n[1]);
        }

        Class coinc = (surf_n.Dot(inter_surf_n[0])) > 0 ? Class::COINC_SAME : Class::COINC_OPP;

        if(fabs(dotp[0]) < DOTP_TOL && fabs(dotp[1]) < DOTP_TOL) {
            // This is actually an edge on face case, just that the face
            // is split into two pieces joining at our edge.
            *indir  = coinc;
            *outdir = coinc;
        } else if(fabs(dotp[0]) < DOTP_TOL && dotp[1] > DOTP_TOL) {
            if(edge_n_out.Dot(inter_edge_n[0]) > 0) {
                *indir  = coinc;
                *outdir = Class::OUTSIDE;
            } else {
                *indir  = Class::INSIDE;
                *outdir = coinc;
            }
        } else if(fabs(dotp[0]) < DOTP_TOL && dotp[1] < -DOTP_TOL) {
            if(edge_n_out.Dot(inter_edge_n[0]) > 0) {
                *indir  = coinc;
                *outdir = Class::INSIDE;
            } else {
                *indir  = Class::OUTSIDE;
                *outdir = coinc;
            }
        } else if(dotp[0] > DOTP_TOL && dotp[1] > DOTP_TOL) {
            *indir  = Class::INSIDE;
            *outdir = Class::OUTSIDE;
        } else if(dotp[0] < -DOTP_TOL && dotp[1] < -DOTP_TOL) {
            *indir  = Class::OUTSIDE;
            *outdir = Class::INSIDE;
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
        if(srf->LineEntirelyOutsideBbox(ea, eb, /*asSegment=*/true)) continue;

        Point2d puv;
        srf->ClosestPointTo(p, &(puv.x), &(puv.y), /*mustConverge=*/false);
        Vector pp = srf->PointAt(puv);

        if((pp.Minus(p)).Magnitude() > LENGTH_EPS) continue;
        Point2d dummy = { 0, 0 };
        SBspUv::Class c = (srf->bsp) ? srf->bsp->ClassifyPoint(puv, dummy, srf) : SBspUv::Class::OUTSIDE;
        if(c == SBspUv::Class::OUTSIDE) continue;

        // Edge-on-face (unless edge-on-edge above superceded)
        Point2d pin, pout;
        srf->ClosestPointTo(p.Plus(edge_n_in),  &pin,  /*mustConverge=*/false);
        srf->ClosestPointTo(p.Plus(edge_n_out), &pout, /*mustConverge=*/false);

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
            p.Minus(ray), p.Plus(ray), &l,
                /*asSegment=*/false, /*trimmed=*/true, /*inclTangent=*/false);

        // no intersections means it's outside
        *indir  = Class::OUTSIDE;
        *outdir = Class::OUTSIDE;
        double dmin = VERY_POSITIVE;
        bool onEdge = false;
        edge_inters = 0;

        SInter *si;
        for(si = l.First(); si; si = l.NextAfter(si)) {
            double t = ((si->p).Minus(p)).DivProjected(ray);
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
                    *indir  = Class::INSIDE;
                    *outdir = Class::INSIDE;
                } else {
                    *indir  = Class::OUTSIDE;
                    *outdir = Class::OUTSIDE;
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

