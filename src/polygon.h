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

namespace SolveSpace {

class SContour {
public:
    int             tag;
    int             timesEnclosed;
    Vector          xminPt;
    List<SPoint>    l;

    void AddPoint(Vector p);
    void MakeEdgesInto(SEdgeList *el) const;
    void Reverse();
    Vector ComputeNormal() const;
    double SignedAreaProjdToNormal(Vector n) const;
    bool IsClockwiseProjdToNormal(Vector n) const;
    bool ContainsPointProjdToNormal(Vector n, Vector p) const;
    void OffsetInto(SContour *dest, double r) const;
    void CopyInto(SContour *dest) const;
    void FindPointWithMinX();
    Vector AnyEdgeMidpoint() const;

    bool IsEar(int bp, double scaledEps) const;
    bool BridgeToContour(SContour *sc, SEdgeList *el, List<Vector> *vl);
    void ClipEarInto(SMesh *m, int bp, double scaledEps);
    void UvTriangulateInto(SMesh *m, SSurface *srf);
};

class SPolygon {
public:
    List<SContour>  l;
    Vector          normal;

    Vector ComputeNormal() const;
    void AddEmptyContour();
    int WindingNumberForPoint(Vector p) const;
    double SignedArea() const;
    bool ContainsPoint(Vector p) const;
    void MakeEdgesInto(SEdgeList *el) const;
    void FixContourDirections();
    void Clear();
    bool SelfIntersecting(Vector *intersectsAt) const;
    bool IsEmpty() const;
    Vector AnyPoint() const;
    void OffsetInto(SPolygon *dest, double r) const;
    void UvTriangulateInto(SMesh *m, SSurface *srf);
    void UvGridTriangulateInto(SMesh *m, SSurface *srf);
    void TriangulateInto(SMesh *m) const;
    void InverseTransformInto(SPolygon *sp, Vector u, Vector v, Vector n) const;
};

// A linked list of triangles
class STriangleLl {
public:
    STriangle       *tri;

    STriangleLl     *next;

    static STriangleLl *Alloc();
};

class SOutline {
public:
    int    tag;
    Vector a, b, nl, nr;

    bool IsVisible(Vector projDir) const;
};

class SOutlineList {
public:
    List<SOutline> l;

    void Clear();
    void AddEdge(Vector a, Vector b, Vector nl, Vector nr, int tag = 0);
    void ListTaggedInto(SEdgeList *el, int auxA = 0, int auxB = 0);

    void MakeFromCopyOf(SOutlineList *ol);
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

