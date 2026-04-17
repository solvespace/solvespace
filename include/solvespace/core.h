/*-----------------------------------------------------------------------------
 * SolveSpace Core C++ API
 *
 * A complete C++ interface for the SolveSpace constraint solver and
 * parametric modeling engine. This header provides everything needed to
 * build a full-featured CAD UI or embed SolveSpace into any application:
 *
 *   - Create and manage groups (sketch, extrude, revolve, translate, etc.)
 *   - Create parametric 2D/3D sketches with geometric entities
 *   - Apply geometric constraints and solve them
 *   - Undo/redo support
 *   - Generate triangle meshes and NURBS surfaces
 *   - Measurement (volume, area, perimeter, center of mass)
 *   - Style management (colors, line widths, stipple patterns)
 *   - Interactive drag support
 *   - Load/save .slvs files
 *   - Import DXF; export STEP, STL, OBJ, DXF, SVG, PDF
 *   - Higher-level operations (split curves, tangent arcs, clipboard)
 *
 * Link against libsolvespace-core.a and its dependencies (slvs-solver, dxfrw).
 *
 * Copyright 2008-2013 Jonathan Westhues.
 * Copyright 2024 SolveSpace contributors.
 *---------------------------------------------------------------------------*/

#ifndef SOLVESPACE_CORE_API_H
#define SOLVESPACE_CORE_API_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace SolveSpace {
namespace Api {

// ========================================================================
//  Value types
// ========================================================================

struct Vec3 {
    double x = 0, y = 0, z = 0;

    Vec3() = default;
    Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    Vec3 Plus(Vec3 b)       const { return {x+b.x, y+b.y, z+b.z}; }
    Vec3 Minus(Vec3 b)      const { return {x-b.x, y-b.y, z-b.z}; }
    Vec3 ScaledBy(double s) const { return {x*s, y*s, z*s}; }
    double Dot(Vec3 b)      const { return x*b.x + y*b.y + z*b.z; }
    Vec3 Cross(Vec3 b) const {
        return {y*b.z - z*b.y, z*b.x - x*b.z, x*b.y - y*b.x};
    }
    double Magnitude() const;
};

struct Quat {
    double w = 1, x = 0, y = 0, z = 0;

    Quat() = default;
    Quat(double w, double x, double y, double z) : w(w), x(x), y(y), z(z) {}

