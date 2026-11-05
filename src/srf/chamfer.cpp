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
// - Corner patches (3 fillets at a vertex, sphere octant / flat triangle for
//   chamfers) come later, probably in their own file. Until then the hole is
//   left open: the fillet surfaces still get their end curves so their own
//   trims close, those curves have only one surface, and SolveSpace reports
//   naked edges. The fillet surfaces should still draw.
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
// 4. Three modified edges (cube corner). Cases 3 on each pair; the corner
//    patch itself comes later (see scope).
// Anything else (end face not perpendicular, etc.) is unsupported for now:
// skip the edge. In general that needs SSurface::IntersectAgainst().
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
//   struct EdgeMod {
//       hSCurve   orig;          // original edge e
//       double    r;
//       bool      chamfer;
//       hSSurface sA, sB;        // faces A and B
//       int       trimA, trimB;  // index of e's trim in sA, sB
//       int       vtx[2];        // vertex record at e's start and finish
//       SBezier   offA, offB;    // offset curves eA, eB
//       Vector    cutA[2], cutB[2];
//       hSCurve   newA, newB;    // new trim curves in A and B
//       hSSurface fillet;
//   };
//   struct EdgeVertex {
//       Vector    p;
//       // per curve meeting here: handle, which end, EdgeMod index or -1
//       // per face around it: handle, convex or reflex corner
//       int       kind;          // case 1-4 or unsupported
//   };
//
// Test order (cube with all coordinates at +/-10)
// -----------------------------------------------
// 1. Chamfer one top edge: line between two planes, case 1 at both ends.
// 2. The same edge as a fillet.
// 3. A concave edge, e.g. an L-shaped extrusion.
// 4. Top edge of a cylinder: arcs in a tangent chain (case 2).
// 5. Two top edges of the cube meeting at a corner (case 3).
// 6. Three edges at a corner, with the hole left open (case 4).
// Check each time: offset direction, surface normal, trim directions, and no
// "trim was unterminated" / "trim was empty" messages.
//
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

// Weight of the middle control point of a rational quadratic quarter circle,
// cos(45 degrees). Used for the fillet cross-section.
static const double QUARTER_ARC_WEIGHT = 0.70710678118654752440;

// One edge to modify. Stored by value: no pointers into the shell's lists,
// which can reallocate when things are added. Arrays of 2 are per end of the
// edge: [0] at e.Start(), [1] at e.Finish().
struct EdgeMod {
    hSCurve   orig;           // the original edge
    double    r;
    bool      chamfer;
    hSSurface sA, sB;         // faces A and B
    int       trimA, trimB;   // index of the edge's trim in sA and sB
    SBezier   e;              // the original edge
    SBezier   offA, offB;     // offset curves eA and eB
    Vector    vtx[2];         // original vertex
    hSCurve   nbA[2], nbB[2]; // neighbor curves f (in A) and g (in B)
    int       kind[2];        // END_FACE or TANGENT, see SetUpEnd()
    hSSurface sC[2];          // END_FACE: end face C, shared by f and g
    Vector    cutA[2], cutB[2]; // where eA and eB end
    double    tCut[2];        // parameter of the cuts on eA, eB and e
    // Geometry built before the shell is changed
    SBezier   newA, newB, endC[2];
    SSurface  fillet;
    // Handles assigned when the shell is changed
    hSCurve   hNewA, hNewB, hEndC[2];
    hSSurface hFillet;
    int       partner[2], partnerEnd[2]; // TANGENT: the neighbor's EdgeMod
};

// Vertex cases, see the plan above.
static const int END_FACE = 1;  // case 1: one modified edge, end face C
static const int TANGENT  = 2;  // case 2: tangent chain of modified edges

// A neighbor curve that gets shortened at one or both ends.
struct Recut {
    hSCurve   orig;
    Vector    start, finish;  // new ends, in the curve's own direction
    SBezier   b;
    hSCurve   newh;
};

