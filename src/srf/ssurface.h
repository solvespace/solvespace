//
// Created by benjamin on 9/27/18.
//

#ifndef SOLVESPACE_SSURFACE_H
#define SOLVESPACE_SSURFACE_H

#include "dsc.h"
#include "sedge.h"
#include "sbspuv.h"
#include "handle.h"

namespace SolveSpace{

// todo: get rid of this circular dependency
class SBezier;
class SCurve;
class SShell;

// A segment of a curve by which a surface is trimmed: indicates which curve,
// by its handle, and the starting and ending points of our segment of it.
// The vector out points out of the surface; it, the surface outer normal,
// and a tangent to the beginning of the curve are all orthogonal.
class STrimBy {
public:
    hSCurve     curve;
    bool        backwards;
    // If a trim runs backwards, then start and finish still correspond to
    // the actual start and finish, but they appear in reverse order in
    // the referenced curve.
    Vector      start;
    Vector      finish;

    static STrimBy EntireCurve(SShell *shell, hSCurve hsc, bool backwards);
};

// An intersection point between a line and a surface
class SInter {
public:
    int         tag;
    Vector      p;
    SSurface    *srf;
    Point2d     pinter;
    Vector      surfNormal;     // of the intersecting surface, at pinter
    bool        onEdge;         // pinter is on edge of trim poly
};

class hSSurface {
public:
    uint32_t v;
};

// A rational polynomial surface in Bezier form.
class SSurface {
public:

    enum class CombineAs : uint32_t {
        UNION      = 10,
        DIFFERENCE = 11,
        INTERSECT  = 12
    };

    int             tag;
    hSSurface       h;

    // Same as newH for the curves; record what a surface gets renamed to
    // when I copy things over.
    hSSurface       newH;

    RgbaColor       color;
    uint32_t        face;

    int             degm, degn;
    Vector          ctrl[4][4];
    double          weight[4][4];

    List<STrimBy>   trim;

    // For testing whether a point (u, v) on the surface lies inside the trim
    SBspUv          *bsp;
    SEdgeList       edges;

    // For caching our initial (u, v) when doing Newton iterations to project
    // a point into our surface.
    Point2d         cached;

    static SSurface FromExtrusionOf(SBezier *spc, Vector t0, Vector t1);
    static SSurface FromRevolutionOf(SBezier *sb, Vector pt, Vector axis,
                                     double thetas, double thetaf);
    static SSurface FromPlane(Vector pt, Vector u, Vector v);
    static SSurface FromTransformationOf(SSurface *a, Vector t, Quaternion q,
                                         double scale,
                                         bool includingTrims);
    void ScaleSelfBy(double s);

    void EdgeNormalsWithinSurface(Point2d auv, Point2d buv,
                                  Vector *pt, Vector *enin, Vector *enout,
                                  Vector *surfn,
                                  uint32_t auxA,
                                  SShell *shell, SShell *sha, SShell *shb);
    void FindChainAvoiding(SEdgeList *src, SEdgeList *dest, SPointList *avoid);
    SSurface MakeCopyTrimAgainst(SShell *parent, SShell *a, SShell *b,
                                 SShell *into, SSurface::CombineAs type);
    void TrimFromEdgeList(SEdgeList *el, bool asUv);
    void IntersectAgainst(SSurface *b, SShell *agnstA, SShell *agnstB,
                          SShell *into);
    void AddExactIntersectionCurve(SBezier *sb, SSurface *srfB,
                                   SShell *agnstA, SShell *agnstB, SShell *into);

    typedef struct {
        int     tag;
        Point2d p;
    } Inter;
    void WeightControlPoints();
    void UnWeightControlPoints();
    void CopyRowOrCol(bool row, int this_ij, SSurface *src, int src_ij);
    void BlendRowOrCol(bool row, int this_ij, SSurface *a, int a_ij,
                       SSurface *b, int b_ij);
    double DepartureFromCoplanar() const;
    void SplitInHalf(bool byU, SSurface *sa, SSurface *sb);
    void AllPointsIntersecting(Vector a, Vector b,
                               List<SInter> *l,
                               bool asSegment, bool trimmed, bool inclTangent);
    void AllPointsIntersectingUntrimmed(Vector a, Vector b,
                                        int *cnt, int *level,
                                        List<Inter> *l, bool asSegment,
                                        SSurface *sorig);

    void ClosestPointTo(Vector p, Point2d *puv, bool mustConverge=true);
    void ClosestPointTo(Vector p, double *u, double *v, bool mustConverge=true);
    bool ClosestPointNewton(Vector p, double *u, double *v, bool mustConverge=true) const;

    bool PointIntersectingLine(Vector p0, Vector p1, double *u, double *v) const;
    Vector ClosestPointOnThisAndSurface(SSurface *srf2, Vector p);
    void PointOnSurfaces(SSurface *s1, SSurface *s2, double *u, double *v);
    Vector PointAt(double u, double v) const;
    Vector PointAt(Point2d puv) const;
    void TangentsAt(double u, double v, Vector *tu, Vector *tv) const;
    Vector NormalAt(Point2d puv) const;
    Vector NormalAt(double u, double v) const;
    bool LineEntirelyOutsideBbox(Vector a, Vector b, bool asSegment) const;
    void GetAxisAlignedBounding(Vector *ptMax, Vector *ptMin) const;
    bool CoincidentWithPlane(Vector n, double d) const;
    bool CoincidentWith(SSurface *ss, bool sameNormal) const;
    bool IsExtrusion(SBezier *of, Vector *along) const;
    bool IsCylinder(Vector *axis, Vector *center, double *r,
                    Vector *start, Vector *finish) const;

    void TriangulateInto(SShell *shell, SMesh *sm);

    // these are intended as bitmasks, even though there's just one now
    enum class MakeAs : uint32_t {
        UV  = 0x01,
        XYZ = 0x00
    };
    void MakeTrimEdgesInto(SEdgeList *sel, MakeAs flags, SCurve *sc, STrimBy *stb);
    void MakeEdgesInto(SShell *shell, SEdgeList *sel, MakeAs flags,
                       SShell *useCurvesFrom=NULL);

    Vector ExactSurfaceTangentAt(Vector p, SSurface *srfA, SSurface *srfB,
                                 Vector dir);
    void MakeSectionEdgesInto(SShell *shell, SEdgeList *sel, SBezierList *sbl);
    void MakeClassifyingBsp(SShell *shell, SShell *useCurvesFrom);
    double ChordToleranceForEdge(Vector a, Vector b) const;
    void MakeTriangulationGridInto(List<double> *l, double vs, double vf,
                                   bool swapped) const;
    Vector PointAtMaybeSwapped(double u, double v, bool swapped) const;

    void Reverse();
    void Clear();
};

}

#endif //SOLVESPACE_SSURFACE_H
