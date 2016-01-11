//-----------------------------------------------------------------------------
// The 2d vector output stuff that isn't specific to any particular file
// format: getting the appropriate lines and curves, performing hidden line
// removal, calculating bounding boxes, and so on. Also raster and triangle
// mesh output.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include <png.h>

void SolveSpaceUI::ExportSectionTo(const std::string &filename) {
    Vector gn = (SS.GW.projRight).Cross(SS.GW.projUp);
    gn = gn.WithMagnitude(1);

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    g->GenerateDisplayItems();
    if(g->displayMesh.IsEmpty()) {
        Error("No solid model present; draw one with extrudes and revolves, "
              "or use Export 2d View to export bare lines and curves.");
        return;
    }

    // The plane in which the exported section lies; need this because we'll
    // reorient from that plane into the xy plane before exporting.
    Vector origin, u, v, n;
    double d;

    SS.GW.GroupSelection();
#define gs (SS.GW.gs)
    if((gs.n == 0 && g->activeWorkplane.v != Entity::FREE_IN_3D.v)) {
        Entity *wrkpl = SK.GetEntity(g->activeWorkplane);
        origin = wrkpl->WorkplaneGetOffset();
        n = wrkpl->Normal()->NormalN();
        u = wrkpl->Normal()->NormalU();
        v = wrkpl->Normal()->NormalV();
    } else if(gs.n == 1 && gs.faces == 1) {
        Entity *face = SK.GetEntity(gs.entity[0]);
        origin = face->FaceGetPointNum();
        n = face->FaceGetNormalNum();
        if(n.Dot(gn) < 0) n = n.ScaledBy(-1);
        u = n.Normal(0);
        v = n.Normal(1);
    } else if(gs.n == 3 && gs.vectors == 2 && gs.points == 1) {
        Vector ut = SK.GetEntity(gs.entity[0])->VectorGetNum(),
               vt = SK.GetEntity(gs.entity[1])->VectorGetNum();
        ut = ut.WithMagnitude(1);
        vt = vt.WithMagnitude(1);

        if(fabs(SS.GW.projUp.Dot(vt)) < fabs(SS.GW.projUp.Dot(ut))) {
            swap(ut, vt);
        }
        if(SS.GW.projRight.Dot(ut) < 0) ut = ut.ScaledBy(-1);
        if(SS.GW.projUp.   Dot(vt) < 0) vt = vt.ScaledBy(-1);

        origin = SK.GetEntity(gs.point[0])->PointGetNum();
        n = ut.Cross(vt);
        u = ut.WithMagnitude(1);
        v = (n.Cross(u)).WithMagnitude(1);
    } else {
        Error("Bad selection for export section. Please select:\n\n"
              "    * nothing, with an active workplane "
                        "(workplane is section plane)\n"
              "    * a face (section plane through face)\n"
              "    * a point and two line segments "
                        "(plane through point and parallel to lines)\n");
        return;
    }
    SS.GW.ClearSelection();

    n = n.WithMagnitude(1);
    d = origin.Dot(n);

    SEdgeList el = {};
    SBezierList bl = {};

    // If there's a mesh, then grab the edges from it.
    g->runningMesh.MakeEdgesInPlaneInto(&el, n, d);

    // If there's a shell, then grab the edges and possibly Beziers.
    g->runningShell.MakeSectionEdgesInto(n, d,
       &el,
       (SS.exportPwlCurves || fabs(SS.exportOffset) > LENGTH_EPS) ? NULL : &bl);

    // All of these are solid model edges, so use the appropriate style.
    SEdge *se;
    for(se = el.l.First(); se; se = el.l.NextAfter(se)) {
        se->auxA = Style::SOLID_EDGE;
    }
    SBezier *sb;
    for(sb = bl.l.First(); sb; sb = bl.l.NextAfter(sb)) {
        sb->auxA = Style::SOLID_EDGE;
    }

    el.CullExtraneousEdges();
    bl.CullIdenticalBeziers();

    // And write the edges.
    VectorFileWriter *out = VectorFileWriter::ForFile(filename);
    if(out) {
        // parallel projection (no perspective), and no mesh
        ExportLinesAndMesh(&el, &bl, NULL,
                           u, v, n, origin, 0,
                           out);
    }
    el.Clear();
    bl.Clear();
}

