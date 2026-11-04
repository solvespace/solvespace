//-----------------------------------------------------------------------------
// Routines to generate our watertight brep shells from the operations
// and entities specified by the user in each group; templated to work either
// on an SShell of ratpoly surfaces or on an SMesh of triangles.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

void Group::AssembleLoops(bool *allClosed,
                          bool *allCoplanar,
                          bool *allNonZeroLen)
{
    SBezierList sbl = {};

    int i;
    for(auto &e : SK.entity) {
        if(e.group != h)
            continue;
        if(e.construction)
            continue;
        if(e.forceHidden)
            continue;

        e.GenerateBezierCurves(&sbl);
    }

    SBezier *sb;
    *allNonZeroLen = true;
    for(sb = sbl.l.First(); sb; sb = sbl.l.NextAfter(sb)) {
        for(i = 1; i <= sb->deg; i++) {
            if(!(sb->ctrl[i]).Equals(sb->ctrl[0])) {
                break;
            }
        }
        if(i > sb->deg) {
            // This is a zero-length edge.
            *allNonZeroLen = false;
            polyError.errorPointAt = sb->ctrl[0];
            goto done;
        }
    }

    // Try to assemble all these Beziers into loops. The closed loops go into
    // bezierLoops, with the outer loops grouped with their holes. The
    // leftovers, if any, go in bezierOpens.
    bezierLoops.FindOuterFacesFrom(&sbl, &polyLoops, NULL,
                                   SS.ChordTolMm(),
                                   allClosed, &(polyError.notClosedAt),
                                   allCoplanar, &(polyError.errorPointAt),
                                   &bezierOpens);
    done:
    sbl.Clear();
}

void Group::GenerateLoops() {
    polyLoops.Clear();
    bezierLoops.Clear();
    bezierOpens.Clear();

    if(type == Type::DRAWING_3D || type == Type::DRAWING_WORKPLANE ||
       type == Type::ROTATE || type == Type::TRANSLATE || type == Type::LINKED)
    {
        bool allClosed = false, allCoplanar = false, allNonZeroLen = false;
        AssembleLoops(&allClosed, &allCoplanar, &allNonZeroLen);
        if(!allNonZeroLen) {
            polyError.how = PolyError::ZERO_LEN_EDGE;
        } else if(!allCoplanar) {
            polyError.how = PolyError::NOT_COPLANAR;
        } else if(!allClosed) {
            polyError.how = PolyError::NOT_CLOSED;
        } else {
            polyError.how = PolyError::GOOD;
            // The self-intersecting check is kind of slow, so don't run it
            // unless requested.
            if(SS.checkClosedContour) {
                if(polyLoops.SelfIntersecting(&(polyError.errorPointAt))) {
                    polyError.how = PolyError::SELF_INTERSECTING;
                }
            }
        }
    }
}

void SShell::RemapFaces(Group *g, int remap) {
    for(SSurface &ss : surface){
        hEntity face = { ss.face };
        if(face == Entity::NO_ENTITY) continue;

        face = g->Remap(face, remap);
        ss.face = face.v;
    }
}

void SMesh::RemapFaces(Group *g, int remap) {
    STriangle *tr;
    for(tr = l.First(); tr; tr = l.NextAfter(tr)) {
        hEntity face = { tr->meta.face };
        if(face == Entity::NO_ENTITY) continue;

        face = g->Remap(face, remap);
        tr->meta.face = face.v;
    }
}

template<class T>
void Group::GenerateForStepAndRepeat(T *steps, T *outs, Group::CombineAs forWhat) {

    int n = (int)valA, a0 = 0;
    if(subtype == Subtype::ONE_SIDED && skipFirst) {
        a0++; n++;
    }

    int a;
    // create all the transformed copies
    std::vector <T> transd(n);
    std::vector <T> workA(n);
    workA[0] = {};
    // first generate a shell/mesh with each transformed copy
#pragma omp parallel for
    for(a = a0; a < n; a++) {
        transd[a] = {};
        workA[a] = {};
        int ap = a*2 - (subtype == Subtype::ONE_SIDED ? 0 : (n-1));

        if(type == Type::TRANSLATE) {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            trans = trans.ScaledBy(ap);
            transd[a].MakeFromTransformationOf(steps,
                trans, Quaternion::IDENTITY, 1.0);
        } else {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            double theta = ap * SK.GetParam(h.param(3))->val;
            double c = cos(theta), s = sin(theta);
            Vector axis = Vector::From(h.param(4), h.param(5), h.param(6));
            Quaternion q = Quaternion::From(c, s*axis.x, s*axis.y, s*axis.z);
            // Rotation is centered at t; so A(x - t) + t = Ax + (t - At)
            transd[a].MakeFromTransformationOf(steps,
                trans.Minus(q.Rotate(trans)), q, 1.0);
        }
    }
    for(a = a0; a < n; a++) {
        // We need to rewrite any plane face entities to the transformed ones.
        int remap = (a == (n - 1)) ? REMAP_LAST : a;
        transd[a].RemapFaces(this, remap);
    }

    std::vector<T> *soFar = &transd;
    std::vector<T> *scratch = &workA;
    // do the boolean operations on pairs of equal size
    while(n > 1) {
        for(a = 0; a < n; a+=2) {
            scratch->at(a/2).Clear();
            // combine a pair of shells
            if((a==0) && (a0==1)) { // if the first was skipped just copy the 2nd
                scratch->at(a/2).MakeFromCopyOf(&(soFar->at(a+1)));
                (soFar->at(a+1)).Clear();
                a0 = 0;
            } else if (a == n-1) { // for an odd number just copy the last one
                scratch->at(a/2).MakeFromCopyOf(&(soFar->at(a)));
                (soFar->at(a)).Clear();
            } else if(forWhat == CombineAs::ASSEMBLE) {
                scratch->at(a/2).MakeFromAssemblyOf(&(soFar->at(a)), &(soFar->at(a+1)));
                (soFar->at(a)).Clear();
                (soFar->at(a+1)).Clear();
            } else {
                scratch->at(a/2).MakeFromUnionOf(&(soFar->at(a)), &(soFar->at(a+1)));
                (soFar->at(a)).Clear();
                (soFar->at(a+1)).Clear();
            }
        }
        swap(scratch, soFar);
        n = (n+1)/2;
    }
    outs->Clear();
    *outs = soFar->at(0);
}

