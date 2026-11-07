// Native planar-solid proof. No STEPcode headers, types or generated bindings.
#include "stepgeometry.h"
#include "stepreader.h"

#include <algorithm>
#include <cmath>
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

// Deliberately bounded first slice: the convexity checks are quadratic. These
// are converter limits, not replacements for resource limits in the parser.
const size_t MAX_FACES = 512;
const size_t MAX_EDGES = 2048;
const size_t MAX_VERTICES = 1024;
const double MAX_COORDINATE_MM = 1e8;

void Require(bool okay, Code code, int entity, const std::string &message) {
    if(!okay) throw Failure{code, entity, message};
}

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

    double Number(const Value &value, int id) const {
        Require(value.kind == Kind::Real || value.kind == Kind::Integer,
                Code::InvalidGeometry, id, "Expected a numeric value.");
        double result = value.kind == Kind::Real ? value.real : (double)value.integer;
        Require(std::isfinite(result), Code::InvalidGeometry, id, "Non-finite numeric value.");
        return result;
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
        // Scale before normalizing, so large/small direction ratios do not
        // overflow/underflow a norm. Direction ratios have no length units.
        double largest = std::max({std::abs(direction.x), std::abs(direction.y),
                                   std::abs(direction.z)});
        Require(largest > 0, Code::InvalidGeometry, id, "Zero direction vector.");
        direction = {direction.x / largest, direction.y / largest, direction.z / largest};
        double magnitude = direction.Magnitude();
        Require(std::isfinite(magnitude) && magnitude > 0, Code::InvalidGeometry, id,
                "Unrepresentable direction vector.");
        return direction.ScaledBy(1.0 / magnitude);
    }

    void LinearKnots(int id, const std::string &multiplicities,
                     const std::string &knots) const {
        auto mult = Get(id, multiplicities), values = Get(id, knots);
        Require(mult.kind == Kind::Aggregate && values.kind == Kind::Aggregate &&
                mult.elements.size() == 2 && values.elements.size() == 2,
                Code::Unsupported, id, "Only single-span B-splines are supported.");
        for(const auto &item : mult.elements) {
            Require(item.kind == Kind::Integer && item.integer == 2, Code::InvalidGeometry,
                    id, "A linear single-span B-spline must have endpoint multiplicities (2,2).");
        }
        double first = Number(values.elements[0], id), last = Number(values.elements[1], id);
        Require(last > first && std::isfinite(last - first), Code::InvalidGeometry, id,
                "B-spline knots must define a finite, strictly increasing interval.");
    }

    void OpenSpline(int id, const std::string &flag) const {
        auto value = Get(id, flag);
        Require(value.kind == Kind::Enumeration && value.text == "F", Code::Unsupported, id,
                "Only explicitly open, non-self-intersecting B-splines are supported.");
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
                        (i == 0 ? 1.0 : 0.0), Code::Units, unit,
                        "Unit dimensions are not length.");
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

struct Vertex {
    int id, point;
    Vector position;
};

struct Edge {
    int id, geometry, start, finish;
    struct Use { int face; bool forwards; };
    std::vector<Use> uses;
};

struct EdgeUse { int edge; bool forwards; };

struct Face {
    int id, geometry;
    Vector origin, x, y, normal;
    bool bounded; // a finite affine spline patch, rather than an unbounded PLANE
    bool rectangular; // source u/v are orthogonal, suitable for native planar UV
    Vector patch[2][2]; // normalized native u/v, already oriented by same_sense
    std::vector<EdgeUse> loop; // STEP's effective face-bound direction
    std::vector<int> vertices;
};

class SolidBuilder : public Attributes {
public:
    explicit SolidBuilder(const Document &document) : Attributes(document) {}

