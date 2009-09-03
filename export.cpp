//-----------------------------------------------------------------------------
// The 2d vector output stuff that isn't specific to any particular file
// format: getting the appropriate lines and curves, performing hidden line
// removal, calculating bounding boxes, and so on. Also raster and triangle
// mesh output.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include <png.h>

void SolveSpace::ExportSectionTo(char *filename) {
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
            SWAP(Vector, ut, vt);
        }
        if(SS.GW.projRight.Dot(ut) < 0) ut = ut.ScaledBy(-1);
        if(SS.GW.projUp.   Dot(vt) < 0) vt = vt.ScaledBy(-1);

        origin = SK.GetEntity(gs.point[0])->PointGetNum();
        n = ut.Cross(vt);
        u = ut.WithMagnitude(1);
        v = (n.Cross(u)).WithMagnitude(1);
    } else {
        Error("Bad selection for export section. Please select:\r\n\r\n"
              "    * nothing, with an active workplane "
                        "(workplane is section plane)\r\n"
              "    * a face (section plane through face)\r\n"
              "    * a point and two line segments "
                        "(plane through point and parallel to lines)\r\n");
        return;
    }
    SS.GW.ClearSelection();

    n = n.WithMagnitude(1);
    d = origin.Dot(n);

    SEdgeList el;
    ZERO(&el);
    SBezierList bl;
    ZERO(&bl);

    // If there's a mesh, then grab the edges from it.
    g->runningMesh.MakeEdgesInPlaneInto(&el, n, d);

    // If there's a shell, then grab the edges and possibly Beziers.
    g->runningShell.MakeSectionEdgesInto(n, d,
       &el, 
       (SS.exportPwlCurves || fabs(SS.exportOffset) > LENGTH_EPS) ? NULL : &bl);

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

void SolveSpace::ExportViewTo(char *filename) {
    int i;
    SEdgeList edges;
    ZERO(&edges);
    SBezierList beziers;
    ZERO(&beziers);

    SMesh *sm = NULL;
    if(SS.GW.showShaded) {
        Group *g = SK.GetGroup(SS.GW.activeGroup);
        g->GenerateDisplayItems();
        sm = &(g->displayMesh);
    }
    if(sm->IsEmpty()) {
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
            edges.AddEdge(se->a, se->b);
        }
    }

    if(SS.GW.showConstraints) {
        Constraint *c;
        for(c = SK.constraint.First(); c; c = SK.constraint.NextAfter(c)) {
            c->GetEdges(&edges);
        }
    }

    Vector u = SS.GW.projRight,
           v = SS.GW.projUp,
           n = u.Cross(v),
           origin = SS.GW.offset.ScaledBy(-1);

    VectorFileWriter *out = VectorFileWriter::ForFile(filename);
    if(out) {
        ExportLinesAndMesh(&edges, &beziers, sm,
                           u, v, n, origin, SS.cameraTangent*SS.GW.scale,
                           out);
    }
    edges.Clear();
    beziers.Clear();
}

void SolveSpace::ExportLinesAndMesh(SEdgeList *sel, SBezierList *sbl, SMesh *sm,
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
        SPolygon sp;
        ZERO(&sp);
        sel->AssemblePolygon(&sp, NULL);
        sel->Clear();

        SPolygon compd;
        ZERO(&compd);
        sp.normal = Vector::From(0, 0, -1);
        sp.FixContourDirections();
        sp.OffsetInto(&compd, SS.exportOffset*s);
        sp.Clear();

        compd.MakeEdgesInto(sel);
        compd.Clear();
    }

    // Now the triangle mesh; project, then build a BSP to perform
    // occlusion testing and generated the shaded surfaces.
    SMesh smp;
    ZERO(&smp);
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
                                  max(0, (SS.lightIntensity[0])*(n.Dot(l0))) +
                                  max(0, (SS.lightIntensity[1])*(n.Dot(l1)));
            double r = min(1, REDf  (tt.meta.color)*lighting),
                   g = min(1, GREENf(tt.meta.color)*lighting),
                   b = min(1, BLUEf (tt.meta.color)*lighting);
            tt.meta.color = RGBf(r, g, b);
            smp.AddTriangle(&tt);
        }
    }

    // Use the BSP routines to generate the split triangles in paint order.
    SBsp3 *bsp = SBsp3::FromMesh(&smp);
    SMesh sms;
    ZERO(&sms);
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
    SEdgeList hlrd;
    ZERO(&hlrd);
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
            SEdgeList out;
            ZERO(&out);
            // Split the original edge against the mesh
            out.AddEdge(se->a, se->b);
            root->OcclusionTestLine(*se, &out, cnt);
            // the occlusion test splits unnecessarily; so fix those
            out.MergeCollinearSegments(se->a, se->b);
            cnt++;
            // And add the results to our output
            SEdge *sen;
            for(sen = out.l.First(); sen; sen = out.l.NextAfter(sen)) {
                hlrd.AddEdge(sen->a, sen->b);
            }
            out.Clear();
        }

        sel = &hlrd;
    }

    // Now write the lines and triangles to the output file
    out->Output(sel, sbl, &sms);

    smp.Clear();
    sms.Clear();
    hlrd.Clear();
}

double VectorFileWriter::MmToPts(double mm) {
    // 72 points in an inch
    return (mm/25.4)*72;
}

bool VectorFileWriter::StringEndsIn(char *str, char *ending) {
    int i, ls = strlen(str), le = strlen(ending);

    if(ls < le) return false;
        
    for(i = 0; i < le; i++) {
        if(tolower(ending[le-i-1]) != tolower(str[ls-i-1])) {
            return false;
        }
    }
    return true;
}

