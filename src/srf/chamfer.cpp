//-----------------------------------------------------------------------------
// Algorithm for adding fillets and chamfers to an existing NURBS shell
// To create these features, edges are split into 2 new edges joined by
// a new surface which can be flat (chamfer) or round (fillet).
// Old: surfA, edge, surfB
// New: surfA, edge1, surfC, edge2, surfB
//
// Copyright 2024 Paul H Kahler.
//-----------------------------------------------------------------------------
//
// PLAN
// ====
//
// Scope for now
// -------------
// - Edges are picked by SShell::GetEdgeModifier(), a test hook. It returns 0
//   for no change, r for a fillet of radius r, or -r for a chamfer whose
//   edges sit where the fillet of radius r would meet the faces. Naming
//   edges properly (topological naming) comes later.
// - Faces A and B meet at 90 degrees, so the offset distance is r. For other
//   angles it would be r / tan(alpha/2), alpha being the angle between faces.
// - Edges meeting at a vertex all have the same radius. Differing radii are
//   not supported.
// - Only accept a curve with exactly one trim on surfA and one on surfB,
//   each covering the whole curve. Curves split by Booleans come later.
// - Corner patches (3 fillets at a vertex): a flat triangle for chamfers; for
//   fillets a triangle on the ball's sphere, made of three bands without any
//   collapsed edge (sphere_patch.cpp).
// - All new code goes in this file, except general curve tools such as
//   SBezier reparameterization, which go in ratpoly.cpp.
//
// Facts about the shell that drive the design
// -------------------------------------------
// - SSurface::trim is an unordered list of STrimBy. Loops are not stored.
//   Corners are found by matching trim start/finish points within a surface.
// - Trim start/finish must exactly equal points in the curve's pts list
//   (SSurface::MakeTrimEdgesInto walks pts and switches on Equals()). So any
//   trim that gets clipped or extended needs a new SCurve whose end points
//   are the new trim end points: rebuild the SBezier, then MakePwlInto().
// - Every curve is used by two trims, one per surface, running in opposite
//   directions. Knowing one trim's direction gives the other's backwards
//   flag.
// - Face interior: per SSurface::EdgeNormalsWithinSurface(), for a trim
//   running from start to finish with tangent t, the interior of the face is
//   on the side of (t x surface normal). Confirm on the test cube.
// - A surface or curve can be evaluated outside [0,1], but ClosestPointTo,
//   the uv BSP and triangulation assume [0,1]. Never rely on parameters
//   outside that range in the finished shell.
//
// Geometry for one edge e between faces A and B
// ---------------------------------------------
// - Offset direction: in each face, away from the other face, i.e. into the
//   face interior (see above). This one rule covers both cases:
//     convex edge  -> rounding, removes material
//     concave edge -> fillet, adds material
//   The edge is convex if the offset direction in A points along -nB.
// - Offset curves eA (in A) and eB (in B), distance r:
//     line in a plane          -> parallel line, exact
//     arc in its own plane     -> concentric arc, same weights, exact
//     curve on a surface perpendicular to a plane -> translate along the
//                                 plane normal, exact
//     cubic in a plane         -> approximate offset (to be discussed)
// - Fillet surface: 3 rows of control points, degn = 2
//     row 0 = eA, row 1 = e (weights times cos(45)), row 2 = eB
//   The tangents at the ends of a quarter circle meet at the original edge,
//   so the original edge is the middle row. Exact for line and arc edges.
//   Chamfer surface: rows eA and eB only, degn = 1.
//   degm = degree of e. Color is the default gray, face = 0.
// - Orientation: at the middle of the surface, its normal must point along
//   roughly (nA + nB), for convex and concave edges alike. If not, swap
//   rows 0 and 2.
// - Matching parameters: e, eA and eB must share the same parameterization,
//   so that rows line up point for point. When the ends move, extend or
//   shrink all three with the same new range [t0, t1] using
//       SBezier::Reparameterized(t0, t1)   (new, in ratpoly.cpp)
//   which works for lines, arcs and cubics, in homogeneous form for
//   rational curves. It must refuse to extend an arc to 180 degrees or more.
//   The surface uses the combined range covering both ends' cut points.
//   Each new trim curve is built separately over its own range, ending
//   exactly at its cut points. Trims work in xyz, so trim curves don't need
//   to share the surface's parameters.
//
// Vertices
// --------
// Each end of a modified edge gets a vertex record: the position, the curves
// meeting there (handle, which end, modified or not), the faces around it,
// and for each face whether its corner there is convex or reflex (over 180
// degrees inside the face). Convex face corners mean offsets meet within the
// existing curves, so neighbors get clipped. Reflex corners mean offsets (and
// possibly unmodified neighbors) need to be extended to meet.
//
// Vertex cases:
// 1. One modified edge, end face C perpendicular to e (cube edge).
//    eA meets neighbor f (between A and C), eB meets neighbor g (between B
//    and C). Re-cut f and g at those points. The fillet's end curve on C is
//    the fillet surface's end row: control points (cutA, V, cutB), weight
//    cos(45). Face C gets that curve in place of the corner. The same works
//    for convex and concave edges.
// 2. Tangent chain: the neighbor is tangent to e and also modified (rounded
//    rectangle, or a circle split into arcs). Test for tangency first;
//    intersecting tangent curves is unreliable. The offset curves already
//    share an end point (V moved along the shared direction), so there is
//    nothing to clip and no end curve.
// 3. Two modified edges e and f sharing face A, meeting at a corner (two top
//    edges of the cube). eA and fA meet in A. The two fillets meet along a
//    curve with control points (eA x fA, V, point on g), weight cos(45). It
//    is exact, being a parallel projection of the cross-section circle. For
//    chamfers it is a line. This curve is not a parameter line of either
//    fillet surface, so each surface is built a bit longer and trimmed by it.
//    That holds between flat faces at equal angles. Otherwise (straight
//    edges) the miter is traced, see TraceMiterEnd(): it runs from the
//    corner in A until it reaches one fillet's contact with its other face,
//    at X. Past X the other fillet cuts into that face, down to g. So one
//    fillet ends in two pieces (miter, face cut), the other in one (miter),
//    and only the first re-cuts g.
// 4. Three modified edges (cube corner). Not miters: in each of its two
//    faces, e's offset crosses the neighbor's offset, and e's fillet ends at
//    the cross-section through those two points (same parameter at a square
//    corner). The three cross-sections bound the corner patch: a sphere
//    octant centered where the three fillet axes meet, or a flat triangle
//    for chamfers. The chamfer triangle is FromPlane, trimmed by the three
//    end curves, facing the same way as the chamfers. The sphere triangle
//    is split at its incenter into three bands, see sphere_patch.cpp.
// An end face at an angle to a straight edge is exact too: the end curve is
// the cross-section projected along the edge onto it (an ellipse arc).
// Anything else (a curved edge or a curved end face at an angle, etc.) is
// unsupported for now,
// and then no edge is changed. In general that needs
// SSurface::IntersectAgainst().
//
// Steps in ModifyEdges()
// ----------------------
// Compute everything first and change the shell only at the end, so a
// failure leaves the shell untouched. Never hold pointers into curve or
// surface across an AddAndAssignId(): the lists can reallocate.
// 1. Collect: loop over the original curve.n only. Store an EdgeMod by value
//    for each curve GetEdgeModifier() picks.
// 2. Vertices: build a vertex record for each end of each EdgeMod and
//    classify it. If any end is unsupported, drop that edge.
// 3. Offset: build eA and eB.
// 4. Cut: find the cut points at each vertex, per its case. Use
//    SBezier::AllIntersectionsWith() and take the point nearest V.
// 5. Build: reparameterize e, eA and eB to their common range; build the
//    fillet/chamfer surfaces, the clipped eA/eB trim curves, the re-cut
//    neighbor curves, and the end and miter curves.
// 6. Commit: add the new curves and surfaces. Rewrite trims:
//      A: trim on e -> trim on eA; neighbors -> their re-cut copies
//      B: same with eB
//      C (end faces): add the end curve, use re-cut neighbors
//      fillet surface: eA, end curve, eB, end curve (or miter curves)
//    Tag the dead curves (e and the old neighbors) and remove them.
//
// Data structures (plain structs)
// -------------------------------
// EdgeMod: one edge to modify, indexed by side s ([0] face A, [1] face B)
// and end k ([0] at e.Start(), [1] at e.Finish()). Recut: an unmodified
// neighbor that gets shortened. See their definitions below.
//
// Test order (cube with all coordinates at +/-10)
// -----------------------------------------------
// 1. Chamfer one top edge: line between two planes, case 1 at both ends.
// 2. The same edge as a fillet.
// 3. A concave edge, e.g. an L-shaped extrusion.
// 4. Top edge of a cylinder: arcs in a tangent chain (case 2).
// 5. Two top edges of the cube meeting at a corner (case 3).
// 6. Three edges at a corner (case 4): flat triangle or sphere patch.
// Check each time: offset direction, surface normal, trim directions, and no
// "trim was unterminated" / "trim was empty" messages.
//
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

// Vertex cases, see the plan above.
static const int END_FACE = 1;  // case 1: neighbors not modified, end face C
static const int TANGENT  = 2;  // case 2: tangent chain of modified edges
static const int MITER    = 3;  // case 3: two modified edges meet at a corner
static const int CORNER   = 4;  // case 4: three modified edges at a corner

