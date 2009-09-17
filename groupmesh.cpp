#include "solvespace.h"

#define gs (SS.GW.gs)

bool Group::AssembleLoops(void) {
    SBezierList sbl;
    ZERO(&sbl);

    int i;
    for(i = 0; i < SK.entity.n; i++) {
        Entity *e = &(SK.entity.elem[i]);
        if(e->group.v != h.v) continue;
        if(e->construction) continue;
        if(e->forceHidden) continue;

        e->GenerateBezierCurves(&sbl);
    }

    bool allClosed;
    bezierLoopSet = SBezierLoopSet::From(&sbl, &poly,
                                         &allClosed, &(polyError.notClosedAt));
    sbl.Clear();
    return allClosed;
}

void Group::GenerateLoops(void) {
    poly.Clear();
    bezierLoopSet.Clear();

    if(type == DRAWING_3D || type == DRAWING_WORKPLANE || 
       type == ROTATE || type == TRANSLATE || type == IMPORTED)
    {
        if(AssembleLoops()) {
            polyError.how = POLY_GOOD;

            if(!poly.AllPointsInPlane(&(polyError.errorPointAt))) {
                // The edges aren't all coplanar; so not a good polygon
                polyError.how = POLY_NOT_COPLANAR;
                poly.Clear();
                bezierLoopSet.Clear();
            }
            if(poly.SelfIntersecting(&(polyError.errorPointAt))) {
                polyError.how = POLY_SELF_INTERSECTING;
                poly.Clear();
                bezierLoopSet.Clear();
            }
        } else {
            polyError.how = POLY_NOT_CLOSED;
            poly.Clear();
            bezierLoopSet.Clear();
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
    ZERO(&workA);
    ZERO(&workB);
    T *soFar = &workA, *scratch = &workB;

    int n = (int)valA, a0 = 0;
    if(subtype == ONE_SIDED && skipFirst) {
        a0++; n++;
    }
    int a;
    for(a = a0; a < n; a++) {
        int ap = a*2 - (subtype == ONE_SIDED ? 0 : (n-1));
        int remap = (a == (n - 1)) ? REMAP_LAST : a;

        T transd;
        ZERO(&transd);
        if(type == TRANSLATE) {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            trans = trans.ScaledBy(ap);
            transd.MakeFromTransformationOf(steps, trans, Quaternion::IDENTITY);
        } else {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            double theta = ap * SK.GetParam(h.param(3))->val;
            double c = cos(theta), s = sin(theta);
            Vector axis = Vector::From(h.param(4), h.param(5), h.param(6));
            Quaternion q = Quaternion::From(c, s*axis.x, s*axis.y, s*axis.z);
            // Rotation is centered at t; so A(x - t) + t = Ax + (t - At)
            transd.MakeFromTransformationOf(steps,
                trans.Minus(q.Rotate(trans)), q);
        }

        // We need to rewrite any plane face entities to the transformed ones.
        transd.RemapFaces(this, remap);

        // And tack this transformed copy on to the return.
        if(soFar->IsEmpty()) {
            scratch->MakeFromCopyOf(&transd);
        } else {
            scratch->MakeFromUnionOf(soFar, &transd);
        }

        SWAP(T *, scratch, soFar);
        scratch->Clear();
        transd.Clear();
    }

    outs->Clear();
    *outs = *soFar;
}

template<class T>
void Group::GenerateForBoolean(T *prevs, T *thiss, T *outs, int how) {
    // If this group contributes no new mesh, then our running mesh is the
    // same as last time, no combining required. Likewise if we have a mesh
    // but it's suppressed.
    if(thiss->IsEmpty() || suppress) {
        outs->MakeFromCopyOf(prevs);
        return;
    }

    // So our group's shell appears in thisShell. Combine this with the
    // previous group's shell, using the requested operation.
    if(how == COMBINE_AS_UNION) {
        outs->MakeFromUnionOf(prevs, thiss);
    } else if(how == COMBINE_AS_DIFFERENCE) {
        outs->MakeFromDifferenceOf(prevs, thiss);
    } else {
        outs->MakeFromAssemblyOf(prevs, thiss);
    }
}

void Group::GenerateShellAndMesh(void) {    
    bool prevBooleanFailed = booleanFailed;
    booleanFailed = false;

    Group *srcg = this;

    thisShell.Clear();
    thisMesh.Clear();
    runningShell.Clear();
    runningMesh.Clear();

    if(type == TRANSLATE || type == ROTATE) {
        // A step and repeat gets merged against the group's prevous group,
        // not our own previous group.
        srcg = SK.GetGroup(opA);

        GenerateForStepAndRepeat<SShell>(&(srcg->thisShell), &thisShell);
        GenerateForStepAndRepeat<SMesh> (&(srcg->thisMesh),  &thisMesh);
    } else if(type == EXTRUDE) {
        Group *src = SK.GetGroup(opA);
        Vector translate = Vector::From(h.param(0), h.param(1), h.param(2));

        Vector tbot, ttop;
        if(subtype == ONE_SIDED) {
            tbot = Vector::From(0, 0, 0); ttop = translate.ScaledBy(2);
        } else {
            tbot = translate.ScaledBy(-1); ttop = translate.ScaledBy(1);
        }
        
        thisShell.MakeFromExtrusionOf(&(src->bezierLoopSet), tbot, ttop, color);
        Vector onOrig = src->bezierLoopSet.point;
        // And for any plane faces, annotate the model with the entity for
        // that face, so that the user can select them with the mouse.
        int i;
        for(i = 0; i < thisShell.surface.n; i++) {
            SSurface *ss = &(thisShell.surface.elem[i]);
            hEntity face = Entity::NO_ENTITY;

            Vector p = ss->PointAt(0, 0),
                   n = ss->NormalAt(0, 0).WithMagnitude(1);
            double d = n.Dot(p);

            if(i == 0 || i == 1) {
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
                if(e->type != Entity::LINE_SEGMENT) continue;

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
    } else if(type == LATHE) {
        Group *src = SK.GetGroup(opA);

        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis = axis.WithMagnitude(1);

        thisShell.MakeFromRevolutionOf(&(src->bezierLoopSet), pt, axis, color);
    } else if(type == IMPORTED) {
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

        thisMesh.MakeFromTransformationOf(&impMesh, offset, q);
        thisMesh.RemapFaces(this, 0);

        thisShell.MakeFromTransformationOf(&impShell, offset, q);
        thisShell.RemapFaces(this, 0);
    }

    if(srcg->meshCombine != COMBINE_AS_ASSEMBLE) {
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

        if(srcg->meshCombine != COMBINE_AS_ASSEMBLE) {
            runningShell.MergeCoincidentSurfaces();
        }

        // If the Boolean failed, then we should note that in the text screen
        // for this group.
        booleanFailed = runningShell.booleanFailed;
        if(booleanFailed != prevBooleanFailed) {
            SS.later.showTW = true;
        }
    } else {
        SMesh prevm, thism;
        ZERO(&prevm);
        ZERO(&thism);

        prevm.MakeFromCopyOf(&(prevg->runningMesh));
        prevg->runningShell.TriangulateInto(&prevm);

        thism.MakeFromCopyOf(&thisMesh);
        thisShell.TriangulateInto(&thism);

        SMesh outm;
        ZERO(&outm);
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

void Group::GenerateDisplayItems(void) {
    // This is potentially slow (since we've got to triangulate a shell, or
    // to find the emphasized edges for a mesh), so we will run it only
    // if its inputs have changed.
    if(displayDirty) {
        displayMesh.Clear();
        runningShell.TriangulateInto(&displayMesh);
        STriangle *tr;
        for(tr = runningMesh.l.First(); tr; tr = runningMesh.l.NextAfter(tr)) {
            STriangle trn = *tr;
            Vector n = trn.Normal();
            trn.an = n;
            trn.bn = n;
            trn.cn = n;
            displayMesh.AddTriangle(&trn);
        }

        displayEdges.Clear();

        if(SS.GW.showEdges) {
            runningShell.MakeEdgesInto(&displayEdges);
            runningMesh.MakeEmphasizedEdgesInto(&displayEdges);
        }

        displayDirty = false;
    }
}

Group *Group::PreviousGroup(void) {
    int i;
    for(i = 0; i < SK.group.n; i++) {
        Group *g = &(SK.group.elem[i]);
        if(g->h.v == h.v) break;
    }
    if(i == 0 || i >= SK.group.n) return NULL;
    return &(SK.group.elem[i-1]);
}

Group *Group::RunningMeshGroup(void) {
    if(type == TRANSLATE || type == ROTATE) {
        return SK.GetGroup(opA)->RunningMeshGroup();
    } else {
        return PreviousGroup();
    }
}

void Group::DrawDisplayItems(int t) {
    int specColor;
    if(t == DRAWING_3D || t == DRAWING_WORKPLANE) {
        // force the color to something dim
        specColor = Style::Color(Style::DIM_SOLID);
    } else {
        specColor = -1; // use the model color
    }
    // The back faces are drawn in red; should never seem them, since we
    // draw closed shells, so that's a debugging aid.
    GLfloat mpb[] = { 1.0f, 0.1f, 0.1f, 1.0 };
    glMaterialfv(GL_BACK, GL_AMBIENT_AND_DIFFUSE, mpb);

    // When we fill the mesh, we need to know which triangles are selected
    // or hovered, in order to draw them differently.
    DWORD mh = 0, ms1 = 0, ms2 = 0;
    hEntity he = SS.GW.hover.entity;
    if(he.v != 0 && SK.GetEntity(he)->IsFace()) {
        mh = he.v;
    }
    SS.GW.GroupSelection();
    if(gs.faces > 0) ms1 = gs.face[0].v;
    if(gs.faces > 1) ms2 = gs.face[1].v;

    if(SS.GW.showShaded) {
        glEnable(GL_LIGHTING);
        glxFillMesh(specColor, &displayMesh, mh, ms1, ms2);
        glDisable(GL_LIGHTING);
    }
    if(SS.GW.showEdges) {
        glxDepthRangeOffset(2);
        glxColorRGB(Style::Color(Style::SOLID_EDGE));
        glLineWidth(Style::Width(Style::SOLID_EDGE));
        glxDrawEdges(&displayEdges, false);
    }

    if(SS.GW.showMesh) glxDebugMesh(&displayMesh);
}

void Group::Draw(void) {
    // Everything here gets drawn whether or not the group is hidden; we
    // can control this stuff independently, with show/hide solids, edges,
    // mesh, etc.

    Group *pg = RunningMeshGroup();
    if(pg && thisMesh.IsEmpty() && thisShell.IsEmpty()) {
        // We don't contribute any new solid model in this group, so our
        // display items are identical to the previous group's; which means
        // that we can just display those, and stop ourselves from
        // recalculating for those every time we get a change in this group.
        pg->GenerateDisplayItems();
        pg->DrawDisplayItems(type);
    } else {
        GenerateDisplayItems();
        DrawDisplayItems(type);
    }

    // And finally show the polygons too
    if(!SS.GW.showShaded) return;
    if(polyError.how == POLY_NOT_CLOSED) {
        // Report this error only in sketch-in-workplane groups; otherwise
        // it's just a nuisance.
        if(type == DRAWING_WORKPLANE) {
            glDisable(GL_DEPTH_TEST);
            glxColorRGBa(Style::Color(Style::DRAW_ERROR), 0.2);
            glLineWidth (Style::Width(Style::DRAW_ERROR));
            glBegin(GL_LINES);
                glxVertex3v(polyError.notClosedAt.a);
                glxVertex3v(polyError.notClosedAt.b);
            glEnd();
            glxColorRGB(Style::Color(Style::DRAW_ERROR));
            glxWriteText("not closed contour!",
                polyError.notClosedAt.b, SS.GW.projRight, SS.GW.projUp,
                NULL, NULL);
            glEnable(GL_DEPTH_TEST);
        }
    } else if(polyError.how == POLY_NOT_COPLANAR ||
              polyError.how == POLY_SELF_INTERSECTING)
    {
        // These errors occur at points, not lines
        if(type == DRAWING_WORKPLANE) {
            glDisable(GL_DEPTH_TEST);
            glxColorRGB(Style::Color(Style::DRAW_ERROR));
            char *msg = (polyError.how == POLY_NOT_COPLANAR) ?
                            "points not all coplanar!" :
                            "contour is self-intersecting!";
            glxWriteText(msg, 
                polyError.errorPointAt, SS.GW.projRight, SS.GW.projUp,
                NULL, NULL);
            glEnable(GL_DEPTH_TEST);
        }
    } else {
        glxColorRGBa(Style::Color(Style::CONTOUR_FILL), 0.5);
        glxDepthRangeOffset(1);
        glxFillPolygon(&poly);
        glxDepthRangeOffset(0);
    }
}