void SolveSpaceUI::ExportViewOrWireframeTo(const std::string &filename, bool wireframe) {
    int i;
    SEdgeList edges = {};
    SBezierList beziers = {};

    SMesh *sm = NULL;
    if(SS.GW.showShaded) {
        Group *g = SK.GetGroup(SS.GW.activeGroup);
        g->GenerateDisplayItems();
        sm = &(g->displayMesh);
    }
    if(sm && sm->IsEmpty()) {
        sm = NULL;
    }

    for(i = 0; i < SK.entity.n; i++) {
        Entity *e = &(SK.entity.elem[i]);
        if(!e->IsVisible()) continue;
        if(e->construction) continue;

        if(SS.exportPwlCurves || (sm && !SS.GW.showHdnLines) ||
                                 fabs(SS.exportOffset) > LENGTH_EPS)
        {
            // We will be doing hidden line removal, which we can't do on
            // exact curves; so we need things broken down to pwls. Same
            // problem with cutter radius compensation.
            e->GenerateEdges(&edges);
        } else {
            e->GenerateBezierCurves(&beziers);
        }
    }

    if(SS.GW.showEdges) {
        Group *g = SK.GetGroup(SS.GW.activeGroup);
        g->GenerateDisplayItems();
        SEdgeList *selr = &(g->displayEdges);
        SEdge *se;
        for(se = selr->l.First(); se; se = selr->l.NextAfter(se)) {
            edges.AddEdge(se->a, se->b, Style::SOLID_EDGE);
        }
    }

    if(SS.GW.showConstraints) {
        Constraint *c;
        for(c = SK.constraint.First(); c; c = SK.constraint.NextAfter(c)) {
            c->GetEdges(&edges);
        }
    }

    if(wireframe) {
        VectorFileWriter *out = VectorFileWriter::ForFile(filename);
        if(out) {
            ExportWireframeCurves(&edges, &beziers, out);
        }
    } else {
        Vector u = SS.GW.projRight,
               v = SS.GW.projUp,
               n = u.Cross(v),
               origin = SS.GW.offset.ScaledBy(-1);

        VectorFileWriter *out = VectorFileWriter::ForFile(filename);
        if(out) {
            ExportLinesAndMesh(&edges, &beziers, sm,
                               u, v, n, origin, SS.CameraTangent()*SS.GW.scale,
                               out);
        }

        if(out && !out->HasCanvasSize()) {
            // These file formats don't have a canvas size, so they just
            // get exported in the raw coordinate system. So indicate what
            // that was on-screen.
            SS.justExportedInfo.draw = true;
            SS.justExportedInfo.pt = origin;
            SS.justExportedInfo.u = u;
            SS.justExportedInfo.v = v;
            InvalidateGraphics();
        }
    }

    edges.Clear();
    beziers.Clear();
}

void SolveSpaceUI::ExportWireframeCurves(SEdgeList *sel, SBezierList *sbl,
                           VectorFileWriter *out)
{
    SBezierLoopSetSet sblss = {};
    SEdge *se;
    for(se = sel->l.First(); se; se = sel->l.NextAfter(se)) {
        SBezier sb = SBezier::From(
                                (se->a).ScaledBy(1.0 / SS.exportScale),
                                (se->b).ScaledBy(1.0 / SS.exportScale));
        sblss.AddOpenPath(&sb);
    }

    sbl->ScaleSelfBy(1.0/SS.exportScale);
    SBezier *sb;
    for(sb = sbl->l.First(); sb; sb = sbl->l.NextAfter(sb)) {
        sblss.AddOpenPath(sb);
    }

    out->Output(&sblss, NULL);
    sblss.Clear();
}