template<class T>
void Group::GenerateForBoolean(T *prevs, T *thiss, T *outs, Group::CombineAs how) {
    // If this group contributes no new mesh, then our running mesh is the
    // same as last time, no combining required. Likewise if we have a mesh
    // but it's suppressed.
    if(thiss->IsEmpty() || suppress) {
        outs->MakeFromCopyOf(prevs);
        return;
    }

    // So our group's shell appears in thisShell. Combine this with the
    // previous group's shell, using the requested operation.
    switch(how) {
        case CombineAs::UNION:
            outs->MakeFromUnionOf(prevs, thiss);
            break;

        case CombineAs::DIFFERENCE:
            outs->MakeFromDifferenceOf(prevs, thiss);
            break;

        case CombineAs::INTERSECTION:
            outs->MakeFromIntersectionOf(prevs, thiss);
            break;

        case CombineAs::ASSEMBLE:
            outs->MakeFromAssemblyOf(prevs, thiss);
            break;
    }
}

//-----------------------------------------------------------------------------
// Fillets and chamfers of straight edges between two plane faces. Each edge
// gets a prismatic tool that runs along it, with a cross section bounded by
// the two faces and by the rounding arc or bevel line. The tool is subtracted
// from the solid at a convex edge and added to it at a concave edge.
//-----------------------------------------------------------------------------
namespace {

struct EdgeFace {
    Vector  n;          // outward normal of the plane face
    bool    side[2];    // whether the face extends to +w or to -w
};

struct EdgeBlend {
    hEntity h;
    Vector  a, b, d;    // end points of the edge, and unit vector from a to b
    Vector  n1, u1;     // outward normal of the first face, and the direction
                        // from the edge into that face
    Vector  n2, u2;     // the same for the second face
    double  theta;      // angle between u1 and u2
    double  k;          // distance from the edge to the rounding or bevel,
                        // along each face
    double  ext;        // how far the tool reaches beyond the faces
    bool    convex;
};

// Points closer than this to a plane lie on it.
const double EDGE_PLANE_TOL = 1e-4;

// Is the point p on the mesh, within a triangle with outward normal n?
bool PointOnMeshFace(SMesh *m, Vector n, Vector p) {
    for(const STriangle &t : m->l) {
        Vector tn = t.Normal();
        if(tn.Magnitude() < LENGTH_EPS) continue;
        tn = tn.WithMagnitude(1);
        if(tn.Dot(n) < 1 - 1e-6) continue;
        if(fabs(tn.Dot(p.Minus(t.a))) > EDGE_PLANE_TOL) continue;
        if(t.ContainsPointProjd(tn, p)) return true;
    }
    return false;
}

// Is there a plane surface in the shell that lies in the plane through p
// with normal n?
bool ShellHasPlane(SShell *sh, Vector n, Vector p) {
    for(SSurface &ss : sh->surface) {
        if(ss.degm != 1 || ss.degn != 1) continue;
        bool onPlane = true;
        for(int i = 0; i < 2 && onPlane; i++) {
            for(int j = 0; j < 2 && onPlane; j++) {
                if(fabs(n.Dot(ss.ctrl[i][j].Minus(p))) > EDGE_PLANE_TOL) {
                    onPlane = false;
                }
            }
        }
        if(onPlane) return true;
    }
    return false;
}

// Find the two plane faces of the solid at the edge, and the size of the
// rounding or bevel on them.
bool AnalyzeEdge(SMesh *body, SShell *bodyShell, hEntity he, bool chamfer,
                 double size, EdgeBlend *eb, std::string *error)
{
    Entity *e = SK.entity.FindByIdNoOops(he);
    if(e == NULL || e->type != Entity::Type::LINE_SEGMENT) {
        *error = ssprintf(_("edge %08x does not exist"), he.v);
        return false;
    }
    Vector a = SK.GetEntity(e->point[0])->PointGetNum(),
           b = SK.GetEntity(e->point[1])->PointGetNum();
    double len = (b.Minus(a)).Magnitude();
    if(len < LENGTH_EPS) {
        *error = ssprintf(_("edge %08x has zero length"), he.v);
        return false;
    }
    if(size < LENGTH_EPS) {
        *error = _("the size must be greater than zero");
        return false;
    }

    Vector d = (b.Minus(a)).WithMagnitude(1),
           mid = (a.Plus(b)).ScaledBy(0.5);
    // The offset to either side of the edge, to find the faces there.
    double eps = max(10*EDGE_PLANE_TOL, 1e-3*min(len, size));

    std::vector<EdgeFace> faces;
    for(const STriangle &t : body->l) {
        Vector n = t.Normal();
        if(n.Magnitude() < LENGTH_EPS) continue;
        n = n.WithMagnitude(1);
        if(fabs(n.Dot(d)) > 1e-6) continue;
        if(fabs(n.Dot(a.Minus(t.a))) > EDGE_PLANE_TOL) continue;
        if(fabs(n.Dot(b.Minus(t.a))) > EDGE_PLANE_TOL) continue;

        Vector w = n.Cross(d);
        for(int i = 0; i < 2; i++) {
            Vector p = mid.Plus(w.ScaledBy(i == 0 ? eps : -eps));
            if(!t.ContainsPointProjd(n, p)) continue;

            EdgeFace *f = NULL;
            for(EdgeFace &ef : faces) {
                if(ef.n.Dot(n) > 1 - 1e-6) f = &ef;
            }
            if(f == NULL) {
                faces.push_back(EdgeFace { n, { false, false } });
                f = &faces.back();
            }
            f->side[i] = true;
        }
    }

    int found = 0;
    for(EdgeFace &f : faces) {
        // A face on both sides continues across the line, so the line is not
        // its boundary.
        if(f.side[0] == f.side[1]) continue;
        Vector u = (f.n.Cross(d)).ScaledBy(f.side[0] ? 1 : -1);
        if(found == 0) {
            eb->n1 = f.n; eb->u1 = u;
        } else {
            eb->n2 = f.n; eb->u2 = u;
        }
        found++;
    }
    if(found != 2 || (!bodyShell->IsEmpty() &&
                      (!ShellHasPlane(bodyShell, eb->n1, a) ||
                       !ShellHasPlane(bodyShell, eb->n2, a))))
    {
        *error = ssprintf(_("line %08x is not an edge between two plane "
                            "faces of the solid"), he.v);
        return false;
    }

    eb->h = he;
    eb->a = a;
    eb->b = b;
    eb->d = d;
    eb->theta = acos(max(-1.0, min(1.0, eb->u1.Dot(eb->u2))));
    if(eb->theta < 1*PI/180 || eb->theta > 179*PI/180) {
        *error = ssprintf(_("the faces at edge %08x are nearly parallel"),
                          he.v);
        return false;
    }
    // At a convex edge the second face turns away from the outside of the
    // first face, so the wedge between u1 and u2 is filled with material.
    eb->convex = (eb->u1.Dot(eb->n2) < 0);
    eb->k = chamfer ? size : size / tan(eb->theta/2);
    // The tool reaches a little beyond the faces (outside at a convex edge,
    // into the material at a concave one), so that it is not coincident with
    // the faces of the solid.
    eb->ext = 0.1*eb->k * (eb->convex ? 1 : -1);

    for(int i = 0; i < 2; i++) {
        Vector n = (i == 0) ? eb->n1 : eb->n2,
               u = (i == 0) ? eb->u1 : eb->u2;
        if(!PointOnMeshFace(body, n, mid.Plus(u.ScaledBy(eb->k)))) {
            *error = ssprintf(_("the size is too large for the faces at "
                                "edge %08x"), he.v);
            return false;
        }
    }
    return true;
}

// Assemble the closed loop of curves in sbl, oriented the same way as the
// sketch of an extrusion group.
bool LoopFrom(SBezierList *sbl, SBezierLoopSetSet *sblss) {
    SPolygon poly = {};
    SEdge notClosedAt = {};
    Vector notCoplanarAt = {};
    bool allClosed, allCoplanar;
    sblss->FindOuterFacesFrom(sbl, &poly, NULL, SS.ChordTolMm(),
                              &allClosed, &notClosedAt,
                              &allCoplanar, &notCoplanarAt, NULL);
    poly.Clear();
    return (allClosed && allCoplanar && sblss->l.n == 1);
}

// Make the tool for the edge. It runs along the edge from the plane through
// pa with normal na to the plane through pb with normal nb.
bool MakeEdgeTool(const EdgeBlend &eb, bool chamfer, Vector pa, Vector na,
                  Vector pb, Vector nb, RgbaColor color, SShell *tool)
{
    // The cross section of the tool, in the plane normal to the edge at a.
    Vector t1 = eb.a.Plus(eb.u1.ScaledBy(eb.k)),
           t2 = eb.a.Plus(eb.u2.ScaledBy(eb.k)),
           p1 = t1.Plus(eb.n1.ScaledBy(eb.ext)),
           p2 = eb.a.Plus((eb.n1.Plus(eb.n2)).ScaledBy(eb.ext)),
           p3 = t2.Plus(eb.n2.ScaledBy(eb.ext));

    SBezierList sbl = {};
    Vector pts[] = { t1, p1, p2, p3, t2 };
    for(int i = 0; i < 4; i++) {
        SBezier line = SBezier::From(pts[i], pts[i+1]);
        sbl.l.Add(&line);
    }
    if(chamfer) {
        SBezier bevel = SBezier::From(t2, t1);
        sbl.l.Add(&bevel);
    } else {
        // A circular arc tangent to both faces, as a rational quadratic Bezier
        // whose middle control point is the corner, where the tangents meet.
        SBezier arc = SBezier::From(t2, eb.a, t1);
        arc.weight[1] = sin(eb.theta/2);
        sbl.l.Add(&arc);
    }

    SBezierLoopSetSet sblss = {};
    bool ok = LoopFrom(&sbl, &sblss) &&
              tool->MakeFromExtrusionBetweenPlanes(&(sblss.l[0]), eb.d,
                                                   pa, na, pb, nb, color);
    sblss.Clear();
    sbl.Clear();
    return ok;
}

// Distance from the edge beyond which the tool does not reach.
double ToolReach(const EdgeBlend &eb) {
    return eb.k + 2*fabs(eb.ext);
}

// Is v an end point of the edge? If so, return the direction from v along
// the edge.
bool EdgeEndAt(const EdgeBlend &eb, Vector v, Vector *dir) {
    if(v.Equals(eb.a, EDGE_PLANE_TOL)) {
        *dir = eb.d;
        return true;
    } else if(v.Equals(eb.b, EDGE_PLANE_TOL)) {
        *dir = eb.d.ScaledBy(-1);
        return true;
    }
    return false;
}

// Must the tools of two edges that meet at a vertex end at the plane that
// bisects the angle between them? Where two rounded edges meet, their tools
// would otherwise overlap there, and their cylinders would intersect in a
// degenerate way. Two bevels may overlap, unless the edges meet at a
// reflex corner of their common face; there the tools must reach past the
// vertex to meet, and the bisecting plane is where they stop.
bool NeedsMiter(const EdgeBlend &eb, const EdgeBlend &ej,
                Vector dj, bool chamfer)
{
    if(!chamfer) return true;
    Vector ni[2] = { eb.n1, eb.n2 }, ui[2] = { eb.u1, eb.u2 },
           nj[2] = { ej.n1, ej.n2 };
    for(int i = 0; i < 2; i++) {
        for(int j = 0; j < 2; j++) {
            if(ni[i].Dot(nj[j]) < 1 - 1e-6) continue;
            // The common face lies between the two edges at the vertex if
            // the other edge runs into the face, as seen from this edge.
            return dj.Dot(ui[i]) < 0;
        }
    }
    return true;
}

} // namespace

