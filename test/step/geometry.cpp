// Native geometry tests are deliberately separate from the dependency-only
// reader suite. No STEPcode headers or definitions are needed here either.
#include "solvespace.h"
#include "stepreader.h"
#include "stepgeometry.h"
#include "stepnurbs.h"

#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

#if defined(SC_STATIC) || defined(SC_SCHEMA_EXPORT) || defined(REAL_TYPE)
#error "STEPcode definitions leaked into the native geometry client"
#endif

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

using namespace SolveSpace;
using Status = Step::GeometryResult::Status;

namespace {
int cases = 0, checks = 0, failures = 0;
struct Failure {};

void Check(bool okay, const char *expression, int line) {
    ++checks;
    if(!okay) {
        std::cerr << "  line " << line << ": " << expression << "\n";
        throw Failure();
    }
}
#define REQUIRE(expr) Check(!!(expr), #expr, __LINE__)

void Run(const std::string &name, const std::function<void()> &test) {
    const char *filter = std::getenv("SLVS_STEP_GEOMETRY_TEST_FILTER");
    if(filter && name.find(filter) == std::string::npos) return;
    ++cases;
    std::cout << name << ": " << std::flush;
    try {
        test();
        std::cout << "OK\n";
    } catch(const Failure &) {
        ++failures;
        std::cout << "FAIL\n";
    } catch(const std::exception &error) {
        ++failures;
        std::cout << "FAIL: " << error.what() << "\n";
    } catch(const Step::Nurbs::Error &error) {
        ++failures;
        std::cout << "FAIL: entity #" << error.entity << ": " << error.message << "\n";
    }
    Platform::FreeAllTemporary();
}

#if defined(_MSC_VER) && defined(_DEBUG)
int CrtFailure(int type, char *message, int *) {
    if(type != _CRT_ASSERT && type != _CRT_ERROR) return 0;
    std::fputs(message, stderr);
    std::_Exit(3);
}
#endif

std::string Ref(int id) { return "#" + std::to_string(id); }

std::string Real(double number) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::uppercase << std::scientific << std::setprecision(17) << number;
    return out.str();
}

std::string Triple(Vector v) {
    return "(" + Real(v.x) + "," + Real(v.y) + "," + Real(v.z) + ")";
}

std::string Refs(const std::vector<int> &ids) {
    std::string text = "(";
    for(int id : ids) {
        if(text.size() > 1) text += ",";
        text += Ref(id);
    }
    return text + ")";
}

std::string Record(int id, const std::string &body) { return Ref(id) + "=" + body + ";\n"; }

std::string ReplaceRecord(std::string text, int id, const std::string &body) {
    size_t start = text.find(Ref(id) + "=");
    if(start == std::string::npos) throw std::runtime_error("Test record not found " + Ref(id));
    size_t finish = text.find(';', start);
    text.replace(start, finish - start + 1, Ref(id) + "=" + body + ";");
    return text;
}

std::string Exchange(const std::string &schema, const std::string &data) {
    return "ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('native geometry proof'),'2;1');\n"
           "FILE_NAME('','',(''),(''),'','','');\nFILE_SCHEMA(('" + schema +
           "'));\nENDSEC;\nDATA;\n" + data + "ENDSEC;\nEND-ISO-10303-21;\n";
}

std::unique_ptr<Step::Document> Read(const std::string &text) {
    std::istringstream input(text);
    auto result = Step::Read(input);
    if(!result.document) std::cerr << result.error << "\n" << result.readerLog;
    REQUIRE(result.document && result.status == Step::ReadResult::Status::Okay);
    return std::move(result.document);
}

void PrintDiagnostics(const Step::GeometryResult &result) {
    for(const auto &diagnostic : result.diagnostics) {
        std::cerr << "solid " << Ref(diagnostic.solid) << ", entity " << Ref(diagnostic.entity)
                  << ": " << diagnostic.message << "\n";
    }
}

Step::GeometryResult Convert(const std::string &schema, const std::string &data) {
    auto document = Read(Exchange(schema, data));
    return Step::ConvertGeometry(*document); // output must outlive the reader
}

// Independent geometry fixtures, not the reader's intentionally invalid cavity
// reference fixture. Face lists are CCW as viewed from outside. Topological
// edge directions are deliberately canonical, not copied from face winding.
std::string Polyhedron(const std::vector<Vector> &points,
                       const std::vector<std::vector<int>> &faces, int offset = 0,
                       bool splines = false) {
    std::string data;
    std::map<std::pair<int, int>, int> edges;
    for(const auto &face : faces) {
        for(size_t i = 0; i < face.size(); ++i) {
            int a = face[i], b = face[(i + 1) % face.size()];
            edges.emplace(std::minmax(a, b), 0);
        }
    }
    int e = 0;
    for(auto &entry : edges) entry.second = e++;
    for(size_t i = 0; i < points.size(); ++i) {
        data += Record(offset + 10 + (int)i, "CARTESIAN_POINT(''," + Triple(points[i]) + ")");
        data += Record(offset + 30 + (int)i, "VERTEX_POINT(''," + Ref(offset + 10 + (int)i) + ")");
    }
    for(const auto &entry : edges) {
        int a = entry.first.first, b = entry.first.second, id = offset + entry.second;
        data += Record(id + 100, "DIRECTION(''," + Triple(points[b].Minus(points[a])) + ")");
        data += Record(id + 200, "VECTOR(''," + Ref(id + 100) + ",1.)");
        if(splines) {
            data += Record(id + 300, "B_SPLINE_CURVE_WITH_KNOTS('',1,(" + Ref(offset + 10 + a) +
                "," + Ref(offset + 10 + b) + "),.UNSPECIFIED.,.F.,.F.,(2,2),(0.,1.),"
                ".PIECEWISE_BEZIER_KNOTS.)");
        } else {
            data += Record(id + 300, "LINE(''," + Ref(offset + 10 + a) + "," + Ref(id + 200) + ")");
        }
        data += Record(id + 400, "EDGE_CURVE(''," + Ref(offset + 30 + a) + "," +
                       Ref(offset + 30 + b) + "," + Ref(id + 300) + ",.T.)");
    }
    std::vector<int> faceIds;
    for(size_t f = 0; f < faces.size(); ++f) {
        const auto &face = faces[f];
        int id = offset + (int)f;
        std::vector<int> uses;
        for(size_t i = 0; i < face.size(); ++i) {
            int a = face[i], b = face[(i + 1) % face.size()];
            int use = offset + 1000 + 32 * (int)f + (int)i;
            uses.push_back(use);
            data += Record(use, "ORIENTED_EDGE('',*,*," +
                Ref(offset + 400 + edges.at(std::minmax(a, b))) + (a < b ? ",.T.)" : ",.F.)"));
        }
        Vector x = points[face[1]].Minus(points[face[0]]);
        Vector normal = x.Cross(points[face[2]].Minus(points[face[0]]));
        data += Record(id + 2000, "EDGE_LOOP(''," + Refs(uses) + ")");
        data += Record(id + 2100, "FACE_OUTER_BOUND(''," + Ref(id + 2000) + ",.T.)");
        data += Record(id + 2200, "DIRECTION(''," + Triple(normal) + ")");
        data += Record(id + 2300, "DIRECTION(''," + Triple(x) + ")");
        data += Record(id + 2400, "AXIS2_PLACEMENT_3D(''," + Ref(offset + 10 + face[0]) +
                       "," + Ref(id + 2200) + "," + Ref(id + 2300) + ")");
        if(splines) {
            Vector p00 = points[face[0]], p10 = points[face[1]], p01 = points[face.back()];
            Vector controls[] = {p00, p01, p10, p10.Plus(p01.Minus(p00))};
            int base = offset + 3000 + 4 * (int)f;
            for(int i = 0; i < 4; ++i) {
                data += Record(base + i, "CARTESIAN_POINT(''," + Triple(controls[i]) + ")");
            }
            data += Record(id + 2500, "B_SPLINE_SURFACE_WITH_KNOTS('',1,1,((" +
                Ref(base) + "," + Ref(base + 1) + "),(" + Ref(base + 2) + "," + Ref(base + 3) +
                ")),.UNSPECIFIED.,.F.,.F.,.F.,(2,2),(2,2),(0.,1.),(0.,1.),.PIECEWISE_BEZIER_KNOTS.)");
        } else {
            data += Record(id + 2500, "PLANE(''," + Ref(id + 2400) + ")");
        }
        data += Record(id + 2600, "ADVANCED_FACE('',(" + Ref(id + 2100) + ")," +
                       Ref(id + 2500) + ",.T.)");
        faceIds.push_back(id + 2600);
    }
    data += Record(offset + 2700, "CLOSED_SHELL(''," + Refs(faceIds) + ")");
    data += Record(offset + 2701, "MANIFOLD_SOLID_BREP('polyhedron'," + Ref(offset + 2700) + ")");
    data += Record(offset + 2800, "(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.MILLI.,.METRE.))");
    data += Record(offset + 2801, "(NAMED_UNIT(*)PLANE_ANGLE_UNIT()SI_UNIT($,.RADIAN.))");
    data += Record(offset + 2802, "(NAMED_UNIT(*)SI_UNIT($,.STERADIAN.)SOLID_ANGLE_UNIT())");
    data += Record(offset + 2810, "(GEOMETRIC_REPRESENTATION_CONTEXT(3)GLOBAL_UNIT_ASSIGNED_CONTEXT(" +
                   Refs({offset + 2800, offset + 2801, offset + 2802}) + ")REPRESENTATION_CONTEXT('3D',''))");
    data += Record(offset + 2820, "ADVANCED_BREP_SHAPE_REPRESENTATION('',(" + Ref(offset + 2701) +
                   ")," + Ref(offset + 2810) + ")");
    return data;
}

std::string Cube(double size = 10, Vector translation = {}, int offset = 0, bool splines = false) {
    std::vector<Vector> points = {{0, 0, 0}, {size, 0, 0}, {size, size, 0}, {0, size, 0},
        {0, 0, size}, {size, 0, size}, {size, size, size}, {0, size, size}};
    for(auto &point : points) point = point.Plus(translation);
    return Polyhedron(points, {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
                               {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}}, offset, splines);
}

std::string Tetrahedron(bool splines = false) {
    return Polyhedron({{0, 0, 0}, {10, 0, 0}, {0, 10, 0}, {0, 0, 10}},
                      {{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}}, 0, splines);
}

std::string ConcavePrism() {
    return Polyhedron({{0, 0, 0}, {10, 0, 0}, {10, 4, 0}, {4, 4, 0}, {4, 10, 0}, {0, 10, 0},
        {0, 0, 10}, {10, 0, 10}, {10, 4, 10}, {4, 4, 10}, {4, 10, 10}, {0, 10, 10}},
        {{0, 5, 4, 3, 2, 1}, {6, 7, 8, 9, 10, 11}, {0, 1, 7, 6}, {1, 2, 8, 7},
         {2, 3, 9, 8}, {3, 4, 10, 9}, {4, 5, 11, 10}, {5, 0, 6, 11}});
}

std::string ConcaveSolidWithConvexFaces() {
    return Polyhedron({{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0},
        {0, 0, 10}, {10, 0, 10}, {10, 10, 10}, {0, 10, 10}, {5, 5, 5}},
        {{0, 3, 2, 1}, {4, 5, 8}, {5, 6, 8}, {6, 7, 8}, {7, 4, 8},
         {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}});
}

struct MeshOwner {
    SMesh mesh = {};
    ~MeshOwner() { mesh.Clear(); }
};

struct ShellOwner {
    SShell shell = {};
    ~ShellOwner() { shell.Clear(); }
};

struct TessellationSettings {
    double chord = SS.chordTolCalculated, exportChord = SS.exportChordTol;
    double exportScale = SS.exportScale;
    int segments = SS.maxSegments, exportSegments = SS.exportMaxSegments;
    bool exporting = SS.exportMode;
    ~TessellationSettings() {
        SS.chordTolCalculated = chord; SS.maxSegments = segments; SS.exportMode = exporting;
        SS.exportChordTol = exportChord; SS.exportScale = exportScale;
        SS.exportMaxSegments = exportSegments;
    }
};

using PointKey = std::tuple<long long, long long, long long>;
PointKey Key(Vector point) {
    return std::make_tuple(std::llround(point.x / LENGTH_EPS),
                           std::llround(point.y / LENGTH_EPS),
                           std::llround(point.z / LENGTH_EPS));
}

void CheckNative(Step::NativeSolid &solid, int faceCount, int edgeCount, int vertexCount,
                 double volume, double area) {
    REQUIRE(solid.shell.surface.n == faceCount);
    REQUIRE(solid.shell.curve.n == edgeCount);
    REQUIRE(solid.vertices.size() == (size_t)vertexCount);
    REQUIRE(solid.faces.size() == (size_t)faceCount);
    REQUIRE(solid.edges.size() == (size_t)edgeCount);
    REQUIRE(!solid.shell.booleanFailed);
    std::map<uint32_t, std::vector<bool>> uses;
    MeshOwner mesh;
    for(const auto &face : solid.faces) {
        auto surface = solid.shell.surface.FindById(face.surface);
        REQUIRE(surface->h.v == (uint32_t)face.face);
        REQUIRE(surface->face == 0); // no fabricated sketch reference handles
        REQUIRE(surface->degm == 1 && surface->degn == 1);
        REQUIRE(surface->NormalAt(0.5, 0.5).WithMagnitude(1).Dot(face.normal) > 1 - 1e-12);
        Vector center = {};
        for(const auto &trim : surface->trim) {
            uses[trim.curve.v].push_back(trim.backwards);
            auto curve = solid.shell.curve.FindById(trim.curve);
            REQUIRE(curve->isExact && curve->exact.deg == 1 && curve->pts.n == 2);
            REQUIRE(curve->exact.entity == 0);
            REQUIRE(curve->surfA.v == surface->h.v || curve->surfB.v == surface->h.v);
            REQUIRE(curve->exact.Start().Equals(curve->pts[0].p));
            REQUIRE(trim.start.Equals(trim.backwards ? curve->exact.Finish() : curve->exact.Start()));
            Vector midpoint = trim.start.Plus(trim.finish).ScaledBy(0.5);
            Point2d uv;
            surface->ClosestPointTo(midpoint, &uv);
            REQUIRE(surface->PointAt(uv).Equals(midpoint));
            center = center.Plus(trim.start);
        }
        center = center.ScaledBy(1.0 / surface->trim.n);
        // Triangulation normalizes polygon directions, so it alone would not
        // detect reversed native trims. Also test the actual native UV BSP.
        SEdgeList uv = {};
        surface->MakeEdgesInto(&solid.shell, &uv, SSurface::MakeAs::UV);
        SBspUv *bsp = SBspUv::From(&uv, surface);
        Point2d inside;
        surface->ClosestPointTo(center, &inside);
        REQUIRE(surface->PointAt(inside).Equals(center));
        REQUIRE(bsp != nullptr);
        REQUIRE(bsp->ClassifyPoint(inside, {0.123, 0.456}, surface) == SBspUv::Class::INSIDE);
        REQUIRE(bsp->ClassifyPoint({-2, -2}, {0.123, 0.456}, surface) == SBspUv::Class::OUTSIDE);
        uv.Clear();
        int start = mesh.mesh.l.n;
        surface->TriangulateInto(&solid.shell, &mesh.mesh);
        REQUIRE(mesh.mesh.l.n > start);
        for(int i = start; i < mesh.mesh.l.n; ++i) {
            const auto &triangle = mesh.mesh.l[i];
            REQUIRE(triangle.Normal().Dot(face.normal) > 0);
            REQUIRE(!triangle.IsDegenerate());
        }
    }
    for(const auto &entry : uses) {
        REQUIRE(entry.second.size() == 2);
        REQUIRE(entry.second[0] != entry.second[1]);
    }
    double meshVolume = 0, meshArea = 0;
    // Center the volume integral for translation-independent numerical checks.
    Vector anchor = solid.vertices.front().position;
    std::map<std::pair<PointKey, PointKey>, std::pair<int, int>> incidence;
    for(const auto &triangle : mesh.mesh.l) {
        meshVolume += triangle.a.Minus(anchor).Dot(triangle.b.Minus(anchor).Cross(
                      triangle.c.Minus(anchor))) / 6.0;
        meshArea += triangle.Area();
        for(int i = 0; i < 3; ++i) {
            auto a = Key(triangle.vertices[i]), b = Key(triangle.vertices[(i + 1) % 3]);
            bool forward = a < b;
            auto &edge = incidence[forward ? std::make_pair(a, b) : std::make_pair(b, a)];
            ++edge.first;
            edge.second += forward ? 1 : -1;
        }
    }
    REQUIRE(std::abs(meshVolume - volume) < 1e-8 * std::max(1.0, volume));
    REQUIRE(std::abs(meshArea - area) < 1e-8 * std::max(1.0, area));
    for(const auto &entry : incidence) {
        REQUIRE(entry.second.first == 2);
        REQUIRE(entry.second.second == 0);
    }
}

void CheckUnitCube(Step::NativeSolid &solid) {
    CheckNative(solid, 6, 12, 8, 1, 6);
    Vector max = {VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE};
    Vector min = {VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE};
    for(const auto &vertex : solid.vertices) vertex.position.MakeMaxMin(&max, &min);
    REQUIRE(min.Equals({0, 0, 0}));
    REQUIRE(max.Equals({1, 1, 1}));
}

Step::GeometryResult Accepted(const std::string &schema, const std::string &data) {
    auto result = Convert(schema, data);
    if(result.status != Status::Okay) PrintDiagnostics(result);
    REQUIRE(result.status == Status::Okay);
    REQUIRE(result.diagnostics.empty());
    REQUIRE(result.solids.size() == 1);
    return result;
}

void Rejected(const std::string &schema, const std::string &data) {
    auto result = Convert(schema, data);
    REQUIRE(result.status == Status::Rejected);
    REQUIRE(result.solids.empty());
    REQUIRE(!result.diagnostics.empty());
    REQUIRE(!result.diagnostics.front().message.empty());
}

std::string ContextWithUnits(const std::vector<int> &units) {
    return "(GEOMETRIC_REPRESENTATION_CONTEXT(3)GLOBAL_UNIT_ASSIGNED_CONTEXT(" +
           Refs(units) + ")REPRESENTATION_CONTEXT('3D',''))";
}