    std::unique_ptr<NativeSolid> Build(int representation, int solid) {
        int context = Ref(representation, "context_of_items");
        scale = Units(document).ForContext(context);
        Require(document.IsA(solid, "MANIFOLD_SOLID_BREP"), Code::Unsupported, solid,
                "Only MANIFOLD_SOLID_BREP is supported by the planar slice.");
        Require(!document.IsA(solid, "BREP_WITH_VOIDS"), Code::Unsupported, solid,
                "Cavity conversion is not implemented; the entire solid is omitted.");
        int outer = Ref(solid, "outer");
        auto shellTypes = document.EntityTypes(outer);
        Require(shellTypes.size() == 1 && shellTypes[0] == "CLOSED_SHELL", Code::Unsupported,
                outer, "Only a direct CLOSED_SHELL is supported.");
        auto faceIds = Refs(outer, "cfs_faces");
        Require(faceIds.size() <= MAX_FACES, Code::Unsupported, outer,
                "Planar proof face limit exceeded.");
        Require(faceIds.size() >= 4, Code::InvalidGeometry, outer, "Too few faces for a closed solid.");
        std::sort(faceIds.begin(), faceIds.end());
        Require(std::adjacent_find(faceIds.begin(), faceIds.end()) == faceIds.end(),
                Code::InvalidGeometry, outer, "Repeated face in CLOSED_SHELL.");
        for(int face : faceIds) faces.push_back(ReadFace(face));
        Validate(outer);

        std::unique_ptr<NativeSolid> result(new NativeSolid);
        result->representation = representation;
        result->context = context;
        result->solid = solid;
        result->outerShell = outer;
        result->lengthToMm = scale;
        MakeNative(result.get());
        return result;
    }

private:
    double scale = 0;
    std::map<int, Vertex> vertices;
    std::map<int, Edge> edges;
    std::vector<Face> faces;

    Vector Point(int id) {
        Require(document.IsA(id, "CARTESIAN_POINT"), Code::InvalidGeometry, id,
                "Expected CARTESIAN_POINT.");
        Vector point = Triple(id, "coordinates").ScaledBy(scale);
        Require(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
                std::abs(point.x) <= MAX_COORDINATE_MM &&
                std::abs(point.y) <= MAX_COORDINATE_MM &&
                std::abs(point.z) <= MAX_COORDINATE_MM, Code::Unsupported, id,
                "Coordinates exceed the planar proof's finite +/-1e8 mm envelope.");
        return point;
    }

    const Vertex &ReadVertex(int id) {
        auto found = vertices.find(id);
        if(found != vertices.end()) return found->second;
        Require(document.IsA(id, "VERTEX_POINT"), Code::Unsupported, id,
                "Only VERTEX_POINT vertices are supported.");
        Require(vertices.size() < MAX_VERTICES, Code::Unsupported, id,
                "Planar proof vertex limit exceeded.");
        int point = Ref(id, "vertex_geometry");
        return vertices.emplace(id, Vertex{id, point, Point(point)}).first->second;
    }

    Edge &ReadEdge(int id) {
        auto found = edges.find(id);
        if(found != edges.end()) return found->second;
        Require(document.IsA(id, "EDGE_CURVE"), Code::Unsupported, id,
                "Only EDGE_CURVE edges are supported.");
        Require(edges.size() < MAX_EDGES, Code::Unsupported, id, "Planar proof edge limit exceeded.");
        int start = Ref(id, "edge_start"), finish = Ref(id, "edge_end");
        auto a = ReadVertex(start).position, b = ReadVertex(finish).position;
        Vector delta = b.Minus(a);
        Require(start != finish && delta.Magnitude() > LENGTH_EPS, Code::InvalidGeometry, id,
                "Closed or zero-length linear edge.");
        int geometry = Ref(id, "edge_geometry");
        Vector origin, direction;
        double span = 0;
        if(document.IsA(geometry, "LINE")) {
            origin = Point(Ref(geometry, "pnt"));
            int vector = Ref(geometry, "dir");
            direction = Direction(Ref(vector, "orientation"));
            Require(Number(Get(vector, "magnitude"), vector) > 0, Code::InvalidGeometry, vector,
                    "LINE vector magnitude must be positive.");
        } else {
            Require(document.IsA(geometry, "B_SPLINE_CURVE_WITH_KNOTS") &&
                    !document.IsA(geometry, "RATIONAL_B_SPLINE_CURVE"), Code::Unsupported,
                    geometry, "Only LINE or non-rational single-span linear B-spline edges are supported.");
            auto degree = Get(geometry, "degree");
            auto controls = Refs(geometry, "control_points_list");
            Require(degree.kind == Kind::Integer && degree.integer == 1 && controls.size() == 2,
                    Code::Unsupported, geometry,
                    "Only degree-1 B-spline curves with two control points are supported.");
            LinearKnots(geometry, "knot_multiplicities", "knots");
            OpenSpline(geometry, "closed_curve");
            OpenSpline(geometry, "self_intersect");
            origin = Point(controls[0]);
            direction = Point(controls[1]).Minus(origin);
            span = direction.Magnitude();
            Require(span > LENGTH_EPS, Code::InvalidGeometry, geometry,
                    "Degenerate linear B-spline control segment.");
            direction = direction.ScaledBy(1.0 / span);
        }
        Require(a.Minus(origin).Cross(direction).Magnitude() <= LENGTH_EPS &&
                b.Minus(origin).Cross(direction).Magnitude() <= LENGTH_EPS,
                Code::InvalidGeometry, id, "Edge vertices do not lie on their linear geometry.");
        if(span > 0) {
            for(Vector point : {a, b}) {
                double distance = point.Minus(origin).Dot(direction);
                Require(distance >= -LENGTH_EPS && distance <= span + LENGTH_EPS,
                        Code::InvalidGeometry, id,
                        "Edge vertices lie outside the finite B-spline curve; "
                        "no extrapolation is performed.");
            }
        }
        Require(delta.Dot(direction) * (Boolean(id, "same_sense") ? 1 : -1) > LENGTH_EPS,
                Code::InvalidGeometry, id, "EDGE_CURVE same_sense disagrees with its linear geometry.");
        return edges.emplace(id, Edge{id, geometry, start, finish, {}}).first->second;
    }

