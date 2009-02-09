#include "solvespace.h"

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

    if(degm == 1 && degn == 1 && b->degm == 1 && b->degn == 1) {
        // Plane-plane intersection, easy; result is a line
        Vector pta = ctrl[0][0], ptb = b->ctrl[0][0];
        Vector na = NormalAt(0, 0), nb = b->NormalAt(0, 0);
        na = na.WithMagnitude(1);
        nb = nb.WithMagnitude(1);

        Vector d = (na.Cross(nb));

        if(d.Magnitude() < LENGTH_EPS) {
            // parallel planes, no intersection
            return;
        }

        Vector inter = Vector::AtIntersectionOfPlanes(na, na.Dot(pta),
                                                      nb, nb.Dot(ptb));

        // The intersection curve can't be longer than the longest curve
        // that lies in both planes, which is the diagonal of the shorter;
        // so just pick one, and then give some slop, not critical.
        double maxl = ((ctrl[0][0]).Minus(ctrl[1][1])).Magnitude();

        Vector v;
        SCurve sc;
        ZERO(&sc);
        sc.surfA = h;
        sc.surfB = b->h;
        v = inter.Minus(d.WithMagnitude(5*maxl));
        sc.pts.Add(&v);
        v = inter.Plus(d.WithMagnitude(5*maxl));
        sc.pts.Add(&v);

        // Now split the line where it intersects our existing surfaces
        SCurve split = sc.MakeCopySplitAgainst(agnstA, agnstB);
        sc.Clear();

        split.interCurve = true;
        into->curve.AddAndAssignId(&split);
    }

    // need to implement general numerical surface intersection for tough
    // cases, just giving up for now
}

void SSurface::AllPointsIntersecting(Vector a, Vector b, List<SInter> *l) {
    if(degm == 1 && degn == 1) {
        // line-plane intersection
        Vector p = ctrl[0][0];
        Vector n = NormalAt(0, 0).WithMagnitude(1);
        double d = n.Dot(p);
        if((n.Dot(a) - d < -LENGTH_EPS && n.Dot(b) - d > LENGTH_EPS) ||
           (n.Dot(b) - d < -LENGTH_EPS && n.Dot(a) - d > LENGTH_EPS))
        {
            // It crosses the plane, one point of intersection
            // (a + t*(b - a)) dot n = d
            // (a dot n) + t*((b - a) dot n) = d
            // t = (d - (a dot n))/((b - a) dot n)
            double t = (d - a.Dot(n)) / ((b.Minus(a)).Dot(n));
            Vector pi = a.Plus((b.Minus(a)).ScaledBy(t));

            Point2d puv, dummy = { 0, 0 };
            ClosestPointTo(pi, &(puv.x), &(puv.y));
            int c = bsp->ClassifyPoint(puv, dummy);

            if(c != SBspUv::OUTSIDE) {
                SInter si;
                si.p = pi;
                si.surfNormal = NormalAt(puv.x, puv.y);
                si.surface = h;
                si.onEdge = (c != SBspUv::INSIDE);
                l->Add(&si);
            }
        }
    }
}

void SShell::AllPointsIntersecting(Vector a, Vector b, List<SInter> *il) {
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)) {
        ss->AllPointsIntersecting(a, b, il);
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

    int ret, cnt = 0;
    for(;;) {
        // Cast a ray in a random direction (two-sided so that we test if
        // the point lies on a surface, but use only one side for in/out
        // testing)
        Vector ray = Vector::From(Random(1), Random(1), Random(1));
        ray = ray.WithMagnitude(1e4);
        AllPointsIntersecting(p.Minus(ray), p.Plus(ray), &l);
        
        double dmin = VERY_POSITIVE;
        ret = OUTSIDE; // no intersections means it's outside
        bool onEdge = false;

        SInter *si;
        for(si = l.First(); si; si = l.NextAfter(si)) {
            double t = ((si->p).Minus(p)).DivPivoting(ray);
            if(t*ray.Magnitude() < -LENGTH_EPS) {
                // wrong side, doesn't count
                continue;
            }

            double d = ((si->p).Minus(p)).Magnitude();

            if(d < dmin) {
                dmin = d;
                if(d < LENGTH_EPS) {
                    // Lies on the surface
                    if((si->surfNormal).Dot(pout) > 0) {
                        ret = ON_PARALLEL;
                    } else {
                        ret = ON_ANTIPARALLEL;
                    }
                } else {
                    // Does not lie on this surface; inside or out, depending
                    // on the normal
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
        // then our ray always lies on edge, and that's okay.
        if(ret == ON_PARALLEL || ret == ON_ANTIPARALLEL || !onEdge) break;
        if(cnt++ > 10) {
            dbp("can't find a ray that doesn't hit on edge!");
            break;
        }
    }

    return ret;
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

