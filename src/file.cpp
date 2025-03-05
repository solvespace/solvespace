//-----------------------------------------------------------------------------
// Routines to write and read our .slvs file format.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "solvespace.h"

#define VERSION_STRING "\261\262\263" "SolveSpaceREVa"

static int StrStartsWith(const char *str, const char *start) {
    return memcmp(str, start, strlen(start)) == 0;
}

//-----------------------------------------------------------------------------
// Clear and free all the dynamic memory associated with our currently-loaded
// sketch. This does not leave the program in an acceptable state (with the
// references created, and so on), so anyone calling this must fix that later.
//-----------------------------------------------------------------------------
void SolveSpaceUI::ClearExisting() {
    UndoClearStack(&redo);
    UndoClearStack(&undo);

    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);
        g->Clear();
    }

    SK.constraint.Clear();
    SK.request.Clear();
    SK.group.Clear();
    SK.groupOrder.Clear();
    SK.style.Clear();

    SK.entity.Clear();
    SK.param.Clear();
    images.clear();
}

hGroup SolveSpaceUI::CreateDefaultDrawingGroup() {
    Group g = {};

    // And an empty group, for the first stuff the user draws.
    g.visible = true;
    g.name = C_("group-name", "sketch-in-plane");
    g.type = Group::Type::DRAWING_WORKPLANE;
    g.subtype = Group::Subtype::WORKPLANE_BY_POINT_ORTHO;
    g.order = 1;
    g.predef.q = Quaternion::From(1, 0, 0, 0);
    hRequest hr = Request::HREQUEST_REFERENCE_XY;
    g.predef.origin = hr.entity(1);
    SK.group.AddAndAssignId(&g);
    SK.GetGroup(g.h)->activeWorkplane = g.h.entity(0);
    return g.h;
}

void SolveSpaceUI::NewFile() {
    ClearExisting();

    // Our initial group, that contains the references.
    Group g = {};
    g.visible = true;
    g.name = C_("group-name", "#references");
    g.type = Group::Type::DRAWING_3D;
    g.order = 0;
    g.h = Group::HGROUP_REFERENCES;
    SK.group.Add(&g);

    // Let's create three two-d coordinate systems, for the coordinate
    // planes; these are our references, present in every sketch.
    Request r = {};
    r.type = Request::Type::WORKPLANE;
    r.group = Group::HGROUP_REFERENCES;
    r.workplane = Entity::FREE_IN_3D;

    r.h = Request::HREQUEST_REFERENCE_XY;
    SK.request.Add(&r);

    r.h = Request::HREQUEST_REFERENCE_YZ;
    SK.request.Add(&r);

    r.h = Request::HREQUEST_REFERENCE_ZX;
    SK.request.Add(&r);

    CreateDefaultDrawingGroup();
}

template<class T, bool>
struct UnderlyingTypeHelper {
    using type = T;
};

template<class T>
struct UnderlyingTypeHelper<T, true> {
    using type = typename std::underlying_type<T>::type;
};

template<class T>
using UnderlyingType = typename UnderlyingTypeHelper<T, std::is_enum<T>::value>::type;

struct StrEqual {
    bool operator()(const char *a, const char *b) const {
        return strcmp(a, b) == 0;
    }
};
struct StrHash {
    uint64_t operator()(const char *v) const {
        // Use FNV hash as the hashing function
        constexpr uint64_t prime{0x100000001B3};
        uint64_t res = 0xcbf29ce484222325;
        for(; *v; ++v) {
            res = (res * prime) ^ *v;
        }
        return res;
    }
};
using SaveTable = std::unordered_map<const char *, SolveSpaceUI::SaveDesc, StrHash, StrEqual>;

// Ideally we should just use `offsetof(type, member)` and save that in `SaveDesc`, but
// since it doesn't work with types that are not standard layout, we have to go through
// this lambda and store it as a function pointer instead
#define MEMBER(type, member) \
    [](const void *obj) { return &static_cast<type *>(const_cast<void *>(obj))->member; }

template<class F>
static constexpr SolveSpaceUI::SaveDesc BoolFormatter(F getter) {
    using ReturnType = typename std::remove_pointer<decltype(getter(nullptr))>::type;
    static_assert(std::is_same<bool, ReturnType>::value, "Only booleans can be bool-formatted");
    return {'b', (void *(*)(const void *)) static_cast<ReturnType *(*)(const void *)>(getter)};
}

template<class F>
static constexpr SolveSpaceUI::SaveDesc IntFormatter(F getter) {
    using ReturnType = typename std::remove_pointer<decltype(getter(nullptr))>::type;
    static_assert(std::is_integral<UnderlyingType<ReturnType>>::value &&
                      sizeof(ReturnType) == sizeof(int),
                  "Only integral types can be int-formatted");
    return {std::is_signed<UnderlyingType<ReturnType>>::value ? 'd' : 'u',
            (void *(*)(const void *)) static_cast<ReturnType *(*)(const void *)>(getter)};
}

template<class F>
static constexpr SolveSpaceUI::SaveDesc HexFormatter(F getter) {
    using ReturnType = typename std::remove_pointer<decltype(getter(nullptr))>::type;
    static_assert(std::is_unsigned<UnderlyingType<ReturnType>>::value &&
                      sizeof(ReturnType) == sizeof(uint32_t),
                  "Only unsigned integral types can be hex-formatted");
    return {'x', (void *(*)(const void *)) static_cast<ReturnType *(*)(const void *)>(getter)};
}

template<class F>
static constexpr SolveSpaceUI::SaveDesc FloatFormatter(F getter) {
    using ReturnType = typename std::remove_pointer<decltype(getter(nullptr))>::type;
    static_assert(std::is_same<double, ReturnType>::value, "Only doubles can be float-formatted");
    return {'f', (void *(*)(const void *)) static_cast<ReturnType *(*)(const void *)>(getter)};
}

template<class F>
static constexpr SolveSpaceUI::SaveDesc ColorFormatter(F getter) {
    using ReturnType = typename std::remove_pointer<decltype(getter(nullptr))>::type;
    static_assert(std::is_same<RgbaColor, ReturnType>::value,
                  "Only RgbaColor can be color-formatted");
    return {'c', (void *(*)(const void *)) static_cast<ReturnType *(*)(const void *)>(getter)};
}