std::string InchUnits(std::string data, const std::string &protocol) {
    data += Record(2830, "DIMENSIONAL_EXPONENTS(1.,0.,0.,0.,0.,0.,0.)");
    data += Record(2831, "LENGTH_MEASURE_WITH_UNIT(LENGTH_MEASURE(25.4),#2800)");
    data += Record(2832, "(CONVERSION_BASED_UNIT('INCH',#2831)LENGTH_UNIT()NAMED_UNIT(" +
                   std::string(protocol == "AP214" ? "*" : "#2830") + "))");
    return ReplaceRecord(data, 2810, ContextWithUnits({2832, 2801, 2802}));
}

std::string Numbers(const std::vector<double> &values) {
    std::string text = "(";
    for(double value : values) { if(text.size() > 1) text += ","; text += Real(value); }
    return text + ")";
}

std::string Integers(const std::vector<int> &values) {
    std::string text = "(";
    for(int value : values) { if(text.size() > 1) text += ","; text += std::to_string(value); }
    return text + ")";
}

std::string SplineCurve(std::string data, int geometry, int base, int degree,
                         const std::vector<Vector> &controls, const std::vector<double> &weights,
                         const std::vector<int> &multiplicities, const std::vector<double> &knots) {
    std::vector<int> ids;
    for(size_t i = 0; i < controls.size(); ++i) {
        ids.push_back(base + (int)i);
        data += Record(ids.back(), "CARTESIAN_POINT(''," + Triple(controls[i]) + ")");
    }
    std::string spline = std::to_string(degree) + "," + Refs(ids) + ",.UNSPECIFIED.,.F.,.F.";
    std::string intervals = Integers(multiplicities) + "," + Numbers(knots) + ",.UNSPECIFIED.";
    std::string body = weights.empty() ? "B_SPLINE_CURVE_WITH_KNOTS(''," + spline + "," + intervals + ")" :
        "(BOUNDED_CURVE()B_SPLINE_CURVE(" + spline + ")B_SPLINE_CURVE_WITH_KNOTS(" + intervals +
        ")CURVE()GEOMETRIC_REPRESENTATION_ITEM()RATIONAL_B_SPLINE_CURVE(" + Numbers(weights) + ")REPRESENTATION_ITEM(''))";
    return ReplaceRecord(data, geometry, body);
}

std::string SplineSurface(std::string data, int geometry, int base, int udegree, int vdegree,
                           const std::vector<std::vector<Vector>> &controls,
                           const std::vector<std::vector<double>> &weights,
                           const std::vector<int> &umult, const std::vector<int> &vmult,
                           const std::vector<double> &uknots, const std::vector<double> &vknots) {
    std::string net = "(", wnet = "(";
    for(size_t i = 0; i < controls.size(); ++i) {
        std::vector<int> ids;
        for(Vector point : controls[i]) {
            ids.push_back(base++);
            data += Record(ids.back(), "CARTESIAN_POINT(''," + Triple(point) + ")");
        }
        if(i != 0) { net += ","; wnet += ","; }
        net += Refs(ids);
        if(!weights.empty()) wnet += Numbers(weights[i]);
    }
    net += ")"; wnet += ")";
    std::string spline = std::to_string(udegree) + "," + std::to_string(vdegree) + "," + net +
                         ",.UNSPECIFIED.,.F.,.F.,.F.";
    std::string intervals = Integers(umult) + "," + Integers(vmult) + "," + Numbers(uknots) +
                            "," + Numbers(vknots) + ",.UNSPECIFIED.";
    std::string body = weights.empty() ? "B_SPLINE_SURFACE_WITH_KNOTS(''," + spline + "," + intervals + ")" :
        "(BOUNDED_SURFACE()B_SPLINE_SURFACE(" + spline + ")B_SPLINE_SURFACE_WITH_KNOTS(" + intervals +
        ")GEOMETRIC_REPRESENTATION_ITEM()RATIONAL_B_SPLINE_SURFACE(" + wnet + ")REPRESENTATION_ITEM('')SURFACE())";
    return ReplaceRecord(data, geometry, body);
}

// Compound-curved top, with unchanged straight boundary and five plane faces.
// Neither rational net is a cylinder or a surface of extrusion.
std::string FreeformBox(bool rational = false, bool multispan = false, double bump = 0.4) {
    std::vector<double> xs = multispan ? std::vector<double>{0, 1.0 / 6, 0.5, 5.0 / 6, 1} :
                                       std::vector<double>{0, 1.0 / 3, 2.0 / 3, 1};
    std::vector<std::vector<Vector>> controls;
    std::vector<std::vector<double>> weights;
    for(size_t i = 0; i < xs.size(); ++i) {
        std::vector<Vector> row;
        std::vector<double> wrow;
        for(int j = 0; j < 4; ++j) {
            bool interior = i != 0 && i + 1 != xs.size() && j != 0 && j != 3;
            row.push_back({xs[i], j / 3.0, 1 + (interior ? bump : 0)});
            wrow.push_back(interior ? 0.8 + 0.1 * ((i + j) % 5) : 1);
        }
        controls.push_back(row); weights.push_back(wrow);
    }
    return SplineSurface(Cube(1), 2501, 5000, 3, 3, controls, rational ? weights :
        std::vector<std::vector<double>>{}, multispan ? std::vector<int>{4, 1, 4} : std::vector<int>{4, 4},
        {4, 4}, multispan ? std::vector<double>{-2, 3, 8} : std::vector<double>{-2, 8}, {2, 7});
}

std::string SplitPlaneBox() {
    std::vector<std::vector<Vector>> controls;
    for(double x : {-1.0, 0.5, 2.0}) {
        std::vector<Vector> row;
        for(double y : {-1.0, 0.5, 2.0}) row.push_back({x, y, 1});
        controls.push_back(row);
    }
    return SplineSurface(Cube(1), 2501, 5000, 1, 1, controls, {}, {2, 1, 2}, {2, 1, 2}, {0, 0.5, 1}, {0, 0.5, 1});
}

std::string BilinearBox() {
    std::vector<Vector> points = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                 {0, 0, 1}, {1, 0, 1}, {1, 1, 1.4}, {0, 1, 1}};
    auto data = Polyhedron(points, {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
                                    {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}});
    return SplineSurface(data, 2501, 5000, 1, 1,
                         {{{0, 0, 1}, {0, 1, 1}}, {{1, 0, 1}, {1, 1, 1.4}}}, {},
                         {2, 2}, {2, 2}, {0, 1}, {0, 1});
}

std::vector<SBezier> FreeformOutline() {
    std::vector<SBezier> curves = {
        SBezier::From(Vector{1, 0, 0}, Vector{1, 1, 0}, Vector{0, 1, 0}),
        SBezier::From(Vector{0, 1, 0}, Vector{-0.3, 1.4, 0}, Vector{-1.3, 0.3, 0}, Vector{-1, 0, 0}),
        SBezier::From(Vector{-1, 0, 0}, Vector{-1, -1, 0}, Vector{0, -1, 0}),
        SBezier::From(Vector{0, -1, 0}, Vector{0.2, -1.2, 0}, Vector{1.3, -0.2, 0}, Vector{1, 0, 0})
    };
    curves[0].weight[1] = curves[2].weight[1] = 0.6; // deliberately not circles
    curves[1].weight[1] = 0.9; curves[1].weight[2] = 1.2;
    curves[3].weight[1] = 1.4; curves[3].weight[2] = 0.8;
    return curves;
}

double OutlineArea(const std::vector<SBezier> &outline) {
    double integral = 0;
    for(const auto &curve : outline) for(int i = 0; i <= 1000; ++i) {
        double t = i / 1000.0;
        Vector p = curve.PointAt(t), d = curve.TangentAt(t);
        integral += (i == 0 || i == 1000 ? 1 : (i % 2 ? 4 : 2)) * (p.x * d.y - p.y * d.x) / 6000;
    }
    return integral;
}

std::string BezierLoft(const std::vector<SBezier> &outline, double height = 2,
                        double topScale = 0.8, bool splitCap = false) {
    std::vector<Vector> points;
    for(const auto &curve : outline) points.push_back(curve.Start());
    Vector offset = {0.2, -0.1, height};
    for(const auto &curve : outline) points.push_back(curve.Start().ScaledBy(topScale).Plus(offset));
    auto data = Polyhedron(points, {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4},
                                    {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}});
    const int bottom[] = {0, 3, 5, 1}, top[] = {8, 10, 11, 9};
    for(int k = 0; k < 4; ++k) {
        const auto &curve = outline[k];
        for(bool upper : {false, true}) {
            SBezier edge = upper ? curve.TransformedBy(offset, Quaternion::IDENTITY, topScale) : curve;
            if(k == 3) edge.Reverse(); // canonical source edge order in Polyhedron
            std::vector<Vector> controls;
            std::vector<double> weights;
            for(int i = 0; i <= edge.deg; ++i) { controls.push_back(edge.ctrl[i]); weights.push_back(edge.weight[i]); }
            data = SplineCurve(data, 300 + (upper ? top[k] : bottom[k]), 8000 + 40 * k + (upper ? 10 : 0),
                               edge.deg, controls, weights, {edge.deg + 1, edge.deg + 1}, {-3, 7});
        }
        std::vector<std::vector<Vector>> controls;
        std::vector<std::vector<double>> weights;
        for(int i = 0; i <= curve.deg; ++i) {
            controls.push_back({curve.ctrl[i], curve.ctrl[i].ScaledBy(topScale).Plus(offset)});
            weights.push_back({curve.weight[i], curve.weight[i]});
        }
        data = SplineSurface(data, 2502 + k, 5000 + 40 * k, curve.deg, 1, controls, weights,
                              {curve.deg + 1, curve.deg + 1}, {2, 2}, {-2, 8}, {3, 9});
    }
    if(splitCap) {
        std::vector<std::vector<Vector>> controls;
        for(double y : {-1.5, 0.2, 1.5}) {
            std::vector<Vector> row;
            for(double x : {-1.5, 0.3, 1.5}) row.push_back({x, y, 0});
            controls.push_back(row);
        }
        data = SplineSurface(data, 2500, 9000, 1, 1, controls, {}, {2, 1, 2}, {2, 1, 2},
                              {0, 0.5, 1}, {0, 0.5, 1});
    }
    return data;
}

std::string ThroughHole() {
    auto data = Cube(4) + Polyhedron({{1, 1, 0}, {3, 1, 0}, {3, 3, 0}, {1, 3, 0},
                                     {1, 1, 4}, {3, 1, 4}, {3, 3, 4}, {1, 3, 4}},
        {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}}, 10000);
    for(int f = 0; f < 6; ++f) {
        data = ReplaceRecord(data, 12100 + f, std::string(f < 2 ? "FACE_BOUND" : "FACE_OUTER_BOUND") +
                               "(''," + Ref(12000 + f) + ",.F.)");
        if(f < 2) data = ReplaceRecord(data, 2600 + f, "ADVANCED_FACE('',(" + Ref(2100 + f) + "," +
                                      Ref(12100 + f) + ")," + Ref(2500 + f) + ",.T.)");
        else data = ReplaceRecord(data, 12600 + f, "ADVANCED_FACE('',(" + Ref(12100 + f) + ")," + Ref(12500 + f) + ",.F.)");
    }
    data = ReplaceRecord(data, 12701, "SHAPE_REPRESENTATION('',(#12400),#12810)");
    data = ReplaceRecord(data, 12820, "SHAPE_REPRESENTATION('',(#12400),#12810)");
    return ReplaceRecord(data, 2700, "CLOSED_SHELL('',(#2600,#2601,#2602,#2603,#2604,#2605,#12602,#12603,#12604,#12605))");
}

double TriangulatedVolume(SShell &shell, double *area = nullptr) {
    MeshOwner mesh;
    shell.TriangulateInto(&mesh.mesh);
    REQUIRE(!mesh.mesh.IsEmpty());
    double volume = 0, sumArea = 0;
    for(const auto &triangle : mesh.mesh.l) {
        volume += triangle.SignedVolume(); sumArea += triangle.Area();
        REQUIRE(!triangle.IsDegenerate());
    }
    if(area) *area = sumArea;
    return volume;
}

void CheckGeneralNative(Step::NativeSolid &solid, int sourceFaces, int sourceEdges, int sourceVertices,
                         double volume, double tolerance = 0.01) {
    REQUIRE(solid.faces.size() == (size_t)sourceFaces);
    REQUIRE(solid.edges.size() == (size_t)sourceEdges);
    REQUIRE(solid.vertices.size() == (size_t)sourceVertices);
    std::set<hSSurface> surfaces;
    std::set<hSCurve> sourceCurves;
    std::map<hSCurve, std::vector<bool>> uses;
    for(const auto &face : solid.faces) {
        REQUIRE(!face.surfaces.empty() && face.surface == face.surfaces.front());
        REQUIRE(face.surface.v == (uint32_t)face.face);
        for(auto handle : face.surfaces) {
            REQUIRE(surfaces.insert(handle).second);
            auto s = solid.shell.surface.FindById(handle);
            REQUIRE(s->face == 0 && s->degm >= 1 && s->degm <= 3 && s->degn >= 1 && s->degn <= 3);
            for(int i = 0; i <= s->degm; ++i) for(int j = 0; j <= s->degn; ++j) REQUIRE(s->weight[i][j] > 0);
            if(face.planar) REQUIRE(s->NormalAt(0.5, 0.5).WithMagnitude(1).Equals(face.normal));
            SEdgeList uv = {};
            s->MakeEdgesInto(&solid.shell, &uv, SSurface::MakeAs::UV);
            auto bsp = SBspUv::From(&uv, s);
            REQUIRE(bsp != nullptr);
            SPolygon polygon = {};
            REQUIRE(uv.AssemblePolygon(&polygon, nullptr, true));
            MeshOwner uvMesh;
            polygon.UvTriangulateInto(&uvMesh.mesh, s, /*planar=*/face.planar);
            REQUIRE(!uvMesh.mesh.IsEmpty());
            const auto &triangle = uvMesh.mesh.l[0];
            Vector center = triangle.a.Plus(triangle.b).Plus(triangle.c).ScaledBy(1.0 / 3);
            REQUIRE(bsp->ClassifyPoint({center.x, center.y}, {0.123, 0.456}, s) == SBspUv::Class::INSIDE);
            polygon.Clear();
            REQUIRE(bsp->ClassifyPoint({-2, -2}, {0.123, 0.456}, s) == SBspUv::Class::OUTSIDE);
            uv.Clear();
            for(const auto &trim : s->trim) {
                auto c = solid.shell.curve.FindById(trim.curve);
                uses[c->h].push_back(trim.backwards);
                REQUIRE(c->surfA == s->h || c->surfB == s->h);
                REQUIRE(trim.start.Equals(trim.backwards ? c->exact.Finish() : c->exact.Start()));
            }
        }
    }
    REQUIRE(surfaces.size() == (size_t)solid.shell.surface.n);
    for(const auto &edge : solid.edges) {
        REQUIRE(!edge.curves.empty() && edge.curve == edge.curves.front());
        REQUIRE(edge.curve.v == (uint32_t)edge.edge);
        for(auto curve : edge.curves) REQUIRE(sourceCurves.insert(curve).second);
    }
    for(const auto &curve : solid.shell.curve) {
        REQUIRE(curve.isExact && curve.exact.deg >= 1 && curve.exact.deg <= 3 && curve.exact.entity == 0);
        REQUIRE(curve.pts.n >= 2 && curve.pts[0].p.Equals(curve.exact.Start()));
        REQUIRE(curve.pts.Last()->p.Equals(curve.exact.Finish()));
        REQUIRE(uses[curve.h].size() == 2 && uses[curve.h][0] != uses[curve.h][1]);
    }
    REQUIRE(!solid.shell.booleanFailed && !solid.shell.IsEmpty());
    REQUIRE(std::abs(TriangulatedVolume(solid.shell) - volume) < tolerance);
}

// Independent Cox-de Boor basis evaluation verifies exact knot insertion,
// including rational weights, nonuniform knots and non-clamped finite domains.
double Basis(const std::vector<double> &knots, int i, int degree, double t) {
    if(degree == 0) return t >= knots[i] && t < knots[i + 1] ? 1 : 0;
    double result = 0;
    if(knots[i + degree] != knots[i]) result += (t - knots[i]) / (knots[i + degree] - knots[i]) * Basis(knots, i, degree - 1, t);
    if(knots[i + degree + 1] != knots[i + 1]) result += (knots[i + degree + 1] - t) /
        (knots[i + degree + 1] - knots[i + 1]) * Basis(knots, i + 1, degree - 1, t);
    return result;
}

std::string NativeSignature(Step::NativeSolid &solid) {
    // Exact output controls/weights and trim data, excluding mutable native
    // projection caches. This inspection must not itself use scene settings.
    std::ostringstream out;
    out.imbue(std::locale::classic());
    for(const auto &surface : solid.shell.surface) {
        out << surface.h.v << ':' << surface.degm << ',' << surface.degn << ';';
        for(int i = 0; i <= surface.degm; ++i) for(int j = 0; j <= surface.degn; ++j) {
            out << Triple(surface.ctrl[i][j]) << Real(surface.weight[i][j]);
        }
        for(const auto &trim : surface.trim) {
            out << trim.curve.v << ',' << trim.backwards << Triple(trim.start) << Triple(trim.finish);
        }
    }
    for(const auto &curve : solid.shell.curve) {
        out << curve.h.v << ':' << curve.surfA.v << ',' << curve.surfB.v << ',' << curve.exact.deg << ';';
        for(int i = 0; i <= curve.exact.deg; ++i) {
            out << Triple(curve.exact.ctrl[i]) << Real(curve.exact.weight[i]);
        }
        for(const auto &point : curve.pts) out << point.vertex << Triple(point.p);
    }
    return out.str();
}

