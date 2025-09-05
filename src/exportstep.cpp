//-----------------------------------------------------------------------------
// Export a STEP file describing our ratpoly shell.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#define FP "%.5f"   // Floating Point coordinate precision. Since LENGTH_EPS = 1e-6 output 5 decimal places thus rounding out errors e.g. 0.999999mm
#define CARTESIAN_POINT_FORMAT "#%d=CARTESIAN_POINT('',(" FP "," FP "," FP "));\n"
const double PRECISION = 2*LENGTH_EPS;

//-----------------------------------------------------------------------------
// Functions for STEP export: duplication check.
//-----------------------------------------------------------------------------
// Check if this point was already defined with a different ID number.
// inputs:
//        number -> id of the cartesian point
//        v -> position of the cartesian point
//        vertex -> id of the vertex linked to this point (<1 if none)
// return:
//        true, if the cartesian point is already defined
bool StepFileWriter::HasCartesianPointAnAlias(int number, Vector v, int vertex, bool *vertex_has_alias) {
    // Look for this point "v" in the alias list.
    for(pointAliases_t &p : pointAliases) {
        if(p.v.Equals(v, PRECISION)) {
            // This point was already defined.
            // The new number is just an alias.
            p.alias.aliases.push_back(number);
            if(vertex > 0) {
                p.vertexAlias.reference = (p.vertexAlias.reference <= 0 ?
                                            vertex : p.vertexAlias.reference);
                p.vertexAlias.aliases.push_back(vertex);

                // If the caller is interested let them know if the vertex is new or has an existing alias.
                if(nullptr != vertex_has_alias) {
                    if(p.vertexAlias.aliases.size() == 1) {
                        // This is a new vertex.
                        *vertex_has_alias = false;
                    } else {
                        *vertex_has_alias = true;
                    }
                }
            }
            return true;
        }
    }

    // No point was found, it means this is a new point.
    pointAliases_t newPoint;
    newPoint.alias.reference = number;
    newPoint.alias.aliases.push_back(number);
    newPoint.v = v;
    newPoint.vertexAlias.reference = vertex;
    if(vertex > 0) {
        newPoint.vertexAlias.aliases.push_back(vertex);
    }

    // A new entry in the list.
    pointAliases.push_back(newPoint);
    return false;
}

// Return a cartesian point index; if the point has aliases return the
// first one.
// input:
//        number -> the id of the requested cartesian point
// return:
//        number, if the point has no aliases, otherwise its first alias
int StepFileWriter::InsertPoint(int number) {
    // Look for a point with index "number" in the list of aliases.
    for(pointAliases_t p : pointAliases) {
        for(int alias : p.alias.aliases) {
            if(alias == number) {
                return p.alias.reference;
            }
        }
    }

    // ERROR: it should never reach this point...
    return -1;
}

// Return a vertex index; if the vertex has aliases return the
// first one.
// input:
//        number -> the id of the requested vertex
// return:
//        number, if the vertex has no aliases, otherwise its first alias
int StepFileWriter::InsertVertex(int number) {
    // Look for a point with index "number" in the list of vertex aliases.
    for(pointAliases_t p : pointAliases) {
        for(int alias : p.vertexAlias.aliases) {
            if(alias == number) {
                return p.vertexAlias.reference;
            }
        }
    }

    // ERROR: it should never reach this point...
    return -1;
}

// Check whether this curve was already defined with a different ID number.
// inputs:
//        number -> id of the curve
//        points -> list of points that define the curve
// return:
//        true, if the curve is already defined
bool StepFileWriter::HasBSplineCurveAnAlias(int number, std::vector<int> points) {
    // Look for this curve in the alias list.
    for(curveAliases_t &c : curveAliases) {
        if(points.size() != c.memberPoints.size()) {
            continue;
        } if(exportParts && !(c.color.Equals(currentColor))) {
          continue;
        } else {
            size_t matches = 0; // is this the same curve?
            // FIXME: this hack should work _most_ of the times
            for(size_t i = 0; i < points.size(); i++) {
                for(size_t j = 0; j < points.size(); j++) {
                    if(points[i] == c.memberPoints[j]) {
                        matches++;
                    }
                }
            }

            if(matches == points.size()) {
                // Add this alias.
                c.alias.aliases.push_back(number);
                return true;
            }
        }
    }

    // No curve was found, it means this is a new curve.
    curveAliases_t newCurve;
    newCurve.alias.reference = number;
    newCurve.alias.aliases.push_back(number);
    newCurve.memberPoints = points;
    newCurve.color = currentColor;

    // A new entry in the list.
    curveAliases.push_back(newCurve);
    return false;
}

