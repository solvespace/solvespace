#include "solvespace.h"

#define gs (SS.GW.gs)

bool Group::AssembleLoops(void) {
    SBezierList sbl;
    ZERO(&sbl);

    int i;
    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        if(e->group.v != h.v) continue;
        if(e->construction) continue;

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

void Group::GenerateShellForStepAndRepeat(void) {
    Group *src = SS.GetGroup(opA);
    SShell *srcs = &(src->thisShell); // the shell to step and repeat

    int n = (int)valA, a0 = 0;
    if(subtype == ONE_SIDED && skipFirst) {
        a0++; n++;
    }
    int a;
    for(a = a0; a < n; a++) {
        int ap = a*2 - (subtype == ONE_SIDED ? 0 : (n-1));
        int remap = (a == (n - 1)) ? REMAP_LAST : a;

        if(type == TRANSLATE) {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            trans = trans.ScaledBy(ap);

        } else {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            double theta = ap * SS.GetParam(h.param(3))->val;
            double c = cos(theta), s = sin(theta);
            Vector axis = Vector::From(h.param(4), h.param(5), h.param(6));
            Quaternion q = Quaternion::From(c, s*axis.x, s*axis.y, s*axis.z);

        }

        if(src->meshCombine == COMBINE_AS_DIFFERENCE) {

        } else {

        }
    }
}

void Group::GenerateShellAndMesh(void) {
    thisShell.Clear();

    if(type == TRANSLATE || type == ROTATE) {
        GenerateShellForStepAndRepeat();
        goto done;
    }

    if(type == EXTRUDE) {
        Group *src = SS.GetGroup(opA);
        Vector translate = Vector::From(h.param(0), h.param(1), h.param(2));

        Vector tbot, ttop;
        if(subtype == ONE_SIDED) {
            tbot = Vector::From(0, 0, 0); ttop = translate.ScaledBy(2);
        } else {
            tbot = translate.ScaledBy(-1); ttop = translate.ScaledBy(1);
        }
        
        thisShell.MakeFromExtrusionOf(&(src->bezierLoopSet), tbot, ttop, color);
    } else if(type == LATHE) {
        Group *src = SS.GetGroup(opA);

        Vector orig = SS.GetEntity(predef.origin)->PointGetNum();
        Vector axis = SS.GetEntity(predef.entityB)->VectorGetNum();
        axis = axis.WithMagnitude(1);

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
        }
    }

    runningMesh.Clear();
    runningShell.Clear();

    // If this group contributes no new mesh, then our running mesh is the
    // same as last time, no combining required. Likewise if we have a mesh
    // but it's suppressed.
    if(suppress) {
        runningShell.MakeFromCopyOf(PreviousGroupShell());
        goto done;
    }

    // So our group's mesh appears in thisMesh. Combine this with the previous
    // group's mesh, using the requested operation.
    bool prevMeshError = meshError.yes;

    meshError.yes = false;
    meshError.interferesAt.Clear();

    SShell *a = PreviousGroupShell();
    if(meshCombine == COMBINE_AS_UNION) {
        runningShell.MakeFromUnionOf(a, &thisShell);
    } else if(meshCombine == COMBINE_AS_DIFFERENCE) {
        runningShell.MakeFromDifferenceOf(a, &thisShell);
    } else {
        if(0) //&(meshError.interferesAt)
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
    runningShell.TriangulateInto(&runningMesh);
    emphEdges.Clear();
    if(h.v == SS.GW.activeGroup.v && SS.edgeColor != 0) {
        thisShell.MakeEdgesInto(&emphEdges);
    }
}

SShell *Group::PreviousGroupShell(void) {
    int i;
    for(i = 0; i < SS.group.n; i++) {
        Group *g = &(SS.group.elem[i]);
        if(g->h.v == h.v) break;
    }
    if(i == 0 || i >= SS.group.n) oops();
    return &(SS.group.elem[i-1].runningShell);
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

        glLineWidth(1);
        glxDepthRangeOffset(2);
        glxColor3d(REDf  (SS.edgeColor),
                   GREENf(SS.edgeColor), 
                   BLUEf (SS.edgeColor));
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
    } else if(polyError.how == POLY_NOT_COPLANAR ||
              polyError.how == POLY_SELF_INTERSECTING)
    {
        // These errors occur at points, not lines
        if(type == DRAWING_WORKPLANE) {
            glDisable(GL_DEPTH_TEST);
            glxColor3d(1, 0, 0);
            glPushMatrix();
                glxTranslatev(polyError.errorPointAt);
                glxOntoWorkplane(SS.GW.projRight, SS.GW.projUp);
                if(polyError.how == POLY_NOT_COPLANAR) {
                    glxWriteText("points not all coplanar!");
                } else {
                    glxWriteText("contour is self-intersecting!");
                }
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