void NurbsMathTests() {
    Run("nurbs/exact_rational_nonuniform_and_unclamped_curve_decomposition", [&]() {
        for(bool clamped : {true, false}) for(int degree = 1; degree <= 3; ++degree) {
            std::vector<Vector> controls;
            std::vector<double> weights;
            for(int i = 0; i < 6; ++i) { controls.push_back({i * 0.7, std::sin(i * 0.8), i * i * 0.1}); weights.push_back(0.5 + i * 0.2); }
            std::vector<int> mult;
            std::vector<double> values;
            int count = clamped ? 7 - degree : 7 + degree;
            for(int i = 0; i < count; ++i) { mult.push_back(clamped && (i == 0 || i + 1 == count) ? degree + 1 : 1); values.push_back(i * i + 2.0 * i - 4); }
            Step::Nurbs::Knots knots(1, degree, controls.size(), mult, values);
            auto spans = Step::Nurbs::Curve(knots, controls, weights);
            REQUIRE(!spans.empty());
            for(const auto &span : spans) for(int sample = 1; sample < 20; ++sample) {
                double t = sample / 20.0, u = span.first + t * (span.last - span.first);
                Vector point = {};
                double weight = 0;
                for(size_t i = 0; i < controls.size(); ++i) {
                    double b = weights[i] * Basis(knots.full, (int)i, degree, u);
                    point = point.Plus(controls[i].ScaledBy(b)); weight += b;
                }
                REQUIRE(span.bezier.PointAt(t).Equals(point.ScaledBy(1 / weight), 1e-11));
            }
        }
    });
    Run("nurbs/exact_rational_tensor_product_decomposition", [&]() {
        auto knots = [](int entity, int degree, int controls, bool clamped) {
            std::vector<int> mult;
            std::vector<double> values;
            int count = clamped ? controls - degree + 1 : controls + degree + 1;
            for(int i = 0; i < count; ++i) {
                mult.push_back(clamped && (i == 0 || i + 1 == count) ? degree + 1 : 1);
                values.push_back(i * i + 2.0 * i - 4);
            }
            return Step::Nurbs::Knots(entity, degree, controls, mult, values);
        };
        std::vector<std::vector<Vector>> controls;
        std::vector<std::vector<double>> weights;
        for(int i = 0; i < 5; ++i) {
            std::vector<Vector> row;
            std::vector<double> wrow;
            for(int j = 0; j < 4; ++j) {
                row.push_back({i * 0.7, j * 0.3, std::sin(i * 0.8 + j * 0.4)});
                wrow.push_back(0.5 + i * 0.17 + j * 0.11);
            }
            controls.push_back(row); weights.push_back(wrow);
        }
        for(bool clampedU : {false, true}) for(bool clampedV : {false, true}) {
            for(int du = 1; du <= 3; ++du) for(int dv = 1; dv <= 3; ++dv) {
                auto u = knots(1, du, 5, clampedU), v = knots(1, dv, 4, clampedV);
                auto patches = Step::Nurbs::Surface(u, v, controls, weights);
                REQUIRE(!patches.empty());
                for(const auto &patch : patches) {
                    for(double s : {0.07, 0.23, 0.5, 0.79, 0.93}) for(double t : {0.07, 0.23, 0.5, 0.79, 0.93}) {
                        double a = patch.u0 + s * (patch.u1 - patch.u0);
                        double b = patch.v0 + t * (patch.v1 - patch.v0);
                        Vector point = {};
                        double weight = 0;
                        for(int i = 0; i < 5; ++i) for(int j = 0; j < 4; ++j) {
                            double basis = weights[i][j] * Basis(u.full, i, du, a) * Basis(v.full, j, dv, b);
                            point = point.Plus(controls[i][j].ScaledBy(basis)); weight += basis;
                        }
                        REQUIRE(patch.surface.PointAt(s, t).Equals(point.ScaledBy(1 / weight), 1e-11));
                    }
                }
            }
        }
    });
    Run("nurbs/collinear_cubic_recognition_checks_curve_not_control_order", [&]() {
        SBezier curve = SBezier::From(Vector{0, 0, 0}, Vector{0.8, 0, 0}, Vector{0.2, 0, 0}, Vector{1, 0, 0});
        for(bool rational : {false, true}) {
            if(rational) { curve.weight[1] = 0.8; curve.weight[2] = 1.2; }
            REQUIRE(Step::Nurbs::IsMonotoneLine(curve, curve.Start(), curve.Finish(), 1e-14));
            SBezier reversed = curve;
            reversed.Reverse();
            REQUIRE(!Step::Nurbs::IsMonotoneLine(reversed, curve.Start(), curve.Finish(), 1e-14));
        }
        curve.ctrl[1].x = 2; curve.ctrl[2].x = -1;
        REQUIRE(!Step::Nurbs::IsMonotoneLine(curve, curve.Start(), curve.Finish(), 1e-14));
        curve.ctrl[1] = {0.8, 1e-8, 0}; curve.ctrl[2] = {0.2, 0, 0};
        REQUIRE(!Step::Nurbs::IsMonotoneLine(curve, curve.Start(), curve.Finish(), 1e-14));
    });
    Run("nurbs/unrepresentable_knot_normalization_is_not_silently_collapsed", [&]() {
        bool rejected = false;
        try {
            Step::Nurbs::Knots knots(123, 1, 4, {1, 1, 1, 1, 1, 1}, {-1e16, -1, 0, 1e-20, 1, 1e16});
        } catch(const Step::Nurbs::Error &error) {
            REQUIRE(error.entity == 123 && error.unsupported);
            REQUIRE(error.message.find("collapse") != std::string::npos);
            rejected = true;
        }
        REQUIRE(rejected);
    });
    Run("nurbs/finite_parameters_and_exact_rational_splits", [&]() {
        SBezier curve = SBezier::From(Vector{1, 0, 0}, Vector{1, 1, 0}, Vector{0, 1, 0});
        curve.weight[0] = 2; curve.weight[1] = 1.4142135624; curve.weight[2] = 2;
        for(int i = 0; i <= 100; ++i) {
            double t = i / 100.0;
            auto matches = Step::Nurbs::Parameters(curve, curve.PointAt(t), 1e-10);
            REQUIRE(matches.size() == 1 && std::abs(matches.front() - t) < 1e-9);
        }
        REQUIRE(Step::Nurbs::Parameters(curve, curve.PointAt(-0.2)).empty());
        auto span = Step::Nurbs::Restrict(curve, 0.17, 0.83);
        for(int i = 0; i <= 20; ++i) REQUIRE(span.PointAt(i / 20.0).Equals(curve.PointAt(0.17 + 0.66 * i / 20.0), 1e-12));
        REQUIRE(curve.weight[1] == 1.4142135624);
        auto hits = Step::Nurbs::Intersections(1, curve, SBezier::From(Vector{0.5, 0, 0}, Vector{0.5, 1.5, 0}));
        REQUIRE(hits.size() == 1);
        REQUIRE(std::abs(curve.PointAt(hits.front().first).x - 0.5) < 1e-10);
    });
    Run("nurbs/native_skew_rational_and_nonaffine_uv_inversion", [&]() {
        SSurface surface = SSurface::FromPlane({2, -3, 4}, {0.3, 1, 0.2}, {1, 0.2, 0.5});
        for(int variant = 0; variant < 4; ++variant) {
            if(variant == 1) surface.weight[1][1] = 2;
            if(variant == 2) surface.ctrl[1][1] = surface.ctrl[1][1].Plus({0.25, 0.5, 0.175});
            if(variant == 3) surface.ctrl[1][1] = surface.ctrl[1][1].Plus({0, 0, 0.3});
            for(double u : {0.13, 0.37, 0.81}) for(double v : {0.17, 0.43, 0.89}) {
                Vector point = surface.PointAt(u, v);
                Point2d uv;
                surface.ClosestPointTo(point, &uv);
                REQUIRE(surface.PointAt(uv).Equals(point, 1e-8));
                REQUIRE(uv.Equals({u, v}, 1e-8));
            }
        }
        REQUIRE(!surface.IsPlanar());
    });
}

void GenericSplineTests(const Step::BindingInfo &binding) {
    const auto &schema = binding.schema;
    const auto name = binding.protocol + "/nurbs_";
    Run(name + "compound_cubic_patch_not_an_extrusion", [&]() {
        auto result = Accepted(schema, FreeformBox());
        CheckGeneralNative(*result.solids[0], 6, 12, 8, 1.1);
        auto top = result.solids[0]->shell.surface.FindById(hSSurface{2601});
        REQUIRE(top->degm == 3 && top->degn == 3);
        REQUIRE(!top->IsPlanar() && !top->IsExtrusion(nullptr, nullptr));
        REQUIRE(std::abs(top->PointAt(0.5, 0.5).z - 1.225) < 1e-12);
        REQUIRE(!result.solids[0]->faces[1].planar);
    });
    Run(name + "rational_compound_cubic_controls_and_weights", [&]() {
        auto result = Accepted(schema, FreeformBox(true));
        auto top = result.solids[0]->shell.surface.FindById(hSSurface{2601});
        REQUIRE(top->degm == 3 && top->degn == 3);
        REQUIRE(top->ctrl[1][1].Equals({1.0 / 3, 1.0 / 3, 1.4}, 1e-14));
        REQUIRE(top->weight[1][1] == 1 && top->weight[1][2] == 0.8 + 0.1 * 3 && top->weight[2][2] == 0.8 + 0.1 * 4);
        REQUIRE(!top->IsExtrusion(nullptr, nullptr));
        CheckGeneralNative(*result.solids[0], 6, 12, 8, 1.11, 0.03);
    });
    Run(name + "exact_cubic_multispan_surface_and_shared_seam", [&]() {
        auto result = Accepted(schema, FreeformBox(false, true));
        auto &solid = *result.solids[0];
        REQUIRE(solid.faces[1].surfaces.size() == 2);
        REQUIRE(solid.shell.surface.n == 7 && solid.shell.curve.n == 15);
        CheckGeneralNative(solid, 6, 12, 8, 1.15);
    });
    Run(name + "rational_multispan_surface", [&]() {
        auto result = Accepted(schema, FreeformBox(true, true));
        REQUIRE(result.solids[0]->shell.surface.n == 7 && result.solids[0]->shell.curve.n == 15);
        CheckGeneralNative(*result.solids[0], 6, 12, 8, 1.15, 0.03);
    });
    Run(name + "trimmed_multispan_plane_grid", [&]() {
        auto result = Accepted(schema, SplitPlaneBox());
        auto &solid = *result.solids[0];
        REQUIRE(solid.shell.surface.n == 9 && solid.shell.curve.n == 20);
        REQUIRE(solid.faces[1].planar && solid.faces[1].surfaces.size() == 4);
        CheckGeneralNative(solid, 6, 12, 8, 1, 1e-8);
    });
    Run(name + "nonplanar_bilinear_patch_is_not_flattened", [&]() {
        auto result = Accepted(schema, BilinearBox());
        auto top = result.solids[0]->shell.surface.FindById(hSSurface{2601});
        REQUIRE(top->degm == 1 && top->degn == 1 && !top->IsPlanar());
        REQUIRE(top->ctrl[1][1].Equals({1, 1, 1.4}, 1e-14));
        REQUIRE(top->PointAt(0.5, 0.5).Equals({0.5, 0.5, 1.1}, 1e-14));
        CheckGeneralNative(*result.solids[0], 6, 12, 8, 1.1);
    });
    Run(name + "rational_quadratic_and_cubic_nonextruded_loft", [&]() {
        auto outline = FreeformOutline();
        auto result = Accepted(schema, BezierLoft(outline));
        CheckGeneralNative(*result.solids[0], 6, 12, 8, OutlineArea(outline) * 2 * (1 + 0.8 + 0.64) / 3, 0.05);
        int lines = 0, planes = 0;
        for(const auto &edge : result.solids[0]->edges) lines += edge.linear ? 1 : 0;
        for(const auto &face : result.solids[0]->faces) planes += face.planar ? 1 : 0;
        REQUIRE(lines == 4 && planes == 2);
        for(int i = 2; i < 6; ++i) {
            auto surface = result.solids[0]->shell.surface.FindById(result.solids[0]->faces[i].surface);
            REQUIRE(!surface->IsExtrusion(nullptr, nullptr));
        }
    });
    Run(name + "curved_trims_cross_multispan_cap_grid", [&]() {
        auto outline = FreeformOutline();
        auto result = Accepted(schema, BezierLoft(outline, 2, 0.8, true));
        REQUIRE(result.solids[0]->faces[0].surfaces.size() == 4);
        REQUIRE(result.solids[0]->shell.surface.n == 9 && result.solids[0]->shell.curve.n == 20);
        CheckGeneralNative(*result.solids[0], 6, 12, 8, OutlineArea(outline) * 2 * (1 + 0.8 + 0.64) / 3, 0.05);
    });
    Run(name + "inner_trim_loops_and_genus_one_shell", [&]() {
        auto result = Accepted(schema, ThroughHole());
        CheckGeneralNative(*result.solids[0], 10, 24, 16, 48, 1e-8);
        double area;
        REQUIRE(std::abs(TriangulatedVolume(result.solids[0]->shell, &area) - 48) < 1e-8);
        REQUIRE(std::abs(area - 120) < 1e-8);
        auto surface = result.solids[0]->shell.surface.FindById(hSSurface{2601});
        SEdgeList uv = {};
        surface->MakeEdgesInto(&result.solids[0]->shell, &uv, SSurface::MakeAs::UV);
        auto bsp = SBspUv::From(&uv, surface);
        Point2d hole;
        surface->ClosestPointTo({2, 2, 4}, &hole);
        REQUIRE(bsp->ClassifyPoint(hole, {0.123, 0.456}, surface) == SBspUv::Class::OUTSIDE);
        uv.Clear();
    });
    Run(name + "compound_patch_native_boolean_union", [&]() {
        auto first = Accepted(schema, FreeformBox());
        auto second = Accepted(schema, Polyhedron({{0.5, 0, 0}, {1.5, 0, 0}, {1.5, 1, 0}, {0.5, 1, 0},
            {0.5, 0, 2}, {1.5, 0, 2}, {1.5, 1, 2}, {0.5, 1, 2}},
            {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}}));
        ShellOwner combined;
        combined.shell.MakeFromUnionOf(&first.solids[0]->shell, &second.solids[0]->shell);
        REQUIRE(!combined.shell.booleanFailed && !combined.shell.IsEmpty());
        REQUIRE(std::abs(TriangulatedVolume(combined.shell) - 2.55) < 0.01);
    });
    Run(name + "nonaffine_and_rational_planes_native_boolean_union", [&]() {
        for(bool cubic : {false, true}) {
            auto data = cubic ? FreeformBox(true, false, 0) : SplineSurface(Cube(1), 2501, 5000, 1, 1,
                {{{0, 0, 1}, {0, 1, 1}}, {{1, 0, 1}, {1, 1.25, 1}}}, {{1, 1}, {1, 2}},
                {2, 2}, {2, 2}, {0, 1}, {0, 1});
            auto first = Accepted(schema, data);
            auto second = Accepted(schema, Cube(1, {0.5, 0, 0}));
            auto top = first.solids[0]->shell.surface.FindById(hSSurface{2601});
            SSurface original = *top;
            REQUIRE(top->IsPlanar() && !top->IsAffine());
            ShellOwner combined;
            combined.shell.MakeFromUnionOf(&first.solids[0]->shell, &second.solids[0]->shell);
            REQUIRE(!combined.shell.booleanFailed && !combined.shell.IsEmpty());
            double area;
            REQUIRE(std::abs(TriangulatedVolume(combined.shell, &area) - 1.5) < 1e-8);
            REQUIRE(std::abs(area - 8) < 1e-8);
            for(int i = 0; i <= top->degm; ++i) for(int j = 0; j <= top->degn; ++j) {
                REQUIRE(top->ctrl[i][j].EqualsExactly(original.ctrl[i][j]));
                REQUIRE(top->weight[i][j] == original.weight[i][j]);
            }
        }
    });
    Run(name + "degree_four_reports_unsupported_without_fitting", [&]() {
        auto data = SplineCurve(Cube(1), 300, 5000, 4, {{0, 0, 0}, {0.25, 0, 0}, {0.5, 0, 0},
                                {0.75, 0, 0}, {1, 0, 0}}, {}, {5, 5}, {0, 1});
        auto result = Convert(schema, data);
        REQUIRE(result.status == Status::Rejected && result.solids.empty());
        REQUIRE(result.diagnostics.size() == 1 && result.diagnostics[0].entity == 300);
        REQUIRE(result.diagnostics[0].code == Step::GeometryDiagnostic::Code::Unsupported);
        REQUIRE(result.diagnostics[0].message.find("1..3") != std::string::npos);
    });
    Run(name + "invalid_rational_weights_reported", [&]() {
        for(double weight : {0.0, -1.0}) {
            auto data = SplineCurve(Cube(1), 300, 5000, 1, {{0, 0, 0}, {1, 0, 0}}, {1, weight}, {2, 2}, {0, 1});
            auto result = Convert(schema, data);
            REQUIRE(result.status == Status::Rejected && result.solids.empty());
            REQUIRE(result.diagnostics[0].entity == 300 && result.diagnostics[0].code == Step::GeometryDiagnostic::Code::InvalidGeometry);
        }
    });
    Run(name + "surface_weights_and_knots_are_validated_without_repair", [&]() {
        const std::vector<std::vector<Vector>> net = {{{0, 0, 1}, {0, 1, 1}}, {{1, 0, 1}, {1, 1, 1}}};
        auto check = [&](const std::string &data, Step::GeometryDiagnostic::Code code) {
            auto result = Convert(schema, data);
            REQUIRE(result.status == Status::Rejected && result.solids.empty());
            REQUIRE(result.diagnostics.size() == 1 && result.diagnostics[0].entity == 2501);
            REQUIRE(result.diagnostics[0].code == code);
        };
        for(double weight : {0.0, -1.0}) {
            check(SplineSurface(Cube(1), 2501, 5000, 1, 1, net, {{1, 1}, {1, weight}},
                               {2, 2}, {2, 2}, {0, 1}, {0, 1}), Step::GeometryDiagnostic::Code::InvalidGeometry);
        }
        check(SplineSurface(Cube(1), 2501, 5000, 1, 1, net, {{1, 1}, {1}},
                           {2, 2}, {2, 2}, {0, 1}, {0, 1}), Step::GeometryDiagnostic::Code::InvalidGeometry);
        for(double weight : {1e-101, 1e101, 1e-13}) {
            check(SplineSurface(Cube(1), 2501, 5000, 1, 1, net, {{1, 1}, {1, weight}},
                               {2, 2}, {2, 2}, {0, 1}, {0, 1}), Step::GeometryDiagnostic::Code::Unsupported);
        }
        check(SplineSurface(Cube(1), 2501, 5000, 1, 1, net, {},
                           {2, 1}, {2, 2}, {0, 1}, {0, 1}), Step::GeometryDiagnostic::Code::InvalidGeometry);
        check(SplineSurface(Cube(1), 2501, 5000, 1, 1, net, {},
                           {2, 2}, {2, 2}, {1, 0}, {0, 1}), Step::GeometryDiagnostic::Code::InvalidGeometry);
        check(SplineSurface(Cube(1), 2501, 5000, 1, 1, net, {},
                           {2, 2}, {2, 2}, {0, 0}, {0, 1}), Step::GeometryDiagnostic::Code::InvalidGeometry);
        check(SplineSurface(Cube(1), 2501, 5000, 1, 1,
                           {{{0, 0, 1}, {0, 1, 1}}, {{1, 0, 1}}}, {},
                           {2, 2}, {2, 2}, {0, 1}, {0, 1}), Step::GeometryDiagnostic::Code::InvalidGeometry);
        check(SplineSurface(Cube(1), 2501, 5000, 4, 1,
                           {{{0, 0, 1}, {0, 1, 1}}, {{0.25, 0, 1}, {0.25, 1, 1}},
                            {{0.5, 0, 1}, {0.5, 1, 1}}, {{0.75, 0, 1}, {0.75, 1, 1}}, {{1, 0, 1}, {1, 1, 1}}}, {},
                           {5, 5}, {2, 2}, {0, 1}, {0, 1}), Step::GeometryDiagnostic::Code::Unsupported);
    });
    Run(name + "monotone_collinear_rational_cubic_remains_an_exact_line_reference", [&]() {
        auto result = Accepted(schema, SplineCurve(Cube(1), 300, 5000, 3,
            {{0, 0, 0}, {0.8, 0, 0}, {0.2, 0, 0}, {1, 0, 0}}, {1, 0.8, 1.2, 1}, {4, 4}, {0, 1}));
        REQUIRE(result.solids[0]->edges[0].linear);
        auto &curve = result.solids[0]->shell.curve.FindById(hSCurve{400})->exact;
        REQUIRE(curve.deg == 3 && curve.ctrl[1].x == 0.8 && curve.ctrl[2].x == 0.2);
        REQUIRE(curve.weight[1] == 0.8 && curve.weight[2] == 1.2);
        CheckGeneralNative(*result.solids[0], 6, 12, 8, 1, 1e-8);
    });
    Run(name + "conversion_is_independent_of_scene_and_restores_settings", [&]() {
        TessellationSettings saved;
        auto document = Read(Exchange(schema, BezierLoft(FreeformOutline(), 2, 0.8, true)));
        auto baseline = Step::ConvertGeometry(*document);
        REQUIRE(baseline.status == Status::Okay && baseline.solids.size() == 1);
        auto signature = NativeSignature(*baseline.solids[0]);
        const double chords[] = {0, 1e30, 1e-30};
        const int counts[] = {0, -7, std::numeric_limits<int>::max()};
        for(int i = 0; i < 3; ++i) {
            SS.chordTolCalculated = chords[i]; SS.maxSegments = counts[i];
            SS.exportMode = i != 0; SS.exportChordTol = 0; SS.exportScale = 0; SS.exportMaxSegments = -1;
            auto result = Step::ConvertGeometry(*document);
            if(result.status != Status::Okay) PrintDiagnostics(result);
            REQUIRE(result.status == Status::Okay && result.solids.size() == 1);
            REQUIRE(NativeSignature(*result.solids[0]) == signature);
            REQUIRE(SS.chordTolCalculated == chords[i] && SS.maxSegments == counts[i] && SS.exportMode == (i != 0));
            REQUIRE(SS.exportChordTol == 0 && SS.exportScale == 0 && SS.exportMaxSegments == -1);
            auto invalid = ReplaceRecord(FreeformBox(), 2101, "FACE_OUTER_BOUND('',#2001,.F.)");
            Rejected(schema, invalid); // Fails after the scoped native settings were installed.
            auto partial = Convert(schema, invalid + Cube(1, {}, 10000));
            REQUIRE(partial.status == Status::Partial && partial.solids.size() == 1 && partial.diagnostics.size() == 1);
            REQUIRE(SS.chordTolCalculated == chords[i] && SS.maxSegments == counts[i] && SS.exportMode == (i != 0));
        }
        for(int variant = 0; variant < 4; ++variant) {
            Step::GeometryOptions options;
            if(variant == 0) options.chordTolerance = std::numeric_limits<double>::quiet_NaN();
            if(variant == 1) options.chordTolerance = -1;
            if(variant == 2) options.maxSegments = 0;
            if(variant == 3) options.maxSegments = 4097;
            auto result = Step::ConvertGeometry(*document, options);
            REQUIRE(result.status == Status::Rejected && result.solids.empty());
            REQUIRE(result.diagnostics.size() == 1 && result.diagnostics[0].code == Step::GeometryDiagnostic::Code::Unsupported);
            REQUIRE(SS.chordTolCalculated == chords[2] && SS.maxSegments == counts[2] && SS.exportMode);
        }
    });
    Run(name + "standard_implicit_spline_knots", [&]() {
        for(const auto &family : {"BEZIER", "UNIFORM", "QUASI_UNIFORM"}) {
            auto data = ReplaceRecord(Cube(1, {}, 0, true), 300, std::string(family) +
                "_CURVE('',1,(#10,#11),.UNSPECIFIED.,.F.,.F.)");
            data = ReplaceRecord(data, 2500, std::string(family) +
                "_SURFACE('',1,1,((#3000,#3001),(#3002,#3003)),.UNSPECIFIED.,.F.,.F.,.F.)");
            auto result = Accepted(schema, data);
            CheckUnitCube(*result.solids[0]);
        }
    });
    Run(name + "repeated_freeform_success_and_failure_cleanup", [&]() {
        auto exercise = [&]() {
            auto a = Accepted(schema, FreeformBox(true, true));
            auto b = Accepted(schema, BezierLoft(FreeformOutline(), 2, 0.8, true));
            Rejected(schema, ReplaceRecord(FreeformBox(), 5005, "CARTESIAN_POINT('',(100.,100.,100.))"));
        };
        exercise();
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtMemState before, after;
        _CrtMemCheckpoint(&before);
#endif
        for(int i = 0; i < 5; ++i) exercise();
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtMemCheckpoint(&after);
        REQUIRE(before.lCounts[_NORMAL_BLOCK] == after.lCounts[_NORMAL_BLOCK]);
        REQUIRE(before.lSizes[_NORMAL_BLOCK] == after.lSizes[_NORMAL_BLOCK]);
#endif
    });
}