    static Quat From(Vec3 u, Vec3 v);
    Vec3 RotationU() const;
    Vec3 RotationV() const;
    Vec3 RotationN() const;
};

struct RgbaColor {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

struct Triangle {
    Vec3      a, b, c;
    Vec3      normals[3];
    RgbaColor color;
};

struct Edge {
    Vec3 a, b;
};

struct BezierCurve {
    int     degree;         // 1 = line, 2 = conic, 3 = cubic
    Vec3    ctrl[4];
    double  weight[4];      // rational weight (1.0 for polynomial)
};

struct BoundingBox {
    Vec3 minp, maxp;
};

// ========================================================================
//  Handle types
// ========================================================================

struct hGroup      { uint32_t v = 0; };
struct hRequest    { uint32_t v = 0; };
struct hEntity     { uint32_t v = 0; };
struct hConstraint { uint32_t v = 0; };
struct hParam      { uint32_t v = 0; };
struct hStyle      { uint32_t v = 0; };

// ========================================================================
//  Enumerations — values match internal SolveSpace enums for direct mapping
// ========================================================================

enum class Unit { MM, INCHES, FEET_INCHES, METERS };

enum class SolveResult {
    OKAY              = 0,
    INCONSISTENT      = 1,
    DIDNT_CONVERGE    = 2,
    TOO_MANY_UNKNOWNS = 3,
    REDUNDANT_OKAY    = 4,
};

// --- Group types (match Group::Type) ---
enum class GroupType : uint32_t {
    DRAWING_3D          = 5000,
    DRAWING_WORKPLANE   = 5001,
    EXTRUDE             = 5100,
    LATHE               = 5101,
    REVOLVE             = 5102,
    HELIX               = 5103,
    ROTATE              = 5200,
    TRANSLATE           = 5201,
    LINKED              = 5300,
};

enum class GroupSubtype : uint32_t {
    WORKPLANE_BY_POINT_ORTHO   = 6000,
    WORKPLANE_BY_LINE_SEGMENTS = 6001,
    WORKPLANE_BY_POINT_NORMAL  = 6002,
    ONE_SIDED                  = 7000,
    TWO_SIDED                  = 7001,
    ONE_SKEWED                 = 7004,
};

enum class MeshCombine : uint32_t {
    UNION        = 0,
    DIFFERENCE   = 1,
    ASSEMBLE     = 2,
    INTERSECTION = 3,
};

// --- Request types (match Request::Type) ---
enum class RequestType : uint32_t {
    WORKPLANE       = 100,
    DATUM_POINT     = 101,
    LINE_SEGMENT    = 200,
    CUBIC           = 300,
    CUBIC_PERIODIC  = 301,
    CIRCLE          = 400,
    ARC_OF_CIRCLE   = 500,
    TTF_TEXT        = 600,
    IMAGE           = 700,
};

// --- Entity types (match EntityBase::Type) ---
enum class EntityType : uint32_t {
    POINT_IN_3D     = 2000,
    POINT_IN_2D     = 2001,
    NORMAL_IN_3D    = 3000,
    NORMAL_IN_2D    = 3001,
    DISTANCE        = 4000,
    WORKPLANE       = 10000,
    LINE_SEGMENT    = 11000,
    CUBIC           = 12000,
    CUBIC_PERIODIC  = 12001,
    CIRCLE          = 13000,
    ARC_OF_CIRCLE   = 14000,
    TTF_TEXT        = 15000,
    IMAGE           = 16000,
};

// --- Constraint types (match ConstraintBase::Type) ---
enum class ConstraintType : uint32_t {
    POINTS_COINCIDENT   =  20,
    PT_PT_DISTANCE      =  30,
    PT_PLANE_DISTANCE   =  31,
    PT_LINE_DISTANCE    =  32,
    PT_FACE_DISTANCE    =  33,
    PROJ_PT_DISTANCE    =  34,
    PT_IN_PLANE         =  41,
    PT_ON_LINE          =  42,
    PT_ON_FACE          =  43,
    EQUAL_LENGTH_LINES  =  50,
    LENGTH_RATIO        =  51,
    EQ_LEN_PT_LINE_D    =  52,
    EQ_PT_LN_DISTANCES  =  53,
    EQUAL_ANGLE         =  54,
    EQUAL_LINE_ARC_LEN  =  55,
    LENGTH_DIFFERENCE   =  56,
    SYMMETRIC           =  60,
    SYMMETRIC_HORIZ     =  61,
    SYMMETRIC_VERT      =  62,
    SYMMETRIC_LINE      =  63,
    AT_MIDPOINT         =  70,
    HORIZONTAL          =  80,
    VERTICAL            =  81,
    DIAMETER            =  90,
    PT_ON_CIRCLE        = 100,
    SAME_ORIENTATION    = 110,
    ANGLE               = 120,
    PARALLEL            = 121,
    PERPENDICULAR       = 122,
    ARC_LINE_TANGENT    = 123,
    CUBIC_LINE_TANGENT  = 124,
    CURVE_CURVE_TANGENT = 125,
    EQUAL_RADIUS        = 130,
    WHERE_DRAGGED       = 200,
    ARC_ARC_LEN_RATIO   = 210,
    ARC_LINE_LEN_RATIO  = 211,
    ARC_ARC_DIFFERENCE  = 212,
    ARC_LINE_DIFFERENCE = 213,
    COMMENT             = 1000,
};

// --- Stipple patterns (match StipplePattern) ---
enum class StippleType : uint32_t {
    CONTINUOUS  = 0,
    SHORT_DASH  = 1,
    DASH        = 2,
    LONG_DASH   = 3,
    DASH_DOT    = 4,
    DASH_DOT_DOT= 5,
    DOT         = 6,
    FREEHAND    = 7,
    ZIGZAG      = 8,
};

// --- Style width/height units (match Style::UnitsAs) ---
enum class UnitsAs : uint32_t {
    PIXELS = 0,
    MM     = 1,
};

// ========================================================================
//  Info / result structs
// ========================================================================

struct SolveStatus {
    SolveResult                 result;
    int                         dof;
    std::vector<hConstraint>    failed;
    std::vector<hConstraint>    redundant;
};

struct GroupInfo {
    hGroup      h;
    GroupType   type;
    int         order;
    std::string name;

