//-----------------------------------------------------------------------------
// Math on rational polynomial surfaces and curves, typically in Bezier
// form. Evaluate, root-find (by Newton's methods), evaluate derivatives,
// and so on.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

// Converge it to better than LENGTH_EPS; we want two points, each
// independently projected into uv and back, to end up equal with the
// LENGTH_EPS. Best case that requires LENGTH_EPS/2, but more is better
// and convergence should be fast by now.
#define RATPOLY_EPS (LENGTH_EPS/(1e2))

static double Bernstein(int k, int deg, double t)
{
    if(k > deg || k < 0) return 0;

    switch(deg) {
        case 0:
            return 1;
            break;

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

double BernsteinDerivative(int k, int deg, double t)
{
    switch(deg) {
        case 0:
            return 0;
            break;

        case 1:
            if(k == 0) {
                return -1;
            } else if(k = 1) {
                return 1;
            }
            break;

        case 2:
            if(k == 0) {
                return -2 + 2*t;
            } else if(k == 1) {
                return 2 - 4*t;
            } else if(k == 2) {
                return 2*t;
            }
            break;

        case 3:
            if(k == 0) {
                return -3 + 6*t - 3*t*t;
            } else if(k == 1) {
                return 3 - 12*t + 9*t*t;
            } else if(k == 2) {
                return 6*t - 9*t*t;
            } else if(k == 3) {
                return 3*t*t;
            }
            break;
    }
    oops();
}

Vector SBezier::PointAt(double t) {
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

Vector SBezier::TangentAt(double t) {
    Vector pt = Vector::From(0, 0, 0), pt_p = Vector::From(0, 0, 0);
    double d = 0, d_p = 0;

    int i;
    for(i = 0; i <= deg; i++) {
        double B  = Bernstein(i, deg, t),
               Bp = BernsteinDerivative(i, deg, t);

        pt = pt.Plus(ctrl[i].ScaledBy(B*weight[i]));
        d += weight[i]*B;

        pt_p = pt_p.Plus(ctrl[i].ScaledBy(Bp*weight[i]));
        d_p += weight[i]*Bp;
    }

    // quotient rule; f(t) = n(t)/d(t), so f' = (n'*d - n*d')/(d^2)
    Vector ret;
    ret = (pt_p.ScaledBy(d)).Minus(pt.ScaledBy(d_p));
    ret = ret.ScaledBy(1.0/(d*d));
    return ret;
}

void SBezier::ClosestPointTo(Vector p, double *t) {
    int i;
    double minDist = VERY_POSITIVE;
    *t = 0;
    double res = (deg <= 2) ? 7.0 : 20.0;
    for(i = 0; i < (int)res; i++) {
        double tryt = (i/res);
        
        Vector tryp = PointAt(tryt);
        double d = (tryp.Minus(p)).Magnitude();
        if(d < minDist) {
            *t = tryt;
            minDist = d;
        }
    }

    Vector p0;
    for(i = 0; i < 15; i++) {
        p0 = PointAt(*t);
        if(p0.Equals(p, RATPOLY_EPS)) {
            return;
        }

        Vector dp = TangentAt(*t);
        Vector pc = p.ClosestPointOnLine(p0, dp);
        *t += (pc.Minus(p0)).DivPivoting(dp);
    }
    dbp("didn't converge (closest point on bezier curve)");
}

void SBezier::SplitAt(double t, SBezier *bef, SBezier *aft) {
    Vector4 ct[4];
    int i;
    for(i = 0; i <= deg; i++) {
        ct[i] = Vector4::From(weight[i], ctrl[i]);
    }

    switch(deg) {
        case 1: {
            Vector4 cts = Vector4::Blend(ct[0], ct[1], t);
            *bef = SBezier::From(ct[0], cts);
            *aft = SBezier::From(cts, ct[1]);
            break;
        }
        case 2: {
            Vector4 ct01 = Vector4::Blend(ct[0], ct[1], t),
                    ct12 = Vector4::Blend(ct[1], ct[2], t),
                    cts  = Vector4::Blend(ct01,  ct12,  t);

            *bef = SBezier::From(ct[0], ct01, cts);
            *aft = SBezier::From(cts, ct12, ct[2]);
            break;
        }
        case 3: {
            Vector4 ct01    = Vector4::Blend(ct[0], ct[1], t),
                    ct12    = Vector4::Blend(ct[1], ct[2], t),
                    ct23    = Vector4::Blend(ct[2], ct[3], t),
                    ct01_12 = Vector4::Blend(ct01,  ct12,  t),
                    ct12_23 = Vector4::Blend(ct12,  ct23,  t),
                    cts     = Vector4::Blend(ct01_12, ct12_23, t);

            *bef = SBezier::From(ct[0], ct01, ct01_12, cts);
            *aft = SBezier::From(cts, ct12_23, ct23, ct[3]);
            break;
        }
        default: oops();
    }
}

void SBezier::MakePwlInto(List<SCurvePt> *l) {
    List<Vector> lv;
    ZERO(&lv);
    MakePwlInto(&lv);
    int i;
    for(i = 0; i < lv.n; i++) {
        SCurvePt scpt;
        scpt.tag    = 0;
        scpt.p      = lv.elem[i];
        scpt.vertex = (i == 0) || (i == (lv.n - 1));
        l->Add(&scpt);
    }
    lv.Clear();
}
void SBezier::MakePwlInto(List<Vector> *l) {
    l->Add(&(ctrl[0]));
    MakePwlWorker(l, 0.0, 1.0);
}
void SBezier::MakePwlWorker(List<Vector> *l, double ta, double tb) {
    Vector pa = PointAt(ta);
    Vector pb = PointAt(tb);

    // Can't test in the middle, or certain cubics would break.
    double tm1 = (2*ta + tb) / 3;
    double tm2 = (ta + 2*tb) / 3;

    Vector pm1 = PointAt(tm1);
    Vector pm2 = PointAt(tm2);

    double d = max(pm1.DistanceToLine(pa, pb.Minus(pa)),
                   pm2.DistanceToLine(pa, pb.Minus(pa)));

    double step = 1.0/SS.maxSegments;
    if((tb - ta) < step || d < SS.ChordTolMm()) {
        // A previous call has already added the beginning of our interval.
        l->Add(&pb);
    } else {
        double tm = (ta + tb) / 2;
        MakePwlWorker(l, ta, tm);
        MakePwlWorker(l, tm, tb);
    }
}

Vector SSurface::PointAt(double u, double v) {
    Vector num = Vector::From(0, 0, 0);
    double den = 0;

    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            double Bi = Bernstein(i, degm, u),
                   Bj = Bernstein(j, degn, v);

            num = num.Plus(ctrl[i][j].ScaledBy(Bi*Bj*weight[i][j]));
            den += weight[i][j]*Bi*Bj;
        }
    }
    num = num.ScaledBy(1.0/den);
    return num;
}

void SSurface::TangentsAt(double u, double v, Vector *tu, Vector *tv) {
    Vector num   = Vector::From(0, 0, 0),
           num_u = Vector::From(0, 0, 0),
           num_v = Vector::From(0, 0, 0);
    double den   = 0,
           den_u = 0,
           den_v = 0;

    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            double Bi  = Bernstein(i, degm, u),
                   Bj  = Bernstein(j, degn, v),
                   Bip = BernsteinDerivative(i, degm, u),
                   Bjp = BernsteinDerivative(j, degn, v);

            num = num.Plus(ctrl[i][j].ScaledBy(Bi*Bj*weight[i][j]));
            den += weight[i][j]*Bi*Bj;

            num_u = num_u.Plus(ctrl[i][j].ScaledBy(Bip*Bj*weight[i][j]));
            den_u += weight[i][j]*Bip*Bj;

            num_v = num_v.Plus(ctrl[i][j].ScaledBy(Bi*Bjp*weight[i][j]));
            den_v += weight[i][j]*Bi*Bjp;
        }
    }
    // quotient rule; f(t) = n(t)/d(t), so f' = (n'*d - n*d')/(d^2)
    *tu = ((num_u.ScaledBy(den)).Minus(num.ScaledBy(den_u)));
    *tu = tu->ScaledBy(1.0/(den*den));

    *tv = ((num_v.ScaledBy(den)).Minus(num.ScaledBy(den_v)));
    *tv = tv->ScaledBy(1.0/(den*den));
}