VectorFileWriter *VectorFileWriter::ForFile(char *filename) {
    VectorFileWriter *ret;
    if(StringEndsIn(filename, ".dxf")) {
        static DxfFileWriter DxfWriter;
        ret = &DxfWriter;
    } else if(StringEndsIn(filename, ".ps") || StringEndsIn(filename, ".eps")) {
        static EpsFileWriter EpsWriter;
        ret = &EpsWriter;
    } else if(StringEndsIn(filename, ".pdf")) {
        static PdfFileWriter PdfWriter;
        ret = &PdfWriter;
    } else if(StringEndsIn(filename, ".svg")) {
        static SvgFileWriter SvgWriter;
        ret = &SvgWriter;
    } else if(StringEndsIn(filename, ".plt")||StringEndsIn(filename, ".hpgl")) {
        static HpglFileWriter HpglWriter;
        ret = &HpglWriter;
    } else if(StringEndsIn(filename, ".step")||StringEndsIn(filename, ".stp")) {
        static Step2dFileWriter Step2dWriter;
        ret = &Step2dWriter;
    } else {
        Error("Can't identify output file type from file extension of "
        "filename '%s'; try .step, .stp, .dxf, .svg, .plt, .hpgl, .pdf, "
        ".eps, or .ps.",
            filename);
        return NULL;
    }

    FILE *f = fopen(filename, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", filename);
        return NULL;
    }
    ret->f = f;
    return ret;
}

void VectorFileWriter::Output(SEdgeList *sel, SBezierList *sbl, SMesh *sm) {
    STriangle *tr;
    SEdge *e;
    SBezier *b;

    // First calculate the bounding box.
    ptMin = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
    ptMax = Vector::From(VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE);
    if(sel) {
        for(e = sel->l.First(); e; e = sel->l.NextAfter(e)) {
            (e->a).MakeMaxMin(&ptMax, &ptMin);
            (e->b).MakeMaxMin(&ptMax, &ptMin);
        }
    }
    if(sm) {
        for(tr = sm->l.First(); tr; tr = sm->l.NextAfter(tr)) {
            (tr->a).MakeMaxMin(&ptMax, &ptMin);
            (tr->b).MakeMaxMin(&ptMax, &ptMin);
            (tr->c).MakeMaxMin(&ptMax, &ptMin);
        }
    }
    if(sbl) {
        for(b = sbl->l.First(); b; b = sbl->l.NextAfter(b)) {
            int i;
            for(i = 0; i <= b->deg; i++) {
                (b->ctrl[i]).MakeMaxMin(&ptMax, &ptMin);
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
    if(sel) {
        for(e = sel->l.First(); e; e = sel->l.NextAfter(e)) {
            LineSegment(e->a.x, e->a.y, e->b.x, e->b.y);
        }
    }
    if(sbl) {
        for(b = sbl->l.First(); b; b = sbl->l.NextAfter(b)) {
            Bezier(b);
        }
    }
    FinishAndCloseFile();
}

void VectorFileWriter::BezierAsPwl(SBezier *sb) {
    List<Vector> lv;
    ZERO(&lv);
    sb->MakePwlInto(&lv);
    int i;
    for(i = 1; i < lv.n; i++) {
        LineSegment(lv.elem[i-1].x, lv.elem[i-1].y,
                    lv.elem[i  ].x, lv.elem[i  ].y);
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
// Export the mesh as an STL file; it should always be vertex-to-vertex and
// not self-intersecting, so not much to do.
//-----------------------------------------------------------------------------
void SolveSpace::ExportMeshTo(char *filename) {
    SMesh *m = &(SK.GetGroup(SS.GW.activeGroup)->displayMesh);
    if(m->IsEmpty()) {
        Error("Active group mesh is empty; nothing to export.");
        return;
    }

    FILE *f = fopen(filename, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", filename);
        return;
    }
    char str[80];
    memset(str, 0, sizeof(str));
    strcpy(str, "STL exported mesh");
    fwrite(str, 1, 80, f);

    DWORD n = m->l.n;
    fwrite(&n, 4, 1, f);

    double s = SS.exportScale;
    int i;
    for(i = 0; i < m->l.n; i++) {
        STriangle *tr = &(m->l.elem[i]);
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

    fclose(f);
}

//-----------------------------------------------------------------------------
// Export a view of the model as an image; we just take a screenshot, by
// rendering the view in the usual way and then copying the pixels.
//-----------------------------------------------------------------------------
void SolveSpace::ExportAsPngTo(char *filename) {
    int w = (int)SS.GW.width, h = (int)SS.GW.height;
    // No guarantee that the back buffer contains anything valid right now,
    // so repaint the scene. And hide the toolbar too.
    int prevShowToolbar = SS.showToolbar;
    SS.showToolbar = false;
    SS.GW.Paint(w, h);
    SS.showToolbar = prevShowToolbar;
    
    FILE *f = fopen(filename, "wb");
    if(!f) goto err;

    png_struct *png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
        NULL, NULL, NULL);
    if(!png_ptr) goto err;

    png_info *info_ptr = png_create_info_struct(png_ptr);
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
    BYTE *pixels = (BYTE *)AllocTemporary(3*w*h);
    BYTE **rowptrs = (BYTE **)AllocTemporary(h*sizeof(BYTE *));
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
    Error("Error writing PNG file '%s'", filename);
    if(f) fclose(f);
    return;
}