void SplineTests(const Step::BindingInfo &binding) {
    const std::string &schema = binding.schema;
    const std::string cube = Cube(1, {}, 0, true);
    Run(binding.protocol + "/spline_cube_1mm", [&]() {
        auto result = Accepted(schema, cube);
        CheckUnitCube(*result.solids.front());
        const auto &edge = result.solids[0]->edges.front();
        const auto &face = result.solids[0]->faces.front();
        REQUIRE(edge.edge == 400 && edge.geometry == 300);
        REQUIRE(face.face == 2600 && face.geometry == 2500);
        auto surface = result.solids[0]->shell.surface.FindById(face.surface);
        // The actual source control net is retained, not a fitted/padded plane.
        REQUIRE(surface->ctrl[0][0].Equals({0, 0, 0}));
        REQUIRE(surface->ctrl[0][1].Equals({1, 0, 0}));
        REQUIRE(surface->ctrl[1][0].Equals({0, 1, 0}));
        REQUIRE(surface->ctrl[1][1].Equals({1, 1, 0}));
    });
    Run(binding.protocol + "/spline_mixed_with_analytic", [&]() {
        auto data = ReplaceRecord(cube, 300, "LINE('',#10,#200)");
        data = ReplaceRecord(data, 2500, "PLANE('',#2400)");
        auto result = Accepted(schema, data);
        CheckUnitCube(*result.solids.front());
    });
    Run(binding.protocol + "/spline_tetrahedron", [&]() {
        auto result = Accepted(schema, Tetrahedron(true));
        CheckNative(*result.solids.front(), 4, 6, 4, 1000.0 / 6, 150 + 50 * std::sqrt(3.0));
    });
    Run(binding.protocol + "/spline_meters", [&]() {
        auto data = ReplaceRecord(Cube(0.001, {}, 0, true), 2800,
                                  "(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT($,.METRE.))");
        auto result = Accepted(schema, data);
        CheckUnitCube(*result.solids.front());
    });
    Run(binding.protocol + "/spline_curve_same_sense", [&]() {
        auto data = ReplaceRecord(cube, 300,
            "B_SPLINE_CURVE_WITH_KNOTS('',1,(#11,#10),.UNSPECIFIED.,.F.,.F.,(2,2),(0.,1.),.UNSPECIFIED.)");
        data = ReplaceRecord(data, 400, "EDGE_CURVE('',#30,#31,#300,.F.)");
        auto result = Accepted(schema, data);
        CheckUnitCube(*result.solids.front());
    });
    Run(binding.protocol + "/spline_face_same_sense", [&]() {
        auto data = ReplaceRecord(cube, 2500,
            "B_SPLINE_SURFACE_WITH_KNOTS('',1,1,((#3000,#3002),(#3001,#3003)),"
            ".UNSPECIFIED.,.F.,.F.,.F.,(2,2),(2,2),(0.,1.),(0.,1.),.UNSPECIFIED.)");
        data = ReplaceRecord(data, 2600, "ADVANCED_FACE('',(#2100),#2500,.F.)");
        auto result = Accepted(schema, data);
        CheckUnitCube(*result.solids.front());
    });
    Run(binding.protocol + "/spline_knot_interval_normalization", [&]() {
        auto data = ReplaceRecord(cube, 300,
            "B_SPLINE_CURVE_WITH_KNOTS('',1,(#10,#11),.UNSPECIFIED.,.F.,.F.,(2,2),(-2.,7.),.UNSPECIFIED.)");
        data = ReplaceRecord(data, 2500,
            "B_SPLINE_SURFACE_WITH_KNOTS('',1,1,((#3000,#3001),(#3002,#3003)),"
            ".UNSPECIFIED.,.F.,.F.,.F.,(2,2),(2,2),(-5.,9.),(2.,4.),.UNSPECIFIED.)");
        auto result = Accepted(schema, data);
        CheckUnitCube(*result.solids.front());
    });
    Run(binding.protocol + "/spline_trimmed_subsegment", [&]() {
        auto data = cube + Record(3100, "CARTESIAN_POINT('',(-1.,0.,0.))") +
                           Record(3101, "CARTESIAN_POINT('',(2.,0.,0.))");
        data = ReplaceRecord(data, 300,
            "B_SPLINE_CURVE_WITH_KNOTS('',1,(#3100,#3101),.UNSPECIFIED.,.F.,.F.,"
            "(2,2),(0.,1.),.UNSPECIFIED.)");
        auto result = Accepted(schema, data);
        CheckUnitCube(*result.solids.front());
    });
    Run(binding.protocol + "/spline_rotated_skew_solid", [&]() {
        Vector a = {1, 0, 0}, b = {0.25, 1, 0}, c = {0.1, 0.2, 1};
        std::vector<Vector> points = {{}, a, a.Plus(b), b,
            c, a.Plus(c), a.Plus(b).Plus(c), b.Plus(c)};
        for(auto &point : points) point = point.RotatedAbout({1, 2, 3}, 0.7).Plus({12, -3, 4});
        auto result = Accepted(schema, Polyhedron(points, {{0, 3, 2, 1}, {4, 5, 6, 7},
            {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}}, 0, true));
        double area = 2 * (a.Cross(b).Magnitude() + a.Cross(c).Magnitude() + b.Cross(c).Magnitude());
        CheckNative(*result.solids.front(), 6, 12, 8, 1, area);
    });
    Run(binding.protocol + "/spline_native_boolean_union", [&]() {
        auto first = Accepted(schema, Cube(10, {}, 0, true));
        auto second = Accepted(schema, Cube(10, {5, 0, 0}));
        ShellOwner combined;
        combined.shell.MakeFromUnionOf(&first.solids[0]->shell, &second.solids[0]->shell);
        REQUIRE(!combined.shell.booleanFailed && !combined.shell.IsEmpty());
        MeshOwner mesh;
        combined.shell.TriangulateInto(&mesh.mesh);
        double volume = 0;
        for(const auto &triangle : mesh.mesh.l) volume += triangle.SignedVolume();
        REQUIRE(std::abs(volume - 1500) < 1e-6);
    });
    Run(binding.protocol + "/spline_no_curve_extrapolation", [&]() {
        auto data = cube + Record(3100, "CARTESIAN_POINT('',(0.5,0.,0.))");
        Rejected(schema, ReplaceRecord(data, 300,
            "B_SPLINE_CURVE_WITH_KNOTS('',1,(#10,#3100),.UNSPECIFIED.,.F.,.F.,(2,2),(0.,1.),.UNSPECIFIED.)"));
    });
    Run(binding.protocol + "/spline_no_surface_extrapolation", [&]() {
        auto data = ReplaceRecord(cube, 3001, "CARTESIAN_POINT('',(0.5,0.,0.))");
        data = ReplaceRecord(data, 3003, "CARTESIAN_POINT('',(0.5,1.,0.))");
        Rejected(schema, data);
    });
    Run(binding.protocol + "/spline_no_skew_surface_extrapolation", [&]() {
        auto data = ReplaceRecord(cube, 3002, "CARTESIAN_POINT('',(0.5,1.,0.))");
        Rejected(schema, ReplaceRecord(data, 3003, "CARTESIAN_POINT('',(1.5,1.,0.))"));
    });
    Run(binding.protocol + "/spline_no_warped_patch_flattening", [&]() {
        for(const auto &z : {"0.01", "1.E-8"}) {
            // The smaller warp is below LENGTH_EPS but not floating-point
            // roundoff: even that must not be fitted away as a plane.
            auto data = ReplaceRecord(cube, 3003, "CARTESIAN_POINT('',(1.,1.," + std::string(z) + "))");
            auto result = Convert(schema, data);
            if(std::string(z) == "0.01") {
                REQUIRE(result.status == Status::Rejected && result.solids.empty());
                REQUIRE(result.diagnostics.size() == 1 && result.diagnostics[0].entity == 2500);
                REQUIRE(result.diagnostics[0].code == Step::GeometryDiagnostic::Code::InvalidGeometry);
            } else {
                REQUIRE(result.status == Status::Okay && result.solids.size() == 1);
                REQUIRE(!result.solids[0]->faces[0].planar);
                auto surface = result.solids[0]->shell.surface.FindById(hSSurface{2600});
                REQUIRE(surface->ctrl[1][1].z == 1e-8);
                REQUIRE(!surface->IsPlanar());
            }
        }
    });
    Run(binding.protocol + "/spline_non_affine_planar_patch", [&]() {
        auto result = Accepted(schema, ReplaceRecord(cube, 3003, "CARTESIAN_POINT('',(1.,1.25,0.))"));
        CheckGeneralNative(*result.solids[0], 6, 12, 8, 1, 1e-8);
        REQUIRE(result.solids[0]->shell.surface[0].ctrl[1][1].Equals({1, 1.25, 0}, 1e-14));
    });
    Run(binding.protocol + "/spline_degenerate_patch", [&]() {
        auto data = ReplaceRecord(cube, 3002, "CARTESIAN_POINT('',(0.,0.,0.))");
        Rejected(schema, ReplaceRecord(data, 3003, "CARTESIAN_POINT('',(1.,0.,0.))"));
    });
    Run(binding.protocol + "/spline_degenerate_curve", [&]() {
        Rejected(schema, ReplaceRecord(cube, 300,
            "B_SPLINE_CURVE_WITH_KNOTS('',1,(#10,#10),.UNSPECIFIED.,.F.,.F.,(2,2),(0.,1.),.UNSPECIFIED.)"));
    });
    Run(binding.protocol + "/spline_higher_order_curve", [&]() {
        Rejected(schema, ReplaceRecord(cube, 300,
            "B_SPLINE_CURVE_WITH_KNOTS('',2,(#10,#11,#12),.UNSPECIFIED.,.F.,.F.,"
            "(3,3),(0.,1.),.UNSPECIFIED.)"));
    });
    Run(binding.protocol + "/spline_trimmed_multispan_curve", [&]() {
        auto result = Accepted(schema, ReplaceRecord(cube, 300,
            "B_SPLINE_CURVE_WITH_KNOTS('',1,(#10,#11,#12),.UNSPECIFIED.,.F.,.F.,"
            "(2,1,2),(0.,0.5,1.),.UNSPECIFIED.)"));
        CheckUnitCube(*result.solids[0]);
    });
    Run(binding.protocol + "/spline_higher_order_surface", [&]() {
        auto result = Accepted(schema, ReplaceRecord(cube, 2500,
            "B_SPLINE_SURFACE_WITH_KNOTS('',2,1,((#3000,#3001),(#3000,#3001),(#3002,#3003)),"
            ".UNSPECIFIED.,.F.,.F.,.F.,(3,3),(2,2),(0.,1.),(0.,1.),.UNSPECIFIED.)"));
        CheckGeneralNative(*result.solids[0], 6, 12, 8, 1, 1e-8);
    });
    Run(binding.protocol + "/spline_multispan_surface", [&]() {
        Rejected(schema, ReplaceRecord(cube, 2500,
            "B_SPLINE_SURFACE_WITH_KNOTS('',1,1,((#3000,#3001),(#3000,#3001),(#3002,#3003)),"
            ".UNSPECIFIED.,.F.,.F.,.F.,(2,1,2),(2,2),(0.,0.5,1.),(0.,1.),.UNSPECIFIED.)"));
    });
    Run(binding.protocol + "/spline_no_rational_curve_weight_dropping", [&]() {
        auto result = Accepted(schema, ReplaceRecord(cube, 300,
            "(BOUNDED_CURVE()B_SPLINE_CURVE(1,(#10,#11),.UNSPECIFIED.,.F.,.F.)"
            "B_SPLINE_CURVE_WITH_KNOTS((2,2),(0.,1.),.UNSPECIFIED.)CURVE()"
            "GEOMETRIC_REPRESENTATION_ITEM()RATIONAL_B_SPLINE_CURVE((1.,2.))REPRESENTATION_ITEM(''))"));
        CheckUnitCube(*result.solids[0]);
        REQUIRE(result.solids[0]->shell.curve.FindById(hSCurve{400})->exact.weight[1] == 2);
    });
    Run(binding.protocol + "/spline_no_rational_surface_weight_dropping", [&]() {
        auto result = Accepted(schema, ReplaceRecord(cube, 2500,
            "(BOUNDED_SURFACE()B_SPLINE_SURFACE(1,1,((#3000,#3001),(#3002,#3003)),"
            ".UNSPECIFIED.,.F.,.F.,.F.)B_SPLINE_SURFACE_WITH_KNOTS((2,2),(2,2),(0.,1.),(0.,1.),"
            ".UNSPECIFIED.)GEOMETRIC_REPRESENTATION_ITEM()RATIONAL_B_SPLINE_SURFACE(((1.,2.),(1.,1.)))"
            "REPRESENTATION_ITEM('')SURFACE())"));
        CheckUnitCube(*result.solids[0]);
        REQUIRE(result.solids[0]->shell.surface[0].weight[0][1] == 2);
    });
    Run(binding.protocol + "/spline_invalid_curve_knots", [&]() {
        for(const auto &knots : {"(0.,0.)", "(1.,0.)"}) {
            Rejected(schema, ReplaceRecord(cube, 300,
                "B_SPLINE_CURVE_WITH_KNOTS('',1,(#10,#11),.UNSPECIFIED.,.F.,.F.,(2,2)," +
                std::string(knots) + ",.UNSPECIFIED.)"));
        }
        Rejected(schema, ReplaceRecord(cube, 300,
            "B_SPLINE_CURVE_WITH_KNOTS('',1,(#10,#11),.UNSPECIFIED.,.F.,.F.,(1,3),(0.,1.),.UNSPECIFIED.)"));
    });
    Run(binding.protocol + "/spline_invalid_surface_knots", [&]() {
        for(const auto &intervals : {"(0.,0.),(0.,1.)", "(0.,1.),(1.,0.)"}) {
            Rejected(schema, ReplaceRecord(cube, 2500,
                "B_SPLINE_SURFACE_WITH_KNOTS('',1,1,((#3000,#3001),(#3002,#3003)),"
                ".UNSPECIFIED.,.F.,.F.,.F.,(2,2),(2,2)," + std::string(intervals) + ",.UNSPECIFIED.)"));
        }
        Rejected(schema, ReplaceRecord(cube, 2500,
            "B_SPLINE_SURFACE_WITH_KNOTS('',1,1,((#3000,#3001),(#3002,#3003)),"
            ".UNSPECIFIED.,.F.,.F.,.F.,(2,2),(1,3),(0.,1.),(0.,1.),.UNSPECIFIED.)"));
    });
    Run(binding.protocol + "/spline_closed_or_uncertain_curve", [&]() {
        for(const auto &flags : {".T.,.F.", ".U.,.F.", ".F.,.T.", ".F.,.U."}) {
            auto data = ReplaceRecord(cube, 300,
                "B_SPLINE_CURVE_WITH_KNOTS('',1,(#10,#11),.UNSPECIFIED.," + std::string(flags) +
                ",(2,2),(0.,1.),.UNSPECIFIED.)");
            if(std::string(flags).find(".U.") != std::string::npos) {
                auto result = Accepted(schema, data); CheckUnitCube(*result.solids[0]);
            } else Rejected(schema, data);
        }
    });
    Run(binding.protocol + "/spline_closed_or_uncertain_surface", [&]() {
        for(const auto &flags : {".T.,.F.,.F.", ".F.,.T.,.F.", ".F.,.F.,.T.", ".U.,.F.,.F."}) {
            auto data = ReplaceRecord(cube, 2500,
                "B_SPLINE_SURFACE_WITH_KNOTS('',1,1,((#3000,#3001),(#3002,#3003)),.UNSPECIFIED.," +
                std::string(flags) + ",(2,2),(2,2),(0.,1.),(0.,1.),.UNSPECIFIED.)");
            if(std::string(flags).find(".U.") != std::string::npos) {
                auto result = Accepted(schema, data); CheckUnitCube(*result.solids[0]);
            } else Rejected(schema, data);
        }
    });
    Run(binding.protocol + "/spline_bad_orientation", [&]() {
        Rejected(schema, ReplaceRecord(cube, 400, "EDGE_CURVE('',#30,#31,#300,.F.)"));
        Rejected(schema, ReplaceRecord(cube, 2600, "ADVANCED_FACE('',(#2100),#2500,.F.)"));
    });
    Run(binding.protocol + "/spline_whole_solid_partial", [&]() {
        auto second = ReplaceRecord(Cube(1, {2, 0, 0}, 10000, true), 13003,
                                    "CARTESIAN_POINT('',(3.,1.,0.01))");
        auto result = Convert(schema, cube + second);
        REQUIRE(result.status == Status::Partial && result.solids.size() == 1);
        REQUIRE(result.diagnostics.size() == 1 && result.diagnostics[0].solid == 12701);
        REQUIRE(result.diagnostics[0].entity == 12500);
        CheckUnitCube(*result.solids.front());
    });
}

