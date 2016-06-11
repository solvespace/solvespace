//-----------------------------------------------------------------------------
// The 2d vector output stuff that isn't specific to any particular file
// format: getting the appropriate lines and curves, performing hidden line
// removal, calculating bounding boxes, and so on. Also raster and triangle
// mesh output.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#ifndef WIN32
#include <unix/gloffscreen.h>
#endif
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

    VectorFileWriter *out = VectorFileWriter::ForFile(filename);
    if(!out) return;

    SS.exportMode = true;
    GenerateAll(GENERATE_ALL);

    SMesh *sm = NULL;
    if(SS.GW.showShaded || SS.GW.showHdnLines) {
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

        if(SS.exportPwlCurves || sm || fabs(SS.exportOffset) > LENGTH_EPS)
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
        if(!out->OutputConstraints(&SK.constraint)) {
            // The output format cannot represent constraints directly,
            // so convert them to edges.
            Constraint *c;
            for(c = SK.constraint.First(); c; c = SK.constraint.NextAfter(c)) {
                c->GetEdges(&edges);
            }
        }
    }

    if(wireframe) {
        Vector u = Vector::From(1.0, 0.0, 0.0),
               v = Vector::From(0.0, 1.0, 0.0),
               n = Vector::From(0.0, 0.0, 1.0),
               origin = Vector::From(0.0, 0.0, 0.0);
        double cameraTan = 0.0,
               scale = 1.0;

        out->SetModelviewProjection(u, v, n, origin, cameraTan, scale);

        ExportWireframeCurves(&edges, &beziers, out);
    } else {
        Vector u = SS.GW.projRight,
               v = SS.GW.projUp,
               n = u.Cross(v),
               origin = SS.GW.offset.ScaledBy(-1);

        out->SetModelviewProjection(u, v, n, origin,
                                    SS.CameraTangent()*SS.GW.scale, SS.exportScale);

        ExportLinesAndMesh(&edges, &beziers, sm,
                           u, v, n, origin, SS.CameraTangent()*SS.GW.scale,
                           out);

        if(!out->HasCanvasSize()) {
            // These file formats don't have a canvas size, so they just
            // get exported in the raw coordinate system. So indicate what
            // that was on-screen.
            SS.justExportedInfo.showOrigin = true;
            SS.justExportedInfo.pt = origin;
            SS.justExportedInfo.u = u;
            SS.justExportedInfo.v = v;
        } else {
            SS.justExportedInfo.showOrigin = false;
        }

        SS.justExportedInfo.draw = true;
        InvalidateGraphics();
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

    out->OutputLinesAndMesh(&sblss, NULL);
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

    SMesh sms = {};

    // We need the mesh for occlusion testing, but if we don't/can't export it,
    // don't generate it.
    if(SS.GW.showShaded && out->CanOutputMesh()) {
        // Use the BSP routines to generate the split triangles in paint order.
        SBsp3 *bsp = SBsp3::FromMesh(&smp);
        if(bsp) bsp->GenerateInPaintOrder(&sms);
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
    }

    // And now we perform hidden line removal if requested
    SEdgeList hlrd = {};
    if(sm) {
        SKdNode *root = SKdNode::From(&smp);

        // Generate the edges where a curved surface turns from front-facing
        // to back-facing.
        if(SS.GW.showEdges) {
            root->MakeCertainEdgesInto(sel, SKdNode::TURNING_EDGES,
                                       /*coplanarIsInter=*/false, NULL, NULL,
                                       GW.showOutlines ? Style::OUTLINE : Style::SOLID_EDGE);
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

            SEdgeList edges = {};
            // Split the original edge against the mesh
            edges.AddEdge(se->a, se->b, se->auxA);
            root->OcclusionTestLine(*se, &edges, cnt, /*removeHidden=*/!SS.GW.showHdnLines);
            // the occlusion test splits unnecessarily; so fix those
            edges.MergeCollinearSegments(se->a, se->b);
            cnt++;
            // And add the results to our output
            SEdge *sen;
            for(sen = edges.l.First(); sen; sen = edges.l.NextAfter(sen)) {
                hlrd.AddEdge(sen->a, sen->b, sen->auxA);
            }
            edges.Clear();
        }

        sel = &hlrd;
    }

    // Clean up: remove overlapping line segments and
    // segments with zero-length projections.
    sel->l.ClearTags();
    for(int i = 0; i < sel->l.n; ++i) {
        SEdge *sei = &sel->l.elem[i];
        hStyle hsi = { (uint32_t)sei->auxA };
        Style *si = Style::Get(hsi);
        if(sei->tag != 0) continue;

        // Remove segments with zero length projections.
        Vector ai = sei->a;
        ai.z = 0.0;
        Vector bi = sei->b;
        bi.z = 0.0;
        Vector di = bi.Minus(ai);
        if(fabs(di.x) < LENGTH_EPS && fabs(di.y) < LENGTH_EPS) {
            sei->tag = 1;
            continue;
        }

        for(int j = i + 1; j < sel->l.n; ++j) {
            SEdge *sej = &sel->l.elem[j];
            if(sej->tag != 0) continue;

            Vector *pAj = &sej->a;
            Vector *pBj = &sej->b;

            // Remove segments with zero length projections.
            Vector aj = sej->a;
            aj.z = 0.0;
            Vector bj = sej->b;
            bj.z = 0.0;
            Vector dj = bj.Minus(aj);
            if(fabs(dj.x) < LENGTH_EPS && fabs(dj.y) < LENGTH_EPS) {
                sej->tag = 1;
                continue;
            }

            // Skip non-collinear segments.
            const double eps = 1e-6;
            if(aj.DistanceToLine(ai, di) > eps) continue;
            if(bj.DistanceToLine(ai, di) > eps) continue;

            double ta = aj.Minus(ai).Dot(di) / di.Dot(di);
            double tb = bj.Minus(ai).Dot(di) / di.Dot(di);
            if(ta > tb) {
                std::swap(pAj, pBj);
                std::swap(ta, tb);
            }

            hStyle hsj = { (uint32_t)sej->auxA };
            Style *sj = Style::Get(hsj);

            bool canRemoveI = sej->auxA == sei->auxA || si->zIndex < sj->zIndex;
            bool canRemoveJ = sej->auxA == sei->auxA || sj->zIndex < si->zIndex;

            if(canRemoveJ) {
                // j-segment inside i-segment
                if(ta > 0.0 - eps && tb < 1.0 + eps) {
                    sej->tag = 1;
                    continue;
                }

                // cut segment
                bool aInside = ta > 0.0 - eps && ta < 1.0 + eps;
                if(tb > 1.0 - eps && aInside) {
                    *pAj = sei->b;
                    continue;
                }

                // cut segment
                bool bInside = tb > 0.0 - eps && tb < 1.0 + eps;
                if(ta < 0.0 - eps && bInside) {
                    *pBj = sei->a;
                    continue;
                }

                // split segment
                if(ta < 0.0 - eps && tb > 1.0 + eps) {
                    sel->AddEdge(sei->b, *pBj, sej->auxA, sej->auxB);
                    *pBj = sei->a;
                    continue;
                }
            }

            if(canRemoveI) {
                // j-segment inside i-segment
                if(ta < 0.0 + eps && tb > 1.0 - eps) {
                    sei->tag = 1;
                    break;
                }

                // cut segment
                bool aInside = ta > 0.0 + eps && ta < 1.0 - eps;
                if(tb > 1.0 - eps && aInside) {
                    sei->b = *pAj;
                    i--;
                    break;
                }

                // cut segment
                bool bInside = tb > 0.0 + eps && tb < 1.0 - eps;
                if(ta < 0.0 + eps && bInside) {
                    sei->a = *pBj;
                    i--;
                    break;
                }

                // split segment
                if(ta > 0.0 + eps && tb < 1.0 - eps) {
                    sel->AddEdge(*pBj, sei->b, sei->auxA, sei->auxB);
                    sei->b = *pAj;
                    i--;
                    break;
                }
            }
        }
    }
    sel->l.RemoveTagged();

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
                             SS.ExportChordTolMm(),
                             &allClosed, &notClosedAt,
                             NULL, NULL,
                             &leftovers);
    for(b = leftovers.l.First(); b; b = leftovers.l.NextAfter(b)) {
        sblss.AddOpenPath(b);
    }

    // Now write the lines and triangles to the output file
    out->OutputLinesAndMesh(&sblss, &sms);

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
    bool needOpen = true;
    if(FilenameHasExtension(filename, ".dxf")) {
        static DxfFileWriter DxfWriter;
        ret = &DxfWriter;
        needOpen = false;
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
    ret->filename = filename;
    if(!needOpen) return ret;

    FILE *f = ssfopen(filename, "wb");
    if(!f) {
        Error("Couldn't write to '%s'", filename.c_str());
        return NULL;
    }
    ret->f = f;
    return ret;
}