    void ReadPlane(Face *face) {
        int placement = Ref(face->geometry, "position");
        Require(document.IsA(placement, "AXIS2_PLACEMENT_3D"), Code::InvalidGeometry, placement,
                "PLANE needs AXIS2_PLACEMENT_3D.");
        face->origin = Point(Ref(placement, "location"));
        auto axis = Get(placement, "axis");
        Require(axis.kind == Kind::Null || axis.kind == Kind::Reference,
                Code::InvalidGeometry, placement, "Invalid optional plane axis.");
        Vector z = axis.kind == Kind::Null ? Vector{0, 0, 1} : Direction(axis.reference);
        auto ref = Get(placement, "ref_direction");
        Require(ref.kind == Kind::Null || ref.kind == Kind::Reference,
                Code::InvalidGeometry, placement, "Invalid optional plane reference direction.");
        // EXPRESS build_axes / first_proj_axis, not an arbitrary model rotation.
        Vector x = ref.kind == Kind::Reference ? Direction(ref.reference) :
                   (z.Equals({1, 0, 0}, 1e-12) ? Vector{0, 1, 0} : Vector{1, 0, 0});
        x = x.Minus(z.ScaledBy(x.Dot(z)));
        Require(x.Magnitude() > 1e-12, Code::InvalidGeometry, placement,
                "Plane axis and reference direction are parallel.");
        face->x = x.ScaledBy(1.0 / x.Magnitude());
        face->normal = Boolean(face->id, "same_sense") ? z : z.Negated();
        face->y = face->normal.Cross(face->x);
    }