void SolveSpaceUI::ExportLinesAndMesh(SEdgeList *sel, SBezierList *sbl, SMesh *sm,
                                    Vector u, Vector v, Vector n,
                                        Vector origin, double cameraTan,
                                    VectorFileWriter *out)
{
    double s = 1.0 / SS.exportScale;

    // Project into the export plane; so when we're done, z doesn't matter,
    // and x and y are what goes in the DXF.
    SEdge *e;
    for(e = sel->l.First(); e; e = sel->l.NextAfter(e)) {
        // project into the specified csys, and apply export scale
        (e->a) = e->a.InPerspective(u, v, n, origin, cameraTan).ScaledBy(s);
        (e->b) = e->b.InPerspective(u, v, n, origin, cameraTan).ScaledBy(s);
    }

    SBezier *b;
    if(sbl) {
        for(b = sbl->l.First(); b; b = sbl->l.NextAfter(b)) {
            *b = b->InPerspective(u, v, n, origin, cameraTan);
            int i;
            for(i = 0; i <= b->deg; i++) {
                b->ctrl[i] = (b->ctrl[i]).ScaledBy(s);
            }
        }
    }

    // If cutter radius compensation is requested, then perform it now
    if(fabs(SS.exportOffset) > LENGTH_EPS) {
        // assemble those edges into a polygon, and clear the edge list
        SPolygon sp = {};
        sel->AssemblePolygon(&sp, NULL);
        sel->Clear();

        SPolygon compd = {};
        sp.normal = Vector::From(0, 0, -1);
        sp.FixContourDirections();
        sp.OffsetInto(&compd, SS.exportOffset*s);
        sp.Clear();

        compd.MakeEdgesInto(sel);
        compd.Clear();
    }

    // Now the triangle mesh; project, then build a BSP to perform
    // occlusion testing and generated the shaded surfaces.
    SMesh smp = {};
    if(sm) {
        Vector l0 = (SS.lightDir[0]).WithMagnitude(1),
               l1 = (SS.lightDir[1]).WithMagnitude(1);
        STriangle *tr;
        for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
            STriangle tt = *tr;
            tt.a = (tt.a).InPerspective(u, v, n, origin, cameraTan).ScaledBy(s);
            tt.b = (tt.b).InPerspective(u, v, n, origin, cameraTan).ScaledBy(s);
            tt.c = (tt.c).InPerspective(u, v, n, origin, cameraTan).ScaledBy(s);

            // And calculate lighting for the triangle
            Vector n = tt.Normal().WithMagnitude(1);
            double lighting = SS.ambientIntensity +
                                  max(0.0, (SS.lightIntensity[0])*(n.Dot(l0))) +
                                  max(0.0, (SS.lightIntensity[1])*(n.Dot(l1)));
            double r = min(1.0, tt.meta.color.redF()   * lighting),
                   g = min(1.0, tt.meta.color.greenF() * lighting),
                   b = min(1.0, tt.meta.color.blueF()  * lighting);
            tt.meta.color = RGBf(r, g, b);
            smp.AddTriangle(&tt);
        }
    }

    // Use the BSP routines to generate the split triangles in paint order.
    SBsp3 *bsp = SBsp3::FromMesh(&smp);
    SMesh sms = {};
    bsp->GenerateInPaintOrder(&sms);
    // And cull the back-facing triangles
    STriangle *tr;
    sms.l.ClearTags();
    for(tr = sms.l.First(); tr; tr = sms.l.NextAfter(tr)) {
        Vector n = tr->Normal();
        if(n.z < 0) {
            tr->tag = 1;
        }
    }
    sms.l.RemoveTagged();

    // And now we perform hidden line removal if requested
    SEdgeList hlrd = {};
    if(sm && !SS.GW.showHdnLines) {
        SKdNode *root = SKdNode::From(&smp);

        // Generate the edges where a curved surface turns from front-facing
        // to back-facing.
        if(SS.GW.showEdges) {
            root->MakeCertainEdgesInto(sel, SKdNode::TURNING_EDGES,
                        false, NULL, NULL);
        }

        root->ClearTags();
        int cnt = 1234;

        SEdge *se;
        for(se = sel->l.First(); se; se = sel->l.NextAfter(se)) {
            if(se->auxA == Style::CONSTRAINT) {
                // Constraints should not get hidden line removed; they're
                // always on top.
                hlrd.AddEdge(se->a, se->b, se->auxA);
                continue;
            }

            SEdgeList out = {};
            // Split the original edge against the mesh
            out.AddEdge(se->a, se->b, se->auxA);
            root->OcclusionTestLine(*se, &out, cnt);
            // the occlusion test splits unnecessarily; so fix those
            out.MergeCollinearSegments(se->a, se->b);
            cnt++;
            // And add the results to our output
            SEdge *sen;
            for(sen = out.l.First(); sen; sen = out.l.NextAfter(sen)) {
                hlrd.AddEdge(sen->a, sen->b, sen->auxA);
            }
            out.Clear();
        }

        sel = &hlrd;
    }

    // We kept the line segments and Beziers separate until now; but put them
    // all together, and also project everything into the xy plane, since not
    // all export targets ignore the z component of the points.
    for(e = sel->l.First(); e; e = sel->l.NextAfter(e)) {
        SBezier sb = SBezier::From(e->a, e->b);
        sb.auxA = e->auxA;
        sbl->l.Add(&sb);
    }
    for(b = sbl->l.First(); b; b = sbl->l.NextAfter(b)) {
        for(int i = 0; i <= b->deg; i++) {
            b->ctrl[i].z = 0;
        }
    }

    // If possible, then we will assemble these output curves into loops. They
    // will then get exported as closed paths.
    SBezierLoopSetSet sblss = {};
    SBezierList leftovers = {};
    SSurface srf = SSurface::FromPlane(Vector::From(0, 0, 0),
                                       Vector::From(1, 0, 0),
                                       Vector::From(0, 1, 0));
    SPolygon spxyz = {};
    bool allClosed;
    SEdge notClosedAt;
    sbl->l.ClearTags();
    sblss.FindOuterFacesFrom(sbl, &spxyz, &srf,
                             SS.ChordTolMm()*s,
                             &allClosed, &notClosedAt,
                             NULL, NULL,
                             &leftovers);
    for(b = leftovers.l.First(); b; b = leftovers.l.NextAfter(b)) {
        sblss.AddOpenPath(b);
    }

    // Now write the lines and triangles to the output file
    out->Output(&sblss, &sms);

    leftovers.Clear();
    spxyz.Clear();
    sblss.Clear();
    smp.Clear();
    sms.Clear();
    hlrd.Clear();
}

double VectorFileWriter::MmToPts(double mm) {
    // 72 points in an inch
    return (mm/25.4)*72;
}