    // Boolean flags
    bool        visible;
    bool        suppress;
    bool        relaxConstraints;
    bool        allowRedundant;
    bool        forceToMesh;
    bool        allDimsReference;

    // Combine mode for solid operations
    MeshCombine meshCombine;
    GroupSubtype subtype;

    // Numeric parameters
    double      valA;       // repeat count (step groups), extrude depth, etc.
    double      valB;       // helix pitch
    double      valC;
    double      scale;      // for linked groups
    RgbaColor   color;

    // Workplane/predef
    hEntity     activeWorkplane;
    hGroup      opA;        // source group for step operations
    Quat        predefQuat; // predefined orientation
    hEntity     predefOrigin;

    // Linked file
    std::string impFile;

    // Solve status
    SolveResult solveResult;
    int         solveDof;
};

struct RequestInfo {
    hRequest    h;
    RequestType type;
    hGroup      group;
    hEntity     workplane;
    bool        construction;
    std::string str;        // for TTF_TEXT
    std::string font;       // for TTF_TEXT
};

struct EntityInfo {
    hEntity     h;
    EntityType  type;
    hGroup      group;
    hEntity     workplane;

    // Sub-entities
    hEntity     point[4];   // point handles (e.g., endpoints of a line)
    int         numPoints;
    hEntity     normal;     // normal handle (for workplanes, circles)
    hEntity     distance;   // distance handle (for circles)

    // Computed values (after solving)
    Vec3        actPoint;
    Quat        actNormal;
    double      actDistance;
    bool        visible;
    bool        construction;
};

struct ConstraintInfo {
    hConstraint     h;
    ConstraintType  type;
    hGroup          group;
    hEntity         workplane;
    double          valA;
    hEntity         ptA, ptB;
    hEntity         entityA, entityB, entityC, entityD;
    bool            other;
    bool            other2;
    bool            reference;
    std::string     comment;
    // Display position (for UI label placement)
    Vec3            disp;
};

struct StyleInfo {
    hStyle      h;
    std::string name;

    double      width;
    UnitsAs     widthAs;
    double      textHeight;
    UnitsAs     textHeightAs;

    RgbaColor   color;
    RgbaColor   fillColor;
    bool        filled;
    bool        visible;
    bool        exportable;

    StippleType stippleType;
    double      stippleScale;
    int         zIndex;
};

// For clipboard operations
struct ClipboardEntry {
    RequestType type;
    Vec3        point;          // position (for datum points)
    bool        construction;
    // The entity handle mapping is handled internally
};

// ========================================================================
//  Engine — the main API class
// ========================================================================

class Engine {
public:
    Engine();
    ~Engine();

    // Non-copyable, movable
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    Engine(Engine &&) noexcept;
    Engine &operator=(Engine &&) noexcept;

    // ================================================================
    //  File I/O
    // ================================================================

    bool LoadFile(const std::string &path);
    bool SaveFile(const std::string &path);
    void Clear();

    // ================================================================
    //  Undo / Redo
    // ================================================================

    /// Save current state to undo stack. Call before any mutation.
    void SaveUndo();

    /// Undo the last operation. Returns false if nothing to undo.
    bool Undo();

    /// Redo the last undone operation. Returns false if nothing to redo.
    bool Redo();

    /// Check whether undo/redo is available.
    bool CanUndo() const;
    bool CanRedo() const;

    // ================================================================
    //  Group management
    // ================================================================

    hGroup GetActiveGroup() const;
    void   SetActiveGroup(hGroup hg);

    /// Get ordered list of all group handles.
    std::vector<hGroup> GetGroupOrder() const;

    /// Get detailed info about a group.
    GroupInfo GetGroupInfo(hGroup hg) const;

    /// Create a 3D sketch group.
    hGroup AddGroupDrawing3d();

    /// Create a 2D sketch group in a workplane.
    hGroup AddGroupDrawingWorkplane(hEntity workplaneOrigin, Quat orientation,
                                    GroupSubtype subtype = GroupSubtype::WORKPLANE_BY_POINT_ORTHO);

