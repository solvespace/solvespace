// Native B-rep conversion. No STEPcode headers, types or generated bindings.
#include "solvespace.h"
#include "stepgeometry.h"
#include "stepnurbs.h"
#include "stepreader.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace SolveSpace {
namespace Step {
namespace {

using Code = GeometryDiagnostic::Code;
using Kind = Value::Kind;

struct Failure {
    Code code;
    int entity;
    std::string message;
};

// Converter/work limits, not replacements for resource limits in the parser.
const size_t MAX_FACES = 512;
const size_t MAX_EDGES = 2048;
const size_t MAX_VERTICES = 1024;
const size_t MAX_NATIVE_CURVES = 32768;
const size_t MAX_TRIM_POINTS = 16384;
const size_t MAX_VALIDATION_TRIANGLES = 32768;
const double MAX_COORDINATE_MM = 1e8;

void Require(bool okay, Code code, int entity, const std::string &message) {
    if(!okay) throw Failure{code, entity, message};
}

bool Finite(Vector point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

double Cross(Point2d a, Point2d b) { return a.x * b.y - a.y * b.x; }

double Roundoff(const std::vector<Vector> &points) {
    double largest = 1;
    for(Vector point : points) largest = std::max({largest, std::abs(point.x),
                                                  std::abs(point.y), std::abs(point.z)});
    return std::min(LENGTH_EPS * 0.01, 64 * std::numeric_limits<double>::epsilon() * largest);
}

struct EdgeListOwner {
    SEdgeList edges = {};
    ~EdgeListOwner() { edges.Clear(); }
};
struct PolygonOwner {
    SPolygon polygon = {};
    ~PolygonOwner() { polygon.Clear(); }
};
struct MeshOwner {
    SMesh mesh = {};
    ~MeshOwner() { mesh.Clear(); }
};

// The native surface/triangulation routines use these application settings,
// just as Sketch::GenerateShellsAndMeshes does. Conversion is synchronous; save
// and restore them on every path, including rejected solids. Never derive an
// import's validation/PWL resolution from the unrelated current sketch.
struct TessellationScope {
    double chord;
    int segments;
    bool exporting;
    TessellationScope(double tolerance, int count)
        : chord(SS.chordTolCalculated), segments(SS.maxSegments), exporting(SS.exportMode) {
        SS.chordTolCalculated = tolerance; SS.maxSegments = count; SS.exportMode = false;
    }
    ~TessellationScope() {
        SS.chordTolCalculated = chord; SS.maxSegments = segments; SS.exportMode = exporting;
    }
};

class Attributes {
public:
    const Document &document;
    explicit Attributes(const Document &document) : document(document) {}

    Value Get(int id, const std::string &name) const {
        Value value;
        Require(document.Attribute(id, name, &value), Code::InvalidGeometry, id,
                "Missing, ambiguous or undecodable attribute '" + name + "'.");
        return value;
    }

    int Ref(int id, const std::string &name) const {
        auto value = Get(id, name);
        Require(value.kind == Kind::Reference && value.reference > 0,
                Code::InvalidGeometry, id, "Expected reference in '" + name + "'.");
        return value.reference;
    }

    std::vector<int> Refs(int id, const std::string &name) const {
        auto value = Get(id, name);
        Require(value.kind == Kind::Aggregate, Code::InvalidGeometry, id,
                "Expected reference aggregate in '" + name + "'.");
        std::vector<int> result;
        for(const auto &item : value.elements) {
            Require(item.kind == Kind::Reference && item.reference > 0,
                    Code::InvalidGeometry, id, "Invalid reference in '" + name + "'.");
            result.push_back(item.reference);
        }
        return result;
    }

    bool Boolean(int id, const std::string &name) const {
        auto value = Get(id, name);
        Require(value.kind == Kind::Enumeration && (value.text == "T" || value.text == "F"),
                Code::InvalidGeometry, id, "Expected Boolean in '" + name + "'.");
        return value.text == "T";
    }

    std::string Logical(int id, const std::string &name) const {
        auto value = Get(id, name);
        Require(value.kind == Kind::Enumeration &&
                (value.text == "T" || value.text == "F" || value.text == "U"),
                Code::InvalidGeometry, id, "Expected logical value in '" + name + "'.");
        return value.text;
    }

    double Number(const Value &value, int id) const {
        Require(value.kind == Kind::Real || value.kind == Kind::Integer,
                Code::InvalidGeometry, id, "Expected a numeric value.");
        double result = value.kind == Kind::Real ? value.real : (double)value.integer;
        Require(std::isfinite(result), Code::InvalidGeometry, id, "Non-finite numeric value.");
        return result;
    }

    std::vector<double> Numbers(int id, const std::string &name) const {
        auto value = Get(id, name);
        Require(value.kind == Kind::Aggregate, Code::InvalidGeometry, id,
                "Expected numeric aggregate in '" + name + "'.");
        std::vector<double> result;
        for(const auto &item : value.elements) result.push_back(Number(item, id));
        return result;
    }

    int Integer(const Value &value, int id) const {
        Require(value.kind == Kind::Integer && value.integer >= 0 &&
                value.integer <= std::numeric_limits<int>::max(), Code::InvalidGeometry, id,
                "Expected a non-negative, representable integer.");
        return (int)value.integer;
    }

    Nurbs::Knots Knots(int id, const std::string &degree, size_t controls,
                      const std::string &multiplicities, const std::string &knots) const {
        int order = Integer(Get(id, degree), id);
        Require(order >= 1 && order <= 3, Code::Unsupported, id,
                "B-spline degree is outside the native Bezier range 1..3; no degree fitting is performed.");
        Require(controls >= (size_t)order + 1 && controls <= Nurbs::MAX_CONTROLS,
                controls > Nurbs::MAX_CONTROLS ? Code::Unsupported : Code::InvalidGeometry,
                id, "Invalid or excessive B-spline control-point count.");
        bool surface = degree != "degree";
        if(!document.IsA(id, surface ? "B_SPLINE_SURFACE_WITH_KNOTS" : "B_SPLINE_CURVE_WITH_KNOTS")) {
            std::vector<int> mult;
            std::vector<double> values;
            if(document.IsA(id, surface ? "BEZIER_SURFACE" : "BEZIER_CURVE")) {
                mult = {order + 1, order + 1}; values = {0, 1};
            } else if(document.IsA(id, surface ? "UNIFORM_SURFACE" : "UNIFORM_CURVE")) {
                for(size_t i = 0; i < controls + order + 1; ++i) { mult.push_back(1); values.push_back((double)i); }
            } else if(document.IsA(id, surface ? "QUASI_UNIFORM_SURFACE" : "QUASI_UNIFORM_CURVE")) {
                for(size_t i = 0; i <= controls - order; ++i) {
                    mult.push_back(i == 0 || i == controls - order ? order + 1 : 1);
                    values.push_back((double)i);
                }
            } else throw Failure{Code::Unsupported, id, "B-spline has no explicit knots or implemented standard implicit-knot definition."};
            return Nurbs::Knots(id, order, controls, mult, values);
        }
        auto mult = Get(id, multiplicities);
        Require(mult.kind == Kind::Aggregate, Code::InvalidGeometry, id,
                "Expected a B-spline knot-multiplicity aggregate.");
        std::vector<int> values;
        for(const auto &item : mult.elements) values.push_back(Integer(item, id));
        return Nurbs::Knots(id, order, controls, values, Numbers(id, knots));
    }

    Vector Triple(int id, const std::string &name) const {
        auto value = Get(id, name);
        Require(value.kind == Kind::Aggregate && value.elements.size() == 3,
                Code::InvalidGeometry, id, "Expected three components in '" + name + "'.");
        return {Number(value.elements[0], id), Number(value.elements[1], id),
                Number(value.elements[2], id)};
    }

    Vector Direction(int id) const {
        Require(document.IsA(id, "DIRECTION"), Code::InvalidGeometry, id, "Expected DIRECTION.");
        auto direction = Triple(id, "direction_ratios");
        double largest = std::max({std::abs(direction.x), std::abs(direction.y),
                                   std::abs(direction.z)});
        Require(largest > 0, Code::InvalidGeometry, id, "Zero direction vector.");
        direction = {direction.x / largest, direction.y / largest, direction.z / largest};
        double magnitude = direction.Magnitude();
        Require(std::isfinite(magnitude) && magnitude > 0, Code::InvalidGeometry, id,
                "Unrepresentable direction vector.");
        return direction.ScaledBy(1.0 / magnitude);
    }
};

class Units : public Attributes {
public:
    explicit Units(const Document &document) : Attributes(document) {}

    double ForContext(int context) {
        Require(document.IsA(context, "GEOMETRIC_REPRESENTATION_CONTEXT") &&
                Number(Get(context, "coordinate_space_dimension"), context) == 3,
                Code::Units, context, "A 3D geometric representation context is required.");
        Require(document.IsA(context, "GLOBAL_UNIT_ASSIGNED_CONTEXT"), Code::Units, context,
                "No explicit global units; millimeters must not be assumed.");
        int length = 0;
        for(int unit : Refs(context, "units")) {
            if(!document.IsA(unit, "LENGTH_UNIT")) continue;
            Require(length == 0, Code::Units, context, "Ambiguous length units in context.");
            length = unit;
        }
        Require(length != 0, Code::Units, context, "No LENGTH_UNIT in context.");
        return Scale(length);
    }

private:
    std::set<int> active;

    double Scale(int unit) {
        Require(active.size() < 16 && active.insert(unit).second, Code::Units, unit,
                "Cyclic or excessively nested conversion-based units.");
        Require(document.IsA(unit, "LENGTH_UNIT"), Code::Units, unit,
                "A conversion factor must resolve to LENGTH_UNIT.");
        auto dimensions = Get(unit, "dimensions");
        if(dimensions.kind != Kind::Derived) {
            Require(dimensions.kind == Kind::Reference, Code::Units, unit,
                    "Missing length dimensional exponents.");
            const char *names[] = {"length_exponent", "mass_exponent", "time_exponent",
                "electric_current_exponent", "thermodynamic_temperature_exponent",
                "amount_of_substance_exponent", "luminous_intensity_exponent"};
            for(int i = 0; i < 7; ++i) {
                Require(Number(Get(dimensions.reference, names[i]), dimensions.reference) ==
                        (i == 0 ? 1.0 : 0.0), Code::Units, unit, "Unit dimensions are not length.");
            }
        }
        double scale = 0;
        if(document.IsA(unit, "SI_UNIT")) {
            auto name = Get(unit, "name");
            Require(name.kind == Kind::Enumeration && name.text == "METRE", Code::Units, unit,
                    "Only SI METRE is supported as a base length unit.");
            auto prefix = Get(unit, "prefix");
            static const std::map<std::string, int> powers = {
                {"EXA", 18}, {"PETA", 15}, {"TERA", 12}, {"GIGA", 9}, {"MEGA", 6},
                {"KILO", 3}, {"HECTO", 2}, {"DECA", 1}, {"DECI", -1}, {"CENTI", -2},
                {"MILLI", -3}, {"MICRO", -6}, {"NANO", -9}, {"PICO", -12},
                {"FEMTO", -15}, {"ATTO", -18}
            };
            int power = 0;
            if(prefix.kind != Kind::Null) {
                auto found = powers.find(prefix.text);
                Require(prefix.kind == Kind::Enumeration && found != powers.end(),
                        Code::Units, unit, "Unsupported SI prefix.");
                power = found->second;
            }
            scale = 1000.0 * std::pow(10.0, power);
        } else if(document.IsA(unit, "CONVERSION_BASED_UNIT")) {
            int measure = Ref(unit, "conversion_factor");
            auto value = Get(measure, "value_component");
            Require(value.selectedType == "LENGTH_MEASURE" ||
                    value.selectedType == "POSITIVE_LENGTH_MEASURE", Code::Units, measure,
                    "Conversion factor must contain a typed length measure.");
            double factor = Number(value, measure);
            Require(factor > 0, Code::Units, measure, "Non-positive length conversion factor.");
            scale = factor * Scale(Ref(measure, "unit_component"));
        } else {
            throw Failure{Code::Units, unit, "Unsupported length unit definition."};
        }
        Require(std::isfinite(scale) && scale > 0, Code::Units, unit,
                "Length conversion is non-finite or non-positive.");
        active.erase(unit);
        return scale;
    }
};

struct Vertex { int id, point; Vector position; };
struct Piece {
    SBezier bezier;
    std::vector<double> cuts;
    size_t segment;
};
struct Edge {
    int id, geometry, start, finish;
    bool linear;
    struct Use { int face; bool forwards; };
    std::vector<Use> uses;
    std::vector<Piece> pieces; // always in topological edge_start -> edge_end order
};
struct EdgeUse { int edge; bool forwards; };
struct Loop {
    int id;
    bool declaredOuter;
    std::vector<EdgeUse> edges;
    std::vector<int> vertices;
    std::vector<Point2d> uv; // validation/classification only, not returned geometry
    double twiceArea = 0;
};
struct Patch {
    Nurbs::SurfacePatch source;
    hSSurface handle;
    std::vector<std::pair<size_t, bool>> trims; // segment index, native backwards
};
struct Face {
    int id, geometry;
    bool planar;
    Vector origin, normal;
    double metric;
    std::vector<Loop> loops;
    std::vector<Patch> patches;
    double UvEps() const { return std::max(1e-12, std::min(1e-4, LENGTH_EPS / metric)); }
};
struct Segment {
    SBezier bezier;
    int edge; // zero for an artificial native patch seam
    hSCurve handle;
    std::vector<std::pair<Patch *, bool>> uses;
};
struct Seam {
    Face *face;
    Patch *a, *b;
    bool constantU;
    SBezier bezier;
    Point2d uv0, uv1;
    std::vector<double> cuts;
};

// +1 inside, 0 on the validation boundary, -1 outside.
int InLoop(const std::vector<Point2d> &loop, Point2d point, double eps) {
    int winding = 0;
    for(size_t i = 0; i < loop.size(); ++i) {
        Point2d a = loop[i], b = loop[(i + 1) % loop.size()], d = b.Minus(a);
        if(point.DistanceToLine(a, d, /*asSegment=*/true) <= eps) return 0;
        if(a.y <= point.y && b.y > point.y && Cross(d, point.Minus(a)) > 0) ++winding;
        if(b.y <= point.y && a.y > point.y && Cross(d, point.Minus(a)) < 0) --winding;
    }
    return winding == 0 ? -1 : 1;
}

int InFace(const Face &face, Point2d point) {
    int winding = 0;
    for(const auto &loop : face.loops) {
        int inside = InLoop(loop.uv, point, face.UvEps());
        if(inside == 0) return 0;
        if(inside == 1) winding += loop.twiceArea > 0 ? 1 : -1;
    }
    return winding == 1 ? 1 : -1;
}

bool SegmentsTouch(Point2d a, Point2d b, Point2d c, Point2d d, double eps) {
    if(std::max(a.x, b.x) < std::min(c.x, d.x) - eps ||
       std::max(c.x, d.x) < std::min(a.x, b.x) - eps ||
       std::max(a.y, b.y) < std::min(c.y, d.y) - eps ||
       std::max(c.y, d.y) < std::min(a.y, b.y) - eps) return false;
    if(a.DistanceToLine(c, d.Minus(c), true) <= eps ||
       b.DistanceToLine(c, d.Minus(c), true) <= eps ||
       c.DistanceToLine(a, b.Minus(a), true) <= eps ||
       d.DistanceToLine(a, b.Minus(a), true) <= eps) return true;
    return (Cross(b.Minus(a), c.Minus(a)) > 0) != (Cross(b.Minus(a), d.Minus(a)) > 0) &&
           (Cross(d.Minus(c), a.Minus(c)) > 0) != (Cross(d.Minus(c), b.Minus(c)) > 0);
}

class SolidBuilder : public Attributes {
public:
    SolidBuilder(const Document &document, const GeometryOptions &options)
        : Attributes(document), options(options) {}

    std::unique_ptr<NativeSolid> Build(int representation, int solid) {
        int context = Ref(representation, "context_of_items");
        scale = Units(document).ForContext(context);
        Require(document.IsA(solid, "MANIFOLD_SOLID_BREP"), Code::Unsupported, solid,
                "Only MANIFOLD_SOLID_BREP is supported.");
        Require(!document.IsA(solid, "BREP_WITH_VOIDS"), Code::Unsupported, solid,
                "Cavity-shell conversion is not implemented; the entire solid is omitted.");
        int outer = Ref(solid, "outer");
        auto shellTypes = document.EntityTypes(outer);
        Require(shellTypes.size() == 1 && shellTypes[0] == "CLOSED_SHELL", Code::Unsupported,
                outer, "Only a direct CLOSED_SHELL is supported.");
        auto faceIds = Refs(outer, "cfs_faces");
        Require(!faceIds.empty() && faceIds.size() <= MAX_FACES, Code::Unsupported, outer,
                "Closed-shell face count is empty or exceeds the converter limit.");
        std::sort(faceIds.begin(), faceIds.end());
        Require(std::adjacent_find(faceIds.begin(), faceIds.end()) == faceIds.end(),
                Code::InvalidGeometry, outer, "Repeated face in CLOSED_SHELL.");
        faces.reserve(faceIds.size());
        for(int face : faceIds) faces.push_back(ReadFace(face));
        Vector min = vertices.begin()->second.position, max = min;
        for(const auto &edge : edges) for(const auto &piece : edge.second.pieces) {
            for(int i = 0; i <= piece.bezier.deg; ++i) piece.bezier.ctrl[i].MakeMaxMin(&max, &min);
        }
        double tolerance = options.chordTolerance == 0 ?
            std::max(32 * LENGTH_EPS, std::min(0.01, max.Minus(min).Magnitude() * 0.001)) : options.chordTolerance;
        Require(std::isfinite(tolerance) && tolerance >= 32 * LENGTH_EPS &&
                options.maxSegments >= 8 && options.maxSegments <= 4096, Code::Unsupported, solid,
                "Native trim/validation tessellation settings are outside the supported numerical/work envelope.");
        TessellationScope tessellation(tolerance, options.maxSegments);
        ValidateTopology(outer);
        for(auto &face : faces) ValidateFace(&face);
        MakeSeams();
        SplitEdges();
        AttachSourceTrims();
        AttachSeams();

        std::unique_ptr<NativeSolid> result(new NativeSolid);
        result->representation = representation;
        result->context = context;
        result->solid = solid;
        result->outerShell = outer;
        result->lengthToMm = scale;
        MakeNative(result.get());
        ValidateNative(result.get());
        return result;
    }

private:
    const GeometryOptions &options;
    double scale = 0;
    std::map<int, Vertex> vertices;
    std::map<int, Edge> edges;
    std::vector<Face> faces;
    std::vector<Segment> segments;
    std::vector<Seam> seams;

    Vector Point(int id) {
        Require(document.IsA(id, "CARTESIAN_POINT"), Code::InvalidGeometry, id,
                "Expected CARTESIAN_POINT.");
        Vector point = Triple(id, "coordinates").ScaledBy(scale);
        Require(Finite(point) && std::abs(point.x) <= MAX_COORDINATE_MM &&
                std::abs(point.y) <= MAX_COORDINATE_MM && std::abs(point.z) <= MAX_COORDINATE_MM,
                Code::Unsupported, id, "Coordinates exceed the finite +/-1e8 mm native envelope.");
        return point;
    }

    const Vertex &ReadVertex(int id) {
        auto found = vertices.find(id);
        if(found != vertices.end()) return found->second;
        Require(document.IsA(id, "VERTEX_POINT"), Code::Unsupported, id,
                "Only VERTEX_POINT vertices are supported.");
        Require(vertices.size() < MAX_VERTICES, Code::Unsupported, id, "Source vertex limit exceeded.");
        int point = Ref(id, "vertex_geometry");
        return vertices.emplace(id, Vertex{id, point, Point(point)}).first->second;
    }

    std::vector<Nurbs::CurveSpan> ReadSplineCurve(int geometry) {
        Require(document.IsA(geometry, "B_SPLINE_CURVE"), Code::Unsupported, geometry,
                "Curve has no implemented exact native conversion (expected LINE or a rational/non-rational B-spline).");
        auto ids = Refs(geometry, "control_points_list");
        auto knots = Knots(geometry, "degree", ids.size(), "knot_multiplicities", "knots");
        std::vector<Vector> controls;
        for(int point : ids) controls.push_back(Point(point));
        std::vector<double> weights(ids.size(), 1);
        if(document.IsA(geometry, "RATIONAL_B_SPLINE_CURVE")) {
            weights = Numbers(geometry, "weights_data");
            Require(weights.size() == controls.size(), Code::InvalidGeometry, geometry,
                    "Rational curve weights do not match its control-point count.");
        }
        Require(Logical(geometry, "self_intersect") != "T", Code::InvalidGeometry, geometry,
                "Source declares a self-intersecting boundary curve.");
        auto spans = Nurbs::Curve(knots, controls, weights);
        if(Logical(geometry, "closed_curve") == "T") {
            Require(spans.front().bezier.Start().Equals(spans.back().bezier.Finish()),
                    Code::InvalidGeometry, geometry, "B-spline closed_curve flag disagrees with its finite endpoints.");
        }
        return spans;
    }

    std::vector<Piece> TrimCurve(int id, const std::vector<Nurbs::CurveSpan> &spans,
                                 Vector start, Vector finish, bool sameSense) {
        auto parameters = [&](Vector point) {
            std::vector<double> result;
            for(const auto &span : spans) {
                for(double t : Nurbs::Parameters(span.bezier, point)) {
                    result.push_back(span.first + t * (span.last - span.first));
                }
            }
            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end(), [](double a, double b) {
                return std::abs(a - b) < 1e-10;
            }), result.end());
            return result;
        };
        auto a = parameters(start), b = parameters(finish);
        Require(!a.empty() && !b.empty(), Code::InvalidGeometry, id,
                "Edge vertices lie outside the finite B-spline curve; no extrapolation is performed.");
        double first = 0, last = 1;
        if(start.Equals(finish) && a.front() == 0 && a.back() == 1 &&
           b.front() == 0 && b.back() == 1 && a.size() == 2 && b.size() == 2) {
            // A complete closed finite curve has an unambiguous whole-domain trim.
        } else {
            Require(a.size() == 1 && b.size() == 1, Code::Unsupported, id,
                    "Ambiguous finite-curve trimming at repeated points; no parameter branch was guessed.");
            first = sameSense ? a.front() : b.front();
            last = sameSense ? b.front() : a.front();
            Require(last > first, Code::InvalidGeometry, id,
                    "EDGE_CURVE same_sense disagrees with its finite spline geometry.");
        }
        std::vector<Piece> pieces;
        for(const auto &span : spans) {
            double lo = std::max(first, span.first), hi = std::min(last, span.last);
            if(hi <= lo) continue;
            double range = span.last - span.first;
            pieces.push_back({Nurbs::Restrict(span.bezier, (lo - span.first) / range,
                                              (hi - span.first) / range), {}});
        }
        Require(!pieces.empty(), Code::InvalidGeometry, id, "Empty finite-curve trim.");
        if(!sameSense) {
            std::reverse(pieces.begin(), pieces.end());
            for(auto &piece : pieces) piece.bezier.Reverse();
        }
        for(size_t i = 1; i < pieces.size(); ++i) {
            Require(pieces[i - 1].bezier.Finish().Equals(pieces[i].bezier.Start()),
                    Code::InvalidGeometry, id, "Discontinuous B-spline spans within one topological edge.");
        }
        return pieces;
    }

