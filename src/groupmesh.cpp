//-----------------------------------------------------------------------------
// Routines to generate our watertight brep shells from the operations
// and entities specified by the user in each group; templated to work either
// on an SShell of ratpoly surfaces or on an SMesh of triangles.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#define gs (SS.GW.gs)

void Group::AssembleLoops(bool *allClosed,
                          bool *allCoplanar,
                          bool *allNonZeroLen)
{
    SBezierList sbl = {};

    int i;
    for(i = 0; i < SK.entity.n; i++) {
        Entity *e = &(SK.entity.elem[i]);
        if(e->group.v != h.v) continue;
        if(e->construction) continue;
        if(e->forceHidden) continue;

        e->GenerateBezierCurves(&sbl);
    }

    SBezier *sb;
    *allNonZeroLen = true;
    for(sb = sbl.l.First(); sb; sb = sbl.l.NextAfter(sb)) {
        for(i = 1; i <= sb->deg; i++) {
            if(!(sb->ctrl[i]).Equals(sb->ctrl[0])) {
                break;
            }
        }
        if(i > sb->deg) {
            // This is a zero-length edge.
            *allNonZeroLen = false;
            polyError.errorPointAt = sb->ctrl[0];
            goto done;
        }
    }

    // Try to assemble all these Beziers into loops. The closed loops go into
    // bezierLoops, with the outer loops grouped with their holes. The
    // leftovers, if any, go in bezierOpens.
    bezierLoops.FindOuterFacesFrom(&sbl, &polyLoops, NULL,
                                   SS.ChordTolMm(),
                                   allClosed, &(polyError.notClosedAt),
                                   allCoplanar, &(polyError.errorPointAt),
                                   &bezierOpens);
    done:
    sbl.Clear();
}

void Group::GenerateLoops() {
    polyLoops.Clear();
    bezierLoops.Clear();
    bezierOpens.Clear();

    if(type == Type::DRAWING_3D || type == Type::DRAWING_WORKPLANE ||
       type == Type::ROTATE || type == Type::TRANSLATE || type == Type::LINKED)
    {
        bool allClosed = false, allCoplanar = false, allNonZeroLen = false;
        AssembleLoops(&allClosed, &allCoplanar, &allNonZeroLen);
        if(!allNonZeroLen) {
            polyError.how = PolyError::ZERO_LEN_EDGE;
        } else if(!allCoplanar) {
            polyError.how = PolyError::NOT_COPLANAR;
        } else if(!allClosed) {
            polyError.how = PolyError::NOT_CLOSED;
        } else {
            polyError.how = PolyError::GOOD;
            // The self-intersecting check is kind of slow, so don't run it
            // unless requested.
            if(SS.checkClosedContour) {
                if(polyLoops.SelfIntersecting(&(polyError.errorPointAt))) {
                    polyError.how = PolyError::SELF_INTERSECTING;
                }
            }
        }
    }
}

void SShell::RemapFaces(Group *g, int remap) {
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)){
        hEntity face = { ss->face };
        if(face.v == Entity::NO_ENTITY.v) continue;

        face = g->Remap(face, remap);
        ss->face = face.v;
    }
}

void SMesh::RemapFaces(Group *g, int remap) {
    STriangle *tr;
    for(tr = l.First(); tr; tr = l.NextAfter(tr)) {
        hEntity face = { tr->meta.face };
        if(face.v == Entity::NO_ENTITY.v) continue;

        face = g->Remap(face, remap);
        tr->meta.face = face.v;
    }
}

template<class T>
void Group::GenerateForStepAndRepeat(T *steps, T *outs) {
    T workA, workB;
    workA = {};
    workB = {};
    T *soFar = &workA, *scratch = &workB;

    int n = (int)valA, a0 = 0;
    if(subtype == Subtype::ONE_SIDED && skipFirst) {
        a0++; n++;
    }
    int a;
    for(a = a0; a < n; a++) {
        int ap = a*2 - (subtype == Subtype::ONE_SIDED ? 0 : (n-1));
        int remap = (a == (n - 1)) ? REMAP_LAST : a;

        T transd = {};
        if(type == Type::TRANSLATE) {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            trans = trans.ScaledBy(ap);
            transd.MakeFromTransformationOf(steps,
                trans, Quaternion::IDENTITY, 1.0);
        } else {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            double theta = ap * SK.GetParam(h.param(3))->val;
            double c = cos(theta), s = sin(theta);
            Vector axis = Vector::From(h.param(4), h.param(5), h.param(6));
            Quaternion q = Quaternion::From(c, s*axis.x, s*axis.y, s*axis.z);
            // Rotation is centered at t; so A(x - t) + t = Ax + (t - At)
            transd.MakeFromTransformationOf(steps,
                trans.Minus(q.Rotate(trans)), q, 1.0);
        }

        // We need to rewrite any plane face entities to the transformed ones.
        transd.RemapFaces(this, remap);

        // And tack this transformed copy on to the return.
        if(soFar->IsEmpty()) {
            scratch->MakeFromCopyOf(&transd);
        } else {
            scratch->MakeFromUnionOf(soFar, &transd);
        }

        swap(scratch, soFar);
        scratch->Clear();
        transd.Clear();
    }

    outs->Clear();
    *outs = *soFar;
}

