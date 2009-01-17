
#ifndef __SURFACE_H
#define __SURFACE_H

// Utility function
double Bernstein(int k, int deg, double t);

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
class SPolyCurve {
public:
    int             tag;
    int             deg;
    Vector          ctrl[4];
    double          weight[4];

    Vector EvalAt(double t);
    Vector Start(void);
    Vector Finish(void);
    void MakePwlInto(List<Vector> *l);
    void MakePwlWorker(List<Vector> *l, double ta, double tb);

    void Reverse(void);

    static SPolyCurve From(Vector p0, Vector p1, Vector p2, Vector p3);
    static SPolyCurve From(Vector p0, Vector p1, Vector p2);
    static SPolyCurve From(Vector p0, Vector p1);
};

class SPolyCurveList {
public:
    List<SPolyCurve>    l;

    void Clear(void);
};

class SPolyCurveLoop {
public:
    List<SPolyCurve>    l;

    bool IsClockwiseProjdToNormal(Vector n);

    static SPolyCurveLoop FromCurves(SPolyCurveList *spcl, bool *notClosed);
};


// Stuff for the surface trim curves: piecewise linear
class SCurve {
public:
    hSCurve         h;

    SPolyCurve      exact; // or deg = 0 if we don't know the exact form
    List<Vector>    pts;
    hSSurface       srfA;
    hSSurface       srfB;
};

// A segment of a curve by which a surface is trimmed: indicates which curve,
// by its handle, and the starting and ending points of our segment of it.
// The vector out points out of the surface; it, the surface outer normal,
// and a tangent to the beginning of the curve are all orthogonal.
class STrimBy {
public:
    hSCurve     curve;
    Vector      start;
    Vector      finish;
    Vector      out;
};

class SSurface {
public:
    hSSurface       h;

    int             degm, degn;
    Vector          ctrl[4][4];
    double          weight[4][4];
    Vector          out00;          // outer normal at ctrl[0][0]

    List<STrimBy>   trim;

    static SSurface FromExtrusionOf(SPolyCurve *spc, Vector t0, Vector t1);

    void TriangulateInto(SMesh *sm);
};

class SShell {
public:
    IdList<SCurve,hSCurve>      curve;
    IdList<SSurface,hSSurface>  surface;

    static SShell FromExtrusionOf(SPolyCurveList *spcl, Vector t0, Vector t1);

    static SShell FromUnionOf(SShell *a, SShell *b);
};

#endif

