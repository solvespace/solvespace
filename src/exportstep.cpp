//-----------------------------------------------------------------------------
// Export a STEP file describing our ratpoly shell.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

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
            CO(sb->ctrl[i]));
    }
    fprintf(f, "\n");

    id = ret + 1 + (sb->deg + 1);
    return ret;
}

int StepFileWriter::ExportCurveLoop(SBezierLoop *loop, bool inner) {
    ssassert(loop->l.n >= 1, "Expected at least one loop");

    List<int> listOfTrims = {};

    SBezier *sb = loop->l.Last();

    // Generate "exactly closed" contours, with the same vertex id for the
    // finish of a previous edge and the start of the next one. So we need
    // the finish of the last Bezier in the loop before we start our process.
    fprintf(f, "#%d=CARTESIAN_POINT('',(%.10f,%.10f,%.10f));\n",
        id, CO(sb->Finish()));
    fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id+1, id);
    int lastFinish = id + 1, prevFinish = lastFinish;
    id += 2;

    for(sb = loop->l.First(); sb; sb = loop->l.NextAfter(sb)) {
        int curveId = ExportCurve(sb);

        int thisFinish;
        if(loop->l.NextAfter(sb) != NULL) {
            fprintf(f, "#%d=CARTESIAN_POINT('',(%.10f,%.10f,%.10f));\n",
                id, CO(sb->Finish()));
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

void StepFileWriter::ExportSurface(SSurface *ss, SBezierList *sbl) {
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
                CO(ss->ctrl[i][j]));
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

        fprintf(f, "),#%d,.T.);\n", srfid);
        fprintf(f, "\n");
        advancedFaces.Add(&advFaceId);

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

    SSurface *ss;
    for(ss = shell->surface.First(); ss; ss = shell->surface.NextAfter(ss)) {
        if(ss->trim.IsEmpty())
            continue;

        // Get all of the loops of Beziers that trim our surface (with each
        // Bezier split so that we use the section as t goes from 0 to 1), and
        // the piecewise linearization of those loops in xyz space.
        SBezierList sbl = {};
        ss->MakeSectionEdgesInto(shell, NULL, &sbl);

        // Apply the export scale factor.
        ss->ScaleSelfBy(1.0/SS.exportScale);
        sbl.ScaleSelfBy(1.0/SS.exportScale);

        ExportSurface(ss, &sbl);

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

