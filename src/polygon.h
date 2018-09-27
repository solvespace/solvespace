//-----------------------------------------------------------------------------
// Anything relating to plane polygons and triangles, and (generally, non-
// planar) meshes thereof.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_POLYGON_H
#define SOLVESPACE_POLYGON_H

#include <stdint.h>
#include <unordered_map>
#include "dsc.h"
#include "list.h"
#include "srf/surface.h"
#include "sedge.h"
#include "striangle.h"
#include "smesh.h"
#include "sbsp.h"
#include "soutline.h"
#include "scontour.h"
#include "spolygon.h"

namespace SolveSpace {

// A linked list of triangles
class STriangleLl {
public:
    STriangle       *tri;

    STriangleLl     *next;

    static STriangleLl *Alloc();
};

class SKdNode {
public:
    struct EdgeOnInfo {
        int        count;
        bool       frontFacing;
        bool       intersectsMesh;
        STriangle *tr;
        int        ai;
        int        bi;
    };

    int which;  // whether c is x, y, or z
    double c;

    SKdNode      *gt;
    SKdNode      *lt;

    STriangleLl  *tris;

    static SKdNode *Alloc();
    static SKdNode *From(SMesh *m);
    static SKdNode *From(STriangleLl *tll);

    void AddTriangle(STriangle *tr);
    void MakeMeshInto(SMesh *m) const;
    void ListTrianglesInto(std::vector<STriangle *> *tl) const;
    void ClearTags() const;

    void FindEdgeOn(Vector a, Vector b, int cnt, bool coplanarIsInter, EdgeOnInfo *info) const;
    void MakeCertainEdgesInto(SEdgeList *sel, EdgeKind how, bool coplanarIsInter,
                              bool *inter, bool *leaky, int auxA = 0) const;
    void MakeOutlinesInto(SOutlineList *sel, EdgeKind tagKind) const;

    void OcclusionTestLine(SEdge orig, SEdgeList *sel, int cnt) const;
    void SplitLinesAgainstTriangle(SEdgeList *sel, STriangle *tr) const;

    void SnapToMesh(SMesh *m);
    void SnapToVertex(Vector v, SMesh *extras);
};

class PolylineBuilder {
public:
    struct Edge;

    struct Vertex {
        Vector              pos;
        std::vector<Edge *> edges;

        bool GetNext(uint32_t kind, Vertex **next, Edge **nextEdge);
        bool GetNext(uint32_t kind, Vector plane, double d, Vertex **next, Edge **nextEdge);
        size_t CountEdgesWithTagAndKind(int tag, uint32_t kind) const;
    };

    struct VertexPairHash {
        size_t operator()(const std::pair<Vertex *, Vertex *> &v) const;
    };

    struct Edge {
        Vertex   *a;
        Vertex   *b;
        uint32_t  kind;
        int       tag;

        union {
            uintptr_t  data;
            SOutline  *outline;
            SEdge     *edge;
        };

        Vertex *GetOtherVertex(Vertex *v) const;
        bool GetStartAndNext(Vertex **start, Vertex **next, bool loop) const;
    };

    std::unordered_map<Vector, Vertex *, VectorHash, VectorPred> vertices;
    std::unordered_map<std::pair<Vertex *, Vertex *>, Edge *, VertexPairHash> edgeMap;
    std::vector<Edge *> edges;

    ~PolylineBuilder();
    void Clear();

    Vertex *AddVertex(const Vector &pos);
    Edge *AddEdge(const Vector &p0, const Vector &p1, uint32_t kind, uintptr_t data = 0);
    void Generate(
        std::function<void(Vertex *start, Vertex *next, Edge *edge)> startFunc,
        std::function<void(Vertex *next, Edge *edge)> nextFunc,
        std::function<void(Edge *)> aloneFunc,
        std::function<void()> endFunc = [](){});

    void MakeFromEdges(const SEdgeList &sel);
    void MakeFromOutlines(const SOutlineList &sol);
    void GenerateEdges(SEdgeList *sel);
    void GenerateOutlines(SOutlineList *sol);
};

}

#endif