    void ReadSplinePlane(Face *face) {
        int geometry = face->geometry;
        Require(document.IsA(geometry, "B_SPLINE_SURFACE_WITH_KNOTS") &&
                !document.IsA(geometry, "RATIONAL_B_SPLINE_SURFACE"), Code::Unsupported,
                geometry, "Only PLANE or non-rational single-span affine B-spline faces are supported.");
        for(const char *name : {"u_degree", "v_degree"}) {
            auto degree = Get(geometry, name);
            Require(degree.kind == Kind::Integer && degree.integer == 1, Code::Unsupported,
                    geometry, "Only degree-(1,1) affine B-spline surfaces are supported.");
        }
        auto controls = Get(geometry, "control_points_list");
        Require(controls.kind == Kind::Aggregate && controls.elements.size() == 2,
                Code::Unsupported, geometry, "Only 2x2 B-spline control nets are supported.");
        double largest = 1;
        for(int i = 0; i < 2; ++i) {
            const auto &row = controls.elements[i];
            Require(row.kind == Kind::Aggregate && row.elements.size() == 2, Code::Unsupported,
                    geometry, "Only 2x2 B-spline control nets are supported.");
            for(int j = 0; j < 2; ++j) {
                const auto &point = row.elements[j];
                Require(point.kind == Kind::Reference, Code::InvalidGeometry, geometry,
                        "Expected Cartesian-point references in the B-spline control net.");
                Vector p = face->patch[i][j] = Point(point.reference);
                largest = std::max({largest, std::abs(p.x), std::abs(p.y), std::abs(p.z)});
            }
        }
        LinearKnots(geometry, "u_multiplicities", "u_knots");
        LinearKnots(geometry, "v_multiplicities", "v_knots");
        for(const char *flag : {"u_closed", "v_closed", "self_intersect"}) OpenSpline(geometry, flag);

        if(!Boolean(face->id, "same_sense")) std::swap(face->patch[0][1], face->patch[1][0]);
        face->origin = face->patch[0][0];
        Vector du = face->patch[1][0].Minus(face->origin);
        Vector dv = face->patch[0][1].Minus(face->origin);
        Vector mixed = face->patch[1][1].Minus(face->patch[1][0]).Minus(dv);
        // Recognize affine geometry only at floating-point roundoff scale,
        // not by fitting/projecting a warped patch within model tolerance.
        // Rectangles keep all four controls in the native patch; skew affine
        // supports are exactly reparameterized to suit native planar UV.
        double roundoff = std::min(LENGTH_EPS * 0.01,
                                  32 * std::numeric_limits<double>::epsilon() * largest);
        Require(mixed.Magnitude() <= roundoff, Code::Unsupported, geometry,
                "Non-affine bilinear B-spline surface is outside the planar slice; "
                "no flattening is performed.");
        Vector cross = du.Cross(dv);
        double area = cross.Magnitude();
        Require(du.Magnitude() > LENGTH_EPS && dv.Magnitude() > LENGTH_EPS &&
                area > LENGTH_EPS * std::max(du.Magnitude(), dv.Magnitude()),
                Code::InvalidGeometry, geometry, "Degenerate affine B-spline patch.");
        face->x = du.ScaledBy(1.0 / du.Magnitude());
        face->normal = cross.ScaledBy(1.0 / area);
        face->y = face->normal.Cross(face->x);
        face->bounded = true;
        face->rectangular = std::abs(du.Dot(dv)) <=
                            roundoff * std::min(du.Magnitude(), dv.Magnitude());
    }

    void CheckPatchDomain(const Face &face, Vector point) {
        Vector du = face.patch[1][0].Minus(face.origin);
        Vector dv = face.patch[0][1].Minus(face.origin);
        double area = du.Cross(dv).Magnitude();
        Vector uDual = dv.Cross(face.normal).ScaledBy(1.0 / area);
        Vector vDual = face.normal.Cross(du).ScaledBy(1.0 / area);
        Vector delta = point.Minus(face.origin);
        double u = delta.Dot(uDual), v = delta.Dot(vDual);
        double ueps = LENGTH_EPS * uDual.Magnitude(), veps = LENGTH_EPS * vDual.Magnitude();
        Require(u >= -ueps && u <= 1 + ueps && v >= -veps && v <= 1 + veps,
                Code::InvalidGeometry, face.id,
                "Face boundary lies outside the finite B-spline patch; no extrapolation is performed.");
    }

