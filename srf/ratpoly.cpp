#include "../solvespace.h"

double Bernstein(int k, int deg, double t)
{
    switch(deg) {
        case 1:
            if(k == 0) {
                return (1 - t);
            } else if(k = 1) {
                return t;
            }
            break;

        case 2:
            if(k == 0) {
                return (1 - t)*(1 - t);
            } else if(k == 1) {
                return 2*(1 - t)*t;
            } else if(k == 2) {
                return t*t;
            }
            break;

        case 3:
            if(k == 0) {
                return (1 - t)*(1 - t)*(1 - t);
            } else if(k == 1) {
                return 3*(1 - t)*(1 - t)*t;
            } else if(k == 2) {
                return 3*(1 - t)*t*t;
            } else if(k == 3) {
                return t*t*t;
            }
            break;
    }
    oops();
}

SPolyCurve SPolyCurve::From(Vector p0, Vector p1) {
    SPolyCurve ret;
    ZERO(&ret);
    ret.deg = 1;
    ret.weight[0] = ret.weight[1] = 1;
    ret.ctrl[0] = p0;
    ret.ctrl[1] = p1;
    return ret;
}

SPolyCurve SPolyCurve::From(Vector p0, Vector p1, Vector p2) {
    SPolyCurve ret;
    ZERO(&ret);
    ret.deg = 2;
    ret.weight[0] = ret.weight[1] = ret.weight[2] = 1;
    ret.ctrl[0] = p0;
    ret.ctrl[1] = p1;
    ret.ctrl[2] = p2;
    return ret;
}

SPolyCurve SPolyCurve::From(Vector p0, Vector p1, Vector p2, Vector p3) {
    SPolyCurve ret;
    ZERO(&ret);
    ret.deg = 3;
    ret.weight[0] = ret.weight[1] = ret.weight[2] = ret.weight[3] = 1;
    ret.ctrl[0] = p0;
    ret.ctrl[1] = p1;
    ret.ctrl[2] = p2;
    ret.ctrl[3] = p3;
    return ret;
}

Vector SPolyCurve::Start(void) {
    return ctrl[0];
}

Vector SPolyCurve::Finish(void) {
    return ctrl[deg];
}

Vector SPolyCurve::PointAt(double t) {
    Vector pt = Vector::From(0, 0, 0);
    double d = 0;

    int i;
    for(i = 0; i <= deg; i++) {
        double B = Bernstein(i, deg, t);
        pt = pt.Plus(ctrl[i].ScaledBy(B*weight[i]));
        d += weight[i]*B;
    }
    pt = pt.ScaledBy(1.0/d);
    return pt;
}

void SPolyCurve::MakePwlInto(List<Vector> *l) {
    l->Add(&(ctrl[0]));
    MakePwlWorker(l, 0.0, 1.0);
}

void SPolyCurve::MakePwlWorker(List<Vector> *l, double ta, double tb) {
    Vector pa = PointAt(ta);
    Vector pb = PointAt(tb);

    // Can't test in the middle, or certain cubics would break.
    double tm1 = (2*ta + tb) / 3;
    double tm2 = (ta + 2*tb) / 3;

    Vector pm1 = PointAt(tm1);
    Vector pm2 = PointAt(tm2);

    double d = max(pm1.DistanceToLine(pa, pb.Minus(pa)),
                   pm2.DistanceToLine(pa, pb.Minus(pa)));

    double tol = SS.chordTol/SS.GW.scale;

    double step = 1.0/SS.maxSegments;
    if((tb - ta) < step || d < tol) {
        // A previous call has already added the beginning of our interval.
        l->Add(&pb);
    } else {
        double tm = (ta + tb) / 2;
        MakePwlWorker(l, ta, tm);
        MakePwlWorker(l, tm, tb);
    }
}

void SPolyCurve::Reverse(void) {
    int i;
    for(i = 0; i < (deg+1)/2; i++) {
        SWAP(Vector, ctrl[i], ctrl[deg-i]);
        SWAP(double, weight[i], weight[deg-i]);
    }
}

void SPolyCurveList::Clear(void) {
    l.Clear();
}

