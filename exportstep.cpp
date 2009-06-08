#include "solvespace.h"

void StepFileWriter::WriteHeader(void) {
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
}

int StepFileWriter::ExportCurve(SBezier *sb) {
    int i, ret = id;

    fprintf(f, "#%d=B_SPLINE_CURVE_WITH_KNOTS('',%d,(",
        ret, sb->deg, (sb->deg + 1), (sb->deg + 1));
    for(i = 0; i <= sb->deg; i++) {
        fprintf(f, "#%d", ret + i + 1);
        if(i != sb->deg) fprintf(f, ",");
    }
    fprintf(f, "),.UNSPECIFIED.,.F.,.F.,(%d, %d),",
        (sb->deg + 1), (sb-> deg + 1));
    fprintf(f, "(0.000,1.000),.UNSPECIFIED.);\n");

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
    List<int> listOfTrims;
    ZERO(&listOfTrims);

    SBezier *sb;
    for(sb = loop->l.First(); sb; sb = loop->l.NextAfter(sb)) {
        int curveId = ExportCurve(sb);

        fprintf(f, "#%d=CARTESIAN_POINT('',(%.10f,%.10f,%.10f));\n",
            id, CO(sb->Start()));
        fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id+1, id);
        fprintf(f, "#%d=CARTESIAN_POINT('',(%.10f,%.10f,%.10f));\n",
            id+2, CO(sb->Finish()));
        fprintf(f, "#%d=VERTEX_POINT('',#%d);\n", id+3, id+2);
        fprintf(f, "#%d=EDGE_CURVE('',#%d,#%d,#%d,%s);\n",
            id+4, id+1, id+3, curveId, ".T.");
        fprintf(f, "#%d=ORIENTED_EDGE('',*,*,#%d,.T.);\n",
            id+5, id+4);

        int oe = id+5;
        listOfTrims.Add(&oe);

        id += 6;
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

void StepFileWriter::ExportSurface(SSurface *ss) {
    int i, j, srfid = id;

    fprintf(f, "#%d=B_SPLINE_SURFACE_WITH_KNOTS('',%d,%d,(", 
        srfid, ss->degm, ss->degn);
    for(i = 0; i <= ss->degm; i++) {
        fprintf(f, "(");
        for(j = 0; j <= ss->degn; j++) {
            fprintf(f, "#%d", srfid + 1 + j + i*(ss->degn + 1));
            if(j != ss->degn) fprintf(f, ",");
        }
        fprintf(f, ")");
        if(i != ss->degm) fprintf(f, ",");
    }
    fprintf(f, "),.UNSPECIFIED.,.F.,.F.,.F.,(%d,%d),(%d,%d),",
        (ss->degm + 1), (ss->degm + 1),
        (ss->degn + 1), (ss->degn + 1));
    fprintf(f, "(0.000,1.000),(0.000,1.000),.UNSPECIFIED.);\n");

    for(i = 0; i <= ss->degm; i++) {
        for(j = 0; j <= ss->degn; j++) {
            fprintf(f, "#%d=CARTESIAN_POINT('',(%.10f,%.10f,%.10f));\n", 
                srfid + 1 + j + i*(ss->degn + 1),
                CO(ss->ctrl[i][j]));
        }
    }
    fprintf(f, "\n");

    id = srfid + 1 + (ss->degm + 1)*(ss->degn + 1);

    // Get all of the loops of Beziers that trim our surface (with each
    // Bezier split so that we use the section as t goes from 0 to 1), and
    // the piecewise linearization of those loops in xyz space.
    SBezierList sbl;
    SPolygon sp;
    ZERO(&sbl);
    ZERO(&sp);
    SEdge errorAt;
    bool allClosed;
    ss->MakeSectionEdgesInto(shell, NULL, &sbl);
    SBezierLoopSet sbls = SBezierLoopSet::From(&sbl, &sp, &allClosed, &errorAt);

    // Convert the xyz piecewise linear to uv piecewise linear.
    SContour *contour;
    for(contour = sp.l.First(); contour; contour = sp.l.NextAfter(contour)) {
        SPoint *pt;
        for(pt = contour->l.First(); pt; pt = contour->l.NextAfter(pt)) {
            double u, v;
            ss->ClosestPointTo(pt->p, &u, &v);
            pt->p = Vector::From(u, v, 0);
        }
    }
    sp.normal = Vector::From(0, 0, 1);

    static const int OUTER_LOOP = 10;
    static const int INNER_LOOP = 20;
    static const int USED_LOOP  = 30;
    // Fix the contour directions; SBezierLoopSet::From() works only for
    // planes, since it uses the polygon xyz space.
    sp.FixContourDirections();
    for(i = 0; i < sp.l.n; i++) {
        SContour    *contour = &(sp.l.elem[i]);
        SBezierLoop *bl = &(sbls.l.elem[i]);
        if(contour->tag) {
            // This contour got reversed in the polygon to make the directions
            // consistent, so the same must be necessary for the Bezier loop.
            bl->Reverse();
        }
        if(contour->IsClockwiseProjdToNormal(sp.normal)) {
            bl->tag = INNER_LOOP;
        } else {
            bl->tag = OUTER_LOOP;
        }
    }


    bool loopsRemaining = true;
    while(loopsRemaining) {
        loopsRemaining = false;
        for(i = 0; i < sbls.l.n; i++) {
            SBezierLoop *loop = &(sbls.l.elem[i]);
            if(loop->tag != OUTER_LOOP) continue;

            // Check if this contour contains any outer loops; if it does, then
            // we should do those "inner outer loops" first; otherwise we
            // will steal their holes, since their holes also lie inside this
            // contour.
            for(j = 0; j < sbls.l.n; j++) {
                SBezierLoop *outer = &(sbls.l.elem[j]);
                if(i == j) continue;
                if(outer->tag != OUTER_LOOP) continue;

                Vector p = sp.l.elem[j].AnyEdgeMidpoint();
                if(sp.l.elem[i].ContainsPointProjdToNormal(sp.normal, p)) {
                    break;
                }
            }
            if(j < sbls.l.n) {
                // It does, can't do this one yet.
                continue;
            }

            loopsRemaining = true;
            loop->tag = USED_LOOP;

            List<int> listOfLoops;
            ZERO(&listOfLoops);

            // Create the face outer boundary from the outer loop.
            int fob = ExportCurveLoop(loop, false);
            listOfLoops.Add(&fob);

            // And create the face inner boundaries from any inner loops that 
            // lie within this contour.
            for(j = 0; j < sbls.l.n; j++) {
                SBezierLoop *inner = &(sbls.l.elem[j]);
                if(inner->tag != INNER_LOOP) continue;

                Vector p = sp.l.elem[j].AnyEdgeMidpoint();
                if(sp.l.elem[i].ContainsPointProjdToNormal(sp.normal, p)) {
                    int fib = ExportCurveLoop(inner, true);
                    listOfLoops.Add(&fib);

                    inner->tag = USED_LOOP;
                }
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
    }
}

void StepFileWriter::ExportTo(char *file) {
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    shell = &(g->runningShell);
    if(shell->surface.n == 0) {
        Error("The model does not contain any surfaces to export.%s",
            g->runningMesh.l.n > 0 ? 
                "\r\n\r\nThe model does contain triangles from a mesh, but "
                "a triangle mesh cannot be exported as a STEP file. Try "
                "File -> Export Mesh... instead." : "");
        return;
    }

    f = fopen(file, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", file);
        return;
    }

    WriteHeader();

    id = 200;

    ZERO(&advancedFaces);

    SSurface *ss;
    for(ss = shell->surface.First(); ss; ss = shell->surface.NextAfter(ss)) {
        if(ss->trim.n == 0) continue;

        ExportSurface(ss);
    }

    fprintf(f, "#%d=CLOSED_SHELL('',(", id);
    int *af;
    for(af = advancedFaces.First(); af; af = advancedFaces.NextAfter(af)) {
        fprintf(f, "#%d", *af);
        if(advancedFaces.NextAfter(af) != NULL) fprintf(f, ",");
    }
    fprintf(f, "));\n");
    fprintf(f, "#%d=MANIFOLD_SOLID_BREP('brep_1',#%d);\n", id+1, id);
    fprintf(f, "#%d=ADVANCED_BREP_SHAPE_REPRESENTATION('',(#%d,#170),#168);\n",
        id+2, id+1);
    fprintf(f, "#%d=SHAPE_REPRESENTATION_RELATIONSHIP($,$,#169,#%d);\n",
        id+3, id+2);

    fprintf(f,
"\n"
"ENDSEC;\n"
"\n"
"END-ISO-10303-21;\n"
        );

    fclose(f);
    advancedFaces.Clear();
}

