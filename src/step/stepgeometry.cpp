// Native B-rep conversion. No STEPcode headers, types or generated bindings.
#include "solvespace.h"
#include "stepcolor.h"
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
const size_t MAX_ASSEMBLY_SURFACES = 32768;
const size_t MAX_ASSEMBLY_CURVES = 65536;
const size_t MAX_ASSEMBLY_TRIM_POINTS = 1048576;
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

    double AnglesForContext(int context) {
        int angle = 0;
        for(int unit : Refs(context, "units")) {
            if(!document.IsA(unit, "PLANE_ANGLE_UNIT")) continue;
            Require(angle == 0, Code::Units, context, "Ambiguous plane-angle units in context.");
            angle = unit;
        }
        Require(angle != 0, Code::Units, context, "No explicit PLANE_ANGLE_UNIT for a conical semi-angle.");
        return AngleScale(angle);
    }

private:
    std::set<int> active;

    double AngleScale(int unit) {
        Require(active.size() < 16 && active.insert(unit).second, Code::Units, unit,
                "Cyclic or excessively nested plane-angle units.");
        Require(document.IsA(unit, "PLANE_ANGLE_UNIT"), Code::Units, unit,
                "A plane-angle conversion factor must resolve to PLANE_ANGLE_UNIT.");
        auto dimensions = Get(unit, "dimensions");
        if(dimensions.kind != Kind::Derived) {
            Require(dimensions.kind == Kind::Reference, Code::Units, unit,
                    "Missing plane-angle dimensional exponents.");
            for(const char *name : {"length_exponent", "mass_exponent", "time_exponent",
                                   "electric_current_exponent", "thermodynamic_temperature_exponent",
                                   "amount_of_substance_exponent", "luminous_intensity_exponent"}) {
                Require(Number(Get(dimensions.reference, name), dimensions.reference) == 0,
                        Code::Units, unit, "Plane-angle units must be dimensionless.");
            }
        }
        double scale = 0;
        if(document.IsA(unit, "SI_UNIT")) {
            auto name = Get(unit, "name"), prefix = Get(unit, "prefix");
            Require(name.kind == Kind::Enumeration && name.text == "RADIAN" &&
                    prefix.kind == Kind::Null, Code::Units, unit,
                    "Only unprefixed SI RADIAN is implemented as a base plane-angle unit.");
            scale = 1;
        } else if(document.IsA(unit, "CONVERSION_BASED_UNIT")) {
            int measure = Ref(unit, "conversion_factor");
            auto value = Get(measure, "value_component");
            Require(value.selectedType == "PLANE_ANGLE_MEASURE" ||
                    value.selectedType == "POSITIVE_PLANE_ANGLE_MEASURE", Code::Units, measure,
                    "Plane-angle conversion needs a typed plane-angle measure.");
            double factor = Number(value, measure);
            Require(factor > 0, Code::Units, measure, "Non-positive plane-angle conversion factor.");
            scale = factor * AngleScale(Ref(measure, "unit_component"));
        } else {
            throw Failure{Code::Units, unit, "Unsupported plane-angle unit definition."};
        }
        Require(std::isfinite(scale) && scale > 0, Code::Units, unit,
                "Plane-angle conversion is non-finite or non-positive.");
        active.erase(unit);
        return scale;
    }

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

struct Placement { Vector origin, x, y, z; };

Vector ReadPoint(const Attributes &attributes, int id, double scale) {
    Require(attributes.document.IsA(id, "CARTESIAN_POINT"), Code::InvalidGeometry, id,
            "Expected CARTESIAN_POINT.");
    Vector point = attributes.Triple(id, "coordinates").ScaledBy(scale);
    Require(Finite(point) && std::abs(point.x) <= MAX_COORDINATE_MM &&
            std::abs(point.y) <= MAX_COORDINATE_MM && std::abs(point.z) <= MAX_COORDINATE_MM,
            Code::Unsupported, id, "Coordinates exceed the finite +/-1e8 mm native envelope.");
    return point;
}

Placement ReadFrame(const Attributes &attributes, int id, double scale) {
    Require(attributes.document.IsA(id, "AXIS2_PLACEMENT_3D"), Code::InvalidGeometry, id,
            "Geometry and rigid assembly frames need AXIS2_PLACEMENT_3D.");
    Placement result;
    result.origin = ReadPoint(attributes, attributes.Ref(id, "location"), scale);
    auto axis = attributes.Get(id, "axis"), ref = attributes.Get(id, "ref_direction");
    Require((axis.kind == Kind::Null || axis.kind == Kind::Reference) &&
            (ref.kind == Kind::Null || ref.kind == Kind::Reference),
            Code::InvalidGeometry, id, "Invalid optional placement axes.");
    result.z = axis.kind == Kind::Null ? Vector{0, 0, 1} : attributes.Direction(axis.reference);
    // EXPRESS changes the default reference only at exactly +/-X. A nearly
    // parallel default must fail below, not silently select another rotation.
    bool alongX = EXACT(std::abs(result.z.x) == 1) && EXACT(result.z.y == 0) && EXACT(result.z.z == 0);
    result.x = ref.kind == Kind::Reference ? attributes.Direction(ref.reference) :
               (alongX ? Vector{0, 1, 0} : Vector{1, 0, 0});
    result.x = result.x.Minus(result.z.ScaledBy(result.x.Dot(result.z)));
    Require(result.x.Magnitude() > 1e-12, Code::InvalidGeometry, id,
            "Placement axis and reference direction are parallel.");
    result.x = result.x.WithMagnitude(1);
    result.y = result.z.Cross(result.x);
    return result;
}

struct RigidPlacement {
    Vector translation = {};
    Quaternion rotation = Quaternion::IDENTITY;

    Vector Apply(Vector point) const { return rotation.Rotate(point).Plus(translation); }

    RigidPlacement Inverse() const {
        Quaternion q = rotation.Inverse();
        return {q.Rotate(translation.Negated()), q};
    }

    RigidPlacement Compose(const RigidPlacement &child) const {
        return {Apply(child.translation), rotation.Times(child.rotation).WithMagnitude(1)};
    }

    static RigidPlacement Frame(const Placement &frame, int entity) {
        // A near-parallel explicit axis pair can lose orthogonality during
        // projection. Do not let quaternion construction choose a nearby frame.
        Require(std::abs(frame.x.Dot(frame.z)) <= 1e-12 &&
                std::abs(frame.x.Dot(frame.y)) <= 1e-12 &&
                std::abs(frame.y.Dot(frame.z)) <= 1e-12 &&
                std::abs(frame.y.MagSquared() - 1) <= 1e-12,
                Code::Representation, entity, "Numerically unresolved rigid placement axes; no nearby frame was substituted.");
        return {frame.origin, Quaternion::From(frame.x, frame.y).WithMagnitude(1)};
    }
};

struct SolidOccurrence {
    int representation, solid;
    RigidPlacement placement;
    std::vector<int> path;
};

// Resolve the complete placement graph before accepting any solid. Definition
// geometry is never imported again at its local origin just because an assembly
// relationship was unsupported. Plain relationships still select one geometry
// representation; only explicit product usages/mapped items create occurrences.
class OccurrenceGraph : public Attributes {
public:
    explicit OccurrenceGraph(const Document &document) : Attributes(document) {}
    int currentRepresentation = 0;
    std::map<int, int> candidates; // source solid -> geometry representation