// One edge to modify. Stored by value: no pointers into the shell's lists,
// which can reallocate when things are added.
// Index s is the side: [0] is face A (the curve's surfA), [1] is face B.
// Index k is the end:  [0] at e.Start(), [1] at e.Finish().
struct EdgeMod {
    hSCurve   orig;           // the original edge
    double    r;
    bool      chamfer;
    // The rolling ball, from SetUpSections(). Every cross-section of the
    // fillet is an arc of the ball from off[0] to off[1], with its middle
    // control point on mid and that point's weight w.
    bool      convex;         // ball in the material (rounding), not the air
    SBezier   spine;          // the ball's center
    SBezier   mid;            // where the tangents at the contacts cross
    double    w;              // cos(half the arc angle); cos(45) when square
    SBezier   e;              // the original edge
    hSSurface face[2];        // [s]
    int       trim[2];        // [s] index of the edge's trim in face[s]
    SBezier   off[2];         // [s] offset curves eA, eB: where the ball
                              //     touches face[s]
    Vector    vtx[2];         // [k] original vertex
    hSCurve   nb[2][2];       // [s][k] neighbor curve in face[s] at vtx[k]
    int       kind[2];        // [k] END_FACE, TANGENT, MITER or CORNER
    int       modSide[2];     // [k] TANGENT, MITER: side of the modified neighbor
    hSSurface sC[2];          // [k] END_FACE: end face C. CORNER: the corner
                              //     patch, or 0 if there is none yet
    Vector    cut[2][2];      // [s][k] where off[s] ends
    Vector    endMid[2];      // [k] END_FACE: the end curve's middle control
                              //     point, on the end face
    std::vector<Vector> endPts[2]; // [k] END_FACE on a curved face: the end
                              //     curve as points (not exact)
    // MITER between edges whose faces meet at different angles, or curved
    // faces: traced, not the exact ellipse. The end is one or two pieces, in
    // order from cut[0][k] to cut[1][k]: the miter shared with the partner
    // fillet, and maybe a piece where this fillet cuts into the partner's
    // other face. Points only, not exact.
    bool      traced[2];      // [k]
    int       nPiece[2];      // [k]
    std::vector<Vector> piecePts[2][2]; // [k][piece]
    bool      pieceOnFace[2][2]; // [k][piece] on the face, not the partner
    hSSurface pieceFace[2];   // [k] that face
    Vector    xPt[2];         // [k] where the miter meets the face piece, or
                              //     g if there is none
    hSCurve   hPiece[2][2];   // [k][piece]
    bool      cutsG[2];       // [k] MITER: this fillet ends on g and re-cuts it
    double    tSrf[2];        // [k] ends of the fillet surface, as params on e
    int       partner[2], partnerEnd[2]; // [k] TANGENT, MITER: the neighbor's EdgeMod
    // Geometry built before the shell is changed
    SBezier   newOff[2], endC[2];
    SSurface  fillet;
    // Handles assigned when the shell is changed
    hSCurve   hNew[2], hEndC[2];
    hSSurface hFillet;
};

// Three fillets or chamfers meeting at a corner (case 4). Chamfers are
// closed by a flat triangle; fillets by three bands on the ball's sphere
// (sphere_patch.cpp), which meet along arcs from each corner to the
// triangle's incenter.
struct Corner {
    Vector    vtx;
    int       n;
    int       mod[3], end[3]; // the EdgeMod ends that meet here
    bool      sphere;
    Vector    p[3];           // the triangle's corners
    int       side[3];        // [member] the patch its end curve borders
    Vector    t;              // sphere: the incenter
    SSurface  patch[3];       // chamfer: patch[0]; sphere: band per side
    SBezier   arc[3];         // sphere: arc[i] from p[i] to t
    hSSurface h[3];
    hSCurve   hArc[3];
};

