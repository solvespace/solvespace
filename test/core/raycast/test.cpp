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

static SSurface MakeHigherDegreePlane() {
    SSurface surface = {};
    surface.degm = surface.degn = 3;
    for(int i = 0; i <= 3; ++i) for(int j = 0; j <= 3; ++j) {
        surface.ctrl[i][j] = {i / 3.0, j / 3.0, 0};
        surface.weight[i][j] = (i == 1 && j == 2) ? 0.75 : 1;
    }
    return surface;
}

static void AddSurfaceBoundary(SShell *shell, SSurface *surface) {
    Vector vertices[] = {surface->PointAt(0, 0), surface->PointAt(0, 1),
                         surface->PointAt(1, 1), surface->PointAt(1, 0)};
    for(int i = 0; i < 4; ++i) {
        SCurve curve = {};
        curve.isExact = true;
        curve.exact = SBezier::From(vertices[i], vertices[(i + 1) % 4]);
        curve.exact.MakePwlInto(&curve.pts);
        curve.surfA = surface->h;
        hSCurve hc = shell->curve.AddAndAssignId(&curve);
        auto trim = STrimBy::EntireCurve(shell, hc, /*backwards=*/false);
        surface->trim.Add(&trim);
    }
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
    for(double weight : {1.0, 2.0}) for(int variant = 0; variant < 3; ++variant) {
        SSurface surface = SSurface::FromPlane({0, 0, 2}, {0, 1, 0}, {1, 0, 0});
        if(variant != 1) surface.weight[1][1] = 2;
        if(variant != 0) surface.ctrl[1][1] = {1, 1.25, 2};
        SCurve curve = {};
        curve.h = {1};
        curve.isExact = true;
        curve.exact = SBezier::From(Vector{0, 0.5, 2}, Vector{1, 0.5, 2});
        curve.exact.weight[1] = weight;
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
    SSurface surface = MakeHigherDegreePlane();
    SSurface plane = SSurface::FromPlane({0, 0, 0}, {0, 1, 0}, {1, 0, 0});
    SCurve boundary = {};
    boundary.isExact = true;
    boundary.exact = SBezier::From(Vector{0, 0, 0}, Vector{1, 1, 0});
    CHECK_TRUE(surface.IsPlanar());
    CHECK_FALSE(surface.IsAffine());
    CHECK_TRUE(surface.CoincidentWith(&plane, /*sameNormal=*/true));
    CHECK_TRUE(surface.CoincidentWithPlane({0, 0, 1}, 0));
    CHECK_TRUE(surface.ContainsPlaneCurve(&boundary));
    Vector point = surface.PointAt(0.37, 0.43);
    List<SInter> intersections = {};
    surface.AllPointsIntersecting(point.Plus({0, 0, -1}), point.Plus({0, 0, 1}),
        &intersections, /*asSegment=*/true, /*trimmed=*/false, /*inclTangent=*/false);
    bool hit = intersections.n == 1 && intersections[0].p.Equals(point, 1e-8);
    intersections.Clear();
    CHECK_TRUE(hit);
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

TEST_CASE(rounded_bilinear_surface_inverse) {
    SSurface original = SSurface::FromPlane({-1, 0, 0}, {0.1, 1, 0}, {1, 0, 0});
    // The rounded corners are not a bit-for-bit parallelogram. Do not loosen
    // IsAffine() just to avoid the general inverse's initial grid search.
    CHECK_FALSE(original.IsAffine());
    for(double angle : {0.0, 0.13, 0.71}) {
        Quaternion q = Quaternion::From(Vector{1, 2, 3}.WithMagnitude(1), angle);
        SSurface surface = SSurface::FromTransformationOf(&original, {2, -3, 4}, q,
                                                          2, /*includingTrims=*/false);
        CHECK_TRUE(surface.IsPlanar());
        for(bool converge : {false, true}) {
            for(double u : {0.13, 0.37, 0.81}) for(double v : {0.17, 0.43, 0.89}) {
                Vector point = surface.PointAt(u, v);
                Point2d uv;
                surface.ClosestPointTo(point, &uv, converge);
                CHECK_TRUE(uv.Equals({u, v}, 1e-10));
                CHECK_TRUE(surface.PointAt(uv).Equals(point, 1e-10));
            }
        }
        Vector point = surface.PointAt(0.37, 0.43), normal = surface.NormalAt(0.37, 0.43);
        Point2d uv;
        surface.ClosestPointTo(point.Plus(normal.WithMagnitude(2)), &uv, /*mustConverge=*/false);
        CHECK_TRUE(surface.PointAt(uv).Equals(point, 1e-8));
    }
}

TEST_CASE(plane_normal_at_collapsed_and_ill_conditioned_corner) {
    SSurface surface = SSurface::FromPlane({0, 0, 0}, {0, 1, 0}, {1, 0, 0});
    SSurface reference = surface;
    SSurface::Plane referencePlane = reference.GetPlane();
    surface.ctrl[1][0] = surface.ctrl[0][0];
    for(int reversed = 0; reversed < 2; ++reversed) {
        SSurface::Plane plane = surface.GetPlane();
        CHECK_TRUE(plane.isPlanar);
        CHECK_TRUE(plane.normal.Equals({0, 0, reversed ? -1.0 : 1.0}, 1e-12));
        CHECK_TRUE(fabs(plane.d) < 1e-12);
        CHECK_TRUE(surface.CoincidentWith(&reference, !reversed, &plane, &referencePlane));
        CHECK_FALSE(surface.CoincidentWith(&reference, reversed, &plane, &referencePlane));
        surface.Reverse();
    }
    // The first three corners are almost collinear. The fourth supplies the
    // stable plane basis; testing against the tiny first cross product would
    // incorrectly reject this net, whose departure is only roundoff.
    surface.ctrl[1][0] = {1, 0, 0};
    surface.ctrl[0][1] = {2, 1e-16, 0};
    surface.ctrl[1][1] = {0, 1, 0.1};
    CHECK_TRUE(surface.IsPlanar());
    surface.ctrl[0][1].z = 1e-8;
    CHECK_FALSE(surface.IsPlanar());
}

TEST_CASE(plane_intersection_clipping_keeps_nonaffine_maps) {
    for(int variant = 0; variant < 6; ++variant) {
        SSurface surface = SSurface::FromPlane({0, 0, 0}, {0, 1, 0}, {1, 0, 0});
        if(variant == 1 || variant == 3) surface.ctrl[1][1].y = 1.25;
        if(variant == 2 || variant == 3) surface.weight[1][1] = 2;
        if(variant == 4) surface = MakeHigherDegreePlane();
        if(variant == 5) surface = SSurface::FromPlane({-1, 2, 0}, {0.1, 1, 0}, {1, 0, 0});
        Vector point = surface.PointAt(0.37, 0.43);
        SSurface cross = SSurface::FromPlane({point.x, point.y - 2, -2}, {0, 0, 4}, {0, 4, 0});
        SSurface::Plane planeA = surface.GetPlane(), planeB = cross.GetPlane();
        for(bool reuse : {false, true}) {
            SShell empty = {}, into = {};
            surface.IntersectAgainst(&cross, &empty, &empty, &into,
                                     reuse ? &planeA : NULL, reuse ? &planeB : NULL);
            bool exact = into.curve.n == 1 && into.curve[0].isExact && into.curve[0].exact.deg == 1;
            bool contains = false;
            if(exact) {
                const SBezier &line = into.curve[0].exact;
                double t = point.Minus(line.ctrl[0]).DivProjected(line.ctrl[1].Minus(line.ctrl[0]));
                contains = t > 0 && t < 1 && line.PointAt(t).Equals(point, 1e-8);
            }
            into.Clear();
            CHECK_TRUE(exact);
            CHECK_TRUE(contains);
        }
    }
}

TEST_CASE(merge_higher_degree_plane_with_native_plane) {
    SurfaceTestTessellation tessellation;
    struct Geometry {
        SShell shell = {};
        SMesh mesh = {};
        ~Geometry() { mesh.Clear(); shell.Clear(); }
    } geometry;
    SSurface high = MakeHigherDegreePlane();
    high.h = {1};
    SSurface native = SSurface::FromPlane({1, 0, 0}, {0, 1, 0}, {1, 0, 0});
    native.h = {2};
    AddSurfaceBoundary(&geometry.shell, &high);
    AddSurfaceBoundary(&geometry.shell, &native);
    geometry.shell.surface.Add(&high);
    geometry.shell.surface.Add(&native);
    geometry.shell.MergeCoincidentSurfaces();
    CHECK_TRUE(geometry.shell.surface.n == 1);
    const SSurface &merged = geometry.shell.surface[0];
    CHECK_TRUE(merged.degm == 1 && merged.degn == 1);
    CHECK_TRUE(merged.IsPlanar());
    geometry.shell.TriangulateInto(&geometry.mesh);
    double area = 0;
    for(const STriangle &triangle : geometry.mesh.l) area += triangle.Normal().Magnitude() / 2;
    CHECK_TRUE(fabs(area - 2) < 1e-8);
}