    std::vector<SolidOccurrence> Resolve() {
        auto dependent = document.EntitiesOfType("CONTEXT_DEPENDENT_SHAPE_REPRESENTATION");
        Require(dependent.size() <= MAX_OCCURRENCES, Code::Representation, 0,
                "Assembly occurrence count exceeds the supported work limit.");
        std::set<int> occurrenceRelations;
        for(int id : dependent) occurrenceRelations.insert(Ref(id, "representation_relation"));

        std::set<int> contentRoots;
        auto representations = document.EntitiesOfType("SHAPE_REPRESENTATION");
        std::sort(representations.begin(), representations.end());
        Require(representations.size() <= MAX_OCCURRENCES, Code::Representation, 0,
                "Shape representation count exceeds the supported work limit.");
        for(int representation : representations) {
            currentRepresentation = representation;
            parents.emplace(representation, representation);
            contexts.emplace(representation, Ref(representation, "context_of_items"));
            for(int item : Refs(representation, "items")) {
                if(document.IsA(item, "AXIS2_PLACEMENT_3D")) {
                    frameOwners[item].insert(representation);
                    continue;
                }
                if(document.IsA(item, "MAPPED_ITEM")) {
                    Require(mappedOwners.emplace(item, representation).second, Code::Representation, item,
                            "A MAPPED_ITEM must have one unambiguous owning shape representation.");
                } else {
                    Require(document.IsA(item, "SOLID_MODEL"), Code::Representation, item,
                            "Unsupported non-solid shape item; representation selection is not implemented.");
                    Require(candidates.emplace(item, representation).second, Code::Representation, item,
                            "Solid occurs in multiple direct representations; its unit/context ownership is ambiguous.");
                }
                contentRoots.insert(representation);
            }
        }

        auto relationships = document.EntitiesOfType("REPRESENTATION_RELATIONSHIP");
        std::sort(relationships.begin(), relationships.end());
        for(int relationship : relationships) {
            currentRepresentation = 0;
            bool supported = document.IsA(relationship, "SHAPE_REPRESENTATION_RELATIONSHIP");
            for(const auto &type : document.EntityTypes(relationship)) {
                if(type != "SHAPE_REPRESENTATION_RELATIONSHIP" && type != "REPRESENTATION_RELATIONSHIP" &&
                   type != "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION") supported = false;
            }
            Require(supported, Code::Representation, relationship,
                    "Only plain shape relationships or declared rigid assembly relationships are implemented.");
            int first = Ref(relationship, "rep_1"), second = Ref(relationship, "rep_2");
            currentRepresentation = first;
            Require(parents.count(first) && parents.count(second), Code::Representation, relationship,
                    "Both related representations must be shape representations.");
            if(occurrenceRelations.count(relationship)) continue;
            Require(!document.IsA(relationship, "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION"),
                    Code::Representation, relationship,
                    "A transforming relationship needs an explicit assembly usage; no parent/child placement was guessed.");
            Require(contexts.at(first) == contexts.at(second), Code::Representation, relationship,
                    "Plain related shape representations must share one explicit coordinate/unit context.");
            int a = Root(first), b = Root(second);
            Require(a != b, Code::Representation, relationship,
                    "Self-referencing, cyclic or duplicate plain shape relationships are not supported.");
            Require(!contentRoots.count(a) || !contentRoots.count(b), Code::Representation, relationship,
                    "Related representations contain alternative geometry; representation selection is not implemented.");
            if(a > b) std::swap(a, b);
            parents.at(b) = a;
            if(contentRoots.erase(b)) contentRoots.insert(a);
        }
        for(int representation : representations) nodes.emplace(Root(representation), Node{});
        for(const auto &candidate : candidates) {
            nodes.at(Root(candidate.second)).solids.emplace_back(candidate.first, candidate.second);
        }

        ReadProductOccurrences(dependent);
        ReadMappedOccurrences();
        std::map<int, int> state;
        std::map<int, bool> hasGeometry;
        std::function<bool(int, int)> check = [&](int node, int depth) {
            Require(depth <= MAX_DEPTH && state[node] != 1, Code::Representation, node,
                    "Cyclic or excessively nested assembly/mapping graph.");
            if(state[node] == 2) return hasGeometry.at(node);
            state[node] = 1;
            auto &value = nodes.at(node);
            bool present = !value.solids.empty();
            for(const auto &arc : value.children) {
                Require(value.solids.empty() || arc.path.front() != -1, Code::Representation, node,
                        "An assembly also advertises direct aggregate solids; alternative geometry was not selected or duplicated.");
                Require(check(arc.child, depth + 1), Code::Representation, arc.path[1],
                        "An assembly/mapped component has no reachable solid model definition.");
                present = true;
            }
            state[node] = 2;
            return hasGeometry[node] = present;
        };
        for(const auto &node : nodes) check(node.first, 0);

        std::vector<SolidOccurrence> result;
        size_t work = 0;
        std::function<void(int, const RigidPlacement &, const std::vector<int> &, int)> expand;
        expand = [&](int node, const RigidPlacement &placement, const std::vector<int> &path, int depth) {
            currentRepresentation = node;
            Require(depth <= MAX_DEPTH && ++work <= MAX_OCCURRENCES, Code::Representation, node,
                    "Expanded assembly/mapping graph exceeds the supported depth/work limit.");
            Require(Finite(placement.translation) && std::isfinite(placement.rotation.w) &&
                    Finite({placement.rotation.vx, placement.rotation.vy, placement.rotation.vz}),
                    Code::Representation, node, "Non-finite composed assembly placement.");
            const auto &value = nodes.at(node);
            for(const auto &solid : value.solids) {
                Require(result.size() < MAX_OCCURRENCES, Code::Representation, node,
                        "Expanded solid occurrence count exceeds the supported limit.");
                result.push_back({solid.second, solid.first, placement, path});
            }
            for(const auto &arc : value.children) {
                std::vector<int> next = path;
                next.insert(next.end(), arc.path.begin(), arc.path.end());
                expand(arc.child, placement.Compose(arc.placement), next, depth + 1);
            }
        };
        for(const auto &node : nodes) if(!node.second.incoming) expand(node.first, {}, {}, 0);
        std::sort(result.begin(), result.end(), [](const SolidOccurrence &a, const SolidOccurrence &b) {
            if(a.path != b.path) return a.path < b.path;
            return std::make_pair(a.solid, a.representation) < std::make_pair(b.solid, b.representation);
        });
        return result;
    }

private:
    static const size_t MAX_OCCURRENCES = 4096;
    static const int MAX_DEPTH = 64;
    struct Arc { int child; RigidPlacement placement; std::vector<int> path; };
    struct Node {
        bool incoming = false;
        std::vector<std::pair<int, int>> solids;
        std::vector<Arc> children;
    };
    std::map<int, int> parents, contexts, mappedOwners;
    std::map<int, std::set<int>> frameOwners;
    std::map<int, double> scales;
    std::map<int, Node> nodes;

    int Root(int representation) {
        while(parents.at(representation) != representation) {
            parents.at(representation) = parents.at(parents.at(representation));
            representation = parents.at(representation);
        }
        return representation;
    }

    double Scale(int representation) {
        int context = contexts.at(representation);
        auto found = scales.find(context);
        if(found == scales.end()) found = scales.emplace(context, Units(document).ForContext(context)).first;
        return found->second;
    }

    bool OwnsFrame(int representation, int frame) {
        auto found = frameOwners.find(frame);
        if(found == frameOwners.end()) return false;
        int root = Root(representation);
        for(int owner : found->second) if(Root(owner) == root) return true;
        return false;
    }

    RigidPlacement ReadOperator(int id, double scale) {
        auto types = document.EntityTypes(id);
        Require(types.size() == 1 && types[0] == "CARTESIAN_TRANSFORMATION_OPERATOR_3D",
                Code::Representation, id,
                "Only a rigid 3D Cartesian transformation operator is implemented; no 2D/non-uniform transform was approximated.");
        auto factor = Get(id, "scale");
        Require(factor.kind == Kind::Null || Number(factor, id) == 1, Code::Representation, id,
                "Assembly/mapping scale must be exactly one; scaling is not silently ignored.");
        auto axis = [&](const char *name, Vector fallback) {
            auto value = Get(id, name);
            Require(value.kind == Kind::Null || value.kind == Kind::Reference,
                    Code::Representation, id, "Invalid optional Cartesian transformation axis.");
            return value.kind == Kind::Null ? fallback : Direction(value.reference);
        };
        // EXPRESS base_axis(3,...): derive Z, then project X, then project Y
        // perpendicular to both. axis2 may select a reflection, which we reject.
        Vector z = axis("axis3", {0, 0, 1});
        bool alongX = EXACT(std::abs(z.x) == 1) && EXACT(z.y == 0) && EXACT(z.z == 0);
        Vector x = axis("axis1", alongX ? Vector{0, 1, 0} : Vector{1, 0, 0});
        x = x.Minus(z.ScaledBy(x.Dot(z)));
        Require(x.Magnitude() > 1e-12, Code::Representation, id, "Undefined projected Cartesian X axis.");
        x = x.WithMagnitude(1);
        Vector y = axis("axis2", {0, 1, 0});
        y = y.Minus(z.ScaledBy(y.Dot(z))).Minus(x.ScaledBy(y.Dot(x)));
        Require(y.Magnitude() > 1e-12, Code::Representation, id, "Undefined projected Cartesian Y axis.");
        y = y.WithMagnitude(1);
        Require(std::abs(x.Dot(z)) <= 1e-12 && std::abs(y.Dot(z)) <= 1e-12 &&
                std::abs(x.Dot(y)) <= 1e-12 && x.Cross(y).Dot(z) > 1 - 1e-12,
                Code::Representation, id, "Reflected or numerically unresolved Cartesian transformation frame.");
        return {ReadPoint(*this, Ref(id, "local_origin"), scale), Quaternion::From(x, y).WithMagnitude(1)};
    }

    void AddArc(int parent, int child, RigidPlacement placement, std::vector<int> path) {
        Require(parent != child, Code::Representation, path[1], "Self-referencing assembly/mapping occurrence.");
        for(const auto &arc : nodes.at(parent).children) {
            Require(arc.child != child || arc.path.front() == path.front(), Code::Representation, path[1],
                    "The same component is advertised by both product usage and mapping; duplicate geometry was not guessed.");
        }
        nodes.at(parent).children.push_back({child, placement, std::move(path)});
        nodes.at(child).incoming = true;
    }

    void ReadProductOccurrences(const std::vector<int> &dependent) {
        std::map<int, std::set<int>> productShapes;
        for(int definition : document.EntitiesOfType("SHAPE_DEFINITION_REPRESENTATION")) {
            int shape = Ref(definition, "definition");
            if(!document.IsA(shape, "PRODUCT_DEFINITION_SHAPE")) continue;
            int product = Ref(shape, "definition");
            if(!document.IsA(product, "PRODUCT_DEFINITION")) continue;
            int representation = Ref(definition, "used_representation");
            productShapes[product].insert(parents.count(representation) ? Root(representation) : 0);
        }
        std::map<int, std::set<int>> shapeProducts;
        for(const auto &product : productShapes) for(int shape : product.second) {
            if(shape != 0) shapeProducts[shape].insert(product.first);
        }
        auto productRoot = [&](int product, int entity) {
            auto found = productShapes.find(product);
            Require(document.IsA(product, "PRODUCT_DEFINITION") && found != productShapes.end() &&
                    found->second.size() == 1 && *found->second.begin() != 0, Code::Representation, entity,
                    "Assembly product needs exactly one resolved shape representation; external/alternative definitions were not selected.");
            return *found->second.begin();
        };
        std::set<int> used;
        for(int id : dependent) {
            int relationship = Ref(id, "representation_relation");
            Require(document.IsA(relationship, "SHAPE_REPRESENTATION_RELATIONSHIP"), Code::Representation, id,
                    "Assembly occurrence needs a shape representation relationship.");
            int shape = Ref(id, "represented_product_relation");
            Require(document.IsA(shape, "PRODUCT_DEFINITION_SHAPE"), Code::Representation, id,
                    "Assembly occurrence needs a PRODUCT_DEFINITION_SHAPE usage.");
            int usage = Ref(shape, "definition");
            auto types = document.EntityTypes(usage);
            Require(types.size() == 1 && types[0] == "NEXT_ASSEMBLY_USAGE_OCCURRENCE" && used.insert(usage).second,
                    Code::Representation, id, "Only a uniquely placed NEXT_ASSEMBLY_USAGE_OCCURRENCE is implemented.");
            int parentProduct = Ref(usage, "relating_product_definition");
            int childProduct = Ref(usage, "related_product_definition");
            int parent = productRoot(parentProduct, id), child = productRoot(childProduct, id);
            Require(shapeProducts.at(parent).size() == 1, Code::Representation, usage,
                    "An assembly shape is shared by different product definitions; product structure cannot be assigned unambiguously.");
            int first = Ref(relationship, "rep_1"), second = Ref(relationship, "rep_2");
            currentRepresentation = first;
            bool forwards = Root(first) == child && Root(second) == parent;
            Require(forwards || (Root(first) == parent && Root(second) == child), Code::Representation, id,
                    "Representation relationship disagrees with the declared product parent/child; direction was not guessed.");
            double firstScale = Scale(first), secondScale = Scale(second);
            RigidPlacement placement;
            if(document.IsA(relationship, "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION")) {
                int transformation = Ref(relationship, "transformation_operator");
                if(document.IsA(transformation, "ITEM_DEFINED_TRANSFORMATION")) {
                    int fromFrame = Ref(transformation, "transform_item_1");
                    int toFrame = Ref(transformation, "transform_item_2");
                    Require(OwnsFrame(first, fromFrame) && OwnsFrame(second, toFrame),
                            Code::Representation, transformation,
                            "Transformation frames must belong to their respective resolved representations; coordinate/unit ownership was not guessed.");
                    auto from = ReadFrame(*this, fromFrame, firstScale);
                    auto to = ReadFrame(*this, toFrame, secondScale);
                    placement = RigidPlacement::Frame(to, toFrame).Compose(RigidPlacement::Frame(from, fromFrame).Inverse());
                } else placement = ReadOperator(transformation, secondScale);
            } else Require(contexts.at(first) == contexts.at(second), Code::Representation, relationship,
                           "An untransformed product usage needs one shared coordinate/unit context.");
            if(!forwards) placement = placement.Inverse();
            AddArc(parent, child, placement, {-1, usage, parentProduct, childProduct});
        }
        for(int usage : document.EntitiesOfType("ASSEMBLY_COMPONENT_USAGE")) {
            Require(used.count(usage), Code::Representation, usage,
                    "Assembly usage has no unique supported placement; no component was imported at its definition origin.");
        }
    }

