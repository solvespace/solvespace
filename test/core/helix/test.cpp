#include "solvespace.h"
#include "harness.h"

static SSurface Helix(double angle, double start, double finish) {
    SBezier line = SBezier::From(Vector::From(2, 0, 0), Vector::From(4, 0, 1));
    return SSurface::FromRevolutionOf(&line, Vector::From(0, 0, 0),
                                      Vector::From(0, 0, 1), 0, angle, start, finish);
}

TEST_CASE(helical_pitch_and_radius) {
    for(double angle : { PI/2, -PI/2, PI/3, PI/6, 1e-5, -1e-5 }) {
        for(double height : { 0.1, 10.0, -10.0 }) {
            SSurface s = Helix(angle, 3, 3 + height);
            CHECK_TRUE(s.degn == 3);
            for(double u : { 0.0, 0.3, 1.0 }) {
                for(int j = 0; j <= 100; j++) {
                    Vector p = s.PointAt(u, j/100.0);
                    double fraction = atan2(p.y, p.x)/angle;
                    CHECK_TRUE(fabs(p.z - (u + 3 + height*fraction)) <
                               fabs(height)*0.0005 + 1e-12);
                    CHECK_TRUE(fabs(sqrt(p.x*p.x + p.y*p.y) - (2 + 2*u)) < 1e-12);
                }
            }
        }
    }
}

TEST_CASE(helical_endpoints_and_tangents) {
    for(double angle : { PI/2, -PI/2, PI/6, 1e-5 }) {
        for(double height : { 10.0, -10.0 }) {
            SSurface a = Helix(angle, 0, height);
            SBezier line = SBezier::From(Vector::From(2, 0, 0), Vector::From(4, 0, 1));
            SSurface b = SSurface::FromRevolutionOf(&line, Vector::From(0, 0, 0),
                                Vector::From(0, 0, 1), angle, 2*angle, height, 2*height);
            for(double u : { 0.0, 0.4, 1.0 }) {
                CHECK_TRUE(a.PointAt(u, 1).Minus(b.PointAt(u, 0)).Magnitude() < 1e-12);
                Vector au, av, bu, bv;
                a.TangentsAt(u, 1, &au, &av);
                b.TangentsAt(u, 0, &bu, &bv);
                CHECK_TRUE(av.Minus(bv).Magnitude() < 1e-11);
                Vector p = a.PointAt(u, 0);
                a.TangentsAt(u, 0, &au, &av);
                Vector expected = Vector::From(0, p.x*angle, height);
                CHECK_TRUE(av.WithMagnitude(1).Minus(expected.WithMagnitude(1)).Magnitude()
                           < 1e-11);
            }
        }
    }
}

TEST_CASE(revolution_remains_quadratic) {
    SSurface s = Helix(PI/2, 3, 3);
    CHECK_TRUE(s.degn == 2);
    for(double t : { 0.0, 0.2, 0.5, 1.0 }) {
        Vector p = s.PointAt(0.4, t);
        CHECK_TRUE(fabs(p.z - 3.4) < 1e-12);
        CHECK_TRUE(fabs(sqrt(p.x*p.x + p.y*p.y) - 2.8) < 1e-12);
    }
}

TEST_CASE(zero_angle_translation) {
    SSurface s = Helix(0, 3, 9);
    for(double t : { 0.0, 0.2, 0.5, 1.0 }) {
        CHECK_TRUE(s.PointAt(0.4, t).Minus(Vector::From(2.8, 0, 3.4 + 6*t)).Magnitude()
                   < 1e-12);
    }
}

TEST_CASE(rational_profile_and_arbitrary_axis) {
    SBezier profile = {};
    profile.deg = 2;
    profile.ctrl[0] = Vector::From(0, 0, 0); // also exercise a point on the axis
    profile.ctrl[1] = Vector::From(3, 0, 2);
    profile.ctrl[2] = Vector::From(2, 0, 4);
    profile.weight[0] = 2;
    profile.weight[1] = 0.7;
    profile.weight[2] = 3;
    Quaternion q = Quaternion::From(Vector::From(1, 2, 3).WithMagnitude(1), 0.7);
    Vector origin = Vector::From(7, -3, 11);
    Vector axis = q.Rotate(Vector::From(0, 0, 1));
    SBezier transformed = profile.TransformedBy(origin, q, 1);
    for(double angle : { PI/2, -PI/2 }) {
        SSurface reference = SSurface::FromRevolutionOf(&profile, Vector::From(0, 0, 0),
                                    Vector::From(0, 0, 1), 0.6, 0.6 + angle, 3, -7);
        SSurface rotated = SSurface::FromRevolutionOf(&transformed, origin, axis,
                                                      0.6, 0.6 + angle, 3, -7);
        for(double u : { 0.0, 0.2, 0.6, 1.0 }) {
            for(double v : { 0.0, 0.2, 0.5, 0.8, 1.0 }) {
                Vector p = reference.PointAt(u, v);
                CHECK_TRUE(rotated.PointAt(u, v).Minus(q.Rotate(p).Plus(origin)).Magnitude()
                           < 1e-11);
                Vector generator = profile.PointAt(u);
                CHECK_TRUE(fabs(sqrt(p.x*p.x + p.y*p.y) - generator.x) < 1e-12);
                if(u != 0) {
                    double fraction = (atan2(p.y, p.x) - 0.6)/angle;
                    CHECK_TRUE(fabs(p.z - (generator.z + 3 - 10*fraction)) < 0.005);
                }
            }
        }
    }
}