template<class F>
static constexpr SolveSpaceUI::SaveDesc StrFormatter(F getter) {
    using ReturnType = typename std::remove_pointer<decltype(getter(nullptr))>::type;
    static_assert(std::is_same<std::string, ReturnType>::value,
                  "Only std::string can be string-formatted");
    return {'S', (void *(*)(const void *)) static_cast<ReturnType *(*)(const void *)>(getter)};
}

template<class F>
static constexpr SolveSpaceUI::SaveDesc MapFormatter(F getter) {
    using ReturnType = typename std::remove_pointer<decltype(getter(nullptr))>::type;
    static_assert(std::is_same<EntityMap, ReturnType>::value,
                  "Only EntityMap can be map-formatted");
    return {'M', (void *(*)(const void *)) static_cast<ReturnType *(*)(const void *)>(getter)};
}

template<class F>
static constexpr SolveSpaceUI::SaveDesc PathFormatter(F getter) {
    using ReturnType = typename std::remove_pointer<decltype(getter(nullptr))>::type;
    static_assert(std::is_same<Platform::Path, ReturnType>::value,
                  "Only Platform::Path can be path-formatted");
    return {'P', (void *(*)(const void *)) static_cast<ReturnType *(*)(const void *)>(getter)};
}

static constexpr SolveSpaceUI::SaveDesc IgnoredFormatter() {
    return {'i', nullptr};
}

static const SaveTable::value_type SAVED_GROUP[] = {
    {"h.v", HexFormatter(MEMBER(Group, h.v))},
    {"type", IntFormatter(MEMBER(Group, type))},
    {"order", IntFormatter(MEMBER(Group, order))},
    {"name", StrFormatter(MEMBER(Group, name))},
    {"activeWorkplane.v", HexFormatter(MEMBER(Group, activeWorkplane.v))},
    {"opA.v", HexFormatter(MEMBER(Group, opA.v))},
    {"opB.v", HexFormatter(MEMBER(Group, opB.v))},
    {"valA", FloatFormatter(MEMBER(Group, valA))},
    {"valB", FloatFormatter(MEMBER(Group, valB))},
    {"valC", FloatFormatter(MEMBER(Group, valB))},
    {"color", ColorFormatter(MEMBER(Group, color))},
    {"subtype", IntFormatter(MEMBER(Group, subtype))},
    {"skipFirst", BoolFormatter(MEMBER(Group, skipFirst))},
    {"meshCombine", IntFormatter(MEMBER(Group, meshCombine))},
    {"forceToMesh", BoolFormatter(MEMBER(Group, forceToMesh))},
    {"predef.q.w", FloatFormatter(MEMBER(Group, predef.q.w))},
    {"predef.q.vx", FloatFormatter(MEMBER(Group, predef.q.vx))},
    {"predef.q.vy", FloatFormatter(MEMBER(Group, predef.q.vy))},
    {"predef.q.vz", FloatFormatter(MEMBER(Group, predef.q.vz))},
    {"predef.origin.v", HexFormatter(MEMBER(Group, predef.origin.v))},
    {"predef.entityB.v", HexFormatter(MEMBER(Group, predef.entityB.v))},
    {"predef.entityC.v", HexFormatter(MEMBER(Group, predef.entityC.v))},
    {"predef.swapUV", BoolFormatter(MEMBER(Group, predef.swapUV))},
    {"predef.negateU", BoolFormatter(MEMBER(Group, predef.negateU))},
    {"predef.negateV", BoolFormatter(MEMBER(Group, predef.negateV))},
    {"visible", BoolFormatter(MEMBER(Group, visible))},
    {"suppress", BoolFormatter(MEMBER(Group, suppress))},
    {"relaxConstraints", BoolFormatter(MEMBER(Group, relaxConstraints))},
    {"allowRedundant", BoolFormatter(MEMBER(Group, allowRedundant))},
    {"allDimsReference", BoolFormatter(MEMBER(Group, allDimsReference))},
    {"scale", FloatFormatter(MEMBER(Group, scale))},
    {"remap", MapFormatter(MEMBER(Group, remap))},
    {"impFile", IgnoredFormatter()},
    {"impFileRel", PathFormatter(MEMBER(Group, linkFile))},
};

static const SaveTable::value_type SAVED_PARAM[] = {
    {"h.v.", HexFormatter(MEMBER(Param, h.v))},
    {"val", FloatFormatter(MEMBER(Param, val))},
};

static const SaveTable::value_type SAVED_REQUEST[] = {
    {"h.v", HexFormatter(MEMBER(Request, h.v))},
    {"type", IntFormatter(MEMBER(Request, type))},
    {"extraPoints", IntFormatter(MEMBER(Request, extraPoints))},
    {"workplane.v", HexFormatter(MEMBER(Request, workplane.v))},
    {"group.v", HexFormatter(MEMBER(Request, group.v))},
    {"construction", BoolFormatter(MEMBER(Request, construction))},
    {"style", HexFormatter(MEMBER(Request, style.v))},
    {"str", StrFormatter(MEMBER(Request, str))},
    {"font", StrFormatter(MEMBER(Request, font))},
    {"file", PathFormatter(MEMBER(Request, file))},
    {"aspectRatio", FloatFormatter(MEMBER(Request, aspectRatio))},
};

