// Experimental native conversion, separate from parsing and file/group linking.
// Only SolveSpace geometry and source instance IDs cross this boundary.
#ifndef SOLVESPACE_STEP_GEOMETRY_H
#define SOLVESPACE_STEP_GEOMETRY_H

#include <memory>
#include <string>
#include <utility>
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
// The application link layer namespaces/remaps them and creates actual hEntities.
struct VertexReference {
    int vertex;
    int point;
    Vector position; // mm
};

struct EdgeReference {
    int edge;
    int geometry;
    hSCurve curve; // first native span (source ID for an unchanged single span)
    int startVertex;
    int finishVertex;
    bool linear; // a truthful LINE_SEGMENT constraint reference is possible
    int adjacentFaces[2]; // SOURCE faces, not artificial patch boundaries
    std::vector<hSCurve> curves; // exact decomposition/splits of this source edge
};

struct FaceReference {
    int face;
    int geometry;
    hSSurface surface; // first retained native patch
    Vector origin;
    Vector normal; // outward supporting-plane normal, only meaningful if planar
    bool planar; // curved walls must not masquerade as constraint planes
    std::vector<hSSurface> surfaces;
    // Effective SOURCE boundary directions, independent of native span/patch
    // splitting. Each loop is sorted by source edge ID for provenance.
    std::vector<std::vector<std::pair<int, bool>>> boundaries;
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

struct GeometryOptions {
    // Native trim/validation PWLs only; never a curve/surface fitting tolerance.
    // Zero chooses 0.1% of this solid's size, bounded to [32*LENGTH_EPS, 0.01] mm.
    // In particular, an empty/current sketch's display tolerance is irrelevant.
    double chordTolerance = 0;
    int maxSegments = 128;
};

// Direct, explicitly unit-qualified 3D B-reps. Lines, circles/circular arcs,
// B-spline curves and tensor-product B-spline surfaces are converted to exact
// rational Bezier spans of native degree <= 3;
// source boundaries are retained, not fitted or replaced by polygons. Native
// trim PWLs and validation tessellation are not geometry-conversion fallbacks.
// Plain shape representation relationships may connect axis-only intermediates
// to one geometry representation within a shared context; no transforms are inferred.
// Unsupported geometry/trimming omits the entire solid, with an entity-specific
// diagnostic. No healing, cavity stripping or assembly flattening.
// Uses SolveSpace's fixed LENGTH_EPS in mm, not a relaxed STEP uncertainty.
// Called synchronously by the GUI. Does not retain Document or vendor objects.
GeometryResult ConvertGeometry(const Document &document,
                               const GeometryOptions &options = GeometryOptions());

} // namespace Step
} // namespace SolveSpace

#endif
