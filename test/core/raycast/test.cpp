#include "solvespace.h"

#include "harness.h"

struct SurfaceTestTessellation {
    double chord = SS.chordTolCalculated;
    int segments = SS.maxSegments;
    bool exporting = SS.exportMode;
    SurfaceTestTessellation() { SS.chordTolCalculated = 0.001; SS.maxSegments = 128; SS.exportMode = false; }
    ~SurfaceTestTessellation() {
        SS.chordTolCalculated = chord; SS.maxSegments = segments; SS.exportMode = exporting;
    }
};

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

TEST_CASE(skew_affine_surface_inverse) {
    SSurface surface = SSurface::FromPlane({2, -3, 4}, {1, 2, 0}, {2, 0, 0});
    CHECK_TRUE(surface.IsAffine());
    CHECK_TRUE(surface.IsPlanar());
    for(double u : {0.13, 0.37, 0.81}) for(double v : {0.17, 0.43, 0.89}) {
        Vector point = surface.PointAt(u, v);
        Point2d uv;
        surface.ClosestPointTo(point, &uv);
        CHECK_TRUE(uv.Equals({u, v}, 1e-10));
        CHECK_TRUE(surface.PointAt(uv).Equals(point, 1e-10));
    }
}

TEST_CASE(rational_and_nonaffine_plane_inverse_and_raycast) {
    for(int variant = 0; variant < 3; ++variant) {
        SSurface surface = SSurface::FromPlane({0, 0, 2}, {0, 1, 0}, {1, 0, 0});
        if(variant != 1) surface.weight[1][1] = 2;
        if(variant != 0) surface.ctrl[1][1] = {1, 1.25, 2};
        CHECK_FALSE(surface.IsAffine());
        CHECK_TRUE(surface.IsPlanar());
        for(double u : {0.13, 0.37, 0.81}) for(double v : {0.17, 0.43, 0.89}) {
            Vector point = surface.PointAt(u, v);
            Point2d uv;
            surface.ClosestPointTo(point, &uv);
            CHECK_TRUE(uv.Equals({u, v}, 1e-8));
            CHECK_TRUE(surface.PointAt(uv).Equals(point, 1e-8));
            List<SInter> intersections = {};
            surface.AllPointsIntersecting(point.Plus({0, 0, -1}), point.Plus({0, 0, 1}),
                &intersections, /*asSegment=*/true, /*trimmed=*/false, /*inclTangent=*/false);
            bool hit = intersections.n == 1 && intersections[0].p.Equals(point, 1e-8) &&
                       intersections[0].pinter.Equals({u, v}, 1e-8);
            intersections.Clear();
            CHECK_TRUE(hit);
        }
    }
}

TEST_CASE(straight_trim_midpoint_on_rational_and_nonaffine_plane) {
    SurfaceTestTessellation tessellation;
    for(int variant = 0; variant < 3; ++variant) {
        SSurface surface = SSurface::FromPlane({0, 0, 2}, {0, 1, 0}, {1, 0, 0});
        if(variant != 1) surface.weight[1][1] = 2;
        if(variant != 0) surface.ctrl[1][1] = {1, 1.25, 2};
        SCurve curve = {};
        curve.h = {1};
        curve.isExact = true;
        curve.exact = SBezier::From(Vector{0, 0.5, 2}, Vector{1, 0.5, 2});
        Point2d a, b;
        surface.ClosestPointTo(curve.exact.Start(), &a);
        surface.ClosestPointTo(curve.exact.Finish(), &b);
        // The UV midpoint does not lie on this straight XYZ trim.
        CHECK_TRUE(fabs(surface.PointAt(a.Plus(b).ScaledBy(0.5)).y - 0.5) > 1e-3);
        SShell shell = {};
        shell.curve.Add(&curve);
        Vector p, enin, enout, normal;
        surface.EdgeNormalsWithinSurface(a, b, &p, &enin, &enout, &normal,
                                         curve.h.v, &shell, NULL, NULL);
        shell.Clear();
        CHECK_TRUE(p.x > 0 && p.x < 1);
        CHECK_TRUE(fabs(p.y - 0.5) < 1e-10 && fabs(p.z - 2) < 1e-10);
        CHECK_TRUE(enin.y < 0 && enout.y > 0);
        CHECK_TRUE(normal.WithMagnitude(1).Equals({0, 0, 1}, 1e-10));
    }
}