    Edge &ReadEdge(int id) {
        auto found = edges.find(id);
        if(found != edges.end()) return found->second;
        Require(document.IsA(id, "EDGE_CURVE"), Code::Unsupported, id, "Only EDGE_CURVE edges are supported.");
        Require(edges.size() < MAX_EDGES, Code::Unsupported, id, "Source edge limit exceeded.");
        Edge edge = {};
        edge.id = id;
        edge.start = Ref(id, "edge_start"); edge.finish = Ref(id, "edge_end");
        Vector a = ReadVertex(edge.start).position, b = ReadVertex(edge.finish).position;
        edge.geometry = Ref(id, "edge_geometry");
        bool sameSense = Boolean(id, "same_sense");
        if(document.IsA(edge.geometry, "LINE")) {
            Vector origin = Point(Ref(edge.geometry, "pnt"));
            int vector = Ref(edge.geometry, "dir");
            Vector direction = Direction(Ref(vector, "orientation"));
            Require(Number(Get(vector, "magnitude"), vector) > 0, Code::InvalidGeometry, vector,
                    "LINE vector magnitude must be positive.");
            Require(edge.start != edge.finish && b.Minus(a).Magnitude() > LENGTH_EPS,
                    Code::InvalidGeometry, id, "Closed or zero-length LINE edge.");
            Require(a.Minus(origin).Cross(direction).Magnitude() <= LENGTH_EPS &&
                    b.Minus(origin).Cross(direction).Magnitude() <= LENGTH_EPS,
                    Code::InvalidGeometry, id, "Edge vertices do not lie on their LINE geometry.");
            Require(b.Minus(a).Dot(direction) * (sameSense ? 1 : -1) > LENGTH_EPS,
                    Code::InvalidGeometry, id, "EDGE_CURVE same_sense disagrees with its LINE geometry.");
            edge.pieces.push_back({SBezier::From(a, b), {}});
        } else {
            edge.pieces = TrimCurve(id, ReadSplineCurve(edge.geometry), a, b, sameSense);
        }
        std::vector<Vector> controls;
        for(const auto &piece : edge.pieces) {
            double extent = 0;
            for(int i = 0; i <= piece.bezier.deg; ++i) {
                controls.push_back(piece.bezier.ctrl[i]);
                extent = std::max(extent, piece.bezier.ctrl[i].Minus(piece.bezier.Start()).Magnitude());
            }
            Require(extent > LENGTH_EPS, Code::InvalidGeometry, edge.geometry,
                    "Degenerate Bezier span in a source edge.");
        }
        edge.linear = !a.Equals(b);
        if(edge.linear) {
            double eps = Roundoff(controls);
            for(const auto &piece : edge.pieces) {
                if(!Nurbs::IsMonotoneLine(piece.bezier, a, b, eps)) edge.linear = false;
            }
        }
        return edges.emplace(id, std::move(edge)).first->second;
    }