// Return a curve index. As above.
int StepFileWriter::InsertCurve(int number) {
    for(curveAliases_t c : curveAliases) {
        for(int alias : c.alias.aliases) {
            if(alias == number && (!exportParts || c.color.Equals(currentColor))) {
                return c.alias.reference;
            }
        }
    }

    // ERROR: it should never reach this point...
    return -1;
}

// Check whether this edge curve was already defined with a different ID number.
// inputs:
//        number -> id of the edge
//        prevFinish, thisFinish -> points of the edge
//        curveID -> curve of the edge
//        flip -> if an edge curve already exists this indicates whether it is in the other direction
// return:
//        true, if the edge curve is already defined
bool StepFileWriter::HasEdgeCurveAnAlias(int number, int prevFinish, int thisFinish, int curveId, bool *flip) {
    // Look for this edge in the alias list.
    for(edgeCurveAliases_t &e : edgeCurveAliases) {
        if(exportParts && !(e.color.Equals(currentColor))) {
          continue;
        }
        // Check both directions.
        if(((prevFinish == e.prevFinish && thisFinish == e.thisFinish) ||
            (prevFinish == e.thisFinish && thisFinish == e.prevFinish)) &&
             curveId == e.curveId) {
            e.alias.aliases.push_back(number);
            if(nullptr != flip) {
                if(prevFinish == e.thisFinish && thisFinish == e.prevFinish)
                    *flip = true;
                else
                    *flip = false;
            }
            return true;
        }
    }

    // New edge curve.
    edgeCurveAliases_t newEdgeCurve;
    newEdgeCurve.alias.reference = number;
    newEdgeCurve.alias.aliases.push_back(number);
    newEdgeCurve.prevFinish = prevFinish;
    newEdgeCurve.thisFinish = thisFinish;
    newEdgeCurve.curveId = curveId;
    newEdgeCurve.color = currentColor;

    edgeCurveAliases.push_back(newEdgeCurve);
    return false;
}

// Return an edge curve index. As above.
int StepFileWriter::InsertEdgeCurve(int number) {
    for(edgeCurveAliases_t e : edgeCurveAliases) {
        for(int alias : e.alias.aliases) {
            if(alias == number && (!exportParts || e.color.Equals(currentColor))) {
                return e.alias.reference;
            }
        }
    }

    // ERROR: it should never reach this point...
    return -1;
}

// Check whether this oriented edge curve was already defined with a different ID number.
// inputs:
//        number -> id of the edge
//        edgeCurveId -> curve of the edge
//        flip -> the direction of the oriented edge
// return:
//        true, if the oriented edge is already defined
bool StepFileWriter::HasOrientedEdgeAnAlias(int number, int edgeCurveId, bool flip) {
    // Look for this edge in the alias list.
    for(orientedEdgeAliases_t &e : orientedEdgeAliases) {
        if((edgeCurveId == e.edgeCurveId) && (flip == e.flip)) {
            e.alias.aliases.push_back(number);
            return true;
        }
    }

    // New oriented edge.
    orientedEdgeAliases_t newOrientedEdge;
    newOrientedEdge.alias.reference = number;
    newOrientedEdge.alias.aliases.push_back(number);
    newOrientedEdge.edgeCurveId = edgeCurveId;
    newOrientedEdge.flip        = flip;

    orientedEdgeAliases.push_back(newOrientedEdge);
    return false;
}