void Group::GenerateEdgeBlends() {
    edgeError.clear();

    Group *prevg = RunningMeshGroup();
    if(prevg == NULL) {
        edgeError = _("there is no solid model before this group");
        return;
    }

    bool toMesh = IsForcedToMesh();
    if(toMesh) {
        runningMesh.MakeFromCopyOf(&(prevg->runningMesh));
        prevg->runningShell.TriangulateInto(&runningMesh);
    } else {
        runningShell.MakeFromCopyOf(&(prevg->runningShell));
    }
    if(suppress) return;

    bool chamfer = (subtype == Subtype::EDGE_CHAMFER);

    // The faces at each edge are found on the solid as it was before this
    // group, so that the result does not depend on the order of the edges.
    SMesh body = {};
    body.MakeFromCopyOf(&(prevg->runningMesh));
    prevg->runningShell.TriangulateInto(&body);

    std::vector<EdgeBlend> blends;
    for(hEntity he : GetEdges()) {
        EdgeBlend eb = {};
        if(AnalyzeEdge(&body, &(prevg->runningShell), he, chamfer, valA,
                       &eb, &edgeError))
        {
            blends.push_back(eb);
        }
    }
    body.Clear();

    size_t n = blends.size();
    std::vector<SShell> tools(n);
    std::vector<bool> haveTool(n, false);
    // Edges whose tools touch at a vertex are combined into one tool first,
    // and applied together; this is their group.
    std::vector<size_t> component(n);
    for(size_t i = 0; i < n; i++) component[i] = i;
    std::function<size_t(size_t)> root = [&](size_t i) {
        while(component[i] != i) i = component[i];
        return i;
    };

    for(size_t i = 0; i < n; i++) {
        const EdgeBlend &eb = blends[i];

        // The plane at each end of the tool, and any further planes that cut
        // it there, given by their normals pointing into the tool.
        Vector endPoint[2] = { eb.a, eb.b }, endNormal[2] = { eb.d, eb.d };
        std::vector<Vector> moreMiters[2];
        for(int end = 0; end < 2; end++) {
            Vector v = (end == 0) ? eb.a : eb.b,
                   di = (end == 0) ? eb.d : eb.d.ScaledBy(-1);
            bool first = true;
            for(size_t j = 0; j < n; j++) {
                const EdgeBlend &ej = blends[j];
                Vector dj;
                if(j == i || ej.convex != eb.convex) continue;
                if(!EdgeEndAt(ej, v, &dj)) continue;
                component[root(i)] = root(j);

                double phi = acos(max(-1.0, min(1.0, di.Dot(dj))));
                if(phi < 10*PI/180) continue;
                if(!NeedsMiter(eb, ej, dj, chamfer)) continue;

                Vector m = (di.Minus(dj)).WithMagnitude(1);
                if(first) {
                    endNormal[end] = m;
                    first = false;
                } else {
                    moreMiters[end].push_back(m);
                }
            }
        }

        // A concave edge whose end meets convex edges would stick out of
        // their blends there. Unless it is mitered, end it a little before
        // the blend of the convex edges begins, on the faces along it.
        for(int end = 0; end < 2 && !eb.convex; end++) {
            if(!endNormal[end].Equals(eb.d)) continue;
            Vector v = (end == 0) ? eb.a : eb.b,
                   di = (end == 0) ? eb.d : eb.d.ScaledBy(-1);
            double s = 0;
            for(const EdgeBlend &ej : blends) {
                Vector dj;
                if(!ej.convex || !EdgeEndAt(ej, v, &dj)) continue;
                Vector nj[2] = { ej.n1, ej.n2 }, uj[2] = { ej.u1, ej.u2 };
                for(int f = 0; f < 2; f++) {
                    double c = di.Dot(uj[f]);
                    if(fabs(nj[f].Dot(di)) > 1e-6 || c < 0.1) continue;
                    s = max(s, 1.02*ej.k / c);
                }
            }
            endPoint[end] = v.Plus(di.ScaledBy(s));
        }

        SShell *tool = &tools[i];
        if(!MakeEdgeTool(eb, chamfer, endPoint[0], endNormal[0],
                         endPoint[1], endNormal[1], color, tool)) {
            edgeError = ssprintf(_("the edges next to edge %08x are too "
                                   "close for this size"), eb.h.v);
            tool->Clear();
            continue;
        }
        haveTool[i] = true;

        // Where three or more of the edges meet, the tool ends at more than
        // one plane. Cut it at the others with a box larger than the tool.
        for(int end = 0; end < 2; end++) {
            Vector v = (end == 0) ? eb.a : eb.b;
            for(Vector m : moreMiters[end]) {
                double big = 4*((eb.b.Minus(eb.a)).Magnitude() + ToolReach(eb));
                Vector u = m.Normal(0).WithMagnitude(big),
                       w = m.Normal(1).WithMagnitude(big);
                Vector c[4] = {
                    v.Minus(u).Minus(w), v.Plus(u).Minus(w),
                    v.Plus(u).Plus(w),   v.Minus(u).Plus(w),
                };
                SBezierList sbl = {};
                for(int k = 0; k < 4; k++) {
                    SBezier line = SBezier::From(c[k], c[(k + 1) % 4]);
                    sbl.l.Add(&line);
                }
                SBezierLoopSetSet sblss = {};
                SShell box = {}, cut = {};
                if(LoopFrom(&sbl, &sblss)) {
                    box.MakeFromExtrusionOf(&(sblss.l[0]), Vector::From(0, 0, 0),
                                            m.ScaledBy(-big), color);
                }
                sblss.Clear();
                sbl.Clear();
                cut.MakeFromDifferenceOf(tool, &box);
                box.Clear();
                tool->Clear();
                *tool = cut;
                if(cut.booleanFailed) haveTool[i] = false;
            }
        }
    }

    // Add material at the concave edges first, then remove it at the convex
    // ones, one group of touching edges at a time. If that fails for a
    // group, leave its edges as they were.
    std::vector<std::string> failed;
    for(int pass = 0; pass < 2; pass++) {
        bool convex = (pass == 1);
        for(size_t c = 0; c < n; c++) {
            if(root(c) != c || blends[c].convex != convex) continue;

            SShell tool = {};
            bool ok = true;
            std::string names;
            for(size_t i = 0; i < n; i++) {
                if(root(i) != c) continue;
                names += ssprintf("%s%08x", names.empty() ? "" : " ",
                                  blends[i].h.v);
                if(!haveTool[i]) {
                    ok = false;
                } else if(tool.IsEmpty()) {
                    tool = tools[i];
                    tools[i] = {};
                } else {
                    SShell merged = {};
                    merged.MakeFromUnionOf(&tool, &tools[i]);
                    if(merged.booleanFailed) ok = false;
                    tool.Clear();
                    tool = merged;
                }
            }

            if(ok && !toMesh) {
                SShell result = {};
                if(convex) {
                    result.MakeFromDifferenceOf(&runningShell, &tool);
                } else {
                    result.MakeFromUnionOf(&runningShell, &tool);
                }
                if(result.booleanFailed) {
                    ok = false;
                    result.Clear();
                } else {
                    runningShell.Clear();
                    runningShell = result;
                }
            } else if(ok) {
                SMesh toolMesh = {}, result = {};
                tool.TriangulateInto(&toolMesh);
                if(convex) {
                    result.MakeFromDifferenceOf(&runningMesh, &toolMesh);
                } else {
                    result.MakeFromUnionOf(&runningMesh, &toolMesh);
                }
                toolMesh.Clear();
                runningMesh.Clear();
                result.RemoveDegenerateTriangles();
                SKdNode *kd = SKdNode::From(&result);
                kd->SnapToMesh(&result);
                kd->MakeMeshInto(&runningMesh);
                result.Clear();
            }
            tool.Clear();
            if(!ok) failed.push_back(names);
        }
    }
    for(SShell &t : tools) t.Clear();

    if(!failed.empty()) {
        std::string list;
        for(const std::string &f : failed) {
            list += (list.empty() ? "" : ", ") + f;
        }
        edgeError = ssprintf(_("could not apply the blend at edges %s"),
                             list.c_str());
    }

    if(!toMesh) {
        runningShell.MergeCoincidentSurfaces();
    }
}

