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

void Group::GetTrajectory(hGroup hg, SContour *traj, SPolygon *section) {
    if(section->IsEmpty()) return;
   
    SEdgeList edges; ZERO(&edges);
    int i, j;

    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        if(e->group.v != hg.v) continue;
        e->GenerateEdges(&edges);
    }

    Vector pn = (section->normal).WithMagnitude(1);
    double pd = pn.Dot(section->AnyPoint());

    // Find the start of the trajectory
    Vector first, last;
    for(i = 0; i < edges.l.n; i++) {
        SEdge *se = &(edges.l.elem[i]);

        bool startA = true, startB = true;
        for(j = 0; j < edges.l.n; j++) {
            if(i == j) continue;
            SEdge *set = &(edges.l.elem[j]);
            if((set->a).Equals(se->a)) startA = false;
            if((set->b).Equals(se->a)) startA = false;
            if((set->a).Equals(se->b)) startB = false;
            if((set->b).Equals(se->b)) startB = false;
        }
        if(startA || startB) {
            // It's possible for both to be true, if only one segment exists
            if(startA) {
                first = se->a;
                last = se->b;
            } else {
                first = se->b;
                last = se->a;
            }
            se->tag = 1;
            break;
        }
    }
    if(i >= edges.l.n) goto cleanup;
    edges.AssembleContour(first, last, traj, NULL);
    if(traj->l.n < 1) goto cleanup;

    // Starting and ending points of the trajectory
    Vector ps, pf;
    ps = traj->l.elem[0].p;
    pf = traj->l.elem[traj->l.n - 1].p;
    // Distances of those points to the section plane
    double ds = fabs(pn.Dot(ps) - pd), df = fabs(pn.Dot(pf) - pd);
    if(ds < LENGTH_EPS && df < LENGTH_EPS) {
        if(section->WindingNumberForPoint(pf) > 0) {
            // Both the start and finish lie on the section plane; let the
            // start be the one that's somewhere within the section. Use
            // winding > 0, not odd/even, since it's natural e.g. to sweep
            // a ring to make a pipe, and draw the trajectory through the
            // center of the ring.
            traj->Reverse();
        }
    } else if(ds > df) {
        // The starting point is the endpoint that's closer to the plane
        traj->Reverse();
    }
cleanup:
    edges.Clear();
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

