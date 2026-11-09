//-----------------------------------------------------------------------------
// Export a STEP file describing our ratpoly shell.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#include <cstdlib>
#include <map>
#include <set>

namespace SolveSpace {

// Rational weights must not share the coordinate-cleanup policy.
#define WEIGHT_FORMAT "%.16E"
const double PRECISION = 2*LENGTH_EPS;

namespace {

std::string FormatCoordinate(double value) {
    // Try the historical five-decimal cleanup, but limit each coordinate change
    // to LENGTH_EPS / 100 in exported millimetres. This is a practical cleanup
    // policy, not a watertightness guarantee. Preserve detailed trim coordinates
    // at full precision, and always include a decimal point for STEP REALs.
    std::string candidate = ssprintf("%.5f", value);
    double rounded = std::strtod(candidate.c_str(), nullptr);
    if(fabs(rounded - value) <= LENGTH_EPS / 100) {
        return rounded == 0 ? "0.00000" : candidate;
    }
    return ssprintf("%.16E", value);
}

bool Finite(Vector p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

bool ValidCurve(const SBezier &curve) {
    if(curve.deg < 1 || curve.deg > 3) return false;
    bool nonzero = false;
    for(int i = 0; i <= curve.deg; ++i) {
        if(!Finite(curve.ctrl[i]) || !std::isfinite(curve.weight[i]) ||
           curve.weight[i] <= 0) return false;
        nonzero |= !curve.ctrl[i].Equals(curve.Start());
    }
    return nonzero;
}

// Compare ordered control nets, allowing reversal. Positive weights related by
// a common factor times r^i describe the same curve under a fractional-linear
// reparameterization. This also accounts for independently restricted rational
// arcs; ignoring weights or unordered control-point membership would not.
bool SameCurve(const SBezier &a, const SBezier &b, bool reverse) {
    if(a.deg != b.deg) return false;
    double first = log(a.weight[0]) - log(b.weight[reverse ? b.deg : 0]);
    double last = log(a.weight[a.deg]) - log(b.weight[reverse ? 0 : b.deg]);
    for(int i = 0; i <= a.deg; ++i) {
        int j = reverse ? b.deg - i : i;
        if(!a.ctrl[i].Equals(b.ctrl[j])) return false;
        // Every positive-weight rational line describes the whole segment.
        if(a.deg == 1) continue;
        double delta = log(a.weight[i]) - log(b.weight[j]);
        if(fabs(delta - (first + (last - first) * i / a.deg)) > 1e-12) return false;
    }
    return true;
}

bool CurveParameter(const SBezier &curve, Vector p, double *t) {
    if(p.Equals(curve.Start())) { *t = 0; return true; }
    if(p.Equals(curve.Finish())) { *t = 1; return true; }
    if(curve.deg == 1) {
        Vector d = curve.Finish().Minus(curve.Start());
        double u = p.Minus(curve.Start()).Dot(d) / d.MagSquared();
        // SBezier::ClosestPointTo uses the affine parameter for degree one;
        // rational lines need the inverse of their weighted parameterization.
        if(u < 0 || u > 1) return false;
        *t = u * curve.weight[0] /
             ((1 - u) * curve.weight[1] + u * curve.weight[0]);
    } else {
        curve.ClosestPointTo(p, t, /*mustConverge=*/false);
        // The usual early-out is sufficient for display geometry, but cuts
        // found independently on opposite traversals need matching rational
        // control nets, not merely points within the display/root tolerance.
        for(int i = 0; i < 10; ++i) {
            if(!std::isfinite(*t) || *t < 0 || *t > 1) return false;
            Vector tangent = curve.TangentAt(*t);
            double length2 = tangent.MagSquared();
            if(!std::isfinite(length2) || length2 <= 0) return false;
            double next = *t + p.Minus(curve.PointAt(*t)).Dot(tangent) / length2;
            if(EXACT(next == *t)) break;
            *t = next;
        }
    }
    return std::isfinite(*t) && *t >= 0 && *t <= 1 && curve.PointAt(*t).Equals(p);
}

SBezier RestrictCurve(const SBezier &curve, double a, double b) {
    SBezier before, after, restricted = curve;
    if(b < 1) {
        restricted.SplitAt(b, &before, &after);
        restricted = before;
    }
    if(a > 0) {
        restricted.SplitAt(a / b, &before, &after);
        restricted = after;
    }
    return restricted;
}

class StepTopology {
    struct Trim {
        SBezier curve;
        uint32_t source;
        double start, finish;
        int face, edge;
        bool forwards;
    };
    struct Face {
        SSurface *surface;
        std::vector<std::vector<int>> loops;
        int body;
    };
    struct Edge {
        SBezier curve;
        int start, finish, id;
    };
    struct Body {
        std::vector<int> faces, edges;
        std::vector<Vector> vertices;
    };
    struct CoincidentEdges {
        SBezier curve;
        std::vector<int> trims;
    };

    std::vector<Trim> trims;
    std::vector<Face> faces;
    std::vector<Edge> edges;
    std::vector<Body> bodies;

    bool Fail(const std::string &message) {
        error = message;
        return false;
    }

    bool MakeFaces(SShell *shell) {
        for(SSurface &surface : shell->surface) {
            if(surface.trim.IsEmpty()) continue;
            if(surface.degm < 1 || surface.degm > 3 ||
               surface.degn < 1 || surface.degn > 3) {
                return Fail("Unsupported surface degree.");
            }
            for(int i = 0; i <= surface.degm; ++i) {
                for(int j = 0; j <= surface.degn; ++j) {
                    if(!Finite(surface.ctrl[i][j]) ||
                       !std::isfinite(surface.weight[i][j]) || surface.weight[i][j] <= 0) {
                        return Fail("Invalid surface control net or rational weights.");
                    }
                }
            }

            SBezierList sections = {};
            std::vector<Trim> sources;
            bool valid = true;
            // Retain the native curve identity through loop assembly. Different
            // curves can bound the same Boolean seam, but assembled copies have
            // independent identities even when all of their geometry coincides.
            for(const STrimBy &trim : surface.trim) {
                SBezierList section = {};
                const SCurve *source = shell->curve.FindById(trim.curve);
                double a = 0, b = 1;
                if(source->isExact) {
                    const SBezier &curve = source->exact;
                    Vector start = trim.backwards ? trim.finish : trim.start;
                    Vector finish = trim.backwards ? trim.start : trim.finish;
                    if(!ValidCurve(curve) ||
                       (!start.Equals(curve.Start()) && !CurveParameter(curve, start, &a)) ||
                       (!finish.Equals(curve.Finish()) && !CurveParameter(curve, finish, &b)) ||
                       a >= b) {
                        valid = false;
                    } else {
                        SBezier restricted = RestrictCurve(curve, a, b);
                        section.l.Add(&restricted);
                    }
                } else {
                    // Keep the existing approximation for genuinely inexact
                    // intersection curves; exact trims never take that path.
                    SSurface single = surface;
                    single.trim = {};
                    single.trim.Add(&trim);
                    single.MakeSectionEdgesInto(shell, NULL, &section);
                    single.trim.Clear();
                }
                if(section.l.IsEmpty()) valid = false;
                for(SBezier &curve : section.l) {
                    if(!ValidCurve(curve)) valid = false;
                    if(!source->isExact) {
                        a = b = -1;
                        for(int i = 0; i + 1 < source->pts.n && b < 0; ++i) {
                            if(!source->pts[i].p.Equals(curve.Start())) continue;
                            for(int j = i + 1; j < source->pts.n; ++j) {
                                if(!source->pts[j].p.Equals(curve.Finish())) continue;
                                a = i;
                                b = j;
                                break;
                            }
                        }
                        if(a < 0 || b <= a) valid = false;
                    }
                    curve.auxA = 0;
                    curve.auxB = (int)sources.size();
                    Trim provenance = {};
                    provenance.curve = curve;
                    provenance.source = trim.curve.v;
                    provenance.start = a;
                    provenance.finish = b;
                    sources.push_back(provenance);
                    sections.l.Add(&curve);
                }
                section.Clear();
            }
            if(!valid) {
                sections.Clear();
                return Fail(ssprintf("Invalid or missing trim on surface %u.", surface.h.v));
            }

            SBezierLoopSetSet grouped = {};
            SBezierLoopSet open = {};
            SPolygon polygon = {};
            bool allClosed;
            SEdge notClosedAt;
            grouped.FindOuterFacesFrom(&sections, &polygon, &surface,
                                      fabs(SS.ExportChordTolMm()), &allClosed, &notClosedAt,
                                      NULL, NULL, &open);
            valid = allClosed && open.l.IsEmpty() && !grouped.l.IsEmpty();
            size_t used = 0;
            for(const SBezierLoopSet &set : grouped.l) {
                Face face = {};
                face.surface = &surface;
                for(const SBezierLoop &loop : set.l) {
                    std::vector<int> indices;
                    for(const SBezier &curve : loop.l) {
                        Trim trim = sources[curve.auxB];
                        if(!SameCurve(curve, trim.curve, false)) swap(trim.start, trim.finish);
                        trim.curve = curve;
                        trim.face = (int)faces.size();
                        indices.push_back((int)trims.size());
                        trims.push_back(trim);
                        ++used;
                    }
                    face.loops.push_back(std::move(indices));
                }
                faces.push_back(std::move(face));
            }
            sections.Clear();
            grouped.Clear();
            open.Clear();
            polygon.Clear();
            if(!valid || used != sources.size()) {
                return Fail(ssprintf("Open or unused trim contour on surface %u.", surface.h.v));
            }
        }
        return !faces.empty() || Fail("The model has no trimmed faces to export.");
    }

    bool SplitTrim(const Trim &trim, const std::vector<Vector> &endpoints,
                   std::vector<Trim> *into) {
        const SBezier &curve = trim.curve;
        Vector pmax = curve.Start(), pmin = pmax;
        for(int i = 1; i <= curve.deg; ++i) curve.ctrl[i].MakeMaxMin(&pmax, &pmin);
        std::vector<double> cuts;
        for(Vector p : endpoints) {
            if(p.x < pmin.x - LENGTH_EPS || p.x > pmax.x + LENGTH_EPS ||
               p.y < pmin.y - LENGTH_EPS || p.y > pmax.y + LENGTH_EPS ||
               p.z < pmin.z - LENGTH_EPS || p.z > pmax.z + LENGTH_EPS ||
               p.Equals(curve.Start()) || p.Equals(curve.Finish())) continue;
            double t;
            if(CurveParameter(curve, p, &t) && t > 0 && t < 1) cuts.push_back(t);
        }
        std::sort(cuts.begin(), cuts.end());
        cuts.push_back(1.0);
        SBezier rest = curve;
        double previous = 0;
        for(double t : cuts) {
            if(t < 1 && curve.PointAt(t).Equals(curve.PointAt(previous))) continue;
            Trim part = trim;
            if(t < 1) {
                SBezier after;
                rest.SplitAt((t - previous) / (1 - previous), &part.curve, &after);
                rest = after;
            } else {
                part.curve = rest;
            }
            if(!ValidCurve(part.curve)) {
                return Fail(ssprintf("Degenerate trim subdivision on surface %u.",
                                     faces[trim.face].surface->h.v));
            }
            into->push_back(part);
            previous = t;
        }
        return true;
    }

    std::vector<CoincidentEdges> GroupEdges(std::vector<Trim> *boundary, bool byBody) {
        std::vector<CoincidentEdges> coincident;
        for(size_t i = 0; i < boundary->size(); ++i) {
            Trim &trim = (*boundary)[i];
            size_t j;
            for(j = 0; j < coincident.size(); ++j) {
                if(byBody && faces[trim.face].body !=
                             faces[(*boundary)[coincident[j].trims[0]].face].body) continue;
                if(SameCurve(trim.curve, coincident[j].curve, false)) {
                    trim.forwards = true;
                    break;
                }
                if(SameCurve(trim.curve, coincident[j].curve, true)) {
                    trim.forwards = false;
                    break;
                }
            }
            if(j == coincident.size()) {
                CoincidentEdges group = {};
                group.curve = trim.curve;
                coincident.push_back(group);
                trim.forwards = true;
            }
            coincident[j].trims.push_back((int)i);
        }
        return coincident;
    }

    bool FindBodies(SShell *shell) {
        std::vector<int> components;
        for(size_t i = 0; i < faces.size(); ++i) components.push_back((int)i);
        auto root = [&](int face) {
            while(components[face] != face) {
                components[face] = components[components[face]];
                face = components[face];
            }
            return face;
        };
        auto connect = [&](int a, int b) {
            a = root(a);
            b = root(b);
            components[max(a, b)] = min(a, b);
        };

        // Establish native adjacencies first, including unequal intervals of a
        // common curve. Assembly copies have independent curve identities. An
        // already paired native boundary must never join a different body just
        // because their curves (or only portions of them) coincide in space.
        std::map<uint32_t, std::vector<int>> sources;
        for(size_t i = 0; i < trims.size(); ++i) sources[trims[i].source].push_back((int)i);
        std::vector<Trim> open;
        for(const auto &entry : sources) {
            const SCurve *source = shell->curve.FindById(hSCurve{entry.first});
            auto point = [&](double t) {
                if(source->isExact) return source->exact.PointAt(t);
                int i = min((int)t, source->pts.n - 2);
                return source->pts[i].p.ScaledBy(i + 1 - t).Plus(
                       source->pts[i + 1].p.ScaledBy(t - i));
            };
            std::vector<double> cuts;
            for(int i : entry.second) {
                cuts.push_back(trims[i].start);
                cuts.push_back(trims[i].finish);
            }
            std::sort(cuts.begin(), cuts.end());
            std::vector<double> distinct;
            for(double t : cuts) {
                if(!distinct.empty() && fabs(t - distinct.back()) < 1e-10 &&
                   point(t).Equals(point(distinct.back()))) continue;
                distinct.push_back(t);
            }
            for(size_t k = 1; k < distinct.size(); ++k) {
                double a = distinct[k - 1], b = distinct[k], middle = (a + b) / 2;
                std::vector<int> uses;
                for(int i : entry.second) {
                    if(min(trims[i].start, trims[i].finish) < middle &&
                       max(trims[i].start, trims[i].finish) > middle) uses.push_back(i);
                }
                if(uses.empty()) continue;
                if(uses.size() == 2 && trims[uses[0]].face != trims[uses[1]].face &&
                   (trims[uses[0]].start < trims[uses[0]].finish) !=
                   (trims[uses[1]].start < trims[uses[1]].finish)) {
                    connect(trims[uses[0]].face, trims[uses[1]].face);
                } else if(uses.size() == 1) {
                    Trim part = trims[uses[0]];
                    if(source->isExact) {
                        part.curve = RestrictCurve(source->exact, a, b);
                        if(part.start > part.finish) part.curve.Reverse();
                    } else {
                        double ta, tb;
                        if(!CurveParameter(part.curve, point(a), &ta) ||
                           !CurveParameter(part.curve, point(b), &tb)) {
                            return Fail("Cannot synchronize an inexact native trim interval.");
                        }
                        part.curve = RestrictCurve(part.curve, min(ta, tb), max(ta, tb));
                    }
                    open.push_back(part);
                } else {
                    return Fail(ssprintf("Non-manifold native trim curve %u.", entry.first));
                }
            }
        }

        // Booleans and colour-preserving coplanar faces can retain different
        // curve handles on opposite sides. Only the unpaired intervals may
        // supply those additional adjacencies. Synchronize their segmentation
        // exactly; ambiguous coincidences do not license a guessed face pair.
        std::vector<Vector> endpoints;
        for(const Trim &trim : open) {
            endpoints.push_back(trim.curve.Start());
            endpoints.push_back(trim.curve.Finish());
        }
        std::vector<Trim> boundary;
        for(const Trim &trim : open) if(!SplitTrim(trim, endpoints, &boundary)) return false;
        for(const CoincidentEdges &group : GroupEdges(&boundary, false)) {
            if(group.trims.size() != 2) continue;
            const Trim &a = boundary[group.trims[0]], &b = boundary[group.trims[1]];
            if(a.face != b.face && a.forwards != b.forwards) connect(a.face, b.face);
        }
        std::map<int, int> bodyForRoot;
        for(size_t i = 0; i < faces.size(); ++i) {
            int component = root((int)i);
            if(bodyForRoot.find(component) == bodyForRoot.end()) {
                bodyForRoot[component] = (int)bodies.size();
                bodies.push_back({});
            }
            faces[i].body = bodyForRoot[component];
            bodies[faces[i].body].faces.push_back((int)i);
        }
        return true;
    }

    bool SplitTrims() {
        // Now synchronize all face boundaries within each resolved body. Do
        // not introduce vertices from other assembled parts: in particular,
        // coincident circles need not have the same seams or parameterization.
        std::vector<Trim> original;
        original.swap(trims);
        std::vector<std::vector<Vector>> endpoints(bodies.size());
        for(const Trim &trim : original) {
            std::vector<Vector> &points = endpoints[faces[trim.face].body];
            points.push_back(trim.curve.Start());
            points.push_back(trim.curve.Finish());
        }
        for(Face &face : faces) for(std::vector<int> &loop : face.loops) {
            std::vector<int> split;
            for(int index : loop) {
                size_t first = trims.size();
                if(!SplitTrim(original[index], endpoints[face.body], &trims)) return false;
                for(size_t i = first; i < trims.size(); ++i) split.push_back((int)i);
            }
            loop.swap(split);
        }
        return true;
    }

    bool MakeEdges() {
        for(const CoincidentEdges &group : GroupEdges(&trims, true)) {
            const std::vector<int> &uses = group.trims;
            if(uses.size() != 2 || trims[uses[0]].face == trims[uses[1]].face ||
               trims[uses[0]].forwards == trims[uses[1]].forwards) {
                return Fail(ssprintf("Boundary at (%.9g, %.9g, %.9g) does not have "
                                     "two distinct, oppositely oriented faces in one body.",
                                     CO(group.curve.Start())));
            }
            Body &body = bodies[faces[trims[uses[0]].face].body];
            Edge edge = {};
            edge.curve = group.curve;
            auto vertex = [&](Vector p) {
                for(size_t i = 0; i < body.vertices.size(); ++i) {
                    if(body.vertices[i].Equals(p)) return (int)i;
                }
                body.vertices.push_back(p);
                return (int)body.vertices.size() - 1;
            };
            edge.start = vertex(edge.curve.Start());
            edge.finish = vertex(edge.curve.Finish());
            for(int i : uses) trims[i].edge = (int)edges.size();
            body.edges.push_back((int)edges.size());
            edges.push_back(edge);
        }
        return true;
    }

    bool ValidateVertices() {
        using HalfEdge = std::pair<int, bool>;
        using Link = std::map<HalfEdge, std::vector<HalfEdge>>;
        for(const Body &body : bodies) {
            std::vector<Link> links(body.vertices.size());
            for(int f : body.faces) {
                std::set<int> usedEdges, usedVertices;
                for(const std::vector<int> &loop : faces[f].loops) {
                    for(size_t i = 0; i < loop.size(); ++i) {
                        const Trim &before = trims[loop[(i + loop.size() - 1) % loop.size()]];
                        const Trim &after = trims[loop[i]];
                        const Edge &a = edges[before.edge], &b = edges[after.edge];
                        int finish = before.forwards ? a.finish : a.start;
                        int start = after.forwards ? b.start : b.finish;
                        if(finish != start || !usedEdges.insert(after.edge).second ||
                           !usedVertices.insert(start).second) {
                            return Fail(ssprintf("Open, repeated or touching boundary on surface %u.",
                                                 faces[f].surface->h.v));
                        }
                        HalfEdge incoming{before.edge, before.forwards};
                        HalfEdge outgoing{after.edge, !after.forwards};
                        links[start][incoming].push_back(outgoing);
                        links[start][outgoing].push_back(incoming);
                    }
                }
            }
            // Two incident faces per edge are insufficient at a pinched vertex:
            // its incident half-edges must also form one connected degree-2 fan.
            for(const Link &link : links) {
                if(link.empty()) return Fail("Unreferenced boundary vertex.");
                std::set<HalfEdge> seen;
                std::vector<HalfEdge> pending{link.begin()->first};
                while(!pending.empty()) {
                    HalfEdge edge = pending.back();
                    pending.pop_back();
                    if(!seen.insert(edge).second) continue;
                    const std::vector<HalfEdge> &neighbors = link.at(edge);
                    if(neighbors.size() != 2) return Fail("Non-manifold boundary vertex.");
                    pending.insert(pending.end(), neighbors.begin(), neighbors.end());
                }
                if(seen.size() != link.size()) return Fail("Disconnected face fans at a vertex.");
            }
        }
        return true;
    }

public:
    std::string error;

    bool MakeFrom(SShell *shell) {
        return MakeFaces(shell) && FindBodies(shell) && SplitTrims() &&
               MakeEdges() && ValidateVertices();
    }

    void ExportTo(StepFileWriter *writer, double scale) {
        FILE *f = writer->f;
        int &id = writer->id;
        std::map<SSurface *, int> surfaceIds;
        std::vector<int> solids;
        for(const Body &body : bodies) {
            std::vector<int> points, vertices;
            for(Vector p : body.vertices) {
                int point = writer->ExportPoint(p.ScaledBy(scale));
                points.push_back(point);
                vertices.push_back(id);
                fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id++, point);
            }
            for(int e : body.edges) {
                Edge &edge = edges[e];
                SBezier curve = edge.curve;
                curve.ScaleSelfBy(scale);
                // Reuse the vertices already resolved within this body, rather
                // than relying on rounding separate curve endpoints to agree.
                int geometry = writer->ExportCurve(&curve, points[edge.start],
                                                   points[edge.finish]);
                edge.id = id++;
                fprintf(f, "#%d=EDGE_CURVE('',#%d,#%d,#%d,.T.);\n", edge.id,
                        vertices[edge.start], vertices[edge.finish], geometry);
            }
            std::vector<int> advancedFaces;
            for(int index : body.faces) {
                const Face &face = faces[index];
                if(surfaceIds.find(face.surface) == surfaceIds.end()) {
                    // Scaling the live shell here would corrupt subsequent
                    // exports and the model's surface/trim correspondence.
                    SSurface surface = *face.surface;
                    surface.ScaleSelfBy(scale);
                    surfaceIds[face.surface] = writer->ExportSurface(&surface);
                }
                std::vector<int> bounds;
                for(const std::vector<int> &loop : face.loops) {
                    std::vector<int> oriented;
                    for(int t : loop) {
                        const Trim &trim = trims[t];
                        oriented.push_back(id);
                        fprintf(f, "#%d=ORIENTED_EDGE('',*,*,#%d,.%c.);\n", id++,
                                edges[trim.edge].id, trim.forwards ? 'T' : 'F');
                    }
                    int edgeLoop = id++;
                    fprintf(f, "#%d=EDGE_LOOP('',(", edgeLoop);
                    for(size_t i = 0; i < oriented.size(); ++i) {
                        fprintf(f, "%s#%d", i ? "," : "", oriented[i]);
                    }
                    fprintf(f, "));\n");
                    // Native UV loops keep material on the right; both outer
                    // and inner STEP bounds need the opposite boundary sense.
                    fprintf(f, "#%d=%s('',#%d,.F.);\n", id,
                            bounds.empty() ? "FACE_OUTER_BOUND" : "FACE_BOUND", edgeLoop);
                    bounds.push_back(id++);
                }
                int advancedFace = id++;
                fprintf(f, "#%d=ADVANCED_FACE('',(", advancedFace);
                for(size_t i = 0; i < bounds.size(); ++i) {
                    fprintf(f, "%s#%d", i ? "," : "", bounds[i]);
                }
                fprintf(f, "),#%d,.T.);\n", surfaceIds.at(face.surface));
                writer->ExportFaceColor(advancedFace, face.surface->color);
                advancedFaces.push_back(advancedFace);
            }
            int shell = id++;
            fprintf(f, "#%d=CLOSED_SHELL('',(", shell);
            for(size_t i = 0; i < advancedFaces.size(); ++i) {
                fprintf(f, "%s#%d", i ? "," : "", advancedFaces[i]);
            }
            fprintf(f, "));\n");
            solids.push_back(id);
            fprintf(f, "#%d=MANIFOLD_SOLID_BREP('brep',#%d);\n", id++, shell);
        }
        int representation = id++;
        fprintf(f, "#%d=ADVANCED_BREP_SHAPE_REPRESENTATION('',(", representation);
        for(int solid : solids) fprintf(f, "#%d,", solid);
        fprintf(f, "#170),#168);\n");
        fprintf(f, "#%d=SHAPE_REPRESENTATION_RELATIONSHIP('','',#169,#%d);\n",
                id++, representation);
    }
};

} // namespace

//-----------------------------------------------------------------------------
// Functions for STEP export: print to file.
//-----------------------------------------------------------------------------
void StepFileWriter::WriteHeader() {
    // AP214 includes the face colour and transparency entities emitted below.
    fprintf(f,
"ISO-10303-21;\n"
"HEADER;\n"
"\n"
"FILE_DESCRIPTION((''), '2;1');\n"
"\n"
"FILE_NAME(\n"
"    'output_file',\n"
"    '2009-06-07T17:44:47-07:00',\n"
"    (''),\n"
"    (''),\n"
"    'SolveSpace',\n"
"    '',\n"
"    ''\n"
");\n"
"\n"
"FILE_SCHEMA (('AUTOMOTIVE_DESIGN'));\n"
"ENDSEC;\n"
"\n"
"DATA;\n"
"\n"
"/**********************************************************\n"
" * This defines the units and tolerances for the file. It\n"
" * is always the same, independent of the actual data.\n"
" **********************************************************/\n"
"#158=(\n"
"LENGTH_UNIT()\n"
"NAMED_UNIT(*)\n"
"SI_UNIT(.MILLI.,.METRE.)\n"
");\n"
"#161=(\n"
"NAMED_UNIT(*)\n"
"PLANE_ANGLE_UNIT()\n"
"SI_UNIT($,.RADIAN.)\n"
");\n"
"#166=(\n"
"NAMED_UNIT(*)\n"
"SI_UNIT($,.STERADIAN.)\n"
"SOLID_ANGLE_UNIT()\n"
");\n"
"#167=UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(%f),#158,\n"
"'DISTANCE_ACCURACY_VALUE',\n"
"'string');\n"
"#168=(\n"
"GEOMETRIC_REPRESENTATION_CONTEXT(3)\n"
"GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#167))\n"
"GLOBAL_UNIT_ASSIGNED_CONTEXT((#166,#161,#158))\n"
"REPRESENTATION_CONTEXT('ID1','3D')\n"
");\n"
"#169=SHAPE_REPRESENTATION('',(#170),#168);\n"
"#170=AXIS2_PLACEMENT_3D('',#173,#171,#172);\n"
"#171=DIRECTION('',(0.,0.,1.));\n"
"#172=DIRECTION('',(1.,0.,0.));\n"
"#173=CARTESIAN_POINT('',(0.,0.,0.));\n"
"\n",
    PRECISION);

    // Start the ID somewhere beyond the header IDs.
    id = 200;
}
void StepFileWriter::WriteProductHeader() {
	fprintf(f,
		"#175 = SHAPE_DEFINITION_REPRESENTATION(#176, #169);\n"
		"#176 = PRODUCT_DEFINITION_SHAPE('Version', 'Test Part', #177);\n"
		"#177 = PRODUCT_DEFINITION('Version', 'Test Part', #182, #178);\n"
		"#178 = PRODUCT_DEFINITION_CONTEXT('3D Mechanical Parts', #181, 'design');\n"
		"#179 = PRODUCT('1', 'Product', 'Test Part', (#180));\n"
		"#180 = PRODUCT_CONTEXT('3D Mechanical Parts', #181, 'mechanical');\n"
		"#181 = APPLICATION_CONTEXT(\n"
		"'configuration controlled 3d designs of mechanical parts and assemblies');\n"
		"#182 = PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('Version',\n"
		"'Test Part', #179, .MADE.);\n"
		"\n"
		);
}
int StepFileWriter::ExportPoint(Vector point) {
    int ret = id++;
    fprintf(f, "#%d=CARTESIAN_POINT('',(%s,%s,%s));\n", ret,
            FormatCoordinate(point.x).c_str(), FormatCoordinate(point.y).c_str(),
            FormatCoordinate(point.z).c_str());
    return ret;
}

int StepFileWriter::ExportCurve(SBezier *sb, int startPoint, int finishPoint) {
    std::vector<int> points;
    for(int i = 0; i <= sb->deg; ++i) {
        if(i == 0 && startPoint) points.push_back(startPoint);
        else if(i == sb->deg && finishPoint) points.push_back(finishPoint);
        else points.push_back(ExportPoint(sb->ctrl[i]));
    }
    int ret = id++;
    fprintf(f, "#%d=(\n", ret);
    fprintf(f, "BOUNDED_CURVE()\n");
    fprintf(f, "B_SPLINE_CURVE(%d,(", sb->deg);
    for(size_t i = 0; i < points.size(); ++i) fprintf(f, "%s#%d", i ? "," : "", points[i]);
    fprintf(f, "),.UNSPECIFIED.,.F.,.F.)\n");
    fprintf(f, "B_SPLINE_CURVE_WITH_KNOTS((%d,%d),", sb->deg + 1, sb->deg + 1);
    fprintf(f, "(0.000,1.000),.UNSPECIFIED.)\n");
    fprintf(f, "CURVE()\n");
    fprintf(f, "GEOMETRIC_REPRESENTATION_ITEM()\n");
    fprintf(f, "RATIONAL_B_SPLINE_CURVE((");
    for(int i = 0; i <= sb->deg; ++i) {
        fprintf(f, "%s" WEIGHT_FORMAT, i ? "," : "", sb->weight[i]);
    }
    fprintf(f, "))\n");
    fprintf(f, "REPRESENTATION_ITEM('')\n);\n\n");
    return ret;
}

int StepFileWriter::ExportSurface(SSurface *ss) {
    std::vector<int> points;
    for(int i = 0; i <= ss->degm; ++i) {
        for(int j = 0; j <= ss->degn; ++j) points.push_back(ExportPoint(ss->ctrl[i][j]));
    }
    int ret = id++;
    fprintf(f, "#%d=(\n", ret);
    fprintf(f, "BOUNDED_SURFACE()\n");
    fprintf(f, "B_SPLINE_SURFACE(%d,%d,(", ss->degm, ss->degn);
    for(int i = 0; i <= ss->degm; ++i) {
        fprintf(f, "%s(", i ? "," : "");
        for(int j = 0; j <= ss->degn; ++j) {
            fprintf(f, "%s#%d", j ? "," : "", points[i * (ss->degn + 1) + j]);
        }
        fprintf(f, ")");
    }
    fprintf(f, "),.UNSPECIFIED.,.F.,.F.,.F.)\n");
    fprintf(f, "B_SPLINE_SURFACE_WITH_KNOTS((%d,%d),(%d,%d),",
            ss->degm + 1, ss->degm + 1, ss->degn + 1, ss->degn + 1);
    fprintf(f, "(0.000,1.000),(0.000,1.000),.UNSPECIFIED.)\n");
    fprintf(f, "GEOMETRIC_REPRESENTATION_ITEM()\n");
    fprintf(f, "RATIONAL_B_SPLINE_SURFACE((");
    for(int i = 0; i <= ss->degm; ++i) {
        fprintf(f, "%s(", i ? "," : "");
        for(int j = 0; j <= ss->degn; ++j) {
            fprintf(f, "%s" WEIGHT_FORMAT, j ? "," : "", ss->weight[i][j]);
        }
        fprintf(f, ")");
    }
    fprintf(f, "))\n");
    fprintf(f, "REPRESENTATION_ITEM('')\n");
    fprintf(f, "SURFACE()\n);\n\n");
    return ret;
}

void StepFileWriter::ExportFaceColor(int face, RgbaColor color) {
    // https://www.cax-if.org/documents/rec_prac_styling_org_v16.pdf sections 4.4.2, 4.2.4
    // https://tracker.dev.opencascade.org/view.php?id=31550
    int rgb = id++, fillColor = id++, fill = id++, fillArea = id++;
    int transparent = id++, rendering = id++, side = id++, usage = id++;
    int assignment = id++, styled = id++;
    fprintf(f, "#%d=COLOUR_RGB('',%.2f,%.2f,%.2f);\n", rgb,
            color.redF(), color.greenF(), color.blueF());
    fprintf(f, "#%d=FILL_AREA_STYLE_COLOUR('',#%d);\n", fillColor, rgb);
    fprintf(f, "#%d=FILL_AREA_STYLE('',(#%d));\n", fill, fillColor);
    fprintf(f, "#%d=SURFACE_STYLE_FILL_AREA(#%d);\n", fillArea, fill);
    fprintf(f, "#%d=SURFACE_STYLE_TRANSPARENT(%.2f);\n", transparent, 1.0 - color.alphaF());
    fprintf(f, "#%d=SURFACE_STYLE_RENDERING_WITH_PROPERTIES(.NORMAL_SHADING.,#%d,(#%d));\n",
            rendering, rgb, transparent);
    fprintf(f, "#%d=SURFACE_SIDE_STYLE('',(#%d,#%d));\n", side, fillArea, rendering);
    fprintf(f, "#%d=SURFACE_STYLE_USAGE(.BOTH.,#%d);\n", usage, side);
    fprintf(f, "#%d=PRESENTATION_STYLE_ASSIGNMENT((#%d));\n", assignment, usage);
    fprintf(f, "#%d=STYLED_ITEM('',(#%d),#%d);\n\n", styled, assignment, face);
}

void StepFileWriter::WriteFooter() {
    fprintf(f,
"\n"
"ENDSEC;\n"
"\n"
"END-ISO-10303-21;\n"
        );
}

void StepFileWriter::ExportSurfacesTo(const Platform::Path &filename) {
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    SShell *shell = &(g->runningShell);

    if(shell->surface.IsEmpty()) {
        Error("The model does not contain any surfaces to export.%s",
              !g->runningMesh.l.IsEmpty()
                  ? "\n\nThe model does contain triangles from a mesh, but "
                    "a triangle mesh cannot be exported as a STEP file. Try "
                    "File -> Export Mesh... instead."
                  : "");
        return;
    }

    // Prepare and validate the complete boundary graph before opening the
    // destination: a partial or ambiguous shell must not become a STEP solid.
    StepTopology topology;
    if(!topology.MakeFrom(shell)) {
        Error("Cannot export STEP - %s", topology.error.c_str());
        return;
    }

    f = OpenFile(filename, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", filename.raw.c_str());
        return;
    }

    WriteHeader();
    WriteProductHeader();
    topology.ExportTo(this, 1.0 / SS.exportScale);
    WriteFooter();

    fclose(f);
    f = NULL;
}

void StepFileWriter::WriteWireframe() {
    fprintf(f, "#%d=GEOMETRIC_CURVE_SET('curves',(", id);
    int *c;
    for(c = curves.First(); c; c = curves.NextAfter(c)) {
        fprintf(f, "#%d", *c);
        if(curves.NextAfter(c) != NULL) fprintf(f, ",");
    }
    fprintf(f, "));\n");
    fprintf(f, "#%d=GEOMETRICALLY_BOUNDED_WIREFRAME_SHAPE_REPRESENTATION"
                    "('',(#%d,#170),#168);\n", id+1, id);
    fprintf(f, "#%d=SHAPE_REPRESENTATION_RELATIONSHIP('','',#169,#%d);\n",
        id+2, id+1);

    id += 3;
    curves.Clear();
}

} // namespace SolveSpace