void Group::GenerateShellAndMesh() {
    bool prevBooleanFailed = booleanFailed;
    booleanFailed = false;

    Group *srcg = this;

    thisShell.Clear();
    thisMesh.Clear();
    runningShell.Clear();
    runningMesh.Clear();

    if(type == Type::FILLET) {
        std::string prevEdgeError = edgeError;
        GenerateEdgeBlends();
        if(booleanFailed != prevBooleanFailed || edgeError != prevEdgeError) {
            SS.ScheduleShowTW();
        }
        displayDirty = true;
        return;
    }

    // Don't attempt a lathe or extrusion unless the source section is good:
    // planar and not self-intersecting.
    bool haveSrc = true;
    if(type == Type::EXTRUDE || type == Type::LATHE || type == Type::REVOLVE) {
        Group *src = SK.GetGroup(opA);
        if(src->polyError.how != PolyError::GOOD) {
            haveSrc = false;
        }
    }

    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        // A step and repeat gets merged against the group's previous group,
        // not our own previous group.
        srcg = SK.GetGroup(opA);

        if(!srcg->suppress) {
            if(!IsForcedToMesh()) {
                GenerateForStepAndRepeat<SShell>(&(srcg->thisShell), &thisShell, srcg->meshCombine);
            } else {
                SMesh prevm = {};
                prevm.MakeFromCopyOf(&srcg->thisMesh);
                srcg->thisShell.TriangulateInto(&prevm);
                GenerateForStepAndRepeat<SMesh> (&prevm, &thisMesh, srcg->meshCombine);
            }
        }
    } else if(type == Type::EXTRUDE && haveSrc) {
        Group *src = SK.GetGroup(opA);
        Vector translate = Vector::From(h.param(0), h.param(1), h.param(2));

        Vector tbot, ttop;
        if(subtype == Subtype::ONE_SIDED || subtype == Subtype::ONE_SKEWED) {
            tbot = {}; ttop = translate.ScaledBy(2);
        } else {
            tbot = translate.ScaledBy(-1); ttop = translate.ScaledBy(1);
        }

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            int is = thisShell.surface.n;
            // Extrude this outer contour (plus its inner contours, if present)
            thisShell.MakeFromExtrusionOf(sbls, tbot, ttop, color);

            // And for any plane faces, annotate the model with the entity for
            // that face, so that the user can select them with the mouse.
            Vector onOrig = sbls->point;
            int i;
            // Not using range-for here because we're starting at a different place and using
            // indices for meaning.
            for(i = is; i < thisShell.surface.n; i++) {
                SSurface *ss = &(thisShell.surface[i]);
                hEntity face = Entity::NO_ENTITY;

                Vector p = ss->PointAt(0, 0),
                       n = ss->NormalAt(0, 0).WithMagnitude(1);
                double d = n.Dot(p);

                if(i == is || i == (is + 1)) {
                    // These are the top and bottom of the shell.
                    if(fabs((onOrig.Plus(ttop)).Dot(n) - d) < LENGTH_EPS) {
                        face = Remap(Entity::NO_ENTITY, REMAP_TOP);
                        ss->face = face.v;
                    }
                    if(fabs((onOrig.Plus(tbot)).Dot(n) - d) < LENGTH_EPS) {
                        face = Remap(Entity::NO_ENTITY, REMAP_BOTTOM);
                        ss->face = face.v;
                    }
                    continue;
                }

                // So these are the sides
                if(ss->degm != 1 || ss->degn != 1) continue;

                for(Entity &e : SK.entity) {
                    if(e.group != opA) continue;
                    if(e.type != Entity::Type::LINE_SEGMENT) continue;

                    Vector a = SK.GetEntity(e.point[0])->PointGetNum(),
                           b = SK.GetEntity(e.point[1])->PointGetNum();
                    a = a.Plus(ttop);
                    b = b.Plus(ttop);
                    // Could get taken backwards, so check all cases.
                    if((a.Equals(ss->ctrl[0][0]) && b.Equals(ss->ctrl[1][0])) ||
                       (b.Equals(ss->ctrl[0][0]) && a.Equals(ss->ctrl[1][0])) ||
                       (a.Equals(ss->ctrl[0][1]) && b.Equals(ss->ctrl[1][1])) ||
                       (b.Equals(ss->ctrl[0][1]) && a.Equals(ss->ctrl[1][1])))
                    {
                        face = Remap(e.h, REMAP_LINE_TO_FACE);
                        ss->face = face.v;
                        break;
                    }
                }
            }
        }
    } else if(type == Type::LATHE && haveSrc) {
        Group *src = SK.GetGroup(opA);

        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            thisShell.MakeFromRevolutionOf(sbls, pt, axis, color, this);
        }
    } else if(type == Type::REVOLVE && haveSrc) {
        Group *src    = SK.GetGroup(opA);
        double anglef = SK.GetParam(h.param(3))->val * 4; // why the 4 is needed?
        double dists = 0, distf = 0;
        double angles = 0.0;
        if(subtype != Subtype::ONE_SIDED) {
            anglef *= 0.5;
            angles = -anglef;
        }
        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis        = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            if(fabs(anglef - angles) < 2 * PI) {
                thisShell.MakeFromHelicalRevolutionOf(sbls, pt, axis, color, this,
                                                      angles, anglef, dists, distf);
            } else {
                thisShell.MakeFromRevolutionOf(sbls, pt, axis, color, this);
            }
        }
    } else if(type == Type::HELIX && haveSrc) {
        Group *src    = SK.GetGroup(opA);
        double anglef = SK.GetParam(h.param(3))->val * 4; // why the 4 is needed?
        double dists = 0, distf = 0;
        double angles = 0.0;
        distf = SK.GetParam(h.param(7))->val * 2; // dist is applied twice
        if(subtype != Subtype::ONE_SIDED) {
            anglef *= 0.5;
            angles = -anglef;
            distf *= 0.5;
            dists = -distf;
        }
        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis        = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            thisShell.MakeFromHelicalRevolutionOf(sbls, pt, axis, color, this,
                                                  angles, anglef, dists, distf);
        }
    } else if(type == Type::LINKED) {
        // The imported shell or mesh are copied over, with the appropriate
        // transformation applied. We also must remap the face entities.
        Vector offset = {
            SK.GetParam(h.param(0))->val,
            SK.GetParam(h.param(1))->val,
            SK.GetParam(h.param(2))->val };
        Quaternion q = {
            SK.GetParam(h.param(3))->val,
            SK.GetParam(h.param(4))->val,
            SK.GetParam(h.param(5))->val,
            SK.GetParam(h.param(6))->val };

        thisMesh.MakeFromTransformationOf(&impMesh, offset, q, scale);
        thisMesh.RemapFaces(this, 0);

        thisShell.MakeFromTransformationOf(&impShell, offset, q, scale);
        thisShell.RemapFaces(this, 0);
    }

    if(srcg->meshCombine != CombineAs::ASSEMBLE) {
        thisShell.MergeCoincidentSurfaces();
    }

    // So now we've got the mesh or shell for this group. Combine it with
    // the previous group's mesh or shell with the requested Boolean, and
    // we're done.

    Group *prevg = srcg->RunningMeshGroup();

    if(!IsForcedToMesh()) {
        SShell *prevs = &(prevg->runningShell);
        GenerateForBoolean<SShell>(prevs, &thisShell, &runningShell,
            srcg->meshCombine);

        if(srcg->meshCombine != CombineAs::ASSEMBLE) {
            runningShell.MergeCoincidentSurfaces();
        }

        // If the Boolean failed, then we should note that in the text screen
        // for this group.
        booleanFailed = runningShell.booleanFailed;
        if(booleanFailed != prevBooleanFailed) {
            SS.ScheduleShowTW();
        }
    } else {
        SMesh prevm, thism;
        prevm = {};
        thism = {};

        prevm.MakeFromCopyOf(&(prevg->runningMesh));
        prevg->runningShell.TriangulateInto(&prevm);

        thism.MakeFromCopyOf(&thisMesh);
        thisShell.TriangulateInto(&thism);

        SMesh outm = {};
        GenerateForBoolean<SMesh>(&prevm, &thism, &outm, srcg->meshCombine);

        // Remove degenerate triangles; if we don't, they'll get split in SnapToMesh
        // in every generated group, resulting in polynomial increase in triangle count,
        // and corresponding slowdown.
        outm.RemoveDegenerateTriangles();

        if(srcg->meshCombine != CombineAs::ASSEMBLE) {
            // And make sure that the output mesh is vertex-to-vertex.
            SKdNode *root = SKdNode::From(&outm);
            root->SnapToMesh(&outm);
            root->MakeMeshInto(&runningMesh);
        } else {
            runningMesh.MakeFromCopyOf(&outm);
        }

        outm.Clear();
        thism.Clear();
        prevm.Clear();
    }

    displayDirty = true;
}