static const SaveTable::value_type SAVED_ENTITY[] = {
    {"h.v", HexFormatter(MEMBER(Entity, h.v))},
    {"type", IntFormatter(MEMBER(Entity, type))},
    {"construction", BoolFormatter(MEMBER(Entity, construction))},
    {"style", HexFormatter(MEMBER(Entity, style.v))},
    {"str", StrFormatter(MEMBER(Entity, str))},
    {"font", StrFormatter(MEMBER(Entity, font))},
    {"file", PathFormatter(MEMBER(Entity, file))},
    {"point[0].v", HexFormatter(MEMBER(Entity, point[0].v))},
    {"point[1].v", HexFormatter(MEMBER(Entity, point[1].v))},
    {"point[2].v", HexFormatter(MEMBER(Entity, point[2].v))},
    {"point[3].v", HexFormatter(MEMBER(Entity, point[3].v))},
    {"point[4].v", HexFormatter(MEMBER(Entity, point[4].v))},
    {"point[5].v", HexFormatter(MEMBER(Entity, point[5].v))},
    {"point[6].v", HexFormatter(MEMBER(Entity, point[6].v))},
    {"point[7].v", HexFormatter(MEMBER(Entity, point[7].v))},
    {"point[8].v", HexFormatter(MEMBER(Entity, point[8].v))},
    {"point[9].v", HexFormatter(MEMBER(Entity, point[9].v))},
    {"point[10].v", HexFormatter(MEMBER(Entity, point[10].v))},
    {"point[11].v", HexFormatter(MEMBER(Entity, point[11].v))},
    {"extraPoints", IntFormatter(MEMBER(Entity, extraPoints))},
    {"normal.v", HexFormatter(MEMBER(Entity, normal.v))},
    {"distance.v", HexFormatter(MEMBER(Entity, distance.v))},
    {"workplane.v", HexFormatter(MEMBER(Entity, workplane.v))},
    {"actPoint.x", FloatFormatter(MEMBER(Entity, actPoint.x))},
    {"actPoint.y", FloatFormatter(MEMBER(Entity, actPoint.y))},
    {"actPoint.z", FloatFormatter(MEMBER(Entity, actPoint.z))},
    {"actNormal.w", FloatFormatter(MEMBER(Entity, actNormal.w))},
    {"actNormal.vx", FloatFormatter(MEMBER(Entity, actNormal.vx))},
    {"actNormal.vy", FloatFormatter(MEMBER(Entity, actNormal.vy))},
    {"actNormal.vz", FloatFormatter(MEMBER(Entity, actNormal.vz))},
    {"actDistance", FloatFormatter(MEMBER(Entity, actDistance))},
    {"actVisible", BoolFormatter(MEMBER(Entity, actVisible))},
};

static const SaveTable::value_type SAVED_CONSTRAINT[] = {
    {"h.v", HexFormatter(MEMBER(Constraint, h.v))},
    {"type", IntFormatter(MEMBER(Constraint, type))},
    {"group.v", HexFormatter(MEMBER(Constraint, group.v))},
    {"workplane.v", HexFormatter(MEMBER(Constraint, workplane.v))},
    {"valA", FloatFormatter(MEMBER(Constraint, valA))},
    {"valP.v", HexFormatter(MEMBER(Constraint, valP.v))},
    {"ptA.v", HexFormatter(MEMBER(Constraint, ptA.v))},
    {"ptB.v", HexFormatter(MEMBER(Constraint, ptB.v))},
    {"entityA.v", HexFormatter(MEMBER(Constraint, entityA.v))},
    {"entityB.v", HexFormatter(MEMBER(Constraint, entityB.v))},
    {"entityC.v", HexFormatter(MEMBER(Constraint, entityC.v))},
    {"entityD.v", HexFormatter(MEMBER(Constraint, entityD.v))},
    {"other", BoolFormatter(MEMBER(Constraint, other))},
    {"other2", BoolFormatter(MEMBER(Constraint, other2))},
    {"reference", BoolFormatter(MEMBER(Constraint, reference))},
    {"comment", StrFormatter(MEMBER(Constraint, comment))},
    {"disp.offset.x", FloatFormatter(MEMBER(Constraint, disp.offset.x))},
    {"disp.offset.y", FloatFormatter(MEMBER(Constraint, disp.offset.y))},
    {"disp.offset.z", FloatFormatter(MEMBER(Constraint, disp.offset.z))},
    {"disp.style", HexFormatter(MEMBER(Constraint, disp.style.v))},
};

static const SaveTable::value_type SAVED_STYLE[] = {
    {"h.v", HexFormatter(MEMBER(Style, h.v))},
    {"name", StrFormatter(MEMBER(Style, name))},
    {"width", FloatFormatter(MEMBER(Style, width))},
    {"widthAs", IntFormatter(MEMBER(Style, widthAs))},
    {"textHeight", FloatFormatter(MEMBER(Style, textHeight))},
    {"textHeightAs", IntFormatter(MEMBER(Style, textHeightAs))},
    {"textAngle", FloatFormatter(MEMBER(Style, textAngle))},
    {"textOrigin", HexFormatter(MEMBER(Style, textOrigin))},
    {"color", ColorFormatter(MEMBER(Style, color))},
    {"fillColor", ColorFormatter(MEMBER(Style, fillColor))},
    {"filled", BoolFormatter(MEMBER(Style, filled))},
    {"visible", BoolFormatter(MEMBER(Style, visible))},
    {"exportable", BoolFormatter(MEMBER(Style, exportable))},
    {"stippleType", IntFormatter(MEMBER(Style, stippleType))},
    {"stippleScale", FloatFormatter(MEMBER(Style, stippleScale))},
};

using SaveTableMap =
    std::unordered_map<std::string, std::pair<std::vector<const char *>, SaveTable>>;

template<size_t N>
SaveTableMap::value_type save_table(const char *name, const SaveTable::value_type (&t)[N]) {
    SaveTable m;
    std::vector<const char *> k;

    m.reserve(N);
    k.reserve(N);

    for(const auto &kv : t) {
        m[kv.first] = kv.second;
        k.push_back(kv.first);
    }

    return {name, {std::move(k), std::move(m)}};
}

static const SaveTableMap SAVED_TYPES = {
    {save_table("Group", SAVED_GROUP)},           {save_table("Param", SAVED_PARAM)},
    {save_table("Request", SAVED_REQUEST)},       {save_table("Entity", SAVED_ENTITY)},
    {save_table("Constraint", SAVED_CONSTRAINT)}, {save_table("Style", SAVED_STYLE)},
};

void *SolveSpaceUI::LoadUnion::Get(char tname) {
    switch(tname) {
    case 'G': return &g;
    case 'P': return &p;
    case 'R': return &r;
    case 'E': return &e;
    case 'C': return &c;
    case 'S': return &s;
    default: ssassert(false, "Invalid type name");
    }
}

const SolveSpaceUI::SaveDesc *SolveSpaceUI::GetSaveDesc(const char *key) {
    const char *prefix_end = strchr(key, '.');
    if(prefix_end == nullptr) {
        return nullptr;
    }

    std::string prefix(key, prefix_end);
    auto table = SAVED_TYPES.find(prefix);
    if(table == SAVED_TYPES.end()) {
        return nullptr;
    }

    const auto &desc_table = table->second.second;
    auto desc              = desc_table.find(prefix_end + 1);
    if(desc == desc_table.end()) {
        return nullptr;
    }

    return &desc->second;
}