// Exercise shell construction too: its longitudinal trim curves must have
// the same degree and weights as the new surface boundaries.
TEST_CASE(helical_shell_trims_and_mesh) {
    // Normally calculated from the sketch bounding box during regeneration.
    SS.chordTolCalculated = 0.01;
    SBezierLoop loop = {};
    Vector corners[] = { Vector::From(2, 0, 0), Vector::From(4, 0, 0),
                         Vector::From(4, 0, 1), Vector::From(2, 0, 1) };
    for(int i = 0; i < 4; i++) {
        SBezier line = SBezier::From(corners[i], corners[(i + 1) % 4]);
        loop.l.Add(&line);
    }
    SBezierLoopSet loops = {};
    // Shell builders expect outer contours clockwise about the sketch normal.
    loop.Reverse();
    loops.l.Add(&loop);
    loops.normal = Vector::From(0, -1, 0);
    Group group = {};
    group.h.v = 42;
    SShell shell = {};
    shell.MakeFromHelicalRevolutionOf(&loops, Vector::From(0, 0, 0),
        Vector::From(0, 0, 1), RGBi(100, 100, 100), &SK, &group, 0, PI, 0, 4);
    bool boundariesMatch = true;
    int cubics = 0;
    for(SCurve &curve : shell.curve) {
        if(curve.exact.deg != 3) continue;
        cubics++;
        SSurface *a = shell.surface.FindById(curve.surfA);
        SSurface *b = shell.surface.FindById(curve.surfB);
        for(double t : { 0.0, 0.25, 0.7, 1.0 }) {
            Vector p = curve.exact.PointAt(t);
            boundariesMatch &= p.Minus(a->PointAt(0, t)).Magnitude() < 1e-12;
            boundariesMatch &= p.Minus(b->PointAt(1, t)).Magnitude() < 1e-12;
        }
    }
    SMesh mesh = {};
    shell.TriangulateInto(&mesh);
    bool hasTriangles = !mesh.l.IsEmpty();
    StepFileWriter writer = {};
    writer.f = tmpfile();
    bool exported = writer.f != NULL;
    if(writer.f) {
        writer.id = 200;
        writer.WriteHeader();
        for(SSurface &surface : shell.surface) {
            SBezierList edges = {};
            surface.MakeSectionEdgesInto(&shell, NULL, &edges);
            writer.ExportSurface(&surface, &edges);
            edges.Clear();
        }
        writer.WriteFooter();
        exported = !ferror(writer.f) && ftell(writer.f) > 0;
        rewind(writer.f);
        std::string step;
        char buffer[4096];
        size_t count;
        while((count = fread(buffer, 1, sizeof(buffer), writer.f)) != 0) {
            step.append(buffer, count);
        }
        exported &= step.find("B_SPLINE_SURFACE(1,3,") != std::string::npos;
        exported &= step.find("B_SPLINE_SURFACE_WITH_KNOTS((2,2),(4,4),") !=
                    std::string::npos;
        fclose(writer.f);
        writer.curves.Clear();
        writer.advancedFaces.Clear();
    }
    SEdgeList edges = {};
    bool intersects, leaks;
    SKdNode::From(&mesh)->MakeCertainEdgesInto(&edges, EdgeKind::NAKED_OR_SELF_INTER,
        /*coplanarIsInter=*/true, &intersects, &leaks);
    bool originalWatertight = !leaks && !intersects;
    edges.Clear();
    mesh.Clear();
    // Cut across the helical faces, rather than only combining disjoint shells.
    SBezierLoop toolLoop = {};
    Vector toolCorners[] = { Vector::From(-5, -1, 0), Vector::From(0.5, -1, 0),
                             Vector::From(0.5, 5, 0), Vector::From(-5, 5, 0) };
    for(int i = 0; i < 4; i++) {
        SBezier line = SBezier::From(toolCorners[i], toolCorners[(i + 1) % 4]);
        toolLoop.l.Add(&line);
    }
    SBezierLoopSet toolLoops = {};
    toolLoop.Reverse();
    toolLoops.l.Add(&toolLoop);
    toolLoops.normal = Vector::From(0, 0, 1);
    SShell tool = {}, result = {};
    tool.MakeFromExtrusionOf(&toolLoops, Vector::From(0, 0, -1),
                            Vector::From(0, 0, 6), RGBi(100, 100, 100));
    result.MakeFromDifferenceOf(&shell, &tool);
    bool booleanOk = !result.booleanFailed && !result.surface.IsEmpty();
    result.TriangulateInto(&mesh);
    double volume = mesh.CalculateVolume();
    SKdNode::From(&mesh)->MakeCertainEdgesInto(&edges, EdgeKind::NAKED_OR_SELF_INTER,
        /*coplanarIsInter=*/true, &intersects, &leaks);
    bool cutWatertight = !leaks && !intersects && !mesh.l.IsEmpty();
    edges.Clear();
    mesh.Clear();
    result.Clear();
    tool.Clear();
    toolLoops.Clear();
    shell.Clear();
    loops.Clear();
    CHECK_TRUE(cubics == 12);
    CHECK_TRUE(boundariesMatch);
    CHECK_TRUE(hasTriangles);
    CHECK_TRUE(exported);
    CHECK_TRUE(originalWatertight);
    CHECK_TRUE(booleanOk);
    CHECK_TRUE(cutWatertight);
    CHECK_TRUE(volume > 0 && volume < 6*PI);
}
