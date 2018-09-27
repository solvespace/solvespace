//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_SBEZIER_H
#define SOLVESPACE_SBEZIER_H

#include "dsc.h"
#include "sedge.h"
#include "scurvept.h"

// Stuff for rational polynomial curves, of degree one to three. These are
// our inputs, and are also calculated for certain exact surface-surface
// intersections.

namespace SolveSpace{

// todo: get rid of this circular dependency
class SSurface;

class SBezierList;
class SBezier {
public:
    int             tag;
    int             auxA, auxB;

    int             deg;
    Vector          ctrl[4];
    double          weight[4];
    uint32_t        entity;

    Vector PointAt(double t) const;
    Vector TangentAt(double t) const;
    void ClosestPointTo(Vector p, double *t, bool mustConverge=true) const;
    void SplitAt(double t, SBezier *bef, SBezier *aft) const;
    bool PointOnThisAndCurve(const SBezier *sbb, Vector *p) const;

    Vector Start() const;
    Vector Finish() const;
    bool Equals(SBezier *b) const;
    void MakePwlInto(SEdgeList *sel, double chordTol=0) const;
    void MakePwlInto(List<SCurvePt> *l, double chordTol=0) const;
    void MakePwlInto(SContour *sc, double chordTol=0) const;
    void MakePwlInto(List<Vector> *l, double chordTol=0) const;
    void MakePwlWorker(List<Vector> *l, double ta, double tb, double chordTol) const;
    void MakePwlInitialWorker(List<Vector> *l, double ta, double tb, double chordTol) const;
    void MakeNonrationalCubicInto(SBezierList *bl, double tolerance, int depth = 0) const;

    void AllIntersectionsWith(const SBezier *sbb, SPointList *spl) const;
    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax) const;
    void Reverse();

    bool IsInPlane(Vector n, double d) const;
    bool IsCircle(Vector axis, Vector *center, double *r) const;
    bool IsRational() const;

    SBezier TransformedBy(Vector t, Quaternion q, double scale) const;
    SBezier InPerspective(Vector u, Vector v, Vector n,
                          Vector origin, double cameraTan) const;
    void ScaleSelfBy(double s);

    static SBezier From(Vector p0, Vector p1, Vector p2, Vector p3);
    static SBezier From(Vector p0, Vector p1, Vector p2);
    static SBezier From(Vector p0, Vector p1);
    static SBezier From(Vector4 p0, Vector4 p1, Vector4 p2, Vector4 p3);
    static SBezier From(Vector4 p0, Vector4 p1, Vector4 p2);
    static SBezier From(Vector4 p0, Vector4 p1);
};

class SBezierList {
public:
    List<SBezier>   l;

    void Clear();
    void ScaleSelfBy(double s);
    void CullIdenticalBeziers();
    void AllIntersectionsWith(SBezierList *sblb, SPointList *spl) const;
    bool GetPlaneContainingBeziers(Vector *p, Vector *u, Vector *v,
                                   Vector *notCoplanarAt) const;
};

class SBezierLoop {
public:
    int             tag;
    List<SBezier>   l;

    inline void Clear() { l.Clear(); }
    bool IsClosed() const;
    void Reverse();
    void MakePwlInto(SContour *sc, double chordTol=0) const;
    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax) const;

    static SBezierLoop FromCurves(SBezierList *spcl,
                                  bool *allClosed, SEdge *errorAt);
};

class SBezierLoopSet {
public:
    List<SBezierLoop> l;
    Vector normal;
    Vector point;
    double area;

    static SBezierLoopSet From(SBezierList *spcl, SPolygon *poly,
                               double chordTol,
                               bool *allClosed, SEdge *errorAt,
                               SBezierList *openContours);

    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax) const;
    double SignedArea();
    void MakePwlInto(SPolygon *sp) const;
    void Clear();
};

class SBezierLoopSetSet {
public:
    List<SBezierLoopSet>    l;

    void FindOuterFacesFrom(SBezierList *sbl, SPolygon *spxyz, SSurface *srfuv,
                            double chordTol,
                            bool *allClosed, SEdge *notClosedAt,
                            bool *allCoplanar, Vector *notCoplanarAt,
                            SBezierList *openContours);
    void AddOpenPath(SBezier *sb);
    void Clear();
};

}

#endif //SOLVESPACE_SBEZIER_H
