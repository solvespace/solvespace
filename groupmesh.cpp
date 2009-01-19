#include "solvespace.h"

#define gs (SS.GW.gs)

bool Group::AssembleLoops(void) {
    SPolyCurveList spcl;
    ZERO(&spcl);

    int i;
    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        if(e->group.v != h.v) continue;
        if(e->construction) continue;

        e->GeneratePolyCurves(&spcl);
    }

    bool allClosed;
    curveLoops = SPolyCurveLoops::From(&spcl, &poly,
                                       &allClosed, &(polyError.notClosedAt));
    spcl.Clear();
    return allClosed;
}

void Group::GenerateLoops(void) {
    poly.Clear();
    curveLoops.Clear();

    if(type == DRAWING_3D || type == DRAWING_WORKPLANE || 
       type == ROTATE || type == TRANSLATE || type == IMPORTED)
    {
        if(AssembleLoops()) {
            polyError.how = POLY_GOOD;

            if(!poly.AllPointsInPlane(&(polyError.notCoplanarAt))) {
                // The edges aren't all coplanar; so not a good polygon
                polyError.how = POLY_NOT_COPLANAR;
                poly.Clear();
                curveLoops.Clear();
            }
        } else {
            polyError.how = POLY_NOT_CLOSED;
            poly.Clear();
            curveLoops.Clear();
        }
    }
}

void Group::AddQuadWithNormal(STriMeta meta, Vector out,
                                Vector a, Vector b, Vector c, Vector d)
{
    // The quad becomes two triangles
    STriangle quad1 = STriangle::From(meta, a, b, c),
              quad2 = STriangle::From(meta, c, d, a);

    // Could be only one of the triangles has area; be sure
    // to use that one for normal checking, then.
    Vector n1 = quad1.Normal(), n2 = quad2.Normal();
    Vector n = (n1.Magnitude() > n2.Magnitude()) ? n1 : n2;
    if(n.Dot(out) < 0) {
        quad1.FlipNormal();
        quad2.FlipNormal();
    }
    // One or both of the endpoints might lie on the axis of
    // rotation, in which case its triangle is zero-area.
    if(n1.Magnitude() > LENGTH_EPS) thisMesh.AddTriangle(&quad1);
    if(n2.Magnitude() > LENGTH_EPS) thisMesh.AddTriangle(&quad2);
}

void Group::GenerateMeshForStepAndRepeat(void) {
    Group *src = SS.GetGroup(opA);
    SMesh *srcm = &(src->thisMesh); // the mesh to step and repeat

    if(srcm->l.n == 0) {
        runningMesh.Clear();
        runningMesh.MakeFromCopy(PreviousGroupMesh());
        return;
    }

    SMesh origm;
    ZERO(&origm);
    origm.MakeFromCopy(src->PreviousGroupMesh());

    int n = (int)valA, a0 = 0;
    if(subtype == ONE_SIDED && skipFirst) {
        a0++; n++;
    }
    int a;
    for(a = a0; a < n; a++) {
        int ap = a*2 - (subtype == ONE_SIDED ? 0 : (n-1));
        int remap = (a == (n - 1)) ? REMAP_LAST : a;

        thisMesh.Clear();
        if(type == TRANSLATE) {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            trans = trans.ScaledBy(ap);
            for(int i = 0; i < srcm->l.n; i++) {
                STriangle tr = srcm->l.elem[i];
                tr.a = (tr.a).Plus(trans);
                tr.b = (tr.b).Plus(trans);
                tr.c = (tr.c).Plus(trans);
                if(tr.meta.face != 0) {
                    hEntity he = { tr.meta.face };
                    tr.meta.face = Remap(he, remap).v;
                }
                thisMesh.AddTriangle(&tr);
            }
        } else {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            double theta = ap * SS.GetParam(h.param(3))->val;
            double c = cos(theta), s = sin(theta);
            Vector axis = Vector::From(h.param(4), h.param(5), h.param(6));
            Quaternion q = Quaternion::From(c, s*axis.x, s*axis.y, s*axis.z);

            for(int i = 0; i < srcm->l.n; i++) {
                STriangle tr = srcm->l.elem[i];
                tr.a = (q.Rotate((tr.a).Minus(trans))).Plus(trans);
                tr.b = (q.Rotate((tr.b).Minus(trans))).Plus(trans);
                tr.c = (q.Rotate((tr.c).Minus(trans))).Plus(trans);
                if(tr.meta.face != 0) {
                    hEntity he = { tr.meta.face };
                    tr.meta.face = Remap(he, remap).v;
                }
                thisMesh.AddTriangle(&tr);
            }
        }

        runningMesh.Clear();
        if(src->meshCombine == COMBINE_AS_DIFFERENCE) {
            runningMesh.MakeFromDifference(&origm, &thisMesh);
        } else {
            runningMesh.MakeFromUnion(&origm, &thisMesh);
        }
        origm.Clear();
        origm.MakeFromCopy(&runningMesh);
    }
    origm.Clear();
    thisMesh.Clear();
}

