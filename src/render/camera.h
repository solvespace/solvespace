//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_CAMERA_H
#define SOLVESPACE_CAMERA_H

#include "dsc.h"
#include "srf/sbezier.h"

namespace SolveSpace{

// A mapping from 3d sketch coordinates to 2d screen coordinates, using
// an axonometric projection.
class Camera {
public:
    double      width;
    double      height;
    double      pixelRatio;
    bool        gridFit;
    Vector      offset;
    Vector      projRight;
    Vector      projUp;
    double      scale;
    double      tangent;

    bool IsPerspective() const { return tangent != 0.0; }

    Point2d ProjectPoint(Vector p) const;
    Vector ProjectPoint3(Vector p) const;
    Vector ProjectPoint4(Vector p, double *w) const;
    Vector UnProjectPoint(Point2d p) const;
    Vector UnProjectPoint3(Vector p) const;
    Vector VectorFromProjs(Vector rightUpForward) const;
    Vector AlignToPixelGrid(Vector v) const;

    SBezier ProjectBezier(SBezier b) const;

    void LoadIdentity();
    void NormalizeProjectionVectors();
};

}

#endif //SOLVESPACE_CAMERA_H
