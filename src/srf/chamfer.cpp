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
// - Edges are pieces: a range of an exact curve with a trim on each of its
//   two faces (Booleans split curves, and may give each face its own
//   coincident copy). Curves that are only points (not exact) are not
//   handled, and won't be.
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
//    With unequal angles the miter runs from the corner in A until it
//    reaches one fillet's contact with its other face, at X. Past X the
//    other fillet cuts into that face, down to g. So one fillet ends in two
//    pieces (miter, face cut), the other in one (miter), and only the first
//    re-cuts g. With flat faces both pieces are exact conics, at convex and
//    reflex corners alike, see ExactMiterEnd(). With curved faces they are
//    traced, see TraceMiterEnd() (convex corners only).
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
// STATUS (2026-11-05)
// -------------------
// Working, fillet and chamfer, checked for closed meshes and against exact
// or semi-analytic volumes:
// - Edges between planes at any angle, convex and concave; lines and arcs;
//   plane-cylinder and cylinder-cylinder edges; arcs on cones (revolved
//   parts) (the rolling ball's cross-section: SolveBallSection(),
//   FollowEdge()).
// - Flat quadratic and cubic edges (TrueType text, Bezier profiles) on
//   straight extrusions: rows offset in the edge's plane by the Tiller and
//   Hanson construction (OffsetInPlane()), exact at the curve's ends.
//   Near-tangent joins (rounded font coordinates) are snapped together.
// - Case 1 end faces: perpendicular, flat at an angle (exact ellipse), and
//   curved (traced end curve, kept as points). Neighbors are shortened, or
//   extended past a reflex corner.
// - Case 2 tangent chains (e.g. the arcs of a circle).
// - Case 3 miters: straight edges between flat faces are exact conics,
//   convex and reflex corners, equal and unequal angles (ExactMiterEnd()).
//   Otherwise (arc edges, curved faces) traced at convex corners
//   (TraceMiterEnd()): each cross-section point is carried along its edge,
//   a line or around the arc's axis, to the first crossing of the partner
//   fillet or the partner's other face. At reflex corners only when both
//   cross-sections are equal (straight extrusions such as text): the miter
//   then runs straight from the corner to the third edge.
// - Case 4 corners of three edges, any angles: flat triangle for chamfers,
//   sphere patch of three bands for fillets (sphere_patch.cpp).
// - Edges split by Booleans, worked on as pieces (GetPiece()), including
//   coincident curve copies on the two faces.
// - Only the last group is modified (groupmesh.cpp), for testing.
//
// Not handled, by decision: point-only (non-exact) curves; differing radii
// at a vertex; convex and concave edges meeting at a vertex.
//
// Next steps:
// - An arc edge meeting its end face at an angle (case 1): trace along
//   circles, as the miters now do.
// - Meshing: a chamfer cone (arc edge) trimmed by traced miters at both
//   ends gets some inverted sliver triangles along the miters (arc_test
//   variant 3, chamfer: volume about 0.4 low, and open edges at chord
//   tolerance 0.0002). The surface and trims check out (every trim point
//   maps onto the surface; the (u, v) outline is sound), so it looks like
//   the triangulator.
// - Arcs crossing in different planes (IntersectCarriers()).
// - An edge between a flat face and the side wall of an extruded Bezier
//   (bezier_test variant 2): in the cross-section the wall is the Bezier,
//   neither a line nor a circle.
// - Approximated fillet surfaces (pipes around curved spines): straight
//   edges along cones on the end caps of revolved parts, edges on skewed
//   extrusions of curves.
// - Miters at reflex corners with curved faces and unequal cross-sections
//   (refused now).
// - Text: very sharp corners (in "B8%") where the offset curves never meet,
//   even extended; the fillet would need a straight extension past the
//   edge's end. "Sag" (chamfer) and "SolveSpace" leave 4 open mesh edges.
// - Accuracy of polynomial edges: the Tiller and Hanson offset is off by up
//   to 0.4 on bezier_test (r = 3, curvature radius 7), so the fillet takes
//   8-9% too much volume. Split the edge before offsetting.
// - Naming edges, so a user can pick them, replacing the GetEdgeModifier()
//   test hook; and making fillet/chamfer its own group type.
//
// Testing
// -------
// Test models are in misc_CAD/fillet_test. GetEdgeModifier() recognizes
// each by its shape and picks its edges; constants there choose variants
// and r (negative for chamfers). It does nothing unless the environment
// variable SOLVESPACE_FILLET_TEST is set. Without the GUI:
//   SOLVESPACE_FILLET_TEST=1 solvespace-cli export-mesh --chord-tol 0.005 \
//       --output %.obj <model>
// then check the OBJ: every triangle edge shared by exactly two triangles,
// and the volume. Curved walls need a fine tolerance (down to 0.0002) for
// the volume to converge. Check also for "trim was unterminated" / "trim was
// empty" and "fillet:" messages.
//
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

// Vertex cases, see the plan above.
static const int END_FACE = 1;  // case 1: neighbors not modified, end face C
static const int TANGENT  = 2;  // case 2: tangent chain of modified edges
static const int MITER    = 3;  // case 3: two modified edges meet at a corner
static const int CORNER   = 4;  // case 4: three modified edges at a corner

// An edge piece: the range of a curve between two vertices, covered by one
// trim on each of the curve's two faces. Booleans split curves, so one curve
// can have several pieces (and run on past them); an unsplit curve is one
// piece. Booleans can also give the two faces separate, coincident copies of
// the curve, so each trim has its own.
struct Piece {
    hSCurve   hc[2];          // the curve each trim is on
    hSSurface face[2];        // the curve's surfA and surfB
    int       trim[2];        // index of the piece's trim in each
    Vector    start, finish;  // ends, in the curve's direction
    SBezier   b;              // the piece's exact geometry
};