VectorFileWriter *VectorFileWriter::ForFile(const std::string &filename) {
    VectorFileWriter *ret;
    if(FilenameHasExtension(filename, ".dxf")) {
        static DxfFileWriter DxfWriter;
        ret = &DxfWriter;
    } else if(FilenameHasExtension(filename, ".ps") || FilenameHasExtension(filename, ".eps")) {
        static EpsFileWriter EpsWriter;
        ret = &EpsWriter;
    } else if(FilenameHasExtension(filename, ".pdf")) {
        static PdfFileWriter PdfWriter;
        ret = &PdfWriter;
    } else if(FilenameHasExtension(filename, ".svg")) {
        static SvgFileWriter SvgWriter;
        ret = &SvgWriter;
    } else if(FilenameHasExtension(filename, ".plt")||FilenameHasExtension(filename, ".hpgl")) {
        static HpglFileWriter HpglWriter;
        ret = &HpglWriter;
    } else if(FilenameHasExtension(filename, ".step")||FilenameHasExtension(filename, ".stp")) {
        static Step2dFileWriter Step2dWriter;
        ret = &Step2dWriter;
    } else if(FilenameHasExtension(filename, ".txt")) {
        static GCodeFileWriter GCodeWriter;
        ret = &GCodeWriter;
    } else {
        Error("Can't identify output file type from file extension of "
        "filename '%s'; try "
        ".step, .stp, .dxf, .svg, .plt, .hpgl, .pdf, .txt, "
        ".eps, or .ps.",
            filename.c_str());
        return NULL;
    }

    FILE *f = ssfopen(filename, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", filename.c_str());
        return NULL;
    }
    ret->f = f;
    return ret;
}

void VectorFileWriter::Output(SBezierLoopSetSet *sblss, SMesh *sm) {
    STriangle *tr;
    SBezier *b;

    // First calculate the bounding box.
    ptMin = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
    ptMax = Vector::From(VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE);
    if(sm) {
        for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
            (tr->a).MakeMaxMin(&ptMax, &ptMin);
            (tr->b).MakeMaxMin(&ptMax, &ptMin);
            (tr->c).MakeMaxMin(&ptMax, &ptMin);
        }
    }
    if(sblss) {
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            SBezierLoop *sbl;
            for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
                for(b = sbl->l.First(); b; b = sbl->l.NextAfter(b)) {
                    for(int i = 0; i <= b->deg; i++) {
                        (b->ctrl[i]).MakeMaxMin(&ptMax, &ptMin);
                    }
                }
            }
        }
    }

    // And now we compute the canvas size.
    double s = 1.0 / SS.exportScale;
    if(SS.exportCanvasSizeAuto) {
        // It's based on the calculated bounding box; we grow it along each
        // boundary by the specified amount.
        ptMin.x -= s*SS.exportMargin.left;
        ptMax.x += s*SS.exportMargin.right;
        ptMin.y -= s*SS.exportMargin.bottom;
        ptMax.y += s*SS.exportMargin.top;
    } else {
        ptMin.x = -(s*SS.exportCanvas.dx);
        ptMin.y = -(s*SS.exportCanvas.dy);
        ptMax.x = ptMin.x + (s*SS.exportCanvas.width);
        ptMax.y = ptMin.y + (s*SS.exportCanvas.height);
    }

    StartFile();
    if(sm && SS.exportShadedTriangles) {
        for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
            Triangle(tr);
        }
    }
    if(sblss) {
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            SBezierLoop *sbl;
            sbl = sbls->l.First();
            if(!sbl) continue;
            b = sbl->l.First();
            if(!b || !Style::Exportable(b->auxA)) continue;

            hStyle hs = { (uint32_t)b->auxA };
            Style *stl = Style::Get(hs);
            double lineWidth   = Style::WidthMm(b->auxA)*s;
            RgbaColor strokeRgb = Style::Color(hs, true);
            RgbaColor fillRgb   = Style::FillColor(hs, true);

            StartPath(strokeRgb, lineWidth, stl->filled, fillRgb);
            for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
                for(b = sbl->l.First(); b; b = sbl->l.NextAfter(b)) {
                    Bezier(b);
                }
            }
            FinishPath(strokeRgb, lineWidth, stl->filled, fillRgb);
        }
    }
    FinishAndCloseFile();
}

void VectorFileWriter::BezierAsPwl(SBezier *sb) {
    List<Vector> lv = {};
    sb->MakePwlInto(&lv, SS.ChordTolMm() / SS.exportScale);
    int i;
    for(i = 1; i < lv.n; i++) {
        SBezier sb = SBezier::From(lv.elem[i-1], lv.elem[i]);
        Bezier(&sb);
    }
    lv.Clear();
}

void VectorFileWriter::BezierAsNonrationalCubic(SBezier *sb, int depth) {
    Vector t0 = sb->TangentAt(0), t1 = sb->TangentAt(1);
    // The curve is correct, and the first derivatives are correct, at the
    // endpoints.
    SBezier bnr = SBezier::From(
                        sb->Start(),
                        sb->Start().Plus(t0.ScaledBy(1.0/3)),
                        sb->Finish().Minus(t1.ScaledBy(1.0/3)),
                        sb->Finish());

    double tol = SS.ChordTolMm() / SS.exportScale;
    // Arbitrary choice, but make it a little finer than pwl tolerance since
    // it should be easier to achieve that with the smooth curves.
    tol /= 2;

    bool closeEnough = true;
    int i;
    for(i = 1; i <= 3; i++) {
        double t = i/4.0;
        Vector p0 = sb->PointAt(t),
               pn = bnr.PointAt(t);
        double d = (p0.Minus(pn)).Magnitude();
        if(d > tol) {
            closeEnough = false;
        }
    }

    if(closeEnough || depth > 3) {
        Bezier(&bnr);
    } else {
        SBezier bef, aft;
        sb->SplitAt(0.5, &bef, &aft);
        BezierAsNonrationalCubic(&bef, depth+1);
        BezierAsNonrationalCubic(&aft, depth+1);
    }
}

