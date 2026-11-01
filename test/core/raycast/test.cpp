#include "solvespace.h"

#include "harness.h"

// A part of a cylinder of radius r about the z axis, for z from 0 to 10, from
// 45 to 135 degrees around, so symmetric about the y axis.
static SSurface MakeCylinder(double r) {
    // A circular arc of 90 degrees as a rational quadratic: the middle control
    // point is where the end tangents meet, weighted by cos(45 degrees).
    double w = cos(PI/4), c = r*w;
    SBezier arc = SBezier::From(Vector::From( c, c,   0),
                                Vector::From( 0, r/w, 0),
                                Vector::From(-c, c,   0));
    arc.weight[1] = w;
    return SSurface::FromExtrusionOf(&arc, Vector::From(0, 0, 0),
                                     Vector::From(0, 0, 10));
}

// A line within LENGTH_EPS of the radius of a cylinder still crosses it at two
// points, much farther apart than LENGTH_EPS; it isn't tangent.
TEST_CASE(line_just_inside_radius) {
    const double r = 7.5, d = r - LENGTH_EPS/2;
    SSurface srf = MakeCylinder(r);

    List<SInter> l = {};
    srf.AllPointsIntersecting(Vector::From(-5, d, 5), Vector::From(5, d, 5), &l,
        /*asSegment=*/true, /*trimmed=*/false, /*inclTangent=*/false);
    size_t n = l.n;
    double x0 = (n == 2) ? min(l[0].p.x, l[1].p.x) : 0,
           x1 = (n == 2) ? max(l[0].p.x, l[1].p.x) : 0;
    l.Clear();

    double xint = sqrt(r*r - d*d);
    CHECK_TRUE(n == 2);
    CHECK_EQ_EPS(x0, -xint);
    CHECK_EQ_EPS(x1,  xint);
}

// A line at the radius is tangent, so it touches at one point, or none if
// tangent points aren't asked for.
TEST_CASE(line_at_radius) {
    const double r = 7.5;
    SSurface srf = MakeCylinder(r);

    List<SInter> l = {};
    srf.AllPointsIntersecting(Vector::From(-5, r, 5), Vector::From(5, r, 5), &l,
        /*asSegment=*/true, /*trimmed=*/false, /*inclTangent=*/true);
    size_t n = l.n;
    double x = (n == 1) ? l[0].p.x : 1;
    l.Clear();
    CHECK_TRUE(n == 1);
    CHECK_EQ_EPS(x, 0);

    srf.AllPointsIntersecting(Vector::From(-5, r, 5), Vector::From(5, r, 5), &l,
        /*asSegment=*/true, /*trimmed=*/false, /*inclTangent=*/false);
    n = l.n;
    l.Clear();
    CHECK_TRUE(n == 0);
}
