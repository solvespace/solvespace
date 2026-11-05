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

typedef struct {
    hSCurve     hc;   // handle of the original curve
    hSSurface   hs;   // handle of the new surface
    
} fillet_info;


// We return 0 for no feature, radius for a fillet, and for chamfers a negative
// value whose absolute value is the radius of an equivalent fillet.
double SShell::GetEdgeModifier(SCurve &sc)
{
  // for testing we will hard code stuff like returning a radius for only degree 2 curves.
  // once things work we'll topo-name curves with entity handles and use that to get info.
  if (!sc.isExact) return 0.0;
  // chamfer any curve up high
  if ((sc.exact.deg > 1) && (sc.exact.ctrl[0].z > 9.0)) return -5.0;
  
  return 0.0;
}

void SShell::ModifyEdges()
{
    dbp("checking edges");
    // Loop over all curves and create an info struct for the ones to modify
    // we will call GetEdgeModifier() to get a radius or 0.
    for(int i=0; i<curve.n; i++) {
        SCurve *sc = &curve[i];
        double m = GetEdgeModifier(*sc);
        bool chamfer = false;
      
        if (abs(m) > 0.000001)
        {
            dbp("modify an edge");
            if (m < 0.0) {
                chamfer = true;
                m = -m;
            }
        
        
	// create a new surface C, for the fillet or chamfer
	// - for now this surface has zero area because the trims are identical
            SSurface srfC = {};
            srfC.degm = sc->exact.deg;
            if (chamfer)
              srfC.degn = 1;
            else
              srfC.degn = 2;

            srfC.degn = 3;  // we're doing something else for now

//            srf.color = ???? what to put here;
            hSSurface hsrfC = surface.AddAndAssignId(&srfC);


        // create 2 new curves/trims as copies of the original. Tag the original curve.
        // for trims we can just modify existing and add a new one.
            Vector zeroVec = {0.0, 0.0, 0.0};
            // create curve A joining surface A with surface C
            SCurve curveA = {};
            curveA.isExact = true;
            curveA.exact = sc->exact.TransformedBy(zeroVec, Quaternion::IDENTITY, 1.0);
            // copy would be better, but we need to redo this later anyway.
            (curveA.exact).MakePwlInto(&(curveA.pts));
            // one of these needs to be the new surface
            curveA.surfA = sc->surfA;
            curveA.surfB = hsrfC;
            hSCurve hcurveA = curve.AddAndAssignId(&curveA);

            // create curve B joining surface B with surface C
            SCurve curveB = {};
            curveB.isExact = true;
            curveB.exact = sc->exact.TransformedBy(zeroVec, Quaternion::IDENTITY, 1.0);
            // copy would be better, but we need to redo this later anyway.
            (curveB.exact).MakePwlInto(&(curveB.pts));
            // one of these needs to be the new surface
            curveB.surfA = hsrfC;
            curveB.surfB = sc->surfB;
            hSCurve hcurveB = curve.AddAndAssignId(&curveB);

  
        }
    }
	// Delete the original curves (we can probably leave the curve and modify the trim(s))

	// For each new curve, find the one that matches our start and end point and
	// shares surfA or B. these are either original unmodified curves or new feature curves
	// either works, as this is what our new curve will intersect


	// Offset all the new curves within their A or B surface

	// Find the intersection points of all our new curves and their neighbors (start and end)

	// Reparameterize our curves and extend our surface if needed (or can U,V go past [0,1]?)

	// Create new trim curves at the ends of our new surface where it meets the others
	// when we get to corners with 3 fillets we will need a spherical surface to join (hard).

	// Delete our info structures
}

} // namespace SolveSpace