void VectorFileWriter::SetModelviewProjection(const Vector &u, const Vector &v, const Vector &n,
                                              const Vector &origin, double cameraTan,
                                              double scale) {
    this->u = u;
    this->v = v;
    this->n = n;
    this->origin = origin;
    this->cameraTan = cameraTan;
    this->scale = scale;
}

Vector VectorFileWriter::Transform(Vector &pos) const {
    return pos.InPerspective(u, v, n, origin, cameraTan).ScaledBy(1.0 / scale);
}

void VectorFileWriter::OutputLinesAndMesh(SBezierLoopSetSet *sblss, SMesh *sm) {
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

            StartPath(strokeRgb, lineWidth, stl->filled, fillRgb, hs);
            for(sbl = sbls->l.First(); sbl; sbl = sbls->l.NextAfter(sbl)) {
                for(b = sbl->l.First(); b; b = sbl->l.NextAfter(b)) {
                    Bezier(b);
                }
            }
            FinishPath(strokeRgb, lineWidth, stl->filled, fillRgb, hs);
        }
    }
    FinishAndCloseFile();
}

void VectorFileWriter::BezierAsPwl(SBezier *sb) {
    List<Vector> lv = {};
    sb->MakePwlInto(&lv, SS.ExportChordTolMm());
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

    double tol = SS.ExportChordTolMm();
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
    SS.exportMode = true;
    GenerateAll(GENERATE_ALL);

    Group *g = SK.GetGroup(SS.GW.activeGroup);
    g->GenerateDisplayItems();

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
    } else if(FilenameHasExtension(filename, ".js") ||
              FilenameHasExtension(filename, ".html")) {
        SEdgeList *e = &(SK.GetGroup(SS.GW.activeGroup)->displayEdges);
        ExportMeshAsThreeJsTo(f, filename, m, e);
    } else {
        Error("Can't identify output file type from file extension of "
              "filename '%s'; try .stl, .obj, .js.", filename.c_str());
    }

    fclose(f);

    SS.justExportedInfo.showOrigin = false;
    SS.justExportedInfo.draw = true;
    InvalidateGraphics();
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
void SolveSpaceUI::ExportMeshAsThreeJsTo(FILE *f, const std::string &filename,
                                         SMesh *sm, SEdgeList *sel)
{
    SPointList spl = {};
    STriangle *tr;
    SEdge *e;
    Vector bndl, bndh;
    const char htmlbegin0[] = R"(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8"></meta>
    <title>Three.js Solvespace Mesh</title>
    <script src="http://threejs.org/build/three.js"></script>
    <script src="http://hammerjs.github.io/dist/hammer.js"></script>
    <style type="text/css">
    body { margin: 0; overflow: hidden; }
    </style>
  </head>
  <body>
    <script>
    </script>
    <script>
window.devicePixelRatio = window.devicePixelRatio || 1;

SolvespaceCamera = function(renderWidth, renderHeight, scale, up, right, offset) {
        THREE.Camera.call(this);

        this.type = 'SolvespaceCamera';

        this.renderWidth = renderWidth;
        this.renderHeight = renderHeight;
        this.zoomScale = scale; /* Avoid namespace collision w/ THREE.Object.scale */
        this.up = up;
        this.right = right;
        this.offset = offset;
        this.depthBias = 0;

        this.updateProjectionMatrix();
    };

    SolvespaceCamera.prototype = Object.create(THREE.Camera.prototype);
    SolvespaceCamera.prototype.constructor = SolvespaceCamera;
    SolvespaceCamera.prototype.updateProjectionMatrix = function() {
        var temp = new THREE.Matrix4();
        var offset = new THREE.Matrix4().makeTranslation(this.offset.x, this.offset.y, this.offset.z);
        // Convert to right handed- do up cross right instead.
        var n = new THREE.Vector3().crossVectors(this.up, this.right);
        var rotate = new THREE.Matrix4().makeBasis(this.right, this.up, n);
        rotate.transpose();
        /* FIXME: At some point we ended up using row-major.
           THREE.js wants column major. Scale/depth correct unaffected b/c diagonal
           matrices remain the same when transposed. makeTranslation also makes
           a column-major matrix. */

        /* TODO: If we want perspective, we need an additional matrix
           here which will modify w for perspective divide. */
        var scale = new THREE.Matrix4().makeScale(2 * this.zoomScale / this.renderWidth,
            2 * this.zoomScale / this.renderHeight, this.zoomScale / 30000.0);

        temp.multiply(scale);
        temp.multiply(rotate);
        temp.multiply(offset);

        this.projectionMatrix.copy(temp);
    };

    SolvespaceCamera.prototype.NormalizeProjectionVectors = function() {
        /* After rotating, up and right may no longer be orthogonal.
        However, their cross product will produce the correct
        rotated plane, and we can recover an orthogonal basis. */
        var n = new THREE.Vector3().crossVectors(this.right, this.up);
        this.up = new THREE.Vector3().crossVectors(n, this.right);
        this.right.normalize();
        this.up.normalize();
    };

    SolvespaceCamera.prototype.rotate = function(right, up) {
        var oldRight = new THREE.Vector3().copy(this.right).normalize();
        var oldUp = new THREE.Vector3().copy(this.up).normalize();
        this.up.applyAxisAngle(oldRight, up);
        this.right.applyAxisAngle(oldUp, right);
        this.NormalizeProjectionVectors();
    }

    SolvespaceCamera.prototype.offsetProj = function(right, up) {
        var shift = new THREE.Vector3(right * this.right.x + up * this.up.x,
            right * this.right.y + up * this.up.y,
            right * this.right.z + up * this.up.z);
        this.offset.add(shift);
    }

    /* Calculate the offset in terms of up and right projection vectors
    that will preserve the world coordinates of the current mouse position after
    the zoom. */
    SolvespaceCamera.prototype.zoomTo = function(x, y, delta) {
        // Get offset components in world coordinates, in terms of up/right.
        var projOffsetX = this.offset.dot(this.right);
        var projOffsetY = this.offset.dot(this.up);

        /* Remove offset before scaling so, that mouse position changes
        proportionally to the model and independent of current offset. */
        var centerRightI = x/this.zoomScale - projOffsetX;
        var centerUpI = y/this.zoomScale - projOffsetY;
        var zoomFactor;

        /* Zoom 20% every 100 delta. */
        if(delta < 0) {
            zoomFactor = (-delta * 0.002 + 1);
        }
        else if(delta > 0) {
            zoomFactor = (delta * (-1.0/600.0) + 1)
        }
        else {
            return;
        }

        this.zoomScale = this.zoomScale * zoomFactor;
        var centerRightF = x/this.zoomScale - projOffsetX;
        var centerUpF = y/this.zoomScale - projOffsetY;

        this.offset.addScaledVector(this.right, centerRightF - centerRightI);
        this.offset.addScaledVector(this.up, centerUpF - centerUpI);
    }


    SolvespaceControls = function(object, domElement) {
        var _this = this;
        this.object = object;
        this.domElement = ( domElement !== undefined ) ? domElement : document;

        var threePan = new Hammer.Pan({event : 'threepan', pointers : 3, enable : false});
        var panAfterTap = new Hammer.Pan({event : 'panaftertap', enable : false});

        this.touchControls = new Hammer.Manager(domElement, {
            recognizers: [
                [Hammer.Pinch, { enable: true }],
                [Hammer.Pan],
                [Hammer.Tap],
            ]
        });

        this.touchControls.add(threePan);
        this.touchControls.add(panAfterTap);

        var changeEvent = {
            type: 'change'
        };
        var startEvent = {
            type: 'start'
        };
        var endEvent = {
            type: 'end'
        };

        var _changed = false;
        var _mouseMoved = false;
        //var _touchPoints = new Array();
        var _offsetPrev = new THREE.Vector2(0, 0);
        var _offsetCur = new THREE.Vector2(0, 0);
        var _rotatePrev = new THREE.Vector2(0, 0);
        var _rotateCur = new THREE.Vector2(0, 0);

        // Used during touch events.
        var _rotateOrig = new THREE.Vector2(0, 0);
        var _offsetOrig = new THREE.Vector2(0, 0);
        var _prevScale = 1.0;

        this.handleEvent = function(event) {
            if (typeof this[event.type] == 'function') {
                this[event.type](event);
            }
        }

        function mousedown(event) {
            event.preventDefault();
            event.stopPropagation();

            switch (event.button) {
                case 0:
                    _rotateCur.set(event.screenX/window.devicePixelRatio, event.screenY/window.devicePixelRatio);
                    _rotatePrev.copy(_rotateCur);
                    document.addEventListener('mousemove', mousemove, false);
                    document.addEventListener('mouseup', mouseup, false);
                    break;
                case 2:
                    _offsetCur.set(event.screenX/window.devicePixelRatio, event.screenY/window.devicePixelRatio);
                    _offsetPrev.copy(_offsetCur);
                    document.addEventListener('mousemove', mousemove, false);
                    document.addEventListener('mouseup', mouseup, false);
                    break;
                default:
                    break;
            }
        }

        function wheel( event ) {
            event.preventDefault();
            /* FIXME: Width and height might not be supported universally, but
            can be calculated? */
            var box = _this.domElement.getBoundingClientRect();
            object.zoomTo(event.clientX - box.width/2 - box.left,
                 -(event.clientY - box.height/2 - box.top), event.deltaY);
            _changed = true;
        }

        function mousemove(event) {
            switch (event.button) {
                case 0:
                    _rotateCur.set(event.screenX/window.devicePixelRatio, event.screenY/window.devicePixelRatio);
                    var diff = new THREE.Vector2().subVectors(_rotateCur, _rotatePrev)
                        .multiplyScalar(1 / object.zoomScale);
                    object.rotate(-0.3 * Math.PI / 180 * diff.x * object.zoomScale,
                         -0.3 * Math.PI / 180 * diff.y * object.zoomScale);
                    _changed = true;
                    _rotatePrev.copy(_rotateCur);
                    break;
                case 2:
                    _mouseMoved = true;
                    _offsetCur.set(event.screenX/window.devicePixelRatio, event.screenY/window.devicePixelRatio);
                    var diff = new THREE.Vector2().subVectors(_offsetCur, _offsetPrev)
                        .multiplyScalar(1 / object.zoomScale);
                    object.offsetProj(diff.x, -diff.y);
                    _changed = true;
                    _offsetPrev.copy(_offsetCur)
                    break;
            }
        }


        function mouseup(event) {
            /* TODO: Opera mouse gestures will intercept this event, making it
            possible to have multiple mousedown events consecutively without
            a corresponding mouseup (so multiple viewports can be rotated/panned
            simultaneously). Disable mouse gestures for now. */
            event.preventDefault();
            event.stopPropagation();

            document.removeEventListener('mousemove', mousemove);
            document.removeEventListener('mouseup', mouseup);

            _this.dispatchEvent(endEvent);
        }

        function pan(event) {
            /* neWcur - prev does not necessarily equal (cur + diff) - prev.
            Floating point is not associative. */
            touchDiff = new THREE.Vector2(event.deltaX, event.deltaY);
            _rotateCur.addVectors(_rotateOrig, touchDiff);
            incDiff = new THREE.Vector2().subVectors(_rotateCur, _rotatePrev)
                .multiplyScalar(1 / object.zoomScale);
            object.rotate(-0.3 * Math.PI / 180 * incDiff.x * object.zoomScale,
                 -0.3 * Math.PI / 180 * incDiff.y * object.zoomScale);
            _changed = true;
            _rotatePrev.copy(_rotateCur);
        }

        function panstart(event) {
            /* TODO: Dynamically enable pan function? */
            _rotateOrig.copy(_rotateCur);
        }

        function pinchstart(event) {
            _prevScale = event.scale;
        }

        function pinch(event) {
            /* FIXME: Width and height might not be supported universally, but
            can be calculated? */
            var box = _this.domElement.getBoundingClientRect();

            /* 16.6... pixels chosen heuristically... matches my touchpad. */
            if (event.scale < _prevScale) {
                object.zoomTo(event.center.x - box.width/2 - box.left,
                     -(event.center.y - box.height/2 - box.top), 100/6.0);
                _changed = true;
            } else if (event.scale > _prevScale) {
                object.zoomTo(event.center.x - box.width/2 - box.left,
                     -(event.center.y - box.height/2 - box.top), -100/6.0);
                _changed = true;
            }

            _prevScale = event.scale;
        }

        /* A tap will enable panning/disable rotate. */
        function tap(event) {
            panAfterTap.set({enable : true});
            _this.touchControls.get('pan').set({enable : false});
        }

        function panaftertap(event) {
            touchDiff = new THREE.Vector2(event.deltaX, event.deltaY);
            _offsetCur.addVectors(_offsetOrig, touchDiff);
            incDiff = new THREE.Vector2().subVectors(_offsetCur, _offsetPrev)
                .multiplyScalar(1 / object.zoomScale);
            object.offsetProj(incDiff.x, -incDiff.y);
            _changed = true;
            _offsetPrev.copy(_offsetCur);
        }

        function panaftertapstart(event) {
            _offsetOrig.copy(_offsetCur);
        }

        function panaftertapend(event) {
            panAfterTap.set({enable : false});
            _this.touchControls.get('pan').set({enable : true});
        }

        function contextmenu(event) {
            event.preventDefault();
        }

        this.update = function() {
            if (_changed) {
                _this.dispatchEvent(changeEvent);
                _changed = false;
            }
        }

        this.domElement.addEventListener('mousedown', mousedown, false);
        this.domElement.addEventListener('wheel', wheel, false);
        this.domElement.addEventListener('contextmenu', contextmenu, false);

        /* Hammer.on wraps addEventListener */
        // Rotate
        this.touchControls.on('pan', pan);
        this.touchControls.on('panstart', panstart);

        // Zoom
        this.touchControls.on('pinch', pinch);
        this.touchControls.on('pinchstart', pinchstart);

        //Pan
        this.touchControls.on('tap', tap);
        this.touchControls.on('panaftertapstart', panaftertapstart);
        this.touchControls.on('panaftertap', panaftertap);
        this.touchControls.on('panaftertapend', panaftertapend);
    }

    SolvespaceControls.prototype = Object.create(THREE.EventDispatcher.prototype);
    SolvespaceControls.prototype.constructor = SolvespaceControls;


    solvespace = function(obj, params) {
        var scene, edgeScene, camera, edgeCamera, renderer;
        var geometry, controls, material, mesh, edges;
        var width, height;
        var directionalLightArray = [];

        if (typeof params === "undefined" || !("width" in params)) {
            width = window.innerWidth;
        } else {
            width = params.width;
        }

        if (typeof params === "undefined" || !("height" in params)) {
            height = window.innerHeight;
        } else {
            height = params.height;
        }

        width *= window.devicePixelRatio;
        height *= window.devicePixelRatio;

        domElement = init();
        render();
        return domElement;


        function init() {
            scene = new THREE.Scene();
            edgeScene = new THREE.Scene();

            camera = new SolvespaceCamera(width/window.devicePixelRatio,
                height/window.devicePixelRatio, 5, new THREE.Vector3(0, 1, 0),
                new THREE.Vector3(1, 0, 0), new THREE.Vector3(0, 0, 0));

            mesh = createMesh(obj);
            scene.add(mesh);
            edges = createEdges(obj);
            edgeScene.add(edges);

            for (var i = 0; i < obj.lights.d.length; i++) {
                var lightColor = new THREE.Color(obj.lights.d[i].intensity,
                    obj.lights.d[i].intensity, obj.lights.d[i].intensity);
                var directionalLight = new THREE.DirectionalLight(lightColor, 1);
                directionalLight.position.set(obj.lights.d[i].direction[0],
                    obj.lights.d[i].direction[1], obj.lights.d[i].direction[2]);
                directionalLightArray.push(directionalLight);
                scene.add(directionalLight);
            }

            var lightColor = new THREE.Color(obj.lights.a, obj.lights.a, obj.lights.a);
            var ambientLight = new THREE.AmbientLight(lightColor.getHex());
            scene.add(ambientLight);

            renderer = new THREE.WebGLRenderer({ antialias: true});
            renderer.setSize(width, height);
            renderer.autoClear = false;
            renderer.domElement.style = "width:"+width/window.devicePixelRatio+"px;height:"+height/window.devicePixelRatio+"px;";

            controls = new SolvespaceControls(camera, renderer.domElement);
            controls.addEventListener("change", render);
            controls.addEventListener("change", lightUpdate);

            animate();
            return renderer.domElement;
        })";
    const char htmlbegin1[] = R"(
        function animate() {
            requestAnimationFrame(animate);
            controls.update();
        }

        function render() {
            var context = renderer.getContext();
            camera.updateProjectionMatrix();
            renderer.clear();

            context.depthRange(0.1, 1);
            renderer.render(scene, camera);

            context.depthRange(0.1-(2/60000.0), 1-(2/60000.0));
            renderer.render(edgeScene, camera);
        }

        function lightUpdate() {
            var changeBasis = new THREE.Matrix4();

            // The original light positions were in camera space.
            // Project them into standard space using camera's basis
            // vectors (up, target, and their cross product).
            n = new THREE.Vector3().crossVectors(camera.up, camera.right);
            changeBasis.makeBasis(camera.right, camera.up, n);

            for (var i = 0; i < 2; i++) {
                var newLightPos = changeBasis.applyToVector3Array(
                    [obj.lights.d[i].direction[0], obj.lights.d[i].direction[1],
                        obj.lights.d[i].direction[2]]);
                directionalLightArray[i].position.set(newLightPos[0],
                    newLightPos[1], newLightPos[2]);
            }
        }

        function createMesh(meshObj) {
            var geometry = new THREE.Geometry();
            var materialIndex = 0;
            var materialList = [];
            var opacitiesSeen = {};

            for (var i = 0; i < meshObj.points.length; i++) {
                geometry.vertices.push(new THREE.Vector3(meshObj.points[i][0],
                    meshObj.points[i][1], meshObj.points[i][2]));
            }

            for (var i = 0; i < meshObj.faces.length; i++) {
                var currOpacity = ((meshObj.colors[i] & 0xFF000000) >>> 24) / 255.0;
                if (opacitiesSeen[currOpacity] === undefined) {
                    opacitiesSeen[currOpacity] = materialIndex;
                    materialIndex++;
                    materialList.push(new THREE.MeshLambertMaterial({
                        vertexColors: THREE.FaceColors,
                        opacity: currOpacity,
                        transparent: true,
                        side: THREE.DoubleSide
                    }));
                }

                geometry.faces.push(new THREE.Face3(meshObj.faces[i][0],
                    meshObj.faces[i][1], meshObj.faces[i][2],
                    [new THREE.Vector3(meshObj.normals[i][0][0],
                        meshObj.normals[i][0][1], meshObj.normals[i][0][2]),
                     new THREE.Vector3(meshObj.normals[i][1][0],
                        meshObj.normals[i][1][1], meshObj.normals[i][1][2]),
                     new THREE.Vector3(meshObj.normals[i][2][0],
                        meshObj.normals[i][2][1], meshObj.normals[i][2][2])],
                    new THREE.Color(meshObj.colors[i] & 0x00FFFFFF),
                    opacitiesSeen[currOpacity]));
            }

            geometry.computeBoundingSphere();
            return new THREE.Mesh(geometry, new THREE.MultiMaterial(materialList));
        }

        function createEdges(meshObj) {
            var geometry = new THREE.Geometry();
            var material = new THREE.LineBasicMaterial();

            for (var i = 0; i < meshObj.edges.length; i++) {
                geometry.vertices.push(new THREE.Vector3(meshObj.edges[i][0][0],
                        meshObj.edges[i][0][1], meshObj.edges[i][0][2]),
                    new THREE.Vector3(meshObj.edges[i][1][0],
                        meshObj.edges[i][1][1], meshObj.edges[i][1][2]));
            }

            geometry.computeBoundingSphere();
            return new THREE.LineSegments(geometry, material);
        }
    };
)";
    const char htmlend[] = R"(
    document.body.appendChild(solvespace(solvespace_model_%s));
    </script>
  </body>
