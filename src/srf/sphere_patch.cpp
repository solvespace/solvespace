//-----------------------------------------------------------------------------
// The corner patch where three fillets meet: a triangle on the sphere of the
// rolling ball, bounded by three great-circle arcs (the fillets' end
// cross-sections).
//
// One patch for the whole triangle would need a collapsed edge (a pole), so
// instead the triangle is split at its incenter T into three triangles
// (P_i, P_i+1, T). Each is covered by a band: the side arc P_i P_i+1 swept
// about the axis through the sphere center parallel to its chord. Turning
// about an axis through the center keeps the arc on the sphere, and the band
// lies between the two circles about that axis through P_i and P_i+1, so it
// never reaches the axis and has no pole. The bands overlap; the arcs from
// each P_i to T trim them.
//
// Why the incenter: a great circle leaving P_i within 90 degrees of the side
// stays inside the band (a great circle touching one of the band's bounding
// circles stays on the band's side of it). The incenter splits each corner
// angle of the triangle in half, and those angles are under 180 degrees, so
// the arcs to T always leave within 90 degrees.
//
// Copyright 2026 Paul H Kahler.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

// The great-circle arc from a to b, both on the sphere about c. Less than
// 180 degrees.
static bool GreatArc(Vector c, Vector a, Vector b, SBezier *out) {
    Vector ua = a.Minus(c),
           ub = b.Minus(c);
    double r = ua.Magnitude();
    ua = ua.WithMagnitude(1.0);
    ub = ub.WithMagnitude(1.0);
    Vector bis = ua.Plus(ub);
    if(bis.Magnitude() < 1e-6) return false;
    // Half the arc's angle is beta. The tangents at a and b cross at r/cos(beta)
    // from the center, on the bisector; that point's weight is cos(beta).
    double cb = sqrt((1 + ua.Dot(ub)) / 2);
    Vector m = c.Plus(bis.WithMagnitude(r / cb));
    *out = SBezier::From(Vector4::From(1.0, a),
                         Vector4::From(cb, m),
                         Vector4::From(1.0, b));
    return true;
}

// The incenter of the spherical triangle with unit vertices a[3]: the point
// at equal angles from the three side planes, on the inside of each.
static bool Incenter(const Vector a[3], Vector *x) {
    Vector n[3];
    for(int i = 0; i < 3; i++) {
        n[i] = a[i].Cross(a[(i + 1) % 3]);
        if(n[i].Magnitude() < 1e-6) return false;
        n[i] = n[i].WithMagnitude(1.0);
        if(n[i].Dot(a[(i + 2) % 3]) < 0) n[i] = n[i].ScaledBy(-1.0);
    }
    // Solve n[i] . x = 1 for all i, then scale x onto the sphere.
    double det = n[0].Dot(n[1].Cross(n[2]));
    if(fabs(det) < 1e-9) return false;
    Vector v = n[1].Cross(n[2]).Plus(n[2].Cross(n[0])).Plus(n[0].Cross(n[1]));
    *x = v.ScaledBy(1.0 / det).WithMagnitude(1.0);
    for(int i = 0; i < 3; i++) {
        if(x->Dot(n[i]) <= 0) return false;
    }
    return true;
}

// The band for side a-b: the side arc swept about the axis through c along
// the chord, from the side to the incenter t.
static bool Band(Vector c, Vector a, Vector b, Vector t, SSurface *out) {
    SBezier side;
    if(!GreatArc(c, a, b, &side)) return false;
    Vector u  = (b.Minus(a)).WithMagnitude(1.0),
           m0 = (a.Minus(c).Plus(b.Minus(c))).WithMagnitude(1.0);
    Vector rho = t.Minus(c);
    rho = rho.Minus(u.ScaledBy(rho.Dot(u)));
    double phi = atan2(rho.Dot(u.Cross(m0)), rho.Dot(m0));
    if(fabs(phi) < 1e-6 || fabs(phi) > PI - 1e-3) return false;
    *out = SSurface::FromRevolutionOf(&side, c, u, 0.0, phi, 0.0, 0.0);
    return true;
}

// Cover the spherical triangle p[3] on the sphere about c with three bands.
// band[i] covers the side from p[i] to p[i+1] and the incenter; arc[i] runs
// from p[i] to the incenter t. The bands face away from c if outward, else
// toward it (a concave corner).
bool SphereTrianglePatch(Vector c, const Vector p[3], bool outward,
                         SSurface band[3], SBezier arc[3], Vector *t)
{
    double r = (p[0].Minus(c)).Magnitude();
    Vector a[3];
    for(int i = 0; i < 3; i++) {
        if(fabs((p[i].Minus(c)).Magnitude() - r) > LENGTH_EPS) return false;
        a[i] = (p[i].Minus(c)).WithMagnitude(1.0);
    }
    Vector x;
    if(!Incenter(a, &x)) return false;
    *t = c.Plus(x.ScaledBy(r));

    for(int i = 0; i < 3; i++) {
        if(!GreatArc(c, p[i], *t, &arc[i])) return false;
        if(!Band(c, p[i], p[(i + 1) % 3], *t, &band[i])) return false;

        SSurface *s = &band[i];
        Vector n = s->NormalAt(0.5, 0.5),
               q = s->PointAt(0.5, 0.5);
        if((n.Dot(q.Minus(c)) > 0) != outward) {
            for(int j = 0; j <= s->degm; j++) {
                swap(s->ctrl[j][0], s->ctrl[j][s->degn]);
                swap(s->weight[j][0], s->weight[j][s->degn]);
            }
        }
        s->color = RGBi(100, 100, 100);
    }
    return true;
}

} // namespace SolveSpace