void Group::GenerateDisplayItems() {
    // This is potentially slow (since we've got to triangulate a shell, or
    // to find the emphasized edges for a mesh), so we will run it only
    // if its inputs have changed.
    if(displayDirty) {
        Group *pg = RunningMeshGroup();
        if(pg && thisMesh.IsEmpty() && thisShell.IsEmpty() && type != Type::FILLET) {
            // We don't contribute any new solid model in this group, so our
            // display items are identical to the previous group's; which means
            // that we can just display those, and stop ourselves from
            // recalculating for those every time we get a change in this group.
            //
            // Note that this can end up recursing multiple times (if multiple
            // groups that contribute no solid model exist in sequence), but
            // that's okay.
            pg->GenerateDisplayItems();

            displayMesh.Clear();
            displayMesh.MakeFromCopyOf(&(pg->displayMesh));

            displayOutlines.Clear();
            if(SS.GW.showEdges || SS.GW.showOutlines) {
                displayOutlines.MakeFromCopyOf(&pg->displayOutlines);
            }
        } else {
            // We do contribute new solid model, so we have to triangulate the
            // shell, and edge-find the mesh.
            displayMesh.Clear();
            runningShell.TriangulateInto(&displayMesh);
            STriangle *t;
            for(t = runningMesh.l.First(); t; t = runningMesh.l.NextAfter(t)) {
                STriangle trn = *t;
                Vector n = trn.Normal();
                trn.an = n;
                trn.bn = n;
                trn.cn = n;
                displayMesh.AddTriangle(&trn);
            }

            displayOutlines.Clear();

            if(SS.GW.showEdges || SS.GW.showOutlines) {
                SOutlineList rawOutlines = {};
                if(!runningMesh.l.IsEmpty()) {
                    // Triangle mesh only; no shell or emphasized edges.
                    runningMesh.MakeOutlinesInto(&rawOutlines, EdgeKind::EMPHASIZED);
                } else {
                    displayMesh.MakeOutlinesInto(&rawOutlines, EdgeKind::SHARP);
                }

                PolylineBuilder builder;
                builder.MakeFromOutlines(rawOutlines);
                builder.GenerateOutlines(&displayOutlines);
                rawOutlines.Clear();
            }
        }

        // If we render this mesh, we need to know whether it's transparent,
        // and we'll want all transparent triangles last, to make the depth test
        // work correctly.
        displayMesh.PrecomputeTransparency();

        // Recalculate mass center if needed
        if(SS.centerOfMass.draw && SS.centerOfMass.dirty && h == SS.GW.activeGroup) {
            SS.UpdateCenterOfMass();
        }
        displayDirty = false;
    }
}

