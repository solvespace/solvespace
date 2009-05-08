
#ifndef __SURFACE_H
#define __SURFACE_H

// Utility functions, Bernstein polynomials of order 1-3 and their derivatives.
double Bernstein(int k, int deg, double t);
double BernsteinDerivative(int k, int deg, double t);

class SSurface;

// Utility data structure, a two-dimensional BSP to accelerate polygon
// operations.
class SBspUv {
public:
    Point2d  a, b;

    SBspUv  *pos;
    SBspUv  *neg;

    SBspUv  *more;

    static const int INSIDE            = 100;
    static const int OUTSIDE           = 200;
    static const int EDGE_PARALLEL     = 300;
    static const int EDGE_ANTIPARALLEL = 400;
    static const int EDGE_OTHER        = 500;

    static SBspUv *Alloc(void);
    static SBspUv *From(SEdgeList *el);

    Point2d IntersectionWith(Point2d a, Point2d b);
    SBspUv *InsertEdge(Point2d a, Point2d b);
    int ClassifyPoint(Point2d p, Point2d eb);
    int ClassifyEdge(Point2d ea, Point2d eb);
};

// Now the data structures to represent a shell of trimmed rational polynomial
// surfaces.

class SShell;

class hSSurface {
public:
    DWORD v;
};

class hSCurve {
public:
    DWORD v;
};

// Stuff for rational polynomial curves, of degree one to three. These are
// our inputs, and are also calculated for certain exact surface-surface
// intersections.
class SBezier {
public:
    int             tag;
    int             deg;
    Vector          ctrl[4];
    double          weight[4];

    Vector PointAt(double t);
    Vector TangentAt(double t);
    void ClosestPointTo(Vector p, double *t);
    void SplitAt(double t, SBezier *bef, SBezier *aft);

    Vector Start(void);
    Vector Finish(void);
    bool Equals(SBezier *b);
    void MakePwlInto(List<Vector> *l);
    void MakePwlWorker(List<Vector> *l, double ta, double tb);

    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax);
    void Reverse(void);

    bool IsCircle(Vector axis, Vector *center, double *r);
    bool IsRational(void);

    SBezier TransformedBy(Vector t, Quaternion q);
    SBezier InPerspective(Vector u, Vector v, Vector n,
                          Vector origin, double cameraTan);

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

    void Clear(void);
    void CullIdenticalBeziers(void);
};

class SBezierLoop {
public:
    List<SBezier>   l;

    inline void Clear(void) { l.Clear(); }
    void Reverse(void);
    void MakePwlInto(SContour *sc);
    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax);

    static SBezierLoop FromCurves(SBezierList *spcl,
                                  bool *allClosed, SEdge *errorAt);
};

class SBezierLoopSet {
public:
    List<SBezierLoop> l;
    Vector normal;
    Vector point;

    static SBezierLoopSet From(SBezierList *spcl, SPolygon *poly,
                          bool *allClosed, SEdge *errorAt);

    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax);
    void Clear(void);
};

// Stuff for the surface trim curves: piecewise linear
class SCurve {
public:
    hSCurve         h;

    // In a Boolean, C = A op B. The curves in A and B get copied into C, and
    // therefore must get new hSCurves assigned. For the curves in A and B,
    // we use newH to record their new handle in C.
    hSCurve         newH;
    static const int FROM_A             = 100;
    static const int FROM_B             = 200;
    static const int FROM_INTERSECTION  = 300;
    int             source;

    bool            isExact;
    SBezier         exact;

    List<Vector>    pts;

    hSSurface       surfA;
    hSSurface       surfB;

    static SCurve FromTransformationOf(SCurve *a, Vector t, Quaternion q);
    SCurve MakeCopySplitAgainst(SShell *agnstA, SShell *agnstB,
                                SSurface *srfA, SSurface *srfB);

    void Clear(void);
};

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

    static STrimBy STrimBy::EntireCurve(SShell *shell, hSCurve hsc, bool bkwds);
};

// An intersection point between a line and a surface
class SInter {
public:
    int         tag;
    Vector      p;
    SSurface    *srf;
    hSSurface   hsrf;
    Vector      surfNormal; // of the intersecting surface, at pinter
    bool        onEdge;     // pinter is on edge of trim poly
};

// A rational polynomial surface in Bezier form.
class SSurface {
public:
    hSSurface       h;

    // Same as newH for the curves; record what a surface gets renamed to
    // when I copy things over.
    hSSurface       newH;

    int             color;
    DWORD           face;

    int             degm, degn;
    Vector          ctrl[4][4];
    double          weight[4][4];

    List<STrimBy>   trim;

    // For testing whether a point (u, v) on the surface lies inside the trim
    SBspUv          *bsp;

    static SSurface FromExtrusionOf(SBezier *spc, Vector t0, Vector t1);
    static SSurface FromRevolutionOf(SBezier *sb, Vector pt, Vector axis,
                                        double thetas, double thetaf);
    static SSurface FromPlane(Vector pt, Vector u, Vector v);
    static SSurface FromTransformationOf(SSurface *a, Vector t, Quaternion q, 
                                         bool includingTrims);

