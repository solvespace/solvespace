
#ifndef __SURFACE_H
#define __SURFACE_H

class hSCurve;
class hSSurface;

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

class STrimBy {
public:
    hSCurve     curve;
    Vector      start;
    Vector      finish;
    Vector      out;        // a vector pointing out of the contour
};

class hSSurface {
public:
    DWORD v;
};

class SSurface {
public:
    hSSurface       h;

    Vector          ctrl[4][4];
    double          weight[4];

    SList<STrimBy>  trim;
};

class SShell {
public:
    IdList<SCurve,hSCurve>      allCurves;
    IdList<SSurface,hSSurface>  surface;
};

#endif

