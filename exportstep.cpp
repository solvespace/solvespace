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

int StepFileWriter::ExportSurface(SSurface *ss) {
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

    SBezierList sbl;
    SPolygon sp;
    ZERO(&sbl);
    ZERO(&sp);
    SEdge errorAt;
    bool allClosed;
    ss->MakeSectionEdgesInto(shell, NULL, &sbl);
    SBezierLoopSet sbls = SBezierLoopSet::From(&sbl, &sp, &allClosed, &errorAt);

    List<int> listOfLoops;
    ZERO(&listOfLoops);

    SBezierLoop *loop;
    for(loop = sbls.l.First(); loop; loop = sbls.l.NextAfter(loop)) {
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

            i = id+5;
            listOfTrims.Add(&i);

            id += 6;
        }

        fprintf(f, "#%d=EDGE_LOOP('',(", id);
        int *ei;
        for(ei = listOfTrims.First(); ei; ei = listOfTrims.NextAfter(ei)) {
            fprintf(f, "#%d", *ei);
            if(listOfTrims.NextAfter(ei) != NULL) fprintf(f, ",");
        }
        fprintf(f, "));\n");

        int fb = id + 1;
        fprintf(f, "#%d=FACE_OUTER_BOUND('',#%d,.T.);\n", fb, id);
        listOfLoops.Add(&fb);

        id += 2;
        listOfTrims.Clear();
    }

    int advFaceId = id;
    fprintf(f, "#%d=ADVANCED_FACE('',(", advFaceId);
    int *fb;
    for(fb = listOfLoops.First(); fb; fb = listOfLoops.NextAfter(fb)) {
        fprintf(f, "#%d", *fb);
        if(listOfLoops.NextAfter(fb) != NULL) fprintf(f, ",");
    }

    fprintf(f, "),#%d,.T.);\n", srfid);
    fprintf(f, "\n");

    id++;

    listOfLoops.Clear();
    return advFaceId;
}

void StepFileWriter::ExportTo(char *file) {
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    shell = &(g->runningShell);
    if(shell->surface.n == 0) {
        Error("The model does not contain any surfaces to export.%s",
            g->runningMesh.l.n > 0 ? 
                "\r\nThe model does contain triangles from a mesh, but a "
                "triangle mesh cannot be exported as a STEP file. Try "
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

    List<int> ls;
    ZERO(&ls);

    SSurface *ss;
    for(ss = shell->surface.First(); ss; ss = shell->surface.NextAfter(ss)) {
        if(ss->trim.n == 0) continue;

        int sid = ExportSurface(ss);
        ls.Add(&sid);
    }

    fprintf(f, "#%d=CLOSED_SHELL('',(", id);
    int *es;
    for(es = ls.First(); es; es = ls.NextAfter(es)) {
        fprintf(f, "#%d", *es);
        if(ls.NextAfter(es) != NULL) fprintf(f, ",");
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
    ls.Clear();
}