TEST_CASE(bilinear_patch_is_not_a_plane) {
    SurfaceTestTessellation tessellation;
    for(double warp : {0.4, 1e-8}) {
        SSurface surface = SSurface::FromPlane({0, 0, 0}, {0, 1, 0}, {1, 0, 0});
        surface.ctrl[1][1].z = warp;
        CHECK_FALSE(surface.IsAffine());
        CHECK_FALSE(surface.IsPlanar());
        Vector point = surface.PointAt(0.3, 0.7);
        List<SInter> intersections = {};
        surface.AllPointsIntersecting({point.x, point.y, -1}, {point.x, point.y, 1},
            &intersections, /*asSegment=*/true, /*trimmed=*/false, /*inclTangent=*/false);
        bool hit = intersections.n == 1 && intersections[0].p.Equals(point, 1e-10);
        intersections.Clear();
        CHECK_TRUE(hit);
    }
}

TEST_CASE(higher_degree_plane_checks_entire_control_net) {
    SSurface surface = {};
    surface.degm = surface.degn = 3;
    for(int i = 0; i <= 3; ++i) for(int j = 0; j <= 3; ++j) {
        surface.ctrl[i][j] = {i / 3.0, j / 3.0, 0};
        surface.weight[i][j] = (i == 1 && j == 2) ? 0.75 : 1;
    }
    SSurface plane = SSurface::FromPlane({0, 0, 0}, {0, 1, 0}, {1, 0, 0});
    SCurve boundary = {};
    boundary.isExact = true;
    boundary.exact = SBezier::From(Vector{0, 0, 0}, Vector{1, 1, 0});
    CHECK_TRUE(surface.IsPlanar());
    CHECK_FALSE(surface.IsAffine());
    CHECK_TRUE(surface.CoincidentWith(&plane, /*sameNormal=*/true));
    CHECK_TRUE(surface.CoincidentWithPlane({0, 0, 1}, 0));
    CHECK_TRUE(surface.ContainsPlaneCurve(&boundary));
    surface.ctrl[1][2].z = 0.1; // All four corners still lie on z=0.
    CHECK_FALSE(surface.IsPlanar());
    CHECK_FALSE(surface.CoincidentWith(&plane, /*sameNormal=*/true));
    CHECK_FALSE(surface.CoincidentWithPlane({0, 0, 1}, 0));
    CHECK_FALSE(surface.ContainsPlaneCurve(&boundary));
}

TEST_CASE(bilinear_patch_triangulation_keeps_curvature) {
    SurfaceTestTessellation tessellation;
    struct Geometry {
        SShell shell = {};
        SMesh mesh = {};
        ~Geometry() { mesh.Clear(); shell.Clear(); }
    } geometry;
    SSurface surface = SSurface::FromPlane({0, 0, 0}, {0, 1, 0}, {1, 0, 0});
    surface.ctrl[1][1].z = 1;
    surface.h = {1};
    Vector vertices[] = {surface.PointAt(0, 0), surface.PointAt(0, 1),
                         surface.PointAt(1, 1), surface.PointAt(1, 0)};
    for(int i = 0; i < 4; ++i) {
        SCurve curve = {};
        curve.h.v = i + 1;
        curve.isExact = true;
        curve.exact = SBezier::From(vertices[i], vertices[(i + 1) % 4]);
        curve.exact.MakePwlInto(&curve.pts);
        geometry.shell.curve.Add(&curve);
        auto trim = STrimBy::EntireCurve(&geometry.shell, curve.h, /*backwards=*/false);
        surface.trim.Add(&trim);
    }
    geometry.shell.surface.Add(&surface);
    geometry.shell.TriangulateInto(&geometry.mesh);
    CHECK_TRUE(geometry.mesh.l.n > 2);
    bool hasInterior = false;
    for(const auto &triangle : geometry.mesh.l) for(Vector point : triangle.vertices) {
        CHECK_TRUE(std::abs(point.z - point.x * point.y) < 1e-10);
        if(point.x > 0 && point.x < 1 && point.y > 0 && point.y < 1) hasInterior = true;
    }
    CHECK_TRUE(hasInterior);
}
