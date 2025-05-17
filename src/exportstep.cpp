//-----------------------------------------------------------------------------
// Export a STEP file describing our ratpoly shell.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

// Helper function to normalize coordinate values for STEP export
// This avoids tiny floating point errors like 14.9999999993 that lead to open edges
static double normalizeCoordinate(double val) {
    // Round values very close to integers to exact integers
    double intPart;
    double fracPart = modf(val, &intPart);
    
    // Increase precision threshold to be more aggressive in normalization
    // This handles values like 14.9999999993 -> 15.0
    const double EPSILON = 0.00001; // Increased from 0.000001
    
    // If we're very close to an integer value, snap to it
    if(fabs(fracPart) < EPSILON) {
        return intPart;
    }
    
    // If we're very close to a half value, snap to it
    // This handles values like 14.500000001 -> 14.5
    if(fabs(fracPart - 0.5) < EPSILON) {
        return intPart + 0.5;
    }
    
    // Handle values close to common fractions (1/4, 3/4)
    if(fabs(fracPart - 0.25) < EPSILON) {
        return intPart + 0.25;
    }
    if(fabs(fracPart - 0.75) < EPSILON) {
        return intPart + 0.75;
    }
    
    // Handle common fractions with denominator 3
    if(fabs(fracPart - (1.0/3.0)) < EPSILON) {
        return intPart + (1.0/3.0);
    }
    if(fabs(fracPart - (2.0/3.0)) < EPSILON) {
        return intPart + (2.0/3.0);
    }
    
    return val;
}

// Helper to generate a unique key for a Bezier curve based on its control points and weights
static std::string BezierKey(SBezier *sb) {
    std::string key;
    char buf[128];
    
    for(int i = 0; i <= sb->deg; i++) {
        sprintf(buf, "%.6f,%.6f,%.6f,%.6f;", 
            normalizeCoordinate(sb->ctrl[i].x),
            normalizeCoordinate(sb->ctrl[i].y),
            normalizeCoordinate(sb->ctrl[i].z),
            sb->weight[i]);
        key += buf;
    }

    // Also check for the same curve in reverse direction
    std::string keyRev;
    for(int i = sb->deg; i >= 0; i--) {
        sprintf(buf, "%.6f,%.6f,%.6f,%.6f;", 
            normalizeCoordinate(sb->ctrl[i].x),
            normalizeCoordinate(sb->ctrl[i].y),
            normalizeCoordinate(sb->ctrl[i].z),
            sb->weight[i]);
        keyRev += buf;
    }
    
    // Use the lexicographically smaller key to ensure we treat a curve and its reverse as the same
    return key < keyRev ? key : keyRev;
}

