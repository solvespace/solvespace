//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_SPOLYGON_H
#define SOLVESPACE_SPOLYGON_H

#include "scontour.h"

namespace SolveSpace{

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

}

#endif //SOLVESPACE_SPOLYGON_H
