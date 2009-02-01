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
            if(bsp->ClassifyPoint(puv, dummy) != SBspUv::OUTSIDE) {
                SInter si;
                si.p = pi;
                si.dot = NormalAt(puv.x, puv.y).Dot(b.Minus(a));
                si.surface = h;
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

int SShell::ClassifyPoint(Vector p) {
    List<SInter> l;
    ZERO(&l);

    Vector d = Vector::From(1e5, 0, 0); // direction is arbitrary
    // but it does need to be a one-sided ray
    AllPointsIntersecting(p, p.Plus(d), &l);
    
    double dmin = VERY_POSITIVE;
    int ret = OUTSIDE; // no intersections means it's outside

    SInter *si;
    for(si = l.First(); si; si = l.NextAfter(si)) {
        double d = ((si->p).Minus(p)).Magnitude();

        if(d < dmin) {
            dmin = d;
            if(d < LENGTH_EPS) {
                ret = ON_SURFACE;
            } else if(si->dot > 0) {
                ret = INSIDE;
            } else {
                ret = OUTSIDE;
            }
        }
    }

    l.Clear();
    return ret;
}