    Face ReadFace(int id) {
        Require(document.IsA(id, "ADVANCED_FACE"), Code::Unsupported, id,
                "Only ADVANCED_FACE is supported by the planar slice.");
        Face face = {};
        face.id = id;
        face.geometry = Ref(id, "face_geometry");
        if(document.IsA(face.geometry, "PLANE")) ReadPlane(&face);
        else ReadSplinePlane(&face);

        auto bounds = Refs(id, "bounds");
        Require(bounds.size() == 1, Code::Unsupported, id,
                "Only a single outer face bound is supported; no holes are dropped.");
        int bound = bounds.front(), loop = Ref(bound, "bound");
        Require(document.IsA(loop, "EDGE_LOOP"), Code::Unsupported, loop,
                "Only EDGE_LOOP face boundaries are supported.");
        auto orientedEdges = Refs(loop, "edge_list");
        Require(orientedEdges.size() >= 3 && orientedEdges.size() <= MAX_EDGES,
                Code::InvalidGeometry, loop, "Invalid planar boundary edge count.");
        std::set<int> uniqueEdges;
        for(int oriented : orientedEdges) {
            Require(document.IsA(oriented, "ORIENTED_EDGE"), Code::Unsupported, oriented,
                    "Expected ORIENTED_EDGE in EDGE_LOOP.");
            int edge = Ref(oriented, "edge_element");
            ReadEdge(edge);
            Require(uniqueEdges.insert(edge).second, Code::InvalidGeometry, loop,
                    "Repeated edge within a face boundary.");
            face.loop.push_back({edge, Boolean(oriented, "orientation")});
        }
        if(!Boolean(bound, "orientation")) {
            std::reverse(face.loop.begin(), face.loop.end());
            for(auto &use : face.loop) use.forwards = !use.forwards;
        }
        std::set<int> uniqueVertices;
        for(size_t i = 0; i < face.loop.size(); ++i) {
            const auto &use = face.loop[i];
            auto &edge = edges.at(use.edge);
            int start = use.forwards ? edge.start : edge.finish;
            int finish = use.forwards ? edge.finish : edge.start;
            const auto &nextUse = face.loop[(i + 1) % face.loop.size()];
            const auto &nextEdge = edges.at(nextUse.edge);
            Require(finish == (nextUse.forwards ? nextEdge.start : nextEdge.finish),
                    Code::InvalidGeometry, loop, "Boundary is not closed by source vertex IDs.");
            Require(uniqueVertices.insert(start).second, Code::InvalidGeometry, loop,
                    "Repeated vertex in planar face boundary.");
            face.vertices.push_back(start);
            Require(edge.uses.size() < 2, Code::InvalidGeometry, edge.id,
                    "More than two incident faces on an edge.");
            edge.uses.push_back({id, use.forwards});
        }

        Vector anchor = vertices.at(face.vertices.front()).position;
        double twiceArea = 0, perimeter = 0;
        for(size_t i = 0; i < face.vertices.size(); ++i) {
            Vector a = vertices.at(face.vertices[i]).position;
            Vector b = vertices.at(face.vertices[(i + 1) % face.vertices.size()]).position;
            Require(std::abs(a.Minus(face.origin).Dot(face.normal)) <= LENGTH_EPS,
                    Code::InvalidGeometry, id, "Face vertices are not on the declared planar geometry.");
            if(face.bounded) CheckPatchDomain(face, a);
            twiceArea += a.Minus(anchor).Cross(b.Minus(anchor)).Dot(face.normal);
            perimeter += b.Minus(a).Magnitude();
        }
        Require(twiceArea > LENGTH_EPS * perimeter, Code::InvalidGeometry, id,
                "Degenerate face or boundary winding disagrees with the face normal.");
        // All vertices must be to the left of each directed STEP edge. This
        // accepts only simple convex polygons, not self-crossings or concavities.
        for(size_t i = 0; i < face.vertices.size(); ++i) {
            Vector a = vertices.at(face.vertices[i]).position;
            Vector d = vertices.at(face.vertices[(i + 1) % face.vertices.size()]).position.Minus(a);
            for(int vertex : face.vertices) {
                Require(d.Cross(vertices.at(vertex).position.Minus(a)).Dot(face.normal) >=
                        -LENGTH_EPS * d.Magnitude(), Code::Unsupported, id,
                        "Non-convex or self-crossing planar face is outside this slice.");
            }
        }
        return face;
    }

