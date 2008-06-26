
#ifndef __POLYGON_H
#define __POLYGON_H

class SPolygon;
class SContour;
class SMesh;
class SBsp3;

template <class T>
class SList {
public:
    T   *elem;
    int  n;
    int  elemsAllocated;

    void Add(T *t) {
        if(n >= elemsAllocated) {
            elemsAllocated = (elemsAllocated + 32)*2;
            elem = (T *)MemRealloc(elem, elemsAllocated*sizeof(elem[0]));
        }
        elem[n++] = *t;
    }

    void ClearTags(void) {
        int i;
        for(i = 0; i < n; i++) {
            elem[i].tag = 0;
        }
    }

    void Clear(void) {
        if(elem) MemFree(elem);
        elem = NULL;
        n = elemsAllocated = 0;
    }

    void RemoveTagged(void) {
        int src, dest;
        dest = 0;
        for(src = 0; src < n; src++) {
            if(elem[src].tag) {
                // this item should be deleted
            } else {
                if(src != dest) {
                    elem[dest] = elem[src];
                }
                dest++;
            }
        }
        n = dest;
        // and elemsAllocated is untouched, because we didn't resize
    }
};

class SEdge {
public:
    int    tag;
    Vector a, b;

    static SEdge From(Vector a, Vector b);
};

class SEdgeList {
public:
    SList<SEdge>    l;

    void Clear(void);
    void AddEdge(Vector a, Vector b);
    bool AssemblePolygon(SPolygon *dest, SEdge *errorAt);
    bool AssembleContour(Vector first, Vector last, SContour *dest,
                                                        SEdge *errorAt);
};

class SPoint {
public:
    int     tag;
    Vector  p;
};

typedef struct {
    DWORD   face;
    int     color;
} STriMeta;

class SContour {
public:
    int             tag;
    SList<SPoint>   l;

    void AddPoint(Vector p);
    void MakeEdgesInto(SEdgeList *el);
    bool VertexIsEar(int v, Vector normal);
    void TriangulateInto(SMesh *m, STriMeta meta, Vector normal);
    void Reverse(void);
    Vector ComputeNormal(void);
    bool IsClockwiseProjdToNormal(Vector n);
    bool ContainsPointProjdToNormal(Vector n, Vector p);
    bool AllPointsInPlane(Vector n, double d, Vector *notCoplanarAt);
};

class SPolygon {
public:
    SList<SContour> l;
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
    bool IsEmpty(void);
    bool IntersectsPolygon(Vector a, Vector b);
    bool VisibleVertices(SContour *outer, SContour *inner,
                         SEdgeList *extras, int *vo, int *vi);
    Vector AnyPoint(void);
};

class STriangle {
public:
    int         tag;
    STriMeta    meta;
    Vector      a, b, c;

    static STriangle From(STriMeta meta, Vector a, Vector b, Vector c);
    Vector Normal(void);
    void FlipNormal(void);
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

    void DebugDraw(void);
};

class SMesh {
public:
    SList<STriangle>    l;

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
    void MakeFromUnion(SMesh *a, SMesh *b);
    void MakeFromDifference(SMesh *a, SMesh *b);
    bool MakeFromInterferenceCheck(SMesh *srca, SMesh *srcb, SMesh *errorAt);
    void MakeFromCopy(SMesh *a);

    DWORD FirstIntersectionWith(Point2d mp);
};

// A linked list of triangles
class STriangleLl {
public:
    int             tag;
    STriangle       tri;

    STriangleLl     *next;
};

// A linked list of linked lists of triangles; extra layer of encapsulation
// required because the same triangle might appear in both branches of the
// tree, if it spans the split plane, and we will need to be able to split
// the triangle into multiple pieces as we remove T intersections.
class STriangleLl2 {
public:
    STriangleLl     *trl;

    STriangleLl2    *next;
};

class SKdTree {
public:
    static const int BY_X = 0;
    static const int BY_Y = 1;
    static const int BY_Z = 2;
    int which;
    double c;

    SKdTree      *gt;
    SKdTree      *lt;

    STriangleLl2 *tris;
};

#endif

