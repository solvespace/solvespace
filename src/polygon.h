//-----------------------------------------------------------------------------
// Anything relating to plane polygons and triangles, and (generally, non-
// planar) meshes thereof.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef __POLYGON_H
#define __POLYGON_H

class SPointList;
class SPolygon;
class SContour;
class SMesh;
class SBsp3;
class SOutlineList;

enum class EarType : uint32_t {
    UNKNOWN = 0,
    NOT_EAR = 1,
    EAR     = 2
};

enum class BspClass : uint32_t {
    POS         = 100,
    NEG         = 101,
    COPLANAR    = 200
};

enum class EdgeKind : uint32_t {
    NAKED_OR_SELF_INTER  = 100,
    SELF_INTER           = 200,
    TURNING              = 300,
    EMPHASIZED           = 400,
    SHARP                = 500,
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
    void AddEdge(Vector a, Vector b, int auxA=0, int auxB=0);
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

typedef struct {
    uint32_t face;
    RgbaColor color;
} STriMeta;

class SPolygon {
public:
    List<SContour>  l;
    Vector          normal;

    Vector ComputeNormal();
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
};

class STriangle {
public:
    int         tag;
    STriMeta    meta;

    union {
        struct { Vector a, b, c; };
        Vector vertices[3];
    };

    union {
        struct { Vector an, bn, cn; };
        Vector normals[3];
    };

    static STriangle From(STriMeta meta, Vector a, Vector b, Vector c);
    Vector Normal() const;
    void FlipNormal();
    double MinAltitude() const;
    int WindingNumberForPoint(Vector p) const;
    bool ContainsPoint(Vector p) const;
    bool ContainsPointProjd(Vector n, Vector p) const;
};

class SBsp2 {
public:
    Vector      np;     // normal to the plane

    Vector      no;     // outer normal to the edge
    double      d;
    SEdge       edge;

    SBsp2       *pos;
    SBsp2       *neg;

    SBsp2       *more;

    void InsertTriangleHow(BspClass how, STriangle *tr, SMesh *m, SBsp3 *bsp3);
    void InsertTriangle(STriangle *tr, SMesh *m, SBsp3 *bsp3);
    Vector IntersectionWith(Vector a, Vector b) const;
    void InsertEdge(SEdge *nedge, Vector nnp, Vector out);
    static SBsp2 *InsertOrCreateEdge(SBsp2 *where, SEdge *nedge,
                                     Vector nnp, Vector out);
    static SBsp2 *Alloc();
};

class SBsp3 {
public:
    Vector      n;
    double      d;

    STriangle   tri;
    SBsp3       *pos;
    SBsp3       *neg;

    SBsp3       *more;

    SBsp2       *edges;

    static SBsp3 *Alloc();
    static SBsp3 *FromMesh(SMesh *m);

    Vector IntersectionWith(Vector a, Vector b) const;

    void InsertHow(BspClass how, STriangle *str, SMesh *instead);
    void Insert(STriangle *str, SMesh *instead);
    static SBsp3 *InsertOrCreate(SBsp3 *where, STriangle *str, SMesh *instead);

    void InsertConvexHow(BspClass how, STriMeta meta, Vector *vertex, int n,
                                SMesh *instead);
    SBsp3 *InsertConvex(STriMeta meta, Vector *vertex, int n, SMesh *instead);

    void InsertInPlane(bool pos2, STriangle *tr, SMesh *m);

    void GenerateInPaintOrder(SMesh *m) const;
};

class SMesh {
public:
    List<STriangle>     l;

    bool    flipNormal;
    bool    keepCoplanar;
    bool    atLeastOneDiscarded;
    bool    isTransparent;

    void Clear();
    void AddTriangle(const STriangle *st);
    void AddTriangle(STriMeta meta, Vector a, Vector b, Vector c);
    void AddTriangle(STriMeta meta, Vector n,
                     Vector a, Vector b, Vector c);
    void DoBounding(Vector v, Vector *vmax, Vector *vmin) const;
    void GetBounding(Vector *vmax, Vector *vmin) const;

    void Simplify(int start);

    void AddAgainstBsp(SMesh *srcm, SBsp3 *bsp3);
    void MakeFromUnionOf(SMesh *a, SMesh *b);
    void MakeFromDifferenceOf(SMesh *a, SMesh *b);

    void MakeFromCopyOf(SMesh *a);
    void MakeFromTransformationOf(SMesh *a, Vector trans,
                                  Quaternion q, double scale);
    void MakeFromAssemblyOf(SMesh *a, SMesh *b);

    void MakeEdgesInPlaneInto(SEdgeList *sel, Vector n, double d);
    void MakeCertainEdgesAndOutlinesInto(SEdgeList *sel, SOutlineList *sol, EdgeKind type);

    bool IsEmpty() const;
    void RemapFaces(Group *g, int remap);

    uint32_t FirstIntersectionWith(Point2d mp) const;
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
    void AddEdge(Vector a, Vector b, Vector nl, Vector nr);

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
    void MakeOutlinesInto(SOutlineList *sel) const;

    void OcclusionTestLine(SEdge orig, SEdgeList *sel, int cnt, bool removeHidden) const;
    void SplitLinesAgainstTriangle(SEdgeList *sel, STriangle *tr, bool removeHidden) const;

    void SnapToMesh(SMesh *m);
    void SnapToVertex(Vector v, SMesh *extras);
};

#endif