    void ReadPlane(Face *face) {
        int placement = Ref(face->geometry, "position");
        Require(document.IsA(placement, "AXIS2_PLACEMENT_3D"), Code::InvalidGeometry, placement,
                "PLANE needs AXIS2_PLACEMENT_3D.");
        face->origin = Point(Ref(placement, "location"));
        auto axis = Get(placement, "axis"), ref = Get(placement, "ref_direction");
        Require((axis.kind == Kind::Null || axis.kind == Kind::Reference) &&
                (ref.kind == Kind::Null || ref.kind == Kind::Reference),
                Code::InvalidGeometry, placement, "Invalid optional plane axes.");
        Vector z = axis.kind == Kind::Null ? Vector{0, 0, 1} : Direction(axis.reference);
        Vector x = ref.kind == Kind::Reference ? Direction(ref.reference) :
                   (z.Equals({1, 0, 0}, 1e-12) ? Vector{0, 1, 0} : Vector{1, 0, 0});
        x = x.Minus(z.ScaledBy(x.Dot(z)));
        Require(x.Magnitude() > 1e-12, Code::InvalidGeometry, placement,
                "Plane axis and reference direction are parallel.");
        x = x.WithMagnitude(1);
        face->normal = Boolean(face->id, "same_sense") ? z : z.Negated();
        Vector y = face->normal.Cross(x);
        double xmin = VERY_POSITIVE, xmax = VERY_NEGATIVE, ymin = xmin, ymax = xmax;
        for(const auto &loop : face->loops) for(const auto &use : loop.edges) {
            for(const auto &piece : edges.at(use.edge).pieces) for(int i = 0; i <= piece.bezier.deg; ++i) {
                Vector delta = piece.bezier.ctrl[i].Minus(face->origin);
                double u = delta.Dot(x), v = delta.Dot(y);
                xmin = std::min(xmin, u); xmax = std::max(xmax, u);
                ymin = std::min(ymin, v); ymax = std::max(ymax, v);
            }
        }
        double xpad = std::max((xmax - xmin) * 0.05, 8 * LENGTH_EPS);
        double ypad = std::max((ymax - ymin) * 0.05, 8 * LENGTH_EPS);
        xmin -= xpad; xmax += xpad; ymin -= ypad; ymax += ypad;
        Vector origin = face->origin.Plus(x.ScaledBy(xmin)).Plus(y.ScaledBy(ymin));
        SSurface surface = SSurface::FromPlane(origin, y.ScaledBy(ymax - ymin), x.ScaledBy(xmax - xmin));
        face->patches.push_back({{surface, 0, 1, 0, 1, 0, 0}});
        face->planar = true;
    }

