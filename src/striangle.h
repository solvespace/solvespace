//
// Created by benjamin on 9/26/18.
//

#ifndef SOLVESPACE_STRIANGLE_H
#define SOLVESPACE_STRIANGLE_H

#include <stdint.h>
#include "dsc.h"

namespace SolveSpace {

typedef struct {
    uint32_t face;
    RgbaColor color;
} STriMeta;

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
    STriangle Transform(Vector o, Vector u, Vector v) const;
    bool Raytrace(const Vector &rayPoint, const Vector &rayDir,
                  double *t, Vector *inters) const;
    double SignedVolume() const;
    bool IsDegenerate() const;
};

}

#endif //SOLVESPACE_STRIANGLE_H