struct SAVEDptr {
    EntityMap      &M() { return *((EntityMap *)this); }
    std::string    &S() { return *((std::string *)this); }
    Platform::Path &P() { return *((Platform::Path *)this); }
    const EntityMap      &M() const { return *((const EntityMap *)this); }
    const std::string    &S() const { return *((const std::string *)this); }
    const Platform::Path &P() const { return *((const Platform::Path *)this); }
    bool      &b() { return *((bool *)this); }
    RgbaColor &c() { return *((RgbaColor *)this); }
    int       &d() { return *((int *)this); }
    unsigned  &u() { return *((unsigned *)this); }
    double    &f() { return *((double *)this); }
    uint32_t  &x() { return *((uint32_t *)this); }
    const bool      &b() const { return *((const bool *)this); }
    const RgbaColor &c() const { return *((const RgbaColor *)this); }
    const int       &d() const { return *((const int *)this); }
    const unsigned  &u() const { return *((const unsigned *)this); }
    const double    &f() const { return *((const double *)this); }
    const uint32_t  &x() const { return *((const uint32_t *)this); }
};

static inline SAVEDptr *SavedPtr(const void *obj, const SolveSpaceUI::SaveDesc &table) {
    return reinterpret_cast<SAVEDptr *>(table.member(obj));
}

void SolveSpaceUI::SaveUsingTable(const Platform::Path &filename, const char *tname,
                                  const void *obj) {
    const auto &table = SAVED_TYPES.at(tname);
    for(const auto &key : table.first) {
        const auto &desc = table.second.at(key);

        int fmt = desc.fmt;
        if(desc.member == nullptr)
            continue;

        const SAVEDptr *p = SavedPtr(obj, desc);
        // Any items that aren't specified are assumed to be zero
        if(fmt == 'S' && p->S().empty())          continue;
        if(fmt == 'P' && p->P().IsEmpty())        continue;
        if(fmt == 'd' && p->d() == 0)             continue;
        if(fmt == 'u' && p->u() == 0)             continue;
        if(fmt == 'f' && EXACT(p->f() == 0.0))    continue;
        if(fmt == 'x' && p->x() == 0)             continue;

        fprintf(fh, "%s.%s=", tname, key);
        switch(fmt) {
            case 'S': fprintf(fh, "%s",    p->S().c_str());       break;
            case 'b': fprintf(fh, "%d",    p->b() ? 1 : 0);       break;
            case 'c': fprintf(fh, "%08x",  p->c().ToPackedInt()); break;
            case 'd': fprintf(fh, "%d",    p->d());               break;
            case 'u': fprintf(fh, "%u",    p->u());               break;
            case 'f': fprintf(fh, "%.20f", p->f());               break;
            case 'x': fprintf(fh, "%08x",  p->x());               break;

            case 'P': {
                if(!p->P().IsEmpty()) {
                    Platform::Path relativePath = p->P().Expand(/*fromCurrentDirectory=*/true).RelativeTo(filename.Expand(/*fromCurrentDirectory=*/true).Parent());
                    ssassert(!relativePath.IsEmpty(), "Cannot relativize path");
                    fprintf(fh, "%s", relativePath.ToPortable().c_str());
                }
                break;
            }

            case 'M': {
                fprintf(fh, "{\n");
                // Sort the mapping, since EntityMap is not deterministic.
                std::vector<std::pair<EntityKey, EntityId>> sorted(p->M().begin(), p->M().end());
                std::sort(sorted.begin(), sorted.end(),
                    [](std::pair<EntityKey, EntityId> &a, std::pair<EntityKey, EntityId> &b) {
                        return a.second.v < b.second.v;
                    });
                for(const auto &it : sorted) {
                    fprintf(fh, "    %d %08x %d\n",
                            it.second.v, it.first.input.v, it.first.copyNumber);
                }
                fprintf(fh, "}");
                break;
            }

            default: ssassert(false, "Unexpected value format");
        }
        fprintf(fh, "\n");
    }
}