    void ReadSplineSurface(Face *face) {
        int geometry = face->geometry;
        Require(document.IsA(geometry, "B_SPLINE_SURFACE"), Code::Unsupported, geometry,
                "Surface has no implemented exact native conversion (expected PLANE or a rational/non-rational tensor-product B-spline).");
        auto net = Get(geometry, "control_points_list");
        Require(net.kind == Kind::Aggregate && !net.elements.empty(), Code::InvalidGeometry,
                geometry, "Empty or invalid B-spline control net.");
        std::vector<std::vector<Vector>> controls;
        for(const auto &row : net.elements) {
            Require(row.kind == Kind::Aggregate && !row.elements.empty() &&
                    (controls.empty() || row.elements.size() == controls.front().size()),
                    Code::InvalidGeometry, geometry, "B-spline control net must be rectangular.");
            Require(controls.size() < Nurbs::MAX_CONTROLS && row.elements.size() <= Nurbs::MAX_CONTROLS &&
                    (controls.size() + 1) * row.elements.size() <= Nurbs::MAX_CONTROLS,
                    Code::Unsupported, geometry, "B-spline surface control-point limit exceeded.");
            std::vector<Vector> points;
            for(const auto &point : row.elements) {
                Require(point.kind == Kind::Reference && point.reference > 0,
                        Code::InvalidGeometry, geometry, "Expected Cartesian-point references in a spline control net.");
                points.push_back(Point(point.reference));
            }
            controls.push_back(std::move(points));
        }
        auto u = Knots(geometry, "u_degree", controls.size(), "u_multiplicities", "u_knots");
        auto v = Knots(geometry, "v_degree", controls.front().size(), "v_multiplicities", "v_knots");
        std::vector<std::vector<double>> weights(controls.size(), std::vector<double>(controls.front().size(), 1));
        if(document.IsA(geometry, "RATIONAL_B_SPLINE_SURFACE")) {
            auto data = Get(geometry, "weights_data");
            Require(data.kind == Kind::Aggregate && data.elements.size() == controls.size(),
                    Code::InvalidGeometry, geometry, "Rational surface weights do not match its control net.");
            for(size_t i = 0; i < controls.size(); ++i) {
                const auto &row = data.elements[i];
                Require(row.kind == Kind::Aggregate && row.elements.size() == controls[i].size(),
                        Code::InvalidGeometry, geometry, "Rational surface weights must match the rectangular control net.");
                for(size_t j = 0; j < controls[i].size(); ++j) weights[i][j] = Number(row.elements[j], geometry);
            }
        }
        Require(Logical(geometry, "self_intersect") != "T", Code::InvalidGeometry, geometry,
                "Source declares a self-intersecting surface.");
        for(const char *flag : {"u_closed", "v_closed"}) Logical(geometry, flag);
        auto source = Nurbs::Surface(u, v, controls, weights);
        for(bool constantU : {true, false}) {
            if(Logical(geometry, constantU ? "u_closed" : "v_closed") != "T") continue;
            int last = (int)(constantU ? u.breaks.size() : v.breaks.size()) - 2;
            for(const auto &a : source) {
                if((constantU ? a.iu : a.iv) != 0) continue;
                for(const auto &b : source) {
                    if((constantU ? b.iu : b.iv) != last ||
                       (constantU ? b.iv != a.iv : b.iu != a.iu)) continue;
                    auto first = Nurbs::IsoCurve(a.surface, constantU, false);
                    auto finish = Nurbs::IsoCurve(b.surface, constantU, true);
                    for(double t : {0.0, 0.25, 0.5, 0.75, 1.0}) Require(first.PointAt(t).Equals(finish.PointAt(t)),
                            Code::InvalidGeometry, geometry, "B-spline closed-surface flag disagrees with its finite boundary geometry.");
                }
            }
        }
        bool reverse = !Boolean(face->id, "same_sense");
        for(auto &patch : source) {
            if(reverse) {
                Nurbs::Transpose(&patch.surface);
                std::swap(patch.u0, patch.v0); std::swap(patch.u1, patch.v1);
                std::swap(patch.iu, patch.iv);
            }
        }
        std::sort(source.begin(), source.end(), [](const Nurbs::SurfacePatch &a,
                                                   const Nurbs::SurfacePatch &b) {
            return std::make_pair(a.iu, a.iv) < std::make_pair(b.iu, b.iv);
        });
        for(const auto &patch : source) face->patches.push_back({patch});
        auto &first = face->patches.front().source.surface;
        face->origin = first.PointAt(0.5, 0.5);
        Vector normal = first.NormalAt(0.5, 0.5);
        if(Finite(normal) && normal.Magnitude() > 0) {
            face->normal = normal.WithMagnitude(1);
            std::vector<Vector> points;
            for(const auto &patch : face->patches) {
                const auto &surface = patch.source.surface;
                for(int i = 0; i <= surface.degm; ++i) for(int j = 0; j <= surface.degn; ++j) {
                    points.push_back(surface.ctrl[i][j]);
                }
            }
            face->planar = true;
            double eps = Roundoff(points);
            for(Vector point : points) if(std::abs(point.Minus(face->origin).Dot(face->normal)) > eps) {
                face->planar = false;
            }
        }
    }