//-----------------------------------------------------------------------------
// Export a triangle mesh, in the requested format.
//-----------------------------------------------------------------------------
void SolveSpaceUI::ExportMeshTo(const std::string &filename) {
    SMesh *m = &(SK.GetGroup(SS.GW.activeGroup)->displayMesh);
    if(m->IsEmpty()) {
        Error("Active group mesh is empty; nothing to export.");
        return;
    }

    FILE *f = ssfopen(filename, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", filename.c_str());
        return;
    }

    if(FilenameHasExtension(filename, ".stl")) {
        ExportMeshAsStlTo(f, m);
    } else if(FilenameHasExtension(filename, ".obj")) {
        ExportMeshAsObjTo(f, m);
    } else if(FilenameHasExtension(filename, ".js")) {
        SEdgeList *e = &(SK.GetGroup(SS.GW.activeGroup)->displayEdges);
        ExportMeshAsThreeJsTo(f, filename, m, e);
    } else {
        Error("Can't identify output file type from file extension of "
              "filename '%s'; try .stl, .obj, .js.", filename.c_str());
    }

    fclose(f);
}

//-----------------------------------------------------------------------------
// Export the mesh as an STL file; it should always be vertex-to-vertex and
// not self-intersecting, so not much to do.
//-----------------------------------------------------------------------------
void SolveSpaceUI::ExportMeshAsStlTo(FILE *f, SMesh *sm) {
    char str[80] = {};
    strcpy(str, "STL exported mesh");
    fwrite(str, 1, 80, f);

    uint32_t n = sm->l.n;
    fwrite(&n, 4, 1, f);

    double s = SS.exportScale;
    int i;
    for(i = 0; i < sm->l.n; i++) {
        STriangle *tr = &(sm->l.elem[i]);
        Vector n = tr->Normal().WithMagnitude(1);
        float w;
        w = (float)n.x;           fwrite(&w, 4, 1, f);
        w = (float)n.y;           fwrite(&w, 4, 1, f);
        w = (float)n.z;           fwrite(&w, 4, 1, f);
        w = (float)((tr->a.x)/s); fwrite(&w, 4, 1, f);
        w = (float)((tr->a.y)/s); fwrite(&w, 4, 1, f);
        w = (float)((tr->a.z)/s); fwrite(&w, 4, 1, f);
        w = (float)((tr->b.x)/s); fwrite(&w, 4, 1, f);
        w = (float)((tr->b.y)/s); fwrite(&w, 4, 1, f);
        w = (float)((tr->b.z)/s); fwrite(&w, 4, 1, f);
        w = (float)((tr->c.x)/s); fwrite(&w, 4, 1, f);
        w = (float)((tr->c.y)/s); fwrite(&w, 4, 1, f);
        w = (float)((tr->c.z)/s); fwrite(&w, 4, 1, f);
        fputc(0, f);
        fputc(0, f);
    }
}

//-----------------------------------------------------------------------------
// Export the mesh as Wavefront OBJ format. This requires us to reduce all the
// identical vertices to the same identifier, so do that first.
//-----------------------------------------------------------------------------
void SolveSpaceUI::ExportMeshAsObjTo(FILE *f, SMesh *sm) {
    SPointList spl = {};
    STriangle *tr;
    for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
        spl.IncrementTagFor(tr->a);
        spl.IncrementTagFor(tr->b);
        spl.IncrementTagFor(tr->c);
    }

    // Output all the vertices.
    SPoint *sp;
    for(sp = spl.l.First(); sp; sp = spl.l.NextAfter(sp)) {
        fprintf(f, "v %.10f %.10f %.10f\r\n",
                        sp->p.x / SS.exportScale,
                        sp->p.y / SS.exportScale,
                        sp->p.z / SS.exportScale);
    }

    // And now all the triangular faces, in terms of those vertices. The
    // file format counts from 1, not 0.
    for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
        fprintf(f, "f %d %d %d\r\n",
                        spl.IndexForPoint(tr->a) + 1,
                        spl.IndexForPoint(tr->b) + 1,
                        spl.IndexForPoint(tr->c) + 1);
    }

    spl.Clear();
}