void Group::GenerateMesh(void) {
    thisMesh.Clear();
    STriMeta meta = { 0, color };

    if(type == TRANSLATE || type == ROTATE) {
        GenerateMeshForStepAndRepeat();
        goto done;
    }

    if(type == EXTRUDE) {
        SEdgeList edges;
        ZERO(&edges);
        int i;
        Group *src = SS.GetGroup(opA);
        Vector translate = Vector::From(h.param(0), h.param(1), h.param(2));

        Vector tbot, ttop;
        if(subtype == ONE_SIDED) {
            tbot = Vector::From(0, 0, 0); ttop = translate.ScaledBy(2);
        } else {
            tbot = translate.ScaledBy(-1); ttop = translate.ScaledBy(1);
        }
        bool flipBottom = translate.Dot(src->poly.normal) > 0;

        // Get a triangulation of the source poly; this is not a closed mesh.
        SMesh srcm; ZERO(&srcm);
        (src->poly).TriangulateInto(&srcm);

        // Do the bottom; that has normal pointing opposite from translate
        meta.face = Remap(Entity::NO_ENTITY, REMAP_BOTTOM).v;
        for(i = 0; i < srcm.l.n; i++) {
            STriangle *st = &(srcm.l.elem[i]);
            Vector at = (st->a).Plus(tbot), 
                   bt = (st->b).Plus(tbot),
                   ct = (st->c).Plus(tbot);
            if(flipBottom) {
                thisMesh.AddTriangle(meta, ct, bt, at);
            } else {
                thisMesh.AddTriangle(meta, at, bt, ct);
            }
        }
        // And the top; that has the normal pointing the same dir as translate
        meta.face = Remap(Entity::NO_ENTITY, REMAP_TOP).v;
        for(i = 0; i < srcm.l.n; i++) {
            STriangle *st = &(srcm.l.elem[i]);
            Vector at = (st->a).Plus(ttop), 
                   bt = (st->b).Plus(ttop),
                   ct = (st->c).Plus(ttop);
            if(flipBottom) {
                thisMesh.AddTriangle(meta, at, bt, ct);
            } else {
                thisMesh.AddTriangle(meta, ct, bt, at);
            }
        }
        srcm.Clear();
        // Get the source polygon to extrude, and break it down to edges
        edges.Clear();
        (src->poly).MakeEdgesInto(&edges);

        edges.l.ClearTags();
        TagEdgesFromLineSegments(&edges);
        // The sides; these are quads, represented as two triangles.
        for(i = 0; i < edges.l.n; i++) {
            SEdge *edge = &(edges.l.elem[i]);
            Vector abot = (edge->a).Plus(tbot), bbot = (edge->b).Plus(tbot);
            Vector atop = (edge->a).Plus(ttop), btop = (edge->b).Plus(ttop);
            // We tagged the edges that came from line segments; their
            // triangles should be associated with that plane face.
            if(edge->tag) {
                hEntity hl = { edge->tag };
                hEntity hf = Remap(hl, REMAP_LINE_TO_FACE);
                meta.face = hf.v;
            } else {
                meta.face = 0;
            }
            if(flipBottom) {
                thisMesh.AddTriangle(meta, bbot, abot, atop);
                thisMesh.AddTriangle(meta, bbot, atop, btop);
            } else {
                thisMesh.AddTriangle(meta, abot, bbot, atop);
                thisMesh.AddTriangle(meta, bbot, btop, atop);
            }
        }
        edges.Clear();
    } else if(type == LATHE) {
        SEdgeList edges;
        ZERO(&edges);
        int a, i;

        Group *src = SS.GetGroup(opA);
        (src->poly).MakeEdgesInto(&edges);

        Vector orig = SS.GetEntity(predef.origin)->PointGetNum();
        Vector axis = SS.GetEntity(predef.entityB)->VectorGetNum();
        axis = axis.WithMagnitude(1);

        // Calculate the max radius, to determine fineness of mesh
        double r, rmax = 0;
        for(i = 0; i < edges.l.n; i++) {
            SEdge *edge = &(edges.l.elem[i]);
            r = (edge->a).DistanceToLine(orig, axis);
            rmax = max(r, rmax);
            r = (edge->b).DistanceToLine(orig, axis);
            rmax = max(r, rmax);
        }

        int n = SS.CircleSides(rmax);
        for(a = 0; a < n; a++) {
            double thetai = (2*PI*WRAP(a-1, n))/n, thetaf = (2*PI*a)/n;
            for(i = 0; i < edges.l.n; i++) {
                SEdge *edge = &(edges.l.elem[i]);

                Vector ai = (edge->a).RotatedAbout(orig, axis, thetai);
                Vector bi = (edge->b).RotatedAbout(orig, axis, thetai);
                Vector af = (edge->a).RotatedAbout(orig, axis, thetaf);
                Vector bf = (edge->b).RotatedAbout(orig, axis, thetaf);

                Vector ab = (edge->b).Minus(edge->a);
                Vector out = ((src->poly).normal).Cross(ab);
                // This is a vector, not a point, so no origin for rotation
                out = out.RotatedAbout(axis, thetai);

                AddQuadWithNormal(meta, out, ai, bi, bf, af);
            }
        }
    } else if(type == IMPORTED) {
        // Triangles are just copied over, with the appropriate transformation
        // applied.
        Vector offset = {
            SS.GetParam(h.param(0))->val,
            SS.GetParam(h.param(1))->val,
            SS.GetParam(h.param(2))->val };
        Quaternion q = {
            SS.GetParam(h.param(3))->val,
            SS.GetParam(h.param(4))->val,
            SS.GetParam(h.param(5))->val,
            SS.GetParam(h.param(6))->val };

        for(int i = 0; i < impMesh.l.n; i++) {
            STriangle st = impMesh.l.elem[i];

            if(st.meta.face != 0) {
                hEntity he = { st.meta.face };
                st.meta.face = Remap(he, 0).v;
            }
            st.a = q.Rotate(st.a).Plus(offset);
            st.b = q.Rotate(st.b).Plus(offset);
            st.c = q.Rotate(st.c).Plus(offset);
            thisMesh.AddTriangle(&st);
        }
    }

    runningMesh.Clear();

    // If this group contributes no new mesh, then our running mesh is the
    // same as last time, no combining required. Likewise if we have a mesh
    // but it's suppressed.
    if(thisMesh.l.n == 0 || suppress) {
        runningMesh.MakeFromCopy(PreviousGroupMesh());
        goto done;
    }

    // So our group's mesh appears in thisMesh. Combine this with the previous
    // group's mesh, using the requested operation.
    bool prevMeshError = meshError.yes;
    meshError.yes = false;
    meshError.interferesAt.Clear();
    SMesh *a = PreviousGroupMesh();
    if(meshCombine == COMBINE_AS_UNION) {
        runningMesh.MakeFromUnion(a, &thisMesh);
    } else if(meshCombine == COMBINE_AS_DIFFERENCE) {
        runningMesh.MakeFromDifference(a, &thisMesh);
    } else {
        if(!runningMesh.MakeFromInterferenceCheck(a, &thisMesh,
                                                &(meshError.interferesAt)))
        {
            meshError.yes = true;
            // And the list of failed triangles goes in meshError.interferesAt
        }
    }
    if(prevMeshError != meshError.yes) {
        // The error is reported in the text window for the group.
        SS.later.showTW = true;
    }

done:
    if(!vvMeshClean) {
        emphEdges.Clear();
        if(h.v == SS.GW.activeGroup.v && SS.edgeColor != 0) {
            SKdNode *root = SKdNode::From(&runningMesh);
            root->SnapToMesh(&runningMesh);
            root->MakeCertainEdgesInto(&emphEdges, true);
        }
        vvMeshClean = true;
    }
}