    Face ReadFace(int id) {
        Require(document.IsA(id, "ADVANCED_FACE"), Code::Unsupported, id, "Only ADVANCED_FACE is supported.");
        Face face = {};
        face.id = id; face.geometry = Ref(id, "face_geometry");
        auto bounds = Refs(id, "bounds");
        Require(!bounds.empty() && bounds.size() <= MAX_EDGES, Code::InvalidGeometry, id,
                "Empty or excessive face-bound count.");
        std::set<int> uniqueEdges, uniqueVertices;
        for(int bound : bounds) {
            Require(document.IsA(bound, "FACE_BOUND"), Code::Unsupported, bound, "Expected FACE_BOUND.");
            Loop loop = {};
            loop.id = Ref(bound, "bound");
            loop.declaredOuter = document.IsA(bound, "FACE_OUTER_BOUND");
            Require(document.IsA(loop.id, "EDGE_LOOP"), Code::Unsupported, loop.id,
                    "Only EDGE_LOOP face boundaries are supported.");
            auto orientedEdges = Refs(loop.id, "edge_list");
            Require(!orientedEdges.empty() && orientedEdges.size() <= MAX_EDGES,
                    Code::InvalidGeometry, loop.id, "Empty or excessive boundary edge count.");
            for(int oriented : orientedEdges) {
                Require(document.IsA(oriented, "ORIENTED_EDGE"), Code::Unsupported, oriented,
                        "Expected ORIENTED_EDGE in EDGE_LOOP.");
                int edge = Ref(oriented, "edge_element");
                ReadEdge(edge);
                Require(uniqueEdges.insert(edge).second, Code::Unsupported, loop.id,
                        "Repeated/seam use of one source edge on the same face needs explicit periodic trimming; no branch was guessed.");
                loop.edges.push_back({edge, Boolean(oriented, "orientation")});
            }
            if(!Boolean(bound, "orientation")) {
                std::reverse(loop.edges.begin(), loop.edges.end());
                for(auto &use : loop.edges) use.forwards = !use.forwards;
            }
            for(size_t i = 0; i < loop.edges.size(); ++i) {
                const auto &use = loop.edges[i];
                auto &edge = edges.at(use.edge);
                int start = use.forwards ? edge.start : edge.finish;
                int finish = use.forwards ? edge.finish : edge.start;
                const auto &nextUse = loop.edges[(i + 1) % loop.edges.size()];
                const auto &next = edges.at(nextUse.edge);
                Require(finish == (nextUse.forwards ? next.start : next.finish),
                        Code::InvalidGeometry, loop.id, "Boundary is not closed by source vertex IDs.");
                Require(uniqueVertices.insert(start).second, Code::InvalidGeometry, loop.id,
                        "Repeated/touching vertex in a source face boundary.");
                loop.vertices.push_back(start);
                Require(edge.uses.size() < 2, Code::InvalidGeometry, edge.id, "More than two incident source faces on an edge.");
                edge.uses.push_back({id, use.forwards});
            }
            face.loops.push_back(std::move(loop));
        }
        if(document.IsA(face.geometry, "PLANE")) ReadPlane(&face);
        else ReadSplineSurface(&face);
        size_t patches = face.patches.size();
        for(const auto &other : faces) patches += other.patches.size();
        Require(patches <= Nurbs::MAX_PATCHES, Code::Unsupported, id, "Native surface-patch limit exceeded for one solid.");
        face.metric = LENGTH_EPS;
        for(const auto &patch : face.patches) {
            const auto &source = patch.source;
            Vector tu, tv;
            source.surface.TangentsAt(0.5, 0.5, &tu, &tv);
            face.metric = std::max({face.metric, tu.Magnitude() / (source.u1 - source.u0),
                                                tv.Magnitude() / (source.v1 - source.v0)});
        }
        return face;
    }

    Point2d Project(Face *face, Vector point) {
        Point2d result = {};
        bool found = false;
        for(auto &patch : face->patches) {
            auto &source = patch.source;
            Vector max, min;
            source.surface.GetAxisAlignedBounding(&max, &min);
            if(point.x < min.x - LENGTH_EPS || point.x > max.x + LENGTH_EPS ||
               point.y < min.y - LENGTH_EPS || point.y > max.y + LENGTH_EPS ||
               point.z < min.z - LENGTH_EPS || point.z > max.z + LENGTH_EPS) continue;
            Point2d uv;
            source.surface.ClosestPointTo(point, &uv);
            if(!std::isfinite(uv.x) || !std::isfinite(uv.y) ||
               uv.x < -face->UvEps() || uv.x > 1 + face->UvEps() ||
               uv.y < -face->UvEps() || uv.y > 1 + face->UvEps() ||
               !source.surface.PointAt(uv).Equals(point)) continue;
            uv.x = std::max(0.0, std::min(1.0, uv.x));
            uv.y = std::max(0.0, std::min(1.0, uv.y));
            Point2d global = {source.u0 + (source.u1 - source.u0) * uv.x,
                              source.v0 + (source.v1 - source.v0) * uv.y};
            Require(!found || result.Equals(global, face->UvEps() * 4), Code::Unsupported, face->geometry,
                    "Ambiguous surface parameter branch at a face boundary; explicit periodic/pcurve trimming is required.");
            result = global; found = true;
        }
        Require(found, Code::InvalidGeometry, face->geometry,
                "Face boundary is not on the finite spline patch/surface; no projection, healing or extrapolation is performed.");
        return result;
    }

    void SampleInterval(Face *face, const SBezier &curve, double a, double b,
                        Point2d auv, Point2d buv, std::vector<Point2d> *into, int depth) {
        double error = 0;
        for(double fraction : {0.25, 0.5, 0.75}) {
            Point2d uv = Project(face, curve.PointAt(a + (b - a) * fraction));
            error = std::max(error, uv.DistanceToLine(auv, buv.Minus(auv), true));
        }
        double tolerance = std::max(face->UvEps() * 2, 1e-4 / face->metric);
        if(error > tolerance) {
            Require(depth < 20, Code::Unsupported, face->id,
                    "Cannot validate curved face trimming within the native subdivision limit.");
            double m = (a + b) * 0.5;
            Point2d muv = Project(face, curve.PointAt(m));
            SampleInterval(face, curve, a, m, auv, muv, into, depth + 1);
            SampleInterval(face, curve, m, b, muv, buv, into, depth + 1);
        } else {
            Require(into->size() < MAX_TRIM_POINTS, Code::Unsupported, face->id, "Face trim-validation point limit exceeded.");
            if(into->empty() || !into->back().Equals(buv, face->UvEps() * 0.01)) into->push_back(buv);
        }
    }

    void ValidateFace(Face *face) {
        // Regularity is checked independently of primitive recognition. No
        // extrusion/cylinder or planar-convexity gate is used for free-form nets.
        for(const auto &patch : face->patches) {
            const auto &surface = patch.source.surface;
            for(int i = 0; i <= 8; ++i) for(int j = 0; j <= 8; ++j) {
                double u = std::max(1e-6, std::min(1 - 1e-6, i / 8.0));
                double v = std::max(1e-6, std::min(1 - 1e-6, j / 8.0));
                Vector tu, tv;
                surface.TangentsAt(u, v, &tu, &tv, /*retry=*/false);
                Vector normal = tu.Cross(tv);
                Require(Finite(surface.PointAt(u, v)) && Finite(normal) &&
                        normal.Magnitude() > LENGTH_EPS * std::max(tu.Magnitude(), tv.Magnitude()),
                        Code::Unsupported, face->geometry,
                        "Singular or numerically degenerate spline patch cannot be validated for native trimming.");
                if(face->planar) Require(normal.Dot(face->normal) > 0, Code::InvalidGeometry,
                        face->geometry, "Folded or reversed planar spline parameterization.");
            }
        }
        for(auto &loop : face->loops) {
            for(const auto &use : loop.edges) {
                const auto &pieces = edges.at(use.edge).pieces;
                for(size_t k = 0; k < pieces.size(); ++k) {
                    SBezier curve = pieces[use.forwards ? k : pieces.size() - 1 - k].bezier;
                    if(!use.forwards) curve.Reverse();
                    if(face->planar) for(int i = 0; i <= curve.deg; ++i) {
                        Require(std::abs(curve.ctrl[i].Minus(face->origin).Dot(face->normal)) <= LENGTH_EPS,
                                Code::InvalidGeometry, face->id, "Face boundary curve is not on its declared plane.");
                    }
                    Point2d start = Project(face, curve.Start());
                    if(loop.uv.empty()) loop.uv.push_back(start);
                    else Require(loop.uv.back().Equals(start, face->UvEps() * 4), Code::InvalidGeometry,
                                 face->id, "Face-boundary parameter path is discontinuous.");
                    // Even a straight XYZ trim may curve in a non-affine UV map.
                    int count = curve.deg == 1 && face->planar ? 1 : 16;
                    for(int i = 0; i < count; ++i) {
                        double a = i / (double)count, b = (i + 1) / (double)count;
                        SampleInterval(face, curve, a, b, Project(face, curve.PointAt(a)),
                                       Project(face, curve.PointAt(b)), &loop.uv, 0);
                    }
                }
            }
            Require(loop.uv.size() >= 4 && loop.uv.front().Equals(loop.uv.back(), face->UvEps() * 4),
                    Code::InvalidGeometry, loop.id, "Degenerate or open face boundary in surface parameters.");
            loop.uv.pop_back();
            double perimeter = 0;
            for(size_t i = 0; i < loop.uv.size(); ++i) {
                Point2d a = loop.uv[i], b = loop.uv[(i + 1) % loop.uv.size()];
                loop.twiceArea += Cross(a, b);
                perimeter += b.Minus(a).Magnitude();
            }
            Require(std::abs(loop.twiceArea) > face->UvEps() * perimeter, Code::InvalidGeometry,
                    loop.id, "Degenerate face boundary in UV.");
            Require(!loop.declaredOuter || loop.twiceArea > 0, Code::InvalidGeometry, face->id,
                    "Outer face-bound winding disagrees with the oriented surface.");
        }
        size_t outer = face->loops.size();
        for(size_t i = 0; i < face->loops.size(); ++i) if(face->loops[i].twiceArea > 0) {
            Require(outer == face->loops.size(), Code::InvalidGeometry, face->id,
                    "Face has multiple outer regions or an incorrectly oriented inner bound.");
            outer = i;
        }
        Require(outer != face->loops.size(), Code::InvalidGeometry, face->id,
                "Face has no positively oriented outer boundary.");
        size_t work = 0;
        for(size_t a = 0; a < face->loops.size(); ++a) {
            const auto &la = face->loops[a].uv;
            for(size_t b = a; b < face->loops.size(); ++b) {
                const auto &lb = face->loops[b].uv;
                for(size_t i = 0; i < la.size(); ++i) for(size_t j = a == b ? i + 1 : 0; j < lb.size(); ++j) {
                    if(a == b && (j == i + 1 || (i == 0 && j + 1 == la.size()))) continue;
                    Require(++work <= 8000000, Code::Unsupported, face->id, "Face trim-intersection validation work limit exceeded.");
                    Require(!SegmentsTouch(la[i], la[(i + 1) % la.size()], lb[j], lb[(j + 1) % lb.size()], face->UvEps()),
                            Code::InvalidGeometry, face->id, "Self-crossing or touching face trim boundaries.");
                }
                if(a != b && a != outer && b != outer) {
                    Require(InLoop(la, lb.front(), face->UvEps()) == -1 &&
                            InLoop(lb, la.front(), face->UvEps()) == -1, Code::InvalidGeometry,
                            face->id, "Nested/overlapping inner face bounds do not define one connected face.");
                }
            }
            if(a != outer) Require(InLoop(face->loops[outer].uv, la.front(), face->UvEps()) == 1,
                    Code::InvalidGeometry, face->id, "Inner face bound lies outside its outer boundary.");
        }
    }