template<class T>
void Group::GenerateForBoolean(T *prevs, T *thiss, T *outs, Group::CombineAs how) {
    // If this group contributes no new mesh, then our running mesh is the
    // same as last time, no combining required. Likewise if we have a mesh
    // but it's suppressed.
    if(thiss->IsEmpty() || suppress) {
        outs->MakeFromCopyOf(prevs);
        return;
    }

    // So our group's shell appears in thisShell. Combine this with the
    // previous group's shell, using the requested operation.
    if(how == CombineAs::UNION) {
        outs->MakeFromUnionOf(prevs, thiss);
    } else if(how == CombineAs::DIFFERENCE) {
        outs->MakeFromDifferenceOf(prevs, thiss);
    } else {
        outs->MakeFromAssemblyOf(prevs, thiss);
    }
}

void Group::GenerateShellAndMesh() {
    bool prevBooleanFailed = booleanFailed;
    booleanFailed = false;

    Group *srcg = this;

    thisShell.Clear();
    thisMesh.Clear();
    runningShell.Clear();
    runningMesh.Clear();

    // Don't attempt a lathe or extrusion unless the source section is good:
    // planar and not self-intersecting.
    bool haveSrc = true;
    if(type == Type::EXTRUDE || type == Type::LATHE) {
        Group *src = SK.GetGroup(opA);
        if(src->polyError.how != PolyError::GOOD) {
            haveSrc = false;
        }
    }

    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        // A step and repeat gets merged against the group's prevous group,
        // not our own previous group.
        srcg = SK.GetGroup(opA);

        GenerateForStepAndRepeat<SShell>(&(srcg->thisShell), &thisShell);
        GenerateForStepAndRepeat<SMesh> (&(srcg->thisMesh),  &thisMesh);
    } else if(type == Type::EXTRUDE && haveSrc) {
        Group *src = SK.GetGroup(opA);
        Vector translate = Vector::From(h.param(0), h.param(1), h.param(2));

        Vector tbot, ttop;
        if(subtype == Subtype::ONE_SIDED) {
            tbot = Vector::From(0, 0, 0); ttop = translate.ScaledBy(2);
        } else {
            tbot = translate.ScaledBy(-1); ttop = translate.ScaledBy(1);
        }

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            int is = thisShell.surface.n;
            // Extrude this outer contour (plus its inner contours, if present)
            thisShell.MakeFromExtrusionOf(sbls, tbot, ttop, color);

            // And for any plane faces, annotate the model with the entity for
            // that face, so that the user can select them with the mouse.
            Vector onOrig = sbls->point;
            int i;
            for(i = is; i < thisShell.surface.n; i++) {
                SSurface *ss = &(thisShell.surface.elem[i]);
                hEntity face = Entity::NO_ENTITY;

                Vector p = ss->PointAt(0, 0),
                       n = ss->NormalAt(0, 0).WithMagnitude(1);
                double d = n.Dot(p);

                if(i == is || i == (is + 1)) {
                    // These are the top and bottom of the shell.
                    if(fabs((onOrig.Plus(ttop)).Dot(n) - d) < LENGTH_EPS) {
                        face = Remap(Entity::NO_ENTITY, REMAP_TOP);
                        ss->face = face.v;
                    }
                    if(fabs((onOrig.Plus(tbot)).Dot(n) - d) < LENGTH_EPS) {
                        face = Remap(Entity::NO_ENTITY, REMAP_BOTTOM);
                        ss->face = face.v;
                    }
                    continue;
                }

                // So these are the sides
                if(ss->degm != 1 || ss->degn != 1) continue;

                Entity *e;
                for(e = SK.entity.First(); e; e = SK.entity.NextAfter(e)) {
                    if(e->group.v != opA.v) continue;
                    if(e->type != Entity::Type::LINE_SEGMENT) continue;

                    Vector a = SK.GetEntity(e->point[0])->PointGetNum(),
                           b = SK.GetEntity(e->point[1])->PointGetNum();
                    a = a.Plus(ttop);
                    b = b.Plus(ttop);
                    // Could get taken backwards, so check all cases.
                    if((a.Equals(ss->ctrl[0][0]) && b.Equals(ss->ctrl[1][0])) ||
                       (b.Equals(ss->ctrl[0][0]) && a.Equals(ss->ctrl[1][0])) ||
                       (a.Equals(ss->ctrl[0][1]) && b.Equals(ss->ctrl[1][1])) ||
                       (b.Equals(ss->ctrl[0][1]) && a.Equals(ss->ctrl[1][1])))
                    {
                        face = Remap(e->h, REMAP_LINE_TO_FACE);
                        ss->face = face.v;
                        break;
                    }
                }
            }
        }
    } else if(type == Type::LATHE && haveSrc) {
        Group *src = SK.GetGroup(opA);

        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            thisShell.MakeFromRevolutionOf(sbls, pt, axis, color, this);
        }
    } else if(type == Type::LINKED) {
        // The imported shell or mesh are copied over, with the appropriate
        // transformation applied. We also must remap the face entities.
        Vector offset = {
            SK.GetParam(h.param(0))->val,
            SK.GetParam(h.param(1))->val,
            SK.GetParam(h.param(2))->val };
        Quaternion q = {
            SK.GetParam(h.param(3))->val,
            SK.GetParam(h.param(4))->val,
            SK.GetParam(h.param(5))->val,
            SK.GetParam(h.param(6))->val };

        thisMesh.MakeFromTransformationOf(&impMesh, offset, q, scale);
        thisMesh.RemapFaces(this, 0);

        thisShell.MakeFromTransformationOf(&impShell, offset, q, scale);
        thisShell.RemapFaces(this, 0);
    }

    if(srcg->meshCombine != CombineAs::ASSEMBLE) {
        thisShell.MergeCoincidentSurfaces();
    }

    // So now we've got the mesh or shell for this group. Combine it with
    // the previous group's mesh or shell with the requested Boolean, and
    // we're done.

    Group *prevg = srcg->RunningMeshGroup();

    if(prevg->runningMesh.IsEmpty() && thisMesh.IsEmpty() && !forceToMesh) {
        SShell *prevs = &(prevg->runningShell);
        GenerateForBoolean<SShell>(prevs, &thisShell, &runningShell,
            srcg->meshCombine);

        if(srcg->meshCombine != CombineAs::ASSEMBLE) {
            runningShell.MergeCoincidentSurfaces();
        }

        // If the Boolean failed, then we should note that in the text screen
        // for this group.
        booleanFailed = runningShell.booleanFailed;
        if(booleanFailed != prevBooleanFailed) {
            SS.ScheduleShowTW();
        }
    } else {
        SMesh prevm, thism;
        prevm = {};
        thism = {};

        prevm.MakeFromCopyOf(&(prevg->runningMesh));
        prevg->runningShell.TriangulateInto(&prevm);

        thism.MakeFromCopyOf(&thisMesh);
        thisShell.TriangulateInto(&thism);

        SMesh outm = {};
        GenerateForBoolean<SMesh>(&prevm, &thism, &outm, srcg->meshCombine);

        // And make sure that the output mesh is vertex-to-vertex.
        SKdNode *root = SKdNode::From(&outm);
        root->SnapToMesh(&outm);
        root->MakeMeshInto(&runningMesh);

        outm.Clear();
        thism.Clear();
        prevm.Clear();
    }

    displayDirty = true;
}