bool SolveSpaceUI::SaveToFile(const Platform::Path &filename) {
    // Make sure all the entities are regenerated up to date, since they will be exported.
    SS.ScheduleShowTW();
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);

    for(Group &g : SK.group) {
        if(g.type != Group::Type::LINKED) continue;

        // Expand for "filename" below is needed on Linux when the file was opened with a relative
        // path on the command line. dialog->RunModal() in SolveSpaceUI::GetFilenameAndSave will
        // convert the file name to full path on Windows but not on GTK.
        if(g.linkFile.Expand(/*fromCurrentDirectory=*/true)
               .RelativeTo(filename.Expand(/*fromCurrentDirectory=*/true))
               .IsEmpty()) {
            Error("This sketch links the sketch '%s'; it can only be saved "
                  "on the same volume.", g.linkFile.raw.c_str());
            return false;
        }
    }

    fh = OpenFile(filename, "wb");
    if(!fh) {
        Error("Couldn't write to file '%s'", filename.raw.c_str());
        return false;
    }

    fprintf(fh, "%s\n\n\n", VERSION_STRING);

    int i, j;
    for(auto &g : SK.group) {
        SaveUsingTable(filename, "Group", &g);
        fprintf(fh, "AddGroup\n\n");
    }

    for(auto &p : SK.param) {
        SaveUsingTable(filename, "Param", &p);
        fprintf(fh, "AddParam\n\n");
    }

    for(auto &r : SK.request) {
        SaveUsingTable(filename, "Request", &r);
        fprintf(fh, "AddRequest\n\n");
    }

    for(auto &e : SK.entity) {
        e.CalculateNumerical(/*forExport=*/true);
        SaveUsingTable(filename, "Entity", &e);
        fprintf(fh, "AddEntity\n\n");
    }

    for(auto &c : SK.constraint) {
        SaveUsingTable(filename, "Constraint", &c);
        fprintf(fh, "AddConstraint\n\n");
    }

    for(auto &s : SK.style) {
        if(s.h.v >= Style::FIRST_CUSTOM) {
            SaveUsingTable(filename, "Style", &s);
            fprintf(fh, "AddStyle\n\n");
        }
    }

    // A group will have either a mesh or a shell, but not both; but the code
    // to print either of those just does nothing if the mesh/shell is empty.

    Group *g = SK.GetGroup(*SK.groupOrder.Last());
    for(const STriangle &tr : g->runningMesh.l) {
        fprintf(fh, "Triangle %08x %08x "
                "%.20f %.20f %.20f  %.20f %.20f %.20f  %.20f %.20f %.20f\n",
            tr.meta.face, tr.meta.color.ToPackedInt(),
            CO(tr.a), CO(tr.b), CO(tr.c));
    }

    SShell *s = &g->runningShell;
    for(const SSurface &srf : s->surface) {
        fprintf(fh, "Surface %08x %08x %08x %d %d\n",
            srf.h.v, srf.color.ToPackedInt(), srf.face, srf.degm, srf.degn);
        for(i = 0; i <= srf.degm; i++) {
            for(j = 0; j <= srf.degn; j++) {
                fprintf(fh, "SCtrl %d %d %.20f %.20f %.20f Weight %20.20f\n",
                    i, j, CO(srf.ctrl[i][j]), srf.weight[i][j]);
            }
        }

        for(const STrimBy &stb : srf.trim) {
            fprintf(fh, "TrimBy %08x %d %.20f %.20f %.20f  %.20f %.20f %.20f\n",
                stb.curve.v, stb.backwards ? 1 : 0,
                CO(stb.start), CO(stb.finish));
        }

        fprintf(fh, "AddSurface\n");
    }
    for(const SCurve &sc : s->curve) {
        fprintf(fh, "Curve %08x %d %d %08x %08x\n",
            sc.h.v,
            sc.isExact ? 1 : 0, sc.exact.deg,
            sc.surfA.v, sc.surfB.v);

        if(sc.isExact) {
            for(i = 0; i <= sc.exact.deg; i++) {
                fprintf(fh, "CCtrl %d %.20f %.20f %.20f Weight %.20f\n",
                    i, CO(sc.exact.ctrl[i]), sc.exact.weight[i]);
            }
        }
        for(const SCurvePt &scpt : sc.pts) {
            fprintf(fh, "CurvePt %d %.20f %.20f %.20f\n",
                scpt.vertex ? 1 : 0, CO(scpt.p));
        }

        fprintf(fh, "AddCurve\n");
    }

    fclose(fh);

    return true;
}

void SolveSpaceUI::LoadUsingTable(const Platform::Path &filename, const char *key, const char *val,
                                  SolveSpaceUI::LoadUnion &obj) {
    const SaveDesc *desc = GetSaveDesc(key);
    if(desc == nullptr) {
        fileLoadError = true;
        return;
    }

    // Ignored members should be ignored
    if(desc->member == nullptr) {
        return;
    }

    SAVEDptr *p    = SavedPtr(obj.Get(key[0]), *desc);
    unsigned int u = 0;
    switch(desc->fmt) {
        case 'S': p->S() = val;                     break;
        case 'b': p->b() = (atoi(val) != 0);        break;
        case 'd': p->d() = atoi(val);               break;
        case 'u': sscanf(val, "%u", &u); p->u()=u;  break;
        case 'f': p->f() = atof(val);               break;
        case 'x': sscanf(val, "%x", &u); p->x()= u; break;

        case 'P': {
            Platform::Path path = Platform::Path::FromPortable(val);
            if(!path.IsEmpty()) {
                p->P() = filename.Parent().Join(path).Expand();
            }
            break;
        }

        case 'c':
            sscanf(val, "%x", &u);
            p->c() = RgbaColor::FromPackedInt(u);
            break;

        case 'M': {
            p->M().clear();
            for(;;) {
                EntityKey ek;
                EntityId ei;
                char line2[1024];
                if (fgets(line2, (int)sizeof(line2), fh) == NULL)
                    break;
                if(sscanf(line2, "%d %x %d", &(ei.v), &(ek.input.v),
                                             &(ek.copyNumber)) == 3) {
                    if(ei.v == Entity::NO_ENTITY.v) {
                        // Commit bd84bc1a mistakenly introduced code that would remap
                        // some entities to NO_ENTITY. This was fixed in commit bd84bc1a,
                        // but files created meanwhile are corrupt, and can cause crashes.
                        //
                        // To fix this, we skip any such remaps when loading; they will be
                        // recreated on the next regeneration. Any resulting orphans will
                        // be pruned in the usual way, recovering to a well-defined state.
                        continue;
                    }
                    p->M().insert({ ek, ei });
                } else {
                    break;
                }
            }
            break;
        }

        default: ssassert(false, "Unexpected value format");
    }
}