    /// Create an extrusion group from the active sketch.
    hGroup AddGroupExtrude(hGroup sourceGroup, double depth,
                           GroupSubtype subtype = GroupSubtype::ONE_SIDED);

    /// Create a lathe group (revolve 360 degrees).
    hGroup AddGroupLathe(hGroup sourceGroup);

    /// Create a revolve group.
    hGroup AddGroupRevolve(hGroup sourceGroup, double angleDeg,
                           GroupSubtype subtype = GroupSubtype::ONE_SIDED);

    /// Create a helix group.
    hGroup AddGroupHelix(hGroup sourceGroup, double turns, double pitch,
                         GroupSubtype subtype = GroupSubtype::ONE_SIDED);

    /// Create a step-translate group.
    hGroup AddGroupTranslate(hGroup sourceGroup, int copies);

    /// Create a step-rotate group.
    hGroup AddGroupRotate(hGroup sourceGroup, int copies);

    /// Link an external .slvs file as a group.
    hGroup AddGroupLinked(const std::string &filePath);

    /// Delete a group and all its requests/constraints.
    void DeleteGroup(hGroup hg);

    // --- Group property setters ---
    void SetGroupName(hGroup hg, const std::string &name);
    void SetGroupVisible(hGroup hg, bool visible);
    void SetGroupSuppress(hGroup hg, bool suppress);
    void SetGroupRelaxConstraints(hGroup hg, bool relax);
    void SetGroupAllowRedundant(hGroup hg, bool allow);
    void SetGroupForceToMesh(hGroup hg, bool force);
    void SetGroupAllDimsReference(hGroup hg, bool ref);
    void SetGroupMeshCombine(hGroup hg, MeshCombine mc);
    void SetGroupSubtype(hGroup hg, GroupSubtype sub);
    void SetGroupColor(hGroup hg, RgbaColor color);
    void SetGroupOpacity(hGroup hg, uint8_t alpha);
    void SetGroupValA(hGroup hg, double val);   // repeat count, depth, etc.
    void SetGroupValB(hGroup hg, double val);   // helix pitch
    void SetGroupScale(hGroup hg, double scale);// linked group scale

    // ================================================================
    //  Request / Entity creation
    // ================================================================

    /// Add a datum point in 3D.
    hEntity AddPoint3d(hGroup group, Vec3 pos);

    /// Add a datum point in a workplane.
    hEntity AddPoint2d(hGroup group, hEntity workplane, double u, double v);

    /// Add a 3D normal.
    hEntity AddNormal3d(hGroup group, Quat q);

    /// Add a line segment. Returns the line entity handle.
    /// The line's endpoints are sub-entities that can be queried.
    hEntity AddLineSegment(hGroup group, hEntity workplane,
                           hEntity ptA, hEntity ptB);

    /// Add a circle. Returns the circle entity handle.
    hEntity AddCircle(hGroup group, hEntity workplane,
                      hEntity center, hEntity normal, hEntity distance);

    /// Add an arc of circle.
    hEntity AddArcOfCircle(hGroup group, hEntity workplane,
                           hEntity normal, hEntity center,
                           hEntity start, hEntity end);

    /// Add a cubic spline.
    hEntity AddCubic(hGroup group, hEntity workplane,
                     hEntity pt0, hEntity pt1, hEntity pt2, hEntity pt3);

    /// Add a workplane from a point and normal.
    hEntity AddWorkplane(hGroup group, hEntity origin, hEntity normal);

    /// Add a distance parameter entity.
    hEntity AddDistance(hGroup group, hEntity workplane, double d);

    /// Add a TTF text entity.
    hEntity AddTTFText(hGroup group, hEntity workplane,
                       const std::string &text, const std::string &font,
                       Vec3 origin, double height);

    /// Delete a request and its generated entities.
    void DeleteRequest(hRequest hr);

    /// Toggle construction flag on a request.
    void SetRequestConstruction(hRequest hr, bool construction);

    /// Get the request that generated this entity (if entity is from a request).
    hRequest GetEntityRequest(hEntity he) const;

    /// Check if entity is from a request (vs. generated by group transform).
    bool EntityIsFromRequest(hEntity he) const;

    // ================================================================
    //  Entity queries
    // ================================================================

    int EntityCount() const;