    void ValidateTopology(int outer) {
        std::map<int, std::set<int>> neighbors;
        for(const auto &entry : edges) {
            const auto &edge = entry.second;
            Require(edge.uses.size() == 2 && edge.uses[0].face != edge.uses[1].face &&
                    edge.uses[0].forwards != edge.uses[1].forwards, Code::InvalidGeometry, edge.id,
                    "Every source edge must have exactly two distinct, oppositely oriented incident faces.");
            neighbors[edge.uses[0].face].insert(edge.uses[1].face);
            neighbors[edge.uses[1].face].insert(edge.uses[0].face);
        }
        std::set<int> seen;
        std::vector<int> pending{faces.front().id};
        while(!pending.empty()) {
            int face = pending.back(); pending.pop_back();
            if(!seen.insert(face).second) continue;
            for(int next : neighbors[face]) pending.push_back(next);
        }
        Require(seen.size() == faces.size(), Code::InvalidGeometry, outer, "Disconnected face components in CLOSED_SHELL.");
        int euler = (int)vertices.size() - (int)edges.size();
        for(const auto &face : faces) euler += 2 - (int)face.loops.size();
        Require(euler <= 2 && euler % 2 == 0, Code::InvalidGeometry, outer,
                "Source topology is not an orientable closed shell (including its inner face bounds).");
        for(auto a = vertices.begin(); a != vertices.end(); ++a) {
            for(auto b = std::next(a); b != vertices.end(); ++b) {
                Require(!a->second.position.Equals(b->second.position), Code::InvalidGeometry, b->first,
                        "Distinct source vertices coincide within native tolerance; no welding is performed.");
            }
            // Distinguish the two ends of a closed source edge. A legitimate
            // one-edge loop has two half-edges at its seam vertex, not one
            // four-valent node.
            using HalfEdge = std::pair<int, bool>;
            std::map<HalfEdge, std::vector<HalfEdge>> link;
            for(const auto &face : faces) for(const auto &loop : face.loops) {
                for(size_t i = 0; i < loop.vertices.size(); ++i) {
                    if(loop.vertices[i] != a->first) continue;
                    const auto &beforeUse = loop.edges[(i + loop.edges.size() - 1) % loop.edges.size()];
                    const auto &afterUse = loop.edges[i];
                    HalfEdge before{beforeUse.edge, beforeUse.forwards};
                    HalfEdge after{afterUse.edge, !afterUse.forwards};
                    link[before].push_back(after); link[after].push_back(before);
                }
            }
            Require(!link.empty(), Code::InvalidGeometry, a->first, "Unreferenced topological vertex.");
            std::set<HalfEdge> visited;
            std::vector<HalfEdge> todo{link.begin()->first};
            while(!todo.empty()) {
                auto edge = todo.back(); todo.pop_back();
                if(!visited.insert(edge).second) continue;
                Require(link[edge].size() == 2, Code::InvalidGeometry, a->first, "Non-manifold vertex link.");
                for(auto next : link[edge]) todo.push_back(next);
            }
            Require(visited.size() == link.size(), Code::InvalidGeometry, a->first, "Disconnected face fans at a vertex.");
        }
    }

    void MakeSeams() {
        for(auto &face : faces) {
            std::map<std::pair<int, int>, Patch *> grid;
            for(auto &patch : face.patches) grid.emplace(std::make_pair(patch.source.iu, patch.source.iv), &patch);
            for(auto &patch : face.patches) for(bool constantU : {true, false}) {
                const auto &p = patch.source;
                auto neighbor = grid.find({p.iu + (constantU ? 1 : 0), p.iv + (constantU ? 0 : 1)});
                if(neighbor == grid.end()) continue;
                SBezier curve = Nurbs::IsoCurve(p.surface, constantU, true);
                SBezier other = Nurbs::IsoCurve(neighbor->second->source.surface, constantU, false);
                for(double t : {0.0, 0.25, 0.5, 0.75, 1.0}) {
                    Require(curve.PointAt(t).Equals(other.PointAt(t)), Code::InvalidGeometry, face.geometry,
                            "Discontinuous tensor-product spline at an internal knot; no patch gap was healed.");
                }
                Point2d a = constantU ? Point2d{p.u1, p.v0} : Point2d{p.u0, p.v1};
                Point2d b = {p.u1, p.v1};
                seams.push_back({&face, &patch, neighbor->second, constantU, curve, a, b, {}});
            }
        }
        for(auto &seam : seams) {
            for(const auto &loop : seam.face->loops) for(const auto &use : loop.edges) {
                for(auto &piece : edges.at(use.edge).pieces) {
                    for(const auto &hit : Nurbs::Intersections(use.edge, piece.bezier, seam.bezier)) {
                        piece.cuts.push_back(hit.first); seam.cuts.push_back(hit.second);
                    }
                }
            }
        }
    }

    void SplitEdges() {
        for(auto &entry : edges) {
            auto &edge = entry.second;
            std::vector<Piece> pieces;
            for(const auto &piece : edge.pieces) {
                auto requested = piece.cuts;
                // STrimBy uses XYZ endpoints to terminate a trim; a single
                // span whose endpoints coincide must be split exactly first.
                if(piece.bezier.Start().Equals(piece.bezier.Finish())) requested.push_back(0.5);
                auto cuts = Nurbs::SortedCuts(piece.bezier, std::move(requested));
                for(size_t i = 1; i < cuts.size(); ++i) {
                    SBezier bezier = Nurbs::Restrict(piece.bezier, cuts[i - 1], cuts[i]);
                    Require(!bezier.Start().Equals(bezier.Finish(), LENGTH_EPS * 0.01) ||
                            cuts.size() == 2, Code::Unsupported, edge.id,
                            "Patch splitting creates a boundary segment below native tolerance; no segment was dropped.");
                    Require(segments.size() < MAX_NATIVE_CURVES, Code::Unsupported, edge.id, "Native curve-span limit exceeded.");
                    pieces.push_back({bezier, {}, segments.size()});
                    segments.push_back({bezier, edge.id});
                }
            }
            edge.pieces = std::move(pieces);
        }
    }

    void AddTrim(Patch *patch, size_t segment, bool backwards) {
        patch->trims.emplace_back(segment, backwards);
        segments[segment].uses.emplace_back(patch, backwards);
    }

    void AttachSourceTrims() {
        for(auto &face : faces) for(const auto &loop : face.loops) for(const auto &use : loop.edges) {
            for(const auto &piece : edges.at(use.edge).pieces) {
                Point2d uv = Project(&face, piece.bezier.PointAt(0.5));
                Point2d tangent = Project(&face, piece.bezier.PointAt(0.51)).Minus(
                                  Project(&face, piece.bezier.PointAt(0.49)));
                if(!use.forwards) tangent = tangent.ScaledBy(-1);
                Patch *selected = nullptr;
                for(auto &patch : face.patches) {
                    const auto &p = patch.source;
                    double eps = face.UvEps() * 4;
                    if(uv.x < p.u0 - eps || uv.x > p.u1 + eps || uv.y < p.v0 - eps || uv.y > p.v1 + eps) continue;
                    bool onU = std::abs(uv.x - p.u0) <= eps || std::abs(uv.x - p.u1) <= eps;
                    bool onV = std::abs(uv.y - p.v0) <= eps || std::abs(uv.y - p.v1) <= eps;
                    Point2d toCenter = Point2d{(p.u0 + p.u1) * 0.5, (p.v0 + p.v1) * 0.5}.Minus(uv);
                    if((onU || onV) && Cross(tangent, toCenter) <= 0) continue;
                    Require(selected == nullptr, Code::Unsupported, face.id,
                            "A boundary span cannot be assigned unambiguously to one native surface patch.");
                    selected = &patch;
                }
                Require(selected != nullptr, Code::InvalidGeometry, face.id,
                        "Boundary winding or finite surface domain leaves no native patch on the material side.");
                // STEP material is on the left; native material is on the right.
                AddTrim(selected, piece.segment, use.forwards);
            }
        }
    }

