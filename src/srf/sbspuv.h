//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_SBSPUV_H
#define SOLVESPACE_SBSPUV_H

#include "dsc.h"
#include "sedge.h"

namespace SolveSpace{
// todo: get rid of this circular dependency
class SSurface;

// Utility data structure, a two-dimensional BSP to accelerate polygon
// operations.
class SBspUv {
public:
    Point2d  a, b;

    SBspUv  *pos;
    SBspUv  *neg;

    SBspUv  *more;

    enum class Class : uint32_t {
        INSIDE            = 100,
        OUTSIDE           = 200,
        EDGE_PARALLEL     = 300,
        EDGE_ANTIPARALLEL = 400,
        EDGE_OTHER        = 500
    };

    static SBspUv *Alloc();
    static SBspUv *From(SEdgeList *el, SSurface *srf);

    void ScalePoints(Point2d *pt, Point2d *a, Point2d *b, SSurface *srf) const;
    double ScaledSignedDistanceToLine(Point2d pt, Point2d a, Point2d b,
                                      SSurface *srf) const;
    double ScaledDistanceToLine(Point2d pt, Point2d a, Point2d b, bool asSegment,
                                SSurface *srf) const;

    void InsertEdge(Point2d a, Point2d b, SSurface *srf);
    static SBspUv *InsertOrCreateEdge(SBspUv *where, Point2d ea, Point2d eb, SSurface *srf);
    Class ClassifyPoint(Point2d p, Point2d eb, SSurface *srf) const;
    Class ClassifyEdge(Point2d ea, Point2d eb, SSurface *srf) const;
    double MinimumDistanceToEdge(Point2d p, SSurface *srf) const;
};

}

#endif //SOLVESPACE_SBSPUV_H
