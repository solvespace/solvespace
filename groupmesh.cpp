#include "solvespace.h"

#define gs (SS.GW.gs)

void Group::GeneratePolygon(void) {
    poly.Clear();

    if(type == DRAWING_3D || type == DRAWING_WORKPLANE || 
       type == ROTATE || type == TRANSLATE)
    {
        SEdgeList edges; ZERO(&edges);
        int i;
        for(i = 0; i < SS.entity.n; i++) {
            Entity *e = &(SS.entity.elem[i]);
            if(e->group.v != h.v) continue;

            e->GenerateEdges(&edges);
        }
        SEdge error;
        if(edges.AssemblePolygon(&poly, &error)) {
            polyError.how = POLY_GOOD;
            poly.normal = poly.ComputeNormal();
            poly.FixContourDirections();

            if(!poly.AllPointsInPlane(&(polyError.notCoplanarAt))) {
                // The edges aren't all coplanar; so not a good polygon
                polyError.how = POLY_NOT_COPLANAR;
                poly.Clear();
            }
        } else {
            polyError.how = POLY_NOT_CLOSED;
            polyError.notClosedAt = error;
            poly.Clear();
        }
        edges.Clear();
    }
}

void Group::GenerateMesh(void) {
    SMesh outm;
    ZERO(&outm);
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

        STriMeta meta = { 0, color };

        // Do the bottom; that has normal pointing opposite from translate
        meta.face = Remap(Entity::NO_ENTITY, REMAP_BOTTOM).v;
        for(i = 0; i < srcm.l.n; i++) {
            STriangle *st = &(srcm.l.elem[i]);
            Vector at = (st->a).Plus(tbot), 
                   bt = (st->b).Plus(tbot),
                   ct = (st->c).Plus(tbot);
            if(flipBottom) {
                outm.AddTriangle(meta, ct, bt, at);
            } else {
                outm.AddTriangle(meta, at, bt, ct);
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
                outm.AddTriangle(meta, at, bt, ct);
            } else {
                outm.AddTriangle(meta, ct, bt, at);
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
                outm.AddTriangle(meta, bbot, abot, atop);
                outm.AddTriangle(meta, bbot, atop, btop);
            } else {
                outm.AddTriangle(meta, abot, bbot, atop);
                outm.AddTriangle(meta, bbot, btop, atop);
            }
        }
        edges.Clear();
    } else if(type == LATHE) {
        SEdgeList edges;
        ZERO(&edges);
        int a, i;

        Group *src = SS.GetGroup(opA);
        (src->poly).MakeEdgesInto(&edges);

        STriMeta meta = { 0, color };
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
        for(a = 0; a <= n; a++) {
            double thetai = (2*PI*WRAP(a-1, n))/n, thetaf = (2*PI*a)/n;
            for(i = 0; i < edges.l.n; i++) {
                SEdge *edge = &(edges.l.elem[i]);

                double da = (edge->a).DistanceToLine(orig, axis);
                double db = (edge->b).DistanceToLine(orig, axis);

                Vector ai = (edge->a).RotatedAbout(orig, axis, thetai);
                Vector bi = (edge->b).RotatedAbout(orig, axis, thetai);
                Vector af = (edge->a).RotatedAbout(orig, axis, thetaf);
                Vector bf = (edge->b).RotatedAbout(orig, axis, thetaf);

                Vector ab = (edge->b).Minus(edge->a);
                Vector out = ((src->poly).normal).Cross(ab);
                // This is a vector, not a point, so no origin for rotation
                out = out.RotatedAbout(axis, thetai);

                // The line sweeps out a quad, so two triangles
                STriangle quad1 = STriangle::From(meta, ai, bi, af),
                          quad2 = STriangle::From(meta, af, bi, bf);

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
                if(da >= LENGTH_EPS) outm.AddTriangle(&quad1);
                if(db >= LENGTH_EPS) outm.AddTriangle(&quad2);
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
            outm.AddTriangle(&st);
        }
    }

    // So our group's mesh appears in outm. Combine this with the previous
    // group's mesh, using the requested operation.
    mesh.Clear();
    bool prevMeshError = meshError.yes;
    meshError.yes = false;
    meshError.interferesAt.Clear();
    SMesh *a = PreviousGroupMesh();
    if(meshCombine == COMBINE_AS_UNION) {
        mesh.MakeFromUnion(a, &outm);
    } else if(meshCombine == COMBINE_AS_DIFFERENCE) {
        mesh.MakeFromDifference(a, &outm);
    } else {
        if(!mesh.MakeFromInterferenceCheck(a, &outm, &(meshError.interferesAt)))
            meshError.yes = true;
        // And the list of failed triangles appears in meshError.interferesAt
    }
    if(prevMeshError != meshError.yes) {
        // The error is reported in the text window for the group.
        SS.later.showTW = true;
    }
    outm.Clear();
}

SMesh *Group::PreviousGroupMesh(void) {
    int i;
    for(i = 0; i < SS.group.n; i++) {
        Group *g = &(SS.group.elem[i]);
        if(g->h.v == h.v) break;
    }
    if(i == 0 || i >= SS.group.n) oops();
    return &(SS.group.elem[i-1].mesh);
}

void Group::Draw(void) {
    // Show this even if the group is not visible. It's already possible
    // to show or hide just this with the "show solids" flag.

    int specColor;
    if(type != EXTRUDE && type != IMPORTED && type != LATHE) {
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

    glEnable(GL_LIGHTING);
    if(SS.GW.showShaded) glxFillMesh(specColor, &mesh, mh, ms1, ms2);
    glDisable(GL_LIGHTING);

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

    if(SS.GW.showMesh) glxDebugMesh(&mesh);

    // And finally show the polygons too
    if(!SS.GW.showShaded) return;
    if(polyError.how == POLY_NOT_CLOSED) {
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
    } else if(polyError.how == POLY_NOT_COPLANAR) {
        glDisable(GL_DEPTH_TEST);
        glxColor3d(1, 0, 0);
        glPushMatrix();
            glxTranslatev(polyError.notCoplanarAt);
            glxOntoWorkplane(SS.GW.projRight, SS.GW.projUp);
            glxWriteText("points not all coplanar!");
        glPopMatrix();
        glEnable(GL_DEPTH_TEST);
    } else {
        glxColor4d(0, 0.1, 0.1, 0.5);
        glPolygonOffset(-1, -1);
        glxFillPolygon(&poly);
        glPolygonOffset(0, 0);
    }
}

