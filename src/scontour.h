//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_SCONTOUR_H
#define SOLVESPACE_SCONTOUR_H

#include "sedge.h"
#include "smesh.h"

namespace SolveSpace{

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

}

#endif //SOLVESPACE_SCONTOUR_H