void ProtocolTests(const Step::BindingInfo &binding) {
    const std::string &schema = binding.schema;
    const std::string cube = Cube();
    Run(binding.protocol + "/cube_10mm", [&]() {
        auto result = Accepted(schema, cube);
        auto &solid = *result.solids.front();
        REQUIRE(solid.solid == 2701 && solid.representation == 2820 && solid.context == 2810);
        REQUIRE(std::abs(solid.lengthToMm - 1) < 1e-15);
        CheckNative(solid, 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/tetrahedron", [&]() {
        auto result = Accepted(schema, Tetrahedron());
        CheckNative(*result.solids.front(), 4, 6, 4, 1000.0 / 6, 150 + 50 * std::sqrt(3.0));
    });
    Run(binding.protocol + "/meters", [&]() {
        auto data = ReplaceRecord(Cube(0.01), 2800, "(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT($,.METRE.))");
        auto result = Accepted(schema, data);
        REQUIRE(result.solids.front()->lengthToMm == 1000);
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/conversion_based_inches", [&]() {
        auto result = Accepted(schema, InchUnits(Cube(10 / 25.4), binding.protocol));
        REQUIRE(std::abs(result.solids.front()->lengthToMm - 25.4) < 1e-12);
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/nested_conversion_based_feet", [&]() {
        auto data = InchUnits(Cube(10 / 304.8), binding.protocol);
        data += Record(2833, "LENGTH_MEASURE_WITH_UNIT(LENGTH_MEASURE(12.),#2832)");
        data += Record(2834, "(CONVERSION_BASED_UNIT('FOOT',#2833)LENGTH_UNIT()NAMED_UNIT(" +
                       std::string(binding.protocol == "AP214" ? "*" : "#2830") + "))");
        data = ReplaceRecord(data, 2810, ContextWithUnits({2834, 2801, 2802}));
        auto result = Accepted(schema, data);
        REQUIRE(std::abs(result.solids.front()->lengthToMm - 304.8) < 1e-12);
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/placement_and_translation", [&]() {
        auto result = Accepted(schema, Cube(10, {123, -456, 789}));
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/face_same_sense", [&]() {
        auto data = ReplaceRecord(cube, 2200, "DIRECTION('',(0.,0.,1.))"); // bottom underlying plane reversed
        data = ReplaceRecord(data, 2600, "ADVANCED_FACE('',(#2100),#2500,.F.)");
        auto result = Accepted(schema, data);
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/bound_orientation", [&]() {
        auto document = Read(Exchange(schema, cube));
        auto data = ReplaceRecord(cube, 2100, "FACE_OUTER_BOUND('',#2000,.F.)");
        data = ReplaceRecord(data, 2000, "EDGE_LOOP('',(#1003,#1002,#1001,#1000))");
        for(int id = 1000; id <= 1003; ++id) {
            Step::Value edge, orientation;
            REQUIRE(document->Attribute(id, "edge_element", &edge));
            REQUIRE(document->Attribute(id, "orientation", &orientation));
            data = ReplaceRecord(data, id, "ORIENTED_EDGE('',*,*," + Ref(edge.reference) +
                                 (orientation.text == "T" ? ",.F.)" : ",.T.)"));
        }
        auto result = Accepted(schema, data);
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/line_same_sense", [&]() {
        auto data = ReplaceRecord(cube, 100, "DIRECTION('',(-1.,0.,0.))");
        data = ReplaceRecord(data, 400, "EDGE_CURVE('',#30,#31,#300,.F.)");
        auto result = Accepted(schema, data);
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/default_plane_axes", [&]() {
        // Top plane has +Z normal and can use both EXPRESS defaults.
        auto result = Accepted(schema, ReplaceRecord(cube, 2401, "AXIS2_PLACEMENT_3D('',#14,$,$)"));
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/rotated_polyhedron", [&]() {
        std::vector<Vector> points = {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0},
            {0, 0, 10}, {10, 0, 10}, {10, 10, 10}, {0, 10, 10}};
        for(auto &point : points) point = point.RotatedAbout({1, 2, 3}, 0.7).Plus({12, -3, 4});
        auto result = Accepted(schema, Polyhedron(points, {{0, 3, 2, 1}, {4, 5, 6, 7},
            {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}}));
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/native_boolean_union", [&]() {
        auto first = Accepted(schema, cube);
        auto second = Accepted(schema, Cube(10, {5, 0, 0}));
        ShellOwner combined;
        combined.shell.MakeFromUnionOf(&first.solids[0]->shell, &second.solids[0]->shell);
        REQUIRE(!combined.shell.booleanFailed);
        REQUIRE(!combined.shell.IsEmpty());
        MeshOwner mesh;
        combined.shell.TriangulateInto(&mesh.mesh);
        double volume = 0;
        for(const auto &triangle : mesh.mesh.l) volume += triangle.SignedVolume();
        REQUIRE(std::abs(volume - 1500) < 1e-6);
    });
    Run(binding.protocol + "/whole_solid_partial", [&]() {
        auto second = ReplaceRecord(Cube(10, {20, 0, 0}, 10000), 12500,
                                    "CYLINDRICAL_SURFACE('',#12400,10.)");
        auto result = Convert(schema, cube + second);
        REQUIRE(result.status == Status::Partial);
        REQUIRE(result.solids.size() == 1 && result.solids.front()->solid == 2701);
        REQUIRE(result.diagnostics.size() == 1);
        REQUIRE(result.diagnostics[0].solid == 12701 && result.diagnostics[0].entity == 12500);
        CheckNative(*result.solids.front(), 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/all_unsupported", [&]() {
        Rejected(schema, ReplaceRecord(cube, 2500, "CYLINDRICAL_SURFACE('',#2400,10.)"));
    });
    Run(binding.protocol + "/no_cavity_stripping", [&]() {
        auto data = ReplaceRecord(cube, 2701, "BREP_WITH_VOIDS('',#2700,(#2900))");
        data += Record(2900, "ORIENTED_CLOSED_SHELL('',*,#2700,.F.)");
        Rejected(schema, data);
    });
    Run(binding.protocol + "/no_face_hole_stripping", [&]() {
        Rejected(schema, ReplaceRecord(cube, 2600, "ADVANCED_FACE('',(#2100,#2101),#2500,.T.)"));
    });
    Run(binding.protocol + "/open_shell", [&]() {
        Rejected(schema, ReplaceRecord(cube, 2700, "CLOSED_SHELL('',(#2600,#2601,#2602,#2603,#2604))"));
    });
    Run(binding.protocol + "/non_manifold_edge", [&]() {
        auto data = cube + Record(2670, "ADVANCED_FACE('',(#2100),#2500,.T.)");
        Rejected(schema, ReplaceRecord(data, 2700,
            "CLOSED_SHELL('',(#2600,#2601,#2602,#2603,#2604,#2605,#2670))"));
    });
    Run(binding.protocol + "/disconnected_closed_shell", [&]() {
        auto data = cube + Cube(10, {20, 0, 0}, 10000);
        data = ReplaceRecord(data, 12820, "SHAPE_REPRESENTATION('',(#12400),#12810)");
        Rejected(schema, ReplaceRecord(data, 2700,
            "CLOSED_SHELL('',(#2600,#2601,#2602,#2603,#2604,#2605,"
                            "#12600,#12601,#12602,#12603,#12604,#12605))"));
    });
    Run(binding.protocol + "/non_convex_simple_face", [&]() {
        auto result = Accepted(schema, ConcavePrism());
        CheckGeneralNative(*result.solids[0], 8, 18, 12, 640, 1e-8);
    });
    Run(binding.protocol + "/non_convex_solid_with_simple_faces", [&]() {
        auto result = Accepted(schema, ConcaveSolidWithConvexFaces());
        CheckGeneralNative(*result.solids[0], 9, 16, 9, 1000.0 - 500.0 / 3, 1e-8);
    });
    Run(binding.protocol + "/bad_face_sense", [&]() {
        Rejected(schema, ReplaceRecord(cube, 2600, "ADVANCED_FACE('',(#2100),#2500,.F.)"));
    });
    Run(binding.protocol + "/bad_bound_sense", [&]() {
        Rejected(schema, ReplaceRecord(cube, 2100, "FACE_OUTER_BOUND('',#2000,.F.)"));
    });
    Run(binding.protocol + "/bad_line_sense", [&]() {
        Rejected(schema, ReplaceRecord(cube, 400, "EDGE_CURVE('',#30,#31,#300,.F.)"));
    });
    Run(binding.protocol + "/off_plane_vertex", [&]() {
        Rejected(schema, ReplaceRecord(cube, 10, "CARTESIAN_POINT('',(0.,0.,1.))"));
    });
    Run(binding.protocol + "/zero_direction", [&]() {
        Rejected(schema, ReplaceRecord(cube, 100, "DIRECTION('',(0.,0.,0.))"));
    });
    Run(binding.protocol + "/parallel_plane_axes", [&]() {
        Rejected(schema, ReplaceRecord(cube, 2400, "AXIS2_PLACEMENT_3D('',#10,#2200,#2200)"));
    });
    Run(binding.protocol + "/missing_length_units", [&]() {
        Rejected(schema, ReplaceRecord(cube, 2810, ContextWithUnits({2801, 2802})));
    });
    Run(binding.protocol + "/ambiguous_units", [&]() {
        auto data = cube + Record(2830, "(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT($,.METRE.))");
        Rejected(schema, ReplaceRecord(data, 2810, ContextWithUnits({2800, 2830, 2801, 2802})));
    });
    Run(binding.protocol + "/non_positive_unit_factor", [&]() {
        auto data = InchUnits(cube, binding.protocol);
        Rejected(schema, ReplaceRecord(data, 2831, "LENGTH_MEASURE_WITH_UNIT(LENGTH_MEASURE(-25.4),#2800)"));
        Rejected(schema, ReplaceRecord(data, 2831, "LENGTH_MEASURE_WITH_UNIT(LENGTH_MEASURE(0.),#2800)"));
    });
    Run(binding.protocol + "/wrong_conversion_measure_type", [&]() {
        auto data = InchUnits(cube, binding.protocol);
        Rejected(schema, ReplaceRecord(data, 2831, "MEASURE_WITH_UNIT(PLANE_ANGLE_MEASURE(25.4),#2800)"));
    });
    Run(binding.protocol + "/cyclic_units", [&]() {
        auto data = InchUnits(cube, binding.protocol);
        Rejected(schema, ReplaceRecord(data, 2831, "LENGTH_MEASURE_WITH_UNIT(LENGTH_MEASURE(25.4),#2832)"));
    });
    Run(binding.protocol + "/no_assembly_flattening", [&]() {
        auto data = cube + Record(2900, "REPRESENTATION_RELATIONSHIP('','',#2820,#2820)");
        Rejected(schema, data);
    });
    Run(binding.protocol + "/no_transform_ignoring", [&]() {
        auto data = cube + Record(2900, "ITEM_DEFINED_TRANSFORMATION('','',#2400,#2401)");
        data += Record(2901, "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION('','',#2820,#2820,#2900)");
        Rejected(schema, data);
    });
    Run(binding.protocol + "/ambiguous_occurrence", [&]() {
        auto data = cube + Record(2900, "ADVANCED_BREP_SHAPE_REPRESENTATION('',(#2701),#2810)");
        Rejected(schema, data);
    });
    Run(binding.protocol + "/orphan_solid", [&]() {
        Rejected(schema, ReplaceRecord(cube, 2820, "SHAPE_REPRESENTATION('',(#2400),#2810)"));
    });
    Run(binding.protocol + "/two_complete_solids", [&]() {
        auto result = Convert(schema, cube + Cube(10, {20, 0, 0}, 10000));
        REQUIRE(result.status == Status::Okay && result.diagnostics.empty());
        REQUIRE(result.solids.size() == 2);
        CheckNative(*result.solids[0], 6, 12, 8, 1000, 600);
        CheckNative(*result.solids[1], 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/numeric_envelope", [&]() {
        Rejected(schema, Cube(10, {1e9, 0, 0}));
        Rejected(schema, Cube(1e-7));
    });
    Run(binding.protocol + "/no_geometry", [&]() {
        Rejected(schema, "");
    });
    Run(binding.protocol + "/stable_source_handles", [&]() {
        auto first = Accepted(schema, cube);
        auto second = Accepted(schema, cube);
        for(size_t i = 0; i < first.solids[0]->edges.size(); ++i) {
            REQUIRE(first.solids[0]->edges[i].edge == second.solids[0]->edges[i].edge);
            REQUIRE(first.solids[0]->edges[i].curve.v == second.solids[0]->edges[i].curve.v);
        }
        first.solids.clear();
        CheckNative(*second.solids[0], 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/source_record_order_independence", [&]() {
        std::istringstream input(cube);
        std::vector<std::string> records;
        std::string line;
        while(std::getline(input, line)) records.push_back(line);
        std::reverse(records.begin(), records.end());
        std::string reordered;
        for(const auto &record : records) reordered += record + "\n";
        auto result = Accepted(schema, reordered);
        REQUIRE(result.solids[0]->vertices.front().vertex == 30);
        REQUIRE(result.solids[0]->vertices.front().point == 10);
        REQUIRE(result.solids[0]->faces.front().face == 2600);
        REQUIRE(result.solids[0]->faces.front().surface.v == 2600);
        CheckNative(*result.solids[0], 6, 12, 8, 1000, 600);
    });
    Run(binding.protocol + "/repeated_success_failure_cleanup", [&]() {
        auto exercise = [&]() {
            auto good = Accepted(schema, cube);
            auto spline = Accepted(schema, Cube(1, {}, 0, true));
            Rejected(schema, ReplaceRecord(cube, 2503, "CYLINDRICAL_SURFACE('',#2403,10.)"));
            Rejected(schema, ReplaceRecord(cube, 2700, "CLOSED_SHELL('',(#2600,#2601,#2602,#2603,#2604))"));
        };
        exercise();
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtMemState before, after;
        _CrtMemCheckpoint(&before);
#endif
        for(int i = 0; i < 20; ++i) exercise();
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtMemCheckpoint(&after);
        REQUIRE(before.lCounts[_NORMAL_BLOCK] == after.lCounts[_NORMAL_BLOCK]);
        REQUIRE(before.lSizes[_NORMAL_BLOCK] == after.lSizes[_NORMAL_BLOCK]);
#endif
    });
}

#ifdef HAVE_STEP_IMPORT
struct LinkApp {
    LinkApp() {
        SS.Init();
        SS.showToolbar = false;
        SS.checkClosedContour = false;
        SS.GW.showPoints = SS.GW.showFaces = SS.GW.showShaded = true;
        SS.GW.showNormals = SS.GW.showWorkplanes = false;
    }
    ~LinkApp() {
        SS.GW.hoverList.Clear();
        SS.GW.ClearSelection();
        SK.Clear();
        SS.Clear();
        SS.saveFile.Clear();
    }
};

struct LinkTestFile {
    Platform::Path path;
    explicit LinkTestFile(const char *extension = "StP") {
        static unsigned serial = 0;
        auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        // Include spaces, Unicode and a mixed-case extension in the real path.
        path = Platform::Path::FromPortable(STEP_LINK_TEST_DIR).Join(
            ssprintf("step link c\xc3\xbc" "be %lld-%u.%s", (long long)now, ++serial, extension));
        path = path.Expand(/*fromCurrentDirectory=*/true);
        REQUIRE(!Platform::FileExists(path));
    }
    void Write(const std::string &text) { REQUIRE(Platform::WriteFile(path, text)); }
    ~LinkTestFile() { Platform::RemoveFile(path); }
};

hGroup MenuLink(const Platform::Path &path) {
    int before = SK.group.n;
    Group::MenuGroup(Command::GROUP_LINK, path);
    REQUIRE(SK.group.n == before + 1);
    auto hg = SS.GW.activeGroup;
    REQUIRE(SK.GetGroup(hg)->type == Group::Type::LINKED);
    REQUIRE(SK.GetGroup(hg)->linkFile.Equals(path));
    REQUIRE(SK.GetGroup(hg)->meshCombine == Group::CombineAs::ASSEMBLE);
    return hg;
}

void PlaceLink(hGroup hg, Vector translation = {}, Quaternion rotation = Quaternion::IDENTITY,
               double scale = 1) {
    double t[] = {translation.x, translation.y, translation.z};
    for(int i = 0; i < 3; ++i) SK.GetParam(hg.param(i))->val = t[i];
    double q[] = {rotation.w, rotation.vx, rotation.vy, rotation.vz};
    for(int i = 0; i < 4; ++i) SK.GetParam(hg.param(3 + i))->val = q[i];
    SK.GetGroup(hg)->scale = scale;
    SS.MarkGroupDirty(hg);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
}

hEntity LinkedReference(hGroup hg, int kind, int source = 0) {
    auto g = SK.GetGroup(hg);
    for(const auto &entry : g->stepRefs) {
        if(entry.first.kind != kind || (source != 0 && entry.first.entity != source)) continue;
        auto mapped = g->remap.find({entry.second, 0});
        if(mapped == g->remap.end()) continue;
        auto h = hg.entity(mapped->second.v);
        if(SK.entity.FindByIdNoOops(h)) return h;
    }
    REQUIRE(false);
    return {};
}

void CheckLinkedNative(hGroup hg, int solids, double volume) {
    auto g = SK.GetGroup(hg);
    REQUIRE(g->IsSolvedOkay());
    REQUIRE(g->linkParams.empty() && !g->forceToMesh);
    REQUIRE(g->impMesh.IsEmpty() && g->thisMesh.IsEmpty() && g->runningMesh.IsEmpty());
    REQUIRE(!g->thisShell.booleanFailed && !g->runningShell.booleanFailed);
    REQUIRE(g->thisShell.surface.n == 6 * solids && g->thisShell.curve.n == 12 * solids);
    REQUIRE(g->impEntity.n == 26 * solids);
    int points = 0, lines = 0, faces = 0;
    for(auto &e : SK.entity) {
        if(e.group != hg) continue;
        REQUIRE(e.Owner() == &SK && !e.forceHidden);
        if(e.IsPoint()) {
            ++points;
            REQUIRE(e.type == Entity::Type::POINT_N_ROT_TRANS);
            REQUIRE(e.PointGetExprs().Eval().Equals(e.PointGetNum()));
        } else if(e.type == Entity::Type::LINE_SEGMENT) {
            ++lines;
            REQUIRE(SK.GetEntity(e.point[0])->group == hg);
            REQUIRE(SK.GetEntity(e.point[1])->group == hg);
        } else if(e.IsFace()) {
            ++faces;
            REQUIRE(e.type == Entity::Type::FACE_N_ROT_TRANS);
            REQUIRE(SK.GetEntity(e.point[0])->group == hg);
            REQUIRE(e.FaceGetPointExprs().Eval().Equals(e.FaceGetPointNum()));
            REQUIRE(e.FaceGetNormalExprs().Eval().Equals(e.FaceGetNormalNum()));
            REQUIRE(e.FaceGetPointNum().Equals(SK.GetEntity(e.point[0])->PointGetNum()));
        } else REQUIRE(false);
    }
    REQUIRE(points == 8 * solids && lines == 12 * solids && faces == 6 * solids);
    for(auto &s : g->thisShell.surface) {
        auto face = SK.entity.FindByIdNoOops(hEntity{s.face});
        REQUIRE(face && face->group == hg && face->IsFace());
        REQUIRE(s.NormalAt(0.5, 0.5).WithMagnitude(1).Equals(face->FaceGetNormalNum()));
        REQUIRE(std::abs(s.PointAt(0.5, 0.5).Minus(face->FaceGetPointNum()).Dot(
                            face->FaceGetNormalNum())) < LENGTH_EPS);
        for(const auto &trim : s.trim) REQUIRE(g->thisShell.curve.FindByIdNoOops(trim.curve));
    }
    for(auto &c : g->thisShell.curve) {
        REQUIRE(c.isExact && c.exact.deg == 1);
        auto line = SK.entity.FindByIdNoOops(hEntity{c.exact.entity});
        REQUIRE(line && line->group == hg && line->type == Entity::Type::LINE_SEGMENT);
        REQUIRE(c.exact.Start().Equals(SK.GetEntity(line->point[0])->PointGetNum()));
        REQUIRE(c.exact.Finish().Equals(SK.GetEntity(line->point[1])->PointGetNum()));
        REQUIRE(g->thisShell.surface.FindByIdNoOops(c.surfA));
        REQUIRE(g->thisShell.surface.FindByIdNoOops(c.surfB));
    }
    MeshOwner mesh;
    g->thisShell.TriangulateInto(&mesh.mesh);
    REQUIRE(std::abs(mesh.mesh.CalculateVolume() - volume) < 1e-6);
    for(const auto &t : mesh.mesh.l) {
        auto face = SK.entity.FindByIdNoOops(hEntity{t.meta.face});
        REQUIRE(face && face->IsFace() && face->group == hg);
    }
}

hEntity LinkDatum(hGroup hg, Vector point) {
    Request r = {};
    r.type = Request::Type::DATUM_POINT;
    r.workplane = Entity::FREE_IN_3D;
    r.group = hg;
    SK.request.AddAndAssignId(&r);
    SS.MarkGroupDirty(hg);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    SK.GetEntity(r.h.entity(0))->PointForceTo(point);
    return r.h.entity(0);
}

hConstraint ConstrainLinked(hEntity a, hEntity b, Command command = Command::ON_ENTITY) {
    SS.GW.ClearSelection();
    SS.GW.MakeSelected(a);
    SS.GW.MakeSelected(b);
    int before = SK.constraint.n;
    Constraint::MenuConstrain(command);
    REQUIRE(SK.constraint.n == before + 1);
    auto hc = SK.constraint[SK.constraint.n - 1].h;
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    REQUIRE(SK.GetGroup(SS.GW.activeGroup)->IsSolvedOkay());
    return hc;
}

void LinkPicking(hGroup hg, double size) {
    SS.GW.scale = 100 / size;
    SS.GW.projRight = {1, 0, 0};
    SS.GW.projUp = {0, 1, 0};
    SS.GW.offset = {};
    SK.GetGroup(hg)->GenerateDisplayItems();
    const Vector locations[] = {{size, size, size}, {size / 2, 0, size},
                                {size / 2, size / 2, size}};
    for(int i = 0; i < 3; ++i) {
        SS.GW.hoverList.Clear();
        SS.GW.HitTestMakeSelection(SS.GW.ProjectPoint(locations[i]));
        REQUIRE(SS.GW.hover.entity.v != 0);
        auto e = SK.GetEntity(SS.GW.hover.entity);
        REQUIRE(e->group == hg);
        REQUIRE(i == 0 ? e->IsPoint() : (i == 1 ? e->type == Entity::Type::LINE_SEGMENT : e->IsFace()));
    }
    SS.GW.hoverList.Clear();
    SS.GW.MakeSelected(LinkedReference(hg, StepEntityKey::VERTEX));
    SS.GW.MakeSelected(LinkedReference(hg, StepEntityKey::EDGE));
    SS.GW.MakeSelected(LinkedReference(hg, StepEntityKey::FACE));
    SS.GW.GroupSelection();
    REQUIRE(SS.GW.gs.points == 1 && SS.GW.gs.lineSegments == 1 && SS.GW.gs.faces == 1);
    SS.GW.ClearSelection();
}

void LinkTests(const Step::BindingInfo &binding) {
    const std::string name = "link/" + binding.protocol + "/", schema = binding.schema;
    Run(name + "menu_dispatch_picking_and_transformed_references", [&]() {
        LinkApp app;
        LinkTestFile file;
        file.Write(Exchange(schema, Cube()));
        auto hg = MenuLink(file.path);
        PlaceLink(hg);
        CheckLinkedNative(hg, 1, 1000);
        REQUIRE(SS.linkedStepFiles.size() == 1 && SS.linkedSketchFiles.empty());
        REQUIRE(SK.GetGroup(hg)->stepRefs.size() == 26 && SK.GetGroup(hg)->remap.size() == 26);
        LinkPicking(hg, 10);
        auto point = LinkedReference(hg, StepEntityKey::VERTEX);
        auto face = LinkedReference(hg, StepEntityKey::FACE);
        Vector originalPoint = SK.GetEntity(point)->PointGetNum();
        Vector originalNormal = SK.GetEntity(face)->FaceGetNormalNum();
        Quaternion q = Quaternion::From(Vector{1, 2, 3}.WithMagnitude(1), 0.7);
        Vector t = {23, -17, 5};
        PlaceLink(hg, t, q, 2);
        REQUIRE(SK.GetEntity(point)->PointGetNum().Equals(q.Rotate(originalPoint.ScaledBy(2)).Plus(t)));
        REQUIRE(SK.GetEntity(face)->FaceGetNormalNum().Equals(q.Rotate(originalNormal)));
        CheckLinkedNative(hg, 1, 8000);
        PlaceLink(hg, t, q, -1);
        CheckLinkedNative(hg, 1, 1000);
    });
    Run(name + "two_instances_and_native_boolean", [&]() {
        LinkApp app;
        LinkTestFile file;
        file.Write(Exchange(schema, Cube(1)));
        auto a = MenuLink(file.path);
        PlaceLink(a);
        auto point = LinkedReference(a, StepEntityKey::VERTEX);
        Vector unchanged = SK.GetEntity(point)->PointGetNum();
        auto b = MenuLink(file.path);
        PlaceLink(b, {0.5, 0, 0});
        CheckLinkedNative(a, 1, 1);
        CheckLinkedNative(b, 1, 1);
        REQUIRE(LinkedReference(b, StepEntityKey::VERTEX) != point);
        REQUIRE(SK.GetEntity(point)->PointGetNum().Equals(unchanged));
        REQUIRE(SS.linkedStepFiles.size() == 1);
        REQUIRE(SK.GetGroup(a)->impShell.surface[0].trim.First() !=
                SK.GetGroup(b)->impShell.surface[0].trim.First());
        SK.GetGroup(b)->meshCombine = Group::CombineAs::UNION;
        SS.MarkGroupDirty(b);
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        auto result = SK.GetGroup(b);
        REQUIRE(result->runningMesh.IsEmpty() && !result->runningShell.booleanFailed);
        MeshOwner mesh;
        result->runningShell.TriangulateInto(&mesh.mesh);
        REQUIRE(std::abs(mesh.mesh.CalculateVolume() - 1.5) < 1e-8);
    });
    Run(name + "point_edge_face_constraints_and_reference_roundtrip", [&]() {
        LinkApp app;
        LinkTestFile file, saved("slvs");
        file.Write(Exchange(schema, Cube()));
        auto linked = MenuLink(file.path);
        PlaceLink(linked);
        Group::MenuGroup(Command::GROUP_3D);
        auto drawing = SS.GW.activeGroup;
        auto vertex = LinkedReference(linked, StepEntityKey::VERTEX);
        auto edge = LinkedReference(linked, StepEntityKey::EDGE);
        auto face = LinkedReference(linked, StepEntityKey::FACE);
        auto p = LinkDatum(drawing, {2, 3, 4});
        auto pc = ConstrainLinked(p, vertex);
        REQUIRE(SK.GetConstraint(pc)->type == Constraint::Type::POINTS_COINCIDENT);
        REQUIRE(SK.GetEntity(p)->PointGetNum().Equals(SK.GetEntity(vertex)->PointGetNum()));
        auto q = LinkDatum(drawing, {4, 4, 4});
        auto lc = ConstrainLinked(q, edge);
        REQUIRE(SK.GetConstraint(lc)->type == Constraint::Type::PT_ON_LINE);
        Vector line = SK.GetEntity(edge)->VectorGetNum().WithMagnitude(1);
        REQUIRE(SK.GetEntity(q)->PointGetNum().Minus(SK.GetEntity(
            SK.GetEntity(edge)->point[0])->PointGetNum()).Cross(line).Magnitude() < LENGTH_EPS);
        auto r = LinkDatum(drawing, {5, 5, 5});
        auto fc = ConstrainLinked(r, face);
        REQUIRE(SK.GetConstraint(fc)->type == Constraint::Type::PT_ON_FACE);
        REQUIRE(std::abs(SK.GetEntity(r)->PointGetNum().Minus(SK.GetEntity(face)->FaceGetPointNum())
                         .Dot(SK.GetEntity(face)->FaceGetNormalNum())) < LENGTH_EPS);
        REQUIRE(SK.GetGroup(drawing)->solved.dof == 3);
        auto identities = SK.GetGroup(linked)->stepRefs;
        REQUIRE(SS.SaveToFile(saved.path));
        REQUIRE(SS.LoadFromFile(saved.path));
        SS.AfterNewFile();
        REQUIRE(!SS.fileLoadError);
        REQUIRE(SK.GetGroup(linked)->stepRefs == identities);
        REQUIRE(SK.constraint.n == 3 && SK.GetGroup(drawing)->IsSolvedOkay());
        REQUIRE(SK.GetConstraint(lc)->entityA == edge && SK.GetConstraint(fc)->entityA == face);
        REQUIRE(SK.GetEntity(p)->PointGetNum().Equals(SK.GetEntity(vertex)->PointGetNum()));
        SS.UndoRemember();
        PlaceLink(linked, {17, 11, -3});
        SS.UndoUndo();
        REQUIRE(SK.GetGroup(linked)->stepRefs == identities);
        REQUIRE(SK.GetEntity(p)->PointGetNum().Equals(SK.GetEntity(vertex)->PointGetNum()));
        SS.UndoRedo();
        REQUIRE(SK.GetGroup(linked)->stepRefs == identities);
        REQUIRE(SK.GetEntity(p)->PointGetNum().Equals(SK.GetEntity(vertex)->PointGetNum()));
    });
    Run(name + "constrain_link_placement_to_another_instance", [&]() {
        LinkApp app;
        LinkTestFile file;
        file.Write(Exchange(schema, Cube()));
        auto a = MenuLink(file.path);
        PlaceLink(a);
        auto av = LinkedReference(a, StepEntityKey::VERTEX);
        auto af = LinkedReference(a, StepEntityKey::FACE);
        Vector fixed = SK.GetEntity(av)->PointGetNum();
        auto b = MenuLink(file.path);
        PlaceLink(b, {7, 9, -3}, Quaternion::From(Vector{1, 2, 3}.WithMagnitude(1), 0.3));
        auto bv = LinkedReference(b, StepEntityKey::VERTEX);
        auto bf = LinkedReference(b, StepEntityKey::FACE);
        ConstrainLinked(bv, av);
        ConstrainLinked(bf, af, Command::PARALLEL);
        REQUIRE(SK.GetGroup(b)->solved.dof == 1);
        REQUIRE(SK.GetEntity(bv)->PointGetNum().Equals(fixed));
        REQUIRE(std::abs(SK.GetEntity(bf)->FaceGetNormalNum().Dot(
                             SK.GetEntity(af)->FaceGetNormalNum())) > 1 - 1e-8);
        REQUIRE(SK.GetEntity(av)->PointGetNum().Equals(fixed));
        CheckLinkedNative(b, 1, 1000);
    });
    Run(name + "multiple_complete_solids_in_one_group", [&]() {
        LinkApp app;
        LinkTestFile file("step");
        file.Write(Exchange(schema, Cube() + Cube(10, {20, 0, 0}, 10000)));
        auto hg = MenuLink(file.path);
        PlaceLink(hg);
        CheckLinkedNative(hg, 2, 2000);
        REQUIRE(SK.group.n == 3 && SS.linkedStepFiles[file.path].solids == 2);
    });
    Run(name + "whole_solid_omissions_not_face_stripping", [&]() {
        LinkApp app;
        LinkTestFile file;
        auto bad = ReplaceRecord(Cube(10, {20, 0, 0}, 10000), 12503,
                                 "CYLINDRICAL_SURFACE('',#12403,10.)");
        file.Write(Exchange(schema, Cube() + bad));
        auto hg = MenuLink(file.path);
        PlaceLink(hg);
        CheckLinkedNative(hg, 1, 1000);
        auto &cache = SS.linkedStepFiles[file.path];
        REQUIRE(cache.status == SolveSpaceUI::LinkedStepFile::STEPStatus::Partial);
        REQUIRE(cache.solids == 1 && cache.omitted == 1);
        REQUIRE(cache.diagnostics.find("12701") != std::string::npos);
        REQUIRE(!cache.pendingDiagnostic);
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(!SS.linkedStepFiles[file.path].pendingDiagnostic);
    });
    Run(name + "invalid_unsupported_empty_and_missing_are_distinct", [&]() {
        LinkApp app;
        LinkTestFile file;
        int groups = SK.group.n, undo = SS.undo.cnt;
        const auto rejected = SolveSpaceUI::LinkedStepFile::STEPStatus::Rejected;
        for(const auto &text : {Exchange(schema, ""), Exchange("UNKNOWN_SCHEMA", Cube()),
             Exchange(schema, "#1=NOT_AN_ENTITY();"), Exchange(schema,
                 ReplaceRecord(Cube(), 2503, "CYLINDRICAL_SURFACE('',#2403,10.)"))}) {
            file.Write(text);
            Group::MenuGroup(Command::GROUP_LINK, file.path);
            REQUIRE(SK.group.n == groups && SS.undo.cnt == undo);
            REQUIRE(SS.linkedStepFiles[file.path].status == rejected);
            REQUIRE(SS.linkedStepFiles[file.path].entities.IsEmpty());
            REQUIRE(SS.linkedStepFiles[file.path].shell.IsEmpty());
            REQUIRE(!SS.linkedStepFiles[file.path].diagnostics.empty());
            REQUIRE(SS.linkedSketchFiles.empty());
        }
        Platform::RemoveFile(file.path);
        Group::MenuGroup(Command::GROUP_LINK, file.path);
        REQUIRE(SK.group.n == groups && SS.undo.cnt == undo);
        REQUIRE(SS.linkedStepFiles[file.path].status == SolveSpaceUI::LinkedStepFile::STEPStatus::Missing);
    });
    Run(name + "cache_reload_provenance_and_no_reference_recycling", [&]() {
        LinkApp app;
        LinkTestFile file;
        auto original = Exchange(schema, Cube(10, {}, 10000));
        file.Write(original);
        auto hg = MenuLink(file.path);
        PlaceLink(hg);
        auto old = LinkedReference(hg, StepEntityKey::VERTEX, 10030);
        auto identities = SK.GetGroup(hg)->stepRefs;
        std::istringstream records(Cube(10, {}, 10000));
        std::vector<std::string> lines;
        for(std::string line; std::getline(records, line);) lines.push_back(line);
        std::reverse(lines.begin(), lines.end());
        std::string reordered;
        for(const auto &line : lines) reordered += line + "\n";
        file.Write(Exchange(schema, reordered));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile));
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(SK.GetGroup(hg)->stepRefs == identities);
        REQUIRE(LinkedReference(hg, StepEntityKey::VERTEX, 10030) == old);
        Platform::RemoveFile(file.path);
        REQUIRE(SS.LoadLinkedGroup(SK.GetGroup(hg), {}, /*checkFiles=*/false));
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(SK.entity.FindByIdNoOops(old)); // generation never rereads the file
        file.Write(Exchange(schema, Cube() + Cube(10, {}, 10000)));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile));
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(LinkedReference(hg, StepEntityKey::VERTEX, 10030) == old);
        for(const auto &entry : identities) {
            REQUIRE(SK.GetGroup(hg)->stepRefs.at(entry.first) == entry.second);
        }
        REQUIRE(SK.GetGroup(hg)->impEntity.n == 52);
        Group::MenuGroup(Command::GROUP_3D);
        auto p = LinkDatum(SS.GW.activeGroup, {0, 0, 0});
        auto c = ConstrainLinked(p, old);
        file.Write(Exchange(schema, Cube())); // older solid/topology disappeared
        REQUIRE(SS.ReloadAllLinked(SS.saveFile));
        REQUIRE(!SS.pendingStepLinkWarnings.empty());
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(!SK.entity.FindByIdNoOops(old));
        REQUIRE(!SK.constraint.FindByIdNoOops(c));
        REQUIRE(LinkedReference(hg, StepEntityKey::VERTEX) != old);
        REQUIRE(SK.GetGroup(hg)->stepRefs.size() == 52); // tombstones, never recycled
    });
    Run(name + "reference_limit_rejected_before_generate", [&]() {
        LinkApp app;
        LinkTestFile file;
        file.Write(Exchange(schema, Cube()));
        auto hg = MenuLink(file.path);
        auto g = SK.GetGroup(hg);
        g->stepRefs.emplace(StepEntityKey{999999, 999999, StepEntityKey::VERTEX, 999999},
                            hEntity{65535});
        auto before = g->stepRefs.size();
        file.Write(Exchange(schema, Cube() + Cube(10, {20, 0, 0}, 10000)));
        REQUIRE(!SS.LoadLinkedGroup(g, {}, /*checkFiles=*/true));
        REQUIRE(g->stepRefs.size() == before && g->impEntity.IsEmpty() && g->impShell.IsEmpty());
        REQUIRE(!SS.pendingStepLinkErrors.empty());
        // The limit belongs to the old group's persistent namespace, not the file.
        Group fresh = {};
        fresh.linkFile = file.path;
        REQUIRE(SS.LoadLinkedGroup(&fresh, {}, /*checkFiles=*/false));
        REQUIRE(fresh.impEntity.n == 52);
        fresh.Clear();
        g->stepRefs.erase(StepEntityKey{999999, 999999, StepEntityKey::VERTEX, 999999, {}});
        g->remap.reserve(65535);
        while(g->remap.size() < 65535) {
            auto index = (uint32_t)g->remap.size() + 1;
            g->remap.emplace(EntityKey{hEntity{index}, 1}, EntityId{index});
        }
        // Also bound the ordinary remap, independently of source-token limits.
        REQUIRE(!SS.LoadLinkedGroup(g, {}, /*checkFiles=*/false));
        REQUIRE(g->impShell.IsEmpty() && g->impEntity.IsEmpty());
    });
    Run(name + "existing_rejected_source_is_not_a_missing_file_or_slvs", [&]() {
        LinkApp app;
        LinkTestFile file;
        auto good = Exchange(schema, Cube());
        file.Write(good);
        auto hg = MenuLink(file.path);
        PlaceLink(hg);
        auto point = LinkedReference(hg, StepEntityKey::VERTEX);
        file.Write(Exchange(schema, "#1=NOT_AN_ENTITY();"));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile));
        REQUIRE(SK.GetGroup(hg)->linkFile.Equals(file.path));
        REQUIRE(SK.GetGroup(hg)->impShell.IsEmpty() && SK.GetGroup(hg)->impEntity.IsEmpty());
        REQUIRE(SS.linkedStepFiles[file.path].status == SolveSpaceUI::LinkedStepFile::STEPStatus::Rejected);
        REQUIRE(SS.linkedStepFiles[file.path].pendingDiagnostic);
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(SK.group.FindByIdNoOops(hg) && !SK.entity.FindByIdNoOops(point));
        file.Write(good);
        REQUIRE(!SS.LoadLinkedGroup(SK.GetGroup(hg), {}, /*checkFiles=*/false));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile));
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(LinkedReference(hg, StepEntityKey::VERTEX) == point);
        CheckLinkedNative(hg, 1, 1000);
    });
    Run(name + "repurposed_source_edge_id_does_not_retarget_constraint", [&]() {
        LinkApp app;
        LinkTestFile file;
        file.Write(Exchange(schema, Cube()));
        auto hg = MenuLink(file.path);
        PlaceLink(hg);
        auto old = LinkedReference(hg, StepEntityKey::EDGE, 400);
        Group::MenuGroup(Command::GROUP_3D);
        auto p = LinkDatum(SS.GW.activeGroup, {5, 2, 1});
        auto c = ConstrainLinked(p, old);
        // Same cube, SAME edge IDs, but a replaced topological endpoint. New
        // connectivity must not inherit an existing edge's constraint identity.
        auto changed = Cube() + Record(50030, "VERTEX_POINT('',#10)");
        changed = ReplaceRecord(changed, 400, "EDGE_CURVE('',#50030,#31,#300,.T.)");
        changed = ReplaceRecord(changed, 401, "EDGE_CURVE('',#50030,#33,#301,.T.)");
        changed = ReplaceRecord(changed, 402, "EDGE_CURVE('',#50030,#34,#302,.T.)");
        file.Write(Exchange(schema, changed));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile));
        REQUIRE(!SS.pendingStepLinkWarnings.empty());
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(!SK.entity.FindByIdNoOops(old));
        REQUIRE(!SK.constraint.FindByIdNoOops(c));
        REQUIRE(LinkedReference(hg, StepEntityKey::EDGE, 400) != old);
        CheckLinkedNative(hg, 1, 1000);
    });
    Run(name + "nested_slvs_dependency_invalidation", [&]() {
        LinkApp app;
        LinkTestFile file, saved("slvs");
        file.Write(Exchange(schema, Cube(1)));
        auto inner = MenuLink(file.path);
        PlaceLink(inner);
        REQUIRE(SS.SaveToFile(saved.path));
        SS.NewFile();
        SS.AfterNewFile();
        auto outer = MenuLink(saved.path);
        PlaceLink(outer);
        REQUIRE(SS.linkedStepFiles.count(file.path) == 1);
        auto loaded = SS.linkedSketchFiles.find(saved.path);
        REQUIRE(loaded != SS.linkedSketchFiles.end() && loaded->second.sketch);
        bool dependency = false;
        for(const auto &made : loaded->second.madeFrom) dependency |= made.path.Equals(file.path);
        REQUIRE(dependency);
        file.Write(Exchange(schema, Cube(2)));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile));
        SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        auto g = SK.GetGroup(outer);
        REQUIRE(g->runningMesh.IsEmpty() && g->runningShell.surface.n == 6);
        MeshOwner mesh;
        g->runningShell.TriangulateInto(&mesh.mesh);
        REQUIRE(std::abs(mesh.mesh.CalculateVolume() - 8) < 1e-8);
    });
}