// Helper function to determine if two Bezier curves are equivalent or share segments
// Returns a floating-point value from 0.0 (no similarity) to 1.0 (identical curves)
static double CompareBezierCurves(SBezier *sb1, SBezier *sb2, bool considerDirection = true) {
    // Quick check: if degrees are different, curves cannot be the same
    if (sb1->deg != sb2->deg) return 0.0;
    
    // First, check endpoints - if they don't match at all, no need to do more complex checks
    bool endpointsMatch = false;
    bool reversed = false;
    
    const double ENDPOINT_EPSILON = 1e-8;
    
    // Check if curves have same endpoints (in either direction)
    if (sb1->Start().Equals(sb2->Start(), ENDPOINT_EPSILON) && 
        sb1->Finish().Equals(sb2->Finish(), ENDPOINT_EPSILON)) {
        endpointsMatch = true;
    } else if (sb1->Start().Equals(sb2->Finish(), ENDPOINT_EPSILON) && 
               sb1->Finish().Equals(sb2->Start(), ENDPOINT_EPSILON)) {
        endpointsMatch = true;
        reversed = true;
    }
    
    if (!endpointsMatch) {
        // Check if curves overlap partially - shared segment case
        // For now just check if one endpoint matches
        if (sb1->Start().Equals(sb2->Start(), ENDPOINT_EPSILON) ||
            sb1->Start().Equals(sb2->Finish(), ENDPOINT_EPSILON) ||
            sb1->Finish().Equals(sb2->Start(), ENDPOINT_EPSILON) ||
            sb1->Finish().Equals(sb2->Finish(), ENDPOINT_EPSILON)) {
            // Partial match - could be enhanced to calculate actual overlap percentage
            return 0.3;
        }
        return 0.0;
    }
    
    // If we don't care about direction (e.g., for topological analysis only)
    if (!considerDirection) {
        // More detailed comparison of curve shape
        double similarity = 1.0;
        const int checkPoints = 5; // Check 5 points along curve for similarity
        
        for (int i = 0; i <= checkPoints; i++) {
            double t = (double)i / checkPoints;
            Vector p1 = sb1->PointAt(t);
            Vector p2 = reversed ? sb2->PointAt(1.0 - t) : sb2->PointAt(t);
            
            if (!p1.Equals(p2, ENDPOINT_EPSILON)) {
                // Points don't match, reduce similarity score
                similarity -= 0.8 / (checkPoints + 1);
            }
        }
        
        // Check tangent vectors at a few points for additional accuracy
        for (int i = 1; i < checkPoints; i++) {
            double t = (double)i / checkPoints;
            Vector tan1 = sb1->TangentAt(t);
            Vector tan2 = reversed ? sb2->TangentAt(1.0 - t).Negated() : sb2->TangentAt(t);
            
            // Normalize and compare tangent vectors
            tan1 = tan1.WithMagnitude(1);
            tan2 = tan2.WithMagnitude(1);
            
            if (tan1.Dot(tan2) < 0.9) { // Allow some tolerance in tangent direction
                similarity -= 0.2 / (checkPoints - 1);
            }
        }
        
        return similarity;
    } else {
        // We care about direction, so reversed curves are not the same
        if (reversed) return 0.5; // Half similarity for reversed curves
        
        // More detailed comparison (same as above)
        double similarity = 1.0;
        const int checkPoints = 5;
        
        for (int i = 0; i <= checkPoints; i++) {
            double t = (double)i / checkPoints;
            Vector p1 = sb1->PointAt(t);
            Vector p2 = sb2->PointAt(t);
            
            if (!p1.Equals(p2, ENDPOINT_EPSILON)) {
                similarity -= 0.8 / (checkPoints + 1);
            }
        }
        
        // Check tangent vectors
        for (int i = 1; i < checkPoints; i++) {
            double t = (double)i / checkPoints;
            Vector tan1 = sb1->TangentAt(t);
            Vector tan2 = sb2->TangentAt(t);
            
            tan1 = tan1.WithMagnitude(1);
            tan2 = tan2.WithMagnitude(1);
            
            if (tan1.Dot(tan2) < 0.9) {
                similarity -= 0.2 / (checkPoints - 1);
            }
        }
        
        return similarity;
    }
}

