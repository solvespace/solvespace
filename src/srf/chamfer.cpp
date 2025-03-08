//-----------------------------------------------------------------------------
// Algorithm for adding fillets and chamfers to an existing NURBS shell
// To create these features, edges are split into 2 new edges joined by
// a new surface which can be flat (chamfer) or round (fillet).
// Old: surfA, edge, surfB
// New: surfA, edge1, surfC, edge2, surfB
//
// Copyright 2024 Paul H Kahler.
//-----------------------------------------------------------------------------
#include "../solvespace.h"

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

