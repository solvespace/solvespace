//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_SEDGE_H
#define SOLVESPACE_SEDGE_H

#include "dsc.h"
#include "list.h"

namespace SolveSpace {

// TODO: reverse the functions that use these
class SPolygon;
class SContour;

enum class EarType : uint32_t {
    UNKNOWN = 0,
    NOT_EAR = 1,
    EAR     = 2
};

enum class EdgeKind : uint32_t {
    NAKED_OR_SELF_INTER  = 100,
    SELF_INTER           = 200,
    TURNING              = 300,
    EMPHASIZED           = 400,
    SHARP                = 500,
};

class SPoint {
public:
    int     tag;

    EarType ear;

    Vector  p;
    Vector  auxv;
};

class SPointList {
public:
    List<SPoint>    l;

    void Clear();
    bool ContainsPoint(Vector pt) const;
    int IndexForPoint(Vector pt) const;
    void IncrementTagFor(Vector pt);
    void Add(Vector pt);
};

class SEdge {
public:
    int    tag;
    int    auxA, auxB;
    Vector a, b;

    static SEdge From(Vector a, Vector b);
    bool EdgeCrosses(Vector a, Vector b, Vector *pi=NULL, SPointList *spl=NULL) const;
};

class SEdgeList {
public:
    List<SEdge>     l;

    void Clear();
    void AddEdge(Vector a, Vector b, int auxA=0, int auxB=0, int tag=0);
    bool AssemblePolygon(SPolygon *dest, SEdge *errorAt, bool keepDir=false) const;
    bool AssembleContour(Vector first, Vector last, SContour *dest,
                         SEdge *errorAt, bool keepDir) const;
    int AnyEdgeCrossings(Vector a, Vector b,
                         Vector *pi=NULL, SPointList *spl=NULL) const;
    bool ContainsEdgeFrom(const SEdgeList *sel) const;
    bool ContainsEdge(const SEdge *se) const;
    void CullExtraneousEdges();
    void MergeCollinearSegments(Vector a, Vector b);
};

// A kd-tree element needs to go on a side of a node if it's when KDTREE_EPS
// of the boundary. So increasing this number never breaks anything, but may
// result in more duplicated elements. So it's conservative to be sloppy here.
#define KDTREE_EPS (20*LENGTH_EPS)

class SEdgeLl {
public:
    SEdge       *se;
    SEdgeLl     *next;

    static SEdgeLl *Alloc();
};

class SKdNodeEdges {
public:
    int which; // whether c is x, y, or z
    double c;
    SKdNodeEdges    *gt;
    SKdNodeEdges    *lt;

    SEdgeLl         *edges;

    static SKdNodeEdges *From(SEdgeList *sel);
    static SKdNodeEdges *From(SEdgeLl *sell);
    static SKdNodeEdges *Alloc();
    int AnyEdgeCrossings(Vector a, Vector b, int cnt,
                         Vector *pi=NULL, SPointList *spl=NULL) const;
};


}

#endif //SOLVESPACE_SEDGE_H