//-----------------------------------------------------------------------------
// Export the mesh as a JavaScript script, which is compatible with Three.js.
//-----------------------------------------------------------------------------
void SolveSpaceUI::ExportMeshAsThreeJsTo(FILE *f, const std::string &filename, SMesh *sm,
                                         SEdgeList *sel)
{
    SPointList spl = {};
    STriangle *tr;
    SEdge *e;
    Vector bndl, bndh;
    const char html[] =
    "/* Autogenerated Three.js viewer for Solvespace Model (copy into another document):\n"
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "  <head>\n"
    "    <meta charset=\"utf-8\"></meta>\n"
    "    <title>Three.js Solvespace Mesh</title>\n"
    "    <script src=\"http://threejs.org/build/three.min.js\"></script>\n"
    "    <script src=\"http://threejs.org/examples/js/controls/OrthographicTrackballControls.js\"></script>\n"
    "    <script src=\"%s.js\"></script>\n"
    "  </head>\n"
    "  <body>\n"
    "    <script>\n"
    "    window.solvespace = function(obj, params) {\n"
    "      var scene, edgeScene, camera, edgeCamera, renderer;\n"
    "      var geometry, controls, material, mesh, edges;\n"
    "      var width, height, edgeBias;\n"
    "      var directionalLightArray = [];\n"
    "\n"
    "      if(typeof params === \"undefined\" || !(\"width\" in params)) {\n"
    "        width = window.innerWidth;\n"
    "      } else {\n"
    "        width = params.width;\n"
    "      }\n"
    "\n"
    "      if(typeof params === \"undefined\" || !(\"height\" in params)) {\n"
    "        height = window.innerHeight;\n"
    "      } else {\n"
    "        height = params.height;\n"
    "      }\n"
    "      edgeBias = obj.bounds.edgeBias;\n"
    "\n"
    "      domElement = init();\n"
    "      render();\n"
    "      return domElement;\n"
    "\n"
    "      function init() {\n"
    "        scene = new THREE.Scene();\n"
    "        edgeScene = new THREE.Scene();\n"
    "\n"
    "        var ratio = (width/height);\n"
    "        camera = new THREE.OrthographicCamera(-obj.bounds.x * ratio,\n"
    "          obj.bounds.x * ratio, obj.bounds.y, -obj.bounds.y, obj.bounds.near,\n"
    "          obj.bounds.far*10);\n"
    "        camera.position.z = obj.bounds.z*3;\n"
    "\n"
    "        mesh = createMesh(obj);\n"
    "        scene.add(mesh);\n"
    "        edges = createEdges(obj);\n"
    "        edgeScene.add(edges);\n"
    "\n"
    "        for(var i = 0; i < obj.lights.d.length; i++) {\n"
    "          var lightColor = new THREE.Color(obj.lights.d[i].intensity,\n"
    "            obj.lights.d[i].intensity, obj.lights.d[i].intensity);\n"
    "          var directionalLight = new THREE.DirectionalLight(lightColor, 1);\n"
    "          directionalLight.position.set(obj.lights.d[i].direction[0],\n"
    "            obj.lights.d[i].direction[1], obj.lights.d[i].direction[2]);\n"
    "          directionalLightArray.push(directionalLight);\n"
    "          scene.add(directionalLight);\n"
    "        }\n"
    "\n"
    "        var lightColor = new THREE.Color(obj.lights.a, obj.lights.a, obj.lights.a);\n"
    "        var ambientLight = new THREE.AmbientLight(lightColor.getHex());\n"
    "        scene.add(ambientLight);\n"
    "\n"
    "        renderer = new THREE.WebGLRenderer();\n"
    "        renderer.setPixelRatio(window.devicePixelRatio);\n"
    "        renderer.setSize(width, height);\n"
    "        renderer.autoClear = false;\n"
    "\n"
    "        controls = new THREE.OrthographicTrackballControls(camera, renderer.domElement);\n"
    "        controls.screen.width = width;\n"
    "        controls.screen.height = height;\n"
    "        controls.radius = (width + height)/4;\n"
    "        controls.rotateSpeed = 2.0;\n"
    "        controls.zoomSpeed = 2.0;\n"
    "        controls.panSpeed = 1.0;\n"
    "        controls.staticMoving = true;\n"
    "        controls.addEventListener(\"change\", render);\n"
    "        controls.addEventListener(\"change\", lightUpdate);\n"
    "        controls.addEventListener(\"change\", setControlsCenter);\n"
    "\n"
    "        animate();\n"
    "        return renderer.domElement;\n"
    "      }\n"
    "\n"
    "      function animate() {\n"
    "        requestAnimationFrame(animate);\n"
    "        controls.update();\n"
    "      }\n"
    "\n"
    "      function render() {\n"
    "        renderer.clear();\n"
    "        renderer.render(scene, camera);\n"
    "        var oldFar = camera.far\n"
    "        camera.far = camera.far + edgeBias;\n"
    "        camera.updateProjectionMatrix();\n"
    "        renderer.render(edgeScene, camera);\n"
    "        camera.far = oldFar;\n"
    "        camera.updateProjectionMatrix();\n"
    "      }\n"
    "\n"
    "      function lightUpdate() {\n"
    "        var projRight = new THREE.Vector3();\n"
    "        var projZ = new THREE.Vector3();\n"
    "        var changeBasis = new THREE.Matrix3();\n"
    "\n"
    "        // The original light positions were in camera space.\n"
    "        // Project them into standard space using camera's basis\n"
    "        // vectors (up, target, and their cross product).\n"
    "        projRight.copy(camera.up);\n"
    "        projZ.copy(camera.position).sub(controls.target).normalize();\n"
    "        projRight.cross(projZ).normalize();\n"
    "        changeBasis.set(projRight.x, camera.up.x, controls.target.x,\n"
    "          projRight.y, camera.up.y, controls.target.y,\n"
    "          projRight.z, camera.up.z, controls.target.z);\n"
    "\n"
    "        for(var i = 0; i < obj.lights.d.length; i++) {\n"
    "          var newLightPos = changeBasis.applyToVector3Array(\n"
    "            [obj.lights.d[i].direction[0], obj.lights.d[i].direction[1],\n"
    "             obj.lights.d[i].direction[2]]);\n"
    "          directionalLightArray[i].position.set(newLightPos[0],\n"
    "            newLightPos[1], newLightPos[2]);\n"
    "        }\n"
    "      }\n"
    "\n"
    "      function setControlsCenter() {\n"
    "        var rect = renderer.domElement.getBoundingClientRect()\n"
    "        controls.screen.left = rect.left + document.body.scrollLeft;\n"
    "        controls.screen.top = rect.top + document.body.scrollTop;\n"
    "      }\n"
    "\n"
    "      function createMesh(mesh_obj) {\n"
    "        var geometry = new THREE.Geometry();\n"
    "        var materialIndex = 0, materialList = [];\n"
    "        var opacitiesSeen = {};\n"
    "\n"
    "        for(var i = 0; i < mesh_obj.points.length; i++) {\n"
    "          geometry.vertices.push(new THREE.Vector3(mesh_obj.points[i][0],\n"
    "            mesh_obj.points[i][1], mesh_obj.points[i][2]));\n"
    "        }\n"
    "\n"
    "        for(var i = 0; i < mesh_obj.faces.length; i++) {\n"
    "          var currOpacity = ((mesh_obj.colors[i] & 0xFF000000) >>> 24)/255.0;\n"
    "          if(opacitiesSeen[currOpacity] === undefined) {\n"
    "            opacitiesSeen[currOpacity] = materialIndex;\n"
    "            materialIndex++;\n"
    "            materialList.push(new THREE.MeshLambertMaterial(\n"
    "              {vertexColors: THREE.FaceColors, opacity: currOpacity,\n"
    "                transparent: true}));\n"
    "          }\n"
    "\n"
    "          geometry.faces.push(new THREE.Face3(mesh_obj.faces[i][0],\n"
    "            mesh_obj.faces[i][1], mesh_obj.faces[i][2],\n"
    "            [new THREE.Vector3(mesh_obj.normals[i][0][0],\n"
    "              mesh_obj.normals[i][0][1], mesh_obj.normals[i][0][2]),\n"
    "             new THREE.Vector3(mesh_obj.normals[i][1][0],\n"
    "              mesh_obj.normals[i][1][1], mesh_obj.normals[i][1][2]),\n"
    "             new THREE.Vector3(mesh_obj.normals[i][2][0],\n"
    "              mesh_obj.normals[i][2][1], mesh_obj.normals[i][2][2])],\n"
    "            new THREE.Color(mesh_obj.colors[i] & 0x00FFFFFF),\n"
    "            opacitiesSeen[currOpacity]));\n"
    "        }\n"
    "\n"
    "        geometry.computeBoundingSphere();\n"
    "        return new THREE.Mesh(geometry, new THREE.MeshFaceMaterial(materialList));\n"
    "      }\n"
    "\n"
    "      function createEdges(mesh_obj) {\n"
    "        var geometry = new THREE.Geometry();\n"
    "        var material = new THREE.LineBasicMaterial();\n"
    "\n"
    "        for(var i = 0; i < mesh_obj.edges.length; i++) {\n"
    "          geometry.vertices.push(new THREE.Vector3(mesh_obj.edges[i][0][0],\n"
    "            mesh_obj.edges[i][0][1], mesh_obj.edges[i][0][2]),\n"
    "            new THREE.Vector3(mesh_obj.edges[i][1][0],\n"
    "            mesh_obj.edges[i][1][1], mesh_obj.edges[i][1][2]));\n"
    "        }\n"
    "\n"
    "        return new THREE.Line(geometry, material, THREE.LinePieces);\n"
    "      }\n"
    "    };\n"
    "\n"
    "    document.body.appendChild(solvespace(three_js_%s));\n"
    "    </script>\n"
    "  </body>\n"
    "</html>\n"
    "*/\n\n";

    // A default three.js viewer with OrthographicTrackballControls is
    // generated as a comment preceding the data.

    // x bounds should be the range of x or y, whichever
    // is larger, before aspect ratio correction is applied.
    // y bounds should be the range of x or y, whichever is
    // larger. No aspect ratio correction is applied.
    // Near plane should be 1.
    // Camera's z-position should be the range of z + 1 or the larger of
    // the x or y bounds, whichever is larger.
    // Far plane should be at least twice as much as the camera's
    // z-position.
    // Edge projection bias should be about 1/500 of the far plane's distance.
    // Further corrections will be applied to the z-position and far plane in
    // the default viewer, but the defaults are fine for a model which
    // only rotates about the world origin.

    sm->GetBounding(&bndh, &bndl);
    double largerBoundXY = max((bndh.x - bndl.x), (bndh.y - bndl.y));
    double largerBoundZ = max(largerBoundXY, (bndh.z - bndl.z + 1));

    std::string baseFilename = filename;

    size_t lastSlash = baseFilename.rfind(PATH_SEP);
    if(lastSlash == std::string::npos) oops();
    baseFilename.erase(0, lastSlash + 1);

    size_t dot = baseFilename.rfind('.');
    baseFilename.erase(dot);

    for(int i = 0; i < baseFilename.length(); i++) {
        if(!isalpha(baseFilename[i]) &&
           /* also permit UTF-8 */ !((unsigned char)baseFilename[i] >= 0x80))
            baseFilename[i] = '_';
    }

    fprintf(f, html, baseFilename.c_str(), baseFilename.c_str());
    fprintf(f, "var three_js_%s = {\n"
               "  bounds: {\n"
               "    x: %f, y: %f, near: %f, far: %f, z: %f, edgeBias: %f\n"
               "  },\n",
            baseFilename.c_str(),
            largerBoundXY,
            largerBoundXY,
            1.0,
            largerBoundZ * 2,
            largerBoundZ,
            largerBoundZ / 250);

    // Output lighting information.
    fputs("  lights: {\n"
          "    d: [\n", f);

    // Directional.
    int lightCount;
    for(lightCount = 0; lightCount < 2; lightCount++)
    {
        fprintf(f, "      {\n"
                   "        intensity: %f, direction: [%f, %f, %f]\n"
                   "      },\n",
                SS.lightIntensity[lightCount],
                CO(SS.lightDir[lightCount]));
    }

    // Global Ambience.
    fprintf(f, "    ],\n"
               "    a: %f\n", SS.ambientIntensity);

    for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
        spl.IncrementTagFor(tr->a);
        spl.IncrementTagFor(tr->b);
        spl.IncrementTagFor(tr->c);
    }

    // Output all the vertices.
    SPoint *sp;
    fputs("  },\n"
          "  points: [\n", f);
    for(sp = spl.l.First(); sp; sp = spl.l.NextAfter(sp)) {
        fprintf(f, "    [%f, %f, %f],\n",
                sp->p.x / SS.exportScale,
                sp->p.y / SS.exportScale,
                sp->p.z / SS.exportScale);
    }

    fputs("  ],\n"
          "  faces: [\n", f);
    // And now all the triangular faces, in terms of those vertices.
    // This time we count from zero.
    for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
        fprintf(f, "    [%d, %d, %d],\n",
                spl.IndexForPoint(tr->a),
                spl.IndexForPoint(tr->b),
                spl.IndexForPoint(tr->c));
    }

    // Output face normals.
    fputs("  ],\n"
          "  normals: [\n", f);
    for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
        fprintf(f, "    [[%f, %f, %f], [%f, %f, %f], [%f, %f, %f]],\n",
                CO(tr->an), CO(tr->bn), CO(tr->cn));
    }

    fputs("  ],\n"
          "  colors: [\n", f);
    // Output triangle colors.
    for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
        fprintf(f, "    0x%x,\n", tr->meta.color.ToARGB32());
    }

    fputs("  ],\n"
          "  edges: [\n", f);
    // Output edges. Assume user's model colors do not obscure white edges.
    for(e = sel->l.First(); e; e = sel->l.NextAfter(e)) {
        fprintf(f, "    [[%f, %f, %f], [%f, %f, %f]],\n",
                CO(e->a), CO(e->b));
    }

    fputs("  ]\n};\n", f);
    spl.Clear();
}