    void AttachSeams() {
        for(auto &seam : seams) {
            auto cuts = Nurbs::SortedCuts(seam.bezier, seam.cuts);
            for(size_t i = 1; i < cuts.size(); ++i) {
                double a = cuts[i - 1], b = cuts[i], m = (a + b) * 0.5;
                Point2d uv = seam.uv0.Plus(seam.uv1.Minus(seam.uv0).ScaledBy(m));
                // An exact source trim lying along a grid line belongs to only
                // its material-side cell. It must not also become an internal seam.
                bool boundary = false;
                Vector point = seam.bezier.PointAt(m);
                for(const auto &loop : seam.face->loops) for(const auto &use : loop.edges) {
                    for(const auto &piece : edges.at(use.edge).pieces) {
                        if(!Nurbs::Parameters(piece.bezier, point, LENGTH_EPS * 0.05).empty()) boundary = true;
                    }
                }
                if(boundary || InFace(*seam.face, uv) != 1) continue;
                Require(segments.size() < MAX_NATIVE_CURVES, Code::Unsupported, seam.face->id, "Native seam-curve limit exceeded.");
                size_t index = segments.size();
                segments.push_back({Nurbs::Restrict(seam.bezier, a, b), 0});
                AddTrim(seam.a, index, seam.constantU);
                AddTrim(seam.b, index, !seam.constantU);
            }
        }
    }

    void MakePwl(NativeSolid *result, SCurve *curve) {
        curve->exact.MakePwlInto(&curve->pts);
        Require(curve->pts.n <= (int)MAX_TRIM_POINTS, Code::Unsupported, result->solid,
                "Native curve trim PWL exceeds the point limit; no geometry was simplified.");
        // The native PWL is a trimming/Boolean/display aid, not replacement
        // geometry. Refine it also in UV: a straight XYZ edge can have a curved
        // inverse image on a rational or non-affine planar patch.
        std::vector<double> cuts{0, 1};
        for(const auto &point : curve->pts) {
            auto parameters = Nurbs::Parameters(curve->exact, point.p, LENGTH_EPS * 0.01);
            if(parameters.size() == 1) cuts.push_back(parameters.front());
        }
        cuts = Nurbs::SortedCuts(curve->exact, cuts);
        double tolerance = std::max(LENGTH_EPS * 4, SS.ChordTolMm());
        std::function<void(double, double, int)> refine;
        refine = [&](double a, double b, int depth) {
            Vector pa = curve->exact.PointAt(a), pb = curve->exact.PointAt(b);
            bool split = false;
            for(hSSurface handle : {curve->surfA, curve->surfB}) {
                auto surface = result->shell.surface.FindById(handle);
                Point2d auv, buv;
                surface->ClosestPointTo(pa, &auv); surface->ClosestPointTo(pb, &buv);
                for(double fraction : {0.25, 0.5, 0.75}) {
                    Point2d uv = auv.Plus(buv.Minus(auv).ScaledBy(fraction));
                    Vector p = surface->PointAt(uv);
                    double t;
                    curve->exact.ClosestPointTo(p, &t, /*mustConverge=*/false);
                    if(!std::isfinite(t) || t < a - 1e-9 || t > b + 1e-9 ||
                       !p.Equals(curve->exact.PointAt(t), tolerance)) split = true;
                }
            }
            if(split) {
                Require(depth < 16 && curve->pts.n < (int)MAX_TRIM_POINTS, Code::Unsupported,
                        result->solid, "Native UV trim PWL cannot meet its tolerance within the subdivision limit.");
                refine(a, (a + b) * 0.5, depth + 1);
                refine((a + b) * 0.5, b, depth + 1);
            } else {
                Require(curve->pts.n < (int)MAX_TRIM_POINTS, Code::Unsupported, result->solid,
                        "Native UV trim PWL exceeds the point limit; no geometry was simplified.");
                SCurvePt point = {};
                point.p = pb; point.vertex = b == 1;
                curve->pts.Add(&point);
            }
        };
        curve->pts.Clear();
        SCurvePt first = {};
        first.p = curve->exact.Start(); first.vertex = true;
        curve->pts.Add(&first);
        for(size_t i = 1; i < cuts.size(); ++i) refine(cuts[i - 1], cuts[i], 0);
    }

    void MakeNative(NativeSolid *result) {
        uint32_t nextSurface = (uint32_t)faces.back().id;
        uint32_t nextCurve = (uint32_t)edges.rbegin()->first;
        for(auto &face : faces) {
            bool first = true;
            FaceReference reference = {};
            reference.face = face.id; reference.geometry = face.geometry;
            reference.origin = face.origin; reference.normal = face.normal; reference.planar = face.planar;
            for(auto &patch : face.patches) {
                if(patch.trims.empty()) continue; // untrimmed support outside the source face
                patch.handle.v = first ? (uint32_t)face.id : ++nextSurface;
                first = false;
                SSurface surface = patch.source.surface;
                surface.h = patch.handle; surface.color = RGBi(150, 150, 150);
                // No Part 21 ID is passed off as a sketch entity handle.
                result->shell.surface.Add(&surface);
                reference.surfaces.push_back(surface.h);
            }
            Require(!reference.surfaces.empty(), Code::InvalidGeometry, face.id, "Source face has no retained native trimmed patch.");
            reference.surface = reference.surfaces.front();
            for(const auto &loop : face.loops) {
                std::vector<std::pair<int, bool>> boundary;
                for(const auto &use : loop.edges) boundary.emplace_back(use.edge, use.forwards);
                std::sort(boundary.begin(), boundary.end());
                reference.boundaries.push_back(std::move(boundary));
            }
            std::sort(reference.boundaries.begin(), reference.boundaries.end());
            result->faces.push_back(std::move(reference));
        }
        std::set<int> assigned;
        for(auto &segment : segments) {
            Require(segment.uses.size() == 2 && segment.uses[0].first != segment.uses[1].first &&
                    segment.uses[0].second != segment.uses[1].second, Code::InvalidGeometry,
                    segment.edge ? segment.edge : result->outerShell,
                    "Every native boundary/seam span must have two opposite, distinct patch uses.");
            segment.handle.v = segment.edge && assigned.insert(segment.edge).second ?
                               (uint32_t)segment.edge : ++nextCurve;
            SCurve curve = {};
            curve.h = segment.handle; curve.isExact = true; curve.exact = segment.bezier;
            curve.surfA = segment.uses[0].first->handle;
            curve.surfB = segment.uses[1].first->handle;
            result->shell.curve.Add(&curve); // own the raw list before allocating its PWL
            MakePwl(result, result->shell.curve.FindById(curve.h));
        }
        for(const auto &face : faces) for(const auto &patch : face.patches) {
            if(patch.trims.empty()) continue;
            auto surface = result->shell.surface.FindById(patch.handle);
            for(const auto &use : patch.trims) {
                auto trim = STrimBy::EntireCurve(&result->shell, segments[use.first].handle, use.second);
                surface->trim.Add(&trim);
            }
        }
        for(const auto &entry : vertices) {
            const auto &v = entry.second;
            result->vertices.push_back({v.id, v.point, v.position});
        }
        for(const auto &entry : edges) {
            const auto &edge = entry.second;
            EdgeReference reference = {};
            reference.edge = edge.id; reference.geometry = edge.geometry;
            reference.startVertex = edge.start; reference.finishVertex = edge.finish;
            reference.linear = edge.linear;
            reference.adjacentFaces[0] = std::min(edge.uses[0].face, edge.uses[1].face);
            reference.adjacentFaces[1] = std::max(edge.uses[0].face, edge.uses[1].face);
            for(const auto &piece : edge.pieces) reference.curves.push_back(segments[piece.segment].handle);
            reference.curve = reference.curves.front();
            result->edges.push_back(std::move(reference));
        }
    }

    struct Triangle {
        STriangle triangle;
        Vector min, max;
    };

    static bool InsideTriangle(const STriangle &triangle, Vector point, double eps) {
        Vector n = triangle.Normal();
        for(int i = 0; i < 3; ++i) {
            Vector a = triangle.vertices[i], d = triangle.vertices[(i + 1) % 3].Minus(a);
            if(d.Cross(point.Minus(a)).Dot(n) <= eps * d.Magnitude() * n.Magnitude()) return false;
        }
        return true;
    }

    static bool CrossesTriangle(Vector a, Vector b, const STriangle &triangle) {
        Vector n = triangle.Normal().WithMagnitude(1);
        double da = a.Minus(triangle.a).Dot(n), db = b.Minus(triangle.a).Dot(n);
        if((da > LENGTH_EPS && db > LENGTH_EPS) || (da < -LENGTH_EPS && db < -LENGTH_EPS) ||
           std::abs(da - db) < LENGTH_EPS * 0.01) return false;
        double t = da / (da - db);
        if(t < 0 || t > 1) return false;
        return InsideTriangle(triangle, a.Plus(b.Minus(a).ScaledBy(t)), LENGTH_EPS);
    }

