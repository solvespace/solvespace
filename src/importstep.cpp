//-----------------------------------------------------------------------------
// Immutable STEP linking. The application owns only native geometry and stored
// numerical entities; STEPcode never crosses the private reader boundary.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#include <sstream>
#include <stdexcept>

#ifdef HAVE_STEP_IMPORT
#include "step/stepgeometry.h"
#include "step/stepreader.h"
#endif

namespace SolveSpace {
namespace {

const uint32_t MAX_STEP_REFERENCES = (1u << 16) - 1;

#ifdef HAVE_STEP_IMPORT
StepEntityKey Key(Step::NativeSolid &solid, int kind, int entity) {
    StepEntityKey key = {solid.representation, solid.solid, kind, entity, {}};
    auto addEdge = [&](const Step::EdgeReference &edge) {
        key.topology.insert(key.topology.end(),
            {edge.edge, edge.geometry, edge.startVertex, edge.finishVertex});
    };
    if(kind == StepEntityKey::VERTEX) {
        for(const auto &v : solid.vertices) {
            if(v.vertex == entity) key.topology.push_back(v.point);
        }
        for(const auto &edge : solid.edges) {
            if(edge.startVertex == entity || edge.finishVertex == entity) addEdge(edge);
        }
    } else if(kind == StepEntityKey::EDGE) {
        for(const auto &edge : solid.edges) if(edge.edge == entity) {
            addEdge(edge);
            auto curve = solid.shell.curve.FindById(edge.curve);
            key.topology.push_back(std::min(curve->surfA.v, curve->surfB.v));
            key.topology.push_back(std::max(curve->surfA.v, curve->surfB.v));
        }
    } else {
        for(const auto &face : solid.faces) if(face.face == entity) {
            key.topology.push_back(face.geometry);
            auto surface = solid.shell.surface.FindById(face.surface);
            std::map<hSCurve, bool> boundary;
            for(const auto &trim : surface->trim) boundary.emplace(trim.curve, trim.backwards);
            for(const auto &edge : solid.edges) {
                auto found = boundary.find(edge.curve);
                if(found == boundary.end()) continue;
                addEdge(edge);
                key.topology.push_back(found->second ? 1 : 0);
            }
        }
    }
    return key;
}

void MakeLinkedGeometry(Step::GeometryResult &geometry, SolveSpaceUI::LinkedStepFile *file) {
    // Allocate a deterministic local namespace from provenance, not traversal
    // order. Each linked group translates this into its own persistent table.
    for(const auto &solid : geometry.solids) {
        for(const auto &v : solid->vertices) {
            file->references.emplace(Key(*solid, StepEntityKey::VERTEX, v.vertex), hEntity{});
        }
        for(const auto &e : solid->edges) {
            file->references.emplace(Key(*solid, StepEntityKey::EDGE, e.edge), hEntity{});
        }
        for(const auto &f : solid->faces) {
            file->references.emplace(Key(*solid, StepEntityKey::FACE, f.face), hEntity{});
        }
    }
    if(file->references.size() > MAX_STEP_REFERENCES) {
        throw std::runtime_error("Too many STEP references for one linked group (maximum 65535). "
                                 "No geometry was linked.");
    }
    uint32_t token = 0;
    for(auto &entry : file->references) entry.second.v = ++token;

    uint32_t nextCurve = 0, nextSurface = 0;
    for(auto &owned : geometry.solids) {
        auto &solid = *owned;
        auto entity = [&](int kind, int source) {
            return file->references.at(Key(solid, kind, source));
        };
        std::map<int, Vector> vertices;
        for(const auto &v : solid.vertices) {
            vertices.emplace(v.vertex, v.position);
            Entity e = {};
            e.h = entity(StepEntityKey::VERTEX, v.vertex);
            e.type = Entity::Type::POINT_N_COPY;
            e.numPoint = e.actPoint = v.position;
            e.actVisible = true;
            file->entities.Add(&e);
        }
        std::map<hSCurve, hEntity> edges;
        for(const auto &edge : solid.edges) {
            Entity e = {};
            e.h = entity(StepEntityKey::EDGE, edge.edge);
            e.type = Entity::Type::LINE_SEGMENT;
            e.point[0] = entity(StepEntityKey::VERTEX, edge.startVertex);
            e.point[1] = entity(StepEntityKey::VERTEX, edge.finishVertex);
            e.actVisible = true;
            file->entities.Add(&e);
            edges.emplace(edge.curve, e.h);
        }
        std::map<hSSurface, hEntity> faces;
        for(const auto &face : solid.faces) {
            // Associate a real boundary vertex with the plane, not an extra
            // unconstrained helper point or an arbitrary plane origin.
            int anchor = 0;
            for(const auto &edge : solid.edges) {
                auto curve = solid.shell.curve.FindById(edge.curve);
                if(curve->surfA != face.surface && curve->surfB != face.surface) continue;
                for(int vertex : {edge.startVertex, edge.finishVertex}) {
                    if(anchor == 0 || vertex < anchor) anchor = vertex;
                }
            }
            if(anchor == 0) throw std::runtime_error("A STEP planar face has no boundary vertex.");
            Entity e = {};
            e.h = entity(StepEntityKey::FACE, face.face);
            e.type = Entity::Type::FACE_NORMAL_PT;
            e.point[0] = entity(StepEntityKey::VERTEX, anchor);
            e.actPoint = vertices.at(anchor);
            e.numNormal = e.actNormal = Quaternion::From(0, CO(face.normal));
            e.actVisible = true;
            file->entities.Add(&e);
            faces.emplace(face.surface, e.h);
        }

        // Assemble complete shells without a Boolean or mesh fallback. Native
        // curve/surface IDs are local to a solid, so rewrite BOTH associations
        // and every trim. No source shell is mutated or shallow-shared.
        std::map<hSCurve, hSCurve> curveIds;
        std::map<hSSurface, hSSurface> surfaceIds;
        for(const auto &c : solid.shell.curve) curveIds.emplace(c.h, hSCurve{++nextCurve});
        for(const auto &s : solid.shell.surface) surfaceIds.emplace(s.h, hSSurface{++nextSurface});
        for(auto &c : solid.shell.curve) {
            SCurve copy = SCurve::FromTransformationOf(&c, {}, Quaternion::IDENTITY, 1);
            copy.h = curveIds.at(c.h);
            copy.surfA = surfaceIds.at(c.surfA);
            copy.surfB = surfaceIds.at(c.surfB);
            copy.exact.entity = edges.at(c.h).v;
            file->shell.curve.Add(&copy);
        }
        for(auto &s : solid.shell.surface) {
            SSurface copy = SSurface::FromTransformationOf(&s, {}, Quaternion::IDENTITY, 1,
                                                          /*includingTrims=*/true);
            copy.h = surfaceIds.at(s.h);
            copy.face = faces.at(s.h).v;
            for(auto &trim : copy.trim) trim.curve = curveIds.at(trim.curve);
            file->shell.surface.Add(&copy);
        }
    }
}
#endif

} // namespace

bool SolveSpaceUI::IsStepFile(const Platform::Path &filename) {
    std::string extension = filename.Extension();
    for(char &c : extension) if(c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return extension == "step" || extension == "stp";
}

void SolveSpaceUI::DropLinkedStep(const Platform::Path &filename) {
    auto found = linkedStepFiles.find(filename);
    if(found == linkedStepFiles.end()) return;
    found->second.entities.Clear();
    found->second.shell.Clear();
    linkedStepFiles.erase(found);
}

SolveSpaceUI::LinkedStepFile *SolveSpaceUI::ReadLinkedStep(const Platform::Path &filename,
                                                        bool checkFiles) {
    auto found = linkedStepFiles.find(filename);
    if(found != linkedStepFiles.end() && !checkFiles) return &found->second;

    std::string source;
    bool readable = ReadFile(filename, &source);
    if(found != linkedStepFiles.end() &&
       ((readable && found->second.status != LinkedStepFile::Status::Missing &&
                     source == found->second.source) ||
        (!readable && found->second.status == LinkedStepFile::Status::Missing))) {
        return &found->second;
    }
    DropLinkedStep(filename);
    auto &file = linkedStepFiles[filename];
    if(!readable) return &file;
    file.source = std::move(source);
    file.status = LinkedStepFile::Status::Rejected;
    try {
#ifdef HAVE_STEP_IMPORT
        std::istringstream input(file.source);
        auto read = Step::Read(input);
        if(!read.document) {
            file.diagnostics = read.error;
            if(!read.readerLog.empty()) file.diagnostics += "\n" + read.readerLog;
            return &file;
        }
        auto geometry = Step::ConvertGeometry(*read.document);
        read.document.reset(); // no runtime/schema objects in the cache
        for(const auto &d : geometry.diagnostics) {
            if(d.solid != 0) ++file.omitted;
            file.diagnostics += ssprintf("Representation #%d, solid #%d, entity #%d: %s\n",
                d.representation, d.solid, d.entity, d.message.c_str());
        }
        if(geometry.status == Step::GeometryResult::Status::Rejected || geometry.solids.empty()) {
            return &file;
        }
        MakeLinkedGeometry(geometry, &file);
        file.solids = geometry.solids.size();
        file.status = geometry.status == Step::GeometryResult::Status::Partial ?
                      LinkedStepFile::Status::Partial : LinkedStepFile::Status::Okay;
#else
        file.diagnostics = "This build has no STEP linking support. "
                           "Configure with ENABLE_STEP_IMPORT=ON to enable it.";
#endif
    } catch(const std::exception &error) {
        file.entities.Clear();
        file.shell.Clear();
        file.references.clear();
        file.status = LinkedStepFile::Status::Rejected;
        file.diagnostics += std::string("STEP linking failed: ") + error.what();
    }
    return &file;
}

bool SolveSpaceUI::LoadLinkedStep(const Platform::Path &filename, EntityList *le,
                                 SMesh *m, SShell *sh, bool checkFiles, Group *group) {
    le->Clear();
    m->Clear();
    sh->Clear();
    auto file = ReadLinkedStep(filename, checkFiles);
    if(checkFiles && (file->status == LinkedStepFile::Status::Partial ||
                      file->status == LinkedStepFile::Status::Rejected)) {
        file->pendingDiagnostic = true;
    }
    if(file->status != LinkedStepFile::Status::Okay &&
       file->status != LinkedStepFile::Status::Partial) return false;

    StepEntityMap persistent = group ? group->stepRefs : file->references;
    uint32_t next = 0;
    for(const auto &entry : persistent) next = std::max(next, entry.second.v);
    if(group) {
        // Even a missing provenance row in a damaged/older container must not
        // let us reuse a source token that already has a persisted group remap.
        for(const auto &entry : group->remap) {
            if(entry.first.copyNumber == 0) next = std::max(next, entry.first.input.v);
        }
    }
    std::map<uint32_t, uint32_t> tokens;
    size_t newRemaps = 0;
    for(const auto &entry : file->references) {
        auto inserted = persistent.emplace(entry.first, hEntity{});
        if(inserted.second) inserted.first->second.v = ++next;
        hEntity token = inserted.first->second;
        tokens.emplace(entry.second.v, token.v);
        if(group && group->remap.count({token, 0}) == 0) ++newRemaps;
    }
    // Old identities deliberately count too. Never recycle a deleted reference
    // or let Generate/Remap hit its 16-bit assertion after repeated reloads.
    if(next > MAX_STEP_REFERENCES ||
       (group && group->remap.size() + newRemaps > MAX_STEP_REFERENCES)) {
        if(checkFiles) {
            pendingStepLinkErrors.push_back(ssprintf(
                "Cannot link STEP file '%s': the linked group's persistent reference limit "
                "(65535) would be exceeded. No geometry was linked; create a new linked group.",
                filename.raw.c_str()));
        }
        return false;
    }
    for(auto &e : file->entities) {
        Entity copy = e;
        copy.h.v = tokens.at(e.h.v);
        if(e.type == Entity::Type::LINE_SEGMENT) {
            copy.point[0].v = tokens.at(e.point[0].v);
            copy.point[1].v = tokens.at(e.point[1].v);
        } else if(e.IsFace()) {
            copy.point[0].v = tokens.at(e.point[0].v);
        }
        le->Add(&copy);
    }
    sh->MakeFromCopyOf(&file->shell);
    for(auto &surface : sh->surface) surface.face = tokens.at(surface.face);
    for(auto &curve : sh->curve) curve.exact.entity = tokens.at(curve.exact.entity);
    if(group) {
        size_t missing = 0;
        for(const auto &entry : group->stepRefs) {
            if(file->references.count(entry.first) == 0) ++missing;
        }
        if(checkFiles && missing != 0) {
            pendingStepLinkWarnings.push_back(ssprintf(
                "STEP reference identities are missing from the source: %zu previous "
                "reference(s) in group '%s' are absent.\n\nFile: %s\n\n"
                "Dependent sketch elements will be removed by normal regeneration; "
                "these references have not been reassigned to different geometry.",
                missing, group->name.c_str(), filename.raw.c_str()));
        }
        group->stepRefs.swap(persistent);
    }
    return true;
}

void SolveSpaceUI::ReportStepLinkDiagnostics() {
    // Called only after regeneration has made the sketch consistent, or before
    // a new rejected link has changed it. Never show a dialog inside Generate.
    for(auto &entry : linkedStepFiles) {
        auto &file = entry.second;
        if(!file.pendingDiagnostic) continue;
        file.pendingDiagnostic = false;
        std::string text;
        if(file.status == LinkedStepFile::Status::Partial) {
            text = ssprintf("STEP file linked with omissions: %zu complete solid(s) linked, "
                            "%zu whole solid(s) omitted.\n\nFile: %s\n\n%s",
                            file.solids, file.omitted, entry.first.raw.c_str(),
                            file.diagnostics.c_str());
            dbp("%s", text.c_str());
            Message("%s", text.c_str());
        } else if(file.status == LinkedStepFile::Status::Rejected) {
            text = ssprintf("Cannot link STEP file; no geometry was accepted.\n\nFile: %s\n\n%s",
                            entry.first.raw.c_str(), file.diagnostics.c_str());
            dbp("%s", text.c_str());
            Error("%s", text.c_str());
        }
    }
    auto errors = std::move(pendingStepLinkErrors);
    pendingStepLinkErrors.clear();
    for(const auto &error : errors) {
        dbp("%s", error.c_str());
        Error("%s", error.c_str());
    }
    auto warnings = std::move(pendingStepLinkWarnings);
    pendingStepLinkWarnings.clear();
    for(const auto &warning : warnings) {
        dbp("%s", warning.c_str());
        Message("%s", warning.c_str());
    }
}

} // namespace SolveSpace