Group *Group::PreviousGroup() const {
    Group *prev = nullptr;
    for(auto const &gh : SK.groupOrder) {
        Group *g = SK.GetGroup(gh);
        if(g->h == h) {
            return prev;
        }
        prev = g;
    }
    return nullptr;
}

Group *Group::RunningMeshGroup() const {
    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        return SK.GetGroup(opA)->RunningMeshGroup();
    } else {
        return PreviousGroup();
    }
}

bool Group::IsMeshGroup() {
    switch(type) {
        case Group::Type::EXTRUDE:
        case Group::Type::LATHE:
        case Group::Type::REVOLVE:
        case Group::Type::HELIX:
        case Group::Type::ROTATE:
        case Group::Type::TRANSLATE:
        case Group::Type::FILLET:
            return true;

        default:
            return false;
    }
}

void Group::DrawMesh(DrawMeshAs how, Canvas *canvas) {
    if(!(SS.GW.showShaded ||
         SS.GW.drawOccludedAs != GraphicsWindow::DrawOccludedAs::VISIBLE)) return;

    switch(how) {
        case DrawMeshAs::DEFAULT: {
            // Force the shade color to something dim to not distract from
            // the sketch.
            Canvas::Fill fillFront = {};
            if(!SS.GW.showShaded) {
                fillFront.layer = Canvas::Layer::DEPTH_ONLY;
            }
            if((type == Type::DRAWING_3D || type == Type::DRAWING_WORKPLANE)
               && SS.GW.dimSolidModel) {
                fillFront.color = Style::Color(Style::DIM_SOLID);
            }
            Canvas::hFill hcfFront = canvas->GetFill(fillFront);

            // The back faces are drawn in red; should never seem them, since we
            // draw closed shells, so that's a debugging aid.
            Canvas::hFill hcfBack = {};
            if(SS.drawBackFaces && !displayMesh.isTransparent) {
                Canvas::Fill fillBack = {};
                fillBack.layer = fillFront.layer;
                fillBack.color = RgbaColor::FromFloat(1.0f, 0.1f, 0.1f);
                hcfBack = canvas->GetFill(fillBack);
            } else {
                hcfBack = hcfFront;
            }

            // Draw the shaded solid into the depth buffer for hidden line removal,
            // and if we're actually going to display it, to the color buffer too.
            canvas->DrawMesh(displayMesh, hcfFront, hcfBack);

            // Draw mesh edges, for debugging.
            if(SS.GW.showMesh) {
                Canvas::Stroke strokeTriangle = {};
                strokeTriangle.zIndex = 1;
                strokeTriangle.color  = RgbaColor::FromFloat(0.0f, 1.0f, 0.0f);
                strokeTriangle.width  = 1;
                strokeTriangle.unit   = Canvas::Unit::PX;
                Canvas::hStroke hcsTriangle = canvas->GetStroke(strokeTriangle);
                SEdgeList edges = {};
                for(const STriangle &t : displayMesh.l) {
                    edges.AddEdge(t.a, t.b);
                    edges.AddEdge(t.b, t.c);
                    edges.AddEdge(t.c, t.a);
                }
                canvas->DrawEdges(edges, hcsTriangle);
                edges.Clear();
            }
            break;
        }

        case DrawMeshAs::HOVERED: {
            Canvas::Fill fill = {};
            fill.color   = Style::Color(Style::HOVERED);
            fill.pattern = Canvas::FillPattern::CHECKERED_A;
            fill.zIndex  = 2;
            Canvas::hFill hcf = canvas->GetFill(fill);

            std::vector<uint32_t> faces;
            hEntity he = SS.GW.hover.entity;
            if(he.v != 0 && SK.GetEntity(he)->IsFace()) {
                faces.push_back(he.v);
            }
            canvas->DrawFaces(displayMesh, faces, hcf);
            break;
        }

        case DrawMeshAs::SELECTED: {
            Canvas::Fill fill = {};
            fill.color   = Style::Color(Style::SELECTED);
            fill.pattern = Canvas::FillPattern::CHECKERED_B;
            fill.zIndex  = 1;
            Canvas::hFill hcf = canvas->GetFill(fill);

            std::vector<uint32_t> faces;
            SS.GW.GroupSelection();
            auto const &gs = SS.GW.gs;
            // See also GraphicsWindow::MakeSelected "if(c >= MAX_SELECTABLE_FACES)"
            // and GraphicsWindow::GroupSelection "if(e->IsFace())"
            for(auto &fc : gs.face) {
                faces.push_back(fc.v);
            }
            canvas->DrawFaces(displayMesh, faces, hcf);
            break;
        }
    }
}

