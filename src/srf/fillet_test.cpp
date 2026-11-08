//-----------------------------------------------------------------------------
// Test hook for fillets and chamfers: picks the edges to modify in the test
// models in misc_CAD/fillet_test, recognizing each model by its shape. A
// throwaway, until edges can be named and picked by the user. See the plan
// in chamfer.cpp.
//
// Copyright 2026 Paul H Kahler.
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {
static bool IsTestLine(const SBezier &b, int axis, double value) {
    Vector p0 = b.Start(), p1 = b.Finish();
    return b.deg == 1 &&
           fabs(p0.Element(axis) - value) < LENGTH_EPS &&
           fabs(p1.Element(axis) - value) < LENGTH_EPS;
}

// For the test hook: a line whose two faces are planes not at 90 degrees.
static bool IsTestSlantedLine(SShell *sh, SCurve *sc) {
    if(sc->exact.deg != 1) return false;
    SSurface *sa = sh->surface.FindByIdNoOops(sc->surfA),
             *sb = sh->surface.FindByIdNoOops(sc->surfB);
    if(sa == NULL || sb == NULL) return false;
    if(sa->degm != 1 || sa->degn != 1 || sb->degm != 1 || sb->degn != 1) return false;
    double dot = (sa->NormalAt(0.5, 0.5).WithMagnitude(1.0)).Dot(
                  sb->NormalAt(0.5, 0.5).WithMagnitude(1.0));
    return fabs(dot) > 1e-6 && fabs(dot) < 1 - 1e-6;
}

// We return 0 for no feature, radius for a fillet, and for chamfers a negative
// value whose absolute value is the radius of an equivalent fillet.
double SShell::GetEdgeModifier(SCurve &sc)
{
    // Off unless the environment variable SOLVESPACE_FILLET_TEST is set, so
    // other models (and the test suite) are left alone.
    static int enabled = -1;
    if(enabled < 0) enabled = (getenv("SOLVESPACE_FILLET_TEST") != NULL) ? 1 : 0;
    if(!enabled) return 0.0;

    // Test hook until edges can be named. Picks edges in the test models in
    // misc_CAD/fillet_test:
    //   test_cube, L_shape: the outer lines around the z = 10 face, which
    //                       meet at corners
    //   test_cube:          the line at x = 10, y = 10, making three edges
    //                       at the corner (10, 10, 10)
    //   L_shape:            the line at x = 0, y = 0 (concave)
    //   test_cylinder:      the arcs of the top circle at z = 10
    //   arc_test:           every edge not in the bottom face (see below)
    //   text_test, bezier_test, revolve_test, skew_test, straight_test,
    //   test_cube_boolean: see below
    //   house_test:         lines between planes not at 90 degrees (eaves).
    //                       If a shell has any of these, only they are
    //                       picked; the rules above would also pick lines in
    //                       the house.
    const double r = 3.0;   // negative for chamfers
    if(!sc.isExact) return 0.0;
    const SBezier &b = sc.exact;
    // text_test (text extruded from z = 0 to 5) is the only model whose
    // shell spans exactly that in z, whatever the letters. Every edge of the
    // top face, with a radius of 0.5, since text strokes are thin.
    double zlo = VERY_POSITIVE, zhi = VERY_NEGATIVE;
    for(SSurface &ss : surface) {
        for(int i = 0; i <= ss.degm; i++) {
            for(int j = 0; j <= ss.degn; j++) {
                zlo = min(zlo, ss.ctrl[i][j].z);
                zhi = max(zhi, ss.ctrl[i][j].z);
            }
        }
    }
    if(fabs(zlo) < LENGTH_EPS && fabs(zhi - 5) < LENGTH_EPS) {
        bool top = true;
        for(int i = 0; i <= b.deg; i++) {
            if(fabs(b.ctrl[i].z - 5) > LENGTH_EPS) top = false;
        }
        return top ? ((r < 0) ? -0.5 : 0.5) : 0.0;
    }

    // arc_end_test (a 120 degree circular segment of radius 20, closed by a
    // chord at y = 10, extruded from z = 0 to 7) is the only model that spans
    // exactly that in z. The chord meets the arc 60 degrees from its tangent.
    // Variants:
    //   0: the top arc, whose fillet ends on the chord's wall at an angle
    //   1: also the top chord (miters of an arc and a line)
    const int arcEndVariant = 0;
    if(fabs(zlo) < LENGTH_EPS && fabs(zhi - 7) < LENGTH_EPS) {
        bool top = true;
        for(int i = 0; i <= b.deg; i++) {
            if(fabs(b.ctrl[i].z - 7) > LENGTH_EPS) top = false;
        }
        if(!top) return 0.0;
        return (b.deg == 2 || arcEndVariant == 1) ? r : 0.0;
    }

    // bezier_test (a profile with a cubic edge from (20, 0) to (-20, 0),
    // extruded from z = -10 to 10) is the only model with a cubic. Variants:
    //   0: the top cubic (its ends meet the side walls at an angle)
    //   1: the whole top loop (miters of the cubic and lines)
    //   2: also the vertical edges at the cubic's ends (corners)
    const int bezierVariant = 0;
    for(SCurve &c : curve) {
        if(!(c.isExact && c.exact.deg == 3)) continue;
        bool top    = fabs(b.Start().z - 10) < LENGTH_EPS && fabs(b.Finish().z - 10) < LENGTH_EPS,
             cubic  = b.deg == 3 && top,
             vert   = b.deg == 1 && IsTestLine(b, 1, 0) &&
                      (IsTestLine(b, 0, 20) || IsTestLine(b, 0, -20));
        if(bezierVariant == 0) return cubic ? r : 0.0;
        if(bezierVariant == 1) return top ? r : 0.0;
        return (top || vert) ? r : 0.0;
    }

    // revolve_test (a profile revolved 80 degrees about the y axis) is the
    // only model with arcs of 80 degrees (weight 0.766). Every arc: they
    // lie between cones, cylinders and a plane, and end on the end caps.
    for(SCurve &c : curve) {
        if(c.isExact && c.exact.deg == 2 && fabs(c.exact.weight[1] - 0.766) < 0.002) {
            return (b.deg == 2) ? r : 0.0;
        }
    }

    // skew_test (a skewed extrusion, about 100 in size) is the only big
    // model. Variants:
    //   1: the six side edges (one concave), ending on the caps at an angle
    //   2: the three edges at the top corner (120, 0, 62.45) and the three
    //      at the bottom corner (-120, 30, -62.45): corners with three
    //      different face angles
    //   3: two edges at the top corner (120, 0, 62.45), the side edge and
    //      the top edge to (0, 0, 62.45): a miter between faces at
    //      different angles
    //   4: every edge except the concave side edge from (-20, 50, -62.45)
    //      and the four cap edges at its ends (the reflex corners): sphere
    //      corners, and miters of unequal angles next to the reflex corners
    //   5: every edge except the concave side edge: also miters of unequal
    //      angles at the reflex corners
    //   6: only the two bottom cap edges at the reflex vertex
    //      (-20, 50, -62.45): one miter of unequal angles at a reflex corner
    const int skewVariant = 5;
    bool big = false;
    for(SSurface &ss : surface) {
        for(int i = 0; i <= ss.degm; i++) {
            for(int j = 0; j <= ss.degn; j++) {
                Vector c = ss.ctrl[i][j];
                if(fabs(c.x) > 50 || fabs(c.y) > 50 || fabs(c.z) > 50) big = true;
            }
        }
    }
    // straight_test: the same kind of profile (with a reflex vertex at
    // (40, 0)), extruded straight from z = -100 to 100. Every edge except
    // the concave lengthwise one.
    bool straight = false;
    for(SSurface &ss : surface) {
        if(fabs(ss.ctrl[0][0].z - 100) < LENGTH_EPS) straight = true;
    }
    if(big && straight) {
        bool concave = IsTestLine(b, 0, 40) && IsTestLine(b, 1, 0);
        return concave ? 0.0 : r;
    }
    if(big) {
        if(b.deg != 1) return 0.0;
        Vector p0 = b.Start(), p1 = b.Finish();
        bool side = fabs(p0.z - p1.z) > LENGTH_EPS;
        if(skewVariant == 1) return side ? r : 0.0;
        if(skewVariant == 6) {
            Vector cb = Vector::From(-20, 50, -62.45);
            bool cap = fabs(p0.z + 62.45) < 0.01 && fabs(p1.z + 62.45) < 0.01;
            return (cap && (p0.Equals(cb, 0.01) || p1.Equals(cb, 0.01))) ? r : 0.0;
        }
        if(skewVariant == 5) {
            Vector cb = Vector::From(-20, 50, -62.45),
                   ct = Vector::From(100, -50, 62.45);
            bool concave = (p0.Equals(cb, 0.01) && p1.Equals(ct, 0.01)) ||
                           (p1.Equals(cb, 0.01) && p0.Equals(ct, 0.01));
            return concave ? 0.0 : r;
        }
        if(skewVariant == 4) {
            Vector cb = Vector::From(-20, 50, -62.45),
                   ct = Vector::From(100, -50, 62.45);
            bool atReflex = p0.Equals(cb, 0.01) || p1.Equals(cb, 0.01) ||
                            p0.Equals(ct, 0.01) || p1.Equals(ct, 0.01);
            return atReflex ? 0.0 : r;
        }
        if(skewVariant == 3) {
            Vector top = Vector::From(120, 0, 62.45),
                   far = Vector::From(0, 0, 62.45),
                   bot = Vector::From(0, 100, -62.45);
            bool sideEdge = (p0.Equals(bot, 0.01) && p1.Equals(top, 0.01)) ||
                            (p1.Equals(bot, 0.01) && p0.Equals(top, 0.01));
            bool topEdge  = (p0.Equals(far, 0.01) && p1.Equals(top, 0.01)) ||
                            (p1.Equals(far, 0.01) && p0.Equals(top, 0.01));
            return (sideEdge || topEdge) ? r : 0.0;
        }
        Vector top = Vector::From(120, 0, 62.45),
               bot = Vector::From(-120, 30, -62.45);
        bool atCorner = p0.Equals(top, 0.01) || p1.Equals(top, 0.01) ||
                        p0.Equals(bot, 0.01) || p1.Equals(bot, 0.01);
        return atCorner ? r : 0.0;
    }

    // arc_test is the only model with exactly one curved surface: pick every
    // edge not in the bottom face (z = -10).
    int curved = 0;
    for(SSurface &ss : surface) {
        if(ss.degm > 1 || ss.degn > 1) curved++;
    }
    // lens_test (two arcs of radius 12 meeting at (0, +/-10), extruded) is
    // the only model with arcs of about 56 degrees (weight 0.881). Every cap
    // arc, and the vertical edges at the tips (between the two cylinders);
    // not the flat seams between the halves of each cylinder.
    bool lens = false;
    for(SCurve &c : curve) {
        if(c.isExact && c.exact.deg == 2 && fabs(c.exact.weight[1] - 0.881) < 0.005) {
            lens = true;
        }
    }
    //   0: those edges
    //   1: only the tip edges
    const int lensVariant = 0;
    if(lens) {
        if(b.deg == 2) return (lensVariant == 0) ? r : 0.0;
        bool tip = IsTestLine(b, 0, 0) && fabs(fabs(b.Start().y) - 10) < LENGTH_EPS;
        return tip ? r : 0.0;
    }

    // test_cube_boolean (the cube with a notch cut across its y = -10
    // face, z from -5 to 5) is the only model with 12 flat faces. The notch
    // splits the vertical edges at x = +/-10, y = -10 into two pieces each.
    //   0: those two split edges
    //   1: also the top face's outer edges and the x = 10, y = 10 edge (the
    //      cube's rules)
    const int booleanVariant = 1;
    if(surface.n == 12 && curved == 0) {
        bool split = IsTestLine(b, 1, -10) &&
                     (IsTestLine(b, 0, 10) || IsTestLine(b, 0, -10));
        if(split) return r;
        if(booleanVariant == 0) return 0.0;
    }

    // L_shape is the only model with 8 faces. Its top face (z = 10) has a
    // reflex corner at (0, 0). Variants:
    //   0: the rules below (outer top edges and the concave edge)
    //   1: only the top edge at y = 0, which must extend its neighbor past
    //      the reflex corner
    //   2: every edge of the top face, with a miter at the reflex corner
    const int lShapeVariant = 0;
    if(surface.n == 8 && curved == 0 && lShapeVariant != 0) {
        if(!IsTestLine(b, 2, 10)) return 0.0;
        if(lShapeVariant == 1) {
            bool reflexSide = IsTestLine(b, 1, 0) && b.Start().x > -LENGTH_EPS &&
                                                     b.Finish().x > -LENGTH_EPS;
            return reflexSide ? r : 0.0;
        }
        return r;
    }
    // Variant 1 instead: only the top radius at y = 0, which ends on the
    // flat side x = 0 at one end and on the cylinder wall at the other.
    // Variant 2: the top arc and the top radius at y = 0, meeting in a
    // miter of an arc and a line at (10, 0, 10). Variant 3: the top arc and
    // both top radii.
    const int arcVariant = 0;
    if(curved == 1 && arcVariant == 1) {
        return (IsTestLine(b, 2, 10) && IsTestLine(b, 1, 0)) ? r : 0.0;
    }
    if(curved == 1 && (arcVariant == 2 || arcVariant == 3)) {
        bool topArc = b.deg == 2 && fabs(b.Start().z - 10) < LENGTH_EPS &&
                                    fabs(b.Finish().z - 10) < LENGTH_EPS;
        bool rad0 = IsTestLine(b, 2, 10) && IsTestLine(b, 1, 0),
             rad1 = IsTestLine(b, 2, 10) && IsTestLine(b, 0, 0);
        return (topArc || rad0 || (arcVariant == 3 && rad1)) ? r : 0.0;
    }
    if(curved == 1) {
        bool bottom = fabs(b.Start().z + 10) < LENGTH_EPS &&
                      fabs(b.Finish().z + 10) < LENGTH_EPS;
        return bottom ? 0.0 : r;
    }
    //                       Variant 1 instead: the two roof edges at
    //                       z = 10, which end on the walls at 45 degrees.
    //                       Variant 2: the eaves and the wall edges at
    //                       z = 10, miters of unequal angles.
    const int houseVariant = 0;
    for(SCurve &other : curve) {
        if(IsTestSlantedLine(this, &other)) {
            if(houseVariant == 2) {
                // Eaves (135 degrees) and the wall edges at z = 10 (90),
                // meeting at the eave corners: miters of unequal angles.
                bool wallEnd = IsTestLine(b, 2, 10) &&
                               (IsTestLine(b, 0, 10) || IsTestLine(b, 0, -10));
                return (wallEnd || IsTestSlantedLine(this, &sc)) ? r : 0.0;
            }
            if(houseVariant == 1) {
                bool roofEnd = IsTestLine(b, 2, 10) &&
                               max(b.Start().y, b.Finish().y) > LENGTH_EPS;
                return roofEnd ? r : 0.0;
            }
            return IsTestSlantedLine(this, &sc) ? r : 0.0;
        }
    }
    if(IsTestLine(b, 2, 10) &&
       (IsTestLine(b, 0, 10) || IsTestLine(b, 0, -10) ||
        IsTestLine(b, 1, 10) || IsTestLine(b, 1, -10)))
    {
        return r;
    }
    if(IsTestLine(b, 0, 10) && IsTestLine(b, 1, 10)) {
        return r;
    }
    if(IsTestLine(b, 0, 0) && IsTestLine(b, 1, 0)) {
        return r;
    }
    if(b.deg == 2 && fabs(b.Start().z - 10) < LENGTH_EPS &&
                     fabs(b.Finish().z - 10) < LENGTH_EPS)
    {
        return r;
    }
    return 0.0;
}

} // namespace SolveSpace
