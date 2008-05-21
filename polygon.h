
#ifndef __POLYGON_H
#define __POLYGON_H

class SPolygon;

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
};

class SEdge {
public:
    int    tag;
    Vector a, b;
};

class SEdgeList {
public:
    SList<SEdge>    l;

    bool AssemblePolygon(SPolygon *dest, SEdge *errorAt);
    void CopyBreaking(SEdgeList *dest);
    static const int UNION = 0, DIFF = 1, INTERSECT = 2;
    bool BooleanOp(int op, bool inA, bool inB);
    void CullForBoolean(int op, SPolygon *a, SPolygon *b);
    void CullDuplicates(void);
};

class SPoint {
public:
    int     tag;
    Vector  p;
};

class SContour {
public:
    SList<SPoint>   l;

    void MakeEdgesInto(SEdgeList *el);
    void Reverse(void);
    Vector ComputeNormal(void);
    bool IsClockwiseProjdToNormal(Vector n);
    bool ContainsPointProjdToNormal(Vector n, Vector p);
    void IntersectAgainstPlane(double *inter, int *inters,
                               Vector u, Vector v, double vp);
};

class SPolygon {
public:
    SList<SContour> l;
    Vector          normal;

    Vector ComputeNormal(void);
    void AddEmptyContour(void);
    void AddPoint(Vector p);
    bool ContainsPoint(Vector p);
    void MakeEdgesInto(SEdgeList *el);
    void FixContourDirections(void);
    void Clear(void);
};

class STriangle {
public:
    Vector a, b, c;
};

class SBsp2 {
    SEdge       edge;
};

class SBsp1d {
    SEdge       edge;
};

class SBsp3 {
public:
    STriangle   tri;
    SBsp3       *pos;
    SBsp3       *neg;

    SBsp3       *more;

    SBsp2       *edges;
};


class SMesh {
public:
    SList<STriangle>    l;
};

#endif

