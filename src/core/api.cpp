//-----------------------------------------------------------------------------
// Implementation of the SolveSpace Core public C++ API.
//
// Wraps the internal SS/SK globals and provides a clean interface for
// external programs to build UIs or embed the constraint solver.
//-----------------------------------------------------------------------------
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "solvespace.h"
#include "solvespace/core.h"

namespace SolveSpace {
namespace Api {

// ========================================================================
//  Handle conversion helpers
// ========================================================================

static inline ::SolveSpace::hGroup   IHG(hGroup h)      { ::SolveSpace::hGroup r;      r.v = h.v; return r; }
static inline ::SolveSpace::hEntity  IHE(hEntity h)     { ::SolveSpace::hEntity r;     r.v = h.v; return r; }
static inline ::SolveSpace::hConstraint IHC(hConstraint h) { ::SolveSpace::hConstraint r; r.v = h.v; return r; }
static inline ::SolveSpace::hParam   IHP(hParam h)      { ::SolveSpace::hParam r;      r.v = h.v; return r; }
static inline ::SolveSpace::hStyle   IHS(hStyle h)      { ::SolveSpace::hStyle r;      r.v = h.v; return r; }
static inline ::SolveSpace::hRequest IHR(hRequest h)    { ::SolveSpace::hRequest r;    r.v = h.v; return r; }

static inline hGroup      AHG(::SolveSpace::hGroup h)      { hGroup r;      r.v = h.v; return r; }
static inline hEntity     AHE(::SolveSpace::hEntity h)     { hEntity r;     r.v = h.v; return r; }
static inline hConstraint AHC(::SolveSpace::hConstraint h) { hConstraint r; r.v = h.v; return r; }
static inline hParam      AHP(::SolveSpace::hParam h)      { hParam r;      r.v = h.v; return r; }
static inline hStyle      AHS(::SolveSpace::hStyle h)      { hStyle r;      r.v = h.v; return r; }
static inline hRequest    AHR(::SolveSpace::hRequest h)    { hRequest r;    r.v = h.v; return r; }

// ========================================================================
//  Type conversion helpers
// ========================================================================

static Vector ToInternal(Vec3 v) { return Vector::From(v.x, v.y, v.z); }
static Vec3 FromInternal(Vector v) { Vec3 r; r.x = v.x; r.y = v.y; r.z = v.z; return r; }
static Quat FromInternal(Quaternion q) { Quat r; r.w = q.w; r.x = q.vx; r.y = q.vy; r.z = q.vz; return r; }
static Quaternion ToInternal(Quat q) { return Quaternion::From(q.w, q.x, q.y, q.z); }

static RgbaColor FromInternalColor(::SolveSpace::RgbaColor c) {
    RgbaColor r;
    r.r = c.red; r.g = c.green; r.b = c.blue; r.a = c.alpha;
    return r;
}

static ::SolveSpace::RgbaColor ToInternalColor(RgbaColor c) {
    return ::SolveSpace::RgbaColor::From(c.r, c.g, c.b, c.a);
}

// ========================================================================
//  Vec3 / Quat methods
// ========================================================================

double Vec3::Magnitude() const { return sqrt(x*x + y*y + z*z); }

Quat Quat::From(Vec3 u, Vec3 v) {
    Quaternion q = Quaternion::From(ToInternal(u), ToInternal(v));
    return FromInternal(q);
}

Vec3 Quat::RotationU() const { return FromInternal(ToInternal(*this).RotationU()); }
Vec3 Quat::RotationV() const { return FromInternal(ToInternal(*this).RotationV()); }
Vec3 Quat::RotationN() const { return FromInternal(ToInternal(*this).RotationN()); }

// ========================================================================
//  Engine::Impl
// ========================================================================

struct Engine::Impl {
    bool initialized = false;

    void EnsureInit() {
        if(initialized) return;
        initialized = true;

        SS.viewUnits = ::SolveSpace::Unit::MM;
        SS.afterDecimalMm = 2;
        SS.afterDecimalInch = 3;
        SS.afterDecimalDegree = 1;
        SS.chordTol = 0.5;
        SS.chordTolCalculated = 0.5;
        SS.maxSegments = 10;
        SS.exportChordTol = 0.1;
        SS.exportMaxSegments = 64;
        SS.cameraTangent = 0;
        SS.usePerspectiveProj = false;
        SS.exportScale = 1.0;

        SS.NewFile();
        SS.GenerateAll(SolveSpaceCore::Generate::ALL);

        if(SK.groupOrder.n >= 2) {
            SS.activeGroup = SK.groupOrder[1];
        } else if(SK.groupOrder.n >= 1) {
            SS.activeGroup = SK.groupOrder[0];
        }
    }

    // Insert a new group after the active group in order
    void InsertGroupAfterActive(Group *g) {
        Group *active = SK.group.FindByIdNoOops(SS.activeGroup);
        int activeOrder = active ? active->order : 0;

        g->order = activeOrder + 1;

        // Bump subsequent groups
        for(int i = 0; i < SK.group.n; i++) {
            if(SK.group[i].h.v != g->h.v && SK.group[i].order >= g->order) {
                SK.group[i].order++;
            }
        }
    }

