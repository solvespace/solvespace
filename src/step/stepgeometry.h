// Experimental native conversion, separate from parsing and file/group linking.
// Only SolveSpace geometry and source instance IDs cross this boundary.
#ifndef SOLVESPACE_STEP_GEOMETRY_H
#define SOLVESPACE_STEP_GEOMETRY_H

#include <memory>
#include <string>
#include <vector>

#include "srf/surface.h"

namespace SolveSpace {
namespace Step {

class Document;

struct GeometryDiagnostic {
    enum class Code { Representation, Units, Unsupported, InvalidGeometry, NoGeometry };
    Code code = Code::NoGeometry;
    int representation = 0;
    int solid = 0;
    int entity = 0;
    std::string message;
};

// These are provenance, not sketch entities or persistent cross-export names.
// Handles are local to one solid and deterministic for unchanged Part 21 IDs.
// A future link layer must namespace/remap them and create actual hEntities.
struct VertexReference {
    int vertex;
    int point;
    Vector position; // mm
};

struct EdgeReference {
    int edge;
    int geometry; // LINE or supported B_SPLINE_CURVE_WITH_KNOTS
    hSCurve curve;
    int startVertex;
    int finishVertex;
};

struct FaceReference {
    int face;
    int geometry; // PLANE or supported B_SPLINE_SURFACE_WITH_KNOTS
    hSSurface surface;
    Vector origin;
    Vector normal; // unit outward normal, including face.same_sense
};

class NativeSolid {
public:
    int representation = 0;
    int context = 0;
    int solid = 0;
    int outerShell = 0;
    double lengthToMm = 0;
    SShell shell = {};
    std::vector<VertexReference> vertices;
    std::vector<EdgeReference> edges;
    std::vector<FaceReference> faces;

    NativeSolid() = default;
    ~NativeSolid();
    NativeSolid(const NativeSolid &) = delete;
    NativeSolid &operator=(const NativeSolid &) = delete;
};

struct GeometryResult {
    enum class Status { Okay, Partial, Rejected };
    Status status = Status::Rejected;
    std::vector<std::unique_ptr<NativeSolid>> solids;
    // Each failed solid is omitted in its entirety. Representation-level
    // ambiguity rejects the document rather than importing misplaced parts.
    std::vector<GeometryDiagnostic> diagnostics;
};

// First vertical slice only: direct 3D representations of convex polyhedra,
// with explicit length units, planar faces and linear edges. Also accepts the
// non-rational, clamped single-span B-spline equivalents (affine 2x2 surfaces,
// two-control-point curves). One EDGE_LOOP per face; no healing, meshing
// fallback, cavity stripping or assembly flattening.
// Uses SolveSpace's fixed LENGTH_EPS in mm, not a relaxed STEP uncertainty.
// Single-threaded, like the current Document adapter. Does not retain Document.
GeometryResult ConvertGeometry(const Document &document);

} // namespace Step
} // namespace SolveSpace

#endif