void Group::Draw(Canvas *canvas) {
    // Everything here gets drawn whether or not the group is hidden; we
    // can control this stuff independently, with show/hide solids, edges,
    // mesh, etc.

    GenerateDisplayItems();
    DrawMesh(DrawMeshAs::DEFAULT, canvas);

    if(SS.GW.showEdges) {
        Canvas::Stroke strokeEdge = Style::Stroke(Style::SOLID_EDGE);
        strokeEdge.zIndex = 1;
        Canvas::hStroke hcsEdge = canvas->GetStroke(strokeEdge);

        canvas->DrawOutlines(displayOutlines, hcsEdge,
                             SS.GW.showOutlines
                             ? Canvas::DrawOutlinesAs::EMPHASIZED_WITHOUT_CONTOUR
                             : Canvas::DrawOutlinesAs::EMPHASIZED_AND_CONTOUR);

        if(SS.GW.drawOccludedAs != GraphicsWindow::DrawOccludedAs::INVISIBLE) {
            Canvas::Stroke strokeHidden = Style::Stroke(Style::HIDDEN_EDGE);
            if(SS.GW.drawOccludedAs == GraphicsWindow::DrawOccludedAs::VISIBLE) {
                strokeHidden.stipplePattern = StipplePattern::CONTINUOUS;
            }
            strokeHidden.layer  = Canvas::Layer::OCCLUDED;
            Canvas::hStroke hcsHidden = canvas->GetStroke(strokeHidden);

            canvas->DrawOutlines(displayOutlines, hcsHidden,
                                 Canvas::DrawOutlinesAs::EMPHASIZED_AND_CONTOUR);
        }
    }

    if(SS.GW.showOutlines) {
        Canvas::Stroke strokeOutline = Style::Stroke(Style::OUTLINE);
        strokeOutline.zIndex = 1;
        Canvas::hStroke hcsOutline = canvas->GetStroke(strokeOutline);

        canvas->DrawOutlines(displayOutlines, hcsOutline,
                             Canvas::DrawOutlinesAs::CONTOUR_ONLY);
    }
}