    // Common group init
    void InitGroup(Group *g, Group::Type type, const char *name) {
        *g = {};
        g->visible = true;
        g->color = ::SolveSpace::RgbaColor::From(100, 100, 100);
        g->scale = 1;
        g->type = type;
        g->name = name;
    }
};

// ========================================================================
//  Engine lifecycle
// ========================================================================

Engine::Engine() : impl(new Impl()) {
    impl->EnsureInit();
}

Engine::~Engine() = default;
Engine::Engine(Engine &&other) noexcept : impl(std::move(other.impl)) {}
Engine &Engine::operator=(Engine &&other) noexcept { impl = std::move(other.impl); return *this; }

void Engine::Clear() {
    SS.NewFile();
    SS.GenerateAll(SolveSpaceCore::Generate::ALL);
    if(SK.groupOrder.n >= 2) {
        SS.activeGroup = SK.groupOrder[1];
    } else if(SK.groupOrder.n >= 1) {
        SS.activeGroup = SK.groupOrder[0];
    }
}

// ========================================================================
//  File I/O
// ========================================================================

bool Engine::LoadFile(const std::string &path) {
    return SS.LoadFromFile(Platform::Path::From(path), false);
}

bool Engine::SaveFile(const std::string &path) {
    return SS.SaveToFile(Platform::Path::From(path));
}

// ========================================================================
//  Undo / Redo
// ========================================================================

void Engine::SaveUndo() {
    SS.UndoRemember();
}

bool Engine::Undo() {
    if(SS.undo.cnt <= 0) return false;
    SS.UndoUndo();
    return true;
}

bool Engine::Redo() {
    if(SS.redo.cnt <= 0) return false;
    SS.UndoRedo();
    return true;
}

bool Engine::CanUndo() const { return SS.undo.cnt > 0; }
bool Engine::CanRedo() const { return SS.redo.cnt > 0; }

// ========================================================================
//  Group management
// ========================================================================

hGroup Engine::GetActiveGroup() const {
    return AHG(SS.activeGroup);
}

void Engine::SetActiveGroup(hGroup hg) {
    SS.activeGroup = IHG(hg);
}

std::vector<hGroup> Engine::GetGroupOrder() const {
    std::vector<hGroup> result;
    result.reserve(SK.groupOrder.n);
    for(int i = 0; i < SK.groupOrder.n; i++) {
        result.push_back(AHG(SK.groupOrder[i]));
    }
    return result;
}

GroupInfo Engine::GetGroupInfo(hGroup hg) const {
    GroupInfo info = {};
    Group *g = SK.group.FindByIdNoOops(IHG(hg));
    if(!g) return info;

    info.h = hg;
    info.type = static_cast<GroupType>(static_cast<uint32_t>(g->type));
    info.order = g->order;
    info.name = g->name;
    info.visible = g->visible;
    info.suppress = g->suppress;
    info.relaxConstraints = g->relaxConstraints;
    info.allowRedundant = g->allowRedundant;
    info.forceToMesh = g->forceToMesh;
    info.allDimsReference = g->allDimsReference;
    info.meshCombine = static_cast<MeshCombine>(static_cast<uint32_t>(g->meshCombine));
    info.subtype = static_cast<GroupSubtype>(static_cast<uint32_t>(g->subtype));
    info.valA = g->valA;
    info.valB = g->valB;
    info.valC = g->valC;
    info.scale = g->scale;
    info.color = FromInternalColor(g->color);
    info.activeWorkplane = AHE(g->activeWorkplane);
    info.opA = AHG(g->opA);
    info.predefQuat = FromInternal(g->predef.q);
    info.predefOrigin = AHE(g->predef.origin);
    info.solveResult = static_cast<SolveResult>(static_cast<int>(g->solved.how));
    info.solveDof = g->solved.dof;

    if(g->type == Group::Type::LINKED) {
        info.impFile = g->linkFile.raw;
    }

    return info;
}

hGroup Engine::AddGroupDrawing3d() {
    Group g = {};
    impl->InitGroup(&g, Group::Type::DRAWING_3D, "sketch-in-3d");
    SK.group.AddAndAssignId(&g);
    impl->InsertGroupAfterActive(&SK.group[SK.group.n - 1]);

    SS.activeGroup = SK.group[SK.group.n - 1].h;
    return AHG(SS.activeGroup);
}

hGroup Engine::AddGroupDrawingWorkplane(hEntity workplaneOrigin, Quat orientation,
                                         GroupSubtype subtype) {
    Group g = {};
    impl->InitGroup(&g, Group::Type::DRAWING_WORKPLANE, "sketch-in-plane");
    g.subtype = static_cast<Group::Subtype>(static_cast<uint32_t>(subtype));
    g.predef.q = ToInternal(orientation);
    g.predef.origin = IHE(workplaneOrigin);

    SK.group.AddAndAssignId(&g);
    impl->InsertGroupAfterActive(&SK.group[SK.group.n - 1]);

    Group *gg = &SK.group[SK.group.n - 1];
    SS.activeGroup = gg->h;
    gg->clean = false;

    SS.GenerateAll(SolveSpaceCore::Generate::ALL);

    // After generation, the workplane entity exists
    gg = SK.group.FindByIdNoOops(SS.activeGroup);
    if(gg) {
        gg->activeWorkplane = gg->h.entity(0);
    }

    return AHG(SS.activeGroup);
}

hGroup Engine::AddGroupExtrude(hGroup sourceGroup, double depth, GroupSubtype subtype) {
    Group g = {};
    impl->InitGroup(&g, Group::Type::EXTRUDE, "extrude");
    g.opA = IHG(sourceGroup);
    g.subtype = static_cast<Group::Subtype>(static_cast<uint32_t>(subtype));
    g.valA = depth;

    SK.group.AddAndAssignId(&g);
    impl->InsertGroupAfterActive(&SK.group[SK.group.n - 1]);

    SS.activeGroup = SK.group[SK.group.n - 1].h;
    return AHG(SS.activeGroup);
}

hGroup Engine::AddGroupLathe(hGroup sourceGroup) {
    Group g = {};
    impl->InitGroup(&g, Group::Type::LATHE, "lathe");
    g.opA = IHG(sourceGroup);

    SK.group.AddAndAssignId(&g);
    impl->InsertGroupAfterActive(&SK.group[SK.group.n - 1]);

    SS.activeGroup = SK.group[SK.group.n - 1].h;
    return AHG(SS.activeGroup);
}

hGroup Engine::AddGroupRevolve(hGroup sourceGroup, double angleDeg, GroupSubtype subtype) {
    Group g = {};
    impl->InitGroup(&g, Group::Type::REVOLVE, "revolve");
    g.opA = IHG(sourceGroup);
    g.valA = angleDeg;
    g.subtype = static_cast<Group::Subtype>(static_cast<uint32_t>(subtype));

    SK.group.AddAndAssignId(&g);
    impl->InsertGroupAfterActive(&SK.group[SK.group.n - 1]);

    SS.activeGroup = SK.group[SK.group.n - 1].h;
    return AHG(SS.activeGroup);
}

hGroup Engine::AddGroupHelix(hGroup sourceGroup, double turns, double pitch,
                              GroupSubtype subtype) {
    Group g = {};
    impl->InitGroup(&g, Group::Type::HELIX, "helix");
    g.opA = IHG(sourceGroup);
    g.valA = turns;
    g.valB = pitch;
    g.subtype = static_cast<Group::Subtype>(static_cast<uint32_t>(subtype));

    SK.group.AddAndAssignId(&g);
    impl->InsertGroupAfterActive(&SK.group[SK.group.n - 1]);

    SS.activeGroup = SK.group[SK.group.n - 1].h;
    return AHG(SS.activeGroup);
}

hGroup Engine::AddGroupTranslate(hGroup sourceGroup, int copies) {
    Group g = {};
    impl->InitGroup(&g, Group::Type::TRANSLATE, "translate");
    g.opA = IHG(sourceGroup);
    g.valA = copies;
    g.subtype = Group::Subtype::ONE_SIDED;

    SK.group.AddAndAssignId(&g);
    impl->InsertGroupAfterActive(&SK.group[SK.group.n - 1]);

    SS.activeGroup = SK.group[SK.group.n - 1].h;
    return AHG(SS.activeGroup);
}

hGroup Engine::AddGroupRotate(hGroup sourceGroup, int copies) {
    Group g = {};
    impl->InitGroup(&g, Group::Type::ROTATE, "rotate");
    g.opA = IHG(sourceGroup);
    g.valA = copies;
    g.subtype = Group::Subtype::ONE_SIDED;

    SK.group.AddAndAssignId(&g);
    impl->InsertGroupAfterActive(&SK.group[SK.group.n - 1]);

    SS.activeGroup = SK.group[SK.group.n - 1].h;
    return AHG(SS.activeGroup);
}

hGroup Engine::AddGroupLinked(const std::string &filePath) {
    Group g = {};
    impl->InitGroup(&g, Group::Type::LINKED, "link");
    g.meshCombine = Group::CombineAs::ASSEMBLE;
    g.linkFile = Platform::Path::From(filePath);

    SK.group.AddAndAssignId(&g);
    impl->InsertGroupAfterActive(&SK.group[SK.group.n - 1]);

    Group *gg = &SK.group[SK.group.n - 1];
    SS.activeGroup = gg->h;

    SS.ReloadAllLinked(SS.saveFile, false);
    gg->clean = false;
    SS.GenerateAll(SolveSpaceCore::Generate::ALL);

    return AHG(SS.activeGroup);
}

void Engine::DeleteGroup(hGroup hg) {
    SK.group.RemoveById(IHG(hg));
}

// --- Group property setters ---

#define GET_GROUP_OR_RETURN(hg) \
    Group *g = SK.group.FindByIdNoOops(IHG(hg)); \
    if(!g) return;

void Engine::SetGroupName(hGroup hg, const std::string &name) {
    GET_GROUP_OR_RETURN(hg); g->name = name;
}
void Engine::SetGroupVisible(hGroup hg, bool v) {
    GET_GROUP_OR_RETURN(hg); g->visible = v;
}
void Engine::SetGroupSuppress(hGroup hg, bool v) {
    GET_GROUP_OR_RETURN(hg); g->suppress = v;
}
void Engine::SetGroupRelaxConstraints(hGroup hg, bool v) {
    GET_GROUP_OR_RETURN(hg); g->relaxConstraints = v;
}
void Engine::SetGroupAllowRedundant(hGroup hg, bool v) {
    GET_GROUP_OR_RETURN(hg); g->allowRedundant = v;
}
void Engine::SetGroupForceToMesh(hGroup hg, bool v) {
    GET_GROUP_OR_RETURN(hg); g->forceToMesh = v;
}
void Engine::SetGroupAllDimsReference(hGroup hg, bool v) {
    GET_GROUP_OR_RETURN(hg); g->allDimsReference = v;
}
void Engine::SetGroupMeshCombine(hGroup hg, MeshCombine mc) {
    GET_GROUP_OR_RETURN(hg);
    g->meshCombine = static_cast<Group::CombineAs>(static_cast<uint32_t>(mc));
}
void Engine::SetGroupSubtype(hGroup hg, GroupSubtype sub) {
    GET_GROUP_OR_RETURN(hg);
    g->subtype = static_cast<Group::Subtype>(static_cast<uint32_t>(sub));
}
void Engine::SetGroupColor(hGroup hg, RgbaColor color) {
    GET_GROUP_OR_RETURN(hg); g->color = ToInternalColor(color);
}
void Engine::SetGroupOpacity(hGroup hg, uint8_t alpha) {
    GET_GROUP_OR_RETURN(hg); g->color.alpha = alpha;
}
void Engine::SetGroupValA(hGroup hg, double val) {
    GET_GROUP_OR_RETURN(hg); g->valA = val;
}
void Engine::SetGroupValB(hGroup hg, double val) {
    GET_GROUP_OR_RETURN(hg); g->valB = val;
}
void Engine::SetGroupScale(hGroup hg, double scale) {
    GET_GROUP_OR_RETURN(hg); g->scale = scale;
}

#undef GET_GROUP_OR_RETURN

// ========================================================================
//  Request / Entity creation
// ========================================================================

// Helper: add a request, generate its entities, and add them to SK
static ::SolveSpace::hRequest DoAddRequest(Request::Type type,
                                            ::SolveSpace::hGroup group,
                                            ::SolveSpace::hEntity workplane,
                                            bool construction = false) {
    Request r = {};
    r.type = type;
    r.group = group;
    r.workplane = workplane;
    r.construction = construction;
    SK.request.AddAndAssignId(&r);

    ::SolveSpace::hRequest hr = SK.request[SK.request.n - 1].h;

    // Generate entities and params from the request
    Request *rp = SK.GetRequest(hr);
    EntityList el = {};
    ParamList pl = {};
    rp->Generate(&el, &pl);

    for(int i = 0; i < pl.n; i++) {
        SK.param.Add(&pl[i]);
    }
    for(int i = 0; i < el.n; i++) {
        SK.entity.Add(&el[i]);
    }
    el.Clear();
    pl.Clear();

    return hr;
}

hEntity Engine::AddPoint3d(hGroup group, Vec3 pos) {
    ::SolveSpace::hRequest hr = DoAddRequest(
        Request::Type::DATUM_POINT, IHG(group), EntityBase::FREE_IN_3D);

    ::SolveSpace::hEntity he = hr.entity(0);
    EntityBase *e = SK.entity.FindByIdNoOops(he);
    if(e) e->PointForceTo(ToInternal(pos));

    return AHE(he);
}

hEntity Engine::AddPoint2d(hGroup group, hEntity workplane, double u, double v) {
    ::SolveSpace::hRequest hr = DoAddRequest(
        Request::Type::DATUM_POINT, IHG(group), IHE(workplane));

    ::SolveSpace::hEntity he = hr.entity(0);
    EntityBase *e = SK.entity.FindByIdNoOops(he);
    if(e) {
        // For 2D points, we need to compute the 3D position from workplane coords
        EntityBase *wp = SK.entity.FindByIdNoOops(IHE(workplane));
        if(wp) {
            EntityBase *wn = SK.entity.FindByIdNoOops(wp->normal);
            if(wn) {
                Quaternion q = wn->NormalGetNum();
                Vector orig = SK.GetEntity(wp->point[0])->PointGetNum();
                Vector p3d = orig.Plus(q.RotationU().ScaledBy(u))
                                 .Plus(q.RotationV().ScaledBy(v));
                e->PointForceTo(p3d);
            }
        }
    }

    return AHE(he);
}

hEntity Engine::AddNormal3d(hGroup group, Quat q) {
    ::SolveSpace::hRequest hr = DoAddRequest(
        Request::Type::WORKPLANE, IHG(group), EntityBase::FREE_IN_3D);

    ::SolveSpace::hEntity normalEntity = hr.entity(1);
    // The workplane request generates: entity(0) = workplane, point[0] = origin,
    // normal = the normal entity. Let's find and set the normal.
    EntityBase *wrkpl = SK.entity.FindByIdNoOops(hr.entity(0));
    if(wrkpl) {
        EntityBase *n = SK.entity.FindByIdNoOops(wrkpl->normal);
        if(n) {
            n->NormalForceTo(ToInternal(q));
            normalEntity = n->h;
        }
    }

    return AHE(normalEntity);
}

hEntity Engine::AddLineSegment(hGroup group, hEntity workplane,
                                hEntity ptA, hEntity ptB) {
    ::SolveSpace::hRequest hr = DoAddRequest(
        Request::Type::LINE_SEGMENT, IHG(group), IHE(workplane));

    // Copy point positions from source points to the line's endpoints
    ::SolveSpace::hEntity lineEnt = hr.entity(0);
    EntityBase *line = SK.entity.FindByIdNoOops(lineEnt);
    if(line) {
        EntityBase *srcA = SK.entity.FindByIdNoOops(IHE(ptA));
        EntityBase *srcB = SK.entity.FindByIdNoOops(IHE(ptB));
        EntityBase *dstA = SK.entity.FindByIdNoOops(line->point[0]);
        EntityBase *dstB = SK.entity.FindByIdNoOops(line->point[1]);
        if(srcA && dstA) dstA->PointForceTo(srcA->PointGetNum());
        if(srcB && dstB) dstB->PointForceTo(srcB->PointGetNum());
    }

    return AHE(lineEnt);
}

hEntity Engine::AddCircle(hGroup group, hEntity workplane,
                           hEntity center, hEntity normal, hEntity distance) {
    ::SolveSpace::hRequest hr = DoAddRequest(
        Request::Type::CIRCLE, IHG(group), IHE(workplane));

    ::SolveSpace::hEntity circleEnt = hr.entity(0);
    EntityBase *circle = SK.entity.FindByIdNoOops(circleEnt);
    if(circle) {
        // Set center point position
        EntityBase *srcCenter = SK.entity.FindByIdNoOops(IHE(center));
        EntityBase *dstCenter = SK.entity.FindByIdNoOops(circle->point[0]);
        if(srcCenter && dstCenter) dstCenter->PointForceTo(srcCenter->PointGetNum());

        // Set distance
        EntityBase *srcDist = SK.entity.FindByIdNoOops(IHE(distance));
        EntityBase *dstDist = SK.entity.FindByIdNoOops(circle->distance);
        if(srcDist && dstDist) dstDist->DistanceForceTo(srcDist->DistanceGetNum());
    }

    return AHE(circleEnt);
}

hEntity Engine::AddArcOfCircle(hGroup group, hEntity workplane,
                                hEntity normal, hEntity center,
                                hEntity start, hEntity end) {
    ::SolveSpace::hRequest hr = DoAddRequest(
        Request::Type::ARC_OF_CIRCLE, IHG(group), IHE(workplane));

    ::SolveSpace::hEntity arcEnt = hr.entity(0);
    EntityBase *arc = SK.entity.FindByIdNoOops(arcEnt);
    if(arc) {
        EntityBase *srcC = SK.entity.FindByIdNoOops(IHE(center));
        EntityBase *srcS = SK.entity.FindByIdNoOops(IHE(start));
        EntityBase *srcE = SK.entity.FindByIdNoOops(IHE(end));

        EntityBase *dstC = SK.entity.FindByIdNoOops(arc->point[0]);
        EntityBase *dstS = SK.entity.FindByIdNoOops(arc->point[1]);
        EntityBase *dstE = SK.entity.FindByIdNoOops(arc->point[2]);

        if(srcC && dstC) dstC->PointForceTo(srcC->PointGetNum());
        if(srcS && dstS) dstS->PointForceTo(srcS->PointGetNum());
        if(srcE && dstE) dstE->PointForceTo(srcE->PointGetNum());
    }

    return AHE(arcEnt);
}

hEntity Engine::AddCubic(hGroup group, hEntity workplane,
                          hEntity pt0, hEntity pt1, hEntity pt2, hEntity pt3) {
    ::SolveSpace::hRequest hr = DoAddRequest(
        Request::Type::CUBIC, IHG(group), IHE(workplane));

    ::SolveSpace::hEntity cubicEnt = hr.entity(0);
    EntityBase *cubic = SK.entity.FindByIdNoOops(cubicEnt);
    if(cubic) {
        ::SolveSpace::hEntity srcPts[] = { IHE(pt0), IHE(pt1), IHE(pt2), IHE(pt3) };
        for(int i = 0; i < 4; i++) {
            EntityBase *src = SK.entity.FindByIdNoOops(srcPts[i]);
            EntityBase *dst = SK.entity.FindByIdNoOops(cubic->point[i]);
            if(src && dst) dst->PointForceTo(src->PointGetNum());
        }
    }

    return AHE(cubicEnt);
}

hEntity Engine::AddWorkplane(hGroup group, hEntity origin, hEntity normal) {
    ::SolveSpace::hRequest hr = DoAddRequest(
        Request::Type::WORKPLANE, IHG(group), EntityBase::FREE_IN_3D);

    ::SolveSpace::hEntity wpEnt = hr.entity(0);
    EntityBase *wp = SK.entity.FindByIdNoOops(wpEnt);
    if(wp) {
        EntityBase *srcOrig = SK.entity.FindByIdNoOops(IHE(origin));
        EntityBase *dstOrig = SK.entity.FindByIdNoOops(wp->point[0]);
        if(srcOrig && dstOrig) dstOrig->PointForceTo(srcOrig->PointGetNum());

        EntityBase *srcNorm = SK.entity.FindByIdNoOops(IHE(normal));
        EntityBase *dstNorm = SK.entity.FindByIdNoOops(wp->normal);
        if(srcNorm && dstNorm) dstNorm->NormalForceTo(srcNorm->NormalGetNum());
    }

    return AHE(wpEnt);
}

hEntity Engine::AddDistance(hGroup group, hEntity workplane, double d) {
    // Distances are sub-entities of circles/arcs. For standalone distance
    // params, create via parameter directly.
    // This creates a DATUM_POINT request and returns a synthetic distance.
    // In practice, distances are part of circle/arc entities.
    (void)group; (void)workplane; (void)d;
    hEntity r;
    r.v = 0;
    return r;
}

hEntity Engine::AddTTFText(hGroup group, hEntity workplane,
                            const std::string &text, const std::string &font,
                            Vec3 origin, double height) {
    Request r = {};
    r.type = Request::Type::TTF_TEXT;
    r.group = IHG(group);
    r.workplane = IHE(workplane);
    r.construction = false;
    r.str = text;
    r.font = font;
    SK.request.AddAndAssignId(&r);

    ::SolveSpace::hRequest hr = SK.request[SK.request.n - 1].h;

    Request *rp = SK.GetRequest(hr);
    EntityList el = {};
    ParamList pl = {};
    rp->Generate(&el, &pl);
    for(int i = 0; i < pl.n; i++) SK.param.Add(&pl[i]);
    for(int i = 0; i < el.n; i++) SK.entity.Add(&el[i]);
    el.Clear();
    pl.Clear();

    ::SolveSpace::hEntity he = hr.entity(0);
    EntityBase *e = SK.entity.FindByIdNoOops(he);
    if(e) {
        EntityBase *pt = SK.entity.FindByIdNoOops(e->point[0]);
        if(pt) pt->PointForceTo(ToInternal(origin));
    }

    return AHE(he);
}

void Engine::DeleteRequest(hRequest hr) {
    SK.request.RemoveById(IHR(hr));
}

void Engine::SetRequestConstruction(hRequest hr, bool construction) {
    Request *r = SK.request.FindByIdNoOops(IHR(hr));
    if(r) r->construction = construction;
}

hRequest Engine::GetEntityRequest(hEntity he) const {
    ::SolveSpace::hEntity ihe = IHE(he);
    if(ihe.isFromRequest()) {
        return AHR(ihe.request());
    }
    hRequest r;
    r.v = 0;
    return r;
}

bool Engine::EntityIsFromRequest(hEntity he) const {
    return IHE(he).isFromRequest();
}

// ========================================================================
//  Entity queries
// ========================================================================

int Engine::EntityCount() const {
    return SK.entity.n;
}

std::vector<hEntity> Engine::GetAllEntities() const {
    std::vector<hEntity> result;
    result.reserve(SK.entity.n);
    for(int i = 0; i < SK.entity.n; i++) {
        result.push_back(AHE(SK.entity[i].h));
    }
    return result;
}

std::vector<hEntity> Engine::GetEntitiesInGroup(hGroup hg) const {
    std::vector<hEntity> result;
    ::SolveSpace::hGroup ihg = IHG(hg);
    for(int i = 0; i < SK.entity.n; i++) {
        if(SK.entity[i].group.v == ihg.v) {
            result.push_back(AHE(SK.entity[i].h));
        }
    }
    return result;
}

EntityInfo Engine::GetEntityInfo(hEntity he) const {
    EntityInfo info = {};
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
    if(!e) return info;

    info.h = he;
    info.type = static_cast<EntityType>(static_cast<uint32_t>(e->type));
    info.group.v = e->group.v;
    info.workplane.v = e->workplane.v;
    info.construction = e->construction;
    info.visible = e->actVisible;
    info.actPoint = FromInternal(e->actPoint);
    info.actNormal = FromInternal(e->actNormal);
    info.actDistance = e->actDistance;

    // Sub-entities
    info.numPoints = 0;
    for(int i = 0; i < MAX_POINTS_IN_ENTITY && i < 4; i++) {
        if(e->point[i].v != 0) {
            info.point[i] = AHE(e->point[i]);
            info.numPoints = i + 1;
        }
    }
    info.normal = AHE(e->normal);
    info.distance = AHE(e->distance);

    return info;
}

Vec3 Engine::GetPointPosition(hEntity he) const {
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
    if(!e) return Vec3();
    return FromInternal(e->PointGetNum());
}

Quat Engine::GetNormalOrientation(hEntity he) const {
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
    if(!e) return Quat();
    return FromInternal(e->NormalGetNum());
}

double Engine::GetDistanceValue(hEntity he) const {
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
    if(!e) return 0.0;
    return e->DistanceGetNum();
}

void Engine::SetParamValue(hParam hp, double val) {
    Param *p = SK.param.FindByIdNoOops(IHP(hp));
    if(p) p->val = val;
}

double Engine::GetParamValue(hParam hp) const {
    Param *p = SK.param.FindByIdNoOops(IHP(hp));
    return p ? p->val : 0.0;
}

void Engine::SetPointPosition(hEntity he, Vec3 pos) {
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
    if(e) e->PointForceTo(ToInternal(pos));
}

void Engine::SetDistanceValue(hEntity he, double val) {
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
    if(e) e->DistanceForceTo(val);
}

void Engine::SetNormalOrientation(hEntity he, Quat q) {
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
    if(e) e->NormalForceTo(ToInternal(q));
}

// ========================================================================
//  Constraint management
// ========================================================================

hConstraint Engine::AddConstraint(ConstraintType type, hGroup group,
                                   hEntity workplane,
                                   double value,
                                   hEntity ptA, hEntity ptB,
                                   hEntity entityA, hEntity entityB) {
    ConstraintBase c = {};
    c.type = static_cast<ConstraintBase::Type>(static_cast<uint32_t>(type));
    c.group = IHG(group);
    c.workplane = IHE(workplane);
    c.valA = value;
    c.ptA = IHE(ptA);
    c.ptB = IHE(ptB);
    c.entityA = IHE(entityA);
    c.entityB = IHE(entityB);

    SK.constraint.AddAndAssignId(&c);

    return AHC(SK.constraint[SK.constraint.n - 1].h);
}

void Engine::RemoveConstraint(hConstraint hc) {
    SK.constraint.RemoveById(IHC(hc));
}

int Engine::ConstraintCount() const {
    return SK.constraint.n;
}

std::vector<hConstraint> Engine::GetAllConstraints() const {
    std::vector<hConstraint> result;
    result.reserve(SK.constraint.n);
    for(int i = 0; i < SK.constraint.n; i++) {
        result.push_back(AHC(SK.constraint[i].h));
    }
    return result;
}

std::vector<hConstraint> Engine::GetConstraintsInGroup(hGroup hg) const {
    std::vector<hConstraint> result;
    ::SolveSpace::hGroup ihg = IHG(hg);
    for(int i = 0; i < SK.constraint.n; i++) {
        if(SK.constraint[i].group.v == ihg.v) {
            result.push_back(AHC(SK.constraint[i].h));
        }
    }
    return result;
}

ConstraintInfo Engine::GetConstraintInfo(hConstraint hc) const {
    ConstraintInfo info = {};
    ConstraintBase *c = SK.constraint.FindByIdNoOops(IHC(hc));
    if(!c) return info;

    info.h = hc;
    info.type = static_cast<ConstraintType>(static_cast<uint32_t>(c->type));
    info.group.v = c->group.v;
    info.workplane.v = c->workplane.v;
    info.valA = c->valA;
    info.ptA.v = c->ptA.v;
    info.ptB.v = c->ptB.v;
    info.entityA.v = c->entityA.v;
    info.entityB.v = c->entityB.v;
    info.entityC.v = c->entityC.v;
    info.entityD.v = c->entityD.v;
    info.other = c->other;
    info.other2 = c->other2;
    info.reference = c->reference;
    info.comment = c->comment;
    info.disp = FromInternal(c->disp.offset);

    return info;
}

void Engine::SetConstraintValue(hConstraint hc, double val) {
    ConstraintBase *c = SK.constraint.FindByIdNoOops(IHC(hc));
    if(c) c->valA = val;
}

void Engine::SetConstraintReference(hConstraint hc, bool reference) {
    ConstraintBase *c = SK.constraint.FindByIdNoOops(IHC(hc));
    if(c) c->reference = reference;
}

void Engine::SetConstraintOther(hConstraint hc, bool other) {
    ConstraintBase *c = SK.constraint.FindByIdNoOops(IHC(hc));
    if(c) c->other = other;
}

void Engine::SetConstraintDisplayOffset(hConstraint hc, Vec3 offset) {
    ConstraintBase *c = SK.constraint.FindByIdNoOops(IHC(hc));
    if(c) c->disp.offset = ToInternal(offset);
}

// ========================================================================
//  Solving & Generation
// ========================================================================

void Engine::Regenerate() {
    SS.GenerateAll(SolveSpaceCore::Generate::ALL);
}

SolveStatus Engine::Solve() {
    SS.GenerateAll(SolveSpaceCore::Generate::ALL, true);

    SolveStatus status = {};
    status.result = SolveResult::OKAY;
    status.dof = 0;

    for(int i = 0; i < SK.group.n; i++) {
        Group &g = SK.group[i];
        if(g.h.v > SS.activeGroup.v) break;

        switch(g.solved.how) {
            case ::SolveSpace::SolveResult::OKAY:
                break;
            case ::SolveSpace::SolveResult::REDUNDANT_OKAY:
                if(status.result == SolveResult::OKAY)
                    status.result = SolveResult::REDUNDANT_OKAY;
                break;
            case ::SolveSpace::SolveResult::DIDNT_CONVERGE:
                status.result = SolveResult::DIDNT_CONVERGE;
                break;
            case ::SolveSpace::SolveResult::TOO_MANY_UNKNOWNS:
                status.result = SolveResult::TOO_MANY_UNKNOWNS;
                break;
            default:
                status.result = SolveResult::INCONSISTENT;
                break;
        }
        status.dof = g.solved.dof;
        for(int j = 0; j < g.solved.remove.n; j++) {
            status.failed.push_back(AHC(g.solved.remove[j]));
        }
    }
    return status;
}

SolveResult Engine::SolveGroup(hGroup hg) {
    SS.SolveGroup(IHG(hg), false);

    Group *g = SK.group.FindByIdNoOops(IHG(hg));
    if(!g) return SolveResult::INCONSISTENT;

    switch(g->solved.how) {
        case ::SolveSpace::SolveResult::OKAY:           return SolveResult::OKAY;
        case ::SolveSpace::SolveResult::REDUNDANT_OKAY: return SolveResult::REDUNDANT_OKAY;
        case ::SolveSpace::SolveResult::DIDNT_CONVERGE: return SolveResult::DIDNT_CONVERGE;
        case ::SolveSpace::SolveResult::TOO_MANY_UNKNOWNS: return SolveResult::TOO_MANY_UNKNOWNS;
        default: return SolveResult::INCONSISTENT;
    }
}

// ========================================================================
//  Geometry output
// ========================================================================

std::vector<Triangle> Engine::GetGroupMesh(hGroup hg) const {
    std::vector<Triangle> result;
    Group *g = SK.group.FindByIdNoOops(IHG(hg));
    if(!g) return result;

    SMesh *m = &g->displayMesh;
    result.reserve(m->l.n);
    for(int i = 0; i < m->l.n; i++) {
        STriangle *tr = &m->l[i];
        Triangle t;
        t.a = FromInternal(tr->a);
        t.b = FromInternal(tr->b);
        t.c = FromInternal(tr->c);
        t.normals[0] = FromInternal(tr->normals[0]);
        t.normals[1] = FromInternal(tr->normals[1]);
        t.normals[2] = FromInternal(tr->normals[2]);
        t.color = FromInternalColor(tr->meta.color);
        result.push_back(t);
    }
    return result;
}

std::vector<Triangle> Engine::GetMesh() const {
    return GetGroupMesh(GetActiveGroup());
}

std::vector<Edge> Engine::GetEdges() const {
    std::vector<Edge> result;
    SEdgeList el = {};
    for(int i = 0; i < SK.entity.n; i++) {
        EntityBase &e = SK.entity[i];
        if(e.group.v > SS.activeGroup.v) continue;
        if(e.construction) continue;
        e.GenerateEdges(&el);
    }
    result.reserve(el.l.n);
    for(int i = 0; i < el.l.n; i++) {
        Edge edge;
        edge.a = FromInternal(el.l[i].a);
        edge.b = FromInternal(el.l[i].b);
        result.push_back(edge);
    }
    el.Clear();
    return result;
}

std::vector<Edge> Engine::GetGroupEdges(hGroup hg) const {
    std::vector<Edge> result;
    ::SolveSpace::hGroup ihg = IHG(hg);
    SEdgeList el = {};
    for(int i = 0; i < SK.entity.n; i++) {
        EntityBase &e = SK.entity[i];
        if(e.group.v != ihg.v) continue;
        if(e.construction) continue;
        e.GenerateEdges(&el);
    }
    result.reserve(el.l.n);
    for(int i = 0; i < el.l.n; i++) {
        Edge edge;
        edge.a = FromInternal(el.l[i].a);
        edge.b = FromInternal(el.l[i].b);
        result.push_back(edge);
    }
    el.Clear();
    return result;
}

std::vector<BezierCurve> Engine::GetBezierCurves(hEntity he) const {
    std::vector<BezierCurve> result;
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
    if(!e) return result;

    SBezierList sbl = {};
    e->GenerateBezierCurves(&sbl);
    for(int i = 0; i < sbl.l.n; i++) {
        SBezier *sb = &sbl.l[i];
        BezierCurve bc;
        bc.degree = sb->deg;
        for(int j = 0; j <= sb->deg && j < 4; j++) {
            bc.ctrl[j] = FromInternal(sb->ctrl[j]);
            bc.weight[j] = sb->weight[j];
        }
        result.push_back(bc);
    }
    sbl.Clear();
    return result;
}

std::vector<Edge> Engine::GetGroupOutlines(hGroup hg) const {
    std::vector<Edge> result;
    Group *g = SK.group.FindByIdNoOops(IHG(hg));
    if(!g) return result;

    SOutlineList *ol = &g->displayOutlines;
    result.reserve(ol->l.n);
    for(int i = 0; i < ol->l.n; i++) {
        Edge edge;
        edge.a = FromInternal(ol->l[i].a);
        edge.b = FromInternal(ol->l[i].b);
        result.push_back(edge);
    }
    return result;
}

BoundingBox Engine::GetBoundingBox() const {
    BoundingBox bb;
    bool first = true;

    for(int i = 0; i < SK.entity.n; i++) {
        EntityBase &e = SK.entity[i];
        if(e.group.v > SS.activeGroup.v) continue;

        // Check point entities
        bool isPoint = (e.type == EntityBase::Type::POINT_IN_3D ||
                        e.type == EntityBase::Type::POINT_IN_2D ||
                        e.type == EntityBase::Type::POINT_N_COPY ||
                        e.type == EntityBase::Type::POINT_N_TRANS ||
                        e.type == EntityBase::Type::POINT_N_ROT_TRANS ||
                        e.type == EntityBase::Type::POINT_N_ROT_AA ||
                        e.type == EntityBase::Type::POINT_N_ROT_AXIS_TRANS);
        if(!isPoint) continue;

        Vector p = e.PointGetNum();
        if(first) {
            bb.minp = FromInternal(p);
            bb.maxp = FromInternal(p);
            first = false;
        } else {
            if(p.x < bb.minp.x) bb.minp.x = p.x;
            if(p.y < bb.minp.y) bb.minp.y = p.y;
            if(p.z < bb.minp.z) bb.minp.z = p.z;
            if(p.x > bb.maxp.x) bb.maxp.x = p.x;
            if(p.y > bb.maxp.y) bb.maxp.y = p.y;
            if(p.z > bb.maxp.z) bb.maxp.z = p.z;
        }
    }
    return bb;
}

// ========================================================================
//  Measurements
// ========================================================================

double Engine::GetVolume() const {
    return GetGroupVolume(GetActiveGroup());
}

double Engine::GetGroupVolume(hGroup hg) const {
    Group *g = SK.group.FindByIdNoOops(IHG(hg));
    if(!g) return 0.0;
    return g->displayMesh.CalculateVolume();
}

double Engine::GetSurfaceArea() const {
    Group *g = SK.group.FindByIdNoOops(SS.activeGroup);
    if(!g) return 0.0;
    std::vector<uint32_t> faces;
    return g->displayMesh.CalculateSurfaceArea(faces);
}

double Engine::GetEntityLength(hEntity he) const {
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
    if(!e) return 0.0;

    SEdgeList el = {};
    e->GenerateEdges(&el);
    double len = 0.0;
    for(int i = 0; i < el.l.n; i++) {
        len += el.l[i].b.Minus(el.l[i].a).Magnitude();
    }
    el.Clear();
    return len;
}

Vec3 Engine::GetCenterOfMass() const {
    Group *g = SK.group.FindByIdNoOops(SS.activeGroup);
    if(!g) return Vec3();
    return FromInternal(g->displayMesh.GetCenterOfMass());
}

// ========================================================================
//  Drag support
// ========================================================================

void Engine::BeginDrag(const std::vector<hEntity> &points) {
    SS.sys.dragged.clear();
    for(const auto &hp : points) {
        EntityBase *e = SK.entity.FindByIdNoOops(IHE(hp));
        if(!e) continue;

        switch(e->type) {
            case EntityBase::Type::POINT_IN_3D:
            case EntityBase::Type::POINT_N_TRANS:
            case EntityBase::Type::POINT_N_ROT_AXIS_TRANS:
                SS.sys.dragged.insert(e->param[0]);
                SS.sys.dragged.insert(e->param[1]);
                SS.sys.dragged.insert(e->param[2]);
                break;
            case EntityBase::Type::POINT_IN_2D:
                SS.sys.dragged.insert(e->param[0]);
                SS.sys.dragged.insert(e->param[1]);
                break;
            case EntityBase::Type::NORMAL_IN_3D:
                SS.sys.dragged.insert(e->param[0]);
                SS.sys.dragged.insert(e->param[1]);
                SS.sys.dragged.insert(e->param[2]);
                SS.sys.dragged.insert(e->param[3]);
                break;
            default:
                break;
        }
    }
}

void Engine::DragTo(const std::vector<hEntity> &points,
                     const std::vector<Vec3> &positions) {
    size_t n = std::min(points.size(), positions.size());
    for(size_t i = 0; i < n; i++) {
        EntityBase *e = SK.entity.FindByIdNoOops(IHE(points[i]));
        if(e) e->PointForceTo(ToInternal(positions[i]));
    }
    SS.GenerateAll(SolveSpaceCore::Generate::ALL);
}

void Engine::EndDrag() {
    SS.sys.dragged.clear();
}

// ========================================================================
//  Style management
// ========================================================================

hStyle Engine::CreateCustomStyle() {
    uint32_t vs = Style::CreateCustomStyle(false);
    hStyle r;
    r.v = vs;
    return r;
}

std::vector<hStyle> Engine::GetAllStyles() const {
    std::vector<hStyle> result;
    result.reserve(SK.style.n);
    for(int i = 0; i < SK.style.n; i++) {
        result.push_back(AHS(SK.style[i].h));
    }
    return result;
}

StyleInfo Engine::GetStyleInfo(hStyle hs) const {
    StyleInfo info = {};
    Style *s = Style::Get(IHS(hs));
    if(!s) return info;

    info.h = hs;
    info.name = s->name;
    info.width = s->width;
    info.widthAs = static_cast<UnitsAs>(static_cast<uint32_t>(s->widthAs));
    info.textHeight = s->textHeight;
    info.textHeightAs = static_cast<UnitsAs>(static_cast<uint32_t>(s->textHeightAs));
    info.color = FromInternalColor(s->color);
    info.fillColor = FromInternalColor(s->fillColor);
    info.filled = s->filled;
    info.visible = s->visible;
    info.exportable = s->exportable;
    info.stippleType = static_cast<StippleType>(static_cast<uint32_t>(s->stippleType));
    info.stippleScale = s->stippleScale;
    info.zIndex = s->zIndex;

    return info;
}

#define GET_STYLE_OR_RETURN(hs) \
    Style *s = SK.style.FindByIdNoOops(IHS(hs)); \
    if(!s) return;

void Engine::SetStyleName(hStyle hs, const std::string &name) {
    GET_STYLE_OR_RETURN(hs); s->name = name;
}
void Engine::SetStyleColor(hStyle hs, RgbaColor color) {
    GET_STYLE_OR_RETURN(hs); s->color = ToInternalColor(color);
}
void Engine::SetStyleFillColor(hStyle hs, RgbaColor color) {
    GET_STYLE_OR_RETURN(hs); s->fillColor = ToInternalColor(color);
}
void Engine::SetStyleFilled(hStyle hs, bool filled) {
    GET_STYLE_OR_RETURN(hs); s->filled = filled;
}
void Engine::SetStyleWidth(hStyle hs, double width, UnitsAs units) {
    GET_STYLE_OR_RETURN(hs);
    s->width = width;
    s->widthAs = static_cast<Style::UnitsAs>(static_cast<uint32_t>(units));
}
void Engine::SetStyleTextHeight(hStyle hs, double height, UnitsAs units) {
    GET_STYLE_OR_RETURN(hs);
    s->textHeight = height;
    s->textHeightAs = static_cast<Style::UnitsAs>(static_cast<uint32_t>(units));
}
void Engine::SetStyleStipple(hStyle hs, StippleType type, double scale) {
    GET_STYLE_OR_RETURN(hs);
    s->stippleType = static_cast<StipplePattern>(static_cast<uint32_t>(type));
    s->stippleScale = scale;
}
void Engine::SetStyleVisible(hStyle hs, bool visible) {
    GET_STYLE_OR_RETURN(hs); s->visible = visible;
}
void Engine::SetStyleExportable(hStyle hs, bool exportable) {
    GET_STYLE_OR_RETURN(hs); s->exportable = exportable;
}
void Engine::SetStyleZIndex(hStyle hs, int zIndex) {
    GET_STYLE_OR_RETURN(hs); s->zIndex = zIndex;
}

#undef GET_STYLE_OR_RETURN

void Engine::AssignStyleToEntity(hEntity he, hStyle hs) {
    ::SolveSpace::hEntity ihe = IHE(he);
    if(!ihe.isFromRequest()) return;

    Request *r = SK.request.FindByIdNoOops(ihe.request());
    if(r) r->style = IHS(hs);
}

RgbaColor Engine::GetEntityColor(hEntity he) const {
    ::SolveSpace::hStyle hs = Style::ForEntity(IHE(he));
    return FromInternalColor(Style::Color(hs));
}

// ========================================================================
//  Import / Export
// ========================================================================

bool Engine::ExportSTL(const std::string &path) const {
    Group *g = SK.group.FindByIdNoOops(SS.activeGroup);
    if(!g) return false;

    SMesh *sm = &g->displayMesh;
    if(sm->l.n == 0) return false;

    FILE *f = fopen(path.c_str(), "wb");
    if(!f) return false;

    // Binary STL: 80-byte header, 4-byte triangle count, then triangles
    char header[80] = {};
    snprintf(header, sizeof(header), "SolveSpace STL export");
    fwrite(header, 1, 80, f);

    uint32_t n = (uint32_t)sm->l.n;
    fwrite(&n, 4, 1, f);

    for(int i = 0; i < sm->l.n; i++) {
        STriangle *tr = &sm->l[i];
        Vector normal = tr->Normal().WithMagnitude(1.0);

        float buf[12];
        buf[0]  = (float)normal.x; buf[1]  = (float)normal.y; buf[2]  = (float)normal.z;
        buf[3]  = (float)tr->a.x;  buf[4]  = (float)tr->a.y;  buf[5]  = (float)tr->a.z;
        buf[6]  = (float)tr->b.x;  buf[7]  = (float)tr->b.y;  buf[8]  = (float)tr->b.z;
        buf[9]  = (float)tr->c.x;  buf[10] = (float)tr->c.y;  buf[11] = (float)tr->c.z;
        fwrite(buf, 4, 12, f);

        uint16_t attr = 0;
        fwrite(&attr, 2, 1, f);
    }
    fclose(f);
    return true;
}

bool Engine::ExportOBJ(const std::string &objPath,
                        const std::string &mtlPath) const {
    Group *g = SK.group.FindByIdNoOops(SS.activeGroup);
    if(!g) return false;

    SMesh *sm = &g->displayMesh;
    if(sm->l.n == 0) return false;

    FILE *fObj = fopen(objPath.c_str(), "w");
    if(!fObj) return false;

    fprintf(fObj, "# SolveSpace OBJ export\n");
    if(!mtlPath.empty()) {
        fprintf(fObj, "mtllib %s\n", mtlPath.c_str());
    }

    // Write vertices and faces
    for(int i = 0; i < sm->l.n; i++) {
        STriangle *tr = &sm->l[i];
        fprintf(fObj, "v %.6f %.6f %.6f\n", tr->a.x, tr->a.y, tr->a.z);
        fprintf(fObj, "v %.6f %.6f %.6f\n", tr->b.x, tr->b.y, tr->b.z);
        fprintf(fObj, "v %.6f %.6f %.6f\n", tr->c.x, tr->c.y, tr->c.z);
    }
    for(int i = 0; i < sm->l.n; i++) {
        int base = i * 3 + 1;
        fprintf(fObj, "f %d %d %d\n", base, base + 1, base + 2);
    }
    fclose(fObj);

    // Write material library if path provided
    if(!mtlPath.empty()) {
        FILE *fMtl = fopen(mtlPath.c_str(), "w");
        if(fMtl) {
            fprintf(fMtl, "# SolveSpace MTL export\n");
            fprintf(fMtl, "newmtl default\n");
            fprintf(fMtl, "Kd 0.6 0.6 0.6\n");
            fclose(fMtl);
        }
    }

    return true;
}

bool Engine::ExportSTEP(const std::string &path) const {
    StepFileWriter sfw = {};
    sfw.ExportSurfacesTo(Platform::Path::From(path));
    return true;
}

bool Engine::ImportDXF(const std::string &path, hGroup group) {
    impl->EnsureInit();
    // Set active group so ImportDxf creates entities in the right group
    ::SolveSpace::hGroup prevActive = SS.activeGroup;
    SS.activeGroup = IHG(group);
    Platform::Path p = Platform::Path::From(path);
    ImportDxf(p);
    SS.activeGroup = prevActive;
    SS.GenerateAll(SolveSpaceCore::Generate::ALL);
    return true;
}

// ========================================================================
//  Internal helpers for higher-level operations
// ========================================================================

// Replace all references to oldpt with newpt in constraints
static void ApiReplacePointInConstraints(::SolveSpace::hEntity oldpt,
                                          ::SolveSpace::hEntity newpt) {
    for(int i = 0; i < SK.constraint.n; i++) {
        if(SK.constraint[i].ptA == oldpt) SK.constraint[i].ptA = newpt;
        if(SK.constraint[i].ptB == oldpt) SK.constraint[i].ptB = newpt;
    }
}

// Add a coincident constraint between two points in the active group/workplane
static void ApiConstrainCoincident(::SolveSpace::hEntity ptA,
                                    ::SolveSpace::hEntity ptB,
                                    ::SolveSpace::hGroup group,
                                    ::SolveSpace::hEntity workplane) {
    ConstraintBase c = {};
    c.type = ConstraintBase::Type::POINTS_COINCIDENT;
    c.group = group;
    c.workplane = workplane;
    c.ptA = ptA;
    c.ptB = ptB;
    SK.constraint.AddAndAssignId(&c);
}

// Add a constraint with full params
static void ApiAddConstraintInternal(ConstraintBase::Type type,
                                      ::SolveSpace::hGroup group,
                                      ::SolveSpace::hEntity workplane,
                                      ::SolveSpace::hEntity ptA,
                                      ::SolveSpace::hEntity ptB,
                                      ::SolveSpace::hEntity entityA,
                                      ::SolveSpace::hEntity entityB,
                                      bool other, bool other2) {
    ConstraintBase c = {};
    c.type = type;
    c.group = group;
    c.workplane = workplane;
    c.ptA = ptA;
    c.ptB = ptB;
    c.entityA = entityA;
    c.entityB = entityB;
    c.other = other;
    c.other2 = other2;
    SK.constraint.AddAndAssignId(&c);
}

// Split a line segment at a point, returning the handle of the new split point
static ::SolveSpace::hEntity ApiSplitLine(::SolveSpace::hEntity he, Vector pinter,
                                           ::SolveSpace::hGroup group,
                                           ::SolveSpace::hEntity workplane) {
    EntityBase *e01 = SK.GetEntity(he);
    ::SolveSpace::hEntity hep0 = e01->point[0], hep1 = e01->point[1];
    Vector p0 = SK.GetEntity(hep0)->PointGetNum();
    Vector p1 = SK.GetEntity(hep1)->PointGetNum();

    if(p0.Equals(pinter)) return hep0;
    if(p1.Equals(pinter)) return hep1;

    // Create two new line segment requests
    ::SolveSpace::hRequest r0i = DoAddRequest(Request::Type::LINE_SEGMENT, group, workplane);
    ::SolveSpace::hRequest ri1 = DoAddRequest(Request::Type::LINE_SEGMENT, group, workplane);

    EntityBase *e0i = SK.GetEntity(r0i.entity(0));
    EntityBase *ei1 = SK.GetEntity(ri1.entity(0));

    SK.GetEntity(e0i->point[0])->PointForceTo(p0);
    SK.GetEntity(e0i->point[1])->PointForceTo(pinter);
    SK.GetEntity(ei1->point[0])->PointForceTo(pinter);
    SK.GetEntity(ei1->point[1])->PointForceTo(p1);

    ApiReplacePointInConstraints(hep0, e0i->point[0]);
    ApiReplacePointInConstraints(hep1, ei1->point[1]);
    ApiConstrainCoincident(e0i->point[1], ei1->point[0], group, workplane);

    return e0i->point[1];
}

// Split a circle or arc at a point
static ::SolveSpace::hEntity ApiSplitCircle(::SolveSpace::hEntity he, Vector pinter,
                                             ::SolveSpace::hGroup group,
                                             ::SolveSpace::hEntity workplane) {
    EntityBase *circle = SK.GetEntity(he);
    if(circle->type == EntityBase::Type::CIRCLE) {
        Vector center = SK.GetEntity(circle->point[0])->PointGetNum();

        ::SolveSpace::hRequest hr = DoAddRequest(Request::Type::ARC_OF_CIRCLE, group, workplane);
        EntityBase *arc = SK.GetEntity(hr.entity(0));

        SK.GetEntity(arc->point[0])->PointForceTo(center);
        SK.GetEntity(arc->point[1])->PointForceTo(pinter);
        SK.GetEntity(arc->point[2])->PointForceTo(pinter);

        ApiConstrainCoincident(arc->point[1], arc->point[2], group, workplane);
        return arc->point[1];
    } else {
        // Arc: split into two arcs
        ::SolveSpace::hEntity hc = circle->point[0];
        ::SolveSpace::hEntity hs = circle->point[1];
        ::SolveSpace::hEntity hf = circle->point[2];
        Vector center = SK.GetEntity(hc)->PointGetNum();
        Vector start  = SK.GetEntity(hs)->PointGetNum();
        Vector finish = SK.GetEntity(hf)->PointGetNum();

        ::SolveSpace::hRequest hr0 = DoAddRequest(Request::Type::ARC_OF_CIRCLE, group, workplane);
        ::SolveSpace::hRequest hr1 = DoAddRequest(Request::Type::ARC_OF_CIRCLE, group, workplane);

        EntityBase *arc0 = SK.GetEntity(hr0.entity(0));
        EntityBase *arc1 = SK.GetEntity(hr1.entity(0));

        SK.GetEntity(arc0->point[0])->PointForceTo(center);
        SK.GetEntity(arc0->point[1])->PointForceTo(start);
        SK.GetEntity(arc0->point[2])->PointForceTo(pinter);

        SK.GetEntity(arc1->point[0])->PointForceTo(center);
        SK.GetEntity(arc1->point[1])->PointForceTo(pinter);
        SK.GetEntity(arc1->point[2])->PointForceTo(finish);

        ApiReplacePointInConstraints(hs, arc0->point[1]);
        ApiReplacePointInConstraints(hf, arc1->point[2]);
        ApiConstrainCoincident(arc0->point[2], arc1->point[1], group, workplane);

        return arc0->point[2];
    }
}

// Split a cubic spline at a point
static ::SolveSpace::hEntity ApiSplitCubic(::SolveSpace::hEntity he, Vector pinter,
                                            ::SolveSpace::hGroup group,
                                            ::SolveSpace::hEntity workplane) {
    EntityBase *e01 = SK.GetEntity(he);
    SBezierList sbl = {};
    e01->GenerateBezierCurves(&sbl);

    ::SolveSpace::hEntity hep0 = e01->point[0];
    ::SolveSpace::hEntity hep1 = e01->point[3 + e01->extraPoints];
    ::SolveSpace::hEntity hep0n = EntityBase::NO_ENTITY;
    ::SolveSpace::hEntity hep1n = EntityBase::NO_ENTITY;
    ::SolveSpace::hEntity hepin = EntityBase::NO_ENTITY;

    for(int i = 0; i < sbl.l.n; i++) {
        SBezier *sb = &(sbl.l[i]);
        if(sb->deg != 3) continue;

        double t;
        sb->ClosestPointTo(pinter, &t, false);
        if(pinter.Equals(sb->PointAt(t))) {
            SBezier b0i, bi1, b01 = *sb;
            b01.SplitAt(t, &b0i, &bi1);

            ::SolveSpace::hRequest r0i = DoAddRequest(Request::Type::CUBIC, group, workplane);
            ::SolveSpace::hRequest ri1 = DoAddRequest(Request::Type::CUBIC, group, workplane);

            EntityBase *e0i = SK.GetEntity(r0i.entity(0));
            EntityBase *ei1 = SK.GetEntity(ri1.entity(0));

            for(int j = 0; j <= 3; j++) {
                SK.GetEntity(e0i->point[j])->PointForceTo(b0i.ctrl[j]);
            }
            for(int j = 0; j <= 3; j++) {
                SK.GetEntity(ei1->point[j])->PointForceTo(bi1.ctrl[j]);
            }

            ApiConstrainCoincident(e0i->point[3], ei1->point[0], group, workplane);
            if(i == 0) hep0n = e0i->point[0];
            hep1n = ei1->point[3];
            hepin = e0i->point[3];
        } else {
            ::SolveSpace::hRequest r = DoAddRequest(Request::Type::CUBIC, group, workplane);
            EntityBase *e = SK.GetEntity(r.entity(0));
            for(int j = 0; j <= 3; j++) {
                SK.GetEntity(e->point[j])->PointForceTo(sb->ctrl[j]);
            }
            if(i == 0) hep0n = e->point[0];
            hep1n = e->point[3];
        }
    }
    sbl.Clear();

    if(hep0n.v) ApiReplacePointInConstraints(hep0, hep0n);
    if(hep1n.v) ApiReplacePointInConstraints(hep1, hep1n);
    return hepin;
}

// ========================================================================
//  Parametric curve helper (line or arc) for tangent arc computation
// ========================================================================

struct ParamCurve {
    bool isLine;
    Vector p0, p1;
    Vector u, v;
    double r, theta0, theta1, dtheta;