// We return 0 for no feature, radius for a fillet, and for chamfers a negative
// value whose absolute value is the radius of an equivalent fillet.
double SShell::GetEdgeModifier(SCurve &sc)
{
    // Test hook until edges can be named. Picks edges in the test models in
    // misc_CAD, which don't overlap:
    //   test_cube, L_shape: the line at y = 10, z = 10 (convex)
    //   L_shape:            the line at x = 0, y = 0 (concave)
    //   test_cylinder:      the arcs of the top circle at z = 10
    const double r = 3.0;   // negative for chamfers
    if(!sc.isExact) return 0.0;
    Vector a = sc.exact.Start(), b = sc.exact.Finish();
    if(sc.exact.deg == 1 &&
       fabs(a.y - 10) < LENGTH_EPS && fabs(b.y - 10) < LENGTH_EPS &&
       fabs(a.z - 10) < LENGTH_EPS && fabs(b.z - 10) < LENGTH_EPS)
    {
        return r;
    }
    if(sc.exact.deg == 1 &&
       fabs(a.x) < LENGTH_EPS && fabs(b.x) < LENGTH_EPS &&
       fabs(a.y) < LENGTH_EPS && fabs(b.y) < LENGTH_EPS)
    {
        return r;
    }
    if(sc.exact.deg == 2 &&
       fabs(a.z - 10) < LENGTH_EPS && fabs(b.z - 10) < LENGTH_EPS)
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

// True if the curve has exactly one trim on each of its surfaces, each
// covering the whole curve.
static bool CurveFullyTrimmed(SShell *sh, SCurve *sc) {
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

// Offset e into surface s by distance d, away from the edge. Only the exact
// cases for now:
//   inward direction constant along e -> translate (line in a plane, or any
//                                        curve on a face perpendicular to a
//                                        plane)
//   arc in a plane face               -> concentric arc, same weights
static bool OffsetEdgeInto(SSurface *s, const SBezier &e, bool backwards,
                           double d, SBezier *out)
{
    Vector in0 = InwardAt(s, e, backwards, 0.0),
           in1 = InwardAt(s, e, backwards, 1.0);
    if(in0.Equals(in1)) {
        *out = e.TransformedBy(in0.ScaledBy(d), Quaternion::IDENTITY, 1.0);
        return true;
    }

    Vector n, center;
    double pd, rad;
    if(SurfaceIsPlane(s, &n, &pd) && e.IsCircle(n, &center, &rad)) {
        Vector inm = InwardAt(s, e, backwards, 0.5);
        double sign = (inm.Dot(e.PointAt(0.5).Minus(center)) > 0) ? 1.0 : -1.0;
        double newr = rad + sign*d;
        if(newr < LENGTH_EPS) return false;
        *out = e;
        for(int i = 0; i <= e.deg; i++) {
            out->ctrl[i] = center.Plus(
                (e.ctrl[i].Minus(center)).ScaledBy(newr/rad));
        }
        return true;
    }
    return false;
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

// Where the offset curve (extended if needed) meets neighbor curve nb. Lines
// only for now.
static bool IntersectCarrier(const SBezier &off, const SBezier &nb, Vector *p) {
    if(off.deg != 1 || nb.deg != 1) return false;
    Vector da = off.ctrl[1].Minus(off.ctrl[0]),
           db = nb.ctrl[1].Minus(nb.ctrl[0]);
    if((da.Cross(db)).Magnitude() < LENGTH_EPS * da.Magnitude()) return false;
    bool skew;
    *p = Vector::AtIntersectionOfLines(off.ctrl[0], off.ctrl[1],
                                       nb.ctrl[0], nb.ctrl[1], &skew);
    return !skew;
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

// Case 2 at end k: neighbor f in A is modified and tangent to e. The offset
// curves already end where the neighbor's offsets start, so the cuts are
// just their end points. The neighbor g in B gets re-cut.
static bool SetUpTangentEnd(SShell *sh, EdgeMod *em, int k) {
    Vector v = em->vtx[k];
    SCurve *f = sh->curve.FindById(em->nbA[k]),
           *g = sh->curve.FindById(em->nbB[k]);
    double tf = f->exact.Start().Equals(v) ? 0.0 : 1.0;
    Vector te = em->e.TangentAt((k == 0) ? 0.0 : 1.0).WithMagnitude(1.0),
           tn = f->exact.TangentAt(tf).WithMagnitude(1.0);
    if(fabs(fabs(te.Dot(tn)) - 1.0) > 1e-6) {
        dbp("fillet: modified neighbor not tangent (miter), not supported yet");
        return false;
    }
    if(!g->isExact || !CurveFullyTrimmed(sh, g)) {
        dbp("fillet: neighbor in B is not a simple exact curve");
        return false;
    }
    // The tangent neighbor's B face must be on the other side of g.
    hSSurface hb2 = OtherSurface(f, em->sA);
    if(OtherSurface(g, em->sB).v != hb2.v) {
        dbp("fillet: tangent neighbors don't share a face in B");
        return false;
    }
    em->tCut[k] = (k == 0) ? 0.0 : 1.0;
    em->cutA[k] = (k == 0) ? em->offA.Start() : em->offA.Finish();
    em->cutB[k] = (k == 0) ? em->offB.Start() : em->offB.Finish();
    double tg = ParamOn(g->exact, em->cutB[k]);
    if(!g->exact.PointAt(tg).Equals(em->cutB[k]) ||
       tg < -LENGTH_EPS || tg > 1 + LENGTH_EPS)
    {
        dbp("fillet: offset in B doesn't land within the neighbor");
        return false;
    }
    em->kind[k] = TANGENT;
    return true;
}

static bool IsModified(const std::vector<EdgeMod> &mods, hSCurve hc) {
    for(const EdgeMod &em : mods) {
        if(em.orig.v == hc.v) return true;
    }
    return false;
}

// Work out end k of the edge: find the neighbors, classify the vertex, and
// find the cut points. Supported so far:
//   END_FACE  case 1: neighbors not modified, end face a plane perpendicular
//             to the edge, cuts within the neighbors
//   TANGENT   case 2: the neighbor in A is modified and tangent to e; the
//             neighbor in B is not. The offsets end at the same points as the
//             neighbor's, and the two fillets share their end curve.
// Reads the shell only.
static bool SetUpEnd(SShell *sh, const std::vector<EdgeMod> &mods,
                     EdgeMod *em, int k)
{
    Vector v = (k == 0) ? em->e.Start() : em->e.Finish();
    em->vtx[k] = v;

    SSurface *sa = sh->surface.FindById(em->sA),
             *sb = sh->surface.FindById(em->sB);
    int ia = FindTrimAtVertex(sa, em->orig, v),
        ib = FindTrimAtVertex(sb, em->orig, v);
    if(ia < 0 || ib < 0) {
        dbp("fillet: no single neighbor at vertex");
        return false;
    }
    hSCurve hf = sa->trim[ia].curve,
            hg = sb->trim[ib].curve;
    em->nbA[k] = hf;
    em->nbB[k] = hg;
    if(IsModified(mods, hg)) {
        dbp("fillet: modified neighbor in B not supported yet");
        return false;
    }
    if(IsModified(mods, hf)) {
        return SetUpTangentEnd(sh, em, k);
    }
    SCurve *f = sh->curve.FindById(hf),
           *g = sh->curve.FindById(hg);
    if(!f->isExact || !g->isExact ||
       !CurveFullyTrimmed(sh, f) || !CurveFullyTrimmed(sh, g))
    {
        dbp("fillet: neighbor is not a simple exact curve");
        return false;
    }
    hSSurface hc = OtherSurface(f, em->sA);
    if(hc.v != OtherSurface(g, em->sB).v) {
        dbp("fillet: neighbors don't share an end face");
        return false;
    }
    SSurface *sc = sh->surface.FindById(hc);
    Vector nc;
    double dc;
    if(!SurfaceIsPlane(sc, &nc, &dc)) {
        dbp("fillet: end face is not a plane");
        return false;
    }
    Vector tan = em->e.TangentAt((k == 0) ? 0.0 : 1.0).WithMagnitude(1.0);
    if(fabs(fabs(tan.Dot(nc)) - 1.0) > 1e-6) {
        dbp("fillet: end face not perpendicular to edge");
        return false;
    }

    if(!IntersectCarrier(em->offA, f->exact, &em->cutA[k]) ||
       !IntersectCarrier(em->offB, g->exact, &em->cutB[k]))
    {
        dbp("fillet: offset curve doesn't meet neighbor");
        return false;
    }
    // Clip only: the cuts must lie within the neighbors.
    double tf = ParamOn(f->exact, em->cutA[k]),
           tg = ParamOn(g->exact, em->cutB[k]);
    if(tf < -LENGTH_EPS || tf > 1 + LENGTH_EPS ||
       tg < -LENGTH_EPS || tg > 1 + LENGTH_EPS)
    {
        dbp("fillet: neighbor would need extending, not supported yet");
        return false;
    }
    // The rows of the fillet surface must stay in register.
    double ta = ParamOn(em->offA, em->cutA[k]),
           tb = ParamOn(em->offB, em->cutB[k]);
    if(fabs(ta - tb) > 1e-6) {
        dbp("fillet: cuts at different parameters");
        return false;
    }
    em->tCut[k] = ta;

    em->kind[k] = END_FACE;
    em->sC[k]   = hc;
    return true;
}

// The part of curve b from p0 to p1 (both on b or its extension), with the
// ends set exactly so trims match the curve's points.
static bool SubCurve(const SBezier &b, Vector p0, Vector p1, SBezier *out) {
    if(!b.Reparameterized(ParamOn(b, p0), ParamOn(b, p1), out)) return false;
    out->ctrl[0] = p0;
    out->ctrl[out->deg] = p1;
    return true;
}

// Fillet or chamfer surface from rows eA, (e,) eB over [tCut[0], tCut[1]],
// facing outward: roughly along nA + nB.
static bool MakeFilletSurface(EdgeMod *em, Vector nA, Vector nB) {
    SBezier e, a, b;
    double t0 = em->tCut[0], t1 = em->tCut[1];
    if(!em->e.Reparameterized(t0, t1, &e) ||
       !em->offA.Reparameterized(t0, t1, &a) ||
       !em->offB.Reparameterized(t0, t1, &b))
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
            s.weight[i][1] = e.weight[i] * QUARTER_ARC_WEIGHT;
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

// End curve on face C at end k: a line for a chamfer, a quarter circle for a
// fillet. Runs from cutA to cutB.
static void MakeEndCurve(EdgeMod *em, int k) {
    if(em->chamfer) {
        em->endC[k] = SBezier::From(em->cutA[k], em->cutB[k]);
    } else {
        Vector mid = em->e.PointAt(em->tCut[k]);
        em->endC[k] = SBezier::From(Vector4::From(1.0, em->cutA[k]),
                                    Vector4::From(QUARTER_ARC_WEIGHT, mid),
                                    Vector4::From(1.0, em->cutB[k]));
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
    // At a tangent joint both fillets cut g at the same point.
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

// For a TANGENT end, find the neighbor's EdgeMod and which of its ends meets
// ours. Their offsets must meet at the same points.
static bool FindPartnerEnd(std::vector<EdgeMod> &mods, int i, int k) {
    EdgeMod *em = &mods[i];
    for(size_t j = 0; j < mods.size(); j++) {
        if(mods[j].orig.v != em->nbA[k].v) continue;
        for(int pk = 0; pk < 2; pk++) {
            if(mods[j].kind[pk] != TANGENT || !mods[j].vtx[pk].Equals(em->vtx[k])) {
                continue;
            }
            if(!mods[j].cutA[pk].Equals(em->cutA[k]) ||
               !mods[j].cutB[pk].Equals(em->cutB[k]))
            {
                return false;
            }
            em->partner[k]    = (int)j;
            em->partnerEnd[k] = pk;
            return true;
        }
    }
    return false;
}

static bool IsDeadCurve(const std::vector<EdgeMod> &mods,
                        const std::vector<Recut> &recuts, hSCurve hc)
{
    if(IsModified(mods, hc)) return true;
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
        em.sA      = sc->surfA;
        em.sB      = sc->surfB;
        em.e       = sc->exact;
        SSurface *sa = surface.FindByIdNoOops(em.sA),
                 *sb = surface.FindByIdNoOops(em.sB);
        if(!sc->isExact || sa == NULL || sb == NULL) {
            dbp("fillet: edge is not exact");
            continue;
        }
        em.trimA = FindOnlyTrim(sa, sc->h);
        em.trimB = FindOnlyTrim(sb, sc->h);
        if(em.trimA < 0 || em.trimB < 0 ||
           !TrimCoversCurve(sc, &sa->trim[em.trimA]) ||
           !TrimCoversCurve(sc, &sb->trim[em.trimB]))
        {
            dbp("fillet: edge is not trimmed as a whole curve");
            continue;
        }
        if(sa->trim[em.trimA].backwards == sb->trim[em.trimB].backwards) {
            dbp("fillet: edge's trims don't run in opposite directions");
            continue;
        }
        mods.push_back(em);
    }
    if(mods.empty()) return;

    // 2-4. Offset curves, vertices and cut points. Read only. If any edge
    // can't be done, do none: neighbors depend on each other.
    for(EdgeMod &em : mods) {
        SSurface *sa = surface.FindById(em.sA),
                 *sb = surface.FindById(em.sB);
        if(!OffsetEdgeInto(sa, em.e, sa->trim[em.trimA].backwards, em.r, &em.offA) ||
           !OffsetEdgeInto(sb, em.e, sb->trim[em.trimB].backwards, em.r, &em.offB))
        {
            dbp("fillet: can't offset this edge");
            return;
        }
    }
    for(EdgeMod &em : mods) {
        if(!SetUpEnd(this, mods, &em, 0) || !SetUpEnd(this, mods, &em, 1)) return;
    }
    // Pair up the ends at tangent joints.
    std::vector<EdgeMod> &good = mods;
    for(size_t i = 0; i < good.size(); i++) {
        for(int k = 0; k < 2; k++) {
            if(good[i].kind[k] != TANGENT) continue;
            if(!FindPartnerEnd(good, (int)i, k)) {
                dbp("fillet: tangent neighbors don't meet");
                return;
            }
        }
    }

    // 5. Build all the new geometry. If anything fails, leave the shell as
    // it was.
    std::vector<Recut> recuts;
    for(EdgeMod &em : good) {
        for(int k = 0; k < 2; k++) {
            bool ok = SetRecutEnd(&recuts, curve.FindById(em.nbB[k]), em.vtx[k], em.cutB[k]);
            if(em.kind[k] == END_FACE) {
                ok = ok && SetRecutEnd(&recuts, curve.FindById(em.nbA[k]), em.vtx[k], em.cutA[k]);
            }
            if(!ok) {
                dbp("fillet: neighbor cut twice at one end");
                return;
            }
            MakeEndCurve(&em, k);
        }
        Vector mid = em.e.PointAt(0.5);
        Vector nA = SurfaceNormalNear(surface.FindById(em.sA), mid),
               nB = SurfaceNormalNear(surface.FindById(em.sB), mid);
        if(!SubCurve(em.offA, em.cutA[0], em.cutA[1], &em.newA) ||
           !SubCurve(em.offB, em.cutB[0], em.cutB[1], &em.newB) ||
           !MakeFilletSurface(&em, nA, nB))
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

    // 6. Change the shell. Add surfaces and curves first, then rewrite trims,
    // looking things up by handle each time since the lists can reallocate.
    for(EdgeMod &em : good) {
        em.hFillet = surface.AddAndAssignId(&em.fillet);
    }
    for(Recut &rc : recuts) {
        SCurve *old = curve.FindById(rc.orig);
        hSSurface sa = old->surfA, sb = old->surfB;
        rc.newh = AddCurve(this, rc.b, sa, sb);
    }
    for(EdgeMod &em : good) {
        em.hNewA = AddCurve(this, em.newA, em.sA, em.hFillet);
        em.hNewB = AddCurve(this, em.newB, em.hFillet, em.sB);
    }
    // End curves. At a tangent joint the two fillets share one, made by the
    // EdgeMod that comes first.
    for(size_t i = 0; i < good.size(); i++) {
        EdgeMod &em = good[i];
        for(int k = 0; k < 2; k++) {
            if(em.kind[k] == END_FACE) {
                em.hEndC[k] = AddCurve(this, em.endC[k], em.sC[k], em.hFillet);
            } else if(em.partner[k] > (int)i) {
                EdgeMod &pm = good[em.partner[k]];
                em.hEndC[k] = AddCurve(this, em.endC[k], em.hFillet, pm.hFillet);
                pm.hEndC[em.partnerEnd[k]] = em.hEndC[k];
            }
        }
    }

    for(EdgeMod &em : good) {
        // A and B: the trim on e moves to eA / eB, same direction.
        SSurface *sa = surface.FindById(em.sA);
        STrimBy ta = sa->trim[em.trimA];
        Vector a0 = ta.backwards ? em.cutA[1] : em.cutA[0],
               a1 = ta.backwards ? em.cutA[0] : em.cutA[1];
        ta = MakeTrim(em.hNewA, a0, a1, em.cutA[0]);
        sa->trim[em.trimA] = ta;

        SSurface *sb = surface.FindById(em.sB);
        STrimBy tb = sb->trim[em.trimB];
        Vector b0 = tb.backwards ? em.cutB[1] : em.cutB[0],
               b1 = tb.backwards ? em.cutB[0] : em.cutB[1];
        tb = MakeTrim(em.hNewB, b0, b1, em.cutB[0]);
        sb->trim[em.trimB] = tb;

        SSurface *sf = surface.FindById(em.hFillet);
        STrimBy rta = ReverseTrim(ta), rtb = ReverseTrim(tb);
        sf->trim.Add(&rta);
        sf->trim.Add(&rtb);

        // End curves. The fillet's trims form a loop: the reversed trim on
        // eA finishes at ta.start, at end k0, so the end trim there runs
        // from cutA to cutB, and the other one from cutB to cutA. An end
        // face gets the reverse; a tangent partner makes its own.
        for(int k = 0; k < 2; k++) {
            bool fromA = ta.start.Equals(em.cutA[k]);
            Vector c0 = fromA ? em.cutA[k] : em.cutB[k],
                   c1 = fromA ? em.cutB[k] : em.cutA[k];
            SCurve *ec = curve.FindById(em.hEndC[k]);
            STrimBy tf = MakeTrim(em.hEndC[k], c0, c1, ec->exact.Start());
            surface.FindById(em.hFillet)->trim.Add(&tf);
            if(em.kind[k] == END_FACE) {
                STrimBy tc = ReverseTrim(tf);
                surface.FindById(em.sC[k])->trim.Add(&tc);
            }
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
        if(IsDeadCurve(good, recuts, sc.h)) continue;
        kept.Add(&sc);
        sc.pts = {};
    }
    curve.Clear();
    kept.MoveSelfInto(&curve);
}

} // namespace SolveSpace
