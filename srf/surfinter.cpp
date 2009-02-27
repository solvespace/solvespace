#include "solvespace.h"

extern int FLAG;

void SSurface::AddExactIntersectionCurve(SBezier *sb, SSurface *srfB,
                            SShell *agnstA, SShell *agnstB, SShell *into)
{
    SCurve sc;
    ZERO(&sc);
    sc.surfA = h;
    sc.surfB = srfB->h;
    sb->MakePwlInto(&(sc.pts));

/*
    Vector *prev = NULL, *v;
    for(v = sc.pts.First(); v; v = sc.pts.NextAfter(v)) {
      if(prev) SS.nakedEdges.AddEdge(*prev, *v);
        prev = v;
    } */

    // Now split the line where it intersects our existing surfaces
    SCurve split = sc.MakeCopySplitAgainst(agnstA, agnstB, this, srfB);
    sc.Clear();

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

    if((degm == 1 && degn == 1 && b->IsExtrusion(NULL, NULL)) ||
       (b->degm == 1 && b->degn == 1 && this->IsExtrusion(NULL, NULL)))
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
            sext->AllPointsIntersecting(p0, p0.Plus(dp), &inters, false, false);
    
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
            // intersection is projection of extruded curve into our plane

            int i;
            for(i = 0; i <= bezier.deg; i++) {
                Vector p0 = bezier.ctrl[i],
                       p1 = p0.Plus(along);

                bezier.ctrl[i] =
                    Vector::AtIntersectionOfPlaneAndLine(n, d, p0, p1, NULL);
            }

            AddExactIntersectionCurve(&bezier, b, agnstA, agnstB, into);
        }
    }

    // need to implement general numerical surface intersection for tough
    // cases, just giving up for now
}

//-----------------------------------------------------------------------------
// Find all points where a line through a and b intersects our surface, and
// add them to the list. If seg is true then report only intersections that
// lie within the finite line segment (not including the endpoints); otherwise
// we work along the infinite line.
//-----------------------------------------------------------------------------
void SSurface::AllPointsIntersecting(Vector a, Vector b,
                                     List<SInter> *l, bool seg, bool trimmed)
{
    Vector ba = b.Minus(a);
    double bam = ba.Magnitude();

    typedef struct {
        int     tag;
        Point2d p;
    } Inter;
    List<Inter> inters;
    ZERO(&inters);

    // First, get all the intersections between the infinite ray and the
    // untrimmed surface.
    int i, j;
    for(i = 0; i < degm; i++) {
        for(j = 0; j < degn; j++) {
            // Intersect the ray with each face in the control polyhedron
            Vector p00 = ctrl[i][j],
                   p01 = ctrl[i][j+1],
                   p10 = ctrl[i+1][j];

            Vector u = p01.Minus(p00), v = p10.Minus(p00);

            Vector n = (u.Cross(v)).WithMagnitude(1);
            double d = n.Dot(p00);

            bool parallel;
            Vector pi =
                Vector::AtIntersectionOfPlaneAndLine(n, d, a, b, &parallel);
            if(parallel) continue;

            double ui = (pi.Minus(p00)).Dot(u) / u.MagSquared(),
                   vi = (pi.Minus(p00)).Dot(v) / v.MagSquared();

            double tol = 1e-2;
            if(ui < -tol || ui > 1 + tol || vi < -tol || vi > 1 + tol) {
                continue;
            }

            Inter inter;
            ClosestPointTo(pi, &inter.p.x, &inter.p.y, false);
            if(PointIntersectingLine(a, b, &inter.p.x, &inter.p.y)) {
                inters.Add(&inter);
            } else {
                // No convergence; but this might not be an error, since
                // the line can intersect the convex hull more times than
                // it intersects the surface itself.
            }
        }
    }

    // Remove duplicate intersection points
    inters.ClearTags();
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
        Point2d dummy = { 0, 0 };
        int c = bsp->ClassifyPoint(puv, dummy);
        if(trimmed && c == SBspUv::OUTSIDE) {
            continue;
        }

        // It does, so generate the intersection
        SInter si;
        si.p = pxyz;
        si.surfNormal = NormalAt(puv.x, puv.y);
        si.hsrf = h;
        si.srf = this;
        si.onEdge = (c != SBspUv::INSIDE);
        l->Add(&si);
    }

    inters.Clear();
}