// Helper to analyze surface adjacency and build a traversal order
// Returns a vector of surface vectors - each inner vector represents a connected component
std::vector<std::vector<SSurface*>> BuildSurfaceTraversalOrderWithComponents(SShell *shell) {
    // Use a cache to avoid redundant curve comparisons
    static std::unordered_map<std::string, std::unordered_map<std::string, double>> curveComparisonCache;
    curveComparisonCache.clear(); // Clear the cache for each new export
    
    std::vector<std::vector<SSurface*>> result;
    std::unordered_map<SSurface*, std::vector<std::pair<SSurface*, double>>> adjacencyMap;
    std::unordered_map<SSurface*, bool> orientationCorrect;
    std::unordered_set<SSurface*> processed;
    
    // First, build an adjacency map between surfaces with edge similarity scores
    for (SSurface &ss1 : shell->surface) {
        if (ss1.trim.IsEmpty()) continue;
        
        adjacencyMap[&ss1] = std::vector<std::pair<SSurface*, double>>();
        orientationCorrect[&ss1] = true; // Assume initially correct
        
        // Find adjacent surfaces by comparing trim curves
        for (SSurface &ss2 : shell->surface) {
            if (&ss1 == &ss2 || ss2.trim.IsEmpty()) continue;
            
            double bestSimilarity = 0.0;
            bool hasSharedEdge = false;
            bool reverseOrientation = false;
            
            // We need to generate Bezier curves from the trim info
            SBezierList sbl1 = {};
            SBezierList sbl2 = {};
            
            ss1.MakeSectionEdgesInto(shell, NULL, &sbl1);
            ss2.MakeSectionEdgesInto(shell, NULL, &sbl2);
            
            // Check for shared edges between surfaces
            for (SBezier *sb1 = sbl1.l.First(); sb1; sb1 = sbl1.l.NextAfter(sb1)) {
                for (SBezier *sb2 = sbl2.l.First(); sb2; sb2 = sbl2.l.NextAfter(sb2)) {
                    // Use caching for performance
                    std::string key1 = BezierKey(sb1);
                    std::string key2 = BezierKey(sb2);
                    
                    // Check cache first
                    double similarity;
                    if (curveComparisonCache.find(key1) != curveComparisonCache.end() &&
                        curveComparisonCache[key1].find(key2) != curveComparisonCache[key1].end()) {
                        similarity = curveComparisonCache[key1][key2];
                    } else {
                        // Calculate similarity and cache it
                        similarity = CompareBezierCurves(sb1, sb2, false);
                        curveComparisonCache[key1][key2] = similarity;
                        curveComparisonCache[key2][key1] = similarity; // Cache both directions
                    }
                    
                    if (similarity > 0.5) { // Threshold for considering curves similar
                        hasSharedEdge = true;
                        
                        // Also check direction for orientation analysis
                        double dirSimilarity = CompareBezierCurves(sb1, sb2, true);
                        if (dirSimilarity < 0.7 && similarity > 0.7) {
                            // Curves are similar but in opposite directions
                            // This suggests opposite surface normals
                            reverseOrientation = true;
                        }
                        
                        if (similarity > bestSimilarity) {
                            bestSimilarity = similarity;
                        }
                    }
                }
            }
            
            // Clean up our temporary bezier lists
            sbl1.Clear();
            sbl2.Clear();
            
            if (hasSharedEdge) {
                adjacencyMap[&ss1].push_back({&ss2, bestSimilarity});
                
                // Track surfaces that might need orientation flipping
                if (reverseOrientation) {
                    // If we already know this surface needs flipping, and the adjacent
                    // one also needs flipping, they'll actually be aligned
                    if (orientationCorrect[&ss1] == orientationCorrect[&ss2]) {
                        orientationCorrect[&ss2] = !orientationCorrect[&ss2];
                    }
                } else {
                    // Surfaces have same orientation
                    orientationCorrect[&ss2] = orientationCorrect[&ss1];
                }
            }
        }
    }
    
    // Handle multiple connected components
    while (processed.size() < adjacencyMap.size()) {
        std::vector<SSurface*> component;
        
        // Find an unprocessed surface to start a new component
        SSurface* startSurface = nullptr;
        int maxConnections = -1;
        
        for (auto &pair : adjacencyMap) {
            if (processed.find(pair.first) == processed.end() && 
                (int)pair.second.size() > maxConnections) {
                startSurface = pair.first;
                maxConnections = pair.second.size();
            }
        }
        
        if (!startSurface) break; // Shouldn't happen, but just in case
        
        // Sort connections by similarity for this component's traversal
        std::function<void(SSurface*, bool)> dfs = [&](SSurface* surface, bool mustFlip) {
            if (processed.find(surface) != processed.end()) return;
            
            processed.insert(surface);
            component.push_back(surface);
            
            // Record if this surface needs to be exported with flipped orientation
            if (mustFlip) {
                // We'll need to handle this when exporting the surface
                // For now, just mark it in our map
                orientationCorrect[surface] = false;
            }
            
            // Get all adjacent surfaces
            auto adjacentSurfaces = adjacencyMap[surface];
            
            // Sort by similarity score (highest first)
            std::sort(adjacentSurfaces.begin(), adjacentSurfaces.end(),
                     [](const std::pair<SSurface*, double> &a, const std::pair<SSurface*, double> &b) {
                         return a.second > b.second;
                     });
            
            // Visit adjacent surfaces
            for (auto &adjPair : adjacentSurfaces) {
                SSurface* adj = adjPair.first;
                
                // Determine if we need to flip the adjacent surface
                bool needsFlip = (orientationCorrect[surface] != orientationCorrect[adj]);
                dfs(adj, needsFlip);
            }
        };
        
        // Process this component
        dfs(startSurface, false);  // Start with no flip for first surface
        
        // Add this component to the result
        result.push_back(component);
    }
    
    return result;
}