    void Validate(int outer) {
        std::map<int, std::set<int>> neighbors;
        for(const auto &entry : edges) {
            const auto &edge = entry.second;
            Require(edge.uses.size() == 2 && edge.uses[0].face != edge.uses[1].face &&
                    edge.uses[0].forwards != edge.uses[1].forwards,
                    Code::InvalidGeometry, edge.id,
                    "Every edge must have exactly two distinct, oppositely oriented incident faces.");
            neighbors[edge.uses[0].face].insert(edge.uses[1].face);
            neighbors[edge.uses[1].face].insert(edge.uses[0].face);
        }
        std::set<int> seen;
        std::vector<int> pending{faces.front().id};
        while(!pending.empty()) {
            int face = pending.back();
            pending.pop_back();
            if(!seen.insert(face).second) continue;
            for(int next : neighbors[face]) pending.push_back(next);
        }
        Require(seen.size() == faces.size(), Code::InvalidGeometry, outer,
                "Disconnected face components in CLOSED_SHELL.");
        Require((int)vertices.size() - (int)edges.size() + (int)faces.size() == 2,
                Code::InvalidGeometry, outer, "Not a genus-zero closed polyhedron.");

        for(auto a = vertices.begin(); a != vertices.end(); ++a) {
            for(auto b = std::next(a); b != vertices.end(); ++b) {
                Require(!a->second.position.Equals(b->second.position), Code::InvalidGeometry,
                        b->first, "Distinct source vertices coincide within native tolerance; "
                                  "no welding is performed.");
            }
            // The link of a manifold vertex must be one cycle, not two face
            // fans touching at a point. Nodes here are the incident edge IDs.
            std::map<int, std::vector<int>> link;
            for(const auto &face : faces) {
                for(size_t i = 0; i < face.vertices.size(); ++i) {
                    if(face.vertices[i] != a->first) continue;
                    int before = face.loop[(i + face.loop.size() - 1) % face.loop.size()].edge;
                    int after = face.loop[i].edge;
                    link[before].push_back(after);
                    link[after].push_back(before);
                }
            }
            std::set<int> visited;
            std::vector<int> todo{link.begin()->first};
            while(!todo.empty()) {
                int edge = todo.back();
                todo.pop_back();
                if(!visited.insert(edge).second) continue;
                Require(link[edge].size() == 2, Code::InvalidGeometry, a->first,
                        "Non-manifold vertex link.");
                for(int next : link[edge]) todo.push_back(next);
            }
            Require(visited.size() == link.size(), Code::InvalidGeometry, a->first,
                    "Disconnected face fans at a vertex.");
        }

        Vector anchor = vertices.begin()->second.position;
        double volume = 0, area = 0;
        for(const auto &face : faces) {
            for(const auto &vertex : vertices) {
                Require(vertex.second.position.Minus(face.origin).Dot(face.normal) <= LENGTH_EPS,
                        Code::Unsupported, face.id,
                        "Non-convex or inward-oriented solid is outside this slice.");
            }
            Vector a = vertices.at(face.vertices[0]).position;
            for(size_t i = 1; i + 1 < face.vertices.size(); ++i) {
                Vector b = vertices.at(face.vertices[i]).position;
                Vector c = vertices.at(face.vertices[i + 1]).position;
                volume += a.Minus(anchor).Dot(b.Minus(anchor).Cross(c.Minus(anchor))) / 6.0;
                area += b.Minus(a).Cross(c.Minus(a)).Magnitude() / 2.0;
            }
        }
        Require(std::isfinite(volume) && std::isfinite(area) && volume > area * LENGTH_EPS,
                Code::InvalidGeometry, outer, "Non-positive or numerically degenerate enclosed volume.");
    }

