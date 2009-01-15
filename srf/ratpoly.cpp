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

Vector SPolyCurve::EvalAt(double t) {
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
    Vector pa = EvalAt(ta);
    Vector pb = EvalAt(tb);

    // Can't test in the middle, or certain cubics would break.
    double tm1 = (2*ta + tb) / 3;
    double tm2 = (ta + 2*tb) / 3;

    Vector pm1 = EvalAt(tm1);
    Vector pm2 = EvalAt(tm2);

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

void SPolyCurveList::Clear(void) {
    l.Clear();
}