    /// Get all entity handles in the sketch.
    std::vector<hEntity> GetAllEntities() const;

    /// Get entities belonging to a specific group.
    std::vector<hEntity> GetEntitiesInGroup(hGroup hg) const;

    /// Get detailed info about an entity, including sub-entity handles.
    EntityInfo GetEntityInfo(hEntity he) const;

    /// Get a point entity's solved 3D position.
    Vec3 GetPointPosition(hEntity he) const;

    /// Get a normal entity's solved quaternion.
    Quat GetNormalOrientation(hEntity he) const;

    /// Get a distance entity's solved value.
    double GetDistanceValue(hEntity he) const;

    /// Set/get parameter values directly.
    void   SetParamValue(hParam hp, double val);
    double GetParamValue(hParam hp) const;

    /// Force a point entity to a specific position (updates params).
    void SetPointPosition(hEntity he, Vec3 pos);

    /// Force a distance entity to a specific value.
    void SetDistanceValue(hEntity he, double val);

    /// Force a normal entity to a specific orientation.
    void SetNormalOrientation(hEntity he, Quat q);

    // ================================================================
    //  Constraint management
    // ================================================================

    /// Add a constraint. Returns the constraint handle.
    hConstraint AddConstraint(ConstraintType type, hGroup group,
                              hEntity workplane,
                              double value = 0.0,
                              hEntity ptA = {}, hEntity ptB = {},
                              hEntity entityA = {}, hEntity entityB = {});

    /// Remove a constraint.
    void RemoveConstraint(hConstraint hc);

    /// Get total constraint count.
    int ConstraintCount() const;

    /// Get all constraint handles.
    std::vector<hConstraint> GetAllConstraints() const;

    /// Get constraints in a specific group.
    std::vector<hConstraint> GetConstraintsInGroup(hGroup hg) const;

    /// Get detailed info about a constraint.
    ConstraintInfo GetConstraintInfo(hConstraint hc) const;

    /// Modify constraint value (distance, angle, etc.).
    void SetConstraintValue(hConstraint hc, double val);

    /// Toggle reference mode (non-driving dimension).
    void SetConstraintReference(hConstraint hc, bool reference);

    /// Toggle supplementary angle.
    void SetConstraintOther(hConstraint hc, bool other);

    /// Move constraint label display position.
    void SetConstraintDisplayOffset(hConstraint hc, Vec3 offset);

    // ================================================================
    //  Solving & Generation
    // ================================================================

    /// Regenerate all groups (geometry + solve). Call after mutations.
    void Regenerate();

    /// Solve all groups up to the active group. Returns detailed status.
    SolveStatus Solve();

    /// Solve a specific group only.
    SolveResult SolveGroup(hGroup hg);

    // ================================================================
    //  Geometry output (for rendering / export)
    // ================================================================

    /// Get the display triangle mesh for a group.
    std::vector<Triangle> GetGroupMesh(hGroup hg) const;

    /// Get the display triangle mesh for the active group.
    std::vector<Triangle> GetMesh() const;

    /// Get wireframe edges for entities up to active group.
    std::vector<Edge> GetEdges() const;

    /// Get edges for a specific group's display.
    std::vector<Edge> GetGroupEdges(hGroup hg) const;

    /// Get Bezier curves for a specific entity.
    std::vector<BezierCurve> GetBezierCurves(hEntity he) const;

    /// Get outline edges (silhouette) for a group.
    std::vector<Edge> GetGroupOutlines(hGroup hg) const;

    /// Get the bounding box of all visible geometry.
    BoundingBox GetBoundingBox() const;

    // ================================================================
    //  Measurements
    // ================================================================

    /// Calculate the volume of the active group's solid mesh.
    double GetVolume() const;

    /// Calculate the volume of a specific group's solid mesh.
    double GetGroupVolume(hGroup hg) const;

    /// Calculate the surface area of the active group's mesh.
    double GetSurfaceArea() const;

    /// Calculate the total edge length of given entities.
    double GetEntityLength(hEntity he) const;

    /// Get the center of mass of the active group's solid mesh.
    Vec3 GetCenterOfMass() const;

    // ================================================================
    //  Drag support (interactive parameter modification)
    // ================================================================