Vector SSurface::NormalAt(double u, double v) {
    Vector tu, tv;
    TangentsAt(u, v, &tu, &tv);
    return tu.Cross(tv);
}

void SSurface::ClosestPointTo(Vector p, double *u, double *v, bool converge) {
    int i, j;

    if(degm == 1 && degn == 1) {
        *u = *v = 0; // a plane, perfect no matter what the initial guess
    } else {
        double minDist = VERY_POSITIVE;
        double res = (max(degm, degn) == 2) ? 7.0 : 20.0;
        for(i = 0; i < (int)res; i++) {
            for(j = 0; j <= (int)res; j++) {
                double tryu = (i/res), tryv = (j/res);
                
                Vector tryp = PointAt(tryu, tryv);
                double d = (tryp.Minus(p)).Magnitude();
                if(d < minDist) {
                    *u = tryu;
                    *v = tryv;
                    minDist = d;
                }
            }
        }
    }

    // Initial guess is in u, v
    Vector p0;
    for(i = 0; i < (converge ? 15 : 3); i++) {
        p0 = PointAt(*u, *v);
        if(converge) {
            if(p0.Equals(p, RATPOLY_EPS)) {
                return;
            }
        }

        Vector tu, tv;
        TangentsAt(*u, *v, &tu, &tv);
        
        // Project the point into a plane through p0, with basis tu, tv; a
        // second-order thing would converge faster but needs second
        // derivatives.
        Vector dp = p.Minus(p0);
        double du = dp.Dot(tu), dv = dp.Dot(tv);
        *u += du / (tu.MagSquared());
        *v += dv / (tv.MagSquared());
    }

    if(converge) {
        dbp("didn't converge");
        dbp("have %.3f %.3f %.3f", CO(p0));
        dbp("want %.3f %.3f %.3f", CO(p));
        dbp("distance = %g", (p.Minus(p0)).Magnitude());
    }

    if(isnan(*u) || isnan(*v)) {
        *u = *v = 0;
    }
}