SPolyCurveLoop SPolyCurveLoop::FromCurves(SPolyCurveList *spcl,
                                          bool *allClosed, SEdge *errorAt)
{
    SPolyCurveLoop loop;
    ZERO(&loop);

    if(spcl->l.n < 1) return loop;
    spcl->l.ClearTags();
  
    SPolyCurve *first = &(spcl->l.elem[0]);
    first->tag = 1;
    loop.l.Add(first);
    Vector start = first->Start();
    Vector hanging = first->Finish();

    spcl->l.RemoveTagged();

    while(spcl->l.n > 0 && !hanging.Equals(start)) {
        int i;
        bool foundNext = false;
        for(i = 0; i < spcl->l.n; i++) {
            SPolyCurve *test = &(spcl->l.elem[i]);

            if((test->Finish()).Equals(hanging)) {
                test->Reverse();
                // and let the next test catch it
            }
            if((test->Start()).Equals(hanging)) {
                test->tag = 1;
                loop.l.Add(test);
                hanging = test->Finish();
                spcl->l.RemoveTagged();
                foundNext = true;
                break;
            }
        }
        if(!foundNext) {
            // The loop completed without finding the hanging edge, so
            // it's an open loop
            errorAt->a = hanging;
            errorAt->b = start;
            *allClosed = false;
            return loop;
        }
    }
    if(hanging.Equals(start)) {
        *allClosed = true;
    } else {    
        // We ran out of edges without forming a closed loop.
        errorAt->a = hanging;
        errorAt->b = start;
        *allClosed = false;
    }

    return loop;
}

void SPolyCurveLoop::Reverse(void) {
    l.Reverse();
}

void SPolyCurveLoop::MakePwlInto(SContour *sc) {
    List<Vector> lv;
    ZERO(&lv);

    int i, j;
    for(i = 0; i < l.n; i++) {
        SPolyCurve *spc = &(l.elem[i]);
        spc->MakePwlInto(&lv);
        
        // Each curve's piecewise linearization includes its endpoints,
        // which we don't want to duplicate (creating zero-len edges).
        for(j = (i == 0 ? 0 : 1); j < lv.n; j++) {
            sc->AddPoint(lv.elem[j]);
        }
        lv.Clear();
    }
    // Ensure that it's exactly closed, not just within a numerical tolerance.
    sc->l.elem[sc->l.n - 1] = sc->l.elem[0];
}

SPolyCurveLoops SPolyCurveLoops::From(SPolyCurveList *spcl, SPolygon *poly,
                                      bool *allClosed, SEdge *errorAt)
{
    int i;
    SPolyCurveLoops ret;
    ZERO(&ret);

    while(spcl->l.n > 0) {
        bool thisClosed;
        SPolyCurveLoop loop;
        loop = SPolyCurveLoop::FromCurves(spcl, &thisClosed, errorAt);
        if(!thisClosed) {
            ret.Clear();
            *allClosed = false;
            return ret;
        }

        ret.l.Add(&loop);
        poly->AddEmptyContour();
        loop.MakePwlInto(&(poly->l.elem[poly->l.n-1]));
    }

    poly->normal = poly->ComputeNormal();
    ret.normal = poly->normal;
    poly->FixContourDirections();

    for(i = 0; i < poly->l.n; i++) {
        if(poly->l.elem[i].tag) {
            // We had to reverse this contour in order to fix the poly
            // contour directions; so need to do the same with the curves.
            ret.l.elem[i].Reverse();
        }
    }

    *allClosed = true;
    return ret;
}

void SPolyCurveLoops::Clear(void) {
    int i;
    for(i = 0; i < l.n; i++) {
        (l.elem[i]).Clear();
    }
    l.Clear();
}

SSurface SSurface::FromExtrusionOf(SPolyCurve *spc, Vector t0, Vector t1) {
    SSurface ret;
    ZERO(&ret);

    ret.degm = spc->deg;
    ret.degn = 1;

    int i;
    for(i = 0; i <= ret.degm; i++) {
        ret.ctrl[i][0] = (spc->ctrl[i]).Plus(t0);
        ret.weight[i][0] = spc->weight[i];

        ret.ctrl[i][1] = (spc->ctrl[i]).Plus(t1);
        ret.weight[i][1] = spc->weight[i];
    }

    return ret;
}

SShell SShell::FromExtrusionOf(SPolyCurveList *spcl, Vector t0, Vector t1) {
    SShell ret;
    ZERO(&ret);

    // Group the input curves into loops, not necessarily in the right order.


    // Find the plane that contains our input section.

    // Generate a polygon from the curves, and use this to test how many
    // times each loop is enclosed. Then set the direction (cw/ccw) to
    // be correct for outlines/holes, so that we generate correct normals.

    // Now generate all the surfaces, top/bottom planes plus extrusions.
   
    // And now all the curves, trimming the top and bottom and their extrusion

    // And the lines, trimming adjacent extrusion surfaces.
    return ret;
}


