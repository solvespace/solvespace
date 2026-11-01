#include "solvespace.h"

#include "harness.h"

// A rational bicubic patch with uneven weights, so that every term of the
// second derivatives matters.
static SSurface MakeBicubic() {
    SSurface srf = {};
    srf.degm = 3;
    srf.degn = 3;
    for(int i = 0; i <= 3; i++) {
        for(int j = 0; j <= 3; j++) {
            srf.ctrl[i][j] = Vector::From(10.0*i + 2.0*j*j,
                                          10.0*j - 1.5*i*j,
                                          3.0*((i + j) % 3) - 0.7*i*i);
            srf.weight[i][j] = 1.0 + 0.3*((i*3 + j*5) % 4);
        }
    }
    return srf;
}

// A quarter of a cylinder of radius r about the z axis, from (r, 0) to (0, r),
// for z from 0 to 10.
static SSurface MakeCylinder(double r) {
    double w = cos(PI/4);
    SBezier arc = SBezier::From(Vector4::From(1, r,   0, 0),
                                Vector4::From(w, w*r, w*r, 0),
                                Vector4::From(1, 0,   r, 0));
    return SSurface::FromExtrusionOf(&arc, Vector::From(0, 0, 0),
                                     Vector::From(0, 0, 10));
}

// An eighth of a sphere of radius r about the origin: a quarter circle in the
// xz plane, from (r, 0, 0) to (0, 0, r), revolved by 90 degrees about z.
static SSurface MakeSphere(double r) {
    double w = cos(PI/4);
    SBezier arc = SBezier::From(Vector4::From(1, r,   0, 0),
                                Vector4::From(w, w*r, 0, w*r),
                                Vector4::From(1, 0,   0, r));
    return SSurface::FromRevolutionOf(&arc, Vector::From(0, 0, 0),
                                      Vector::From(0, 0, 1), 0, PI/2, 0, 0);
}

// An orthonormal basis of the tangent plane at (u, v), turned by angle a from
// the direction of the u tangent.
static void TangentBasis(const SSurface &srf, double u, double v, double a,
                         Vector *e1, Vector *e2) {
    Vector tu, tv;
    srf.TangentsAt(u, v, &tu, &tv);
    Vector n  = (tu.Cross(tv)).WithMagnitude(1),
           x  = tu.WithMagnitude(1),
           y  = n.Cross(x);
    *e1 = x.ScaledBy(cos(a)).Plus(y.ScaledBy(sin(a)));
    *e2 = n.Cross(*e1);
}

TEST_CASE(second_derivatives_match_finite_differences) {
    SSurface srf = MakeBicubic();
    const double h = 1e-6;
    for(double u : { 0.0, 0.3, 0.7, 1.0 }) {
        for(double v : { 0.0, 0.45, 1.0 }) {
            Vector suu, suv, svv;
            srf.SecondDerivativesAt(u, v, &suu, &suv, &svv);

            // Central differences of the exact first derivatives.
            Vector tu0, tv0, tu1, tv1, tu2, tv2, tu3, tv3;
            srf.TangentsAt(u - h, v, &tu0, &tv0);
            srf.TangentsAt(u + h, v, &tu1, &tv1);
            srf.TangentsAt(u, v - h, &tu2, &tv2);
            srf.TangentsAt(u, v + h, &tu3, &tv3);
            Vector fuu = (tu1.Minus(tu0)).ScaledBy(1/(2*h)),
                   fuv = (tv1.Minus(tv0)).ScaledBy(1/(2*h)),
                   fvu = (tu3.Minus(tu2)).ScaledBy(1/(2*h)),
                   fvv = (tv3.Minus(tv2)).ScaledBy(1/(2*h));

            double scale = max(1.0, max(suu.Magnitude(), svv.Magnitude()));
            CHECK_TRUE((suu.Minus(fuu)).Magnitude() < 1e-5*scale);
            CHECK_TRUE((suv.Minus(fuv)).Magnitude() < 1e-5*scale);
            CHECK_TRUE((suv.Minus(fvu)).Magnitude() < 1e-5*scale);
            CHECK_TRUE((svv.Minus(fvv)).Magnitude() < 1e-5*scale);
        }
    }
}

TEST_CASE(cylinder) {
    const double r = 7.5;
    SSurface srf = MakeCylinder(r);
    for(double u : { 0.0, 0.25, 0.6, 1.0 }) {
        for(double a : { 0.0, 0.4, PI/2 }) {
            double v = 0.5;
            Vector e1, e2;
            TangentBasis(srf, u, v, a, &e1, &e2);
            double h11, h12, h22;
            srf.CurvatureFormAt(u, v, e1, e2, &h11, &h12, &h22);

            // The curvature is 1/r around the axis and 0 along it. It curves
            // toward the axis, so the sign depends on which way the normal
            // points.
            Vector p = srf.PointAt(u, v),
                   n = (srf.NormalAt(u, v)).WithMagnitude(1);
            double k = (n.Dot(Vector::From(p.x, p.y, 0)) > 0) ? -1/r : 1/r;
            Vector around = Vector::From(-p.y, p.x, 0).WithMagnitude(1),
                   along  = Vector::From(0, 0, 1);
            double c = around.Dot(e1), s = around.Dot(e2);
            CHECK_EQ_EPS(h11*c*c + 2*h12*c*s + h22*s*s, k);
            c = along.Dot(e1);
            s = along.Dot(e2);
            CHECK_EQ_EPS(h11*c*c + 2*h12*c*s + h22*s*s, 0);
            // Taken together, the form has determinant 0 and trace k.
            CHECK_EQ_EPS(h11*h22 - h12*h12, 0);
            CHECK_EQ_EPS(h11 + h22, k);
        }
    }
}

TEST_CASE(sphere) {
    const double r = 12;
    SSurface srf = MakeSphere(r);
    for(double u : { 0.1, 0.5, 0.9 }) {
        for(double v : { 0.0, 0.3, 1.0 }) {
            Vector e1, e2;
            TangentBasis(srf, u, v, 0.7, &e1, &e2);
            double h11, h12, h22;
            srf.CurvatureFormAt(u, v, e1, e2, &h11, &h12, &h22);

            // The same curvature 1/r in every direction, toward the center.
            Vector p = srf.PointAt(u, v),
                   n = (srf.NormalAt(u, v)).WithMagnitude(1);
            double k = (n.Dot(p) > 0) ? -1/r : 1/r;
            CHECK_EQ_EPS(h11, k);
            CHECK_EQ_EPS(h22, k);
            CHECK_EQ_EPS(h12, 0);
        }
    }
}
