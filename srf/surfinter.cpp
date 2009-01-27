#include "solvespace.h"

void SSurface::IntersectAgainst(SSurface *b, SShell *into) {
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
        v = inter.Minus(d.WithMagnitude(2*maxl));
        sc.pts.Add(&v);
        v = inter.Plus(d.WithMagnitude(2*maxl));
        sc.pts.Add(&v);
        sc.interCurve = true;
       
        // Now split the line where it intersects our existing surfaces
        SCurve split = sc.MakeCopySplitAgainst(into);
        sc.Clear();

        into->curve.AddAndAssignId(&split);
    }

    // need to implement general numerical surface intersection for tough
    // cases, just giving up for now
}

