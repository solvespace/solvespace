
#ifndef __SURFACE_H
#define __SURFACE_H

class hSCurve;
class hSSurface;

// Stuff for rational polynomial curves, of degree one to three. These are
// our inputs.
class SPolyCurve {
public:
    int             deg;
    Vector          ctrl[4];
    double          weight[4];

    Vector EvalAt(double t);
    void GeneratePwlInto(SEdgeList *el);

    static SPolyCurve From(Vector p0, Vector p1, Vector p2, Vector p3);
    static SPolyCurve From(Vector p0, Vector p1);
};

class SPolyCurveList {
public:
    List<SPolyCurve>    l;
};


// Stuff for the surface trim curves: piecewise linear
class hSCurve {
public:
    DWORD v;
};

class SCurve {
public:
    hSCurve         h;

    SList<Vector>   pts;
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

class hSSurface {
public:
    DWORD v;
};

class SSurface {
public:
    hSSurface       h;

    int             degm, degn;
    Vector          ctrl[4][4];
    double          weight[4][4];

    SList<STrimBy>  trim;
};

class SShell {
public:
    IdList<SCurve,hSCurve>      curve;
    IdList<SSurface,hSSurface>  surface;
};

#endif