    void MakeFromEntity(::SolveSpace::hEntity he, bool reverse) {
        *this = {};
        EntityBase *e = SK.GetEntity(he);
        if(e->type == EntityBase::Type::LINE_SEGMENT) {
            isLine = true;
            p0 = e->EndpointStart();
            p1 = e->EndpointFinish();
            if(reverse) std::swap(p0, p1);
        } else if(e->type == EntityBase::Type::ARC_OF_CIRCLE) {
            isLine = false;
            p0 = SK.GetEntity(e->point[0])->PointGetNum();
            Vector pe = SK.GetEntity(e->point[1])->PointGetNum();
            r = pe.Minus(p0).Magnitude();
            e->ArcGetAngles(&theta0, &theta1, &dtheta);
            if(reverse) { std::swap(theta0, theta1); dtheta = -dtheta; }
            EntityBase *wrkpln = SK.GetEntity(e->workplane)->Normal();
            u = wrkpln->NormalU();
            v = wrkpln->NormalV();
        }
    }

    Vector PointAt(double t) const {
        if(isLine) return p0.Plus(p1.Minus(p0).ScaledBy(t));
        double th = theta0 + dtheta * t;
        return p0.Plus(u.ScaledBy(r*cos(th)).Plus(v.ScaledBy(r*sin(th))));
    }