    void ReadMappedOccurrences() {
        auto items = document.EntitiesOfType("MAPPED_ITEM");
        Require(items.size() <= MAX_OCCURRENCES, Code::Representation, 0, "Mapping count exceeds the supported work limit.");
        for(int item : items) {
            auto owner = mappedOwners.find(item);
            Require(owner != mappedOwners.end(), Code::Representation, item, "MAPPED_ITEM has no supported shape representation owner.");
            int parent = owner->second;
            currentRepresentation = parent;
            int map = Ref(item, "mapping_source");
            Require(document.IsA(map, "REPRESENTATION_MAP"), Code::Representation, item, "Expected REPRESENTATION_MAP.");
            int child = Ref(map, "mapped_representation");
            Require(parents.count(child), Code::Representation, map, "Mapped representation must be a resolved shape representation.");
            int originFrame = Ref(map, "mapping_origin");
            Require(OwnsFrame(child, originFrame), Code::Representation, map,
                    "Mapping origin must belong to the resolved mapped representation; coordinate/unit ownership was not guessed.");
            auto origin = ReadFrame(*this, originFrame, Scale(child));
            auto target = ReadOperator(Ref(item, "mapping_target"), Scale(parent));
            auto placement = target.Compose(RigidPlacement::Frame(origin, originFrame).Inverse());
            AddArc(Root(parent), Root(child), placement, {-2, item, map, child});
        }
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
    bool periodic[2] = {};
    int band = -1, bandLower = -1; // periodic coordinate and lower-bound loop
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

Point2d ParameterDelta(const Face &face, Point2d delta) {
    if(face.periodic[0]) {
        if(delta.x > 0.5) delta.x -= 1;
        if(delta.x < -0.5) delta.x += 1;
    }
    if(face.periodic[1]) {
        if(delta.y > 0.5) delta.y -= 1;
        if(delta.y < -0.5) delta.y += 1;
    }
    return delta;
}

int InFace(const Face &face, Point2d point) {
    if(face.band >= 0) {
        double eps = face.UvEps();
        if(point.x < -eps || point.x > 1 + eps || point.y < -eps || point.y > 1 + eps) return -1;
        double across = face.band == 0 ? point.y : point.x;
        return across <= eps || across >= 1 - eps ? 0 : 1;
    }
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

std::vector<SBezier> CircularArcs(double first, double sweep, bool periodic) {
    int count = (int)std::ceil(sweep / (PI / 2));
    double step = sweep / count, weight = std::cos(step / 2);
    auto point = [](double angle) { return Vector{std::cos(angle), std::sin(angle), 0}; };
    Vector initial = point(first);
    std::vector<SBezier> result;
    for(int i = 0; i < count; ++i) {
        Vector a = i == 0 ? initial : result.back().Finish();
        Vector b = point(first + step * (i + 0.5)).ScaledBy(1 / weight);
        Vector c = i + 1 == count && periodic ? initial : point(first + step * (i + 1));
        SBezier curve = SBezier::From(a, b, c);
        curve.weight[1] = weight;
        result.push_back(curve);
    }
    return result;
}

// Cavity ownership must not be inferred from bounding boxes, mesh volume or a
// few boundary samples. Prove exact patch separation before classifying one
// point on each connected shell. All copies/subdivisions are validation-local.
class ShellBoundary {
public:
    enum class Location { Outside, Inside, Boundary, Ambiguous };
    struct Patch {
        SSurface surface;
        Vector min, max;
        SSurface::Plane plane;
    };
    std::vector<Patch> patches;
    Vector probe = {}, min = {VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE};
    Vector max = {VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE};

    bool SeparatedFrom(const ShellBoundary &other, size_t *work) const {
        for(const auto &a : patches) for(const auto &b : other.patches) {
            if(!SeparatedPatches(a.surface, b.surface, a.min, a.max, b.min, b.max,
                                 a.plane, b.plane, 0, work)) return false;
        }
        return true;
    }

    Location Classify(Vector point, size_t *work) {
        if(point.OutsideAndNotOn(max, min)) return Location::Outside;
        static const Vector directions[] = {
            {1, 0.371, 0.529}, {0.529, 1, 0.371}, {0.371, 0.529, 1},
            {1, -0.617, 0.223}, {0.223, 1, -0.617}, {-0.617, 0.223, 1}
        };
        Location result = Location::Ambiguous;
        int agreeing = 0;
        for(Vector direction : directions) {
            Location location;
            if(!Cast(point, direction.WithMagnitude(1), &location, work)) continue;
            if(location == Location::Boundary) return location;
            if(agreeing && location != result) return Location::Ambiguous;
            result = location;
            if(++agreeing == 2) return result;
        }
        return Location::Ambiguous;
    }

private:
    static bool SeparatedPatches(const SSurface &a, const SSurface &b,
                                 Vector amin, Vector amax, Vector bmin, Vector bmax,
                                 const SSurface::Plane &planeA, const SSurface::Plane &planeB,
                                 int depth, size_t *work) {
        if(++(*work) > 1000000 || depth > 32) return false;
        Vector hi, lo;
        a.GetAxisAlignedBounding(&hi, &lo);
        amin = {std::max(amin.x, lo.x), std::max(amin.y, lo.y), std::max(amin.z, lo.z)};
        amax = {std::min(amax.x, hi.x), std::min(amax.y, hi.y), std::min(amax.z, hi.z)};
        b.GetAxisAlignedBounding(&hi, &lo);
        bmin = {std::max(bmin.x, lo.x), std::max(bmin.y, lo.y), std::max(bmin.z, lo.z)};
        bmax = {std::min(bmax.x, hi.x), std::min(bmax.y, hi.y), std::min(bmax.z, hi.z)};
        if(amax.x < amin.x - LENGTH_EPS || amax.y < amin.y - LENGTH_EPS || amax.z < amin.z - LENGTH_EPS ||
           bmax.x < bmin.x - LENGTH_EPS || bmax.y < bmin.y - LENGTH_EPS || bmax.z < bmin.z - LENGTH_EPS ||
           amax.x < bmin.x - LENGTH_EPS || bmax.x < amin.x - LENGTH_EPS ||
           amax.y < bmin.y - LENGTH_EPS || bmax.y < amin.y - LENGTH_EPS ||
           amax.z < bmin.z - LENGTH_EPS || bmax.z < amin.z - LENGTH_EPS) return true;
        auto beyondPlane = [](const SSurface &surface, Vector point, Vector normal) {
            double lo = VERY_POSITIVE, hi = VERY_NEGATIVE;
            for(int i = 0; i <= surface.degm; ++i) for(int j = 0; j <= surface.degn; ++j) {
                double d = surface.ctrl[i][j].Minus(point).Dot(normal);
                lo = std::min(lo, d); hi = std::max(hi, d);
            }
            return lo > LENGTH_EPS || hi < -LENGTH_EPS;
        };
        if((planeA.isPlanar && beyondPlane(b, a.ctrl[0][0], planeA.normal)) ||
           (planeB.isPlanar && beyondPlane(a, b.ctrl[0][0], planeB.normal))) return true;
        bool splitA = amax.Minus(amin).MagSquared() >= bmax.Minus(bmin).MagSquared();
        SSurface split = splitA ? a : b;
        double du = 0, dv = 0;
        for(int i = 0; i <= split.degm; ++i) for(int j = 0; j <= split.degn; ++j) {
            if(i) du = std::max(du, split.ctrl[i][j].Minus(split.ctrl[i - 1][j]).MagSquared());
            if(j) dv = std::max(dv, split.ctrl[i][j].Minus(split.ctrl[i][j - 1]).MagSquared());
        }
        SSurface first = {}, second = {};
        split.SplitInHalf(du >= dv, &first, &second);
        if(splitA) return
            SeparatedPatches(first, b, amin, amax, bmin, bmax, planeA, planeB, depth + 1, work) &&
            SeparatedPatches(second, b, amin, amax, bmin, bmax, planeA, planeB, depth + 1, work);
        return SeparatedPatches(a, first, amin, amax, bmin, bmax, planeA, planeB, depth + 1, work) &&
               SeparatedPatches(a, second, amin, amax, bmin, bmax, planeA, planeB, depth + 1, work);
    }

    bool Cast(Vector point, Vector direction, Location *location, size_t *work) {
        double reach = point.Magnitude() + 2 * max.Minus(min).Magnitude();
        struct Crossing { double distance, normal; };
        std::vector<Crossing> crossings;
        for(auto &patch : patches) {
            if(++(*work) > 1000000) return false;
            auto &surface = patch.surface;
            if(patch.plane.isPlanar && std::abs(patch.plane.normal.Dot(direction)) < 1e-8) return false;
            List<SInter> intersections = {};
            surface.AllPointsIntersecting(point.Minus(direction.ScaledBy(reach)),
                point.Plus(direction.ScaledBy(reach)), &intersections,
                /*asSegment=*/true, /*trimmed=*/false, /*inclTangent=*/true);
            bool reliable = true;
            for(const auto &hit : intersections) {
                if(!Finite(hit.p) || !std::isfinite(hit.pinter.x) || !std::isfinite(hit.pinter.y)) {
                    reliable = false; break;
                }
                auto c = surface.bsp->ClassifyPoint(hit.pinter, {0, 0}, &surface);
                double distance = hit.p.Minus(point).Dot(direction);
                if(c != SBspUv::Class::OUTSIDE && std::abs(distance) <= LENGTH_EPS) {
                    intersections.Clear();
                    *location = Location::Boundary;
                    return true;
                }
                // Trim PWLs only discard hits well away from their uncertainty
                // band. Retry rays near a trim, artificial seam or tangency.
                if(surface.bsp->MinimumDistanceToEdge(hit.pinter, &surface) <=
                   4 * std::max(4 * LENGTH_EPS, SS.ChordTolMm())) {
                    reliable = false; break;
                }
                if(c == SBspUv::Class::OUTSIDE) continue;
                double normal = hit.surfNormal.Dot(direction) / hit.surfNormal.Magnitude();
                if(!std::isfinite(normal) || std::abs(normal) < 1e-6) { reliable = false; break; }
                crossings.push_back({distance, normal});
            }
            intersections.Clear();
            if(!reliable) return false;
        }
        std::sort(crossings.begin(), crossings.end(),
                  [](const Crossing &a, const Crossing &b) { return a.distance < b.distance; });
        if(crossings.size() % 2 != 0) return false;
        int before = 0;
        for(size_t i = 0; i < crossings.size(); ++i) {
            if(i && crossings[i].distance - crossings[i - 1].distance <= LENGTH_EPS) return false;
            // Each base shell was validated outward: crossings must alternate
            // entering/exiting, with both ray endpoints outside its control hull.
            if((crossings[i].normal < 0) != (i % 2 == 0)) return false;
            if(crossings[i].distance < 0) ++before;
        }
        *location = before % 2 ? Location::Inside : Location::Outside;
        return true;
    }
};

class SolidBuilder : public Attributes {
public:
    SolidBuilder(const Document &document, const GeometryOptions &options)
        : Attributes(document), options(options) {}

    std::unique_ptr<NativeSolid> Build(int representation, int solid) {
        context = Ref(representation, "context_of_items");
        scale = Units(document).ForContext(context);
        Require(document.IsA(solid, "MANIFOLD_SOLID_BREP"), Code::Unsupported, solid,
                "Only MANIFOLD_SOLID_BREP is supported.");
        int outer = Ref(solid, "outer");
        ReadShell(outer, 0);
        if(document.IsA(solid, "BREP_WITH_VOIDS")) {
            auto voids = Refs(solid, "voids");
            Require(!voids.empty() && voids.size() < MAX_FACES, Code::Unsupported, solid,
                    "BREP_WITH_VOIDS needs a nonempty, bounded set of cavity shells.");
            std::sort(voids.begin(), voids.end());
            Require(std::adjacent_find(voids.begin(), voids.end()) == voids.end(),
                    Code::InvalidGeometry, solid, "Repeated oriented cavity shell in BREP_WITH_VOIDS.");
            for(int oriented : voids) {
                auto types = document.EntityTypes(oriented);
                Require(types.size() == 1 && types[0] == "ORIENTED_CLOSED_SHELL",
                        Code::Unsupported, oriented, "A cavity must reference ORIENTED_CLOSED_SHELL.");
                Require(!Boolean(oriented, "orientation"), Code::InvalidGeometry, oriented,
                        "A cavity must reverse its outward CLOSED_SHELL (orientation .F.); no orientation was repaired.");
                ReadShell(Ref(oriented, "closed_shell_element"), oriented);
            }
        }
        faces.reserve(faceShell.size());
        for(const auto &entry : faceShell) faces.push_back(ReadFace(entry.first));
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
        ValidateTopology();
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
        ValidateVoids(result.get());
        // Source base shells have just been validated outward. Apply only the
        // explicit oriented-shell reversal, to both geometry and provenance.
        for(auto &face : result->faces) if(faceShell.at(face.face) != 0) {
            face.normal = face.normal.Negated();
            for(auto &loop : face.boundaries) for(auto &use : loop) use.second = !use.second;
            for(hSSurface surface : face.surfaces) result->shell.surface.FindById(surface)->Reverse();
        }
        return result;
    }

private:
    const GeometryOptions &options;
    int context = 0;
    double scale = 0;
    std::map<int, Vertex> vertices;
    std::map<int, Edge> edges;
    std::vector<Face> faces;
    std::vector<Segment> segments;
    std::vector<Seam> seams;
    struct SourceShell {
        int id, oriented; // oriented is zero for the outer shell
        std::vector<int> faces;
    };
    std::vector<SourceShell> shells;
    std::map<int, size_t> faceShell;

    void ReadShell(int id, int oriented) {
        auto types = document.EntityTypes(id);
        Require(types.size() == 1 && types[0] == "CLOSED_SHELL", Code::Unsupported, id,
                "Only a direct CLOSED_SHELL is supported as a base shell; nested oriented shells are not allowed.");
        for(const auto &shell : shells) Require(shell.id != id, Code::InvalidGeometry,
                oriented ? oriented : id, "Outer and cavity shells must not reuse one CLOSED_SHELL.");
        auto ids = Refs(id, "cfs_faces");
        Require(!ids.empty() && ids.size() + faceShell.size() <= MAX_FACES, Code::Unsupported,
                id, "Closed-shell face count is empty or exceeds the converter limit for the complete solid.");
        std::sort(ids.begin(), ids.end());
        for(int face : ids) Require(faceShell.emplace(face, shells.size()).second,
                Code::InvalidGeometry, id, "Repeated face in or between CLOSED_SHELLs.");
        shells.push_back({id, oriented, std::move(ids)});
    }

    Vector Point(int id) {
        return ReadPoint(*this, id, scale);
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

    Placement ReadPlacement(int geometry) {
        return ReadFrame(*this, Ref(geometry, "position"), scale);
    }

    std::vector<Nurbs::CurveSpan> ReadSplineCurve(int geometry) {
        Require(document.IsA(geometry, "B_SPLINE_CURVE"), Code::Unsupported, geometry,
                "Curve has no implemented exact native conversion (expected LINE, CIRCLE or a rational/non-rational B-spline).");
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

    std::vector<Piece> ReadCircleCurve(const Edge &edge, Vector start, Vector finish, bool sameSense) {
        Placement placement = ReadPlacement(edge.geometry);
        double radius = Number(Get(edge.geometry, "radius"), edge.geometry);
        Require(radius > 0, Code::InvalidGeometry, edge.geometry, "CIRCLE radius must be positive.");
        radius *= scale;
        Require(std::isfinite(radius) && radius > LENGTH_EPS && radius <= MAX_COORDINATE_MM,
                Code::Unsupported, edge.geometry, "CIRCLE radius is outside the native numerical envelope.");
        auto radial = [&](double angle) {
            return placement.x.ScaledBy(std::cos(angle)).Plus(placement.y.ScaledBy(std::sin(angle)));
        };
        auto parameter = [&](Vector point) {
            Vector delta = point.Minus(placement.origin);
            double angle = std::atan2(delta.Dot(placement.y), delta.Dot(placement.x));
            Require(placement.origin.Plus(radial(angle).ScaledBy(radius)).Equals(point),
                    Code::InvalidGeometry, edge.id, "Edge vertices do not lie on their CIRCLE geometry.");
            return angle;
        };
        double first = parameter(sameSense ? start : finish);
        double last = parameter(sameSense ? finish : start);
        Vector a = placement.origin.Plus(radial(first).ScaledBy(radius));
        Vector b = placement.origin.Plus(radial(last).ScaledBy(radius));
        bool closed = edge.start == edge.finish;
        Require(closed || !a.Equals(b), Code::InvalidGeometry, edge.id,
                "Distinct CIRCLE edge endpoints coincide within native tolerance.");
        double sweep = closed ? 2 * PI : last - first;
        if(sweep <= 0) sweep += 2 * PI;
        // The source vertex selects a full circle's seam. same_sense selects
        // the minor/major arc and traversal, including across the angular seam.
        // Each <=90-degree span is an exact, positive-weight rational quadratic.
        int count = (int)std::ceil(sweep / (PI / 2));
        double step = sweep / count, weight = std::cos(step / 2);
        std::vector<Piece> pieces;
        for(int i = 0; i < count; ++i) {
            Vector p0 = i == 0 ? a : pieces.back().bezier.Finish();
            Vector p1 = placement.origin.Plus(radial(first + step * (i + 0.5)).ScaledBy(radius / weight));
            Vector p2 = i + 1 == count ? (closed ? a : b) :
                        placement.origin.Plus(radial(first + step * (i + 1)).ScaledBy(radius));
            SBezier bezier = SBezier::From(p0, p1, p2);
            bezier.weight[1] = weight;
            for(int k = 0; k <= bezier.deg; ++k) {
                Vector point = bezier.ctrl[k];
                Require(Finite(point) && std::abs(point.x) <= MAX_COORDINATE_MM &&
                        std::abs(point.y) <= MAX_COORDINATE_MM && std::abs(point.z) <= MAX_COORDINATE_MM,
                        Code::Unsupported, edge.geometry, "CIRCLE controls exceed the native coordinate envelope.");
            }
            pieces.push_back({bezier, {}});
        }
        if(!sameSense) {
            std::reverse(pieces.begin(), pieces.end());
            for(auto &piece : pieces) piece.bezier.Reverse();
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
        bool circle = document.IsA(edge.geometry, "CIRCLE");
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
        } else if(circle) {
            edge.pieces = ReadCircleCurve(edge, a, b, sameSense);
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
        edge.linear = !circle && !a.Equals(b);
        if(edge.linear) {
            double eps = Roundoff(controls);
            for(const auto &piece : edge.pieces) {
                if(!Nurbs::IsMonotoneLine(piece.bezier, a, b, eps)) edge.linear = false;
            }
        }
        return edges.emplace(id, std::move(edge)).first->second;
    }

    void ReadPlane(Face *face) {
        Placement placement = ReadPlacement(face->geometry);
        face->origin = placement.origin;
        Vector x = placement.x;
        face->normal = Boolean(face->id, "same_sense") ? placement.z : placement.z.Negated();
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
                "Surface has no implemented exact native conversion (expected a supported plane, cone, cylinder, sphere, torus or tensor-product B-spline).");
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
        FinishSurface(face, std::move(source));
    }

    void FinishSurface(Face *face, std::vector<Nurbs::SurfacePatch> source) {
        bool reverse = !Boolean(face->id, "same_sense");
        if(reverse) {
            std::swap(face->periodic[0], face->periodic[1]);
            if(face->band >= 0) face->band = 1 - face->band;
        }
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

    bool AxialBand(Face *face, const Placement &placement,
                   const std::function<double(double)> &radiusAt,
                   double *bottom, double *top) {
        if(face->loops.size() != 2) return false;
        double height[2];
        int direction[2];
        for(int i = 0; i < 2; ++i) {
            const auto &loop = face->loops[i];
            if(loop.edges.size() != 1) return false;
            const auto &use = loop.edges[0];
            const auto &edge = edges.at(use.edge);
            if(edge.start != edge.finish || !document.IsA(edge.geometry, "CIRCLE")) return false;
            Placement circle = ReadPlacement(edge.geometry);
            Vector delta = circle.origin.Minus(placement.origin);
            height[i] = delta.Dot(placement.z);
            double radius = radiusAt(height[i]);
            double circleRadius = Number(Get(edge.geometry, "radius"), edge.geometry) * scale;
            Require(std::isfinite(radius) && radius > LENGTH_EPS &&
                    delta.Minus(placement.z.ScaledBy(height[i])).Magnitude() +
                     std::abs(circleRadius - radius) + radius * circle.z.Cross(placement.z).Magnitude() <= LENGTH_EPS,
                     Code::InvalidGeometry, face->geometry,
                     "Circular axial-band boundary is not on its declared surface.");
            direction[i] = (circle.z.Dot(placement.z) > 0 ? 1 : -1) *
                           (Boolean(edge.id, "same_sense") ? 1 : -1) * (use.forwards ? 1 : -1);
        }
        int lower = height[0] < height[1] ? 0 : 1;
        Require(height[1 - lower] - height[lower] > LENGTH_EPS, Code::InvalidGeometry,
                face->id, "Touching or coincident axial-band boundaries.");
        int sense = Boolean(face->id, "same_sense") ? 1 : -1;
        Require(direction[lower] == sense && direction[1 - lower] == -sense,
                Code::InvalidGeometry, face->id, "Axial-band winding disagrees with the oriented surface.");
        int declaredOuter = 0;
        for(const auto &loop : face->loops) if(loop.declaredOuter) ++declaredOuter;
        Require(declaredOuter <= 1, Code::InvalidGeometry, face->id, "More than one declared outer axial-band bound.");
        *bottom = height[lower]; *top = height[1 - lower];
        face->band = 0; face->bandLower = lower;
        return true;
    }

    void AxialBounds(const Face &face, const Placement &placement, double *bottom, double *top) {
        *bottom = VERY_POSITIVE; *top = VERY_NEGATIVE;
        for(const auto &loop : face.loops) for(const auto &use : loop.edges) {
            for(const auto &piece : edges.at(use.edge).pieces) for(int i = 0; i <= piece.bezier.deg; ++i) {
                double height = piece.bezier.ctrl[i].Minus(placement.origin).Dot(placement.z);
                *bottom = std::min(*bottom, height); *top = std::max(*top, height);
            }
        }
    }

    Vector ChartPoint(const Face &face) {
        // A coordinate cut, not a choice of material or a replacement trim.
        for(const auto &loop : face.loops) for(const auto &use : loop.edges) {
            const auto &edge = edges.at(use.edge);
            if(edge.start == edge.finish || !document.IsA(edge.geometry, "CIRCLE") || edge.pieces.size() < 2) continue;
            size_t count = edge.pieces.size();
            const auto &middle = edge.pieces[count / 2].bezier;
            return count % 2 == 0 ? middle.Start() : middle.PointAt(0.5);
        }
        const auto &use = face.loops[0].edges[0];
        const auto &pieces = edges.at(use.edge).pieces;
        return use.forwards ? pieces.front().bezier.Start() : pieces.back().bezier.Finish();
    }

    std::vector<Nurbs::SurfacePatch> Revolve(const Placement &placement, double first,
                                            const std::vector<SBezier> &profile,
                                            double sweep = 2 * PI, bool periodic = true) {
        auto arcs = CircularArcs(first, sweep, periodic);
        std::vector<Nurbs::SurfacePatch> source;
        for(size_t iu = 0; iu < arcs.size(); ++iu) for(size_t iv = 0; iv < profile.size(); ++iv) {
            SSurface surface = {};
            surface.degm = arcs[iu].deg; surface.degn = profile[iv].deg;
            for(int i = 0; i <= surface.degm; ++i) for(int j = 0; j <= surface.degn; ++j) {
                Vector radial = placement.x.ScaledBy(arcs[iu].ctrl[i].x).Plus(placement.y.ScaledBy(arcs[iu].ctrl[i].y));
                Vector point = placement.origin.Plus(radial.ScaledBy(profile[iv].ctrl[j].x))
                                               .Plus(placement.z.ScaledBy(profile[iv].ctrl[j].y));
                Require(Finite(point) && std::abs(point.x) <= MAX_COORDINATE_MM &&
                        std::abs(point.y) <= MAX_COORDINATE_MM && std::abs(point.z) <= MAX_COORDINATE_MM,
                        Code::Unsupported, 0, "Surface-of-revolution controls exceed the native coordinate envelope.");
                surface.ctrl[i][j] = point;
                surface.weight[i][j] = arcs[iu].weight[i] * profile[iv].weight[j];
            }
            source.push_back({surface, iu / (double)arcs.size(), (iu + 1) / (double)arcs.size(),
                              iv / (double)profile.size(), (iv + 1) / (double)profile.size(), (int)iu, (int)iv});
        }
        return source;
    }

    void ReadCylinderOrCone(Face *face, bool cone) {
        int geometry = face->geometry;
        Placement placement = ReadPlacement(geometry);
        double radius = Number(Get(geometry, "radius"), geometry);
        Require(cone ? radius >= 0 : radius > 0, Code::InvalidGeometry, geometry,
                "Cylinder radius must be positive; a cone reference radius must be non-negative.");
        radius *= scale;
        Require(std::isfinite(radius) && (cone || radius > LENGTH_EPS) && radius <= MAX_COORDINATE_MM,
                Code::Unsupported, geometry, "Cone/cylinder radius is outside the native numerical envelope.");
        double slope = 0;
        if(cone) {
            double angle = Number(Get(geometry, "semi_angle"), geometry) * Units(document).AnglesForContext(context);
            Require(std::isfinite(angle) && std::abs(angle) > 0 && std::abs(angle) < PI / 2,
                    Code::InvalidGeometry, geometry, "Cone semi-angle must be nonzero and strictly between -pi/2 and pi/2.");
            slope = std::tan(angle);
        }
        auto radiusAt = [&](double height) { return radius + slope * height; };
        double bottom = VERY_POSITIVE, top = VERY_NEGATIVE, first = 0;
        if(!AxialBand(face, placement, radiusAt, &bottom, &top)) {
            // Positive rational weights bound the axial extent by the source
            // curve controls. This only bounds the support, never replaces a
            // trim; ordinary face validation still selects the finite region.
            AxialBounds(*face, placement, &bottom, &top);
            Require(top - bottom > LENGTH_EPS, Code::InvalidGeometry, geometry,
                    "Cone/cylinder face has no non-degenerate finite axial extent.");
            double pad = std::max((top - bottom) * 0.05, 8 * LENGTH_EPS);
            if(cone) {
                double clearance = std::min(radiusAt(bottom), radiusAt(top));
                Require(clearance > LENGTH_EPS, Code::Unsupported, geometry,
                        "Cone trimming reaches or crosses its apex; singular native trims are not implemented.");
                pad = std::min(pad, (clearance - LENGTH_EPS) / (2 * std::abs(slope)));
            }
            bottom -= pad; top += pad;
            Vector p = ChartPoint(*face).Minus(placement.origin);
            first = std::atan2(p.Dot(placement.y), p.Dot(placement.x)) - PI;
        }
        Require(std::min(radiusAt(bottom), radiusAt(top)) > LENGTH_EPS,
                Code::Unsupported, geometry, "Cone/cylinder support reaches a singular or sub-tolerance radius.");
        face->periodic[0] = true;
        SBezier profile = SBezier::From(Vector{radiusAt(bottom), bottom, 0}, Vector{radiusAt(top), top, 0});
        FinishSurface(face, Revolve(placement, first, {profile}));
    }

    void ReadSphere(Face *face) {
        int geometry = face->geometry;
        Placement placement = ReadPlacement(geometry);
        double radius = Number(Get(geometry, "radius"), geometry);
        Require(radius > 0, Code::InvalidGeometry, geometry, "Sphere radius must be positive.");
        radius *= scale;
        Require(std::isfinite(radius) && radius > LENGTH_EPS && radius <= MAX_COORDINATE_MM,
                Code::Unsupported, geometry, "Sphere radius is outside the native numerical envelope.");
        auto radiusAt = [&](double height) {
            return std::abs(height) < radius ? std::sqrt((radius - height) * (radius + height)) : 0;
        };
        // A sphere has no preferred polar axis. Coaxial circular bounds define
        // a regular band even when their axis differs from the source placement.
        bool circles = face->loops.size() == 2;
        if(circles) for(const auto &loop : face->loops) {
            if(loop.edges.size() != 1) { circles = false; break; }
            const auto &edge = edges.at(loop.edges[0].edge);
            if(edge.start != edge.finish || !document.IsA(edge.geometry, "CIRCLE")) { circles = false; break; }
        }
        if(circles) {
            Vector axis = ReadPlacement(edges.at(face->loops[0].edges[0].edge).geometry).z;
            if(axis.Dot(placement.z) < 0) axis = axis.Negated();
            Vector ref = std::abs(axis.Dot(placement.x)) < 0.9 ? placement.x : placement.y;
            placement.z = axis;
            placement.x = ref.Minus(axis.ScaledBy(ref.Dot(axis))).WithMagnitude(1);
            placement.y = axis.Cross(placement.x);
            for(const auto &loop : face->loops) {
                Placement circle = ReadPlacement(edges.at(loop.edges[0].edge).geometry);
                Vector delta = circle.origin.Minus(placement.origin);
                if(circle.z.Cross(axis).Magnitude() > 1e-12 ||
                   delta.Minus(axis.ScaledBy(delta.Dot(axis))).Magnitude() > LENGTH_EPS) circles = false;
            }
        }
        double bottom, top, first = 0;
        if(!circles || !AxialBand(face, placement, radiusAt, &bottom, &top)) {
            // Choose only a coordinate chart. Source winding still determines
            // material; loops crossing its cut or enclosing a pole are rejected.
            Vector middle = {};
            for(const auto &loop : face->loops) for(const auto &use : loop.edges) {
                for(const auto &piece : edges.at(use.edge).pieces) {
                    for(double t : {0.0, 0.25, 0.5, 0.75}) middle = middle.Plus(piece.bezier.PointAt(t).Minus(placement.origin));
                }
            }
            if(middle.Magnitude() <= LENGTH_EPS) middle = ChartPoint(*face).Minus(placement.origin);
            Require(middle.Magnitude() > LENGTH_EPS, Code::InvalidGeometry, geometry,
                    "Spherical boundary does not define a finite chart direction.");
            Vector x = middle.WithMagnitude(1);
            Vector ref = std::abs(x.Dot(placement.z)) < 0.9 ? placement.z : placement.x;
            placement.z = x.Cross(ref).WithMagnitude(1);
            placement.x = x; placement.y = placement.z.Cross(x);
            first = -PI;
            AxialBounds(*face, placement, &bottom, &top);
            double clearance = radius - std::max(std::abs(bottom), std::abs(top));
            Require(clearance > 8 * LENGTH_EPS, Code::Unsupported, geometry,
                    "Sphere trimming cannot be bounded in a regular chart away from its poles.");
            double pad = std::min(std::max((top - bottom) * 0.05, 8 * LENGTH_EPS), clearance * 0.5);
            bottom -= pad; top += pad;
        }
        double a = std::atan2(bottom, radiusAt(bottom)), b = std::atan2(top, radiusAt(top));
        Require(b > a && radiusAt(bottom) > LENGTH_EPS && radiusAt(top) > LENGTH_EPS,
                Code::Unsupported, geometry, "Spherical support reaches a pole or has no finite regular span.");
        face->periodic[0] = true;
        auto profile = CircularArcs(a, b - a, false);
        for(auto &arc : profile) for(int j = 0; j <= arc.deg; ++j) arc.ctrl[j] = arc.ctrl[j].ScaledBy(radius);
        FinishSurface(face, Revolve(placement, first, profile));
    }

    bool TorusBand(Face *face, const Placement &placement, double major, double minor,
                   double *first, double *sweep, bool inner = false) {
        if(face->loops.size() != 2) return false;
        struct Bound { int periodic, direction; double angle; } bounds[2];
        for(int i = 0; i < 2; ++i) {
            const auto &loop = face->loops[i];
            if(loop.edges.size() != 1) return false;
            const auto &use = loop.edges[0];
            const auto &edge = edges.at(use.edge);
            if(edge.start != edge.finish || !document.IsA(edge.geometry, "CIRCLE")) return false;
            Placement circle = ReadPlacement(edge.geometry);
            double radius = Number(Get(edge.geometry, "radius"), edge.geometry) * scale;
            Vector delta = circle.origin.Minus(placement.origin), center, axis;
            double height = delta.Dot(placement.z), expectedRadius;
            if(circle.z.Cross(placement.z).Magnitude() <= 1e-12) {
                if(delta.Minus(placement.z.ScaledBy(height)).Magnitude() > LENGTH_EPS) return false;
                bounds[i].periodic = 0;
                bounds[i].angle = std::atan2(height, (inner ? -radius : radius) - major);
                center = placement.origin.Plus(placement.z.ScaledBy(minor * std::sin(bounds[i].angle)));
                expectedRadius = major + minor * std::cos(bounds[i].angle);
                axis = placement.z;
            } else {
                if(major < minor) return false; // selected spindle branches are not periodic meridian bands
                Vector radial = delta.Minus(placement.z.ScaledBy(height));
                if(radial.Magnitude() <= LENGTH_EPS) return false;
                radial = radial.WithMagnitude(1);
                axis = radial.Cross(placement.z);
                if(circle.z.Cross(axis).Magnitude() > 1e-12) return false;
                bounds[i].periodic = 1;
                bounds[i].angle = std::atan2(radial.Dot(placement.y), radial.Dot(placement.x));
                center = placement.origin.Plus(radial.ScaledBy(major));
                expectedRadius = minor;
            }
            Require(circle.origin.Minus(center).Magnitude() + std::abs(radius - std::abs(expectedRadius)) +
                    radius * circle.z.Cross(axis).Magnitude() <= LENGTH_EPS,
                    Code::InvalidGeometry, face->geometry,
                    "Circular torus-band boundary is not on its TOROIDAL_SURFACE.");
            bounds[i].direction = (circle.z.Dot(axis) > 0 ? 1 : -1) *
                                  (Boolean(edge.id, "same_sense") ? 1 : -1) * (use.forwards ? 1 : -1);
        }
        if(bounds[0].periodic != bounds[1].periodic) return false;
        Require(bounds[0].direction == -bounds[1].direction, Code::InvalidGeometry, face->id,
                "Circular torus-band bounds must have opposite winding.");
        int periodic = bounds[0].periodic;
        int direction = (periodic == 0 ? 1 : -1) * (Boolean(face->id, "same_sense") ? 1 : -1);
        int lower = bounds[0].direction == direction ? 0 : 1;
        double width = bounds[1 - lower].angle - bounds[lower].angle;
        if(width <= 0) width += 2 * PI;
        Require(2 * (periodic == 0 ? minor : major - minor) * std::abs(std::sin(width / 2)) > LENGTH_EPS,
                Code::InvalidGeometry, face->id, "Touching or coincident torus-band boundaries.");
        int declaredOuter = 0;
        for(const auto &loop : face->loops) if(loop.declaredOuter) ++declaredOuter;
        Require(declaredOuter <= 1, Code::InvalidGeometry, face->id, "More than one declared outer torus-band bound.");
        first[1 - periodic] = bounds[lower].angle;
        sweep[1 - periodic] = width;
        face->periodic[1 - periodic] = false;
        face->band = periodic;
        face->bandLower = lower;
        return true;
    }

    void ReadTorus(Face *face) {
        int geometry = face->geometry;
        bool degenerate = document.IsA(geometry, "DEGENERATE_TOROIDAL_SURFACE");
        bool inner = degenerate && !Boolean(geometry, "select_outer");
        Placement placement = ReadPlacement(geometry);
        double major = Number(Get(geometry, "major_radius"), geometry);
        double minor = Number(Get(geometry, "minor_radius"), geometry);
        Require(major > 0 && minor > 0, Code::InvalidGeometry, geometry, "Torus radii must be positive.");
        major *= scale; minor *= scale;
        Require(std::isfinite(major) && std::isfinite(minor) && minor > LENGTH_EPS &&
                major <= MAX_COORDINATE_MM && minor <= MAX_COORDINATE_MM,
                Code::Unsupported, geometry, "Torus radii are outside the native numerical envelope.");
        Require(degenerate ? major < minor : major > minor,
                degenerate ? Code::InvalidGeometry : Code::Unsupported, geometry,
                "A ring torus requires major_radius > minor_radius; a selected degenerate torus requires major_radius < minor_radius.");
        face->periodic[0] = face->periodic[1] = true;
        double first[2] = {}, sweep[2] = {2 * PI, 2 * PI};
        bool band = TorusBand(face, placement, major, minor, first, sweep, inner);
        if(degenerate) {
            Require(band && face->band == 0, Code::Unsupported, geometry,
                    "A selected degenerate torus currently needs two coaxial circular bounds away from its singular tips.");
            double phi = std::acos(-major / minor);
            double lower = inner ? phi : -phi, upper = inner ? 2 * PI - phi : phi;
            while(first[1] < lower) first[1] += 2 * PI;
            Require(first[1] > lower && first[1] + sweep[1] < upper,
                    Code::InvalidGeometry, face->id,
                    "Degenerate torus winding selects the wrong branch or crosses a singular tip.");
        }
        if(!band) {
            // This only chooses a coordinate chart, never a trimming branch.
            // Ordinary loop validation still requires a continuous finite UV
            // region; a cut crossing or ambiguous periodic trim is rejected.
            const auto &use = face->loops[0].edges[0];
            const auto &pieces = edges.at(use.edge).pieces;
            Vector point = use.forwards ? pieces.front().bezier.Start() : pieces.back().bezier.Finish();
            Vector p = point.Minus(placement.origin);
            double x = p.Dot(placement.x), y = p.Dot(placement.y);
            first[0] = std::atan2(y, x) - PI;
            first[1] = std::atan2(p.Dot(placement.z), std::hypot(x, y) - major) - PI;
        }
        auto profile = CircularArcs(first[1], sweep[1], face->periodic[1]);
        for(auto &arc : profile) for(int j = 0; j <= arc.deg; ++j) {
            arc.ctrl[j] = {major + minor * arc.ctrl[j].x, minor * arc.ctrl[j].y, 0};
        }
        FinishSurface(face, Revolve(placement, first[0], profile, sweep[0], face->periodic[0]));
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
        else if(document.IsA(face.geometry, "CYLINDRICAL_SURFACE")) ReadCylinderOrCone(&face, false);
        else if(document.IsA(face.geometry, "CONICAL_SURFACE")) ReadCylinderOrCone(&face, true);
        else if(document.IsA(face.geometry, "SPHERICAL_SURFACE")) ReadSphere(&face);
        else if(document.IsA(face.geometry, "TOROIDAL_SURFACE")) ReadTorus(&face);
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
        bool analytic = document.IsA(face->geometry, "TOROIDAL_SURFACE") ||
                        document.IsA(face->geometry, "CYLINDRICAL_SURFACE") ||
                        document.IsA(face->geometry, "CONICAL_SURFACE") ||
                        document.IsA(face->geometry, "SPHERICAL_SURFACE");
        for(auto &patch : face->patches) {
            auto &source = patch.source;
            Vector max, min;
            source.surface.GetAxisAlignedBounding(&max, &min);
            if(point.x < min.x - LENGTH_EPS || point.x > max.x + LENGTH_EPS ||
               point.y < min.y - LENGTH_EPS || point.y > max.y + LENGTH_EPS ||
               point.z < min.z - LENGTH_EPS || point.z > max.z + LENGTH_EPS) continue;
            Point2d uv;
            source.surface.ClosestPointTo(point, &uv);
            // A narrow analytic patch needs accurate UVs in its short
            // direction, beyond the closest-point routine's XYZ stop test.
            // Refine on the unchanged exact patch; membership checks follow.
            if(analytic) source.surface.ClosestPointNewton(point, &uv.x, &uv.y, /*mustConverge=*/false);
            if(!std::isfinite(uv.x) || !std::isfinite(uv.y) ||
               uv.x < -face->UvEps() || uv.x > 1 + face->UvEps() ||
               uv.y < -face->UvEps() || uv.y > 1 + face->UvEps() ||
               !source.surface.PointAt(uv).Equals(point)) continue;
            uv.x = std::max(0.0, std::min(1.0, uv.x));
            uv.y = std::max(0.0, std::min(1.0, uv.y));
            Point2d global = {source.u0 + (source.u1 - source.u0) * uv.x,
                              source.v0 + (source.v1 - source.v0) * uv.y};
            if(face->periodic[0] && std::abs(global.x - 1) <= face->UvEps() * 4) global.x = 0;
            if(face->periodic[1] && std::abs(global.y - 1) <= face->UvEps() * 4) global.y = 0;
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

    void ValidatePeriodicBand(Face *face) {
        // Noncontractible circle bounds are not individually closed UV loops.
        // The surface reader proved they are distinct iso-circles and used
        // their source winding to select the strip. Check that native boundaries
        // project to its two sides and wind exactly once around the period.
        for(size_t l = 0; l < face->loops.size(); ++l) {
            double side = (int)l == face->bandLower ? 0 : 1;
            double direction = (face->band == 0 ? 1 : -1) * (side == 0 ? 1 : -1);
            Point2d initial = {}, previous = {};
            bool first = true;
            double winding = 0;
            for(const auto &use : face->loops[l].edges) {
                const auto &pieces = edges.at(use.edge).pieces;
                for(size_t k = 0; k < pieces.size(); ++k) {
                    SBezier curve = pieces[use.forwards ? k : pieces.size() - 1 - k].bezier;
                    if(!use.forwards) curve.Reverse();
                    for(int i = 0; i <= 16; ++i) {
                        Point2d uv = Project(face, curve.PointAt(i / 16.0));
                        double across = face->band == 0 ? uv.y : uv.x;
                        Require(std::abs(across - side) <= face->UvEps() * 4, Code::InvalidGeometry,
                                face->geometry, "Periodic-band boundary does not follow its native patch boundary.");
                        if(first) { initial = previous = uv; first = false; }
                        Point2d delta = ParameterDelta(*face, uv.Minus(previous));
                        double along = face->band == 0 ? delta.x : delta.y;
                        Require(std::abs(along) < 0.5 && direction * along >= -face->UvEps() * 4,
                                Code::Unsupported, face->id, "Ambiguous or reversing periodic-band parameter path.");
                        winding += along;
                        previous = uv;
                    }
                }
            }
            Require(previous.Equals(initial, face->UvEps() * 4) &&
                    std::abs(winding - direction) <= face->UvEps() * 8, Code::InvalidGeometry,
                    face->id, "Periodic-band boundary does not close with one oriented periodic winding.");
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
        if(face->band >= 0) {
            ValidatePeriodicBand(face);
            return;
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

    void ValidateTopology() {
        std::map<int, std::set<int>> neighbors;
        for(const auto &entry : edges) {
            const auto &edge = entry.second;
            Require(edge.uses.size() == 2 && edge.uses[0].face != edge.uses[1].face &&
                    edge.uses[0].forwards != edge.uses[1].forwards, Code::InvalidGeometry, edge.id,
                    "Every source edge must have exactly two distinct, oppositely oriented incident faces.");
            Require(faceShell.at(edge.uses[0].face) == faceShell.at(edge.uses[1].face),
                    Code::InvalidGeometry, edge.id, "An edge must not be shared between closed shells.");
            neighbors[edge.uses[0].face].insert(edge.uses[1].face);
            neighbors[edge.uses[1].face].insert(edge.uses[0].face);
        }
        for(size_t s = 0; s < shells.size(); ++s) {
            const auto &shell = shells[s];
            std::set<int> seen, shellEdges, shellVertices;
            std::vector<int> pending{shell.faces.front()};
            while(!pending.empty()) {
                int face = pending.back(); pending.pop_back();
                if(!seen.insert(face).second) continue;
                for(int next : neighbors[face]) pending.push_back(next);
            }
            Require(seen.size() == shell.faces.size(), Code::InvalidGeometry, shell.id,
                    "Disconnected face components in CLOSED_SHELL.");
            int euler = 0;
            for(const auto &face : faces) if(faceShell.at(face.id) == s) {
                euler += 2 - (int)face.loops.size();
                for(const auto &loop : face.loops) {
                    shellVertices.insert(loop.vertices.begin(), loop.vertices.end());
                    for(const auto &use : loop.edges) shellEdges.insert(use.edge);
                }
            }
            euler += (int)shellVertices.size() - (int)shellEdges.size();
            Require(euler <= 2 && euler % 2 == 0, Code::InvalidGeometry, shell.id,
                    "Source topology is not an orientable closed shell (including its inner face bounds).");
        }
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
                if(neighbor == grid.end() && face.periodic[constantU ? 0 : 1]) {
                    neighbor = grid.find({constantU ? 0 : p.iu, constantU ? p.iv : 0});
                }
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
                    auto hits = (document.IsA(seam.face->geometry, "TOROIDAL_SURFACE") ||
                                 document.IsA(seam.face->geometry, "SPHERICAL_SURFACE")) ?
                        Nurbs::TorusSeamIntersections(piece.bezier, seam.bezier) :
                        Nurbs::Intersections(use.edge, piece.bezier, seam.bezier);
                    for(const auto &hit : hits) {
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
                Point2d tangent = ParameterDelta(face, Project(&face, piece.bezier.PointAt(0.51)).Minus(
                                                      Project(&face, piece.bezier.PointAt(0.49))));
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
            reference.shell = shells[faceShell.at(face.id)].id;
            reference.orientedShell = shells[faceShell.at(face.id)].oriented;
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

    static void ImproveValidationTriangulation(const SSurface &surface, SMesh *mesh) {
        // Ear clipping in UV can make skinny boundary-only triangles whose XYZ
        // chords fold or lie in a neighbouring face's plane, although the exact
        // surface is regular. Flip only interior diagonals of convex UV quads,
        // improving the worse XYZ triangle. No vertex, trim, UV coverage or
        // validation tolerance changes; all normal/intersection checks follow.
        auto quality = [&](const STriangle &uv) {
            if(uv.Normal().z >= 0) return -std::numeric_limits<double>::infinity();
            STriangle xyz = uv;
            xyz.a = surface.PointAt(uv.a.x, uv.a.y);
            xyz.b = surface.PointAt(uv.b.x, uv.b.y);
            xyz.c = surface.PointAt(uv.c.x, uv.c.y);
            Vector center = uv.a.Plus(uv.b).Plus(uv.c).ScaledBy(1.0 / 3);
            double size = xyz.a.Minus(xyz.b).MagSquared() + xyz.b.Minus(xyz.c).MagSquared() +
                          xyz.c.Minus(xyz.a).MagSquared();
            return size > 0 ? -xyz.Normal().Dot(surface.NormalAt(center.x, center.y).WithMagnitude(1)) / size :
                              -std::numeric_limits<double>::infinity();
        };
        std::vector<double> scores;
        for(const auto &triangle : mesh->l) scores.push_back(quality(triangle));
        for(int pass = 0; pass < 32; ++pass) {
            using Point = std::pair<double, double>;
            std::map<std::pair<Point, Point>, std::pair<int, int>> adjacency;
            std::set<int> changed;
            for(int i = 0; i < mesh->l.n; ++i) for(int edge = 0; edge < 3; ++edge) {
                auto &triangle = mesh->l[i];
                Vector a = triangle.vertices[edge], b = triangle.vertices[(edge + 1) % 3];
                Point ka = {a.x, a.y}, kb = {b.x, b.y};
                auto match = adjacency.find({kb, ka});
                if(match == adjacency.end()) { adjacency[{ka, kb}] = {i, edge}; continue; }
                int j = match->second.first, other = match->second.second;
                if(changed.count(i) || changed.count(j)) continue;
                if(triangle.Normal().z > 0 || mesh->l[j].Normal().z > 0) continue;
                Vector c = triangle.vertices[(edge + 2) % 3], d = mesh->l[j].vertices[(other + 2) % 3];
                STriangle first = {}, second = {};
                first.a = c; first.b = d; first.c = b;
                second.a = d; second.b = c; second.c = a;
                double q1 = quality(first), q2 = quality(second);
                if(!std::isfinite(q1) || !std::isfinite(q2) ||
                   std::min(q1, q2) <= std::min(scores[i], scores[j]) + 1e-10) continue;
                mesh->l[i] = first; mesh->l[j] = second;
                scores[i] = q1; scores[j] = q2;
                changed.insert(i); changed.insert(j);
            }
            if(changed.empty()) break;
        }
    }

    void ValidateNative(NativeSolid *result) {
        std::vector<Triangle> triangles;
        struct Measure { Vector anchor; double volume = 0, area = 0; };
        std::vector<Measure> measures;
        for(const auto &shell : shells) {
            const Face *first = nullptr;
            for(const auto &face : faces) if(face.id == shell.faces.front()) first = &face;
            ssassert(first != nullptr, "Source shell has no face");
            measures.push_back({vertices.at(first->loops.front().vertices.front()).position});
        }
        for(auto &surface : result->shell.surface) {
            int sourceFace = result->outerShell;
            for(const auto &face : result->faces) {
                if(std::find(face.surfaces.begin(), face.surfaces.end(), surface.h) != face.surfaces.end()) sourceFace = face.face;
            }
            auto &measure = measures[faceShell.at(sourceFace)];
            Vector anchor = measure.anchor;
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
            else {
                polygon.polygon.UvGridTriangulateInto(&mesh.mesh, &surface);
                bool obliqueTrim = false;
                for(const auto &edge : edges.edges.l) {
                    if(std::abs(edge.a.x - edge.b.x) > 1e-12 &&
                       std::abs(edge.a.y - edge.b.y) > 1e-12) obliqueTrim = true;
                }
                if(obliqueTrim) ImproveValidationTriangulation(surface, &mesh.mesh);
            }
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
                measure.volume += triangle.a.Minus(anchor).Dot(triangle.b.Minus(anchor).Cross(triangle.c.Minus(anchor))) / 6;
                measure.area += triangle.Area();
                Triangle record = {triangle, triangle.a, triangle.a};
                triangle.b.MakeMaxMin(&record.max, &record.min); triangle.c.MakeMaxMin(&record.max, &record.min);
                Require(triangles.size() < MAX_VALIDATION_TRIANGLES, Code::Unsupported, result->outerShell,
                        "Native shell tessellation-validation limit exceeded; no mesh fallback is performed.");
                triangles.push_back(record);
            }
            Require(std::abs(coveredArea - expectedArea) <= std::max(1e-10, expectedArea * 1e-6),
                    Code::InvalidGeometry, sourceFace, "Native triangulation does not cover its complete trimmed UV region.");
        }
        double volume = 0, area = 0;
        for(size_t i = 0; i < shells.size(); ++i) {
            const auto &measure = measures[i];
            Require(std::isfinite(measure.volume) && std::isfinite(measure.area) &&
                    measure.volume > measure.area * LENGTH_EPS, Code::InvalidGeometry, shells[i].id,
                    "Non-positive or numerically degenerate enclosed native volume in a base CLOSED_SHELL.");
            volume += i == 0 ? measure.volume : -measure.volume;
            area += measure.area;
        }
        Require(std::isfinite(volume) && std::isfinite(area) && volume > area * LENGTH_EPS,
                Code::InvalidGeometry, result->outerShell, "Non-positive or numerically degenerate enclosed native volume.");
        ValidateIntersections(result->outerShell, &triangles);
    }

    void ValidateVoids(NativeSolid *result) {
        if(shells.size() == 1) return;
        std::vector<ShellBoundary> boundaries(shells.size());
        Vector anchor = vertices.begin()->second.position;
        for(const auto &face : faces) {
            size_t s = faceShell.at(face.id);
            auto &boundary = boundaries[s];
            if(face.id == shells[s].faces.front()) {
                boundary.probe = vertices.at(face.loops.front().vertices.front()).position.Minus(anchor);
            }
            for(const auto &patch : face.patches) {
                if(patch.trims.empty()) continue;
                auto native = result->shell.surface.FindById(patch.handle);
                ShellBoundary::Patch copy = {};
                copy.surface = SSurface::FromTransformationOf(native, anchor.Negated(),
                    Quaternion::IDENTITY, 1, /*includingTrims=*/false);
                copy.plane = copy.surface.GetPlane();
                copy.surface.GetAxisAlignedBounding(&copy.max, &copy.min);
                if(copy.plane.isPlanar) {
                    // A trimmed planar region lies inside its exact boundary's
                    // control hull. Do not use this bound for curved interiors.
                    Vector lo = {VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE}, hi = lo.Negated();
                    for(const auto &loop : face.loops) for(const auto &use : loop.edges) {
                        for(const auto &piece : edges.at(use.edge).pieces) {
                            for(int i = 0; i <= piece.bezier.deg; ++i) {
                                piece.bezier.ctrl[i].Minus(anchor).MakeMaxMin(&hi, &lo);
                            }
                        }
                    }
                    // Conservatively include source/surface agreement and
                    // subdivision roundoff; this only enlarges validation boxes.
                    Vector padding = {LENGTH_EPS, LENGTH_EPS, LENGTH_EPS};
                    lo = lo.Minus(padding); hi = hi.Plus(padding);
                    copy.min = {std::max(copy.min.x, lo.x), std::max(copy.min.y, lo.y), std::max(copy.min.z, lo.z)};
                    copy.max = {std::min(copy.max.x, hi.x), std::min(copy.max.y, hi.y), std::min(copy.max.z, hi.z)};
                }
                EdgeListOwner uv;
                native->MakeEdgesInto(&result->shell, &uv.edges, SSurface::MakeAs::UV);
                copy.surface.bsp = SBspUv::From(&uv.edges, &copy.surface);
                Require(copy.surface.bsp != nullptr, Code::InvalidGeometry, face.id,
                        "Cannot classify an empty native cavity/outer face.");
                copy.min.MakeMaxMin(&boundary.max, &boundary.min);
                copy.max.MakeMaxMin(&boundary.max, &boundary.min);
                boundary.patches.push_back(copy);
            }
        }
        size_t work = 0;
        using Location = ShellBoundary::Location;
        auto requireLocation = [&](Location actual, Location expected, int entity) {
            Require(actual != Location::Ambiguous, Code::Unsupported, entity,
                    "Cavity-shell containment is numerically ambiguous or exceeds the validation work limit; no ownership was guessed.");
            Require(actual == expected, Code::InvalidGeometry, entity,
                    "Cavity shells must lie strictly inside the outer shell and outside one another; no cavity was stripped or merged.");
        };
        for(size_t i = 1; i < shells.size(); ++i) for(size_t j = 0; j < i; ++j) {
            Require(boundaries[i].SeparatedFrom(boundaries[j], &work), Code::Unsupported,
                    shells[i].oriented,
                    "Cavity shells touch/intersect, or exact patch separation is unresolved within the validation limits; the entire solid is omitted.");
            requireLocation(boundaries[j].Classify(boundaries[i].probe, &work),
                            j == 0 ? Location::Inside : Location::Outside, shells[i].oriented);
            requireLocation(boundaries[i].Classify(boundaries[j].probe, &work),
                            Location::Outside, shells[i].oriented);
        }
    }
};

GeometryDiagnostic Diagnostic(const Failure &failure, int representation, int solid,
                              const std::vector<int> &path = {}) {
    GeometryDiagnostic diagnostic;
    diagnostic.code = failure.code;
    diagnostic.representation = representation;
    diagnostic.solid = solid;
    diagnostic.entity = failure.entity;
    diagnostic.message = failure.message;
    diagnostic.occurrencePath = path;
    return diagnostic;
}

std::unique_ptr<NativeSolid> PlaceSolid(NativeSolid &source, const SolidOccurrence &occurrence) {
    std::unique_ptr<NativeSolid> result(new NativeSolid);
    result->representation = source.representation;
    result->context = source.context;
    result->solid = source.solid;
    result->outerShell = source.outerShell;
    result->lengthToMm = source.lengthToMm;
    result->occurrencePath = occurrence.path;
    const auto &placement = occurrence.placement;
    result->shell.MakeFromTransformationOf(&source.shell, placement.translation, placement.rotation, 1);
    result->vertices = source.vertices;
    result->edges = source.edges;
    result->faces = source.faces;
    auto check = [&](Vector point) {
        Require(Finite(point) && std::abs(point.x) <= MAX_COORDINATE_MM &&
                std::abs(point.y) <= MAX_COORDINATE_MM && std::abs(point.z) <= MAX_COORDINATE_MM,
                Code::Unsupported, source.solid,
                "Placed native geometry exceeds the finite +/-1e8 mm coordinate envelope; no occurrence was approximated.");
    };
    for(auto &vertex : result->vertices) {
        vertex.position = placement.Apply(vertex.position);
        check(vertex.position);
    }
    for(auto &face : result->faces) {
        face.origin = placement.Apply(face.origin);
        face.normal = placement.rotation.Rotate(face.normal);
        Require(Finite(face.origin) && Finite(face.normal), Code::Unsupported, face.face,
                "Non-finite placed native face reference.");
    }
    for(const auto &surface : result->shell.surface) {
        for(int i = 0; i <= surface.degm; ++i) for(int j = 0; j <= surface.degn; ++j) check(surface.ctrl[i][j]);
        for(const auto &trim : surface.trim) { check(trim.start); check(trim.finish); }
    }
    for(const auto &curve : result->shell.curve) {
        for(int i = 0; i <= curve.exact.deg; ++i) check(curve.exact.ctrl[i]);
        for(const auto &point : curve.pts) check(point.p);
    }
    return result;
}

} // namespace

NativeSolid::~NativeSolid() { shell.Clear(); }

GeometryResult ConvertGeometry(const Document &document, const GeometryOptions &options) {
    GeometryResult result;
    OccurrenceGraph graph(document);
    std::vector<SolidOccurrence> occurrences;
    try {
        occurrences = graph.Resolve();
    } catch(const Failure &failure) {
        result.diagnostics.push_back(Diagnostic(failure, graph.currentRepresentation, 0));
        return result;
    }
    for(int solid : document.EntitiesOfType("SOLID_MODEL")) {
        if(graph.candidates.count(solid)) continue;
        result.diagnostics.push_back(Diagnostic({Code::Representation, solid,
            "Solid has no supported direct shape representation/unit context; it is omitted."}, 0, solid));
    }
    // Validate each source definition exactly once, then make independently
    // owned rigid copies. No Boolean, fitting, per-component group or mesh is
    // involved; local shell handles remain available to the link-layer remap.
    std::map<int, std::unique_ptr<NativeSolid>> definitions;
    std::map<int, GeometryDiagnostic> failures;
    size_t surfaces = 0, curves = 0, trimPoints = 0;
    for(const auto &occurrence : occurrences) {
        int representation = occurrence.representation, solid = occurrence.solid;
        if(!definitions.count(solid) && !failures.count(solid)) {
            try {
                definitions.emplace(solid, SolidBuilder(document, options).Build(representation, solid));
            } catch(const Failure &failure) {
                failures.emplace(solid, Diagnostic(failure, representation, solid));
            } catch(const Nurbs::Error &failure) {
                failures.emplace(solid, Diagnostic({failure.unsupported ? Code::Unsupported : Code::InvalidGeometry,
                    failure.entity, failure.message}, representation, solid));
            }
        }
        auto failure = failures.find(solid);
        if(failure != failures.end()) {
            auto diagnostic = failure->second;
            diagnostic.occurrencePath = occurrence.path;
            result.diagnostics.push_back(std::move(diagnostic));
            continue;
        }
        auto &definition = *definitions.at(solid);
        surfaces += definition.shell.surface.n; curves += definition.shell.curve.n;
        for(const auto &curve : definition.shell.curve) trimPoints += curve.pts.n;
        if(surfaces > MAX_ASSEMBLY_SURFACES || curves > MAX_ASSEMBLY_CURVES ||
           trimPoints > MAX_ASSEMBLY_TRIM_POINTS) {
            result.solids.clear();
            result.diagnostics.push_back(Diagnostic({Code::Representation, solid,
                "Complete assembly exceeds the native surface/curve/trim-point work limit; no geometry was linked."},
                representation, 0, occurrence.path));
            return result;
        }
        try {
            result.solids.push_back(PlaceSolid(definition, occurrence));
        } catch(const Failure &failure) {
            result.diagnostics.push_back(Diagnostic(failure, representation, solid, occurrence.path));
        }
    }
    if(!result.solids.empty()) {
        result.status = result.diagnostics.empty() ? GeometryResult::Status::Okay : GeometryResult::Status::Partial;
    } else if(result.diagnostics.empty()) {
        result.diagnostics.push_back(Diagnostic({Code::NoGeometry, 0, "No supported solid geometry was found."}, 0, 0));
    }
    // Presentation must not influence geometric acceptance or reference identity.
    ApplySurfaceColors(document, &result);
    return result;
}

} // namespace Step
} // namespace SolveSpace