    /// Mark entity points as being dragged. Call before Solve during drag.
    void BeginDrag(const std::vector<hEntity> &points);

    /// Update dragged points to new positions and re-solve.
    void DragTo(const std::vector<hEntity> &points,
                const std::vector<Vec3> &positions);

    /// End dragging (clear dragged state).
    void EndDrag();

    // ================================================================
    //  Style management
    // ================================================================

    /// Create a new custom style. Returns its handle.
    hStyle CreateCustomStyle();

    /// Get all style handles.
    std::vector<hStyle> GetAllStyles() const;

    /// Get detailed info about a style.
    StyleInfo GetStyleInfo(hStyle hs) const;

    // --- Style property setters ---
    void SetStyleName(hStyle hs, const std::string &name);
    void SetStyleColor(hStyle hs, RgbaColor color);
    void SetStyleFillColor(hStyle hs, RgbaColor color);
    void SetStyleFilled(hStyle hs, bool filled);
    void SetStyleWidth(hStyle hs, double width, UnitsAs units);
    void SetStyleTextHeight(hStyle hs, double height, UnitsAs units);
    void SetStyleStipple(hStyle hs, StippleType type, double scale);
    void SetStyleVisible(hStyle hs, bool visible);
    void SetStyleExportable(hStyle hs, bool exportable);
    void SetStyleZIndex(hStyle hs, int zIndex);

    /// Assign a style to an entity's request.
    void AssignStyleToEntity(hEntity he, hStyle hs);

    /// Get the effective color for an entity (considering style, group, etc.).
    RgbaColor GetEntityColor(hEntity he) const;

    // ================================================================
    //  Import / Export
    // ================================================================

    bool ExportSTL(const std::string &path) const;
    bool ExportOBJ(const std::string &objPath,
                   const std::string &mtlPath) const;
    bool ExportSTEP(const std::string &path) const;

    /// Import a DXF file into a group.
    bool ImportDXF(const std::string &path, hGroup group);

    // ================================================================
    //  Higher-level sketch operations
    // ================================================================

    /// Split a line or curve at a point, creating two new entities.
    /// Returns handles to the two new entities (may be empty on failure).
    std::vector<hEntity> SplitEntityAt(hEntity entity, Vec3 splitPoint);

    /// Create a tangent arc between two curves meeting at a point.
    /// Returns the arc entity handle.
    hEntity MakeTangentArc(hEntity curveA, hEntity curveB,
                           hEntity sharedPoint);

    // ================================================================
    //  Clipboard
    // ================================================================

    /// Copy entities to internal clipboard.
    void CopyToClipboard(const std::vector<hEntity> &entities);

    /// Paste from clipboard with a translation offset.
    /// Returns handles of newly created entities.
    std::vector<hEntity> PasteFromClipboard(Vec3 offset);

    /// Paste with full transform (translation, rotation, scale, mirror).
    std::vector<hEntity> PasteTransformed(Vec3 offset, Quat rotation,
                                          double scale, bool mirror);

    // ================================================================
    //  Configuration
    // ================================================================

    void   SetUnit(Unit u);
    Unit   GetUnit() const;

    void   SetChordTolerance(double mm);
    double GetChordTolerance() const;

    void   SetMaxSegments(int n);
    int    GetMaxSegments() const;

    void   SetExportChordTolerance(double mm);
    void   SetExportMaxSegments(int n);
    void   SetExportScale(double s);

    /// Get/set the number of decimal places for display.
    void SetDecimalPlaces(int mm, int inch, int degree);

    // ================================================================
    //  Workplane helpers
    // ================================================================

    /// Get the reference workplane entities created at init.
    hEntity GetXYWorkplane() const;
    hEntity GetYZWorkplane() const;
    hEntity GetZXWorkplane() const;

    /// Get the origin point of a workplane entity.
    hEntity GetWorkplaneOrigin(hEntity workplane) const;

    /// Get the normal of a workplane entity.
    hEntity GetWorkplaneNormal(hEntity workplane) const;

    /// Convert a 3D point to 2D workplane coordinates.
    void ProjectOntoWorkplane(hEntity workplane, Vec3 p3d,
                              double *u, double *v) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Api
} // namespace SolveSpace

#endif // SOLVESPACE_CORE_API_H