// One edge to modify. Stored by value: no pointers into the shell's lists,
// which can reallocate when things are added.
// Index s is the side: [0] is face A (the curve's surfA), [1] is face B.
// Index k is the end:  [0] at e.Start(), [1] at e.Finish().
struct EdgeMod {
    hSCurve   orig;           // the curve the edge is (a piece of)
    hSCurve   origCurve[2];   // [s] the curve its trim on face[s] uses
    double    r;
    bool      chamfer;
    // The rolling ball, from SetUpSections(). Every cross-section of the
    // fillet is an arc of the ball from off[0] to off[1], with its middle
    // control point on mid and that point's weight w.
    bool      convex;         // ball in the material (rounding), not the air
    SBezier   spine;          // the ball's center
    SBezier   mid;            // where the tangents at the contacts cross
    double    w;              // cos(half the arc angle); cos(45) when square
    SBezier   e;              // the original edge (the piece)
    hSSurface face[2];        // [s]
    int       trim[2];        // [s] index of the edge's trim in face[s]
    SBezier   off[2];         // [s] offset curves eA, eB: where the ball
                              //     touches face[s]
    Vector    vtx[2];         // [k] original vertex
    Piece     nb[2][2];       // [s][k] neighbor piece in face[s] at vtx[k]
    int       nbTrim[2][2];   // [s][k] its trim's index in face[s]
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
    bool      pieceExact[2];  // [k] the pieces are exact (pieceCurve), not
                              //     traced (piecePts)
    SBezier   pieceCurve[2][2]; // [k][piece]
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

// A neighbor piece that gets shortened (or extended) at one or both ends.
struct Recut {
    Piece     pc;
    Vector    start, finish;  // new ends, in the curve's direction
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

static bool SubCurve(const SBezier &b, Vector p0, Vector p1, SBezier *out);

// The piece that trim ti of surface hs covers. On the curve's other face,
// the matching trim runs between the same two points the other way, on the
// same curve or a coincident copy of it. False if a curve isn't exact, or
// there isn't exactly one such trim.
static bool GetPiece(SShell *sh, hSSurface hs, int ti, Piece *pc) {
    STrimBy *stb = &sh->surface.FindById(hs)->trim[ti];
    SCurve *sc = sh->curve.FindById(stb->curve);
    if(!sc->isExact) return false;
    pc->face[0] = sc->surfA;
    pc->face[1] = sc->surfB;
    pc->start   = stb->backwards ? stb->finish : stb->start;
    pc->finish  = stb->backwards ? stb->start  : stb->finish;
    if(pc->start.Equals(sc->exact.Start()) && pc->finish.Equals(sc->exact.Finish())) {
        pc->b = sc->exact;
    } else if(!SubCurve(sc->exact, pc->start, pc->finish, &pc->b)) {
        return false;
    }
    Vector mid = pc->b.PointAt(0.5);
    for(int i = 0; i < 2; i++) {
        if(pc->face[i].v == hs.v) {
            pc->trim[i] = ti;
            pc->hc[i]   = sc->h;
            continue;
        }
        SSurface *f = sh->surface.FindByIdNoOops(pc->face[i]);
        if(f == NULL) return false;
        pc->trim[i] = -1;
        for(int j = 0; j < f->trim.n; j++) {
            STrimBy *t = &f->trim[j];
            if(!t->start.Equals(stb->finish) || !t->finish.Equals(stb->start)) continue;
            // The copy names this face, but its other surface may be a
            // coincident one from the other Boolean operand.
            SCurve *c2 = sh->curve.FindById(t->curve);
            bool onFace = (c2->surfA.v == f->h.v || c2->surfB.v == f->h.v);
            if(!c2->isExact || !onFace) continue;
            double tm;
            c2->exact.ClosestPointTo(mid, &tm, /*mustConverge=*/false);
            if(!c2->exact.PointAt(tm).Equals(mid)) continue;
            if(pc->trim[i] >= 0) return false;
            pc->trim[i] = j;
            pc->hc[i]   = c2->h;
        }
        if(pc->trim[i] < 0) return false;
    }
    return true;
}

static hSSurface OtherFace(const Piece &pc, hSSurface hs) {
    return (pc.face[0].v == hs.v) ? pc.face[1] : pc.face[0];
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
    // IsCircle() looks only at the control points. A circular arc's middle
    // weight (relative to its equal end weights) is the cosine of the angle
    // between the first leg and the chord.
    if(fabs(b.weight[0] - b.weight[2]) > 1e-9) return false;
    Vector leg   = (b.ctrl[1].Minus(b.ctrl[0])).WithMagnitude(1.0),
           chord = (b.ctrl[2].Minus(b.ctrl[0])).WithMagnitude(1.0);
    if(fabs(b.weight[1] / b.weight[0] - leg.Dot(chord)) > 1e-6) return false;
    return b.IsCircle(*n, center, rad);
}

// True if b is a polynomial (not rational) quadratic or cubic, flat, with
// the unit normal of its plane in n.
static bool FlatPolyCurve(const SBezier &b, Vector *n) {
    if(b.deg < 2) return false;
    for(int i = 1; i <= b.deg; i++) {
        if(fabs(b.weight[i] - b.weight[0]) > 1e-9) return false;
    }
    Vector best = Vector::From(0, 0, 0);
    for(int i = 1; i <= b.deg; i++) {
        for(int j = i + 1; j <= b.deg; j++) {
            Vector c = (b.ctrl[i].Minus(b.ctrl[0])).Cross(b.ctrl[j].Minus(b.ctrl[0]));
            if(c.Magnitude() > best.Magnitude()) best = c;
        }
    }
    if(best.Magnitude() < LENGTH_EPS) return false;
    *n = best.WithMagnitude(1.0);
    for(int i = 1; i <= b.deg; i++) {
        if(fabs((b.ctrl[i].Minus(b.ctrl[0])).Dot(*n)) > LENGTH_EPS) return false;
    }
    return true;
}

// Flat polynomial curve b offset in its plane (normal n) by distance a,
// toward n x tangent: each leg of the control polygon moves by a, square to
// itself; the end points move with their legs, and the inner control points
// go where neighboring moved legs cross (Tiller and Hanson). Exact at both
// ends, in position and direction; approximate between. False if a leg is
// degenerate or the offset folds back (a larger than the curvature allows).
static bool OffsetInPlane(const SBezier &b, Vector n, double a, SBezier *out) {
    *out = b;
    if(fabs(a) < LENGTH_EPS) return true;
    Vector ln[3], dir[3];
    for(int i = 0; i < b.deg; i++) {
        dir[i] = b.ctrl[i + 1].Minus(b.ctrl[i]);
        if(dir[i].Magnitude() < LENGTH_EPS) return false;
        ln[i] = n.Cross(dir[i]).WithMagnitude(1.0);
    }
    out->ctrl[0]     = b.ctrl[0].Plus(ln[0].ScaledBy(a));
    out->ctrl[b.deg] = b.ctrl[b.deg].Plus(ln[b.deg - 1].ScaledBy(a));
    for(int j = 1; j < b.deg; j++) {
        Vector p0 = b.ctrl[j - 1].Plus(ln[j - 1].ScaledBy(a)),
               p1 = b.ctrl[j].Plus(ln[j].ScaledBy(a));
        if((dir[j - 1].Cross(dir[j])).Magnitude() < 1e-9 * dir[j].MagSquared()) {
            out->ctrl[j] = p1;
        } else {
            out->ctrl[j] = Vector::AtIntersectionOfLines(p0, p0.Plus(dir[j - 1]),
                                                         p1, p1.Plus(dir[j]),
                                                         NULL, NULL, NULL);
        }
    }
    for(int i = 0; i < b.deg; i++) {
        if((out->ctrl[i + 1].Minus(out->ctrl[i])).Dot(dir[i]) <= 0) return false;
    }
    return true;
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

// Where two circles in one plane (normal n) meet; of the two points, the one
// nearest to near.
static bool CircleMeetsCircle(Vector c1, double r1, Vector c2, double r2,
                              Vector n, Vector near, Vector *p)
{
    Vector dc = c2.Minus(c1);
    double d = dc.Magnitude();
    if(d < LENGTH_EPS) return false;
    if(d > r1 + r2 + LENGTH_EPS || d < fabs(r1 - r2) - LENGTH_EPS) return false;
    double a = (r1*r1 - r2*r2 + d*d) / (2*d),
           h = sqrt(max(0.0, r1*r1 - a*a));
    Vector u  = dc.ScaledBy(1.0 / d),
           v  = n.Cross(u).WithMagnitude(1.0),
           m  = c1.Plus(u.ScaledBy(a)),
           p1 = m.Plus(v.ScaledBy(h)),
           p2 = m.Minus(v.ScaledBy(h));
    *p = ((p1.Minus(near)).Magnitude() < (p2.Minus(near)).Magnitude()) ? p1 : p2;
    return true;
}

// Where the line through l0 and l1 meets flat curve c (plane normal n),
// extended past its ends if needed. A line crossing the plane meets
// it at one point; a line in the plane is solved along the curve, and of
// several points the one nearest to near is taken.
static bool LineMeetsFlatCurve(Vector l0, Vector l1, const SBezier &c, Vector n,
                               Vector near, Vector *p)
{
    Vector dir = l1.Minus(l0);
    double dn = dir.Dot(n);
    if(fabs(dn) > LENGTH_EPS * dir.Magnitude()) {
        double t = (c.Start().Minus(l0)).Dot(n) / dn;
        *p = l0.Plus(dir.ScaledBy(t));
        double tc;
        c.ClosestPointTo(*p, &tc, /*mustConverge=*/false);
        return (c.PointAt(tc).Minus(*p)).Magnitude() < LENGTH_EPS;
    }
    if(fabs((c.Start().Minus(l0)).Dot(n)) > LENGTH_EPS) return false;
    // In the plane: where the curve crosses the line.
    Vector ln = n.Cross(dir).WithMagnitude(1.0);
    const int N = 800;
    double t0 = -1.5, t1 = 2.5, prevT = t0;
    double prevG = (c.PointAt(t0).Minus(l0)).Dot(ln);
    bool found = false;
    for(int i = 1; i <= N; i++) {
        double t = t0 + (t1 - t0) * i / N,
               g = (c.PointAt(t).Minus(l0)).Dot(ln);
        if((g > 0) != (prevG > 0)) {
            double lo = prevT, hi = t, glo = prevG;
            for(int it = 0; it < 60; it++) {
                double m = (lo + hi) / 2, gm = (c.PointAt(m).Minus(l0)).Dot(ln);
                if((gm > 0) == (glo > 0)) { lo = m; glo = gm; } else { hi = m; }
            }
            Vector q = c.PointAt((lo + hi) / 2);
            if(!found || (q.Minus(near)).Magnitude() < (p->Minus(near)).Magnitude()) {
                *p = q;
                found = true;
            }
        }
        prevT = t;
        prevG = g;
    }
    return found;
}

// Where two curves in one plane (normal n) meet, extended past their ends
// if needed: along a, the side of b it is on changes. Of several
// points, the one nearest to near.
static bool FlatCurvesMeet(const SBezier &a, const SBezier &b, Vector n,
                           Vector near, Vector *p)
{
    // Far past the ends too: at a sharp reflex corner the offsets cross well
    // behind the vertex.
    const int N = 1800;
    double t0 = -4.0, t1 = 5.0;
    // Which side of b the point a(t) is on, and how far from b it is.
    double prevT = t0, prevG = 0, dist;
    bool found = false;
    for(int i = 0; i <= N; i++) {
        double t = t0 + (t1 - t0) * i / N, tb;
        Vector pa = a.PointAt(t);
        b.ClosestPointTo(pa, &tb, /*mustConverge=*/false);
        Vector q = b.PointAt(tb);
        double g = (pa.Minus(q)).Dot(n.Cross(b.TangentAt(tb)));
        if(i > 0 && (g > 0) != (prevG > 0)) {
            double lo = prevT, hi = t, glo = prevG;
            for(int it = 0; it < 60; it++) {
                double m = (lo + hi) / 2, tm;
                Vector pm = a.PointAt(m);
                b.ClosestPointTo(pm, &tm, /*mustConverge=*/false);
                double gm = (pm.Minus(b.PointAt(tm))).Dot(n.Cross(b.TangentAt(tm)));
                if((gm > 0) == (glo > 0)) { lo = m; glo = gm; } else { hi = m; }
            }
            Vector pr = a.PointAt((lo + hi) / 2);
            double tr;
            b.ClosestPointTo(pr, &tr, /*mustConverge=*/false);
            dist = (b.PointAt(tr).Minus(pr)).Magnitude();
            // A real crossing, not a jump of the closest point.
            if(dist < LENGTH_EPS &&
               (!found || (pr.Minus(near)).Magnitude() < (p->Minus(near)).Magnitude()))
            {
                *p = pr;
                found = true;
            }
        }
        prevT = t;
        prevG = g;
    }
    return found;
}

// Where two curves, extended if needed, meet; of several points, the one
// nearest to near (the vertex). Lines, a line with an arc, and two arcs in
// one plane.
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
    if(a.deg != 1 && b.deg != 1) {
        Vector ca, na, cb, nb;
        double ra, rb;
        bool arcA = ArcCircle(a, &ca, &ra, &na),
             arcB = ArcCircle(b, &cb, &rb, &nb);
        if(!arcA && !FlatPolyCurve(a, &na)) return false;
        if(!arcB && !FlatPolyCurve(b, &nb)) return false;
        if((na.Cross(nb)).Magnitude() > 1e-6 ||
           fabs((b.Start().Minus(a.Start())).Dot(na)) > LENGTH_EPS)
        {
            dbp("fillet: curves not in one plane, not supported yet");
            return false;
        }
        if(arcA && arcB) return CircleMeetsCircle(ca, ra, cb, rb, na, near, p);
        return FlatCurvesMeet(a, b, na, near, p);
    }
    const SBezier *line = (a.deg == 1) ? &a : &b,
                  *arc  = (a.deg == 1) ? &b : &a;
    Vector center, n;
    double rad;
    if(line->deg == 1 && FlatPolyCurve(*arc, &n)) {
        return LineMeetsFlatCurve(line->ctrl[0], line->ctrl[1], *arc, n, near, p);
    }
    if(!ArcCircle(*arc, &center, &rad, &n)) return false;
    return LineMeetsCircle(line->ctrl[0], line->ctrl[1], center, rad, n, near, p);
}

// How a face looks in the cross-section plane through an edge point (the
// plane perpendicular to the edge). Two kinds so far:
//   line:   a straight line of the face runs away from the edge, with the
//           same normal along it: a plane, a cylinder cut across its axis
//           (an arc edge on a cylinder wall), or a cone through its axis (an
//           arc edge on a revolved part)
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
    // A face with a straight line running away from the edge, with the same
    // normal all along it (a cone, for an arc edge on a revolved part): in
    // the cross-section that is a line. A surface of degree 1 in u or v is
    // straight in that direction.
    Point2d uv;
    s->ClosestPointTo(fs->p, &uv, /*mustConverge=*/false);
    for(int i = 0; i < 2; i++) {
        if((i == 0) ? (s->degm != 1) : (s->degn != 1)) continue;
        Vector a = (i == 0) ? s->PointAt(0, uv.y) : s->PointAt(uv.x, 0),
               b = (i == 0) ? s->PointAt(1, uv.y) : s->PointAt(uv.x, 1);
        Vector ruling = (b.Minus(a)).WithMagnitude(1.0);
        if(fabs(fabs(ruling.Dot(fs->dir)) - 1.0) > 1e-6) continue;
        // Normals inside the line, since a cone's apex has none.
        Vector na = (i == 0) ? s->NormalAt(0.25, uv.y) : s->NormalAt(uv.x, 0.25),
               nb = (i == 0) ? s->NormalAt(0.75, uv.y) : s->NormalAt(uv.x, 0.75);
        if(!na.WithMagnitude(1.0).Equals(nb.WithMagnitude(1.0), 1e-6)) continue;
        fs->isCircle = false;
        return true;
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
    // Convex edge: the ball is in the material, inside both faces. Concave:
    // it is in the air, outside both.
    bool convex = fs[0].dir.Dot(fs[1].n) < 0;
    bs->convex = convex;
    double sg = convex ? -1.0 : 1.0;
    Vector p = fs[0].p,
           tan = em->e.TangentAt(t).WithMagnitude(1.0);
    double r = em->r;

    // Each face offset by r toward the ball; the center is where they meet.
    if(fs[0].isCircle && fs[1].isCircle) {
        double or0 = ((p.Plus(fs[0].n.ScaledBy(sg*r))).Minus(fs[0].center)).Magnitude(),
               or1 = ((p.Plus(fs[1].n.ScaledBy(sg*r))).Minus(fs[1].center)).Magnitude();
        if(!CircleMeetsCircle(fs[0].center, or0, fs[1].center, or1, tan, p, &bs->c)) {
            dbp("fillet: radius too big for this edge");
            return false;
        }
    } else if(!fs[0].isCircle && !fs[1].isCircle) {
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
// along e the way every section point is: a translate of e for a line, a
// concentric arc about the same axis for an arc, or for a flat polynomial
// curve the same offset in its plane (approximated, OffsetInPlane()) and
// along its plane's normal.
static bool FollowEdge(const SBezier &e, Vector q, SBezier *out) {
    if(e.deg == 1) {
        *out = e.TransformedBy(q.Minus(e.Start()), Quaternion::IDENTITY, 1.0);
        return true;
    }
    Vector center, n;
    double rad;
    if(FlatPolyCurve(e, &n)) {
        Vector in = n.Cross(e.TangentAt(0.0)).WithMagnitude(1.0),
               dq = q.Minus(e.Start());
        if(!OffsetInPlane(e, n, dq.Dot(in), out)) return false;
        *out = out->TransformedBy(n.ScaledBy(dq.Dot(n)), Quaternion::IDENTITY, 1.0);
        return true;
    }
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

// Index of the one trim in s, other than trim exclude, that starts or
// finishes at v. -1 if there are none or more than one.
static int FindTrimAtVertex(SSurface *s, int exclude, Vector v) {
    int found = -1;
    for(int i = 0; i < s->trim.n; i++) {
        STrimBy *stb = &s->trim[i];
        if(i == exclude) continue;
        if(stb->start.Equals(v) || stb->finish.Equals(v)) {
            if(found >= 0) return -1;
            found = i;
        }
    }
    return found;
}

// The EdgeMod whose piece has trim ti on surface hs, or -1.
static int FindEdgeModAt(const std::vector<EdgeMod> &mods, hSSurface hs, int ti) {
    for(size_t i = 0; i < mods.size(); i++) {
        for(int s = 0; s < 2; s++) {
            if(mods[i].face[s].v == hs.v && mods[i].trim[s] == ti) return (int)i;
        }
    }
    return -1;
}

// The EdgeMod of the neighbor in face s at end k of em, or -1.
static int NeighborMod(const std::vector<EdgeMod> &mods, const EdgeMod *em, int s, int k) {
    return FindEdgeModAt(mods, em->face[s], em->nbTrim[s][k]);
}

static bool MakeFilletSurface(EdgeMod *em, Vector nA, Vector nB);

// em's fillet surface, built well past its end at vertex v: by 10 radii or a
// quarter of a straight edge, whichever is more, or by 0.3 of an arc's
// parameter.
static bool LongFillet(SShell *sh, const EdgeMod &em, Vector v, SSurface *out) {
    EdgeMod c = em;
    int kv = c.e.Start().Equals(v) ? 0 : 1;
    double half;
    if(c.e.deg == 1) {
        double len = (c.e.Finish().Minus(c.e.Start())).Magnitude();
        half = max(0.25, 10 * c.r / len);
    } else {
        half = 0.3;
    }
    c.tSrf[0] = kv - half;
    c.tSrf[1] = kv + half;
    Vector mid = c.e.PointAt(0.5);
    if(!MakeFilletSurface(&c, SurfaceNormalNear(sh->surface.FindById(c.face[0]), mid),
                              SurfaceNormalNear(sh->surface.FindById(c.face[1]), mid)))
    {
        return false;
    }
    *out = c.fillet;
    return true;
}

// The curve where surfaces srf and other meet, from a to b, as points: the
// march of the Boolean code. toward is the way the curve leaves a; tangent
// says the surfaces are tangent at a.
static bool MarchPiece(SSurface *srf, SSurface *other, Vector a, Vector b,
                       Vector toward, bool tangent, std::vector<Vector> *pts)
{
    List<MarchPoint> ends = {};
    MarchPoint mp = { 0, b, toward, /*tangent=*/false };
    ends.Add(&mp);
    // From a point of tangency the first step goes along startv. Otherwise
    // the march takes the way along the curve with a negative dot product
    // with startv (in Booleans, a trim edge's normal), so pass the reverse.
    Vector startv = tangent ? toward : toward.ScaledBy(-1.0);
    List<SCurvePt> cp = {};
    bool ok = srf->MarchIntersection(other, a, startv, tangent, &ends, &cp);
    if(ok) {
        for(SCurvePt &q : cp) pts->push_back(q.p);
    }
    cp.Clear();
    ends.Clear();
    return ok;
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
//   otherwise:             where the fillet and C meet, marched from one
//                          cut to the other. Kept as points, not exact.
static bool SetUpEndFace(SShell *sh, EdgeMod *em, int k) {
    const Piece &f = em->nb[0][k], &g = em->nb[1][k];
    hSSurface hc = OtherFace(f, em->face[0]);
    if(hc.v != OtherFace(g, em->face[1]).v) {
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
    if(fabs(tan.Dot(nc)) < 1e-3) {
        dbp("fillet: edge nearly parallel to its end face");
        return false;
    }
    if(!IntersectCarriers(em->off[0], f.b, em->vtx[k], &em->cut[0][k]) ||
       !IntersectCarriers(em->off[1], g.b, em->vtx[k], &em->cut[1][k]))
    {
        dbp("fillet: offset curve doesn't meet neighbor");
        return false;
    }
    if(!CanMoveEnd(f.b, em->vtx[k], em->cut[0][k]) ||
       !CanMoveEnd(g.b, em->vtx[k], em->cut[1][k]))
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
    } else if(plane && em->e.deg == 1) {
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
        SSurface own;
        std::vector<Vector> pts;
        if(!LongFillet(sh, *em, em->vtx[k], &own) ||
           !MarchPiece(&own, sc, em->cut[0][k], em->cut[1][k],
                       em->cut[1][k].Minus(em->cut[0][k]), /*tangent=*/false, &pts))
        {
            dbp("fillet: can't find where the fillet meets its end face");
            return false;
        }
        double tmin = min(t0, t1), tmax = max(t0, t1);
        for(Vector &q : pts) {
            double t = ParamOn(em->e, q);
            tmin = min(tmin, t);
            tmax = max(tmax, t);
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
    const Piece &f = em->nb[s][k], &g = em->nb[o][k];
    // The tangent neighbor's other face must be on the other side of g.
    if(OtherFace(g, em->face[o]).v != OtherFace(f, em->face[s]).v) {
        dbp("fillet: tangent neighbors don't share a face");
        return false;
    }
    em->tSrf[k] = (k == 0) ? 0.0 : 1.0;
    for(int i = 0; i < 2; i++) {
        em->cut[i][k] = (k == 0) ? em->off[i].Start() : em->off[i].Finish();
    }
    if(!CanMoveEnd(g.b, em->vtx[k], em->cut[o][k])) {
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
    const EdgeMod &fm = mods[NeighborMod(mods, em, s, k)];
    const Piece &f = em->nb[s][k], &g = em->nb[o][k];
    if(fabs(fm.r - em->r) > LENGTH_EPS || fm.chamfer != em->chamfer) {
        dbp("fillet: edges meeting at a corner must have the same radius");
        return false;
    }
    if(fm.convex != em->convex) {
        dbp("fillet: convex and concave edges at one corner, not supported");
        return false;
    }
    if(OtherFace(g, em->face[o]).v != OtherFace(f, em->face[s]).v) {
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

    // With straight edges and flat faces the miter is exact, see
    // ExactMiterEnd(). Otherwise
    // TraceMiterEnd() finds it. Either runs once every edge is set up.
    em->traced[k] = true;
    SSurface *ss = sh->surface.FindById(em->face[s]);
    Vector pn;
    double pd;
    int fo = 1 - fs;
    if(em->e.deg == 1 && fm.e.deg == 1 &&
       SurfaceIsPlane(ss, &pn, &pd) &&
       SurfaceIsPlane(sh->surface.FindById(em->face[o]), &pn, &pd) &&
       SurfaceIsPlane(sh->surface.FindById(fm.face[fo]), &pn, &pd))
    {
        em->pieceExact[k] = true;
        return true;
    }
    // At a reflex corner of S the offsets cross past the vertex and the
    // fillets run over each other; tracing toward the vertex would hit the
    // partner's other face where that face doesn't exist. The corner is
    // reflex when f, leaving the vertex, heads out of S as seen from e. With
    // equal cross-sections there is nothing to trace for (no face cut), so
    // that case is fine.
    Vector in = InwardAt(ss, em->e, ss->trim[em->trim[s]].backwards,
                         (k == 0) ? 0.0 : 1.0);
    Vector fAway = fm.e.Start().Equals(em->vtx[k])
                       ? fm.e.TangentAt(0.0)
                       : fm.e.TangentAt(1.0).ScaledBy(-1.0);
    if(fAway.Dot(in) < 0 && fabs(em->w - fm.w) > 1e-9) {
        dbp("fillet: miter at a reflex corner with curved faces, not supported yet");
        return false;
    }
    return true;
}

// The arc of the circle about center that starts at center + u0 (|u0| is
// the radius) and turns toward u1, from angle th0 to th1. Less than 180
// degrees.
static SBezier ArcBetween(Vector center, Vector u0, Vector u1, double th0, double th1) {
    double rad = u0.Magnitude();
    Vector a = u0.WithMagnitude(1.0),
           b = (u1.Minus(a.ScaledBy(u1.Dot(a)))).WithMagnitude(1.0);
    double thm = (th0 + th1) / 2, half = (th1 - th0) / 2;
    Vector p0 = center.Plus(a.ScaledBy(rad*cos(th0))).Plus(b.ScaledBy(rad*sin(th0))),
           p2 = center.Plus(a.ScaledBy(rad*cos(th1))).Plus(b.ScaledBy(rad*sin(th1))),
           pm = center.Plus(a.ScaledBy(rad*cos(thm)/cos(half)))
                      .Plus(b.ScaledBy(rad*sin(thm)/cos(half)));
    return SBezier::From(Vector4::From(1.0, p0),
                         Vector4::From(cos(half), pm),
                         Vector4::From(1.0, p2));
}

// Curve b moved along direction d onto the plane n . x = dn. A parallel
// projection, so a conic stays a conic with the same weights.
static bool ProjectAlong(const SBezier &b, Vector d, Vector n, double dn, SBezier *out) {
    if(fabs(n.Dot(d)) < 1e-9 * d.Magnitude()) return false;
    *out = b;
    for(int i = 0; i <= b.deg; i++) {
        double lam = (dn - n.Dot(b.ctrl[i])) / n.Dot(d);
        out->ctrl[i] = b.ctrl[i].Plus(d.ScaledBy(lam));
    }
    return true;
}

// Unit direction along straight edge e toward its end k.
static Vector TowardEnd(const SBezier &e, int k) {
    Vector d = e.Finish().Minus(e.Start());
    return ((k == 1) ? d : d.ScaledBy(-1.0)).WithMagnitude(1.0);
}

// An exact miter at end k of mods[i]: straight edges, flat faces, at a
// convex or reflex corner of the shared face S and with equal or unequal
// angles.
//
// Fillets: both are circular cylinders of radius r whose axes (spines) lie
// in S offset by r, so they cross, at c. Equal cylinders with crossing axes
// meet in the plane through c that bisects the axes; its normal is the
// difference of the edges' directions toward the vertex, and it contains
// P_S, the corner of the offsets in S. Reflecting in that plane swaps the
// fillets and keeps the angle around each axis, so along the miter both are
// at the same angle from S. The fillet with the shorter section arc ends
// when the miter reaches its contact with its other face, at Y; the other
// goes on and cuts into that face, down to g. Each piece is a sub-arc of a
// cross-section projected along its edge onto a plane: an ellipse.
//
// Chamfers: the two chamfer planes meet in a line from P_S, which ends where
// it first reaches either chamfer's other edge, at Y; the other chamfer cuts
// into that face in a line, down to g.
static bool ExactMiterEnd(SShell *sh, std::vector<EdgeMod> &mods, int i, int k) {
    EdgeMod *em = &mods[i];
    int s = em->modSide[k], o = 1 - s;
    const EdgeMod &fm = mods[NeighborMod(mods, em, s, k)];
    int fk = fm.vtx[0].Equals(em->vtx[k]) ? 0 : 1;
    int fs = (fm.face[0].v == em->face[s].v) ? 0 : 1, fo = 1 - fs;
    const Piece &g = em->nb[o][k];
    em->pieceFace[k] = OtherFace(g, em->face[o]);
    Vector fn;
    double fd;
    if(!SurfaceIsPlane(sh->surface.FindById(em->pieceFace[k]), &fn, &fd)) return false;

    Vector P  = em->cut[s][k],
           de = TowardEnd(em->e, k),
           df = TowardEnd(fm.e, fk),
           nm = de.Minus(df);
    if(nm.Magnitude() < 1e-6) return false;
    nm = nm.WithMagnitude(1.0);
    double dm = nm.Dot(P);

    SBezier miter, cutIn;
    bool shorter, equal;
    if(em->chamfer) {
        Vector a0 = em->off[s].Start(), a1 = em->off[o].Start(),
               b0 = fm.off[fs].Start(), b1 = fm.off[fo].Start();
        Vector nE = (a1.Minus(a0)).Cross(de),
               nF = (b1.Minus(b0)).Cross(df),
               m  = nE.Cross(nF);
        if(m.Magnitude() < 1e-9) return false;
        bool skew;
        double lE, lF, dummy;
        Vector::AtIntersectionOfLines(P, P.Plus(m), em->off[o].ctrl[0], em->off[o].ctrl[1],
                                      &skew, &lE, &dummy);
        Vector::AtIntersectionOfLines(P, P.Plus(m), fm.off[fo].ctrl[0], fm.off[fo].ctrl[1],
                                      &skew, &lF, &dummy);
        if(lE * lF <= 0) return false;
        double l = (fabs(lE) < fabs(lF)) ? lE : lF;
        equal   = fabs(fabs(lE) - fabs(lF)) < LENGTH_EPS / m.Magnitude();
        shorter = !equal && fabs(lE) < fabs(lF);
        Vector Y = P.Plus(m.ScaledBy(l));
        miter = SBezier::From(P, Y);
        if(!equal && !shorter) {
            Vector Q;
            if(!IntersectCarriers(em->off[o], g.b, em->vtx[k], &Q)) return false;
            cutIn = SBezier::From(Y, Q);
        }
    } else {
        Vector ce = em->spine.Start(), cf = fm.spine.Start();
        Vector u0 = em->off[s].Start().Minus(ce), u1 = em->off[o].Start().Minus(ce),
               v0 = fm.off[fs].Start().Minus(cf), v1 = fm.off[fo].Start().Minus(cf);
        double Ae = acos(max(-1.0, min(1.0, u0.WithMagnitude(1).Dot(u1.WithMagnitude(1))))),
               Af = acos(max(-1.0, min(1.0, v0.WithMagnitude(1).Dot(v1.WithMagnitude(1)))));
        double thY = min(Ae, Af);
        equal   = fabs(Ae - Af) < 1e-9;
        shorter = !equal && Ae < Af;
        if(!ProjectAlong(ArcBetween(ce, u0, u1, 0, thY), de, nm, dm, &miter)) return false;
        if(!equal && !shorter) {
            if(!ProjectAlong(ArcBetween(ce, u0, u1, thY, Ae), de, fn, fd, &cutIn)) return false;
        }
    }
    // Set the ends exactly.
    miter.ctrl[0] = P;
    Vector Y = miter.Finish();
    em->xPt[k] = Y;
    if(equal || shorter) {
        em->cut[o][k] = Y;
        em->nPiece[k] = 1;
    } else {
        cutIn.ctrl[0] = Y;
        if(!IntersectCarriers(em->off[o], g.b, em->vtx[k], &em->cut[o][k])) return false;
        cutIn.ctrl[cutIn.deg] = em->cut[o][k];
        em->nPiece[k] = 2;
    }
    if(equal || !shorter) {
        if(!CanMoveEnd(g.b, em->vtx[k], em->cut[o][k])) {
            dbp("fillet: neighbor is too short for this radius");
            return false;
        }
    }

    // Pieces in order from cut[0] to cut[1]: from S, the miter then the cut
    // into the face.
    SBezier pc[2];
    int np = em->nPiece[k];
    pc[0] = miter;
    if(np == 2) pc[1] = cutIn;
    for(int n = 0; n < np; n++) {
        int idx = (s == 0) ? n : (np - 1 - n);
        SBezier c = pc[n];
        if(s == 1) c.Reverse();
        em->pieceCurve[k][idx]  = c;
        em->pieceOnFace[k][idx] = (n == 1);
    }

    // The surface must reach the whole end: the curves lie within their
    // control points.
    double tmin = VERY_POSITIVE, tmax = VERY_NEGATIVE;
    for(int n = 0; n < np; n++) {
        for(int j = 0; j <= pc[n].deg; j++) {
            double t = ParamOn(em->mid, pc[n].ctrl[j]);
            tmin = min(tmin, t);
            tmax = max(tmax, t);
        }
    }
    em->tSrf[k]  = (k == 0) ? tmin : tmax;
    em->cutsG[k] = !(shorter);
    return true;
}

// Signed distance from p to surface srf, along the normal at the closest
// point q. dist is |p - q|, and inside says whether q is within the
// surface's (u, v) square.
static double SignedDistTo(SSurface *srf, Vector p, double *dist, bool *inside) {
    Point2d uv;
    srf->ClosestPointTo(p, &uv, /*mustConverge=*/false);
    Vector q = srf->PointAt(uv),
           n = srf->NormalAt(uv).WithMagnitude(1.0);
    *dist   = (p.Minus(q)).Magnitude();
    *inside = uv.x > -1e-6 && uv.x < 1 + 1e-6 && uv.y > -1e-6 && uv.y < 1 + 1e-6;
    return (p.Minus(q)).Dot(n);
}

// What a traced path can stop on: the partner fillet (only within its
// cross-section) and/or a face.
struct PathTargets {
    SSurface *partner;
    SSurface *face;
};

// March along path (a curve on the fillet, carried along the edge) from
// parameter tFrom toward tTo, and find the first point where it crosses the
// partner fillet or the face. which is 0 for the partner, 1 for the face.
static bool FirstPathHit(const PathTargets &pt, const SBezier &path,
                         double tFrom, double tTo, double *t, Vector *p, int *which)
{
    const int N = 200;
    SSurface *srf[2] = { pt.partner, pt.face };
    double prevG[2] = { 0, 0 }, dist;
    bool inside;
    for(int i = 0; i < 2; i++) {
        if(srf[i]) prevG[i] = SignedDistTo(srf[i], path.PointAt(tFrom), &dist, &inside);
    }
    double prevT = tFrom;
    for(int step = 1; step <= N; step++) {
        double tt = tFrom + (tTo - tFrom) * step / N;
        Vector pp = path.PointAt(tt);
        bool found = false;
        for(int i = 0; i < 2; i++) {
            if(!srf[i]) continue;
            double gi = SignedDistTo(srf[i], pp, &dist, &inside);
            if((gi > 0) != (prevG[i] > 0)) {
                double lo = prevT, hi = tt, glo = prevG[i];
                for(int it = 0; it < 60; it++) {
                    double m  = (lo + hi) / 2,
                           gm = SignedDistTo(srf[i], path.PointAt(m), &dist, &inside);
                    if((gm > 0) == (glo > 0)) {
                        lo  = m;
                        glo = gm;
                    } else {
                        hi = m;
                    }
                }
                double tr = (lo + hi) / 2;
                Vector pr = path.PointAt(tr);
                SignedDistTo(srf[i], pr, &dist, &inside);
                // A real crossing, not a jump of the closest point; on the
                // partner, within its cross-section.
                bool real = dist < LENGTH_EPS && (i == 1 || inside);
                if(real && (!found || (tr - *t) * (tTo - tFrom) < 0)) {
                    *t     = tr;
                    *p     = pr;
                    *which = i;
                    found  = true;
                }
            }
            prevG[i] = gi;
        }
        if(found) return true;
        prevT = tt;
    }
    return false;
}

// The path of section point sec(sp) along the edge.
static bool PathAt(const EdgeMod *em, const SBezier &sec, double sp, SBezier *path) {
    return FollowEdge(em->e, sec.PointAt(sp), path);
}

// A traced miter at end k of mods[i]: straight or arc edges, any of the
// supported faces. Carried along e toward the vertex (along a line, or
// around the arc's axis), each point of e's cross-section first crosses
// either the partner's fillet (only within its cross-section) or the
// partner's other face, beyond g. From the shared face S that is the
// partner; it may switch to the face at some point X, where the miter meets
// the partner's contact with that face. Then this fillet ends in two
// pieces, the miter and a cut into the face; otherwise in one, the miter.
// The crossings only find X; the pieces themselves are marched, as Booleans
// march intersection curves. Reads the shell only.
static bool TraceMiterEnd(SShell *sh, std::vector<EdgeMod> &mods, int i, int k) {
    EdgeMod *em = &mods[i];
    int s = em->modSide[k], o = 1 - s;
    const EdgeMod &pm = mods[NeighborMod(mods, em, s, k)];
    const Piece &g = em->nb[o][k];
    em->pieceFace[k] = OtherFace(g, em->face[o]);

    // Both fillets, reaching well past their ends at this vertex.
    SSurface own, partner;
    if(!LongFillet(sh, *em, em->vtx[k], &own) ||
       !LongFillet(sh, pm, em->vtx[k], &partner))
    {
        return false;
    }

    // Equal cross-sections (the same angles, as on a straight extrusion):
    // the two fillets are mirror images about the miter, so it runs from the
    // corner in S straight to g, with no cut into a face.
    if(fabs(em->w - pm.w) < 1e-9) {
        if(!IntersectCarriers(em->off[o], g.b, em->vtx[k], &em->cut[o][k]) ||
           !CanMoveEnd(g.b, em->vtx[k], em->cut[o][k]))
        {
            dbp("fillet: miter can't reach the third edge");
            return false;
        }
        em->xPt[k] = em->cut[o][k];
        std::vector<Vector> miter;
        Vector mStart = em->cut[s][k];
        if(!MarchPiece(&own, &partner, mStart, em->cut[o][k],
                       em->cut[o][k].Minus(mStart), /*tangent=*/true, &miter))
        {
            dbp("fillet: can't trace the miter");
            return false;
        }
        if(s == 1) std::reverse(miter.begin(), miter.end());
        em->nPiece[k]          = 1;
        em->piecePts[k][0]     = miter;
        em->pieceOnFace[k][0]  = false;
        double t0 = ParamOn(em->off[0], em->cut[0][k]),
               t1 = ParamOn(em->off[1], em->cut[1][k]);
        double tmin = min(t0, t1), tmax = max(t0, t1);
        for(Vector &q : miter) {
            double t = ParamOn(em->e, q);
            tmin = min(tmin, t);
            tmax = max(tmax, t);
        }
        em->tSrf[k]  = (k == 0) ? tmin : tmax;
        em->cutsG[k] = true;
        return true;
    }

    PathTargets pt = {};
    pt.partner = &partner;
    pt.face    = sh->surface.FindById(em->pieceFace[k]);
    double ext   = (em->e.deg == 1) ? 0.5 : 0.3,
           tFrom = 0.5,
           tTo   = (k == 0) ? -ext : 1 + ext;
    SBezier sec = StartSection(em);

    // Section parameters at S (where the miter starts) and at the other face.
    double sS = (s == 0) ? 0.0 : 1.0, sO = 1.0 - sS;
    double t;
    Vector p;
    int which;
    SBezier path;
    double sInS = sS + (sO - sS) * 1e-3;
    if(!PathAt(em, sec, sInS, &path) ||
       !FirstPathHit(pt, path, tFrom, tTo, &t, &p, &which) || which != 0)
    {
        dbp("fillet: miter doesn't start on the partner fillet");
        return false;
    }
    Vector nearS = p;
    Vector q;
    bool faceAtO = PathAt(em, sec, sO, &path) &&
                   FirstPathHit(pt, path, tFrom, tTo, &t, &q, &which) && which == 1;
    double sX = sO;
    if(faceAtO) {
        // Bisect for where the path stops hitting the partner.
        double lo = sInS, hi = sO;
        for(int it = 0; it < 50; it++) {
            double m = (lo + hi) / 2;
            if(!PathAt(em, sec, m, &path) ||
               !FirstPathHit(pt, path, tFrom, tTo, &t, &p, &which))
            {
                return false;
            }
            if(which == 0) lo = m; else hi = m;
        }
        sX = (lo + hi) / 2;
        if(fabs(sX - sO) < 1e-6) faceAtO = false;
    }
    // With a face piece, X is where the paths switch from the partner to the
    // face, on the partner's contact with that face. Without one, it is
    // where our own contact with the other face hits the partner.
    int ps = (pm.face[0].v == em->face[s].v) ? 0 : 1, po = 1 - ps;
    if(!PathAt(em, sec, sX, &path) ||
       !FirstPathHit(pt, path, tFrom, tTo, &t, &p, &which))
    {
        return false;
    }
    if(faceAtO) {
        double tc;
        pm.off[po].ClosestPointTo(p, &tc, /*mustConverge=*/false);
        em->xPt[k] = pm.off[po].PointAt(tc);
    } else {
        em->xPt[k] = p;
    }

    if(faceAtO) {
        // This fillet reaches g, and re-cuts it.
        if(!IntersectCarriers(em->off[o], g.b, em->vtx[k], &em->cut[o][k]) ||
           !CanMoveEnd(g.b, em->vtx[k], em->cut[o][k]))
        {
            dbp("fillet: miter can't reach the third edge");
            return false;
        }
    } else {
        em->cut[o][k] = em->xPt[k];
    }

    // March the pieces: the miter from the corner in S (where the two
    // fillets are tangent to each other, both being tangent to S) toward the
    // point found next to it, then any cut into the face from X down to g.
    Vector mStart = em->cut[s][k],
           mEnd   = faceAtO ? em->xPt[k] : em->cut[o][k];
    std::vector<Vector> miter, faceCut;
    if(!MarchPiece(&own, pt.partner, mStart, mEnd, nearS.Minus(mStart), /*tangent=*/true, &miter) ||
       (faceAtO && !MarchPiece(&own, pt.face, em->xPt[k], em->cut[o][k],
                               em->cut[o][k].Minus(em->xPt[k]), /*tangent=*/false, &faceCut)))
    {
        dbp("fillet: can't trace the miter");
        return false;
    }
    // In order from cut[0] to cut[1]; S is at cut[s].
    em->nPiece[k] = faceAtO ? 2 : 1;
    if(s == 1) {
        std::reverse(miter.begin(), miter.end());
        std::reverse(faceCut.begin(), faceCut.end());
    }
    int mi = (s == 0) ? 0 : em->nPiece[k] - 1;
    em->piecePts[k][mi]    = miter;
    em->pieceOnFace[k][mi] = false;
    if(faceAtO) {
        em->piecePts[k][1 - mi]    = faceCut;
        em->pieceOnFace[k][1 - mi] = true;
    }

    // The surface must reach every point of the end.
    double t0 = ParamOn(em->off[0], em->cut[0][k]),
           t1 = ParamOn(em->off[1], em->cut[1][k]);
    double tmin = min(t0, t1), tmax = max(t0, t1);
    for(int n = 0; n < em->nPiece[k]; n++) {
        for(Vector &q : em->piecePts[k][n]) {
            double t = ParamOn(em->e, q);
            tmin = min(tmin, t);
            tmax = max(tmax, t);
        }
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
        const EdgeMod &fm = mods[NeighborMod(mods, em, s, k)];
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
        int i = FindTrimAtVertex(srf, em->trim[s], v);
        if(i < 0) {
            dbp("fillet: no single neighbor at vertex");
            return false;
        }
        if(!GetPiece(sh, em->face[s], i, &em->nb[s][k])) {
            dbp("fillet: neighbor is not an exact edge");
            return false;
        }
        em->nbTrim[s][k] = i;
        modified[s] = (FindEdgeModAt(mods, em->face[s], i) >= 0);
    }
    if(modified[0] && modified[1]) {
        return SetUpCornerEnd(sh, mods, em, k);
    }
    if(!modified[0] && !modified[1]) {
        return SetUpEndFace(sh, em, k);
    }

    int s = modified[0] ? 0 : 1;
    const SBezier &fb = em->nb[s][k].b;
    double tf = fb.Start().Equals(v) ? 0.0 : 1.0;
    Vector te = em->e.TangentAt((k == 0) ? 0.0 : 1.0).WithMagnitude(1.0),
           tn = fb.TangentAt(tf).WithMagnitude(1.0);
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
    int j = NeighborMod(mods, em, em->modSide[k], k);
    if(mods[j].convex != em->convex) {
        dbp("fillet: convex and concave edges meet, not supported");
        return false;
    }
    for(int pk = 0; pk < 2; pk++) {
        EdgeMod &pm = mods[j];
        if(pm.kind[pk] != em->kind[k] || !pm.vtx[pk].Equals(em->vtx[k])) {
            continue;
        }
        if(em->traced[k] || pm.traced[pk]) {
            // Only the corner in the shared face is known yet.
            Vector a = em->cut[em->modSide[k]][k],
                   b = pm.cut[pm.modSide[pk]][pk];
            if(!a.Equals(b) || em->traced[k] != pm.traced[pk]) return false;
        } else {
            // A join that is tangent only to within the tangency test (font
            // outlines, with their rounded coordinates) leaves the two
            // offsets ending a little apart; snap the partner's onto ours.
            double tol = (em->kind[k] == TANGENT) ? 1e-3 : LENGTH_EPS;
            bool same    = pm.cut[0][pk].Equals(em->cut[0][k], tol) &&
                           pm.cut[1][pk].Equals(em->cut[1][k], tol),
                 swapped = pm.cut[0][pk].Equals(em->cut[1][k], tol) &&
                           pm.cut[1][pk].Equals(em->cut[0][k], tol);
            if(!same && !swapped) return false;
            pm.cut[0][pk] = same ? em->cut[0][k] : em->cut[1][k];
            pm.cut[1][pk] = same ? em->cut[1][k] : em->cut[0][k];
            // Move the ends of the partner's rows too, so its surface goes
            // through the shared points and end curve exactly.
            int ei = (k == 0) ? 0 : em->e.deg, pi = (pk == 0) ? 0 : pm.e.deg;
            for(int s2 = 0; s2 < 2; s2++) {
                int es = same ? s2 : 1 - s2;
                pm.off[s2].ctrl[pi] = em->off[es].ctrl[ei];
            }
            pm.mid.ctrl[pi]   = em->mid.ctrl[ei];
            pm.spine.ctrl[pi] = em->spine.ctrl[ei];
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

static int FindOrAddRecut(std::vector<Recut> *rcs, const Piece &pc) {
    for(size_t i = 0; i < rcs->size(); i++) {
        const Piece &q = (*rcs)[i].pc;
        if(q.face[0].v == pc.face[0].v && q.trim[0] == pc.trim[0]) return (int)i;
    }
    Recut rc = {};
    rc.pc     = pc;
    rc.start  = pc.start;
    rc.finish = pc.finish;
    rcs->push_back(rc);
    return (int)rcs->size() - 1;
}

// Move the end of neighbor piece pc that is at vertex v to the cut point.
static bool SetRecutEnd(std::vector<Recut> *rcs, const Piece &pc, Vector v, Vector cut) {
    Recut *rc = &(*rcs)[FindOrAddRecut(rcs, pc)];
    // At a tangent joint or a miter both fillets cut g at the same point.
    if(pc.start.Equals(v)) {
        if(!rc->start.Equals(v) && !rc->start.Equals(cut)) return false;
        rc->start = cut;
    } else if(pc.finish.Equals(v)) {
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

static bool CurveInUse(SShell *sh, hSCurve hc) {
    for(SSurface &s : sh->surface) {
        for(STrimBy &stb : s.trim) {
            if(stb.curve.v == hc.v) return true;
        }
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
        hSCurve hc = sc->h;
        hSSurface hfaces[2] = { sc->surfA, sc->surfB };
        if(!sc->isExact) {
            dbp("fillet: edge is not exact");
            return;
        }
        // Each trim on this curve is (one side of) one piece of it. The
        // curve may be used on only one face, the other face using a copy.
        for(int fi = 0; fi < 2; fi++) {
        SSurface *sa = surface.FindById(hfaces[fi]);
        for(int ti = 0; ti < sa->trim.n; ti++) {
            if(sa->trim[ti].curve.v != hc.v) continue;
            if(FindEdgeModAt(mods, hfaces[fi], ti) >= 0) continue;
            Piece pc;
            if(!GetPiece(this, hfaces[fi], ti, &pc)) {
                dbp("fillet: edge piece isn't trimmed on both faces");
                return;
            }
            EdgeMod em = {};
            em.orig    = hc;
            em.r       = fabs(m);
            em.chamfer = (m < 0);
            em.e       = pc.b;
            em.face[0] = pc.face[0];
            em.face[1] = pc.face[1];
            em.trim[0] = pc.trim[0];
            em.trim[1] = pc.trim[1];
            em.origCurve[0] = pc.hc[0];
            em.origCurve[1] = pc.hc[1];
            mods.push_back(em);
        }
        }
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
            if(mods[i].kind[k] != MITER || !mods[i].traced[k]) continue;
            bool ok = mods[i].pieceExact[k] ? ExactMiterEnd(this, mods, (int)i, k)
                                            : TraceMiterEnd(this, mods, (int)i, k);
            if(!ok) {
                dbp("fillet: can't build the miter");
                return;
            }
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
                ok = ok && SetRecutEnd(&recuts, em.nb[s][k], em.vtx[k], em.cut[s][k]);
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
        if(!SubCurve(rc.pc.b, rc.start, rc.finish, &rc.b)) {
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
        rc.newh = AddCurve(this, rc.b, rc.pc.face[0], rc.pc.face[1]);
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
                    hSSurface other = em.pieceOnFace[k][n] ? em.pieceFace[k] : pm.hFillet;
                    if(em.pieceExact[k]) {
                        em.hPiece[k][n] = AddCurve(this, em.pieceCurve[k][n],
                                                   em.hFillet, other);
                    } else {
                        em.hPiece[k][n] = AddPwlCurve(this, em.piecePts[k][n],
                                                      em.hFillet, other);
                    }
                    if(!em.pieceOnFace[k][n]) pm.hPiece[pk][0] = em.hPiece[k][n];
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

    // Neighbors: point each piece's two trims at its re-cut curve. Other
    // pieces of the same curve keep it.
    for(Recut &rc : recuts) {
        for(int i = 0; i < 2; i++) {
            STrimBy *stb = &surface.FindById(rc.pc.face[i])->trim[rc.pc.trim[i]];
            bool along = stb->start.Equals(rc.pc.start);
            Vector a = along ? rc.start : rc.finish,
                   b = along ? rc.finish : rc.start;
            *stb = MakeTrim(rc.newh, a, b, rc.start);
        }
    }

    // Remove the curves of modified edges and re-cut neighbors that no trim
    // uses any more. SCurve has no tag, so copy the live curves to a new
    // list, which takes over their points.
    std::vector<hSCurve> touched;
    for(EdgeMod &em : mods) {
        touched.push_back(em.origCurve[0]);
        touched.push_back(em.origCurve[1]);
    }
    for(Recut &rc : recuts) {
        touched.push_back(rc.pc.hc[0]);
        touched.push_back(rc.pc.hc[1]);
    }
    IdList<SCurve,hSCurve> kept = {};
    for(SCurve &sc : curve) {
        bool dead = false;
        for(hSCurve h : touched) {
            if(h.v == sc.h.v) dead = !CurveInUse(this, sc.h);
        }
        if(dead) continue;
        kept.Add(&sc);
        sc.pts = {};
    }
    curve.Clear();
    kept.MoveSelfInto(&curve);
}

} // namespace SolveSpace