void SShell::AllPointsIntersecting(Vector a, Vector b,
                                   List<SInter> *il, bool seg, bool trimmed)
{
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        ss->AllPointsIntersecting(a, b, il, seg, trimmed);
    }
}

//-----------------------------------------------------------------------------
// Does the given point lie on our shell? It might be inside or outside, or
// it might be on the surface with pout parallel or anti-parallel to the
// intersecting surface's normal.
//
// To calculate, we intersect a ray through p with our shell, and classify
// using the closest intersection point. If the ray hits a surface on edge,
// then just reattempt in a different random direction.
//-----------------------------------------------------------------------------
int SShell::ClassifyPoint(Vector p, Vector pout) {
    List<SInter> l;
    ZERO(&l);

    srand(0);

    int ret, cnt = 0, edge_inters;
    double edge_dotp[2];

    for(;;) {
        // Cast a ray in a random direction (two-sided so that we test if
        // the point lies on a surface, but use only one side for in/out
        // testing)
        Vector ray = Vector::From(Random(1), Random(1), Random(1));
        AllPointsIntersecting(p.Minus(ray), p.Plus(ray), &l, false, true);
        
        double dmin = VERY_POSITIVE;
        ret = OUTSIDE; // no intersections means it's outside
        bool onEdge = false;
        edge_inters = 0;

        if(FLAG) {
            dbp("inters=%d cnt=%d", l.n, cnt);
            SS.nakedEdges.AddEdge(p, p.Plus(ray.WithMagnitude(50)));
        }

        SInter *si;
        for(si = l.First(); si; si = l.NextAfter(si)) {
            double t = ((si->p).Minus(p)).DivPivoting(ray);
            if(t*ray.Magnitude() < -LENGTH_EPS) {
                // wrong side, doesn't count
                continue;
            }

            if(FLAG) SS.nakedEdges.AddEdge(p, si->p);

            double d = ((si->p).Minus(p)).Magnitude();

            // Handle edge-on-edge
            if(d < LENGTH_EPS && si->onEdge && edge_inters < 2) {
                edge_dotp[edge_inters] = (si->surfNormal).Dot(pout);
                edge_inters++;
            }

            if(d < dmin) {
                dmin = d;
                if(d < LENGTH_EPS) {
                    // Edge-on-face (unless edge-on-edge above supercedes)
                    if((si->surfNormal).Dot(pout) > 0) {
                        ret = SURF_PARALLEL;
                    } else {
                        ret = SURF_ANTIPARALLEL;
                    }
                } else {
                    // Edge does not lie on surface; either strictly inside
                    // or strictly outside
                    if((si->surfNormal).Dot(ray) > 0) {
                        ret = INSIDE;
                    } else {
                        ret = OUTSIDE;
                    }
                }
                onEdge = si->onEdge;
            }
        }
        l.Clear();

        // If the point being tested lies exactly on an edge of the shell,
        // then our ray always lies on edge, and that's okay. Otherwise
        // try again in a different random direction.
        if((edge_inters == 2) || !onEdge) break;
        if(cnt++ > 20) {
            dbp("can't find a ray that doesn't hit on edge!");
            break;
        }
    }

    if(edge_inters == 2) {
        double tol = 1e-3;

        if(edge_dotp[0] > -tol && edge_dotp[1] > -tol) {
            return EDGE_PARALLEL;
        } else if(edge_dotp[0] < tol && edge_dotp[1] < tol) {
            return EDGE_ANTIPARALLEL;
        } else {
            return EDGE_TANGENT;
        }
    } else {
        return ret;
    }
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
                                     SEdgeList *el)
{
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        if(proto->CoincidentWith(ss, sameNormal)) {
            ss->MakeEdgesInto(this, el, false);
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