void ExternalLinkedUnitCube(const std::string &path) {
    LinkApp app;
    auto file = Platform::Path::From(path).Expand(/*fromCurrentDirectory=*/true);
    auto hg = MenuLink(file);
    PlaceLink(hg);
    CheckLinkedNative(hg, 1, 1);
    LinkPicking(hg, 1);
}
#endif

void CompareSourceCube(const std::string &path, Step::NativeSolid &solid) {
    // Validation only: regenerate a supplied source sketch in memory. Do not
    // re-export STEP, save/upgrade the fixture, or depend on its cached shell.
    struct SketchOwner {
        SketchOwner() { SS.Init(); }
        ~SketchOwner() { SK.Clear(); SS.Clear(); }
    } sketch;
    SS.showToolbar = false;
    SS.checkClosedContour = false;
    REQUIRE(SS.LoadFromFile(Platform::Path::From(path).Expand(/*fromCurrentDirectory=*/true)));
    SS.AfterNewFile(); // includes Generate::ALL, as in the existing test harness
    REQUIRE(!SS.fileLoadError && !SK.groupOrder.IsEmpty());
    for(const auto &group : SK.group) REQUIRE(group.IsSolvedOkay());
    Group *group = SK.GetRunningMeshGroupFor(*SK.groupOrder.Last());
    REQUIRE(group != nullptr);
    auto &source = group->runningShell;
    REQUIRE(group->runningMesh.l.IsEmpty());
    REQUIRE(!source.IsEmpty() && !source.booleanFailed);
    REQUIRE(source.surface.n == solid.shell.surface.n);
    REQUIRE(source.curve.n == solid.shell.curve.n);

    std::set<PointKey> vertices;
    for(const auto &curve : source.curve) {
        REQUIRE(curve.isExact && curve.exact.deg == 1);
        Vector a = curve.exact.Start(), b = curve.exact.Finish();
        vertices.insert(Key(a));
        vertices.insert(Key(b));
        int matches = 0;
        for(const auto &imported : solid.shell.curve) {
            Vector x = imported.exact.Start(), y = imported.exact.Finish();
            if((a.Equals(x) && b.Equals(y)) || (a.Equals(y) && b.Equals(x))) ++matches;
        }
        REQUIRE(matches == 1);
    }
    REQUIRE(vertices.size() == solid.vertices.size());
    for(const auto &vertex : solid.vertices) REQUIRE(vertices.count(Key(vertex.position)) == 1);
    for(const auto &surface : source.surface) {
        Vector point = surface.PointAt(0.5, 0.5);
        Vector normal = surface.NormalAt(0.5, 0.5).WithMagnitude(1);
        int matches = 0;
        for(const auto &face : solid.faces) {
            if(normal.Dot(face.normal) > 1 - 1e-12 &&
               std::abs(point.Minus(face.origin).Dot(face.normal)) < LENGTH_EPS) ++matches;
        }
        REQUIRE(matches == 1); // compare outward supporting planes, not face IDs
    }

    MeshOwner mesh;
    source.TriangulateInto(&mesh.mesh);
    Vector max = {VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE};
    Vector min = {VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE};
    double volume = 0, area = 0;
    for(const auto &triangle : mesh.mesh.l) {
        volume += triangle.SignedVolume();
        area += triangle.Area();
        for(const auto &point : triangle.vertices) point.MakeMaxMin(&max, &min);
    }
    REQUIRE(min.Equals({0, 0, 0}) && max.Equals({1, 1, 1}));
    REQUIRE(std::abs(volume - 1) < 1e-8);
    REQUIRE(std::abs(area - 6) < 1e-8);
}