void Group::GenerateDisplayItems() {
    // This is potentially slow (since we've got to triangulate a shell, or
    // to find the emphasized edges for a mesh), so we will run it only
    // if its inputs have changed.
    if(displayDirty) {
        Group *pg = RunningMeshGroup();
        if(pg && thisMesh.IsEmpty() && thisShell.IsEmpty()) {
            // We don't contribute any new solid model in this group, so our
            // display items are identical to the previous group's; which means
            // that we can just display those, and stop ourselves from
            // recalculating for those every time we get a change in this group.
            //
            // Note that this can end up recursing multiple times (if multiple
            // groups that contribute no solid model exist in sequence), but
            // that's okay.
            pg->GenerateDisplayItems();

            displayMesh.Clear();
            displayMesh.MakeFromCopyOf(&(pg->displayMesh));

            displayEdges.Clear();
            displayOutlines.Clear();
            if(SS.GW.showEdges) {
                SEdge *se;
                SEdgeList *src = &(pg->displayEdges);
                for(se = src->l.First(); se; se = src->l.NextAfter(se)) {
                    displayEdges.l.Add(se);
                }
                displayOutlines.MakeFromCopyOf(&pg->displayOutlines);
            }
        } else {
            // We do contribute new solid model, so we have to triangulate the
            // shell, and edge-find the mesh.
            displayMesh.Clear();
            runningShell.TriangulateInto(&displayMesh);
            STriangle *t;
            for(t = runningMesh.l.First(); t; t = runningMesh.l.NextAfter(t)) {
                STriangle trn = *t;
                Vector n = trn.Normal();
                trn.an = n;
                trn.bn = n;
                trn.cn = n;
                displayMesh.AddTriangle(&trn);
            }

            displayEdges.Clear();
            displayOutlines.Clear();

            if(SS.GW.showEdges) {
                if(runningMesh.l.n > 0) {
                    // Triangle mesh only; no shell or emphasized edges.
                    runningMesh.MakeCertainEdgesAndOutlinesInto(
                        &displayEdges, &displayOutlines, EdgeKind::EMPHASIZED);
                } else {
                    displayMesh.MakeCertainEdgesAndOutlinesInto(
                        &displayEdges, &displayOutlines, EdgeKind::SHARP);
                }
            }
        }

        displayDirty = false;
    }
}