// A neighbor curve that gets shortened at one or both ends.
struct Recut {
    hSCurve   orig;
    Vector    start, finish;  // new ends, in the curve's own direction
    SBezier   b;
    hSCurve   newh;
};

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
    // Test hook until edges can be named. Picks edges in the test models in
    // misc_CAD:
    //   test_cube, L_shape: the outer lines around the z = 10 face, which
    //                       meet at corners
    //   test_cube:          the line at x = 10, y = 10, making three edges
    //                       at the corner (10, 10, 10)
    //   L_shape:            the line at x = 0, y = 0 (concave)
    //   test_cylinder:      the arcs of the top circle at z = 10
    //   arc_test:           every edge not in the bottom face (see below)
    //   skew_test, straight_test: see below
    //   house_test:         lines between planes not at 90 degrees (eaves).
    //                       If a shell has any of these, only they are
    //                       picked; the rules above would also pick lines in
    //                       the house.
    const double r = 3.0;   // negative for chamfers
    if(!sc.isExact) return 0.0;
    const SBezier &b = sc.exact;
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
    const int skewVariant = 4;
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
    const int arcVariant = 0;
    if(curved == 1 && arcVariant == 1) {
        return (IsTestLine(b, 2, 10) && IsTestLine(b, 1, 0)) ? r : 0.0;
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

// Index of the one trim in s that uses curve hc, or -1 if there are none or
// more than one.
static int FindOnlyTrim(SSurface *s, hSCurve hc) {
    int found = -1;
    for(int i = 0; i < s->trim.n; i++) {
        if(s->trim[i].curve.v == hc.v) {
            if(found >= 0) return -1;
            found = i;
        }
    }
    return found;
}

static bool TrimCoversCurve(SCurve *sc, STrimBy *stb) {
    Vector first = sc->pts[0].p,
           last  = sc->pts[sc->pts.n - 1].p;
    if(stb->backwards) {
        return stb->start.Equals(last) && stb->finish.Equals(first);
    } else {
        return stb->start.Equals(first) && stb->finish.Equals(last);
    }
}

// True if the curve is exact and has exactly one trim on each of its
// surfaces, each covering the whole curve.
static bool IsSimpleCurve(SShell *sh, SCurve *sc) {
    if(!sc->isExact) return false;
    hSSurface hs[2] = { sc->surfA, sc->surfB };
    for(int i = 0; i < 2; i++) {
        SSurface *s = sh->surface.FindByIdNoOops(hs[i]);
        if(s == NULL) return false;
        int it = FindOnlyTrim(s, sc->h);
        if(it < 0 || !TrimCoversCurve(sc, &s->trim[it])) return false;
    }
    return true;
}

static hSSurface OtherSurface(SCurve *sc, hSSurface hs) {
    return (sc->surfA.v == hs.v) ? sc->surfB : sc->surfA;
}

static bool SurfaceIsPlane(SSurface *s, Vector *n, double *d) {
    if(s->degm != 1 || s->degn != 1) return false;
    *n = s->NormalAt(0.5, 0.5).WithMagnitude(1.0);
    *d = n->Dot(s->ctrl[0][0]);
    return s->CoincidentWithPlane(*n, *d);
}

static Vector SurfaceNormalNear(SSurface *s, Vector p) {
    Point2d uv;
    s->ClosestPointTo(p, &uv, /*mustConverge=*/false);
    return s->NormalAt(uv).WithMagnitude(1.0);
}

// Unit vector at e(t), within surface s and perpendicular to e, pointing into
// the face. backwards is the flag of e's trim on s. Per
// SSurface::EdgeNormalsWithinSurface() the interior is on the side of
// (trim direction x surface normal).
static Vector InwardAt(SSurface *s, const SBezier &e, bool backwards, double t) {
    Vector tan = e.TangentAt(t);
    if(backwards) tan = tan.ScaledBy(-1.0);
    Vector n = SurfaceNormalNear(s, e.PointAt(t));
    return tan.Cross(n).WithMagnitude(1.0);
}

// Parameter of point p, which lies on (or on the extension of) curve b.
static double ParamOn(const SBezier &b, Vector p) {
    if(b.deg == 1) {
        Vector d = b.ctrl[1].Minus(b.ctrl[0]);
        return (p.Minus(b.ctrl[0])).Dot(d) / d.MagSquared();
    }
    double t;
    b.ClosestPointTo(p, &t, /*mustConverge=*/false);
    return t;
}

// True if the end of neighbor curve b at vertex v can move to p, which is on
// b or its extension: shortening b (a convex corner of the face) or
// lengthening it past v (a reflex corner), but not past b's other end.
static bool CanMoveEnd(const SBezier &b, Vector v, Vector p) {
    double t = ParamOn(b, p);
    if(!b.PointAt(t).Equals(p)) return false;
    if(b.Start().Equals(v))  return t < 1 - LENGTH_EPS;
    if(b.Finish().Equals(v)) return t > LENGTH_EPS;
    return false;
}

// The circle that arc b lies on: center, radius and unit normal. False if b
// is not a circular arc.
static bool ArcCircle(const SBezier &b, Vector *center, double *rad, Vector *n) {
    if(b.deg != 2) return false;
    *n = (b.ctrl[1].Minus(b.ctrl[0])).Cross(b.ctrl[2].Minus(b.ctrl[1]));
    if(n->Magnitude() < LENGTH_EPS) return false;
    *n = n->WithMagnitude(1.0);
    return b.IsCircle(*n, center, rad);
}

// Where the line through l0 and l1 meets a circle. A line crossing the
// circle's plane meets it at most once; a line in the plane can meet it
// twice, and then we take the point nearest to near.
static bool LineMeetsCircle(Vector l0, Vector l1, Vector center, double rad,
                            Vector n, Vector near, Vector *p)
{
    Vector dir = l1.Minus(l0);
    double dn = dir.Dot(n);
    if(fabs(dn) > LENGTH_EPS * dir.Magnitude()) {
        double t = (center.Minus(l0)).Dot(n) / dn;
        *p = l0.Plus(dir.ScaledBy(t));
        return fabs((p->Minus(center)).Magnitude() - rad) < LENGTH_EPS;
    }
    if(fabs((l0.Minus(center)).Dot(n)) > LENGTH_EPS) return false;

    Vector m = l0.Minus(center);
    double a = dir.Dot(dir), b = 2*m.Dot(dir), c = m.Dot(m) - rad*rad;
    double disc = b*b - 4*a*c;
    if(disc < -LENGTH_EPS) return false;
    double sq = sqrt(max(0.0, disc));
    Vector p1 = l0.Plus(dir.ScaledBy((-b + sq) / (2*a))),
           p2 = l0.Plus(dir.ScaledBy((-b - sq) / (2*a)));
    *p = ((p1.Minus(near)).Magnitude() < (p2.Minus(near)).Magnitude()) ? p1 : p2;
    return true;
}

// Where two curves, extended if needed, meet; of several points, the one
// nearest to near (the vertex). Lines, and a line with an arc, for now.
static bool IntersectCarriers(const SBezier &a, const SBezier &b, Vector near,
                              Vector *p)
{
    if(a.deg == 1 && b.deg == 1) {
        Vector da = a.ctrl[1].Minus(a.ctrl[0]),
               db = b.ctrl[1].Minus(b.ctrl[0]);
        if((da.Cross(db)).Magnitude() < LENGTH_EPS * da.Magnitude()) return false;
        bool skew;
        *p = Vector::AtIntersectionOfLines(a.ctrl[0], a.ctrl[1],
                                           b.ctrl[0], b.ctrl[1], &skew);
        return !skew;
    }
    const SBezier *line = (a.deg == 1) ? &a : &b,
                  *arc  = (a.deg == 1) ? &b : &a;
    Vector center, n;
    double rad;
    if(line->deg != 1 || !ArcCircle(*arc, &center, &rad, &n)) return false;
    return LineMeetsCircle(line->ctrl[0], line->ctrl[1], center, rad, n, near, p);
}

// How a face looks in the cross-section plane through an edge point (the
// plane perpendicular to the edge). Two kinds so far:
//   line:   a straight line of the face runs away from the edge: a plane, or
//           a cylinder cut across its axis (an arc edge on a cylinder wall)
//   circle: a cylinder whose axis runs along the edge
struct FaceSection {
    bool    isCircle;
    Vector  p;          // the edge point
    Vector  dir;        // unit direction into the face, away from the edge
    Vector  n;          // outward unit normal at p
    Vector  center;     // circle: center, in the cross-section plane
    double  rad;        // circle: radius
};

static bool GetFaceSection(SSurface *s, const SBezier &e, bool backwards,
                           double t, FaceSection *fs)
{
    fs->p   = e.PointAt(t);
    fs->dir = InwardAt(s, e, backwards, t);
    fs->n   = SurfaceNormalNear(s, fs->p);
    Vector tan = e.TangentAt(t).WithMagnitude(1.0);

    Vector pn;
    double pd;
    if(SurfaceIsPlane(s, &pn, &pd)) {
        fs->isCircle = false;
        return true;
    }
    Vector axis, center, st, fi;
    double rad;
    if(s->IsCylinder(&axis, &center, &rad, &st, &fi)) {
        axis = axis.WithMagnitude(1.0);
        if(fabs(fabs(axis.Dot(tan)) - 1.0) < 1e-6) {
            fs->isCircle = true;
            fs->center   = center.Plus(axis.ScaledBy((fs->p.Minus(center)).Dot(axis)));
            fs->rad      = rad;
            return true;
        }
        if(fabs(fabs(axis.Dot(fs->dir)) - 1.0) < 1e-6) {
            fs->isCircle = false;
            return true;
        }
    }
    return false;
}

// The ball in the cross-section at e(t): its center, where it touches each
// face, where the tangents at those points cross, and the weight for the arc
// between them.
struct BallSection {
    bool    convex;
    Vector  c;
    Vector  contact[2];
    Vector  mid;
    double  w;
};

static bool SolveBallSection(SShell *sh, EdgeMod *em, double t, BallSection *bs) {
    FaceSection fs[2];
    for(int s = 0; s < 2; s++) {
        SSurface *srf = sh->surface.FindById(em->face[s]);
        if(!GetFaceSection(srf, em->e, srf->trim[em->trim[s]].backwards, t, &fs[s])) {
            dbp("fillet: face type not supported yet");
            return false;
        }
    }
    if(fs[0].isCircle && fs[1].isCircle) {
        dbp("fillet: edge between two cylinders not supported yet");
        return false;
    }
    // Convex edge: the ball is in the material, inside both faces. Concave:
    // it is in the air, outside both.
    bool convex = fs[0].dir.Dot(fs[1].n) < 0;
    bs->convex = convex;
    double sg = convex ? -1.0 : 1.0;
    Vector p = fs[0].p,
           tan = em->e.TangentAt(t).WithMagnitude(1.0);
    double r = em->r;

    // Each face offset by r toward the ball; the center is where they meet.
    if(!fs[0].isCircle && !fs[1].isCircle) {
        Vector a0 = p.Plus(fs[0].n.ScaledBy(sg*r)),
               b0 = p.Plus(fs[1].n.ScaledBy(sg*r));
        if((fs[0].dir.Cross(fs[1].dir)).Magnitude() < 1e-6) {
            dbp("fillet: faces are nearly flat or folded back");
            return false;
        }
        bool skew;
        bs->c = Vector::AtIntersectionOfLines(a0, a0.Plus(fs[0].dir),
                                              b0, b0.Plus(fs[1].dir), &skew);
        if(skew) return false;
    } else {
        int li = fs[0].isCircle ? 1 : 0, ci = 1 - li;
        Vector l0 = p.Plus(fs[li].n.ScaledBy(sg*r));
        double orad = ((p.Plus(fs[ci].n.ScaledBy(sg*r))).Minus(fs[ci].center)).Magnitude();
        if(!LineMeetsCircle(l0, l0.Plus(fs[li].dir), fs[ci].center, orad, tan,
                            p, &bs->c))
        {
            dbp("fillet: radius too big for this edge");
            return false;
        }
    }

    // Where the ball touches each face, and the tangents there.
    Vector td[2];
    for(int s = 0; s < 2; s++) {
        if(fs[s].isCircle) {
            Vector rc = bs->c.Minus(fs[s].center);
            bs->contact[s] = fs[s].center.Plus(rc.WithMagnitude(fs[s].rad));
        } else {
            bs->contact[s] = bs->c.Minus(fs[s].n.ScaledBy(sg*r));
        }
        td[s] = tan.Cross(bs->c.Minus(bs->contact[s]));
    }
    bool skew;
    bs->mid = Vector::AtIntersectionOfLines(bs->contact[0], bs->contact[0].Plus(td[0]),
                                            bs->contact[1], bs->contact[1].Plus(td[1]),
                                            &skew);
    if(skew) return false;
    Vector r0 = (bs->contact[0].Minus(bs->c)).WithMagnitude(1.0),
           r1 = (bs->contact[1].Minus(bs->c)).WithMagnitude(1.0);
    double theta = acos(max(-1.0, min(1.0, r0.Dot(r1))));
    bs->w = cos(theta / 2);
    return true;
}

// The curve that point q, in the cross-section at e(0), traces when carried
// along e the way every section point is: a translate of e for a line, or a
// concentric arc about the same axis for an arc.
static bool FollowEdge(const SBezier &e, Vector q, SBezier *out) {
    if(e.deg == 1) {
        *out = e.TransformedBy(q.Minus(e.Start()), Quaternion::IDENTITY, 1.0);
        return true;
    }
    Vector center, n;
    double rad;
    if(!ArcCircle(e, &center, &rad, &n)) return false;
    double h = (q.Minus(center)).Dot(n);
    Vector radial = q.Minus(center).Minus(n.ScaledBy(h));
    double rho = radial.Magnitude();
    if(rho < LENGTH_EPS) return false;
    *out = e;
    for(int i = 0; i <= e.deg; i++) {
        out->ctrl[i] = center.Plus(n.ScaledBy(h)).Plus(
            (e.ctrl[i].Minus(center)).ScaledBy(rho / rad));
    }
    return true;
}

// The rolling ball along the whole edge: the offset curves where it touches
// each face, the spine, and the middle row. Solved at the start of the edge
// and carried along; the section at the end must agree, or the faces aren't
// a kind this can do.
static bool SetUpSections(SShell *sh, EdgeMod *em) {
    BallSection b0, b1;
    if(!SolveBallSection(sh, em, 0.0, &b0) || !SolveBallSection(sh, em, 1.0, &b1)) {
        return false;
    }
    if(!FollowEdge(em->e, b0.contact[0], &em->off[0]) ||
       !FollowEdge(em->e, b0.contact[1], &em->off[1]) ||
       !FollowEdge(em->e, b0.c, &em->spine) ||
       !FollowEdge(em->e, b0.mid, &em->mid))
    {
        dbp("fillet: can't carry the cross-section along this edge");
        return false;
    }
    if(!em->off[0].Finish().Equals(b1.contact[0]) ||
       !em->off[1].Finish().Equals(b1.contact[1]) ||
       !em->mid.Finish().Equals(b1.mid) || fabs(b0.w - b1.w) > 1e-6)
    {
        dbp("fillet: cross-section changes along the edge, not supported yet");
        return false;
    }
    em->w      = b0.w;
    em->convex = b0.convex;
    return true;
}

// Index of the one trim in s, not on curve exclude, that starts or finishes
// at v. -1 if there are none or more than one.
static int FindTrimAtVertex(SSurface *s, hSCurve exclude, Vector v) {
    int found = -1;
    for(int i = 0; i < s->trim.n; i++) {
        STrimBy *stb = &s->trim[i];
        if(stb->curve.v == exclude.v) continue;
        if(stb->start.Equals(v) || stb->finish.Equals(v)) {
            if(found >= 0) return -1;
            found = i;
        }
    }
    return found;
}

static int FindEdgeMod(const std::vector<EdgeMod> &mods, hSCurve hc) {
    for(size_t i = 0; i < mods.size(); i++) {
        if(mods[i].orig.v == hc.v) return (int)i;
    }
    return -1;
}

// Case 1 at end k: neither neighbor is modified. f (in A) and g (in B) share
// end face C, which must be a plane perpendicular to e. The offsets are cut
// where they meet f and g, within those curves.
// What a traced ray can stop on: a straight fillet (swept, solved exactly)
// and/or a surface (by raycast). The ray is q + t*d; it starts at edge
// parameter tFrom (inside the fillet, where nothing is in the way) and goes
// in direction dir (+1 or -1, toward the vertex), and it stops at the first
// thing it hits. which is 0 for the fillet, 1 for the surface.
struct RayTargets {
    const EdgeMod *sweep;
    SSurface      *srf;
    double         tFrom;
    double         dir;
};

static SBezier StartSection(const EdgeMod *em);

// Keep hit (ts, p) if it is ahead of tFrom and before the best so far.
static void ConsiderHit(const RayTargets &rt, double ts, Vector p, int i,
                        bool *found, double *t, Vector *pp, int *which)
{
    if((ts - rt.tFrom) * rt.dir <= 0) return;
    if(*found && (ts - *t) * rt.dir >= 0) return;
    *t = ts;
    *pp = p;
    *which = i;
    *found = true;
}

// Where the line q + t*d hits a straight fillet, which is its start section
// swept along its edge. Looking along the edge, the fillet is just the
// section curve, and the line is a 2D line; where they cross is a quadratic
// (linear for a chamfer) in the section parameter. Only crossings within the
// section (0 to 1) count.
static void HitsOnSweep(const RayTargets &rt, Vector q, Vector d,
                        bool *found, double *t, Vector *pp, int *which)
{
    const EdgeMod *sw = rt.sweep;
    SBezier sec = StartSection(sw);
    Vector ax = (sw->e.Finish().Minus(sw->e.Start())).WithMagnitude(1.0),
           o  = sec.ctrl[0];
    // Project along the edge onto the section plane.
    Vector pq = q.Minus(ax.ScaledBy((q.Minus(o)).Dot(ax))),
           pd = d.Minus(ax.ScaledBy(d.Dot(ax)));
    if(pd.Magnitude() < LENGTH_EPS * d.Magnitude()) return;
    Vector n = ax.Cross(pd);
    double a[3], w[3];
    for(int i = 0; i <= sec.deg; i++) {
        Vector pc = sec.ctrl[i].Minus(ax.ScaledBy((sec.ctrl[i].Minus(o)).Dot(ax)));
        a[i] = n.Dot(pc.Minus(pq));
        w[i] = sec.weight[i];
    }
    double roots[2];
    int nr = 0;
    if(sec.deg == 1) {
        if(fabs(a[0] - a[1]) > 1e-12) roots[nr++] = a[0] / (a[0] - a[1]);
    } else {
        double A = w[0]*a[0] - 2*w[1]*a[1] + w[2]*a[2],
               B = 2*(w[1]*a[1] - w[0]*a[0]),
               C = w[0]*a[0];
        if(fabs(A) < 1e-12) {
            if(fabs(B) > 1e-12) roots[nr++] = -C / B;
        } else {
            double disc = B*B - 4*A*C;
            if(disc >= 0) {
                double sq = sqrt(disc);
                roots[nr++] = (-B + sq) / (2*A);
                roots[nr++] = (-B - sq) / (2*A);
            }
        }
    }
    for(int i = 0; i < nr; i++) {
        double sg = roots[i];
        if(sg < -1e-9 || sg > 1 + 1e-9) continue;
        Vector x  = sec.PointAt(sg),
               px = x.Minus(ax.ScaledBy((x.Minus(o)).Dot(ax)));
        double ts = (px.Minus(pq)).Dot(pd) / pd.MagSquared();
        ConsiderHit(rt, ts, q.Plus(d.ScaledBy(ts)), 0, found, t, pp, which);
    }
}

static bool FirstHit(const RayTargets &rt, Vector q, Vector d,
                     double *t, Vector *p, int *which)
{
    bool found = false;
    if(rt.sweep) HitsOnSweep(rt, q, d, &found, t, p, which);
    if(rt.srf) {
        List<SInter> l = {};
        rt.srf->AllPointsIntersecting(q, q.Plus(d), &l, /*asSegment=*/false,
                                      /*trimmed=*/false, /*inclTangent=*/true);
        for(SInter &si : l) {
            double ts = (si.p.Minus(q)).Dot(d) / d.MagSquared();
            ConsiderHit(rt, ts, si.p, 1, &found, t, p, which);
        }
        l.Clear();
    }
    return found;
}

// Add to pts the points of the end curve for section parameters in (s0, s1],
// where p1 is already known. Splits until each chord is within the chord
// tolerance. Tracks the range of edge parameters reached.
static bool TraceEndCurve(const RayTargets &rt, const SBezier &sec, Vector d,
                          double s0, Vector p0, double s1, Vector p1, int depth,
                          std::vector<Vector> *pts, double *tmin, double *tmax)
{
    double sm = (s0 + s1) / 2, tm;
    Vector pm;
    int which;
    if(!FirstHit(rt, sec.PointAt(sm), d, &tm, &pm, &which)) return false;
    *tmin = min(*tmin, tm);
    *tmax = max(*tmax, tm);
    double dev = (pm.Minus(p0.Plus(p1).ScaledBy(0.5))).Magnitude();
    if(depth < 2 || (depth < 10 && dev > SS.ChordTolMm())) {
        return TraceEndCurve(rt, sec, d, s0, p0, sm, pm, depth + 1, pts, tmin, tmax) &&
               TraceEndCurve(rt, sec, d, sm, pm, s1, p1, depth + 1, pts, tmin, tmax);
    }
    pts->push_back(p1);
    return true;
}

// The cross-section of the fillet at the start of the edge, from off[0] to
// off[1]; a point at section parameter s and edge parameter t is
// sec(s) + t*d, d running along the (straight) edge.
static SBezier StartSection(const EdgeMod *em) {
    if(em->chamfer) {
        return SBezier::From(em->off[0].Start(), em->off[1].Start());
    }
    return SBezier::From(Vector4::From(1.0, em->off[0].Start()),
                         Vector4::From(em->w, em->mid.Start()),
                         Vector4::From(1.0, em->off[1].Start()));
}

// Case 1 at end k: neither neighbor is modified. f (in A) and g (in B) share
// end face C. The offsets are cut where they meet f and g, which are
// shortened, or extended past the vertex at a reflex corner. The end curve is
// where the fillet meets C:
//   C perpendicular to e:  a cross-section of the fillet
//   C a plane at an angle: (straight edges) the cross-section projected
//                          along the edge onto C, an ellipse arc
//   C curved:              (straight edges) traced: each point of the
//                          cross-section moves along a line parallel to the
//                          edge, and the curve is where those lines hit C.
//                          Kept as points, not exact.
static bool SetUpEndFace(SShell *sh, EdgeMod *em, int k) {
    SCurve *f = sh->curve.FindById(em->nb[0][k]),
           *g = sh->curve.FindById(em->nb[1][k]);
    if(!IsSimpleCurve(sh, f) || !IsSimpleCurve(sh, g)) {
        dbp("fillet: neighbor is not a simple exact curve");
        return false;
    }
    hSSurface hc = OtherSurface(f, em->face[0]);
    if(hc.v != OtherSurface(g, em->face[1]).v) {
        dbp("fillet: neighbors don't share an end face");
        return false;
    }
    SSurface *sc = sh->surface.FindById(hc);
    Vector nc;
    double dc;
    bool plane = SurfaceIsPlane(sc, &nc, &dc);
    Vector tan = em->e.TangentAt((k == 0) ? 0.0 : 1.0).WithMagnitude(1.0);
    if(!plane) nc = SurfaceNormalNear(sc, em->vtx[k]);
    bool perpendicular = plane && fabs(fabs(tan.Dot(nc)) - 1.0) < 1e-6;
    if(!perpendicular && em->e.deg != 1) {
        dbp("fillet: curved edge meeting its end face at an angle, not supported yet");
        return false;
    }
    if(fabs(tan.Dot(nc)) < 1e-3) {
        dbp("fillet: edge nearly parallel to its end face");
        return false;
    }
    if(!IntersectCarriers(em->off[0], f->exact, em->vtx[k], &em->cut[0][k]) ||
       !IntersectCarriers(em->off[1], g->exact, em->vtx[k], &em->cut[1][k]))
    {
        dbp("fillet: offset curve doesn't meet neighbor");
        return false;
    }
    if(!CanMoveEnd(f->exact, em->vtx[k], em->cut[0][k]) ||
       !CanMoveEnd(g->exact, em->vtx[k], em->cut[1][k]))
    {
        dbp("fillet: neighbor is too short for this radius");
        return false;
    }
    double t0 = ParamOn(em->off[0], em->cut[0][k]),
           t1 = ParamOn(em->off[1], em->cut[1][k]);
    if(perpendicular) {
        // The end curve is a cross-section, so both cuts are at one
        // parameter.
        if(fabs(t0 - t1) > 1e-6) {
            dbp("fillet: cuts at different parameters");
            return false;
        }
        em->tSrf[k]   = t0;
        em->endMid[k] = em->mid.PointAt(t0);
    } else if(plane) {
        // A straight fillet is its cross-section swept along the edge, so
        // where it meets the end face is a cross-section projected along the
        // edge onto that plane: an ellipse arc with the same weight. Its
        // middle control point is where the middle row crosses the plane.
        // The curve lies within its control points, so the surface must
        // reach all three.
        bool parallel;
        em->endMid[k] = Vector::AtIntersectionOfPlaneAndLine(nc, dc,
                            em->mid.ctrl[0], em->mid.ctrl[1], &parallel);
        if(parallel) return false;
        double tm = ParamOn(em->mid, em->endMid[k]);
        em->tSrf[k] = (k == 0) ? min(tm, min(t0, t1)) : max(tm, max(t0, t1));
    } else {
        SBezier sec = StartSection(em);
        Vector d = em->e.Finish().Minus(em->e.Start());
        RayTargets rt = {};
        rt.srf    = sc;
        rt.tFrom  = 0.5;
        rt.dir    = (k == 0) ? -1.0 : 1.0;
        double tmin = min(t0, t1), tmax = max(t0, t1);
        std::vector<Vector> pts;
        pts.push_back(em->cut[0][k]);
        if(!TraceEndCurve(rt, sec, d, 0.0, em->cut[0][k], 1.0, em->cut[1][k],
                          0, &pts, &tmin, &tmax))
        {
            dbp("fillet: can't find where the fillet meets its end face");
            return false;
        }
        em->endPts[k] = pts;
        em->tSrf[k]   = (k == 0) ? tmin : tmax;
    }
    em->kind[k] = END_FACE;
    em->sC[k]   = hc;
    return true;
}

// Case 2 at end k: the neighbor in face s is modified and tangent to e. The
// offsets already end where the neighbor's start, so the cuts are just their
// end points. The neighbor g on the other side gets re-cut.
static bool SetUpTangentEnd(SShell *sh, EdgeMod *em, int k, int s) {
    int o = 1 - s;
    SCurve *f = sh->curve.FindById(em->nb[s][k]),
           *g = sh->curve.FindById(em->nb[o][k]);
    if(!IsSimpleCurve(sh, g)) {
        dbp("fillet: neighbor is not a simple exact curve");
        return false;
    }
    // The tangent neighbor's other face must be on the other side of g.
    if(OtherSurface(g, em->face[o]).v != OtherSurface(f, em->face[s]).v) {
        dbp("fillet: tangent neighbors don't share a face");
        return false;
    }
    em->tSrf[k] = (k == 0) ? 0.0 : 1.0;
    for(int i = 0; i < 2; i++) {
        em->cut[i][k] = (k == 0) ? em->off[i].Start() : em->off[i].Finish();
    }
    if(!CanMoveEnd(g->exact, em->vtx[k], em->cut[o][k])) {
        dbp("fillet: offset doesn't land on the neighbor");
        return false;
    }
    em->kind[k]    = TANGENT;
    em->modSide[k] = s;
    return true;
}

// Case 3 at end k: the neighbor f in face s (the shared face S) is modified
// and meets e at a corner. Straight edges only for now. In S the offsets of
// e and f meet each other. On the other side, the offset meets g, the edge
// between e's other face and f's other face. The two fillets meet along the
// miter curve, from the corner in S, through the vertex, to the cut on g.
static bool SetUpMiterEnd(SShell *sh, const std::vector<EdgeMod> &mods,
                          EdgeMod *em, int k, int s)
{
    int o = 1 - s;
    const EdgeMod &fm = mods[FindEdgeMod(mods, em->nb[s][k])];
    SCurve *f = sh->curve.FindById(fm.orig),
           *g = sh->curve.FindById(em->nb[o][k]);
    if(em->e.deg != 1 || fm.e.deg != 1) {
        dbp("fillet: miter of curved edges not supported yet");
        return false;
    }
    if(fabs(fm.r - em->r) > LENGTH_EPS || fm.chamfer != em->chamfer) {
        dbp("fillet: edges meeting at a corner must have the same radius");
        return false;
    }
    if(fm.convex != em->convex) {
        dbp("fillet: convex and concave edges at one corner, not supported");
        return false;
    }
    if(!IsSimpleCurve(sh, g)) {
        dbp("fillet: neighbor is not a simple exact curve");
        return false;
    }
    if(OtherSurface(g, em->face[o]).v != OtherSurface(f, em->face[s]).v) {
        dbp("fillet: corner neighbors don't share a face");
        return false;
    }
    int fs = (fm.face[0].v == em->face[s].v) ? 0 : 1;
    if(!IntersectCarriers(em->off[s], fm.off[fs], em->vtx[k], &em->cut[s][k])) {
        dbp("fillet: offset curves don't meet at the corner");
        return false;
    }
    em->kind[k]    = MITER;
    em->modSide[k] = s;
    em->cutsG[k]   = true;

    // Between flat faces at equal angles, the miter is exact: the two
    // fillets meet in the plane that bisects the corner, along an ellipse
    // from the corner in S, through the vertex, to the cut on g. Otherwise
    // TraceMiterEnd() finds it, once every edge is set up.
    bool flat  = em->mid.Start().Equals(em->e.Start()) &&
                 fm.mid.Start().Equals(fm.e.Start());
    bool equal = fabs(em->w - fm.w) < 1e-9;
    if(!(flat && equal)) {
        // At a reflex corner of S the offsets cross past the vertex and the
        // fillets run over each other; tracing toward the vertex would hit
        // the partner's other face where that face doesn't exist. The corner
        // is reflex when f, leaving the vertex, heads out of S as seen from e.
        SSurface *ss = sh->surface.FindById(em->face[s]);
        Vector in = InwardAt(ss, em->e, ss->trim[em->trim[s]].backwards,
                             (k == 0) ? 0.0 : 1.0);
        Vector fAway = fm.e.Start().Equals(em->vtx[k])
                           ? fm.e.Finish().Minus(fm.e.Start())
                           : fm.e.Start().Minus(fm.e.Finish());
        if(fAway.Dot(in) < 0) {
            dbp("fillet: miter of unequal angles at a reflex corner, not supported yet");
            return false;
        }
        em->traced[k] = true;
        return true;
    }
    if(!IntersectCarriers(em->off[o], g->exact, em->vtx[k], &em->cut[o][k])) {
        dbp("fillet: offset curves don't meet at the corner");
        return false;
    }
    if(!CanMoveEnd(g->exact, em->vtx[k], em->cut[o][k])) {
        dbp("fillet: neighbor is too short for this radius");
        return false;
    }
    // The surface has to reach past both ends of the miter curve.
    double ts = ParamOn(em->off[s], em->cut[s][k]),
           to = ParamOn(em->off[o], em->cut[o][k]);
    em->tSrf[k] = (k == 0) ? min(ts, to) : max(ts, to);
    return true;
}

// A traced miter at end k of mods[i] (straight edges). Each point of e's
// cross-section slides along e toward the vertex until it first hits either
// the partner's fillet (solved exactly, see HitsOnSweep()) or the partner's
// other face,
// beyond g. From the shared face S it hits the partner; it may switch to the
// face at some point X, where the miter meets the partner's contact with
// that face. Then this fillet ends in two pieces, the miter and a cut into
// the face; otherwise in one, the miter. Reads the shell only.
static bool TraceMiterEnd(SShell *sh, std::vector<EdgeMod> &mods, int i, int k) {
    EdgeMod *em = &mods[i];
    int s = em->modSide[k], o = 1 - s;
    const EdgeMod &pm = mods[FindEdgeMod(mods, em->nb[s][k])];
    SCurve *g = sh->curve.FindById(em->nb[o][k]);
    em->pieceFace[k] = OtherSurface(g, em->face[o]);

    RayTargets rt = {};
    rt.sweep  = &pm;
    rt.srf    = sh->surface.FindById(em->pieceFace[k]);
    rt.tFrom  = 0.5;
    rt.dir    = (k == 0) ? -1.0 : 1.0;
    SBezier sec = StartSection(em);
    Vector d = em->e.Finish().Minus(em->e.Start());

    // Section parameters at S (where the miter starts) and at the other face.
    double sS = (s == 0) ? 0.0 : 1.0, sO = 1.0 - sS;
    double t;
    Vector p;
    int which;
    double sInS = sS + (sO - sS) * 1e-3;
    if(!FirstHit(rt, sec.PointAt(sInS), d, &t, &p, &which) || which != 0) {
        dbp("fillet: miter doesn't start on the partner fillet");
        return false;
    }
    Vector q;
    bool faceAtO = FirstHit(rt, sec.PointAt(sO), d, &t, &q, &which) && which == 1;
    double sX = sO;
    if(faceAtO) {
        // Bisect for where the ray stops hitting the partner.
        double lo = sInS, hi = sO;
        for(int it = 0; it < 50; it++) {
            double m = (lo + hi) / 2;
            if(!FirstHit(rt, sec.PointAt(m), d, &t, &p, &which)) return false;
            if(which == 0) lo = m; else hi = m;
        }
        sX = (lo + hi) / 2;
        if(fabs(sX - sO) < 1e-6) faceAtO = false;
    }
    // With a face piece, X is where the rays switch from the partner to the
    // face, on the partner's contact line with that face. Without one, it is
    // where our own contact line with the other face hits the partner.
    int ps = (pm.face[0].v == em->face[s].v) ? 0 : 1, po = 1 - ps;
    if(!FirstHit(rt, sec.PointAt(sX), d, &t, &p, &which)) return false;
    if(faceAtO) {
        em->xPt[k] = p.ClosestPointOnLine(pm.off[po].ctrl[0],
                         pm.off[po].ctrl[1].Minus(pm.off[po].ctrl[0]));
    } else {
        em->xPt[k] = p;
    }

    if(faceAtO) {
        // This fillet reaches g, and re-cuts it.
        if(!IntersectCarriers(em->off[o], g->exact, em->vtx[k], &em->cut[o][k]) ||
           !CanMoveEnd(g->exact, em->vtx[k], em->cut[o][k]))
        {
            dbp("fillet: miter can't reach the third edge");
            return false;
        }
    } else {
        em->cut[o][k] = em->xPt[k];
    }

    // Trace the pieces, in order from section parameter 0 to 1.
    double tmin = VERY_POSITIVE, tmax = VERY_NEGATIVE;
    double t0 = ParamOn(em->off[0], em->cut[0][k]),
           t1 = ParamOn(em->off[1], em->cut[1][k]);
    tmin = min(t0, t1);
    tmax = max(t0, t1);
    em->nPiece[k] = faceAtO ? 2 : 1;
    for(int n = 0; n < em->nPiece[k]; n++) {
        double a, b;
        Vector pa, pb;
        bool onFace;
        if(em->nPiece[k] == 1) {
            a = 0.0; b = 1.0;
            pa = em->cut[0][k]; pb = em->cut[1][k];
            onFace = false;
        } else {
            a  = (n == 0) ? 0.0 : sX;
            b  = (n == 0) ? sX  : 1.0;
            pa = (n == 0) ? em->cut[0][k] : em->xPt[k];
            pb = (n == 0) ? em->xPt[k]    : em->cut[1][k];
            // The piece on S's side is the miter.
            onFace = (s == 0) ? (n == 1) : (n == 0);
        }
        RayTargets one = rt;
        if(onFace) {
            one.sweep = NULL;
        } else {
            one.srf = NULL;
        }
        em->pieceOnFace[k][n] = onFace;
        std::vector<Vector> pts;
        pts.push_back(pa);
        if(!TraceEndCurve(one, sec, d, a, pa, b, pb, 0, &pts, &tmin, &tmax)) {
            dbp("fillet: can't trace the miter");
            return false;
        }
        em->piecePts[k][n] = pts;
    }
    em->tSrf[k]  = (k == 0) ? tmin : tmax;
    em->cutsG[k] = faceAtO;
    return true;
}

// After both ends of a traced miter are traced: at most one of the two
// fillets cuts into a face, and they must agree on X. The one without a face
// piece ends where the other's miter does; snap it there.
static bool PairTracedMiter(std::vector<EdgeMod> &mods, int i, int k) {
    EdgeMod *em = &mods[i], *pm = &mods[em->partner[k]];
    int pk = em->partnerEnd[k];
    bool ef = (em->nPiece[k] == 2), pf = (pm->nPiece[pk] == 2);
    if(ef && pf) {
        dbp("fillet: both fillets at a miter cut into a face");
        return false;
    }
    EdgeMod *owner = ef ? em : (pf ? pm : (i < em->partner[k] ? em : pm));
    EdgeMod *other = (owner == em) ? pm : em;
    int ok_ = (owner == em) ? k : pk, otk = (owner == em) ? pk : k;
    if(!owner->xPt[ok_].Equals(other->xPt[otk], 1e-3)) {
        dbp("fillet: the two sides of a traced miter don't agree");
        return false;
    }
    int oo = 1 - other->modSide[otk];
    other->xPt[otk]     = owner->xPt[ok_];
    other->cut[oo][otk] = owner->xPt[ok_];
    if(!(ef || pf)) {
        // Neither cuts into a face: X is on g, and both re-cut g there.
        int ownO = 1 - owner->modSide[ok_];
        owner->cut[ownO][ok_] = owner->xPt[ok_];
    }
    other->cutsG[otk] = !(ef || pf);
    owner->cutsG[ok_] = true;
    return true;
}

// Case 4 at end k: both neighbors f (in A) and g (in B) are modified, three
// fillets at a corner. In each face the offsets of e and its neighbor cross;
// e's fillet ends at the cross-section through those two points, where the
// corner patch (sphere or flat triangle) attaches. Edges may be lines or
// arcs, as long as the offsets cross where IntersectCarriers() can find it.
static bool SetUpCornerEnd(SShell *sh, const std::vector<EdgeMod> &mods,
                           EdgeMod *em, int k)
{
    hSSurface other[2];
    for(int s = 0; s < 2; s++) {
        const EdgeMod &fm = mods[FindEdgeMod(mods, em->nb[s][k])];
        if(fabs(fm.r - em->r) > LENGTH_EPS || fm.chamfer != em->chamfer) {
            dbp("fillet: edges meeting at a corner must have the same radius");
            return false;
        }
        if(fm.convex != em->convex) {
            dbp("fillet: convex and concave edges at one corner, not supported");
            return false;
        }
        int fs = (fm.face[0].v == em->face[s].v) ? 0 : 1;
        if(fm.face[fs].v != em->face[s].v) return false;
        other[s] = fm.face[1 - fs];
        if(!IntersectCarriers(em->off[s], fm.off[fs], em->vtx[k], &em->cut[s][k])) {
            dbp("fillet: offset curves don't meet at the corner");
            return false;
        }
    }
    if(other[0].v != other[1].v) {
        dbp("fillet: corner neighbors don't share a face");
        return false;
    }
    // For a fillet both points are where the corner ball touches e's faces,
    // so they are on one cross-section. A chamfer of a straight edge is
    // planar, so its end line can run across it, as at a miter; the surface
    // must reach both points.
    double t0 = ParamOn(em->off[0], em->cut[0][k]),
           t1 = ParamOn(em->off[1], em->cut[1][k]);
    bool planar = em->chamfer && em->e.deg == 1;
    if(!planar && fabs(t0 - t1) > 1e-6) {
        dbp("fillet: corner cuts not on one cross-section");
        return false;
    }
    em->tSrf[k] = (k == 0) ? min(t0, t1) : max(t0, t1);
    em->kind[k] = CORNER;
    return true;
}

// Work out end k of the edge: find the neighbors, classify the vertex, and
// find the cut points. Reads the shell only.
static bool SetUpEnd(SShell *sh, const std::vector<EdgeMod> &mods,
                     EdgeMod *em, int k)
{
    Vector v = (k == 0) ? em->e.Start() : em->e.Finish();
    em->vtx[k] = v;

    bool modified[2];
    for(int s = 0; s < 2; s++) {
        SSurface *srf = sh->surface.FindById(em->face[s]);
        int i = FindTrimAtVertex(srf, em->orig, v);
        if(i < 0) {
            dbp("fillet: no single neighbor at vertex");
            return false;
        }
        em->nb[s][k] = srf->trim[i].curve;
        modified[s] = (FindEdgeMod(mods, em->nb[s][k]) >= 0);
    }
    if(modified[0] && modified[1]) {
        return SetUpCornerEnd(sh, mods, em, k);
    }
    if(!modified[0] && !modified[1]) {
        return SetUpEndFace(sh, em, k);
    }

    int s = modified[0] ? 0 : 1;
    SCurve *f = sh->curve.FindById(em->nb[s][k]);
    double tf = f->exact.Start().Equals(v) ? 0.0 : 1.0;
    Vector te = em->e.TangentAt((k == 0) ? 0.0 : 1.0).WithMagnitude(1.0),
           tn = f->exact.TangentAt(tf).WithMagnitude(1.0);
    if(fabs(fabs(te.Dot(tn)) - 1.0) < 1e-6) {
        return SetUpTangentEnd(sh, em, k, s);
    } else {
        return SetUpMiterEnd(sh, mods, em, k, s);
    }
}

// For a TANGENT or MITER end, find the neighbor's EdgeMod and which of its
// ends meets ours. They share the end curve, so their cut points must match.
static bool FindPartnerEnd(std::vector<EdgeMod> &mods, int i, int k) {
    EdgeMod *em = &mods[i];
    int j = FindEdgeMod(mods, em->nb[em->modSide[k]][k]);
    if(mods[j].convex != em->convex) {
        dbp("fillet: convex and concave edges meet, not supported");
        return false;
    }
    for(int pk = 0; pk < 2; pk++) {
        const EdgeMod &pm = mods[j];
        if(pm.kind[pk] != em->kind[k] || !pm.vtx[pk].Equals(em->vtx[k])) {
            continue;
        }
        if(em->traced[k] || pm.traced[pk]) {
            // Only the corner in the shared face is known yet.
            Vector a = em->cut[em->modSide[k]][k],
                   b = pm.cut[pm.modSide[pk]][pk];
            if(!a.Equals(b) || em->traced[k] != pm.traced[pk]) return false;
        } else {
            bool same    = pm.cut[0][pk].Equals(em->cut[0][k]) &&
                           pm.cut[1][pk].Equals(em->cut[1][k]),
                 swapped = pm.cut[0][pk].Equals(em->cut[1][k]) &&
                           pm.cut[1][pk].Equals(em->cut[0][k]);
            if(!same && !swapped) return false;
        }
        em->partner[k]    = j;
        em->partnerEnd[k] = pk;
        return true;
    }
    return false;
}

// The part of curve b from p0 to p1 (both on b or its extension), with the
// ends set exactly so trims match the curve's points.
static bool SubCurve(const SBezier &b, Vector p0, Vector p1, SBezier *out) {
    if(!b.Reparameterized(ParamOn(b, p0), ParamOn(b, p1), out)) return false;
    out->ctrl[0] = p0;
    out->ctrl[out->deg] = p1;
    return true;
}

// Fillet or chamfer surface from rows eA, (mid,) eB over [tSrf[0], tSrf[1]],
// facing outward: roughly along nA + nB.
static bool MakeFilletSurface(EdgeMod *em, Vector nA, Vector nB) {
    SBezier e, a, b;
    double t0 = em->tSrf[0], t1 = em->tSrf[1];
    if(!em->mid.Reparameterized(t0, t1, &e) ||
       !em->off[0].Reparameterized(t0, t1, &a) ||
       !em->off[1].Reparameterized(t0, t1, &b))
    {
        return false;
    }

    SSurface s = {};
    s.degm = e.deg;
    s.degn = em->chamfer ? 1 : 2;
    for(int i = 0; i <= s.degm; i++) {
        s.ctrl[i][0]   = a.ctrl[i];
        s.weight[i][0] = a.weight[i];
        if(!em->chamfer) {
            s.ctrl[i][1]   = e.ctrl[i];
            s.weight[i][1] = e.weight[i] * em->w;
        }
        s.ctrl[i][s.degn]   = b.ctrl[i];
        s.weight[i][s.degn] = b.weight[i];
    }
    if(s.NormalAt(0.5, 0.5).Dot(nA.Plus(nB)) < 0) {
        for(int i = 0; i <= s.degm; i++) {
            swap(s.ctrl[i][0], s.ctrl[i][s.degn]);
            swap(s.weight[i][0], s.weight[i][s.degn]);
        }
    }
    s.color = RGBi(100, 100, 100);
    em->fillet = s;
    return true;
}

// End curve at end k, from cut[0][k] to cut[1][k]: a line for a chamfer, a
// circular arc of the ball (or, at a miter, its projection, an ellipse) for a
// fillet. The middle control point is on the middle row; at a miter it is the
// vertex, where the middle row (the edge itself, between flat faces) ends.
static void MakeEndCurve(EdgeMod *em, int k) {
    if(em->chamfer) {
        em->endC[k] = SBezier::From(em->cut[0][k], em->cut[1][k]);
    } else {
        Vector mid;
        if(em->kind[k] == MITER) {
            mid = em->vtx[k];
        } else if(em->kind[k] == END_FACE) {
            mid = em->endMid[k];
        } else {
            mid = em->mid.PointAt(em->tSrf[k]);
        }
        em->endC[k] = SBezier::From(Vector4::From(1.0, em->cut[0][k]),
                                    Vector4::From(em->w, mid),
                                    Vector4::From(1.0, em->cut[1][k]));
    }
}

static int FindOrAddRecut(std::vector<Recut> *rcs, SCurve *sc) {
    for(size_t i = 0; i < rcs->size(); i++) {
        if((*rcs)[i].orig.v == sc->h.v) return (int)i;
    }
    Recut rc = {};
    rc.orig   = sc->h;
    rc.start  = sc->exact.Start();
    rc.finish = sc->exact.Finish();
    rcs->push_back(rc);
    return (int)rcs->size() - 1;
}

// Move the end of neighbor sc that is at vertex v to the cut point.
static bool SetRecutEnd(std::vector<Recut> *rcs, SCurve *sc, Vector v, Vector cut) {
    Recut *rc = &(*rcs)[FindOrAddRecut(rcs, sc)];
    // At a tangent joint or a miter both fillets cut g at the same point.
    if(sc->exact.Start().Equals(v)) {
        if(!rc->start.Equals(v) && !rc->start.Equals(cut)) return false;
        rc->start = cut;
    } else if(sc->exact.Finish().Equals(v)) {
        if(!rc->finish.Equals(v) && !rc->finish.Equals(cut)) return false;
        rc->finish = cut;
    } else {
        return false;
    }
    return true;
}

// A curve kept as points only, not exact, as Booleans make for intersection
// curves.
static hSCurve AddPwlCurve(SShell *sh, const std::vector<Vector> &pts,
                           hSSurface sa, hSSurface sb)
{
    SCurve sc = {};
    sc.isExact = false;
    for(size_t i = 0; i < pts.size(); i++) {
        SCurvePt pt = {};
        pt.p      = pts[i];
        pt.vertex = (i == 0) || (i == pts.size() - 1);
        sc.pts.Add(&pt);
    }
    sc.surfA  = sa;
    sc.surfB  = sb;
    sc.source = SCurve::Source::INTERSECTION;
    return sh->curve.AddAndAssignId(&sc);
}

static hSCurve AddCurve(SShell *sh, const SBezier &b, hSSurface sa, hSSurface sb) {
    SCurve sc = {};
    sc.isExact = true;
    sc.exact   = b;
    b.MakePwlInto(&sc.pts);
    sc.surfA  = sa;
    sc.surfB  = sb;
    sc.source = SCurve::Source::INTERSECTION;
    return sh->curve.AddAndAssignId(&sc);
}

// A trim on curve hc from start to finish, where the curve itself runs from
// curveStart.
static STrimBy MakeTrim(hSCurve hc, Vector start, Vector finish, Vector curveStart) {
    STrimBy stb = {};
    stb.curve     = hc;
    stb.start     = start;
    stb.finish    = finish;
    stb.backwards = !start.Equals(curveStart);
    return stb;
}

// The same edge as seen from the surface on the other side.
static STrimBy ReverseTrim(const STrimBy &t) {
    STrimBy r = t;
    r.start     = t.finish;
    r.finish    = t.start;
    r.backwards = !t.backwards;
    return r;
}

static bool AddToCorner(std::vector<Corner> *corners, Vector v, int i, int k) {
    Corner *c = NULL;
    for(Corner &cc : *corners) {
        if(cc.vtx.Equals(v)) c = &cc;
    }
    if(c == NULL) {
        Corner nc = {};
        nc.vtx = v;
        corners->push_back(nc);
        c = &corners->back();
    }
    if(c->n >= 3) return false;
    c->mod[c->n] = i;
    c->end[c->n] = k;
    c->n++;
    return true;
}

// The corner's three points: the cut points of the three edges, each shared
// by two of them.
static bool CornerPoints(std::vector<EdgeMod> &mods, Corner *c) {
    if(c->n != 3) return false;
    EdgeMod *em0 = &mods[c->mod[0]],
            *em1 = &mods[c->mod[1]];
    int k0 = c->end[0], k1 = c->end[1];
    c->p[0] = em0->cut[0][k0];
    c->p[1] = em0->cut[1][k0];
    if(!em1->cut[0][k1].Equals(c->p[0]) && !em1->cut[0][k1].Equals(c->p[1])) {
        c->p[2] = em1->cut[0][k1];
    } else {
        c->p[2] = em1->cut[1][k1];
    }
    return true;
}

// The flat triangle closing three chamfers at a corner. It faces the same
// way as the chamfers.
static bool MakeChamferCorner(std::vector<EdgeMod> &mods, Corner *c) {
    if(!CornerPoints(mods, c)) return false;
    Vector p0 = c->p[0], p1 = c->p[1], p2 = c->p[2];
    EdgeMod *em0 = &mods[c->mod[0]];
    Vector mid = em0->endC[c->end[0]].PointAt(0.5);
    Vector nChamfer = SurfaceNormalNear(&em0->fillet, mid);
    SSurface srf = SSurface::FromPlane(p0, p1.Minus(p0), p2.Minus(p0));
    if(srf.NormalAt(0.25, 0.25).Dot(nChamfer) < 0) {
        srf = SSurface::FromPlane(p0, p2.Minus(p0), p1.Minus(p0));
    }
    srf.color = RGBi(100, 100, 100);
    c->sphere   = false;
    c->patch[0] = srf;
    for(int m = 0; m < 3; m++) c->side[m] = 0;
    return true;
}

// The sphere patch closing three fillets at a corner. The ball's center is
// on all three spines; the patch faces the same way as the fillets.
static bool MakeSphereCorner(std::vector<EdgeMod> &mods, Corner *c) {
    if(!CornerPoints(mods, c)) return false;
    Vector center = mods[c->mod[0]].spine.PointAt(mods[c->mod[0]].tSrf[c->end[0]]);
    for(int m = 1; m < 3; m++) {
        EdgeMod *em = &mods[c->mod[m]];
        if(!em->spine.PointAt(em->tSrf[c->end[m]]).Equals(center)) {
            dbp("fillet: the three spines don't meet at the corner");
            return false;
        }
    }
    EdgeMod *em0 = &mods[c->mod[0]];
    Vector q  = em0->endC[c->end[0]].PointAt(0.5),
           nf = SurfaceNormalNear(&em0->fillet, q);
    bool outward = nf.Dot(q.Minus(center)) > 0;
    if(!SphereTrianglePatch(center, c->p, outward, c->patch, c->arc, &c->t)) {
        dbp("fillet: can't build the sphere patch");
        return false;
    }
    // Which band each fillet's end curve borders: side i runs p[i] to p[i+1].
    for(int m = 0; m < 3; m++) {
        EdgeMod *em = &mods[c->mod[m]];
        int k = c->end[m];
        c->side[m] = -1;
        for(int i = 0; i < 3; i++) {
            Vector a = c->p[i], b = c->p[(i + 1) % 3];
            if((em->cut[0][k].Equals(a) && em->cut[1][k].Equals(b)) ||
               (em->cut[0][k].Equals(b) && em->cut[1][k].Equals(a)))
            {
                c->side[m] = i;
            }
        }
        if(c->side[m] < 0) return false;
    }
    c->sphere = true;
    return true;
}

static bool IsDeadCurve(const std::vector<EdgeMod> &mods,
                        const std::vector<Recut> &recuts, hSCurve hc)
{
    if(FindEdgeMod(mods, hc) >= 0) return true;
    for(const Recut &rc : recuts) {
        if(rc.orig.v == hc.v) return true;
    }
    return false;
}

void SShell::ModifyEdges()
{
    std::vector<EdgeMod> mods;

    // 1. Collect. Loop over the original curves only.
    int ncurves = curve.n;
    for(int i = 0; i < ncurves; i++) {
        SCurve *sc = &curve[i];
        double m = GetEdgeModifier(*sc);
        if(fabs(m) < LENGTH_EPS) continue;

        EdgeMod em = {};
        em.orig    = sc->h;
        em.r       = fabs(m);
        em.chamfer = (m < 0);
        em.e       = sc->exact;
        em.face[0] = sc->surfA;
        em.face[1] = sc->surfB;
        if(!IsSimpleCurve(this, sc)) {
            dbp("fillet: edge is not a simple exact curve");
            return;
        }
        SSurface *s0 = surface.FindById(em.face[0]),
                 *s1 = surface.FindById(em.face[1]);
        em.trim[0] = FindOnlyTrim(s0, sc->h);
        em.trim[1] = FindOnlyTrim(s1, sc->h);
        if(s0->trim[em.trim[0]].backwards == s1->trim[em.trim[1]].backwards) {
            dbp("fillet: edge's trims don't run in opposite directions");
            return;
        }
        mods.push_back(em);
    }
    if(mods.empty()) return;

    // 2-4. Offset curves, vertices and cut points. Read only. If any edge
    // can't be done, do none: neighbors depend on each other.
    for(EdgeMod &em : mods) {
        if(!SetUpSections(this, &em)) return;
    }
    for(EdgeMod &em : mods) {
        if(!SetUpEnd(this, mods, &em, 0) || !SetUpEnd(this, mods, &em, 1)) return;
    }
    for(size_t i = 0; i < mods.size(); i++) {
        for(int k = 0; k < 2; k++) {
            if(mods[i].kind[k] == END_FACE || mods[i].kind[k] == CORNER) continue;
            if(!FindPartnerEnd(mods, (int)i, k)) {
                dbp("fillet: modified neighbors don't meet");
                return;
            }
        }
    }
    for(size_t i = 0; i < mods.size(); i++) {
        for(int k = 0; k < 2; k++) {
            if(mods[i].kind[k] == MITER && mods[i].traced[k] &&
               !TraceMiterEnd(this, mods, (int)i, k)) return;
        }
    }
    for(size_t i = 0; i < mods.size(); i++) {
        for(int k = 0; k < 2; k++) {
            if(mods[i].kind[k] == MITER && mods[i].traced[k] &&
               mods[i].partner[k] > (int)i && !PairTracedMiter(mods, (int)i, k)) return;
        }
    }

    // 5. Build all the new geometry. If anything fails, leave the shell as
    // it was. Unmodified neighbors are re-cut where the offsets meet them.
    std::vector<Recut> recuts;
    for(EdgeMod &em : mods) {
        for(int k = 0; k < 2; k++) {
            bool ok = true;
            for(int s = 0; s < 2; s++) {
                if(em.kind[k] == CORNER) continue;
                if(em.kind[k] != END_FACE && em.modSide[k] == s) continue;
                if(em.kind[k] == MITER && !em.cutsG[k]) continue;
                ok = ok && SetRecutEnd(&recuts, curve.FindById(em.nb[s][k]),
                                       em.vtx[k], em.cut[s][k]);
            }
            if(!ok) {
                dbp("fillet: neighbor cut twice at one end");
                return;
            }
            if(!em.traced[k]) MakeEndCurve(&em, k);
        }
        Vector mid = em.e.PointAt(0.5);
        Vector n0 = SurfaceNormalNear(surface.FindById(em.face[0]), mid),
               n1 = SurfaceNormalNear(surface.FindById(em.face[1]), mid);
        if(!SubCurve(em.off[0], em.cut[0][0], em.cut[0][1], &em.newOff[0]) ||
           !SubCurve(em.off[1], em.cut[1][0], em.cut[1][1], &em.newOff[1]) ||
           !MakeFilletSurface(&em, n0, n1))
        {
            dbp("fillet: can't build curves or surface");
            return;
        }
    }
    for(Recut &rc : recuts) {
        if(!SubCurve(curve.FindById(rc.orig)->exact, rc.start, rc.finish, &rc.b)) {
            dbp("fillet: can't re-cut neighbor");
            return;
        }
    }
    std::vector<Corner> corners;
    for(size_t i = 0; i < mods.size(); i++) {
        for(int k = 0; k < 2; k++) {
            if(mods[i].kind[k] != CORNER) continue;
            if(!AddToCorner(&corners, mods[i].vtx[k], (int)i, k)) {
                dbp("fillet: more than three edges at a corner");
                return;
            }
        }
    }
    for(Corner &c : corners) {
        bool ok = mods[c.mod[0]].chamfer ? MakeChamferCorner(mods, &c)
                                         : MakeSphereCorner(mods, &c);
        if(!ok) {
            dbp("fillet: can't build corner");
            return;
        }
    }

    // 6. Change the shell. Add surfaces and curves first, then rewrite trims,
    // looking things up by handle each time since the lists can reallocate.
    for(EdgeMod &em : mods) {
        em.hFillet = surface.AddAndAssignId(&em.fillet);
    }
    for(Corner &c : corners) {
        for(int i = 0; i < (c.sphere ? 3 : 1); i++) {
            c.h[i] = surface.AddAndAssignId(&c.patch[i]);
        }
        for(int m = 0; m < 3; m++) {
            mods[c.mod[m]].sC[c.end[m]] = c.h[c.side[m]];
        }
    }
    for(Recut &rc : recuts) {
        SCurve *old = curve.FindById(rc.orig);
        hSSurface sa = old->surfA, sb = old->surfB;
        rc.newh = AddCurve(this, rc.b, sa, sb);
    }
    for(EdgeMod &em : mods) {
        for(int s = 0; s < 2; s++) {
            em.hNew[s] = AddCurve(this, em.newOff[s], em.face[s], em.hFillet);
        }
    }
    // End curves. Where two fillets meet they share one, made by the
    // EdgeMod that comes first.
    for(size_t i = 0; i < mods.size(); i++) {
        EdgeMod &em = mods[i];
        for(int k = 0; k < 2; k++) {
            if(em.traced[k]) {
                // The fillet with the face piece (or the first, if neither
                // has one) makes the shared miter, and its face piece.
                EdgeMod &pm = mods[em.partner[k]];
                int pk = em.partnerEnd[k];
                bool owner = em.nPiece[k] == 2 ||
                             (pm.nPiece[pk] == 1 && em.partner[k] > (int)i);
                if(!owner) continue;
                for(int n = 0; n < em.nPiece[k]; n++) {
                    if(em.pieceOnFace[k][n]) {
                        em.hPiece[k][n] = AddPwlCurve(this, em.piecePts[k][n],
                                                      em.pieceFace[k], em.hFillet);
                    } else {
                        em.hPiece[k][n] = AddPwlCurve(this, em.piecePts[k][n],
                                                      em.hFillet, pm.hFillet);
                        pm.hPiece[pk][0] = em.hPiece[k][n];
                    }
                }
            } else if(!em.endPts[k].empty()) {
                em.hEndC[k] = AddPwlCurve(this, em.endPts[k], em.sC[k], em.hFillet);
            } else if(em.kind[k] == END_FACE || em.kind[k] == CORNER) {
                em.hEndC[k] = AddCurve(this, em.endC[k], em.sC[k], em.hFillet);
            } else if(em.partner[k] > (int)i) {
                EdgeMod &pm = mods[em.partner[k]];
                em.hEndC[k] = AddCurve(this, em.endC[k], em.hFillet, pm.hFillet);
                pm.hEndC[em.partnerEnd[k]] = em.hEndC[k];
            }
        }
    }

    for(EdgeMod &em : mods) {
        // A and B: the trim on e moves to the offset curve, same direction.
        // The fillet gets the reverse.
        STrimBy t[2];
        for(int s = 0; s < 2; s++) {
            SSurface *srf = surface.FindById(em.face[s]);
            bool bk = srf->trim[em.trim[s]].backwards;
            Vector p0 = bk ? em.cut[s][1] : em.cut[s][0],
                   p1 = bk ? em.cut[s][0] : em.cut[s][1];
            t[s] = MakeTrim(em.hNew[s], p0, p1, em.cut[s][0]);
            srf->trim[em.trim[s]] = t[s];
            STrimBy rt = ReverseTrim(t[s]);
            surface.FindById(em.hFillet)->trim.Add(&rt);
        }

        // End curves. The fillet's trims form a loop: the reversed trim on
        // side 0 finishes at t[0].start, at one end, so the end trim there
        // runs from cut[0] to cut[1], and the other one from cut[1] to
        // cut[0]. An end face or corner patch gets the reverse; a partner
        // makes its own.
        for(int k = 0; k < 2; k++) {
            bool from0 = t[0].start.Equals(em.cut[0][k]);
            if(em.traced[k]) {
                // The pieces run from cut[0] to cut[1]; walk them in the
                // loop's direction.
                Vector ends[3];
                int np = em.nPiece[k];
                ends[0]  = em.cut[0][k];
                ends[np] = em.cut[1][k];
                if(np == 2) ends[1] = em.xPt[k];
                for(int m = 0; m < np; m++) {
                    int n = from0 ? m : (np - 1 - m);
                    Vector a = from0 ? ends[n] : ends[n + 1],
                           b = from0 ? ends[n + 1] : ends[n];
                    SCurve *pc = curve.FindById(em.hPiece[k][n]);
                    STrimBy tp = MakeTrim(em.hPiece[k][n], a, b, pc->pts[0].p);
                    surface.FindById(em.hFillet)->trim.Add(&tp);
                    if(em.pieceOnFace[k][n]) {
                        STrimBy tc = ReverseTrim(tp);
                        surface.FindById(em.pieceFace[k])->trim.Add(&tc);
                    }
                }
                continue;
            }
            Vector c0 = from0 ? em.cut[0][k] : em.cut[1][k],
                   c1 = from0 ? em.cut[1][k] : em.cut[0][k];
            SCurve *ec = curve.FindById(em.hEndC[k]);
            STrimBy tf = MakeTrim(em.hEndC[k], c0, c1, ec->pts[0].p);
            surface.FindById(em.hFillet)->trim.Add(&tf);
            if(em.kind[k] == END_FACE || em.kind[k] == CORNER) {
                STrimBy tc = ReverseTrim(tf);
                surface.FindById(em.sC[k])->trim.Add(&tc);
            }
        }
    }

    // Sphere corners: each band now has its side's trim, the reverse of the
    // fillet's. Add the arcs to the incenter, each shared by two bands, and
    // close each band's loop in the direction its side trim runs.
    for(Corner &c : corners) {
        if(!c.sphere) continue;
        for(int i = 0; i < 3; i++) {
            c.hArc[i] = AddCurve(this, c.arc[i], c.h[(i + 2) % 3], c.h[i]);
        }
        for(int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            SSurface *band = surface.FindById(c.h[i]);
            bool forward = band->trim[0].start.Equals(c.p[i]);
            STrimBy t0, t1;
            if(forward) {
                // p[i] -> p[j] -> t -> p[i]
                t0 = MakeTrim(c.hArc[j], c.p[j], c.t, c.p[j]);
                t1 = MakeTrim(c.hArc[i], c.t, c.p[i], c.p[i]);
            } else {
                // p[j] -> p[i] -> t -> p[j]
                t0 = MakeTrim(c.hArc[i], c.p[i], c.t, c.p[i]);
                t1 = MakeTrim(c.hArc[j], c.t, c.p[j], c.p[j]);
            }
            band->trim.Add(&t0);
            band->trim.Add(&t1);
        }
    }

    // Neighbors: point their trims at the re-cut curves.
    for(Recut &rc : recuts) {
        for(SSurface &s : surface) {
            for(STrimBy &stb : s.trim) {
                if(stb.curve.v != rc.orig.v) continue;
                stb.curve  = rc.newh;
                stb.start  = stb.backwards ? rc.finish : rc.start;
                stb.finish = stb.backwards ? rc.start  : rc.finish;
            }
        }
    }

    // Remove the original edges and the old neighbors. SCurve has no tag, so
    // copy the live curves to a new list, which takes over their points.
    IdList<SCurve,hSCurve> kept = {};
    for(SCurve &sc : curve) {
        if(IsDeadCurve(mods, recuts, sc.h)) continue;
        kept.Add(&sc);
        sc.pts = {};
    }
    curve.Clear();
    kept.MoveSelfInto(&curve);
}

} // namespace SolveSpace