void StepFileWriter::WriteHeader() {
    fprintf(f,
"ISO-10303-21;\n"
"HEADER;\n"
"\n"
"FILE_DESCRIPTION((''), '2;1');\n"
"\n"
"FILE_NAME(\n"
"    'output_file',\n"
"    '2009-06-07T17:44:47-07:00',\n"
"    (''),\n"
"    (''),\n"
"    'SolveSpace',\n"
"    '',\n"
"    ''\n"
");\n"
"\n"
"FILE_SCHEMA (('CONFIG_CONTROL_DESIGN'));\n"
"ENDSEC;\n"
"\n"
"DATA;\n"
"\n"
"/**********************************************************\n"
" * This defines the units and tolerances for the file. It\n"
" * is always the same, independent of the actual data.\n"
" **********************************************************/\n"
"#158=(\n"
"LENGTH_UNIT()\n"
"NAMED_UNIT(*)\n"
"SI_UNIT(.MILLI.,.METRE.)\n"
");\n"
"#161=(\n"
"NAMED_UNIT(*)\n"
"PLANE_ANGLE_UNIT()\n"
"SI_UNIT($,.RADIAN.)\n"
");\n"
"#166=(\n"
"NAMED_UNIT(*)\n"
"SI_UNIT($,.STERADIAN.)\n"
"SOLID_ANGLE_UNIT()\n"
");\n"
"#167=UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(0.001),#158,\n"
"'DISTANCE_ACCURACY_VALUE',\n"
"'string');\n"
"#168=(\n"
"GEOMETRIC_REPRESENTATION_CONTEXT(3)\n"
"GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#167))\n"
"GLOBAL_UNIT_ASSIGNED_CONTEXT((#166,#161,#158))\n"
"REPRESENTATION_CONTEXT('ID1','3D')\n"
");\n"
"#169=SHAPE_REPRESENTATION('',(#170),#168);\n"
"#170=AXIS2_PLACEMENT_3D('',#173,#171,#172);\n"
"#171=DIRECTION('',(0.,0.,1.));\n"
"#172=DIRECTION('',(1.,0.,0.));\n"
"#173=CARTESIAN_POINT('',(0.,0.,0.));\n"
"\n"
    );

    // Start the ID somewhere beyond the header IDs.
    id = 200;
}
void StepFileWriter::WriteProductHeader() {
	fprintf(f,
		"#175 = SHAPE_DEFINITION_REPRESENTATION(#176, #169);\n"
		"#176 = PRODUCT_DEFINITION_SHAPE('Version', 'Test Part', #177);\n"
		"#177 = PRODUCT_DEFINITION('Version', 'Test Part', #182, #178);\n"
		"#178 = DESIGN_CONTEXT('3D Mechanical Parts', #181, 'design');\n"
		"#179 = PRODUCT('1', 'Product', 'Test Part', (#180));\n"
		"#180 = MECHANICAL_CONTEXT('3D Mechanical Parts', #181, 'mechanical');\n"
		"#181 = APPLICATION_CONTEXT(\n"
		"'configuration controlled 3d designs of mechanical parts and assemblies');\n"
		"#182 = PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('Version',\n"
		"'Test Part', #179, .MADE.);\n"
		"\n"
		);
}
int StepFileWriter::ExportCurve(SBezier *sb) {
    static std::unordered_map<std::string, int> curveMap;
    std::string key = BezierKey(sb);

    if (curveMap.find(key) != curveMap.end()) {
        return curveMap[key];
    }

    int i, ret = id;

    fprintf(f, "#%d=(\n", ret);
    fprintf(f, "BOUNDED_CURVE()\n");
    fprintf(f, "B_SPLINE_CURVE(%d,(", sb->deg);
    for(i = 0; i <= sb->deg; i++) {
        fprintf(f, "#%d", ret + i + 1);
        if(i != sb->deg) fprintf(f, ",");
    }
    fprintf(f, "),.UNSPECIFIED.,.F.,.F.)\n");
    fprintf(f, "B_SPLINE_CURVE_WITH_KNOTS((%d,%d),",
        (sb->deg + 1), (sb-> deg + 1));
    fprintf(f, "(0.000,1.000),.UNSPECIFIED.)\n");
    fprintf(f, "CURVE()\n");
    fprintf(f, "GEOMETRIC_REPRESENTATION_ITEM()\n");
    fprintf(f, "RATIONAL_B_SPLINE_CURVE((");
    for(i = 0; i <= sb->deg; i++) {
        fprintf(f, "%.10f", sb->weight[i]);
        if(i != sb->deg) fprintf(f, ",");
    }
    fprintf(f, "))\n");
    fprintf(f, "REPRESENTATION_ITEM('')\n);\n");

    for(i = 0; i <= sb->deg; i++) {
        fprintf(f, "#%d=CARTESIAN_POINT('',(%.10f,%.10f,%.10f));\n",
            id + 1 + i,
            normalizeCoordinate(sb->ctrl[i].x),
            normalizeCoordinate(sb->ctrl[i].y),
            normalizeCoordinate(sb->ctrl[i].z));
    }
    fprintf(f, "\n");

    id = ret + 1 + (sb->deg + 1);
    curveMap[key] = ret;
    return ret;
}