Group *Group::PreviousGroup() {
    int i;
    for(i = 0; i < SK.groupOrder.n; i++) {
        Group *g = SK.GetGroup(SK.groupOrder.elem[i]);
        if(g->h.v == h.v) break;
    }
    if(i == 0 || i >= SK.groupOrder.n) return NULL;
    return SK.GetGroup(SK.groupOrder.elem[i - 1]);
}

Group *Group::RunningMeshGroup() {
    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        return SK.GetGroup(opA)->RunningMeshGroup();
    } else {
        return PreviousGroup();
    }
}

bool Group::IsMeshGroup() {
    switch(type) {
        case Group::Type::EXTRUDE:
        case Group::Type::LATHE:
        case Group::Type::ROTATE:
        case Group::Type::TRANSLATE:
            return true;

        default:
            return false;
    }
}

void Group::DrawDisplayItems(Group::Type t) {
    RgbaColor specColor;
    bool useSpecColor;
    if(t == Type::DRAWING_3D || t == Type::DRAWING_WORKPLANE) {
        // force the color to something dim
        specColor = Style::Color(Style::DIM_SOLID);
        useSpecColor = true;
    } else {
        useSpecColor = false; // use the model color
    }
    // The back faces are drawn in red; should never seem them, since we
    // draw closed shells, so that's a debugging aid.
    GLfloat mpb[] = { 1.0f, 0.1f, 0.1f, 1.0f };
    glMaterialfv(GL_BACK, GL_AMBIENT_AND_DIFFUSE, mpb);

    // When we fill the mesh, we need to know which triangles are selected
    // or hovered, in order to draw them differently.
    uint32_t mh = 0, ms1 = 0, ms2 = 0;
    hEntity he = SS.GW.hover.entity;
    if(he.v != 0 && SK.GetEntity(he)->IsFace()) {
        mh = he.v;
    }
    SS.GW.GroupSelection();
    if(gs.faces > 0) ms1 = gs.face[0].v;
    if(gs.faces > 1) ms2 = gs.face[1].v;

    if(SS.GW.showShaded || SS.GW.showHdnLines) {
        if(SS.drawBackFaces && !displayMesh.isTransparent) {
            // For debugging, draw the backs of the triangles in red, so that we
            // notice when a shell is open
            glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 1);
        } else {
            glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, 0);
        }

        // Draw the shaded solid into the depth buffer for hidden line removal,
        // and if we're actually going to display it, to the color buffer too.
        glEnable(GL_LIGHTING);
        if(!SS.GW.showShaded) glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        ssglFillMesh(useSpecColor, specColor, &displayMesh, mh, ms1, ms2);
        if(!SS.GW.showShaded) glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_LIGHTING);
    }

    if(SS.GW.showEdges) {
        Vector projDir = SS.GW.projRight.Cross(SS.GW.projUp);

        glDepthMask(GL_FALSE);
        if(SS.GW.showHdnLines) {
            ssglDepthRangeOffset(0);
            glDepthFunc(GL_GREATER);
            ssglDrawEdges(&displayEdges, false, { Style::HIDDEN_EDGE });
            ssglDrawOutlines(&displayOutlines, projDir, { Style::HIDDEN_EDGE });
            glDepthFunc(GL_LEQUAL);
        }
        ssglDepthRangeOffset(2);
        ssglDrawEdges(&displayEdges, false, { Style::SOLID_EDGE });
        if(SS.GW.showOutlines) {
            ssglDrawOutlines(&displayOutlines, projDir, { Style::OUTLINE });
        } else {
            ssglDrawOutlines(&displayOutlines, projDir, { Style::SOLID_EDGE });
        }
        glDepthMask(GL_TRUE);
    }

    if(SS.GW.showMesh) ssglDebugMesh(&displayMesh);
}