bool SolveSpaceUI::LoadFromFile(const Platform::Path &filename, bool canCancel) {
    bool fileIsEmpty = true;
    allConsistent = false;
    fileLoadError = false;

    fh = OpenFile(filename, "rb");
    if(!fh) {
        Error("Couldn't read from file '%s'", filename.raw.c_str());
        return false;
    }

    ClearExisting();

    LoadUnion sv = {};
    sv.g.scale = 1; // default is 1, not 0; so legacy files need this
    Style::FillDefaultStyle(&sv.s);

    char line[1024];
    while(fgets(line, (int)sizeof(line), fh)) {
        fileIsEmpty = false;

        char *s = strchr(line, '\n');
        if(s) *s = '\0';
        // We should never get files with \r characters in them, but mailers
        // will sometimes mangle attachments.
        s = strchr(line, '\r');
        if(s) *s = '\0';

        if(*line == '\0') continue;

        char *e = strchr(line, '=');
        if(e) {
            *e = '\0';
            const char *key = line, *val = e+1;
            LoadUsingTable(filename, key, val, sv);
        } else if(strcmp(line, "AddGroup")==0) {
            // legacy files have a spurious dependency between linked groups
            // and their parent groups, remove
            if(sv.g.type == Group::Type::LINKED)
                sv.g.opA.v = 0;

            SK.group.Add(&(sv.g));
            sv.g = {};
            sv.g.scale = 1; // default is 1, not 0; so legacy files need this
        } else if(strcmp(line, "AddParam")==0) {
            // params are regenerated, but we want to preload the values
            // for initial guesses
            SK.param.Add(&(sv.p));
            sv.p = {};
        } else if(strcmp(line, "AddEntity")==0) {
            // entities are regenerated
        } else if(strcmp(line, "AddRequest")==0) {
            SK.request.Add(&(sv.r));
            sv.r = {};
        } else if(strcmp(line, "AddConstraint")==0) {
            SK.constraint.Add(&(sv.c));
            sv.c = {};
        } else if(strcmp(line, "AddStyle")==0) {
            SK.style.Add(&(sv.s));
            sv.s = {};
            Style::FillDefaultStyle(&sv.s);
        } else if(strcmp(line, VERSION_STRING)==0) {
            // do nothing, version string
        } else if(StrStartsWith(line, "Triangle ")      ||
                  StrStartsWith(line, "Surface ")       ||
                  StrStartsWith(line, "SCtrl ")         ||
                  StrStartsWith(line, "TrimBy ")        ||
                  StrStartsWith(line, "Curve ")         ||
                  StrStartsWith(line, "CCtrl ")         ||
                  StrStartsWith(line, "CurvePt ")       ||
                  strcmp(line, "AddSurface")==0         ||
                  strcmp(line, "AddCurve")==0)
        {
            // ignore the mesh or shell, since we regenerate that
        } else {
            fileLoadError = true;
        }
    }

    fclose(fh);

    if(fileIsEmpty) {
        Error(_("The file is empty. It may be corrupt."));
        NewFile();
    }

    if(fileLoadError) {
        Error(_("Unrecognized data in file. This file may be corrupt, or "
                "from a newer version of the program."));
        // At least leave the program in a non-crashing state.
        if(SK.group.IsEmpty()) {
            NewFile();
        }
    }
    if(!ReloadAllLinked(filename, canCancel)) {
        return false;
    }
    UpgradeLegacyData();

    return true;
}

void SolveSpaceUI::UpgradeLegacyData() {
    for(Request &r : SK.request) {
        switch(r.type) {
            // TTF text requests saved in versions prior to 3.0 only have two
            // reference points (origin and origin plus v); version 3.0 adds two
            // more points, and if we don't do anything, then they will appear
            // at workplane origin, and the solver will mess up the sketch if
            // it is not fully constrained.
            case Request::Type::TTF_TEXT: {
                IdList<Entity,hEntity> entity = {};
                IdList<Param,hParam>   param = {};
                r.Generate(&entity, &param);

                // If we didn't load all of the entities and params that this
                // request would generate, then add them now, so that we can
                // force them to their appropriate positions.
                for(Param &p : param) {
                    if(SK.param.FindByIdNoOops(p.h) != NULL) continue;
                    SK.param.Add(&p);
                }
                bool allPointsExist = true;
                for(Entity &e : entity) {
                    if(SK.entity.FindByIdNoOops(e.h) != NULL) continue;
                    SK.entity.Add(&e);
                    allPointsExist = false;
                }

                if(!allPointsExist) {
                    Entity *text = entity.FindById(r.h.entity(0));
                    Entity *b = entity.FindById(text->point[2]);
                    Entity *c = entity.FindById(text->point[3]);
                    ExprVector bex, cex;
                    text->RectGetPointsExprs(&bex, &cex);
                    b->PointForceParamTo(bex.Eval());
                    c->PointForceParamTo(cex.Eval());
                }
                entity.Clear();
                param.Clear();
                break;
            }

            default:
                break;
        }
    }

    // Constraints saved in versions prior to 3.0 never had any params;
    // version 3.0 introduced params to constraints to avoid the hairy ball problem,
    // so force them where they belong.
    IdList<Param,hParam> oldParam = {};
    SK.param.DeepCopyInto(&oldParam);
    SS.GenerateAll(SolveSpaceUI::Generate::REGEN);

    auto AllParamsExistFor = [&](Constraint &c) {
        IdList<Param,hParam> param = {};
        c.Generate(&param);
        bool allParamsExist = true;
        for(Param &p : param) {
            if(oldParam.FindByIdNoOops(p.h) != NULL) continue;
            allParamsExist = false;
            break;
        }
        param.Clear();
        return allParamsExist;
    };

    for(Constraint &c : SK.constraint) {
        switch(c.type) {
            case Constraint::Type::PT_ON_LINE: {
                if(AllParamsExistFor(c)) continue;

                EntityBase *eln = SK.GetEntity(c.entityA);
                EntityBase *ea = SK.GetEntity(eln->point[0]);
                EntityBase *eb = SK.GetEntity(eln->point[1]);
                EntityBase *ep = SK.GetEntity(c.ptA);

                ExprVector exp = ep->PointGetExprsInWorkplane(c.workplane);
                ExprVector exa = ea->PointGetExprsInWorkplane(c.workplane);
                ExprVector exb = eb->PointGetExprsInWorkplane(c.workplane);
                ExprVector exba = exb.Minus(exa);
                Param *p = SK.GetParam(c.h.param(0));
                p->val = exba.Dot(exp.Minus(exa))->Eval() / exba.Dot(exba)->Eval();
                break;
            }

            case Constraint::Type::CUBIC_LINE_TANGENT: {
                if(AllParamsExistFor(c)) continue;

                EntityBase *cubic = SK.GetEntity(c.entityA);
                EntityBase *line  = SK.GetEntity(c.entityB);

                ExprVector a;
                if(c.other) {
                    a = cubic->CubicGetFinishTangentExprs();
                } else {
                    a = cubic->CubicGetStartTangentExprs();
                }

                ExprVector b = line->VectorGetExprs();

                Param *param = SK.GetParam(c.h.param(0));
                param->val = a.Dot(b)->Eval() / b.Dot(b)->Eval();
                break;
            }

            case Constraint::Type::SAME_ORIENTATION: {
                if(AllParamsExistFor(c)) continue;

                EntityBase *an = SK.GetEntity(c.entityA);
                EntityBase *bn = SK.GetEntity(c.entityB);

                ExprVector a = an->NormalExprsN();
                ExprVector b = bn->NormalExprsN();

                Param *param = SK.GetParam(c.h.param(0));
                param->val = a.Dot(b)->Eval() / b.Dot(b)->Eval();
                break;
            }

            case Constraint::Type::PARALLEL: {
                if(AllParamsExistFor(c)) continue;

                EntityBase *ea = SK.GetEntity(c.entityA),
                           *eb = SK.GetEntity(c.entityB);
                ExprVector a = ea->VectorGetExprsInWorkplane(c.workplane);
                ExprVector b = eb->VectorGetExprsInWorkplane(c.workplane);

                Param *param = SK.GetParam(c.h.param(0));
                param->val = a.Dot(b)->Eval() / b.Dot(b)->Eval();
                break;
            }

            default:
                break;
        }
    }
    oldParam.Clear();
}