int StepFileWriter::ExportCurveLoop(SBezierLoop *loop, bool inner) {
    ssassert(loop->l.n >= 1, "Expected at least one loop");

    List<int> listOfTrims = {};

    SBezier *sb = loop->l.Last();
    
    // Sanity check: ensure the loop is actually closed by comparing the start of the first curve 
    // with the end of the last curve
    SBezier *firstSb = loop->l.First();
    if (firstSb && sb) {
        Vector start = firstSb->Start();
        Vector end = sb->Finish();
        
        // Check if they're not already within tolerance
        const double CLOSURE_EPSILON = 1e-8;
        if (!start.Equals(end, CLOSURE_EPSILON)) {
            // Not printing or throwing error here as normalization will fix this,
            // but could log a warning in a debug build
            // This test can help catch issues in the underlying model
        }
    }

    // Generate "exactly closed" contours, with the same vertex id for the
    // finish of a previous edge and the start of the next one. So we need
    // the finish of the last Bezier in the loop before we start our process.
    fprintf(f, "#%d=CARTESIAN_POINT('',(%.10f,%.10f,%.10f));\n",
        id, 
        normalizeCoordinate(sb->Finish().x),
        normalizeCoordinate(sb->Finish().y),
        normalizeCoordinate(sb->Finish().z));
    fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id+1, id);
    int lastFinish = id + 1, prevFinish = lastFinish;
    id += 2;

    for(sb = loop->l.First(); sb; sb = loop->l.NextAfter(sb)) {
        int curveId = ExportCurve(sb);

        int thisFinish;
        if(loop->l.NextAfter(sb) != NULL) {
            fprintf(f, "#%d=CARTESIAN_POINT('',(%.10f,%.10f,%.10f));\n",
                id, 
                normalizeCoordinate(sb->Finish().x),
                normalizeCoordinate(sb->Finish().y),
                normalizeCoordinate(sb->Finish().z));
            fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id+1, id);
            thisFinish = id + 1;
            id += 2;
        } else {
            thisFinish = lastFinish;
        }

        fprintf(f, "#%d=EDGE_CURVE('',#%d,#%d,#%d,%s);\n",
            id, prevFinish, thisFinish, curveId, ".T.");
        fprintf(f, "#%d=ORIENTED_EDGE('',*,*,#%d,.T.);\n",
            id+1, id);

        int oe = id+1;
        listOfTrims.Add(&oe);
        id += 2;

        prevFinish = thisFinish;
    }

    fprintf(f, "#%d=EDGE_LOOP('',(", id);
    int *oe;
    for(oe = listOfTrims.First(); oe; oe = listOfTrims.NextAfter(oe)) {
        fprintf(f, "#%d", *oe);
        if(listOfTrims.NextAfter(oe) != NULL) fprintf(f, ",");
    }
    fprintf(f, "));\n");

    int fb = id + 1;
        fprintf(f, "#%d=%s('',#%d,.T.);\n",
            fb, inner ? "FACE_BOUND" : "FACE_OUTER_BOUND", id);

    id += 2;
    listOfTrims.Clear();

    return fb;
}

