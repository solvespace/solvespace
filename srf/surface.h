
#ifndef __SURFACE_H
#define __SURFACE_H

// Utility functions, Bernstein polynomials of order 1-3 and their derivatives.
double Bernstein(int k, int deg, double t);
double BernsteinDerivative(int k, int deg, double t);

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
// our inputs.
class SBezier {
public:
    int             tag;
    int             deg;
    Vector          ctrl[4];
    double          weight[4];

    Vector PointAt(double t);
    Vector Start(void);
    Vector Finish(void);
    void MakePwlInto(List<Vector> *l);
    void MakePwlInto(List<Vector> *l, Vector offset);
    void MakePwlWorker(List<Vector> *l, double ta, double tb, Vector offset);

    void GetBoundingProjd(Vector u, Vector orig, double *umin, double *umax);
    void Reverse(void);

    SBezier TransformedBy(Vector t, Quaternion q);

    static SBezier From(Vector p0, Vector p1, Vector p2, Vector p3);
    static SBezier From(Vector p0, Vector p1, Vector p2);
    static SBezier From(Vector p0, Vector p1);
};

class SBezierList {
public:
    List<SBezier>   l;

    void Clear(void);
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

    hSCurve         newH;       // when merging with booleans
    bool            interCurve; // it's a newly-calculated intersection

    bool            isExact;
    SBezier         exact;

    List<Vector>    pts;

    hSSurface       surfA;
    hSSurface       surfB;

    static SCurve FromTransformationOf(SCurve *a, Vector t, Quaternion q);
    SCurve MakeCopySplitAgainst(SShell *against);

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

// A rational polynomial surface in Bezier form.
class SSurface {
public:
    hSSurface       h;

    int             color;
    DWORD           face;

    int             degm, degn;
    Vector          ctrl[4][4];
    double          weight[4][4];

    List<STrimBy>   trim;

    // The trims broken down into piecewise linear segments.
    SEdgeList       orig;
    SEdgeList       inside;
    SEdgeList       onSameNormal;
    SEdgeList       onFlipNormal;

    static SSurface FromExtrusionOf(SBezier *spc, Vector t0, Vector t1);
    static SSurface FromPlane(Vector pt, Vector u, Vector v);
    static SSurface FromTransformationOf(SSurface *a, Vector t, Quaternion q, 
                                         bool includingTrims);

    SSurface MakeCopyTrimAgainst(SShell *against, SShell *shell,
                                 int type, bool opA);
    void TrimFromEdgeList(SEdgeList *el);
    void IntersectAgainst(SSurface *b, SShell *into);

    void ClosestPointTo(Vector p, double *u, double *v);
    Vector PointAt(double u, double v);
    void TangentsAt(double u, double v, Vector *tu, Vector *tv);
    Vector NormalAt(double u, double v);
    void GetAxisAlignedBounding(Vector *ptMax, Vector *ptMin);

    void TriangulateInto(SShell *shell, SMesh *sm);
    void MakeEdgesInto(SShell *shell, SEdgeList *sel, bool asUv);

    void Clear(void);
};

class SShell {
public:
    IdList<SCurve,hSCurve>      curve;
    IdList<SSurface,hSSurface>  surface;

    void MakeFromExtrusionOf(SBezierLoopSet *sbls, Vector t0, Vector t1,
                             int color);

    void MakeFromUnionOf(SShell *a, SShell *b);
    void MakeFromDifferenceOf(SShell *a, SShell *b);
    static const int AS_UNION      = 10;
    static const int AS_DIFFERENCE = 11;
    static const int AS_INTERSECT  = 12;
    void MakeFromBoolean(SShell *a, SShell *b, int type);
    void CopyCurvesSplitAgainst(SShell *against, SShell *into);
    void CopySurfacesTrimAgainst(SShell *against, SShell *into, int t, bool a);
    void MakeIntersectionCurvesAgainst(SShell *against, SShell *into);
    void MakeEdgeListUseNewCurveIds(SEdgeList *el);
    void CleanupAfterBoolean(void);

    void MakeFromCopyOf(SShell *a);
    void MakeFromTransformationOf(SShell *a, Vector trans, Quaternion q);

    void TriangulateInto(SMesh *sm);
    void MakeEdgesInto(SEdgeList *sel);
    void Clear(void);
};

#endif