bool SolveSpaceUI::LoadEntitiesFromFile(const Platform::Path &filename, EntityList *le,
                                        SMesh *m, SShell *sh)
{
    if(strcmp(filename.Extension().c_str(), "emn")==0) {
        return LinkIDF(filename, le, m, sh);
    } else if(strcmp(filename.Extension().c_str(), "EMN")==0) {
        return LinkIDF(filename, le, m, sh);
    } else if(strcmp(filename.Extension().c_str(), "stl")==0) {
        return LinkStl(filename, le, m, sh);    
    } else if(strcmp(filename.Extension().c_str(), "STL")==0) {
        return LinkStl(filename, le, m, sh);    
    } else {
        return LoadEntitiesFromSlvs(filename, le, m, sh);
    }
}

bool SolveSpaceUI::LoadEntitiesFromSlvs(const Platform::Path &filename, EntityList *le,
                                        SMesh *m, SShell *sh)
{
    SSurface srf = {};
    SCurve crv = {};

    fh = OpenFile(filename, "rb");
    if(!fh) return false;

    le->Clear();
    LoadUnion sv = {};

    char line[1024];
    while(fgets(line, (int)sizeof(line), fh)) {
        char *s = strchr(line, '\n');
        if(s) *s = '\0';
        // We should never get files with \r characters in them, but mailers
        // will sometimes mangle attachments.
        s = strchr(line, '\r');
        if(s) *s = '\0';

        if(*line == '\0') continue;

        char *e = strchr(line, '=');
        if(e) {
            *e = '\0';
            char *key = line, *val = e+1;
            LoadUsingTable(filename, key, val, sv);
        } else if(strcmp(line, "AddGroup")==0) {
            // These get allocated whether we want them or not.
            sv.g.remap.clear();
        } else if(strcmp(line, "AddParam")==0) {

        } else if(strcmp(line, "AddEntity")==0) {
            le->Add(&(sv.e));
            sv.e = {};
        } else if(strcmp(line, "AddRequest")==0) {

        } else if(strcmp(line, "AddConstraint")==0) {

        } else if(strcmp(line, "AddStyle")==0) {
            // Linked file contains a style that we don't have yet,
            // so import it.
            if (SK.style.FindByIdNoOops(sv.s.h) == nullptr) {
                SK.style.Add(&(sv.s));
            }
            sv.s = {};
            Style::FillDefaultStyle(&sv.s);
        } else if(strcmp(line, VERSION_STRING)==0) {

        } else if(StrStartsWith(line, "Triangle ")) {
            STriangle tr = {};
            unsigned int rgba = 0;
            if(sscanf(line, "Triangle %x %x  "
                             "%lf %lf %lf  %lf %lf %lf  %lf %lf %lf",
                &(tr.meta.face), &rgba,
                &(tr.a.x), &(tr.a.y), &(tr.a.z),
                &(tr.b.x), &(tr.b.y), &(tr.b.z),
                &(tr.c.x), &(tr.c.y), &(tr.c.z)) != 11) {
                ssassert(false, "Unexpected Triangle format");
            }
            tr.meta.color = RgbaColor::FromPackedInt((uint32_t)rgba);
            m->AddTriangle(&tr);
        } else if(StrStartsWith(line, "Surface ")) {
            unsigned int rgba = 0;
            if(sscanf(line, "Surface %x %x %x %d %d",
                &(srf.h.v), &rgba, &(srf.face),
                &(srf.degm), &(srf.degn)) != 5) {
                ssassert(false, "Unexpected Surface format");
            }
            srf.color = RgbaColor::FromPackedInt((uint32_t)rgba);
        } else if(StrStartsWith(line, "SCtrl ")) {
            int i, j;
            Vector c;
            double w;
            if(sscanf(line, "SCtrl %d %d %lf %lf %lf Weight %lf",
                                &i, &j, &(c.x), &(c.y), &(c.z), &w) != 6)
            {
                ssassert(false, "Unexpected SCtrl format");
            }
            srf.ctrl[i][j] = c;
            srf.weight[i][j] = w;
        } else if(StrStartsWith(line, "TrimBy ")) {
            STrimBy stb = {};
            int backwards;
            if(sscanf(line, "TrimBy %x %d  %lf %lf %lf  %lf %lf %lf",
                &(stb.curve.v), &backwards,
                &(stb.start.x), &(stb.start.y), &(stb.start.z),
                &(stb.finish.x), &(stb.finish.y), &(stb.finish.z)) != 8)
            {
                ssassert(false, "Unexpected TrimBy format");
            }
            stb.backwards = (backwards != 0);
            srf.trim.Add(&stb);
        } else if(strcmp(line, "AddSurface")==0) {
            sh->surface.Add(&srf);
            srf = {};
        } else if(StrStartsWith(line, "Curve ")) {
            int isExact;
            if(sscanf(line, "Curve %x %d %d %x %x",
                &(crv.h.v),
                &(isExact),
                &(crv.exact.deg),
                &(crv.surfA.v), &(crv.surfB.v)) != 5)
            {
                ssassert(false, "Unexpected Curve format");
            }
            crv.isExact = (isExact != 0);
        } else if(StrStartsWith(line, "CCtrl ")) {
            int i;
            Vector c;
            double w;
            if(sscanf(line, "CCtrl %d %lf %lf %lf Weight %lf",
                                &i, &(c.x), &(c.y), &(c.z), &w) != 5)
            {
                ssassert(false, "Unexpected CCtrl format");
            }
            crv.exact.ctrl[i] = c;
            crv.exact.weight[i] = w;
        } else if(StrStartsWith(line, "CurvePt ")) {
            SCurvePt scpt;
            int vertex;
            if(sscanf(line, "CurvePt %d %lf %lf %lf",
                &vertex,
                &(scpt.p.x), &(scpt.p.y), &(scpt.p.z)) != 4)
            {
                ssassert(false, "Unexpected CurvePt format");
            }
            scpt.vertex = (vertex != 0);
            crv.pts.Add(&scpt);
        } else if(strcmp(line, "AddCurve")==0) {
            sh->curve.Add(&crv);
            crv = {};
        } else ssassert(false, "Unexpected operation");
    }

    fclose(fh);
    return true;
}