    void MakeNative(NativeSolid *result) {
        for(const auto &entry : vertices) {
            const auto &vertex = entry.second;
            result->vertices.push_back({vertex.id, vertex.point, vertex.position});
        }
        for(const auto &entry : edges) {
            const auto &edge = entry.second;
            SCurve curve = {};
            curve.h.v = (uint32_t)edge.id;
            curve.isExact = true;
            curve.exact = SBezier::From(vertices.at(edge.start).position,
                                        vertices.at(edge.finish).position);
            curve.surfA.v = (uint32_t)edge.uses[0].face;
            curve.surfB.v = (uint32_t)edge.uses[1].face;
            // The shell owns each raw List before it is allocated, including
            // allocation-failure paths. Lines need only their exact endpoints.
            result->shell.curve.Add(&curve);
            auto native = result->shell.curve.FindById(curve.h);
            for(int i = 0; i < 2; ++i) {
                SCurvePt point = {};
                point.p = curve.exact.ctrl[i];
                point.vertex = true;
                native->pts.Add(&point);
            }
            result->edges.push_back({edge.id, edge.geometry, curve.h, edge.start, edge.finish});
        }
        for(const auto &face : faces) {
            SSurface surface = {};
            if(face.bounded && face.rectangular) {
                // A clamped degree-(1,1) single span is exactly one native
                // Bezier patch after normalizing its knot intervals to [0,1].
                surface.degm = surface.degn = 1;
                for(int i = 0; i < 2; ++i) {
                    for(int j = 0; j < 2; ++j) {
                        surface.ctrl[i][j] = face.patch[i][j];
                        surface.weight[i][j] = 1;
                    }
                }
            } else {
                // Native planar ClosestPointTo assumes orthogonal u/v. A skew
                // affine patch is exactly a plane: reparameterize its support,
                // after checking trims against the original finite domain.
                // No source boundary moves and no non-affine patch is fitted.
                double xmin = MAX_COORDINATE_MM * 4, xmax = -xmin;
                double ymin = xmin, ymax = xmax;
                for(int vertex : face.vertices) {
                    Vector delta = vertices.at(vertex).position.Minus(face.origin);
                    double x = delta.Dot(face.x), y = delta.Dot(face.y);
                    xmin = std::min(xmin, x); xmax = std::max(xmax, x);
                    ymin = std::min(ymin, y); ymax = std::max(ymax, y);
                }
                double xpad = std::max((xmax - xmin) * 0.05, 8 * LENGTH_EPS);
                double ypad = std::max((ymax - ymin) * 0.05, 8 * LENGTH_EPS);
                xmin -= xpad; xmax += xpad;
                ymin -= ypad; ymax += ypad;
                Vector origin = face.origin.Plus(face.x.ScaledBy(xmin)).Plus(face.y.ScaledBy(ymin));
                // FromPlane stores its first argument direction along native v,
                // and its second along native u. Swap x/y for an outward normal.
                surface = SSurface::FromPlane(origin,
                    face.y.ScaledBy(ymax - ymin), face.x.ScaledBy(xmax - xmin));
            }
            surface.h.v = (uint32_t)face.id;
            surface.color = RGBi(150, 150, 150);
            // Do NOT use Part 21 IDs as fake sketch hEntities in surface.face
            // or exact.entity. The link layer must allocate those separately.
            result->shell.surface.Add(&surface);
            auto native = result->shell.surface.FindById(surface.h);
            for(const auto &use : face.loop) {
                // STEP's outer bound has material to its left; native trims
                // have material to their right in UV. Reverse each edge use.
                auto trim = STrimBy::EntireCurve(&result->shell,
                    hSCurve{(uint32_t)use.edge}, use.forwards);
                native->trim.Add(&trim);
            }
            result->faces.push_back({face.id, face.geometry, surface.h, face.origin, face.normal});
        }
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

GeometryResult ConvertGeometry(const Document &document) {
    GeometryResult result;
    Attributes attributes(document);
    std::map<int, int> candidates; // solid -> representation; sorted source IDs
    int currentRepresentation = 0;
    try {
        // Until occurrence paths and transforms exist, reject these structures
        // before constructing anything. Importing a component at its definition
        // origin would look plausible but be geometrically wrong.
        for(const char *type : {"MAPPED_ITEM", "REPRESENTATION_RELATIONSHIP",
                               "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION",
                               "ASSEMBLY_COMPONENT_USAGE"}) {
            auto entities = document.EntitiesOfType(type);
            Require(entities.empty(), Code::Representation, entities.empty() ? 0 : entities.front(),
                    "Representation relationships, mappings and assemblies are not implemented; "
                    "no flattening is performed.");
        }
        auto representations = document.EntitiesOfType("SHAPE_REPRESENTATION");
        std::sort(representations.begin(), representations.end());
        for(int representation : representations) {
            currentRepresentation = representation;
            for(int item : attributes.Refs(representation, "items")) {
                if(document.IsA(item, "AXIS2_PLACEMENT_3D")) continue; // not a placement transform
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
            result.solids.push_back(SolidBuilder(document).Build(candidate.second, candidate.first));
        } catch(const Failure &failure) {
            result.diagnostics.push_back(Diagnostic(failure, candidate.second, candidate.first));
        }
    }
    if(!result.solids.empty()) {
        result.status = result.diagnostics.empty() ? GeometryResult::Status::Okay :
                                                    GeometryResult::Status::Partial;
    } else if(result.diagnostics.empty()) {
        result.diagnostics.push_back(Diagnostic({Code::NoGeometry, 0,
            "No supported solid geometry was found."}, 0, 0));
    }
    return result;
}

} // namespace Step
} // namespace SolveSpace