void Group::GenerateMeshForSweep(bool helical,
                                    Vector axisp, Vector axis, Vector onHelix)
{
    STriMeta meta = { 0, color };
    int a, i;

    // The closed section that will be swept along the curve
    Group *section = SS.GetGroup(helical ? opA : opB);
    SEdgeList edges;
    ZERO(&edges);
    (section->poly).MakeEdgesInto(&edges);

    // The trajectory along which the section will be swept
    SContour traj;
    ZERO(&traj);
    if(helical) {
        double r0 = onHelix.DistanceToLine(axisp, axis);
        int n = (int)(SS.CircleSides(r0)*valA) + 4;
        Vector origin = onHelix.ClosestPointOnLine(axisp, axis);
        Vector u = (onHelix.Minus(origin)).WithMagnitude(1);
        Vector v = (axis.Cross(u)).WithMagnitude(1);
        for(i = 0; i <= n; i++) {
            double turns = (i*valA)/n;
            double theta = turns*2*PI;
            double r = r0 + turns*valC;
            if(subtype == LEFT_HANDED) theta = -theta;
            Vector p = origin.Plus(
                            u.ScaledBy(r*cos(theta)).Plus(
                            v.ScaledBy(r*sin(theta)).Plus(
                            axis.WithMagnitude(turns*valB))));
            traj.AddPoint(p);
        }
    } else {
        GetTrajectory(opA, &traj, &(section->poly));
    }

    if(traj.l.n <= 0) {
        edges.Clear();
        return; // no trajectory, nothing to do
    }

    // Initial offset/orientation determined by first pwl in trajectory
    Vector origRef    =  traj.l.elem[0].p;
    Vector origNormal = (traj.l.elem[1].p).Minus(origRef);
    origNormal = origNormal.WithMagnitude(1);
    Vector oldRef = origRef, oldNormal = origNormal;

    Vector oldU, oldV;
    if(helical) {
        oldU = axis.WithMagnitude(1);
        oldV = (oldNormal.Cross(oldU)).WithMagnitude(1);
        // numerical fixup, since pwl segment isn't exactly tangent...
        oldU = (oldV.Cross(oldNormal)).WithMagnitude(1);
    } else {
        oldU = oldNormal.Normal(0);
        oldV = oldNormal.Normal(1);
    }

    // The endcap at the start of the curve
    SPolygon cap;
    ZERO(&cap);
    edges.l.ClearTags();
    edges.AssemblePolygon(&cap, NULL);
    cap.normal = cap.ComputeNormal();
    if(oldNormal.Dot(cap.normal) > 0) {
        cap.normal = (cap.normal).ScaledBy(-1);
    }
    cap.TriangulateInto(&thisMesh, meta);
    cap.Clear();

    // Rewrite the source polygon so that the trajectory is along the
    // z axis, and the poly lies in the xy plane
    for(i = 0; i < edges.l.n; i++) {
        SEdge *e = &(edges.l.elem[i]);
        e->a = ((e->a).Minus(oldRef)).DotInToCsys(oldU, oldV, oldNormal);
        e->b = ((e->b).Minus(oldRef)).DotInToCsys(oldU, oldV, oldNormal);
    }
    Vector polyn =
        (section->poly.normal).DotInToCsys(oldU, oldV, oldNormal);

    for(a = 1; a < traj.l.n; a++) {
        Vector thisRef = traj.l.elem[a].p;
        Vector thisNormal, useNormal;
        if(a == traj.l.n - 1) {
            thisNormal = oldNormal;
            useNormal = oldNormal;
        } else {
            thisNormal = (traj.l.elem[a+1].p).Minus(thisRef);
            useNormal = (thisNormal.Plus(oldNormal)).ScaledBy(0.5);
        }

        Vector useV, useU;
        useNormal = useNormal.WithMagnitude(1);
        if(helical) {
            // The axis of rotation is always a basis vector
            useU = axis.WithMagnitude(1);
            useV = (useNormal.Cross(useU)).WithMagnitude(1);
        } else {
            // Choose a new coordinate system, normal to the trajectory and
            // with the minimum possible twist about the normal.
            useV = (useNormal.Cross(oldU)).WithMagnitude(1);
            useU = (useV.Cross(useNormal)).WithMagnitude(1);
        }

        Quaternion qi = Quaternion::From(oldU, oldV);
        Quaternion qf = Quaternion::From(useU, useV);

        for(i = 0; i < edges.l.n; i++) {
            SEdge *edge = &(edges.l.elem[i]);
            Vector ai, bi, af, bf;
            ai = qi.Rotate(edge->a).Plus(oldRef);
            bi = qi.Rotate(edge->b).Plus(oldRef);

            af = qf.Rotate(edge->a).Plus(thisRef);
            bf = qf.Rotate(edge->b).Plus(thisRef);

            Vector ab = (edge->b).Minus(edge->a);
            Vector out = polyn.Cross(ab);
            out = qf.Rotate(out);

            AddQuadWithNormal(meta, out, ai, bi, bf, af);
        }
        oldRef = thisRef;
        oldNormal = thisNormal;
        oldU = useU;
        oldV = useV;
    }

    Quaternion q = Quaternion::From(oldU, oldV);
    for(i = 0; i < edges.l.n; i++) {
        SEdge *edge = &(edges.l.elem[i]);
        (edge->a) = q.Rotate(edge->a).Plus(oldRef);
        (edge->b) = q.Rotate(edge->b).Plus(oldRef);
    }
    edges.l.ClearTags();
    edges.AssemblePolygon(&cap, NULL);
    cap.normal = cap.ComputeNormal();
    if(oldNormal.Dot(cap.normal) < 0) {
        cap.normal = (cap.normal).ScaledBy(-1);
    }
    cap.TriangulateInto(&thisMesh, meta);
    cap.Clear();

    traj.l.Clear();
    edges.Clear();
}

void Group::GenerateMesh(void) {
    thisMesh.Clear();
    STriMeta meta = { 0, color };

    if(type == TRANSLATE || type == ROTATE) {
        GenerateMeshForStepAndRepeat();
        return;
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
    } else if(type == SWEEP) {
        Vector zp = Vector::From(0, 0, 0);
        GenerateMeshForSweep(false, zp, zp, zp);
    } else if(type == HELICAL_SWEEP) {
        Entity *ln = SS.GetEntity(predef.entityB);
        Vector lna = SS.GetEntity(ln->point[0])->PointGetNum(),
               lnb = SS.GetEntity(ln->point[1])->PointGetNum();
        Vector onh = SS.GetEntity(predef.origin)->PointGetNum();
        GenerateMeshForSweep(true, lna, lnb.Minus(lna), onh);
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
    // same as last time, no combining required.
    if(thisMesh.l.n == 0) {
        runningMesh.MakeFromCopy(PreviousGroupMesh());
        return;
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

    glEnable(GL_LIGHTING);
    if(SS.GW.showShaded) glxFillMesh(specColor, &runningMesh, mh, ms1, ms2);
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

    if(SS.GW.showMesh) glxDebugMesh(&runningMesh);

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
        glxDepthRangeOffset(1);
        glxFillPolygon(&poly);
        glxDepthRangeOffset(0);
    }
}