SMesh *Group::PreviousGroupMesh(void) {
    int i;
    for(i = 0; i < SS.group.n; i++) {
        Group *g = &(SS.group.elem[i]);
        if(g->h.v == h.v) break;
    }
    if(i == 0 || i >= SS.group.n) oops();
    return &(SS.group.elem[i-1].runningMesh);
}

void Group::Draw(void) {
    // Show this even if the group is not visible. It's already possible
    // to show or hide just this with the "show solids" flag.

    int specColor;
    if(type == DRAWING_3D || type == DRAWING_WORKPLANE) {
        specColor = RGB(25, 25, 25); // force the color to something dim
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
    if(he.v != 0 && SS.GetEntity(he)->IsFace()) {
        mh = he.v;
    }
    SS.GW.GroupSelection();
    if(gs.faces > 0) ms1 = gs.face[0].v;
    if(gs.faces > 1) ms2 = gs.face[1].v;

    if(SS.GW.showShaded) {
        glEnable(GL_LIGHTING);
        glxFillMesh(specColor, &runningMesh, mh, ms1, ms2);
        glDisable(GL_LIGHTING);

        glxDrawEdges(&emphEdges);
    }

    if(meshError.yes) {
        // Draw the error triangles in bright red stripes, with no Z buffering
        GLubyte mask[32*32/8];
        memset(mask, 0xf0, sizeof(mask));
        glPolygonStipple(mask);

        int specColor = 0;
        glDisable(GL_DEPTH_TEST);
        glColor3d(0, 0, 0);
        glxFillMesh(0, &meshError.interferesAt, 0, 0, 0);
        glEnable(GL_POLYGON_STIPPLE);
        glColor3d(1, 0, 0);
        glxFillMesh(0, &meshError.interferesAt, 0, 0, 0);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_POLYGON_STIPPLE);
    }

    if(SS.GW.showMesh) glxDebugMesh(&runningMesh);

    // And finally show the polygons too
    if(!SS.GW.showShaded) return;
    if(polyError.how == POLY_NOT_CLOSED) {
        // Report this error only in sketch-in-workplane groups; otherwise
        // it's just a nuisance.
        if(type == DRAWING_WORKPLANE) {
            glDisable(GL_DEPTH_TEST);
            glxColor4d(1, 0, 0, 0.2);
            glLineWidth(10);
            glBegin(GL_LINES);
                glxVertex3v(polyError.notClosedAt.a);
                glxVertex3v(polyError.notClosedAt.b);
            glEnd();
            glLineWidth(1);
            glxColor3d(1, 0, 0);
            glPushMatrix();
                glxTranslatev(polyError.notClosedAt.b);
                glxOntoWorkplane(SS.GW.projRight, SS.GW.projUp);
                glxWriteText("not closed contour!");
            glPopMatrix();
            glEnable(GL_DEPTH_TEST);
        }
    } else if(polyError.how == POLY_NOT_COPLANAR) {
        // And this one too
        if(type == DRAWING_WORKPLANE) {
            glDisable(GL_DEPTH_TEST);
            glxColor3d(1, 0, 0);
            glPushMatrix();
                glxTranslatev(polyError.notCoplanarAt);
                glxOntoWorkplane(SS.GW.projRight, SS.GW.projUp);
                glxWriteText("points not all coplanar!");
            glPopMatrix();
            glEnable(GL_DEPTH_TEST);
        }
    } else {
        glxColor4d(0, 0.1, 0.1, 0.5);
        glxDepthRangeOffset(1);
        glxFillPolygon(&poly);
        glxDepthRangeOffset(0);
    }
}