bool SSurface::PointIntersectingLine(Vector p0, Vector p1, double *u, double *v)
{
    int i;
    for(i = 0; i < 15; i++) {
        Vector pi, p, tu, tv;
        p = PointAt(*u, *v);
        TangentsAt(*u, *v, &tu, &tv);
        
        Vector n = (tu.Cross(tv)).WithMagnitude(1);
        double d = p.Dot(n);

        bool parallel;
        pi = Vector::AtIntersectionOfPlaneAndLine(n, d, p0, p1, &parallel);
        if(parallel) break;

        // Check for convergence
        if(pi.Equals(p, RATPOLY_EPS)) return true;

        // Adjust our guess and iterate
        Vector dp = pi.Minus(p);
        double du = dp.Dot(tu), dv = dp.Dot(tv);
        *u += du / (tu.MagSquared());
        *v += dv / (tv.MagSquared());
    }
//    dbp("didn't converge (surface intersecting line)");
    return false;
}

void SSurface::PointOnSurfaces(SSurface *s1, SSurface *s2,
                                                double *up, double *vp)
{
    double u[3] = { *up, 0, 0 }, v[3] = { *vp, 0, 0 };
    SSurface *srf[3] = { this, s1, s2 };

    // Get initial guesses for (u, v) in the other surfaces
    Vector p = PointAt(*u, *v);
    (srf[1])->ClosestPointTo(p, &(u[1]), &(v[1]), false);
    (srf[2])->ClosestPointTo(p, &(u[2]), &(v[2]), false);

    int i, j;
    for(i = 0; i < 15; i++) {
        // Approximate each surface by a plane
        Vector p[3], tu[3], tv[3], n[3];
        double d[3];
        for(j = 0; j < 3; j++) {
            p[j] = (srf[j])->PointAt(u[j], v[j]);
            (srf[j])->TangentsAt(u[j], v[j], &(tu[j]), &(tv[j]));
            n[j] = ((tu[j]).Cross(tv[j])).WithMagnitude(1);
            d[j] = (n[j]).Dot(p[j]);
        }

        // If a = b and b = c, then does a = c? No, it doesn't.
        if((p[0]).Equals(p[1], RATPOLY_EPS) && 
           (p[1]).Equals(p[2], RATPOLY_EPS) &&
           (p[2]).Equals(p[0], RATPOLY_EPS))
        {
            *up = u[0];
            *vp = v[0];
            return;
        }

        bool parallel;
        Vector pi = Vector::AtIntersectionOfPlanes(n[0], d[0],
                                                   n[1], d[1],
                                                   n[2], d[2], &parallel);
        if(parallel) break;

        for(j = 0; j < 3; j++) {
            Vector dp = pi.Minus(p[j]);
            double du = dp.Dot(tu[j]), dv = dp.Dot(tv[j]);
            u[j] += du / (tu[j]).MagSquared();
            v[j] += dv / (tv[j]).MagSquared();
        }
    }
    dbp("didn't converge (three surfaces intersecting)");
}