void ExternalUnitCube(const std::string &path, const std::string &source) {
    std::string text;
    REQUIRE(Platform::ReadFile(Platform::Path::From(path), &text));
    auto document = Read(text);
    auto result = Step::ConvertGeometry(*document);
    document.reset();
    if(result.status != Status::Okay) PrintDiagnostics(result);
    REQUIRE(result.status == Status::Okay && result.diagnostics.empty());
    REQUIRE(result.solids.size() == 1);
    CheckUnitCube(*result.solids.front());
    ShellOwner shifted, combined;
    shifted.shell.MakeFromTransformationOf(&result.solids[0]->shell, {0.5, 0, 0},
                                            Quaternion::IDENTITY, 1.0);
    combined.shell.MakeFromUnionOf(&result.solids[0]->shell, &shifted.shell);
    REQUIRE(!combined.shell.booleanFailed && !combined.shell.IsEmpty());
    MeshOwner mesh;
    combined.shell.TriangulateInto(&mesh.mesh);
    double volume = 0, area = 0;
    for(const auto &triangle : mesh.mesh.l) {
        volume += triangle.SignedVolume();
        area += triangle.Area();
    }
    REQUIRE(std::abs(volume - 1.5) < 1e-8);
    REQUIRE(std::abs(area - 8) < 1e-8);
    if(!source.empty()) CompareSourceCube(source, *result.solids[0]);
}

struct FineTessellation {
    double chord = SS.chordTolCalculated;
    int segments = SS.maxSegments;
    bool exporting = SS.exportMode;
    FineTessellation() { SS.chordTolCalculated = 1e-4; SS.maxSegments = 2048; SS.exportMode = false; }
    ~FineTessellation() { SS.chordTolCalculated = chord; SS.maxSegments = segments; SS.exportMode = exporting; }
};

void CompareSourceCylinder(const std::string &path, Step::NativeSolid &solid) {
    struct SketchOwner {
        SketchOwner() { SS.Init(); SS.showToolbar = false; SS.checkClosedContour = false; }
        ~SketchOwner() { SK.Clear(); SS.Clear(); }
    } sketch;
    REQUIRE(SS.LoadFromFile(Platform::Path::From(path).Expand(true)));
    SS.AfterNewFile();
    REQUIRE(!SS.fileLoadError && !SK.groupOrder.IsEmpty());
    for(const auto &group : SK.group) REQUIRE(group.IsSolvedOkay());
    auto group = SK.GetRunningMeshGroupFor(*SK.groupOrder.Last());
    REQUIRE(group && group->runningMesh.IsEmpty());
    auto &source = group->runningShell;
    REQUIRE(!source.booleanFailed && source.surface.n == 6 && source.curve.n == 12);
    std::set<PointKey> vertices;
    for(const auto &curve : source.curve) {
        REQUIRE(curve.isExact);
        vertices.insert(Key(curve.exact.Start())); vertices.insert(Key(curve.exact.Finish()));
        int matches = 0;
        for(const auto &imported : solid.shell.curve) {
            Vector a = curve.exact.Start(), b = curve.exact.Finish();
            Vector x = imported.exact.Start(), y = imported.exact.Finish();
            if(!((a.Equals(x) && b.Equals(y)) || (a.Equals(y) && b.Equals(x)))) continue;
            for(int i = 0; i <= 16; ++i) REQUIRE(!Step::Nurbs::Parameters(imported.exact,
                curve.exact.PointAt(i / 16.0), LENGTH_EPS * 0.1).empty());
            ++matches;
        }
        REQUIRE(matches == 1);
    }
    REQUIRE(vertices.size() == solid.vertices.size());
    for(const auto &vertex : solid.vertices) REQUIRE(vertices.count(Key(vertex.position)) == 1);
    for(const auto &surface : source.surface) {
        int matches = 0;
        for(auto &imported : solid.shell.surface) {
            bool matchesAll = true;
            for(double u : {0.125, 0.375, 0.625, 0.875}) for(double v : {0.125, 0.375, 0.625, 0.875}) {
                Vector point = surface.PointAt(u, v);
                Point2d uv;
                imported.ClosestPointTo(point, &uv);
                if(!imported.PointAt(uv).Equals(point, LENGTH_EPS * 0.1) ||
                   imported.NormalAt(uv).WithMagnitude(1).Dot(surface.NormalAt(u, v).WithMagnitude(1)) < 1 - 1e-10) {
                    matchesAll = false;
                }
            }
            if(matchesAll) ++matches;
        }
        REQUIRE(matches == 1);
    }
    // Regenerated source, then a disposable native copy with finer trim PWLs
    // for measurement. Never save, re-export, or alter the supplied sketch.
    FineTessellation fine;
    ShellOwner measured;
    measured.shell.MakeFromCopyOf(&source);
    for(auto &curve : measured.shell.curve) {
        curve.pts.Clear(); curve.exact.MakePwlInto(&curve.pts, 1e-4, 0.03125);
    }
    double area;
    REQUIRE(std::abs(TriangulatedVolume(measured.shell, &area) - 31.25 * PI) < 0.01);
    REQUIRE(std::abs(area - 37.5 * PI) < 0.01);
}