void Group::DrawPolyError(Canvas *canvas) {
    const Camera &camera = canvas->GetCamera();

    Canvas::Stroke strokeUnclosed = Style::Stroke(Style::DRAW_ERROR);
    strokeUnclosed.color = strokeUnclosed.color.WithAlpha(50);
    Canvas::hStroke hcsUnclosed = canvas->GetStroke(strokeUnclosed);

    Canvas::Stroke strokeError = Style::Stroke(Style::DRAW_ERROR);
    strokeError.layer = Canvas::Layer::FRONT;
    strokeError.width = 1.0f;
    Canvas::hStroke hcsError = canvas->GetStroke(strokeError);

    double textHeight = Style::DefaultTextHeight() / camera.scale;

    // And finally show the polygons too, and any errors if it's not possible
    // to assemble the lines into closed polygons.
    if(polyError.how == PolyError::NOT_CLOSED) {
        // Report this error only in sketch-in-workplane groups; otherwise
        // it's just a nuisance.
        if(type == Type::DRAWING_WORKPLANE) {
            canvas->DrawVectorText(_("not closed contour, or not all same style!"),
                                   textHeight,
                                   polyError.notClosedAt.b, camera.projRight, camera.projUp,
                                   hcsError);
            canvas->DrawLine(polyError.notClosedAt.a, polyError.notClosedAt.b, hcsUnclosed);
        }
    } else if(polyError.how == PolyError::NOT_COPLANAR ||
              polyError.how == PolyError::SELF_INTERSECTING ||
              polyError.how == PolyError::ZERO_LEN_EDGE) {
        // These errors occur at points, not lines
        if(type == Type::DRAWING_WORKPLANE) {
            const char *msg;
            if(polyError.how == PolyError::NOT_COPLANAR) {
                msg = _("points not all coplanar!");
            } else if(polyError.how == PolyError::SELF_INTERSECTING) {
                msg = _("contour is self-intersecting!");
            } else {
                msg = _("zero-length edge!");
            }
            canvas->DrawVectorText(msg, textHeight,
                                   polyError.errorPointAt, camera.projRight, camera.projUp,
                                   hcsError);
        }
    } else {
        // The contours will get filled in DrawFilledPaths.
    }
}

void Group::DrawFilledPaths(Canvas *canvas) {
    for(const SBezierLoopSet &sbls : bezierLoops.l) {
        if(sbls.l.IsEmpty() || sbls.l[0].l.IsEmpty())
            continue;

        // In an assembled loop, all the styles should be the same; so doesn't
        // matter which one we grab.
        const SBezier *sb = &(sbls.l[0].l[0]);
        Style *s = Style::Get({ (uint32_t)sb->auxA });

        Canvas::Fill fill = {};
        fill.zIndex = 1;
        if(s->filled) {
            // This is a filled loop, where the user specified a fill color.
            fill.color = s->fillColor;
        } else if(h == SS.GW.activeGroup && SS.checkClosedContour &&
                    polyError.how == PolyError::GOOD) {
            // If this is the active group, and we are supposed to check
            // for closed contours, and we do indeed have a closed and
            // non-intersecting contour, then fill it dimly.
            fill.color = Style::Color(Style::CONTOUR_FILL).WithAlpha(127);
        } else continue;
        Canvas::hFill hcf = canvas->GetFill(fill);

        SPolygon sp = {};
        sbls.MakePwlInto(&sp);
        canvas->DrawPolygon(sp, hcf);
        sp.Clear();
    }
}

void Group::DrawContourAreaLabels(Canvas *canvas) {
    const Camera &camera = canvas->GetCamera();
    Vector gr = camera.projRight.ScaledBy(1 / camera.scale);
    Vector gu = camera.projUp.ScaledBy(1 / camera.scale);

    for(SBezierLoopSet &sbls : bezierLoops.l) {
        if(sbls.l.IsEmpty() || sbls.l[0].l.IsEmpty())
            continue;

        Vector min = sbls.l[0].l[0].ctrl[0];
        Vector max = min;
        Vector zero = {};
        sbls.GetBoundingProjd({1.0, 0.0, 0.0}, zero, &min.x, &max.x);
        sbls.GetBoundingProjd({0.0, 1.0, 0.0}, zero, &min.y, &max.y);
        sbls.GetBoundingProjd({0.0, 0.0, 1.0}, zero, &min.z, &max.z);

        Vector mid = min.Plus(max).ScaledBy(0.5);

        hStyle hs = { Style::CONSTRAINT };
        Canvas::Stroke stroke = Style::Stroke(hs);
        stroke.layer = Canvas::Layer::FRONT;

        std::string label = SS.MmToStringSI(fabs(sbls.SignedArea()), /*dim=*/2);
        double fontHeight = Style::TextHeight(hs);
        double textWidth  = VectorFont::Builtin()->GetWidth(fontHeight, label),
               textHeight = VectorFont::Builtin()->GetCapHeight(fontHeight);
        Vector pos = mid.Minus(gr.ScaledBy(textWidth / 2.0))
                        .Minus(gu.ScaledBy(textHeight / 2.0));
        canvas->DrawVectorText(label, fontHeight, pos, gr, gu, canvas->GetStroke(stroke));
    }
}

} // namespace SolveSpace