// Return an oriented edge index. As above.
int StepFileWriter::InsertOrientedEdge(int number) {
    for(orientedEdgeAliases_t e : orientedEdgeAliases) {
        for(int alias : e.alias.aliases) {
            if(alias == number) {
                return e.alias.reference;
            }
        }
    }

    // ERROR: it should never reach this point...
    return -1;
}

//-----------------------------------------------------------------------------
// Functions for STEP export: print to file.
//-----------------------------------------------------------------------------
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
"#167=UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(%f),#158,\n"
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
"\n",
    PRECISION);

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
    int i, ret = id;
    std::vector<int> curvePoints = {};

    for(i = 0; i <= sb->deg; i++) {
        if (!HasCartesianPointAnAlias(id + 1 + i, sb->ctrl[i], -1))
            fprintf(f, CARTESIAN_POINT_FORMAT,
                id + 1 + i,
                CO(sb->ctrl[i]));
    }
    
    for(i = 0; i <= sb->deg; i++) {
        int point = InsertPoint(id + 1 + i);
        curvePoints.push_back(point);
    }
    
    if (!HasBSplineCurveAnAlias(ret, curvePoints)) {
        fprintf(f, "#%d=(\n", ret);
        fprintf(f, "BOUNDED_CURVE()\n");
        fprintf(f, "B_SPLINE_CURVE(%d,(", sb->deg);
        for(i = 0; i <= sb->deg; i++) {
            fprintf(f, "#%d", InsertPoint(ret + i + 1));
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
        fprintf(f, "\n");
    }
    
    id = ret + 1 + (sb->deg + 1);
    return ret;
}