    SSurface MakeCopyTrimAgainst(SShell *against, SShell *parent, SShell *into,
                                 int type, bool opA);
    void TrimFromEdgeList(SEdgeList *el);
    void IntersectAgainst(SSurface *b, SShell *agnstA, SShell *agnstB, 
                          SShell *into);
    void AddExactIntersectionCurve(SBezier *sb, SSurface *srfB,
                          SShell *agnstA, SShell *agnstB, SShell *into);

    typedef struct {
        int     tag;
        Point2d p;
    } Inter;
    void WeightControlPoints(void);
    void UnWeightControlPoints(void);
    void CopyRowOrCol(bool row, int this_ij, SSurface *src, int src_ij);
    void BlendRowOrCol(bool row, int this_ij, SSurface *a, int a_ij,
                                              SSurface *b, int b_ij);
    double DepartureFromCoplanar(void);
    void SplitInHalf(bool byU, SSurface *sa, SSurface *sb);
    void AllPointsIntersecting(Vector a, Vector b,
                                    List<SInter> *l,
                                    bool seg, bool trimmed, bool inclTangent);
    void AllPointsIntersectingUntrimmed(Vector a, Vector b,
                                            int *cnt, int *level,
                                            List<Inter> *l, bool segment,
                                            SSurface *sorig);

    void ClosestPointTo(Vector p, double *u, double *v, bool converge=true);
    bool PointIntersectingLine(Vector p0, Vector p1, double *u, double *v);
    void PointOnSurfaces(SSurface *s1, SSurface *s2, double *u, double *v);
    Vector PointAt(double u, double v);
    void TangentsAt(double u, double v, Vector *tu, Vector *tv);
    Vector NormalAt(double u, double v);
    bool LineEntirelyOutsideBbox(Vector a, Vector b, bool segment);
    void GetAxisAlignedBounding(Vector *ptMax, Vector *ptMin);
    bool CoincidentWithPlane(Vector n, double d);
    bool CoincidentWith(SSurface *ss, bool sameNormal);
    bool IsExtrusion(SBezier *of, Vector *along);
    bool IsCylinder(Vector *axis, Vector *center, double *r,
                        Vector *start, Vector *finish);

    void TriangulateInto(SShell *shell, SMesh *sm);
    void MakeTrimEdgesInto(SEdgeList *sel, bool asUv, SCurve *sc, STrimBy *stb);
    void MakeEdgesInto(SShell *shell, SEdgeList *sel, bool asUv,
            SShell *useCurvesFrom=NULL);
    void MakeSectionEdgesInto(SShell *shell, SEdgeList *sel, SBezierList *sbl);
    void MakeClassifyingBsp(SShell *shell);
    double ChordToleranceForEdge(Vector a, Vector b);
    void MakeTriangulationGridInto(List<double> *l, double vs, double vf,
                                    bool swapped);
    Vector PointAtMaybeSwapped(double u, double v, bool swapped);

    void Reverse(void);
    void Clear(void);
};

class SShell {
public:
    IdList<SCurve,hSCurve>      curve;
    IdList<SSurface,hSSurface>  surface;

    void MakeFromExtrusionOf(SBezierLoopSet *sbls, Vector t0, Vector t1,
                             int color);
    void MakeFromRevolutionOf(SBezierLoopSet *sbls, Vector pt, Vector axis,
                             int color);

    void MakeFromUnionOf(SShell *a, SShell *b);
    void MakeFromDifferenceOf(SShell *a, SShell *b);
    static const int AS_UNION      = 10;
    static const int AS_DIFFERENCE = 11;
    static const int AS_INTERSECT  = 12;
    void MakeFromBoolean(SShell *a, SShell *b, int type);
    void CopyCurvesSplitAgainst(bool opA, SShell *agnst, SShell *into);
    void CopySurfacesTrimAgainst(SShell *against, SShell *into, int t, bool a);
    void MakeIntersectionCurvesAgainst(SShell *against, SShell *into);
    void MakeClassifyingBsps(void);
    void AllPointsIntersecting(Vector a, Vector b, List<SInter> *il,
                                bool seg, bool trimmed, bool inclTangent);
    void MakeCoincidentEdgesInto(SSurface *proto, bool sameNormal,
                                 SEdgeList *el, SShell *useCurvesFrom);
    void CleanupAfterBoolean(void);

    static const int INSIDE                 = 100;
    static const int OUTSIDE                = 200;
    static const int SURF_PARALLEL          = 300;
    static const int SURF_ANTIPARALLEL      = 400;
    static const int EDGE_PARALLEL          = 500;
    static const int EDGE_ANTIPARALLEL      = 600;
    static const int EDGE_TANGENT           = 700;

    int ClassifyPoint(Vector p, Vector edge_n, Vector surf_n);


    void MakeFromCopyOf(SShell *a);
    void MakeFromTransformationOf(SShell *a, Vector trans, Quaternion q);

    void TriangulateInto(SMesh *sm);
    void MakeEdgesInto(SEdgeList *sel);
    void MakeSectionEdgesInto(Vector n, double d,
                                SEdgeList *sel, SBezierList *sbl);
    void Clear(void);
};

#endif

