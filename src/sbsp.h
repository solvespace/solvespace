//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_SBSP_H
#define SOLVESPACE_SBSP_H

#include <stdint.h>
#include "dsc.h"
#include "sedge.h"
#include "striangle.h"
#include "smesh.h"

namespace SolveSpace {

enum class BspClass : uint32_t {
    POS         = 100,
    NEG         = 101,
    COPLANAR    = 200
};

class SBsp3;

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
    static SBsp3 *FromMesh(const SMesh *m);

    Vector IntersectionWith(Vector a, Vector b) const;

    void InsertHow(BspClass how, STriangle *str, SMesh *instead);
    void Insert(STriangle *str, SMesh *instead);
    static SBsp3 *InsertOrCreate(SBsp3 *where, STriangle *str, SMesh *instead);

    void InsertConvexHow(BspClass how, STriMeta meta, Vector *vertex, size_t n,
                         SMesh *instead);
    SBsp3 *InsertConvex(STriMeta meta, Vector *vertex, size_t n, SMesh *instead);

    void InsertInPlane(bool pos2, STriangle *tr, SMesh *m);

    void GenerateInPaintOrder(SMesh *m) const;
};

}

#endif //SOLVESPACE_SBSP_H