int StepFileWriter::ExportCurveLoop(SBezierLoop *loop, bool inner) {
    ssassert(loop->l.n >= 1, "Expected at least one loop");

    List<int> listOfTrims = {};

    SBezier *sb = loop->l.Last();

    int lastFinish, prevFinish;
    // Generate "exactly closed" contours, with the same vertex id for the
    // finish of a previous edge and the start of the next one. So we need
    // the finish of the last Bezier in the loop before we start our process.
    bool vertex_has_alias;
    if(!HasCartesianPointAnAlias(id, sb->Finish(), id + 1, &vertex_has_alias)) {
        fprintf(f, CARTESIAN_POINT_FORMAT,
            id, CO(sb->Finish()));
        fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id + 1, InsertPoint(id));
        lastFinish = id + 1;
    } else if(!vertex_has_alias) {
        fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id + 1, InsertPoint(id));
        lastFinish = id + 1; 
    }
    else {
        lastFinish = InsertVertex(id+1);
    }
    prevFinish = lastFinish;
    id += 2;

    for(sb = loop->l.First(); sb; sb = loop->l.NextAfter(sb)) {
        int curveId = ExportCurve(sb);

        int thisFinish;
        if(loop->l.NextAfter(sb) != NULL) {
            if(!HasCartesianPointAnAlias(id, sb->Finish(), id + 1, &vertex_has_alias)) {
                fprintf(f, CARTESIAN_POINT_FORMAT,
                    id, CO(sb->Finish()));
                fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id + 1, InsertPoint(id));
                thisFinish = id + 1;
            } else if(!vertex_has_alias) {
                fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id + 1, InsertPoint(id));
                thisFinish = id + 1;
            } else {
                thisFinish = InsertVertex(id+1);
            }
            id += 2;
        } else {
            thisFinish = lastFinish;
        }

        bool flip_edge;
        if (!HasEdgeCurveAnAlias(id, prevFinish, thisFinish, InsertCurve(curveId), &flip_edge)) {
            fprintf(f, "#%d=EDGE_CURVE('',#%d,#%d,#%d,%s);\n",
                id, prevFinish, thisFinish, InsertCurve(curveId), ".T.");
            if(!HasOrientedEdgeAnAlias(id + 1, id, flip_edge)) {
                fprintf(f, "#%d=ORIENTED_EDGE('',*,*,#%d,.T.);\n", id + 1, id);
            } else {
                ssassert(false, "Impossible");
            }
        } else if(!HasOrientedEdgeAnAlias(id + 1, id, flip_edge)) {
            fprintf(f, "#%d=ORIENTED_EDGE('',*,*,#%d,.%c.);\n", id + 1, InsertEdgeCurve(id),
                    flip_edge ? 'F' : 'T');
        }
        
        int oe = id+1;
        listOfTrims.Add(&oe);
        id += 2;

        prevFinish = thisFinish;
    }

    fprintf(f, "#%d=EDGE_LOOP('',(", id);
    int *oe;
    for(oe = listOfTrims.First(); oe; oe = listOfTrims.NextAfter(oe)) {
        fprintf(f, "#%d", InsertOrientedEdge(*oe));
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

void StepFileWriter::ExportSurface(SSurface *ss, SBezierList *sbl) {
    int i, j, srfid = id;

    // Read the colour of the surface: use it to tell apart surfaces
    // from different parts.
    currentColor = ss->color;

    // First, define the control points for the untrimmed surface, if they
    // were not already defined.
    for(i = 0; i <= ss->degm; i++) {
        for(j = 0; j <= ss->degn; j++) {
            if (!HasCartesianPointAnAlias(srfid + 1 + j + i*(ss->degn + 1), 
                                          ss->ctrl[i][j], -1)) {
                fprintf(f, CARTESIAN_POINT_FORMAT,
                    srfid + 1 + j + i*(ss->degn + 1),
                    CO(ss->ctrl[i][j]));
            }
        }
    }

    // Then, we create the untrimmed surface. We always specify a rational
    // B-spline surface (in fact, just a Bezier surface).
    fprintf(f, "#%d=(\n", srfid);
    fprintf(f, "BOUNDED_SURFACE()\n");
    fprintf(f, "B_SPLINE_SURFACE(%d,%d,(", ss->degm, ss->degn);
    for(i = 0; i <= ss->degm; i++) {
        fprintf(f, "(");
        for(j = 0; j <= ss->degn; j++) {
            fprintf(f, "#%d", InsertPoint(srfid + 1 + j + i*(ss->degn + 1)));
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

        fprintf(f, "),#%d,.T.);\n", srfid);
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

    // Initialization of lists.
    pointAliases = {};
    curveAliases = {};
    edgeCurveAliases = {};
    orientedEdgeAliases = {};

    WriteHeader();
    WriteProductHeader();

    advancedFaces = {};

    for(SSurface &ss : shell->surface) {
        if(ss.trim.IsEmpty())
            continue;

        // Get all of the loops of Beziers that trim our surface (with each
        // Bezier split so that we use the section as t goes from 0 to 1), and
        // the piecewise linearization of those loops in xyz space.
        SBezierList sbl = {};
        ss.MakeSectionEdgesInto(shell, NULL, &sbl);

        // Apply the export scale factor.
        ss.ScaleSelfBy(1.0/SS.exportScale);
        sbl.ScaleSelfBy(1.0/SS.exportScale);

        ExportSurface(&ss, &sbl);

        sbl.Clear();
    }

    fprintf(f, "#%d=CLOSED_SHELL('',(", id);
    int *af;
    for(af = advancedFaces.First(); af; af = advancedFaces.NextAfter(af)) {
        fprintf(f, "#%d", *af);
        if(advancedFaces.NextAfter(af) != NULL) fprintf(f, ",");
    }
    fprintf(f, "));\n");
    fprintf(f, "#%d=MANIFOLD_SOLID_BREP('brep',#%d);\n", id+1, id);
    fprintf(f, "#%d=ADVANCED_BREP_SHAPE_REPRESENTATION('',(#%d,#170),#168);\n",
        id+2, id+1);
    fprintf(f, "#%d=SHAPE_REPRESENTATION_RELATIONSHIP($,$,#169,#%d);\n",
        id+3, id+2);

    WriteFooter();

    fclose(f);
    advancedFaces.Clear();
    pointAliases.clear();
    curveAliases.clear();
    edgeCurveAliases.clear();
    orientedEdgeAliases.clear();
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