void ExternalCylinder(const std::string &path, const std::string &source) {
    FineTessellation fine;
    std::string text;
    REQUIRE(Platform::ReadFile(Platform::Path::From(path), &text));
    auto document = Read(text);
    Step::GeometryOptions options;
    options.chordTolerance = 1e-4; options.maxSegments = 2048;
    auto result = Step::ConvertGeometry(*document, options);
    document.reset();
    if(result.status != Status::Okay) PrintDiagnostics(result);
    REQUIRE(result.status == Status::Okay && result.solids.size() == 1 && result.diagnostics.empty());
    auto &solid = *result.solids[0];
    REQUIRE(solid.shell.surface.n == 6 && solid.shell.curve.n == 12);
    REQUIRE(solid.lengthToMm == 1);
    CheckGeneralNative(solid, 6, 12, 8, 31.25 * PI, 0.01);
    Vector min = {VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE}, max = min.Negated();
    for(const auto &vertex : solid.vertices) vertex.position.MakeMaxMin(&max, &min);
    REQUIRE(min.Equals({-2.5, -2.5, 0}) && max.Equals({2.5, 2.5, 5}));
    int lines = 0, planes = 0, arcs = 0;
    for(const auto &edge : solid.edges) lines += edge.linear ? 1 : 0;
    for(const auto &face : solid.faces) planes += face.planar ? 1 : 0;
    for(const auto &curve : solid.shell.curve) if(curve.exact.deg == 2) {
        REQUIRE(curve.exact.weight[0] == 1 && curve.exact.weight[1] == 0.7071067812 && curve.exact.weight[2] == 1);
        ++arcs;
    }
    REQUIRE(lines == 4 && planes == 2 && arcs == 8);
    auto arc = solid.shell.curve.FindById(hSCurve{21});
    REQUIRE(arc->exact.ctrl[1].Equals({-2.5, -2.5, 0}, 1e-14));
    double area;
    REQUIRE(std::abs(TriangulatedVolume(solid.shell, &area) - 31.25 * PI) < 0.01);
    REQUIRE(std::abs(area - 37.5 * PI) < 0.01);
    ShellOwner shifted, combined;
    shifted.shell.MakeFromTransformationOf(&solid.shell, {2.5, 0, 0}, Quaternion::IDENTITY, 1);
    combined.shell.MakeFromUnionOf(&solid.shell, &shifted.shell);
    REQUIRE(!combined.shell.booleanFailed && !combined.shell.IsEmpty());
    double diskUnion = 2 * PI * 2.5 * 2.5 - (2 * 2.5 * 2.5 * std::acos(0.5) - 1.25 * std::sqrt(18.75));
    REQUIRE(std::abs(TriangulatedVolume(combined.shell, &area) - 5 * diskUnion) < 0.03);
    REQUIRE(std::abs(area - (2 * diskUnion + 5 * (4 * 2.5 * (PI - std::acos(0.5))))) < 0.03);
    if(!source.empty()) CompareSourceCylinder(source, solid);
}

#ifdef HAVE_STEP_IMPORT
void CheckCurvedLink(hGroup hg, int references, double volume, double tolerance) {
    auto group = SK.GetGroup(hg);
    REQUIRE(group->IsSolvedOkay() && group->impEntity.n == references);
    REQUIRE(group->linkParams.empty() && !group->forceToMesh);
    REQUIRE(group->impMesh.IsEmpty() && group->thisMesh.IsEmpty() && group->runningMesh.IsEmpty());
    REQUIRE(!group->thisShell.booleanFailed && !group->thisShell.IsEmpty());
    for(auto &surface : group->thisShell.surface) {
        if(surface.face != 0) {
            auto face = SK.entity.FindByIdNoOops(hEntity{surface.face});
            REQUIRE(face && face->IsFace() && face->group == hg && surface.IsPlanar());
            REQUIRE(surface.NormalAt(0.5, 0.5).WithMagnitude(1).Equals(face->FaceGetNormalNum()));
        } else REQUIRE(!surface.IsPlanar());
    }
    for(auto &curve : group->thisShell.curve) {
        REQUIRE(curve.isExact);
        if(curve.exact.entity != 0) {
            auto edge = SK.entity.FindByIdNoOops(hEntity{curve.exact.entity});
            REQUIRE(edge && edge->type == Entity::Type::LINE_SEGMENT && edge->group == hg);
            Vector a = SK.GetEntity(edge->point[0])->PointGetNum(), b = SK.GetEntity(edge->point[1])->PointGetNum();
            for(int i = 0; i <= curve.exact.deg; ++i) REQUIRE(curve.exact.ctrl[i].DistanceToLine(a, b.Minus(a)) < LENGTH_EPS);
        }
    }
    REQUIRE(std::abs(TriangulatedVolume(group->thisShell) - volume) < tolerance);
}

void CurvedLinkTests(const Step::BindingInfo &binding) {
    const auto name = "link/" + binding.protocol + "/nurbs_";
    const auto &schema = binding.schema;
    Run(name + "truthful_references_placement_constraints_and_roundtrip", [&]() {
        LinkApp app;
        LinkTestFile file, saved("slvs");
        auto outline = FreeformOutline();
        double volume = OutlineArea(outline) * 2 * (1 + 0.8 + 0.64) / 3;
        file.Write(Exchange(schema, BezierLoft(outline)));
        auto hg = MenuLink(file.path); PlaceLink(hg);
        CheckCurvedLink(hg, 14, volume, 0.05);
        REQUIRE(SK.GetGroup(hg)->stepRefs.size() == 14);
        auto identities = SK.GetGroup(hg)->stepRefs;
        auto vertex = LinkedReference(hg, StepEntityKey::VERTEX);
        auto edge = LinkedReference(hg, StepEntityKey::EDGE);
        auto face = LinkedReference(hg, StepEntityKey::FACE);
        Group::MenuGroup(Command::GROUP_3D);
        auto drawing = SS.GW.activeGroup;
        auto point = LinkDatum(drawing, {1, 2, 3});
        ConstrainLinked(point, vertex);
        auto onEdge = LinkDatum(drawing, {1, 2, 3}); ConstrainLinked(onEdge, edge);
        auto onFace = LinkDatum(drawing, {1, 2, 3}); ConstrainLinked(onFace, face);
        REQUIRE(SS.SaveToFile(saved.path));
        REQUIRE(SS.LoadFromFile(saved.path)); SS.AfterNewFile();
        REQUIRE(!SS.fileLoadError && SK.GetGroup(hg)->stepRefs == identities && SK.constraint.n == 3);
        REQUIRE(SK.GetGroup(drawing)->IsSolvedOkay());
        SS.UndoRemember();
        PlaceLink(hg, {3, -2, 7}, Quaternion::From(Vector{1, 2, 3}.WithMagnitude(1), 0.4), 2);
        CheckCurvedLink(hg, 14, volume * 8, 0.4);
        REQUIRE(SK.GetEntity(point)->PointGetNum().Equals(SK.GetEntity(vertex)->PointGetNum()));
        SS.UndoUndo();
        CheckCurvedLink(hg, 14, volume, 0.05);
        SS.UndoRedo();
        REQUIRE(SK.GetGroup(hg)->stepRefs == identities);
    });
    Run(name + "artificial_patch_seams_do_not_change_source_identities", [&]() {
        LinkApp app;
        LinkTestFile file;
        file.Write(Exchange(schema, Cube(1)));
        auto hg = MenuLink(file.path); PlaceLink(hg);
        auto identities = SK.GetGroup(hg)->stepRefs;
        auto edge = LinkedReference(hg, StepEntityKey::EDGE, 408);
        auto face = LinkedReference(hg, StepEntityKey::FACE, 2601);
        file.Write(Exchange(schema, SplitPlaneBox()));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile)); SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(SK.GetGroup(hg)->stepRefs == identities);
        REQUIRE(LinkedReference(hg, StepEntityKey::EDGE, 408) == edge);
        REQUIRE(LinkedReference(hg, StepEntityKey::FACE, 2601) == face);
        REQUIRE(SK.GetGroup(hg)->thisShell.surface.n == 9 && SK.GetGroup(hg)->thisShell.curve.n == 20);
        REQUIRE(SK.GetGroup(hg)->impEntity.n == 26);
        for(const auto &surface : SK.GetGroup(hg)->thisShell.surface) REQUIRE(surface.face != 0);
        int artificial = 0;
        for(const auto &curve : SK.GetGroup(hg)->thisShell.curve) artificial += curve.exact.entity == 0 ? 1 : 0;
        REQUIRE(artificial == 4);
    });
    Run(name + "plane_becoming_curved_does_not_retarget_plane_constraint", [&]() {
        LinkApp app;
        LinkTestFile file;
        file.Write(Exchange(schema, Cube(1)));
        auto hg = MenuLink(file.path); PlaceLink(hg);
        auto oldFace = LinkedReference(hg, StepEntityKey::FACE, 2601);
        auto edge = LinkedReference(hg, StepEntityKey::EDGE, 408);
        Group::MenuGroup(Command::GROUP_3D);
        auto point = LinkDatum(SS.GW.activeGroup, {0.5, 0.5, 1});
        auto constraint = ConstrainLinked(point, oldFace);
        file.Write(Exchange(schema, FreeformBox()));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile)); SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(!SK.entity.FindByIdNoOops(oldFace) && !SK.constraint.FindByIdNoOops(constraint));
        REQUIRE(SK.GetGroup(hg)->impEntity.n == 25 && SK.GetGroup(hg)->stepRefs.size() == 26);
        REQUIRE(LinkedReference(hg, StepEntityKey::EDGE, 408) == edge);
        auto identities = SK.GetGroup(hg)->stepRefs;
        file.Write(Exchange(schema, FreeformBox(false, true)));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile)); SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(SK.GetGroup(hg)->stepRefs == identities && LinkedReference(hg, StepEntityKey::EDGE, 408) == edge);
        REQUIRE(SK.GetGroup(hg)->thisShell.surface.n == 7);
    });
    Run(name + "line_becoming_curved_retires_line_reference_not_its_vertices", [&]() {
        LinkApp app;
        LinkTestFile file;
        std::vector<SBezier> outline = {
            SBezier::From(Vector{1, 0, 0}, Vector{0, 1, 0}),
            SBezier::From(Vector{0, 1, 0}, Vector{-1, 0, 0}),
            SBezier::From(Vector{-1, 0, 0}, Vector{0, -1, 0}),
            SBezier::From(Vector{0, -1, 0}, Vector{1, 0, 0})
        };
        file.Write(Exchange(schema, BezierLoft(outline)));
        auto hg = MenuLink(file.path); PlaceLink(hg);
        REQUIRE(SK.GetGroup(hg)->impEntity.n == 26);
        auto vertex = LinkedReference(hg, StepEntityKey::VERTEX, 30);
        auto oldEdge = LinkedReference(hg, StepEntityKey::EDGE, 400);
        auto otherEdge = LinkedReference(hg, StepEntityKey::EDGE, 401);
        auto identities = SK.GetGroup(hg)->stepRefs;
        Group::MenuGroup(Command::GROUP_3D);
        auto point = LinkDatum(SS.GW.activeGroup, {0.5, 0.5, 0});
        auto constraint = ConstrainLinked(point, oldEdge);
        outline[0] = FreeformOutline()[0]; // Same source vertices, curve/edge IDs and connectivity.
        file.Write(Exchange(schema, BezierLoft(outline)));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile)); SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(!SK.entity.FindByIdNoOops(oldEdge) && !SK.constraint.FindByIdNoOops(constraint));
        REQUIRE(LinkedReference(hg, StepEntityKey::VERTEX, 30) == vertex);
        REQUIRE(LinkedReference(hg, StepEntityKey::EDGE, 401) == otherEdge);
        REQUIRE(SK.GetGroup(hg)->stepRefs == identities && SK.GetGroup(hg)->impEntity.n == 23);
        int curved = 0;
        for(const auto &curve : SK.GetGroup(hg)->thisShell.curve) if(curve.exact.deg == 2) {
            REQUIRE(curve.exact.entity == 0 && curve.exact.weight[1] == 0.6);
            ++curved;
        }
        REQUIRE(curved == 2 && SK.GetGroup(hg)->runningMesh.IsEmpty());
    });
    Run(name + "collinear_cubic_retains_existing_line_identity", [&]() {
        LinkApp app;
        LinkTestFile file;
        file.Write(Exchange(schema, Cube(1)));
        auto hg = MenuLink(file.path); PlaceLink(hg);
        auto edge = LinkedReference(hg, StepEntityKey::EDGE, 400);
        auto identities = SK.GetGroup(hg)->stepRefs;
        file.Write(Exchange(schema, SplineCurve(Cube(1), 300, 5000, 3,
            {{0, 0, 0}, {0.8, 0, 0}, {0.2, 0, 0}, {1, 0, 0}}, {1, 0.8, 1.2, 1}, {4, 4}, {0, 1})));
        REQUIRE(SS.ReloadAllLinked(SS.saveFile)); SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        REQUIRE(SK.GetGroup(hg)->stepRefs == identities && LinkedReference(hg, StepEntityKey::EDGE, 400) == edge);
        REQUIRE(SK.GetEntity(edge)->type == Entity::Type::LINE_SEGMENT && SK.GetGroup(hg)->impEntity.n == 26);
        bool retained = false;
        for(const auto &curve : SK.GetGroup(hg)->thisShell.curve) if(curve.exact.deg == 3) {
            REQUIRE(curve.exact.entity == edge.v && curve.exact.weight[1] == 0.8 && curve.exact.weight[2] == 1.2);
            retained = true;
        }
        REQUIRE(retained && SK.GetGroup(hg)->runningMesh.IsEmpty());
    });
}

void ExternalLinkedCylinder(const std::string &path) {
    LinkApp app;
    LinkTestFile saved("slvs");
    auto hg = MenuLink(Platform::Path::From(path).Expand(true));
    PlaceLink(hg);
    CheckCurvedLink(hg, 14, 31.25 * PI, 0.25);
    auto identities = SK.GetGroup(hg)->stepRefs;
    REQUIRE(identities.size() == 14);
    int points = 0, lines = 0, faces = 0;
    for(const auto &entity : SK.entity) if(entity.group == hg) {
        if(entity.IsPoint()) ++points;
        else if(entity.type == Entity::Type::LINE_SEGMENT) ++lines;
        else if(entity.IsFace()) ++faces;
        else REQUIRE(false);
    }
    REQUIRE(points == 8 && lines == 4 && faces == 2);
    Group::MenuGroup(Command::GROUP_3D);
    auto drawing = SS.GW.activeGroup;
    auto vertex = LinkedReference(hg, StepEntityKey::VERTEX);
    auto edge = LinkedReference(hg, StepEntityKey::EDGE);
    auto face = LinkedReference(hg, StepEntityKey::FACE);
    auto point = LinkDatum(drawing, {1, 2, 3});
    ConstrainLinked(point, vertex);
    auto onLine = LinkDatum(drawing, {1, 2, 3});
    REQUIRE(SK.GetConstraint(ConstrainLinked(onLine, edge))->type == Constraint::Type::PT_ON_LINE);
    auto onPlane = LinkDatum(drawing, {1, 2, 3});
    REQUIRE(SK.GetConstraint(ConstrainLinked(onPlane, face))->type == Constraint::Type::PT_ON_FACE);
    REQUIRE(SS.SaveToFile(saved.path));
    REQUIRE(SS.LoadFromFile(saved.path)); SS.AfterNewFile();
    REQUIRE(!SS.fileLoadError && SK.GetGroup(hg)->stepRefs == identities);
    REQUIRE(SK.GetGroup(drawing)->IsSolvedOkay() && SK.constraint.n == 3);
    Quaternion rotation = Quaternion::From(Vector{1, 2, 3}.WithMagnitude(1), 0.4);
    PlaceLink(hg, {7, -3, 11}, rotation, 2);
    CheckCurvedLink(hg, 14, 250 * PI, 2);
    REQUIRE(SK.GetEntity(point)->PointGetNum().Equals(SK.GetEntity(vertex)->PointGetNum()));
    REQUIRE(SS.ReloadAllLinked(SS.saveFile)); SS.GenerateAll(SolveSpaceUI::Generate::ALL);
    REQUIRE(SK.GetGroup(hg)->stepRefs == identities);
}
#endif

} // namespace

int main(int argc, char **argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportHook(CrtFailure);
#endif
    SS.exportMode = false;
    SS.chordTol = 0.1;
    SS.chordTolCalculated = 0.01;
    SS.maxSegments = 64;
    auto args = Platform::InitCli(argc, argv);
    bool external = (args.size() == 3 || args.size() == 5) &&
                    (args[1] == "--cube-1mm" || args[1] == "--cylinder-5mm");
    bool source = args.size() == 5 && args[3] == "--source-slvs";
    if((args.size() != 1 && !external) || (args.size() == 5 && !source)) {
        std::cerr << "Usage: solvespace-step-geometry-testsuite "
                     "[--cube-1mm|--cylinder-5mm file.step [--source-slvs original.slvs]]\n";
        return 2;
    }
#ifdef HAVE_STEP_IMPORT
    Run("link/remap_only_existing_edge_references", [&]() {
        Group g = {};
        g.h = {1000};
        hEntity source = {123}, other = {0x12340000};
        auto placed = g.Remap(source, 0);
        ShellOwner shell;
        for(auto entity : {source, other}) {
            SCurve curve = {};
            curve.isExact = true;
            curve.exact.entity = entity.v;
            shell.shell.curve.AddAndAssignId(&curve);
        }
        shell.shell.RemapFaces(&g, 0);
        REQUIRE(g.remap.size() == 1);
        REQUIRE(shell.shell.curve[0].exact.entity == placed.v);
        REQUIRE(shell.shell.curve[1].exact.entity == other.v);
    });
    Run("link/file_dispatch_and_dialog_filters", [&]() {
        for(const auto &extension : {"step", "stp", "STEP", "STP", "StEp", "sTp"}) {
            REQUIRE(SS.IsStepFile(Platform::Path::From(std::string("cube.") + extension)));
        }
        REQUIRE(!SS.IsStepFile(Platform::Path::From("cube.slvs")));
        REQUIRE(!SS.IsStepFile(Platform::Path::From("cube.step.bak")));
        bool step = false, stp = false;
        for(const auto &filter : Platform::SolveSpaceLinkFileFilters) {
            for(const auto &extension : filter.extensions) {
                step |= extension == "step";
                stp |= extension == "stp";
            }
        }
        REQUIRE(step && stp);
    });
#endif
    NurbsMathTests();
    for(const auto &binding : Step::SupportedBindings()) {
        ProtocolTests(binding);
        SplineTests(binding);
        GenericSplineTests(binding);
#ifdef HAVE_STEP_IMPORT
        LinkTests(binding);
        CurvedLinkTests(binding);
#endif
    }
    if(external) {
        if(args[1] == "--cylinder-5mm") {
            Run("external/cylinder_5mm", [&]() { ExternalCylinder(args[2], source ? args[4] : ""); });
#ifdef HAVE_STEP_IMPORT
            Run("external/link_cylinder_5mm", [&]() { ExternalLinkedCylinder(args[2]); });
#endif
        } else {
            Run("external/cube_1mm", [&]() { ExternalUnitCube(args[2], source ? args[4] : ""); });
#ifdef HAVE_STEP_IMPORT
            Run("external/link_cube_1mm", [&]() { ExternalLinkedUnitCube(args[2]); });
#endif
        }
    }
    std::cout << cases << " cases, " << checks << " checks, " << failures << " failures\n";
    return !cases ? 2 : (failures ? 1 : 0);
}