    Vector TangentAt(double t) const {
        if(isLine) return p1.Minus(p0);
        double th = theta0 + dtheta * t;
        Vector tang = u.ScaledBy(-r*sin(th)).Plus(v.ScaledBy(r*cos(th)));
        return tang.ScaledBy(dtheta);
    }

    double LengthForAuto() const {
        if(isLine) return p1.Minus(p0).Magnitude() / 3;
        return fabs(dtheta) * r / 20;
    }
};

// ========================================================================
//  Higher-level sketch operations
// ========================================================================

std::vector<hEntity> Engine::SplitEntityAt(hEntity entity, Vec3 splitPoint) {
    std::vector<hEntity> result;

    EntityBase *e = SK.entity.FindByIdNoOops(IHE(entity));
    if(!e) return result;

    ::SolveSpace::hEntity ihe = IHE(entity);
    ::SolveSpace::hGroup grp = e->group;
    ::SolveSpace::hEntity wp = e->workplane;
    Vector pinter = ToInternal(splitPoint);

    ::SolveSpace::hEntity splitPt = EntityBase::NO_ENTITY;

    if(e->type == EntityBase::Type::LINE_SEGMENT) {
        splitPt = ApiSplitLine(ihe, pinter, grp, wp);
    } else if(e->IsCircle()) {
        splitPt = ApiSplitCircle(ihe, pinter, grp, wp);
    } else if(e->type == EntityBase::Type::CUBIC ||
              e->type == EntityBase::Type::CUBIC_PERIODIC) {
        splitPt = ApiSplitCubic(ihe, pinter, grp, wp);
    } else {
        return result;
    }

    // Delete the original request
    if(ihe.isFromRequest()) {
        ::SolveSpace::hRequest hr = ihe.request();
        if(splitPt.v && hr.v != splitPt.request().v) {
            Request *req = SK.request.FindByIdNoOops(hr);
            if(req && req->group == grp && !req->construction) {
                SK.request.RemoveById(hr);
            }
        }
    }

    if(splitPt.v) result.push_back(AHE(splitPt));
    return result;
}

hEntity Engine::MakeTangentArc(hEntity curveA, hEntity curveB,
                                hEntity sharedPoint) {
    EntityBase *ea = SK.entity.FindByIdNoOops(IHE(curveA));
    EntityBase *eb = SK.entity.FindByIdNoOops(IHE(curveB));
    EntityBase *ep = SK.entity.FindByIdNoOops(IHE(sharedPoint));
    if(!ea || !eb || !ep) { hEntity r; r.v = 0; return r; }

    Vector pshared = ep->PointGetNum();
    ::SolveSpace::hGroup grp = ea->group;
    ::SolveSpace::hEntity wp = ea->workplane;

    // Determine which endpoint of each curve is at the shared point
    bool pointf[2];
    pointf[0] = ea->EndpointFinish().Equals(pshared);
    pointf[1] = eb->EndpointFinish().Equals(pshared);

    // Get workplane normal
    EntityBase *wrkpln = SK.entity.FindByIdNoOops(wp);
    if(!wrkpln) { hEntity r; r.v = 0; return r; }
    EntityBase *wrkplnNorm = SK.entity.FindByIdNoOops(wrkpln->normal);
    if(!wrkplnNorm) { hEntity r; r.v = 0; return r; }
    Vector wn = wrkplnNorm->NormalN();

    // Build parametric curves
    ParamCurve pc[2];
    pc[0].MakeFromEntity(IHE(curveA), pointf[0]);
    pc[1].MakeFromEntity(IHE(curveB), pointf[1]);

    // Newton iteration to find tangent arc
    double t[2] = {0, 0}, tp[2];
    Vector pinter;
    double arcR = 0.0, vv = 0.0;
    int iters = 1000;

    for(int i = 0; i < iters + 20; i++) {
        Vector p0 = pc[0].PointAt(t[0]);
        Vector p1 = pc[1].PointAt(t[1]);
        Vector t0 = pc[0].TangentAt(t[0]);
        Vector t1 = pc[1].TangentAt(t[1]);

        pinter = Vector::AtIntersectionOfLines(p0, p0.Plus(t0),
                                                p1, p1.Plus(t1),
                                                NULL, NULL, NULL);

        Vector vc = wn.Cross(t0).WithMagnitude(1);
        vv = t1.Dot(vc);

        double dot = t0.WithMagnitude(1).Dot(t1.WithMagnitude(1));
        double theta = acos(std::max(-1.0, std::min(1.0, dot)));
        if(theta < 1e-6) break;

        // Auto-radius: fit within the curves
        arcR = pc[0].LengthForAuto() * tan(theta / 2);
        arcR = std::min(arcR, pc[1].LengthForAuto() * tan(theta / 2));

        // Ramp radius for convergence
        if(i < iters) {
            arcR *= 0.1 + 0.9 * i / (double)iters;
        }

        double el = arcR / tan(theta / 2);
        Vector pa0 = pinter.Plus(t0.WithMagnitude(el));
        Vector pa1 = pinter.Plus(t1.WithMagnitude(el));

        tp[0] = t[0]; tp[1] = t[1];
        t[0] += pa0.Minus(p0).DivProjected(t0);
        t[1] += pa1.Minus(p1).DivProjected(t1);
    }

    // Check convergence
    if(fabs(tp[0] - t[0]) > 1e-3 || fabs(tp[1] - t[1]) > 1e-3 ||
       t[0] < 0.01 || t[1] < 0.01 || t[0] > 0.99 || t[1] > 0.99) {
        hEntity r; r.v = 0; return r;
    }

    // Compute arc center
    Vector center = pc[0].PointAt(t[0]);
    Vector v0inter = pinter.Minus(center);
    int a, b;
    if(vv < 0) {
        a = 1; b = 2;
        center = center.Minus(v0inter.Cross(wn).WithMagnitude(arcR));
    } else {
        a = 2; b = 1;
        center = center.Plus(v0inter.Cross(wn).WithMagnitude(arcR));
    }

    // Mark original entities as construction
    ::SolveSpace::hEntity iheA = IHE(curveA);
    ::SolveSpace::hEntity iheB = IHE(curveB);
    if(iheA.isFromRequest()) {
        Request *rr = SK.request.FindByIdNoOops(iheA.request());
        if(rr) rr->construction = true;
    }
    if(iheB.isFromRequest()) {
        Request *rr = SK.request.FindByIdNoOops(iheB.request());
        if(rr) rr->construction = true;
    }

    // Create the tangent arc
    ::SolveSpace::hRequest harc = DoAddRequest(Request::Type::ARC_OF_CIRCLE, grp, wp);
    EntityBase *earc = SK.GetEntity(harc.entity(0));

    SK.GetEntity(earc->point[0])->PointForceTo(center);
    SK.GetEntity(earc->point[a])->PointForceTo(pc[0].PointAt(t[0]));
    SK.GetEntity(earc->point[b])->PointForceTo(pc[1].PointAt(t[1]));

    // Add tangency constraints
    if(pc[0].isLine) {
        ApiAddConstraintInternal(ConstraintBase::Type::ARC_LINE_TANGENT,
            grp, wp, EntityBase::NO_ENTITY, EntityBase::NO_ENTITY,
            harc.entity(0), IHE(curveA), (b == 1), false);
    } else {
        ApiAddConstraintInternal(ConstraintBase::Type::CURVE_CURVE_TANGENT,
            grp, wp, EntityBase::NO_ENTITY, EntityBase::NO_ENTITY,
            harc.entity(0), IHE(curveA), (b == 1), (pc[0].dtheta < 0));
    }
    if(pc[1].isLine) {
        ApiAddConstraintInternal(ConstraintBase::Type::ARC_LINE_TANGENT,
            grp, wp, EntityBase::NO_ENTITY, EntityBase::NO_ENTITY,
            harc.entity(0), IHE(curveB), (a == 1), false);
    } else {
        ApiAddConstraintInternal(ConstraintBase::Type::CURVE_CURVE_TANGENT,
            grp, wp, EntityBase::NO_ENTITY, EntityBase::NO_ENTITY,
            harc.entity(0), IHE(curveB), (a == 1), (pc[1].dtheta < 0));
    }

    return AHE(harc.entity(0));
}

// ========================================================================
//  Clipboard
// ========================================================================

// Internal clipboard entry
struct ClipEntry {
    Request::Type type;
    int           extraPoints;
    ::SolveSpace::hStyle style;
    std::string   str;
    std::string   font;
    bool          construction;
    Vector        point[MAX_POINTS_IN_ENTITY];
    double        distance;
    int           numPoints;
    bool          hasDistance;
    ::SolveSpace::hEntity oldEnt;
    ::SolveSpace::hEntity oldPointEnt[MAX_POINTS_IN_ENTITY];
    ::SolveSpace::hRequest newReq;
};

struct ClipConstraint {
    ConstraintBase c;
};

// Store clipboard data in a static to persist across calls
static std::vector<ClipEntry> sClipboard;
static std::vector<ClipConstraint> sClipConstraints;
// The workplane coordinate system used when copying
static Vector sClipU, sClipV, sClipN, sClipOrigin;

static bool ClipContainsEntity(::SolveSpace::hEntity he) {
    if(he == EntityBase::NO_ENTITY) return true;
    for(auto &cr : sClipboard) {
        if(cr.oldEnt == he) return true;
        for(int i = 0; i < cr.numPoints; i++) {
            if(cr.oldPointEnt[i] == he) return true;
        }
    }
    return false;
}

static ::SolveSpace::hEntity ClipNewEntityFor(::SolveSpace::hEntity he) {
    if(he == EntityBase::NO_ENTITY) return EntityBase::NO_ENTITY;
    for(auto &cr : sClipboard) {
        if(cr.oldEnt == he) return cr.newReq.entity(0);
        for(int i = 0; i < cr.numPoints; i++) {
            if(cr.oldPointEnt[i] == he)
                return cr.newReq.entity(1 + i);
        }
    }
    return EntityBase::NO_ENTITY;
}

void Engine::CopyToClipboard(const std::vector<hEntity> &entities) {
    sClipboard.clear();
    sClipConstraints.clear();

    // Determine active workplane coordinate system
    // Use the workplane of the first entity, or XY plane as fallback
    ::SolveSpace::hEntity wpEnt = EntityBase::FREE_IN_3D;
    for(auto &he : entities) {
        EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
        if(e && e->workplane.v != EntityBase::FREE_IN_3D.v) {
            wpEnt = e->workplane;
            break;
        }
    }

    if(wpEnt.v != EntityBase::FREE_IN_3D.v) {
        EntityBase *wp = SK.GetEntity(wpEnt);
        EntityBase *wn = SK.GetEntity(wp->normal);
        sClipU = wn->NormalU();
        sClipV = wn->NormalV();
        sClipN = wn->NormalN();
        sClipOrigin = SK.GetEntity(wp->point[0])->PointGetNum();
    } else {
        sClipU = Vector::From(1, 0, 0);
        sClipV = Vector::From(0, 1, 0);
        sClipN = Vector::From(0, 0, 1);
        sClipOrigin = Vector::From(0, 0, 0);
    }

    for(auto &he : entities) {
        EntityBase *e = SK.entity.FindByIdNoOops(IHE(he));
        if(!e) continue;

        Request::Type reqType;
        int pts;
        bool hasNormal, hasDistance;
        if(!EntReqTable::GetEntityInfo(e->type, e->extraPoints,
               &reqType, &pts, &hasNormal, &hasDistance)) {
            if(!e->h.isFromRequest()) continue;
            Request *r = SK.GetRequest(e->h.request());
            if(r->type != Request::Type::DATUM_POINT) continue;
            EntReqTable::GetEntityInfo((EntityBase::Type)0, e->extraPoints,
                &reqType, &pts, &hasNormal, &hasDistance);
        }
        if(reqType == Request::Type::WORKPLANE) continue;

        ClipEntry cr = {};
        cr.type = reqType;
        cr.extraPoints = e->extraPoints;
        cr.style = e->style;
        cr.str = e->str;
        cr.font = e->font;
        cr.construction = e->construction;
        cr.numPoints = pts;
        cr.hasDistance = hasDistance;

        for(int i = 0; i < pts; i++) {
            Vector pt;
            if(reqType == Request::Type::DATUM_POINT) {
                pt = e->PointGetNum();
            } else {
                pt = SK.GetEntity(e->point[i])->PointGetNum();
            }
            pt = pt.Minus(sClipOrigin);
            pt = pt.DotInToCsys(sClipU, sClipV, sClipN);
            cr.point[i] = pt;
        }
        if(hasDistance) {
            cr.distance = SK.GetEntity(e->distance)->DistanceGetNum();
        }

        cr.oldEnt = e->h;
        for(int i = 0; i < pts; i++) {
            cr.oldPointEnt[i] = e->point[i];
        }

        sClipboard.push_back(cr);
    }

    // Copy constraints that reference only clipboard entities
    for(int i = 0; i < SK.constraint.n; i++) {
        ConstraintBase &c = SK.constraint[i];
        if(!ClipContainsEntity(c.ptA) || !ClipContainsEntity(c.ptB) ||
           !ClipContainsEntity(c.entityA) || !ClipContainsEntity(c.entityB) ||
           !ClipContainsEntity(c.entityC) || !ClipContainsEntity(c.entityD)) {
            continue;
        }
        if(c.type == ConstraintBase::Type::COMMENT) continue;
        ClipConstraint cc;
        cc.c = c;
        sClipConstraints.push_back(cc);
    }
}

std::vector<hEntity> Engine::PasteFromClipboard(Vec3 offset) {
    return PasteTransformed(offset, Quat(1, 0, 0, 0), 1.0, false);
}

std::vector<hEntity> Engine::PasteTransformed(Vec3 offset, Quat rotation,
                                               double scale, bool mirror) {
    std::vector<hEntity> result;
    if(sClipboard.empty()) return result;

    double effectiveScale = mirror ? -fabs(scale) : fabs(scale);
    Vector trans = ToInternal(offset);

    // Compute rotation angle around workplane normal from quaternion
    // (simplified: use the angle of rotation projected onto workplane)
    double theta = 0;
    if(rotation.x != 0 || rotation.y != 0 || rotation.z != 0) {
        double halfAngle = acos(std::max(-1.0, std::min(1.0, rotation.w)));
        theta = 2.0 * halfAngle;
    }

    for(auto &cr : sClipboard) {
        ::SolveSpace::hRequest hr = DoAddRequest(cr.type, SS.activeGroup,
            EntityBase::FREE_IN_3D);
        Request *r = SK.request.FindByIdNoOops(hr);
        if(!r) continue;

        r->extraPoints = cr.extraPoints;
        r->style = cr.style;
        r->str = cr.str;
        r->font = cr.font;
        r->construction = cr.construction;

        int pts = cr.numPoints;
        for(int i = 0; i < pts; i++) {
            Vector pt = cr.point[i];
            if(effectiveScale < 0) pt.x *= -1;
            pt = pt.ScaledBy(fabs(effectiveScale));
            pt = pt.ScaleOutOfCsys(sClipU, sClipV, Vector::From(0, 0, 0));
            pt = pt.Plus(sClipOrigin);
            pt = pt.RotatedAbout(sClipN, theta);
            pt = pt.Plus(trans);

            int j = (cr.type == Request::Type::DATUM_POINT) ? i : i + 1;
            SK.GetEntity(hr.entity(j))->PointForceTo(pt);
        }
        if(cr.hasDistance) {
            SK.GetEntity(hr.entity(64))->DistanceForceTo(
                cr.distance * fabs(effectiveScale));
        }

        cr.newReq = hr;
        result.push_back(AHE(hr.entity(0)));
    }

    // Paste constraints with remapped entity handles
    for(auto &cc : sClipConstraints) {
        ConstraintBase c = {};
        c.group = SS.activeGroup;
        c.workplane = cc.c.workplane;
        c.type = cc.c.type;
        c.valA = cc.c.valA;
        c.ptA = ClipNewEntityFor(cc.c.ptA);
        c.ptB = ClipNewEntityFor(cc.c.ptB);
        c.entityA = ClipNewEntityFor(cc.c.entityA);
        c.entityB = ClipNewEntityFor(cc.c.entityB);
        c.entityC = ClipNewEntityFor(cc.c.entityC);
        c.entityD = ClipNewEntityFor(cc.c.entityD);
        c.other = cc.c.other;
        c.other2 = cc.c.other2;
        c.reference = cc.c.reference;
        c.disp = cc.c.disp;
        c.comment = cc.c.comment;

        // Adjust values for transforms
        switch(c.type) {
            case ConstraintBase::Type::PT_PT_DISTANCE:
            case ConstraintBase::Type::PROJ_PT_DISTANCE:
            case ConstraintBase::Type::DIAMETER:
                c.valA *= fabs(effectiveScale);
                break;
            case ConstraintBase::Type::PT_LINE_DISTANCE:
                c.valA *= effectiveScale;
                break;
            case ConstraintBase::Type::ARC_LINE_TANGENT:
                if(effectiveScale < 0) c.other = !c.other;
                break;
            case ConstraintBase::Type::CURVE_CURVE_TANGENT:
                if(effectiveScale < 0) {
                    EntityBase *eA = SK.entity.FindByIdNoOops(c.entityA);
                    EntityBase *eB = SK.entity.FindByIdNoOops(c.entityB);
                    if(eA && eA->type == EntityBase::Type::ARC_OF_CIRCLE)
                        c.other = !c.other;
                    if(eB && eB->type == EntityBase::Type::ARC_OF_CIRCLE)
                        c.other2 = !c.other2;
                }
                break;
            case ConstraintBase::Type::HORIZONTAL:
            case ConstraintBase::Type::VERTICAL:
                if(fabs(fmod(theta + (M_PI/2), M_PI)) < 1e-6) {
                    c.type = (c.type == ConstraintBase::Type::HORIZONTAL)
                        ? ConstraintBase::Type::VERTICAL
                        : ConstraintBase::Type::HORIZONTAL;
                }
                break;
            default:
                break;
        }

        SK.constraint.AddAndAssignId(&c);
    }

    return result;
}

// ========================================================================
//  Configuration
// ========================================================================

void Engine::SetUnit(Unit u) {
    SS.viewUnits = static_cast<::SolveSpace::Unit>(static_cast<int>(u));
}

Unit Engine::GetUnit() const {
    return static_cast<Unit>(static_cast<int>(SS.viewUnits));
}

void Engine::SetChordTolerance(double mm) {
    SS.chordTol = mm;
    SS.chordTolCalculated = mm;
}

double Engine::GetChordTolerance() const {
    return SS.chordTolCalculated;
}

void Engine::SetMaxSegments(int n) {
    SS.maxSegments = n;
}

int Engine::GetMaxSegments() const {
    return SS.maxSegments;
}

void Engine::SetExportChordTolerance(double mm) {
    SS.exportChordTol = mm;
}

void Engine::SetExportMaxSegments(int n) {
    SS.exportMaxSegments = n;
}

void Engine::SetExportScale(double s) {
    SS.exportScale = s;
}

void Engine::SetDecimalPlaces(int mm, int inch, int degree) {
    SS.afterDecimalMm = mm;
    SS.afterDecimalInch = inch;
    SS.afterDecimalDegree = degree;
}

// ========================================================================
//  Workplane helpers
// ========================================================================

hEntity Engine::GetXYWorkplane() const {
    return AHE(Request::HREQUEST_REFERENCE_XY.entity(0));
}

hEntity Engine::GetYZWorkplane() const {
    return AHE(Request::HREQUEST_REFERENCE_YZ.entity(0));
}

hEntity Engine::GetZXWorkplane() const {
    return AHE(Request::HREQUEST_REFERENCE_ZX.entity(0));
}

hEntity Engine::GetWorkplaneOrigin(hEntity workplane) const {
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(workplane));
    if(!e) { hEntity r; r.v = 0; return r; }
    return AHE(e->point[0]);
}

hEntity Engine::GetWorkplaneNormal(hEntity workplane) const {
    EntityBase *e = SK.entity.FindByIdNoOops(IHE(workplane));
    if(!e) { hEntity r; r.v = 0; return r; }
    return AHE(e->normal);
}

void Engine::ProjectOntoWorkplane(hEntity workplane, Vec3 p3d,
                                   double *u, double *v) const {
    EntityBase *wp = SK.entity.FindByIdNoOops(IHE(workplane));
    if(!wp) { *u = 0; *v = 0; return; }

    EntityBase *wn = SK.entity.FindByIdNoOops(wp->normal);
    if(!wn) { *u = 0; *v = 0; return; }

    Quaternion q = wn->NormalGetNum();
    Vector orig = SK.GetEntity(wp->point[0])->PointGetNum();
    Vector delta = ToInternal(p3d).Minus(orig);

    *u = delta.Dot(q.RotationU());
    *v = delta.Dot(q.RotationV());
}

} // namespace Api
} // namespace SolveSpace