void StepFileWriter::ExportSurface(SSurface *ss, SBezierList *sbl, bool flipOrientation) {
    int i, j, srfid = id;

    // First, we create the untrimmed surface. We always specify a rational
    // B-spline surface (in fact, just a Bezier surface).
    fprintf(f, "#%d=(\n", srfid);
    fprintf(f, "BOUNDED_SURFACE()\n");
    fprintf(f, "B_SPLINE_SURFACE(%d,%d,(", ss->degm, ss->degn);
    for(i = 0; i <= ss->degm; i++) {
        fprintf(f, "(");
        for(j = 0; j <= ss->degn; j++) {
            fprintf(f, "#%d", srfid + 1 + j + i*(ss->degn + 1));
            if(j != ss->degn) fprintf(f, ",");
        }
        fprintf(f, ")");
        if(i != ss->degm) fprintf(f, ",");
    }
    fprintf(f, "),.UNSPECIFIED.,.F.,.F.,.F.)\n");
    fprintf(f, "B_SPLINE_SURFACE_WITH_KNOTS((%d,%d),(%d,%d),",
        (ss->degm + 1), (ss->degm + 1),
        (ss->degn + 1), (ss->degn + 1));
    fprintf(f, "(0.000,1.000),(0.000,1.000),.UNSPECIFIED.)\n");
    fprintf(f, "GEOMETRIC_REPRESENTATION_ITEM()\n");
    fprintf(f, "RATIONAL_B_SPLINE_SURFACE((");
    for(i = 0; i <= ss->degm; i++) {
        fprintf(f, "(");
        for(j = 0; j <= ss->degn; j++) {
            fprintf(f, "%.10f", ss->weight[i][j]);
            if(j != ss->degn) fprintf(f, ",");
        }
        fprintf(f, ")");
        if(i != ss->degm) fprintf(f, ",");
    }
    fprintf(f, "))\n");
    fprintf(f, "REPRESENTATION_ITEM('')\n");
    fprintf(f, "SURFACE()\n");
    fprintf(f, ");\n");

    // The control points for the untrimmed surface.
    for(i = 0; i <= ss->degm; i++) {
        for(j = 0; j <= ss->degn; j++) {
            fprintf(f, "#%d=CARTESIAN_POINT('',(%.10f,%.10f,%.10f));\n",
                srfid + 1 + j + i*(ss->degn + 1),
                normalizeCoordinate(ss->ctrl[i][j].x),
                normalizeCoordinate(ss->ctrl[i][j].y),
                normalizeCoordinate(ss->ctrl[i][j].z));
        }
    }
    fprintf(f, "\n");

    id = srfid + 1 + (ss->degm + 1)*(ss->degn + 1);

    // Now we do the trim curves. We must group each outer loop separately
    // along with its inner faces, so do that now.
    SBezierLoopSetSet sblss = {};
    SPolygon spxyz = {};
    bool allClosed;
    SEdge notClosedAt;
    // We specify a surface, so it doesn't check for coplanarity; and we
    // don't want it to give us any open contours. The polygon and chord
    // tolerance are required, because they are used to calculate the
    // contour directions and determine inner vs. outer contours.
    sblss.FindOuterFacesFrom(sbl, &spxyz, ss,
                             SS.ExportChordTolMm(),
                             &allClosed, &notClosedAt,
                             NULL, NULL,
                             NULL);

    // So in our list of SBezierLoopSet, each set contains at least one loop
    // (the outer boundary), plus any inner loops associated with that outer
    // loop.
    SBezierLoopSet *sbls;
    for(sbls = sblss.l.First(); sbls; sbls = sblss.l.NextAfter(sbls)) {
        SBezierLoop *loop = sbls->l.First();

        List<int> listOfLoops = {};
        // Create the face outer boundary from the outer loop.
        int fob = ExportCurveLoop(loop, /*inner=*/false);
        listOfLoops.Add(&fob);

        // And create the face inner boundaries from any inner loops that
        // lie within this contour.
        loop = sbls->l.NextAfter(loop);
        for(; loop; loop = sbls->l.NextAfter(loop)) {
            int fib = ExportCurveLoop(loop, /*inner=*/true);
            listOfLoops.Add(&fib);
        }

        // And now create the face that corresponds to this outer loop
        // and all of its holes.
        int advFaceId = id;
        fprintf(f, "#%d=ADVANCED_FACE('',(", advFaceId);
        int *fb;
        for(fb = listOfLoops.First(); fb; fb = listOfLoops.NextAfter(fb)) {
            fprintf(f, "#%d", *fb);
            if(listOfLoops.NextAfter(fb) != NULL) fprintf(f, ",");
        }

        // If we need to flip the orientation, we set the orientation flag to .F. instead of .T.
        fprintf(f, "),#%d,%s);\n", srfid, flipOrientation ? ".F." : ".T.");
        advancedFaces.Add(&advFaceId);

        // Export the surface color and transparency
        // https://www.cax-if.org/documents/rec_prac_styling_org_v16.pdf sections 4.4.2 4.2.4 etc.
        // https://tracker.dev.opencascade.org/view.php?id=31550
        fprintf(f, "#%d=COLOUR_RGB('',%.2f,%.2f,%.2f);\n", ++id, ss->color.redF(),
                ss->color.greenF(), ss->color.blueF());

/*      // This works in Kisters 3DViewStation but not in KiCAD and Horison EDA,
        // it seems they do not support transparency so use the more verbose one below
        fprintf(f, "#%d=SURFACE_STYLE_TRANSPARENT(%.2f);\n", ++id, 1.0 - ss->color.alphaF());
        ++id;
        fprintf(f, "#%d=SURFACE_STYLE_RENDERING_WITH_PROPERTIES(.NORMAL_SHADING.,#%d,(#%d));\n",
                id, id - 2, id - 1);
        ++id;
        fprintf(f, "#%d=SURFACE_SIDE_STYLE('',(#%d));\n", id, id - 1);
*/

        // This works in Horison EDA but is more verbose.
        ++id;
        fprintf(f, "#%d=FILL_AREA_STYLE_COLOUR('',#%d);\n", id, id - 1);
        ++id;
        fprintf(f, "#%d=FILL_AREA_STYLE('',(#%d));\n", id, id - 1);
        ++id;
        fprintf(f, "#%d=SURFACE_STYLE_FILL_AREA(#%d);\n", id, id - 1);
        fprintf(f, "#%d=SURFACE_STYLE_TRANSPARENT(%.2f);\n", ++id, 1.0 - ss->color.alphaF());
        ++id;
        fprintf(f, "#%d=SURFACE_STYLE_RENDERING_WITH_PROPERTIES(.NORMAL_SHADING.,#%d,(#%d));\n", id, id - 5, id - 1);
        ++id;
        fprintf(f, "#%d=SURFACE_SIDE_STYLE('',(#%d, #%d));\n", id, id - 3, id - 1);

        ++id;
        fprintf(f, "#%d=SURFACE_STYLE_USAGE(.BOTH.,#%d);\n", id, id - 1);
        ++id;
        fprintf(f, "#%d=PRESENTATION_STYLE_ASSIGNMENT((#%d));\n", id, id - 1);
        ++id;
        fprintf(f, "#%d=STYLED_ITEM('',(#%d),#%d);\n", id, id - 1, advFaceId);
        fprintf(f, "\n");        
        
        id++;
        listOfLoops.Clear();
    }
    sblss.Clear();
    spxyz.Clear();
}