//-----------------------------------------------------------------------------
// Export a view of the model as an image; we just take a screenshot, by
// rendering the view in the usual way and then copying the pixels.
//-----------------------------------------------------------------------------
void SolveSpaceUI::ExportAsPngTo(const std::string &filename) {
    int w = (int)SS.GW.width, h = (int)SS.GW.height;
    // No guarantee that the back buffer contains anything valid right now,
    // so repaint the scene. And hide the toolbar too.
    bool prevShowToolbar = SS.showToolbar;
    SS.showToolbar = false;
    SS.GW.Paint();
    SS.showToolbar = prevShowToolbar;

    FILE *f = ssfopen(filename, "wb");
    if(!f) goto err;

    png_struct *png_ptr; png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
        NULL, NULL, NULL);
    if(!png_ptr) goto err;

    png_info *info_ptr; info_ptr = png_create_info_struct(png_ptr);
    if(!png_ptr) goto err;

    if(setjmp(png_jmpbuf(png_ptr))) goto err;

    png_init_io(png_ptr, f);

    // glReadPixels wants to align things on 4-boundaries, and there's 3
    // bytes per pixel. As long as the row width is divisible by 4, all
    // works out.
    w &= ~3; h &= ~3;

    png_set_IHDR(png_ptr, info_ptr, w, h,
        8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    // Get the pixel data from the framebuffer
    uint8_t *pixels; pixels = (uint8_t *)AllocTemporary(3*w*h);
    uint8_t **rowptrs; rowptrs = (uint8_t **)AllocTemporary(h*sizeof(uint8_t *));
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    int y;
    for(y = 0; y < h; y++) {
        // gl puts the origin at lower left, but png puts it top left
        rowptrs[y] = pixels + ((h - 1) - y)*(3*w);
    }
    png_write_image(png_ptr, rowptrs);

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(f);
    return;

err:
    Error("Error writing PNG file '%s'", filename.c_str());
    if(f) fclose(f);
    return;
}