void Group::Draw() {
    // Everything here gets drawn whether or not the group is hidden; we
    // can control this stuff independently, with show/hide solids, edges,
    // mesh, etc.

    GenerateDisplayItems();
    DrawDisplayItems(type);

    if(!SS.checkClosedContour) return;

    // And finally show the polygons too, and any errors if it's not possible
    // to assemble the lines into closed polygons.
    if(polyError.how == PolyError::NOT_CLOSED) {
        // Report this error only in sketch-in-workplane groups; otherwise
        // it's just a nuisance.
        if(type == Type::DRAWING_WORKPLANE) {
            glDisable(GL_DEPTH_TEST);
            ssglColorRGBa(Style::Color(Style::DRAW_ERROR), 0.2);
            ssglLineWidth (Style::Width(Style::DRAW_ERROR));
            glBegin(GL_LINES);
                ssglVertex3v(polyError.notClosedAt.a);
                ssglVertex3v(polyError.notClosedAt.b);
            glEnd();
            ssglColorRGB(Style::Color(Style::DRAW_ERROR));
            ssglWriteText("not closed contour, or not all same style!",
                Style::DefaultTextHeight(),
                polyError.notClosedAt.b, SS.GW.projRight, SS.GW.projUp,
                NULL, NULL);
            glEnable(GL_DEPTH_TEST);
        }
    } else if(polyError.how == PolyError::NOT_COPLANAR ||
              polyError.how == PolyError::SELF_INTERSECTING ||
              polyError.how == PolyError::ZERO_LEN_EDGE)
    {
        // These errors occur at points, not lines
        if(type == Type::DRAWING_WORKPLANE) {
            glDisable(GL_DEPTH_TEST);
            ssglColorRGB(Style::Color(Style::DRAW_ERROR));
            const char *msg;
            if(polyError.how == PolyError::NOT_COPLANAR) {
                msg = "points not all coplanar!";
            } else if(polyError.how == PolyError::SELF_INTERSECTING) {
                msg = "contour is self-intersecting!";
            } else {
                msg = "zero-length edge!";
            }
            ssglWriteText(msg, Style::DefaultTextHeight(),
                polyError.errorPointAt, SS.GW.projRight, SS.GW.projUp,
                NULL, NULL);
            glEnable(GL_DEPTH_TEST);
        }
    } else {
        // The contours will get filled in DrawFilledPaths.
    }
}

void Group::FillLoopSetAsPolygon(SBezierLoopSet *sbls) {
    SPolygon sp = {};
    sbls->MakePwlInto(&sp);
    ssglDepthRangeOffset(1);
    ssglFillPolygon(&sp);
    ssglDepthRangeOffset(0);
    sp.Clear();
}

void Group::DrawFilledPaths() {
    SBezierLoopSet *sbls;
    SBezierLoopSetSet *sblss = &bezierLoops;
    for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
        if(sbls->l.n == 0 || sbls->l.elem[0].l.n == 0) continue;
        // In an assembled loop, all the styles should be the same; so doesn't
        // matter which one we grab.
        SBezier *sb = &(sbls->l.elem[0].l.elem[0]);
        hStyle hs = { (uint32_t)sb->auxA };
        Style *s = Style::Get(hs);
        if(s->filled) {
            // This is a filled loop, where the user specified a fill color.
            ssglColorRGBa(s->fillColor, 1);
            FillLoopSetAsPolygon(sbls);
        } else {
            if(h.v == SS.GW.activeGroup.v && SS.checkClosedContour &&
               polyError.how == PolyError::GOOD)
            {
                // If this is the active group, and we are supposed to check
                // for closed contours, and we do indeed have a closed and
                // non-intersecting contour, then fill it dimly.
                ssglColorRGBa(Style::Color(Style::CONTOUR_FILL), 0.5);
                ssglDepthRangeOffset(1);
                FillLoopSetAsPolygon(sbls);
                ssglDepthRangeOffset(0);
            }
        }
    }
}