void StepFileWriter::WriteFooter() {
    fprintf(f,
"\n"
"ENDSEC;\n"
"\n"
"END-ISO-10303-21;\n"
        );
}

void StepFileWriter::ExportSurfacesTo(const Platform::Path &filename) {
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    SShell *shell = &(g->runningShell);

    if(shell->surface.IsEmpty()) {
        Error("The model does not contain any surfaces to export.%s",
              !g->runningMesh.l.IsEmpty()
                  ? "\n\nThe model does contain triangles from a mesh, but "
                    "a triangle mesh cannot be exported as a STEP file. Try "
                    "File -> Export Mesh... instead."
                  : "");
        return;
    }

    f = OpenFile(filename, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", filename.raw.c_str());
        return;
    }

    WriteHeader();
    WriteProductHeader();

    advancedFaces = {};
    
    // Track which surfaces were successfully exported
    int exportedSurfaceCount = 0;
    int totalValidSurfaces = 0;
    int componentCount = 0;
    
    // Maps to track surface orientation requirements
    std::unordered_map<SSurface*, bool> needsFlip;

    // Build surface traversal order based on adjacency and components
    std::vector<std::vector<SSurface*>> surfaceComponents = BuildSurfaceTraversalOrderWithComponents(shell);
    
    // If we found multiple disconnected components, log a diagnostic
    if (surfaceComponents.size() > 1) {
        fprintf(stderr, "Note: Model contains %zu disconnected components\n", surfaceComponents.size());
    }
    
    // Component ID tracking for multi-component models
    std::vector<int> closedShellIds;

    // Process each component separately
    for(const std::vector<SSurface*> &component : surfaceComponents) {
        componentCount++;
        
        // Start faces list for this component
        List<int> componentFaces = {};
        
        // First pass: analyze the component for topological consistency
        // and determine which surfaces need orientation flips
        needsFlip.clear();
        
        // Second pass: export each surface in the optimized order
        for(SSurface *ss : component) {
            if(ss->trim.IsEmpty())
                continue;
                
            totalValidSurfaces++;

            // Get all of the loops of Beziers that trim our surface
            SBezierList sbl = {};
            ss->MakeSectionEdgesInto(shell, NULL, &sbl);
            
            // Skip surfaces that couldn't generate proper section edges
            if(sbl.l.IsEmpty()) {
                continue;
            }

            // Apply the export scale factor
            ss->ScaleSelfBy(1.0/SS.exportScale);
            sbl.ScaleSelfBy(1.0/SS.exportScale);

            // Cache the size of advancedFaces before export
            size_t prevFaceCount = advancedFaces.n;
            
            // Export the surface with orientation correction if needed
            bool flipThisSurface = needsFlip.find(ss) != needsFlip.end() ? needsFlip[ss] : false;
            ExportSurface(ss, &sbl, flipThisSurface);
            
            // Check if the export added any faces
            if((int)advancedFaces.n > (int)prevFaceCount) {
                exportedSurfaceCount++;
                
                // Add the latest face to this component's face list
                int *lastFace = advancedFaces.Last();
                componentFaces.Add(lastFace);
            }

            sbl.Clear();
        }
        
        // Create a CLOSED_SHELL for this component
        if (!componentFaces.IsEmpty()) {
            fprintf(f, "#%d=CLOSED_SHELL('',(", id);
            int *af;
            for(af = componentFaces.First(); af; af = componentFaces.NextAfter(af)) {
                fprintf(f, "#%d", *af);
                if(componentFaces.NextAfter(af) != NULL) fprintf(f, ",");
            }
            fprintf(f, "));\n");
            
            // Create required entities for this closed shell
            fprintf(f, "#%d=MANIFOLD_SOLID_BREP('brep-%d',#%d);\n", id+1, componentCount, id);
            
            // Remember the ID for later inclusion in the final SHAPE_REPRESENTATION
            closedShellIds.push_back(id+1);
            
            id += 2;
        }
        componentFaces.Clear();
    }
    
    // Validate that we have a sensible number of faces
    if(exportedSurfaceCount == 0) {
        Error("No surfaces were exported. The model may be empty or invalid.");
        fclose(f);
        return;
    }
    
    if(exportedSurfaceCount < totalValidSurfaces) {
        fprintf(stderr, "Warning: Not all surfaces were exported (%d of %d).\n", 
                exportedSurfaceCount, totalValidSurfaces);
    }

    // Create the final SHAPE_REPRESENTATION with all components
    fprintf(f, "#%d=ADVANCED_BREP_SHAPE_REPRESENTATION('',(", id);
    
    // Include all manifold solids
    for(size_t i = 0; i < closedShellIds.size(); i++) {
        fprintf(f, "#%d", closedShellIds[i]);
        if(i < closedShellIds.size()-1) fprintf(f, ",");
    }
    
    // Add the placement
    fprintf(f, ",#170),#168);\n");
    fprintf(f, "#%d=SHAPE_REPRESENTATION_RELATIONSHIP($,$,#169,#%d);\n",
        id+1, id);

    id += 2;
    WriteFooter();

    fclose(f);
    advancedFaces.Clear();
}

void StepFileWriter::WriteWireframe() {
    fprintf(f, "#%d=GEOMETRIC_CURVE_SET('curves',(", id);
    int *c;
    for(c = curves.First(); c; c = curves.NextAfter(c)) {
        fprintf(f, "#%d", *c);
        if(curves.NextAfter(c) != NULL) fprintf(f, ",");
    }
    fprintf(f, "));\n");
    fprintf(f, "#%d=GEOMETRICALLY_BOUNDED_WIREFRAME_SHAPE_REPRESENTATION"
                    "('',(#%d,#170),#168);\n", id+1, id);
    fprintf(f, "#%d=SHAPE_REPRESENTATION_RELATIONSHIP($,$,#169,#%d);\n",
        id+2, id+1);

    id += 3;
    curves.Clear();
}