static Platform::MessageDialog::Response LocateImportedFile(const Platform::Path &filename,
                                                            bool canCancel) {
    Platform::MessageDialogRef dialog = CreateMessageDialog(SS.GW.window);

    using Platform::MessageDialog;
    dialog->SetType(MessageDialog::Type::QUESTION);
    dialog->SetTitle(C_("title", "Missing File"));
    dialog->SetMessage(ssprintf(C_("dialog", "The linked file “%s” is not present."),
                                filename.raw.c_str()));
    dialog->SetDescription(C_("dialog", "Do you want to locate it manually?\n\n"
                                        "If you decline, any geometry that depends on "
                                        "the missing file will be permanently removed."));
    dialog->AddButton(C_("button", "&Yes"), MessageDialog::Response::YES,
                      /*isDefault=*/true);
    dialog->AddButton(C_("button", "&No"), MessageDialog::Response::NO);
    if(canCancel) {
        dialog->AddButton(C_("button", "&Cancel"), MessageDialog::Response::CANCEL);
    }

    // FIXME(async): asyncify this call
    return dialog->RunModal();
}

bool SolveSpaceUI::ReloadAllLinked(const Platform::Path &saveFile, bool canCancel) {
    Platform::SettingsRef settings = Platform::GetSettings();

    std::map<Platform::Path, Platform::Path, Platform::PathLess> linkMap;

    allConsistent = false;

    for(Group &g : SK.group) {
        if(g.type != Group::Type::LINKED) continue;

        g.impEntity.Clear();
        g.impMesh.Clear();
        g.impShell.Clear();

        // If we prompted for this specific file before, don't ask again.
        if(linkMap.count(g.linkFile)) {
            g.linkFile = linkMap[g.linkFile];
        }

try_again:
        if(LoadEntitiesFromFile(g.linkFile, &g.impEntity, &g.impMesh, &g.impShell)) {
            // We loaded the data, good. Now import its dependencies as well.
            for(Entity &e : g.impEntity) {
                if(e.type != Entity::Type::IMAGE) continue;
                if(!ReloadLinkedImage(g.linkFile, &e.file, canCancel)) {
                    return false;
                }
            }
            if(g.IsTriangleMeshAssembly())
                g.forceToMesh = true;
        } else if(linkMap.count(g.linkFile) == 0) {
            dbp("Missing file for group: %s", g.name.c_str());
            // The file was moved; prompt the user for its new location.
            const auto linkFileRelative = g.linkFile.RelativeTo(saveFile);
            switch(LocateImportedFile(linkFileRelative, canCancel)) {
                case Platform::MessageDialog::Response::YES: {
                    Platform::FileDialogRef dialog = Platform::CreateOpenFileDialog(SS.GW.window);
                    dialog->AddFilters(Platform::SolveSpaceLinkFileFilters);
                    dialog->ThawChoices(settings, "LinkSketch");
                    dialog->SuggestFilename(linkFileRelative);
                    if(dialog->RunModal()) {
                        dialog->FreezeChoices(settings, "LinkSketch");
                        linkMap[g.linkFile] = dialog->GetFilename();
                        g.linkFile = dialog->GetFilename();
                        goto try_again;
                    } else {
                        if(canCancel) return false;
                        break;
                    }
                }

                case Platform::MessageDialog::Response::NO:
                    linkMap[g.linkFile].Clear();
                    // Geometry will be pruned by GenerateAll().
                    break;

                case Platform::MessageDialog::Response::CANCEL:
                    return false;

                default:
                    ssassert(false, "Unexpected dialog response");
            }
        } else {
            // User was already asked to and refused to locate a missing linked file.
        }
    }

    for(Request &r : SK.request) {
        if(r.type != Request::Type::IMAGE) continue;

        if(!ReloadLinkedImage(saveFile, &r.file, canCancel)) {
            return false;
        }
    }

    return true;
}

bool SolveSpaceUI::ReloadLinkedImage(const Platform::Path &saveFile,
                                     Platform::Path *filename, bool canCancel) {
    Platform::SettingsRef settings = Platform::GetSettings();

    std::shared_ptr<Pixmap> pixmap;
    bool promptOpenFile = false;
    if(filename->IsEmpty()) {
        // We're prompting the user for a new image.
        promptOpenFile = true;
    } else {
        auto image = SS.images.find(*filename);
        if(image != SS.images.end()) return true;

        pixmap = Pixmap::ReadPng(*filename);
        if(pixmap == NULL) {
            // The file was moved; prompt the user for its new location.
            switch(LocateImportedFile(filename->RelativeTo(saveFile), canCancel)) {
                case Platform::MessageDialog::Response::YES:
                    promptOpenFile = true;
                    break;

                case Platform::MessageDialog::Response::NO:
                    // We don't know where the file is, record it as absent.
                    break;

                case Platform::MessageDialog::Response::CANCEL:
                    return false;

                default:
                    ssassert(false, "Unexpected dialog response");
            }
        }
    }

    if(promptOpenFile) {
        Platform::FileDialogRef dialog = Platform::CreateOpenFileDialog(SS.GW.window);
        dialog->AddFilters(Platform::RasterFileFilters);
        dialog->ThawChoices(settings, "LinkImage");
        dialog->SuggestFilename(filename->RelativeTo(saveFile));
        if(dialog->RunModal()) {
            dialog->FreezeChoices(settings, "LinkImage");
            *filename = dialog->GetFilename();
            pixmap = Pixmap::ReadPng(*filename);
            if(pixmap == NULL) {
                Error("The image '%s' is corrupted.", filename->raw.c_str());
            }
            // We know where the file is now, good.
        } else if(canCancel) {
            return false;
        }
    }

    // We loaded the data, good.
    SS.images[*filename] = pixmap;
    return true;
}
