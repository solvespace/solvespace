
#ifndef __POLYGON_H
#define __POLYGON_H

class SPolygon;
class SContour;
class SMesh;
class SBsp3;

class SEdge {
public:
    int    tag;
    int    auxA, auxB;
    Vector a, b;

    static SEdge From(Vector a, Vector b);
};

class SEdgeList {
public:
    List<SEdge>     l;

    void Clear(void);
    void AddEdge(Vector a, Vector b, int auxA=0, int auxB=0);
    bool AssemblePolygon(SPolygon *dest, SEdge *errorAt, bool keepDir=false);
    bool AssembleContour(Vector first, Vector last, SContour *dest,
                            SEdge *errorAt, bool keepDir);
    int AnyEdgeCrossings(Vector a, Vector b, Vector *pi=NULL);
    void CullExtraneousEdges(void);
    void MergeCollinearSegments(Vector a, Vector b);
};

class SPoint {
public:
    int     tag;

    static const int UNKNOWN = 0;
    static const int NOT_EAR = 1;
    static const int EAR     = 2;
    int     ear;

    Vector  p;
};

class SContour {
public:
    int             tag;
    int             timesEnclosed;
    Vector          xminPt;
    List<SPoint>    l;

    void AddPoint(Vector p);
    void MakeEdgesInto(SEdgeList *el);
    void Reverse(void);
    Vector ComputeNormal(void);
    bool IsClockwiseProjdToNormal(Vector n);
    bool ContainsPointProjdToNormal(Vector n, Vector p);
    bool AllPointsInPlane(Vector n, double d, Vector *notCoplanarAt);
    void OffsetInto(SContour *dest, double r);
    void CopyInto(SContour *dest);
    void FindPointWithMinX(void);

    bool IsEar(int bp);
    bool BridgeToContour(SContour *sc, SEdgeList *el, List<Vector> *vl);
    void ClipEarInto(SMesh *m, int bp);
    void UvTriangulateInto(SMesh *m, SSurface *srf);
};

typedef struct {
    DWORD   face;
    int     color;
} STriMeta;

class SPolygon {
public:
    List<SContour>  l;
    Vector          normal;

    Vector ComputeNormal(void);
    void AddEmptyContour(void);
    int WindingNumberForPoint(Vector p);
    bool ContainsPoint(Vector p);
    void MakeEdgesInto(SEdgeList *el);
    void FixContourDirections(void);
    void TriangulateInto(SMesh *m);
    void TriangulateInto(SMesh *m, STriMeta meta);
    void Clear(void);
    bool AllPointsInPlane(Vector *notCoplanarAt);
    bool SelfIntersecting(Vector *intersectsAt);
    bool IsEmpty(void);
    Vector AnyPoint(void);
    void OffsetInto(SPolygon *dest, double r);
    void UvTriangulateInto(SMesh *m, SSurface *srf);
    void UvGridTriangulateInto(SMesh *m, SSurface *srf);
};

class STriangle {
public:
    int         tag;
    STriMeta    meta;
    Vector      a, b, c;
    Vector      an, bn, cn;

    static STriangle From(STriMeta meta, Vector a, Vector b, Vector c);
    Vector Normal(void);
    void FlipNormal(void);
    double MinAltitude(void);
    int WindingNumberForPoint(Vector p);
    bool ContainsPoint(Vector p);
    bool ContainsPointProjd(Vector n, Vector p);
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

    static const int POS = 100, NEG = 101, COPLANAR = 200;
    void InsertTriangleHow(int how, STriangle *tr, SMesh *m, SBsp3 *bsp3);
    void InsertTriangle(STriangle *tr, SMesh *m, SBsp3 *bsp3);
    Vector IntersectionWith(Vector a, Vector b);
    SBsp2 *InsertEdge(SEdge *nedge, Vector nnp, Vector out);
    static SBsp2 *Alloc(void);

    void DebugDraw(Vector n, double d);
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

    static SBsp3 *Alloc(void);
    static SBsp3 *FromMesh(SMesh *m);

    Vector IntersectionWith(Vector a, Vector b);

    static const int POS = 100, NEG = 101, COPLANAR = 200;
    void InsertHow(int how, STriangle *str, SMesh *instead);
    SBsp3 *Insert(STriangle *str, SMesh *instead);

    void InsertConvexHow(int how, STriMeta meta, Vector *vertex, int n,
                                SMesh *instead);
    SBsp3 *InsertConvex(STriMeta meta, Vector *vertex, int n, SMesh *instead);

    void InsertInPlane(bool pos2, STriangle *tr, SMesh *m);

    void GenerateInPaintOrder(SMesh *m);

    void DebugDraw(void);
};

class SMesh {
public:
    List<STriangle>     l;

    bool    flipNormal;
    bool    keepCoplanar;
    bool    atLeastOneDiscarded;

    void Clear(void);
    void AddTriangle(STriangle *st);
    void AddTriangle(STriMeta meta, Vector a, Vector b, Vector c);
    void AddTriangle(STriMeta meta, Vector n, Vector a, Vector b, Vector c);
    void DoBounding(Vector v, Vector *vmax, Vector *vmin);
    void GetBounding(Vector *vmax, Vector *vmin);

    void Simplify(int start);

    void AddAgainstBsp(SMesh *srcm, SBsp3 *bsp3);
    void MakeFromUnionOf(SMesh *a, SMesh *b);
    void MakeFromDifferenceOf(SMesh *a, SMesh *b);

    void MakeFromCopyOf(SMesh *a);
    void MakeFromTransformationOf(SMesh *a, Vector trans, Quaternion q);
    void MakeFromAssemblyOf(SMesh *a, SMesh *b);

    void MakeEdgesInPlaneInto(SEdgeList *sel, Vector n, double d);
    void MakeEmphasizedEdgesInto(SEdgeList *sel);

    bool IsEmpty(void);
    void RemapFaces(Group *g, int remap);

    DWORD FirstIntersectionWith(Point2d mp);
};

// A linked list of triangles
class STriangleLl {
public:
    int             tag;
    STriangle       *tri;

    STriangleLl     *next;

    static STriangleLl *Alloc(void);
};

class SKdNode {
public:
    static const int BY_X = 0;
    static const int BY_Y = 1;
    static const int BY_Z = 2;
    int which;
    double c;

    SKdNode      *gt;
    SKdNode      *lt;

    STriangleLl  *tris;

    static SKdNode *Alloc(void);
    static SKdNode *From(SMesh *m);
    static SKdNode *From(STriangleLl *tll, int which);

    void AddTriangle(STriangle *tr);
    void MakeMeshInto(SMesh *m);
    void ClearTags(void);

    void FindEdgeOn(Vector a, Vector b, int *n, int cnt, bool coplanarIsInter,
                                                bool *inter, bool *fwd,
                                                DWORD *face);
    static const int NAKED_OR_SELF_INTER_EDGES  = 100;
    static const int SELF_INTER_EDGES           = 200;
    static const int TURNING_EDGES              = 300;
    static const int EMPHASIZED_EDGES           = 400;
    void MakeCertainEdgesInto(SEdgeList *sel, int how, bool coplanarIsInter,
                                                bool *inter, bool *leaky);

    void OcclusionTestLine(SEdge orig, SEdgeList *sel, int cnt);
    void SplitLinesAgainstTriangle(SEdgeList *sel, STriangle *tr);

    void SnapToMesh(SMesh *m);
    void SnapToVertex(Vector v, SMesh *extras);
};

#endif