    void ValidateIntersections(int outer, std::vector<Triangle> *triangles) {
        std::sort(triangles->begin(), triangles->end(), [](const Triangle &a, const Triangle &b) {
            return a.min.x < b.min.x;
        });
        size_t work = 0;
        for(size_t i = 0; i < triangles->size(); ++i) {
            const auto &a = (*triangles)[i];
            for(size_t j = i + 1; j < triangles->size() && (*triangles)[j].min.x <= a.max.x + LENGTH_EPS; ++j) {
                const auto &b = (*triangles)[j];
                if(a.min.y > b.max.y + LENGTH_EPS || b.min.y > a.max.y + LENGTH_EPS ||
                   a.min.z > b.max.z + LENGTH_EPS || b.min.z > a.max.z + LENGTH_EPS) continue;
                Require(++work <= 8000000, Code::Unsupported, outer, "Native shell intersection-validation work limit exceeded.");
                bool crossing = false;
                for(int k = 0; k < 3; ++k) {
                    crossing |= CrossesTriangle(a.triangle.vertices[k], a.triangle.vertices[(k + 1) % 3], b.triangle);
                    crossing |= CrossesTriangle(b.triangle.vertices[k], b.triangle.vertices[(k + 1) % 3], a.triangle);
                }
                Vector na = a.triangle.Normal().WithMagnitude(1), nb = b.triangle.Normal().WithMagnitude(1);
                if(std::abs(na.Dot(nb)) > 1 - 1e-10 &&
                   std::abs(a.triangle.a.Minus(b.triangle.a).Dot(nb)) <= LENGTH_EPS) {
                    Vector ac = a.triangle.a.Plus(a.triangle.b).Plus(a.triangle.c).ScaledBy(1.0 / 3);
                    Vector bc = b.triangle.a.Plus(b.triangle.b).Plus(b.triangle.c).ScaledBy(1.0 / 3);
                    crossing |= InsideTriangle(b.triangle, ac, LENGTH_EPS) || InsideTriangle(a.triangle, bc, LENGTH_EPS);
                }
                Require(!crossing, Code::InvalidGeometry, outer, "Native shell validation found intersecting/overlapping surface regions.");
            }
        }
    }

    void ValidateNative(NativeSolid *result) {
        std::vector<Triangle> triangles;
        Vector anchor = vertices.begin()->second.position;
        double volume = 0, area = 0;
        for(auto &surface : result->shell.surface) {
            int sourceFace = result->outerShell;
            for(const auto &face : result->faces) {
                if(std::find(face.surfaces.begin(), face.surfaces.end(), surface.h) != face.surfaces.end()) sourceFace = face.face;
            }
            EdgeListOwner edges;
            surface.MakeEdgesInto(&result->shell, &edges.edges, SSurface::MakeAs::UV);
            PolygonOwner polygon;
            Require(edges.edges.AssemblePolygon(&polygon.polygon, nullptr, /*keepDir=*/true),
                    Code::InvalidGeometry, sourceFace, "Native UV trims do not assemble into closed directed contours.");
            double twiceArea = 0;
            for(const auto &contour : polygon.polygon.l) {
                for(int i = 1; i < contour.l.n; ++i) {
                    Vector a = contour.l[i - 1].p, b = contour.l[i].p;
                    twiceArea += a.x * b.y - a.y * b.x;
                }
            }
            Require(twiceArea < 0, Code::InvalidGeometry, sourceFace,
                    "Native trim winding does not put material on the right in UV.");
            double expectedArea = -twiceArea * 0.5;
            MeshOwner mesh;
            if(surface.IsPlanar())
                polygon.polygon.UvTriangulateInto(&mesh.mesh, &surface, /*planar=*/true);
            else polygon.polygon.UvGridTriangulateInto(&mesh.mesh, &surface);
            double coveredArea = 0;
            for(auto triangle : mesh.mesh.l) {
                coveredArea += triangle.Area();
                Vector center = triangle.a.Plus(triangle.b).Plus(triangle.c).ScaledBy(1.0 / 3);
                triangle.a = surface.PointAt(triangle.a.x, triangle.a.y);
                triangle.b = surface.PointAt(triangle.b.x, triangle.b.y);
                triangle.c = surface.PointAt(triangle.c.x, triangle.c.y);
                triangle.FlipNormal();
                Require(Finite(triangle.a) && Finite(triangle.b) && Finite(triangle.c) &&
                        triangle.Normal().Dot(surface.NormalAt(center.x, center.y)) > 0,
                        Code::InvalidGeometry, sourceFace, "Folded or degenerate native triangulation of a surface patch.");
                volume += triangle.a.Minus(anchor).Dot(triangle.b.Minus(anchor).Cross(triangle.c.Minus(anchor))) / 6;
                area += triangle.Area();
                Triangle record = {triangle, triangle.a, triangle.a};
                triangle.b.MakeMaxMin(&record.max, &record.min); triangle.c.MakeMaxMin(&record.max, &record.min);
                Require(triangles.size() < MAX_VALIDATION_TRIANGLES, Code::Unsupported, result->outerShell,
                        "Native shell tessellation-validation limit exceeded; no mesh fallback is performed.");
                triangles.push_back(record);
            }
            Require(std::abs(coveredArea - expectedArea) <= std::max(1e-10, expectedArea * 1e-6),
                    Code::InvalidGeometry, sourceFace, "Native triangulation does not cover its complete trimmed UV region.");
        }
        Require(std::isfinite(volume) && std::isfinite(area) && volume > area * LENGTH_EPS,
                Code::InvalidGeometry, result->outerShell, "Non-positive or numerically degenerate enclosed native volume.");
        ValidateIntersections(result->outerShell, &triangles);
    }
};

GeometryDiagnostic Diagnostic(const Failure &failure, int representation, int solid) {
    GeometryDiagnostic diagnostic;
    diagnostic.code = failure.code;
    diagnostic.representation = representation;
    diagnostic.solid = solid;
    diagnostic.entity = failure.entity;
    diagnostic.message = failure.message;
    return diagnostic;
}

} // namespace

NativeSolid::~NativeSolid() { shell.Clear(); }

GeometryResult ConvertGeometry(const Document &document, const GeometryOptions &options) {
    GeometryResult result;
    Attributes attributes(document);
    std::map<int, int> candidates;
    int currentRepresentation = 0;
    try {
        // Occurrence paths/transforms remain a separate, explicit limitation.
        // Components must never be imported at guessed definition origins.
        for(const char *type : {"MAPPED_ITEM", "REPRESENTATION_RELATIONSHIP",
                               "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION", "ASSEMBLY_COMPONENT_USAGE"}) {
            auto entities = document.EntitiesOfType(type);
            Require(entities.empty(), Code::Representation, entities.empty() ? 0 : entities.front(),
                    "Representation relationships, mappings and assemblies are not implemented; no flattening is performed.");
        }
        auto representations = document.EntitiesOfType("SHAPE_REPRESENTATION");
        std::sort(representations.begin(), representations.end());
        for(int representation : representations) {
            currentRepresentation = representation;
            for(int item : attributes.Refs(representation, "items")) {
                if(document.IsA(item, "AXIS2_PLACEMENT_3D")) continue;
                Require(document.IsA(item, "SOLID_MODEL"), Code::Representation, item,
                        "Unsupported non-solid shape item; representation selection is not implemented.");
                Require(candidates.emplace(item, representation).second, Code::Representation, item,
                        "Solid occurs more than once; occurrence/context selection is ambiguous.");
            }
        }
    } catch(const Failure &failure) {
        result.diagnostics.push_back(Diagnostic(failure, currentRepresentation, 0));
        return result;
    }
    for(int solid : document.EntitiesOfType("SOLID_MODEL")) {
        if(candidates.count(solid)) continue;
        result.diagnostics.push_back(Diagnostic({Code::Representation, solid,
            "Solid has no supported direct shape representation/unit context; it is omitted."}, 0, solid));
    }
    for(const auto &candidate : candidates) {
        try {
            result.solids.push_back(SolidBuilder(document, options).Build(candidate.second, candidate.first));
        } catch(const Failure &failure) {
            result.diagnostics.push_back(Diagnostic(failure, candidate.second, candidate.first));
        } catch(const Nurbs::Error &failure) {
            result.diagnostics.push_back(Diagnostic({failure.unsupported ? Code::Unsupported : Code::InvalidGeometry,
                failure.entity, failure.message}, candidate.second, candidate.first));
        }
    }
    if(!result.solids.empty()) {
        result.status = result.diagnostics.empty() ? GeometryResult::Status::Okay : GeometryResult::Status::Partial;
    } else if(result.diagnostics.empty()) {
        result.diagnostics.push_back(Diagnostic({Code::NoGeometry, 0, "No supported solid geometry was found."}, 0, 0));
    }
    return result;
}

} // namespace Step
} // namespace SolveSpace