</html>
)";

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

    std::string extension = filename,
                noExtFilename = filename;
    size_t dot = noExtFilename.rfind('.');
    extension.erase(0, dot + 1);
    noExtFilename.erase(dot);

    std::string baseFilename = noExtFilename;
    size_t lastSlash = baseFilename.rfind(PATH_SEP);
    if(lastSlash == std::string::npos) oops();
    baseFilename.erase(0, lastSlash + 1);

    for(size_t i = 0; i < baseFilename.length(); i++) {
        if(!isalpha(baseFilename[i]) &&
           /* also permit UTF-8 */ !((unsigned char)baseFilename[i] >= 0x80))
            baseFilename[i] = '_';
    }

    if(extension == "html") {
        fputs(htmlbegin0, f);
        fputs(htmlbegin1, f);
    }

    fprintf(f, "var solvespace_model_%s = {\n"
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
                e->a.x / SS.exportScale,
                e->a.y / SS.exportScale,
                e->a.z / SS.exportScale,
                e->b.x / SS.exportScale,
                e->b.y / SS.exportScale,
                e->b.z / SS.exportScale);
    }

    fputs("  ]\n};\n", f);

    if(extension == "html")
        fprintf(f, htmlend, baseFilename.c_str());

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
#ifndef WIN32
    std::unique_ptr<GLOffscreen> gloffscreen(new GLOffscreen);
    gloffscreen->begin(w, h);
#endif
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

