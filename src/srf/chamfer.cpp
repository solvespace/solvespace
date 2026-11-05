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
// 4. Three modified edges (cube corner). Not miters: in each of its two
//    faces, e's offset crosses the neighbor's offset, and e's fillet ends at
//    the cross-section through those two points (same parameter at a square
//    corner). The three cross-sections bound the corner patch: a sphere
//    octant centered where the three fillet axes meet, or a flat triangle
//    for chamfers. The chamfer triangle is done (FromPlane, trimmed by the
//    three end curves, facing the same way as the chamfers). The sphere
//    comes later (see scope); until then a fillet corner is an open hole and
//    its curves name the fillet as both of their surfaces.
// Anything else (end face not perpendicular, etc.) is unsupported for now,
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
// 6. Three edges at a corner, with the hole left open (case 4).
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
    double    tSrf[2];        // [k] ends of the fillet surface, as params on e
    int       partner[2], partnerEnd[2]; // [k] TANGENT, MITER: the neighbor's EdgeMod
    // Geometry built before the shell is changed
    SBezier   newOff[2], endC[2];
    SSurface  fillet;
    // Handles assigned when the shell is changed
    hSCurve   hNew[2], hEndC[2];
    hSSurface hFillet;
};

// Three chamfers meeting at a corner (case 4), closed by a flat triangle.
// Fillet corners (sphere octant) are not done yet; their hole stays open.
struct Corner {
    Vector    vtx;
    int       n;
    int       mod[3], end[3]; // the EdgeMod ends that meet here
    SSurface  patch;
    hSSurface h;
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
    //   arc_test:           the arc at z = 10
    //   house_test:         lines between planes not at 90 degrees (eaves).
    //                       If a shell has any of these, only they are
    //                       picked; the rules above would also pick lines in
    //                       the house.
    const double r = 3.0;   // negative for chamfers
    if(!sc.isExact) return 0.0;
    const SBezier &b = sc.exact;
    for(SCurve &other : curve) {
        if(IsTestSlantedLine(this, &other)) {
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

// True if p is on curve b, within its [0, 1] range.
static bool PointWithinCurve(const SBezier &b, Vector p) {
    double t = ParamOn(b, p);
    return t > -LENGTH_EPS && t < 1 + LENGTH_EPS && b.PointAt(t).Equals(p);
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
    em->w = b0.w;
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
    Vector nc;
    double dc;
    if(!SurfaceIsPlane(sh->surface.FindById(hc), &nc, &dc)) {
        dbp("fillet: end face is not a plane");
        return false;
    }
    Vector tan = em->e.TangentAt((k == 0) ? 0.0 : 1.0).WithMagnitude(1.0);
    if(fabs(fabs(tan.Dot(nc)) - 1.0) > 1e-6) {
        dbp("fillet: end face not perpendicular to edge");
        return false;
    }
    if(!IntersectCarriers(em->off[0], f->exact, em->vtx[k], &em->cut[0][k]) ||
       !IntersectCarriers(em->off[1], g->exact, em->vtx[k], &em->cut[1][k]))
    {
        dbp("fillet: offset curve doesn't meet neighbor");
        return false;
    }
    if(!PointWithinCurve(f->exact, em->cut[0][k]) ||
       !PointWithinCurve(g->exact, em->cut[1][k]))
    {
        dbp("fillet: neighbor would need extending, not supported yet");
        return false;
    }
    // The end curve is a cross-section, so both cuts are at one parameter.
    double t0 = ParamOn(em->off[0], em->cut[0][k]),
           t1 = ParamOn(em->off[1], em->cut[1][k]);
    if(fabs(t0 - t1) > 1e-6) {
        dbp("fillet: cuts at different parameters");
        return false;
    }
    em->tSrf[k] = t0;
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
    if(!PointWithinCurve(g->exact, em->cut[o][k])) {
        dbp("fillet: offset doesn't land within the neighbor");
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
    if(em->e.deg != 1 || fm.e.deg != 1 ||
       !em->mid.Start().Equals(em->e.Start()) || !fm.mid.Start().Equals(fm.e.Start()))
    {
        dbp("fillet: miter only between flat faces so far");
        return false;
    }
    if(fabs(fm.r - em->r) > LENGTH_EPS || fm.chamfer != em->chamfer) {
        dbp("fillet: edges meeting at a corner must have the same radius");
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
    if(!IntersectCarriers(em->off[s], fm.off[fs], em->vtx[k], &em->cut[s][k]) ||
       !IntersectCarriers(em->off[o], g->exact, em->vtx[k], &em->cut[o][k]))
    {
        dbp("fillet: offset curves don't meet at the corner");
        return false;
    }
    if(!PointWithinCurve(g->exact, em->cut[o][k])) {
        dbp("fillet: neighbor would need extending, not supported yet");
        return false;
    }
    // The surface has to reach past both ends of the miter curve.
    double ts = ParamOn(em->off[s], em->cut[s][k]),
           to = ParamOn(em->off[o], em->cut[o][k]);
    em->tSrf[k]    = (k == 0) ? min(ts, to) : max(ts, to);
    em->kind[k]    = MITER;
    em->modSide[k] = s;
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
    for(int pk = 0; pk < 2; pk++) {
        const EdgeMod &pm = mods[j];
        if(pm.kind[pk] != em->kind[k] || !pm.vtx[pk].Equals(em->vtx[k])) {
            continue;
        }
        bool same    = pm.cut[0][pk].Equals(em->cut[0][k]) &&
                       pm.cut[1][pk].Equals(em->cut[1][k]),
             swapped = pm.cut[0][pk].Equals(em->cut[1][k]) &&
                       pm.cut[1][pk].Equals(em->cut[0][k]);
        if(!same && !swapped) return false;
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
        Vector mid = (em->kind[k] == MITER) ? em->vtx[k]
                                            : em->mid.PointAt(em->tSrf[k]);
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

// The flat triangle closing three chamfers at a corner. Its corners are the
// cut points of the chamfers; it faces the same way as the chamfers.
static bool MakeChamferCorner(std::vector<EdgeMod> &mods, Corner *c) {
    if(c->n != 3) return false;
    EdgeMod *em0 = &mods[c->mod[0]],
            *em1 = &mods[c->mod[1]];
    int k0 = c->end[0], k1 = c->end[1];
    Vector p0 = em0->cut[0][k0],
           p1 = em0->cut[1][k0],
           p2;
    if(!em1->cut[0][k1].Equals(p0) && !em1->cut[0][k1].Equals(p1)) {
        p2 = em1->cut[0][k1];
    } else {
        p2 = em1->cut[1][k1];
    }

    Vector mid = em0->endC[k0].PointAt(0.5);
    Vector nChamfer = SurfaceNormalNear(&em0->fillet, mid);
    SSurface srf = SSurface::FromPlane(p0, p1.Minus(p0), p2.Minus(p0));
    if(srf.NormalAt(0.25, 0.25).Dot(nChamfer) < 0) {
        srf = SSurface::FromPlane(p0, p2.Minus(p0), p1.Minus(p0));
    }
    srf.color = RGBi(100, 100, 100);
    c->patch = srf;
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

    // 5. Build all the new geometry. If anything fails, leave the shell as
    // it was. Unmodified neighbors are re-cut where the offsets meet them.
    std::vector<Recut> recuts;
    for(EdgeMod &em : mods) {
        for(int k = 0; k < 2; k++) {
            bool ok = true;
            for(int s = 0; s < 2; s++) {
                if(em.kind[k] == CORNER) continue;
                if(em.kind[k] != END_FACE && em.modSide[k] == s) continue;
                ok = ok && SetRecutEnd(&recuts, curve.FindById(em.nb[s][k]),
                                       em.vtx[k], em.cut[s][k]);
            }
            if(!ok) {
                dbp("fillet: neighbor cut twice at one end");
                return;
            }
            MakeEndCurve(&em, k);
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
            if(mods[i].kind[k] != CORNER || !mods[i].chamfer) continue;
            if(!AddToCorner(&corners, mods[i].vtx[k], (int)i, k)) {
                dbp("fillet: more than three edges at a corner");
                return;
            }
        }
    }
    for(Corner &c : corners) {
        if(!MakeChamferCorner(mods, &c)) {
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
        c.h = surface.AddAndAssignId(&c.patch);
        for(int i = 0; i < 3; i++) {
            mods[c.mod[i]].sC[c.end[i]] = c.h;
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
            if(em.kind[k] == END_FACE || (em.kind[k] == CORNER && em.sC[k].v)) {
                em.hEndC[k] = AddCurve(this, em.endC[k], em.sC[k], em.hFillet);
            } else if(em.kind[k] == CORNER) {
                // Edge of the open corner hole: it has only the fillet. Both
                // handles name the fillet, so code that looks up both
                // surfaces (Booleans of later groups) doesn't hit a null one.
                em.hEndC[k] = AddCurve(this, em.endC[k], em.hFillet, em.hFillet);
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
            Vector c0 = from0 ? em.cut[0][k] : em.cut[1][k],
                   c1 = from0 ? em.cut[1][k] : em.cut[0][k];
            SCurve *ec = curve.FindById(em.hEndC[k]);
            STrimBy tf = MakeTrim(em.hEndC[k], c0, c1, ec->exact.Start());
            surface.FindById(em.hFillet)->trim.Add(&tf);
            if(em.kind[k] == END_FACE || (em.kind[k] == CORNER && em.sC[k].v)) {
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
        if(IsDeadCurve(mods, recuts, sc.h)) continue;
        kept.Add(&sc);
        sc.pts = {};
    }
    curve.Clear();
    kept.MoveSelfInto(&curve);
}

} // namespace SolveSpace
