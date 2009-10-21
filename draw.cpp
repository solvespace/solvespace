//-----------------------------------------------------------------------------
// The root function to paint our graphics window, after setting up all the
// views and such appropriately. Also contains all the stuff to manage the
// selection.
//-----------------------------------------------------------------------------
#include "solvespace.h"

bool GraphicsWindow::Selection::Equals(Selection *b) {
    if(entity.v     != b->entity.v)     return false;
    if(constraint.v != b->constraint.v) return false;
    return true;
}

bool GraphicsWindow::Selection::IsEmpty(void) {
    if(entity.v)        return false;
    if(constraint.v)    return false;
    return true;
}

bool GraphicsWindow::Selection::IsStylable(void) {
    if(entity.v) return true;
    if(constraint.v) {
        Constraint *c = SK.GetConstraint(constraint);
        if(c->type == Constraint::COMMENT) return true;
    }
    return false;
}

void GraphicsWindow::Selection::Clear(void) {
    entity.v = constraint.v = 0;
    emphasized = false;
}

void GraphicsWindow::Selection::Draw(void) {
    Vector refp;
    if(entity.v) {
        Entity *e = SK.GetEntity(entity);
        e->Draw();
        if(emphasized) refp = e->GetReferencePos();
    }
    if(constraint.v) {
        Constraint *c = SK.GetConstraint(constraint);
        c->Draw();
        if(emphasized) refp = c->GetReferencePos();
    }
    if(emphasized && (constraint.v || entity.v)) {
        // We want to emphasize this constraint or entity, by drawing a thick
        // line from the top left corner of the screen to the reference point
        // of that entity or constraint.
        double s = 0.501/SS.GW.scale;
        Vector topLeft =       SS.GW.projRight.ScaledBy(-SS.GW.width*s);
        topLeft = topLeft.Plus(SS.GW.projUp.ScaledBy(SS.GW.height*s));
        topLeft = topLeft.Minus(SS.GW.offset);

        glLineWidth(40);
        DWORD rgb = Style::Color(Style::HOVERED);
        glColor4d(REDf(rgb), GREENf(rgb), BLUEf(rgb), 0.2);
        glBegin(GL_LINES);
            glxVertex3v(topLeft);
            glxVertex3v(refp);
        glEnd();
        glLineWidth(1);
    }
}

void GraphicsWindow::ClearSelection(void) {
    for(int i = 0; i < MAX_SELECTED; i++) {
        selection[i].Clear();
    }
    SS.later.showTW = true;
    InvalidateGraphics();
}

void GraphicsWindow::ClearNonexistentSelectionItems(void) {
    bool change = false;
    for(int i = 0; i < MAX_SELECTED; i++) {
        Selection *s = &(selection[i]);
        if(s->constraint.v && !(SK.constraint.FindByIdNoOops(s->constraint))) {
            s->constraint.v = 0;
            change = true;
        }
        if(s->entity.v && !(SK.entity.FindByIdNoOops(s->entity))) {
            s->entity.v = 0;
            change = true;
        }
    }
    if(change) InvalidateGraphics();
}

//-----------------------------------------------------------------------------
// Toggle the selection state of the hovered item: if it was selected then
// un-select it, and if it wasn't then select it.
//-----------------------------------------------------------------------------
void GraphicsWindow::ToggleSelectionStateOfHovered(void) {
    if(hover.IsEmpty()) return;

    // If an item was selected, then we just un-select it.
    int i;
    for(i = 0; i < MAX_SELECTED; i++) {
        if(selection[i].Equals(&hover)) {
            selection[i].Clear();
            break;
        }
    }
    if(i != MAX_SELECTED) return;

    // So it's not selected, so we should select it.

    if(hover.entity.v != 0 && SK.GetEntity(hover.entity)->IsFace()) {
        // In the interest of speed for the triangle drawing code,
        // only two faces may be selected at a time.
        int c = 0;
        for(i = 0; i < MAX_SELECTED; i++) {
            hEntity he = selection[i].entity;
            if(he.v != 0 && SK.GetEntity(he)->IsFace()) {
                c++;
                if(c >= 2) selection[i].Clear();
            }
        }
    }

    for(i = 0; i < MAX_SELECTED; i++) {
        if(selection[i].IsEmpty()) {
            selection[i] = hover;
            break;
        }
    }
}

//-----------------------------------------------------------------------------
// Sort the selection according to various critieria: the entities and
// constraints separately, counts of certain types of entities (circles,
// lines, etc.), and so on.
//-----------------------------------------------------------------------------
void GraphicsWindow::GroupSelection(void) {
    memset(&gs, 0, sizeof(gs));
    int i;
    for(i = 0; i < MAX_SELECTED; i++) {
        Selection *s = &(selection[i]);
        if(s->entity.v) {
            (gs.n)++;

            Entity *e = SK.entity.FindById(s->entity);
            // A list of points, and a list of all entities that aren't points.
            if(e->IsPoint()) {
                gs.point[(gs.points)++] = s->entity;
            } else {
                gs.entity[(gs.entities)++] = s->entity;
                (gs.stylables)++;
            }

            // And an auxiliary list of normals, including normals from
            // workplanes.
            if(e->IsNormal()) {
                gs.anyNormal[(gs.anyNormals)++] = s->entity;
            } else if(e->IsWorkplane()) {
                gs.anyNormal[(gs.anyNormals)++] = e->Normal()->h;
            }

            // And of vectors (i.e., stuff with a direction to constrain)
            if(e->HasVector()) {
                gs.vector[(gs.vectors)++] = s->entity;
            }

            // Faces (which are special, associated/drawn with triangles)
            if(e->IsFace()) {
                gs.face[(gs.faces)++] = s->entity;
            }

            // And some aux counts too
            switch(e->type) {
                case Entity::WORKPLANE:     (gs.workplanes)++; break;
                case Entity::LINE_SEGMENT:  (gs.lineSegments)++; break;
                case Entity::CUBIC:         (gs.cubics)++; break;
                // but don't count periodic cubics

                case Entity::ARC_OF_CIRCLE:
                    (gs.circlesOrArcs)++;
                    (gs.arcs)++;
                    break;

                case Entity::CIRCLE:        (gs.circlesOrArcs)++; break;
            }
        }
        if(s->constraint.v) {
            gs.constraint[(gs.constraints)++] = s->constraint;
            Constraint *c = SK.GetConstraint(s->constraint);
            if(c->type == Constraint::COMMENT) {
                (gs.stylables)++;
                (gs.comments)++;
            }
        }
    }
}

void GraphicsWindow::HitTestMakeSelection(Point2d mp) {
    int i;
    double d, dmin = 1e12;
    Selection s;
    ZERO(&s);

    // Always do the entities; we might be dragging something that should
    // be auto-constrained, and we need the hover for that.
    for(i = 0; i < SK.entity.n; i++) {
        Entity *e = &(SK.entity.elem[i]);
        // Don't hover whatever's being dragged.
        if(e->h.request().v == pending.point.request().v) {
            // The one exception is when we're creating a new cubic; we
            // want to be able to hover the first point, because that's
            // how we turn it into a periodic spline.
            if(!e->IsPoint()) continue;
            if(!e->h.isFromRequest()) continue;
            Request *r = SK.GetRequest(e->h.request());
            if(r->type != Request::CUBIC) continue;
            if(r->extraPoints < 2) continue;
            if(e->h.v != r->h.entity(1).v) continue;
        }

        d = e->GetDistance(mp);
        if(d < 10 && d < dmin) {
            memset(&s, 0, sizeof(s));
            s.entity = e->h;
            dmin = d;
        }
    }

    // The constraints and faces happen only when nothing's in progress.
    if(pending.operation == 0) {
        // Constraints
        for(i = 0; i < SK.constraint.n; i++) {
            d = SK.constraint.elem[i].GetDistance(mp);
            if(d < 10 && d < dmin) {
                memset(&s, 0, sizeof(s));
                s.constraint = SK.constraint.elem[i].h;
                dmin = d;
            }
        }

        // Faces, from the triangle mesh; these are lowest priority
        if(s.constraint.v == 0 && s.entity.v == 0 && showShaded && showFaces) {
            Group *g = SK.GetGroup(activeGroup);
            SMesh *m = &(g->displayMesh);

            DWORD v = m->FirstIntersectionWith(mp);
            if(v) {
                s.entity.v = v;
            }
        }
    }

    if(!s.Equals(&hover)) {
        hover = s;
        InvalidateGraphics();
    }
}

//-----------------------------------------------------------------------------
// Project a point in model space to screen space, exactly as gl would; return
// units are pixels.
//-----------------------------------------------------------------------------
Point2d GraphicsWindow::ProjectPoint(Vector p) {
    Vector p3 = ProjectPoint3(p);
    Point2d p2 = { p3.x, p3.y };
    return p2;
}
//-----------------------------------------------------------------------------
// Project a point in model space to screen space, exactly as gl would; return
// units are pixels. The z coordinate is also returned, also in pixels.
//-----------------------------------------------------------------------------
Vector GraphicsWindow::ProjectPoint3(Vector p) {
    double w;
    Vector r = ProjectPoint4(p, &w);
    return r.ScaledBy(scale/w);
}
//-----------------------------------------------------------------------------
// Project a point in model space halfway into screen space. The scale is
// not applied, and the perspective divide isn't applied; instead the w
// coordinate is returned separately.
//-----------------------------------------------------------------------------
Vector GraphicsWindow::ProjectPoint4(Vector p, double *w) {
    p = p.Plus(offset);

    Vector r;
    r.x = p.Dot(projRight);
    r.y = p.Dot(projUp);
    r.z = p.Dot(projUp.Cross(projRight));

    *w = 1 + r.z*SS.CameraTangent()*scale;
    return r;
}

void GraphicsWindow::NormalizeProjectionVectors(void) {
    Vector norm = projRight.Cross(projUp);
    projUp = norm.Cross(projRight);

    projUp = projUp.ScaledBy(1/projUp.Magnitude());
    projRight = projRight.ScaledBy(1/projRight.Magnitude());
}

Vector GraphicsWindow::VectorFromProjs(Vector rightUpForward) {
    Vector n = projRight.Cross(projUp);

    Vector r = (projRight.ScaledBy(rightUpForward.x));
    r =  r.Plus(projUp.ScaledBy(rightUpForward.y));
    r =  r.Plus(n.ScaledBy(rightUpForward.z));
    return r;
}

void GraphicsWindow::Paint(int w, int h) {
    int i;
    havePainted = true;
    width = w; height = h;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();

    glScaled(scale*2.0/w, scale*2.0/h, scale*1.0/30000);

    double mat[16];
    // Last thing before display is to apply the perspective
    double clp = SS.CameraTangent()*scale;
    MakeMatrix(mat, 1,              0,              0,              0,
                    0,              1,              0,              0,
                    0,              0,              1,              0,
                    0,              0,              clp,            1);
    glMultMatrixd(mat);
    // Before that, we apply the rotation
    Vector n = projUp.Cross(projRight);
    MakeMatrix(mat, projRight.x,    projRight.y,    projRight.z,    0,
                    projUp.x,       projUp.y,       projUp.z,       0,
                    n.x,            n.y,            n.z,            0,
                    0,              0,              0,              1);
    glMultMatrixd(mat);
    // And before that, the translation
    MakeMatrix(mat, 1,              0,              0,              offset.x,
                    0,              1,              0,              offset.y,
                    0,              0,              1,              offset.z,
                    0,              0,              0,              1);
    glMultMatrixd(mat);

    glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity();

    glShadeModel(GL_SMOOTH);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    // don't enable GL_POLYGON_SMOOTH; that looks ugly on some graphics cards,
    // drawn with leaks in the mesh
    glEnable(GL_POLYGON_OFFSET_LINE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_DEPTH_TEST); 
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_NORMALIZE);
   
    // At the same depth, we want later lines drawn over earlier.
    glDepthFunc(GL_LEQUAL);

    if(SS.AllGroupsOkay()) {
        glClearColor(REDf(SS.backgroundColor),
                     GREENf(SS.backgroundColor),
                     BLUEf(SS.backgroundColor), 1.0f);
    } else {
        // Draw a different background whenever we're having solve problems.
        DWORD rgb = Style::Color(Style::DRAW_ERROR);
        glClearColor(0.4f*REDf(rgb), 0.4f*GREENf(rgb), 0.4f*BLUEf(rgb), 1.0f);
        // And show the text window, which has info to debug it
        ForceTextWindowShown();
    }

    glClearDepth(1.0); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

    // Nasty case when we're reloading the imported files; could be that
    // we get an error, so a dialog pops up, and a message loop starts, and
    // we have to get called to paint ourselves. If the sketch is screwed
    // up, then we could trigger an oops trying to draw.
    if(!SS.allConsistent) return;

    // Let's use two lights, at the user-specified locations
    GLfloat f;
    glEnable(GL_LIGHT0);
    f = (GLfloat)SS.lightIntensity[0];
    GLfloat li0[] = { f, f, f, 1.0f };
    glLightfv(GL_LIGHT0, GL_DIFFUSE, li0);
    glLightfv(GL_LIGHT0, GL_SPECULAR, li0);

    glEnable(GL_LIGHT1);
    f = (GLfloat)SS.lightIntensity[1];
    GLfloat li1[] = { f, f, f, 1.0f };
    glLightfv(GL_LIGHT1, GL_DIFFUSE, li1);
    glLightfv(GL_LIGHT1, GL_SPECULAR, li1);

    Vector ld;
    ld = VectorFromProjs(SS.lightDir[0]);
    GLfloat ld0[4] = { (GLfloat)ld.x, (GLfloat)ld.y, (GLfloat)ld.z, 0 };
    glLightfv(GL_LIGHT0, GL_POSITION, ld0);
    ld = VectorFromProjs(SS.lightDir[1]);
    GLfloat ld1[4] = { (GLfloat)ld.x, (GLfloat)ld.y, (GLfloat)ld.z, 0 };
    glLightfv(GL_LIGHT1, GL_POSITION, ld1);

    if(SS.drawBackFaces) {
        // For debugging, draw the backs of the triangles in red, so that we
        // notice when a shell is open
        glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 1);
    } else {
        glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 0);
    }

    GLfloat ambient[4] = { (float)SS.ambientIntensity,
                           (float)SS.ambientIntensity,
                           (float)SS.ambientIntensity, 1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    glxUnlockColor();

    if(showSnapGrid && LockedInWorkplane()) {
        hEntity he = ActiveWorkplane();
        EntityBase *wrkpl = SK.GetEntity(he),
                   *norm  = wrkpl->Normal();
        Vector wu, wv, wn, wp;
        wp = SK.GetEntity(wrkpl->point[0])->PointGetNum();
        wu = norm->NormalU();
        wv = norm->NormalV();
        wn = norm->NormalN();

        double g = SS.gridSpacing;

        double umin = VERY_POSITIVE, umax = VERY_NEGATIVE, 
               vmin = VERY_POSITIVE, vmax = VERY_NEGATIVE;
        int a;
        for(a = 0; a < 4; a++) {
            // Ideally, we would just do +/- half the width and height; but
            // allow some extra slop for rounding.
            Vector horiz = projRight.ScaledBy((0.6*width)/scale  + 2*g),
                   vert  = projUp.   ScaledBy((0.6*height)/scale + 2*g);
            if(a == 2 || a == 3) horiz = horiz.ScaledBy(-1);
            if(a == 1 || a == 3) vert  = vert. ScaledBy(-1);
            Vector tp = horiz.Plus(vert).Minus(offset);
          
            // Project the point into our grid plane, normal to the screen
            // (not to the grid plane). If the plane is on edge then this is
            // impossible so don't try to draw the grid.
            bool parallel;
            Vector tpp = Vector::AtIntersectionOfPlaneAndLine(
                                            wn, wn.Dot(wp),
                                            tp, tp.Plus(n),
                                            &parallel);
            if(parallel) goto nogrid;

            tpp = tpp.Minus(wp);
            double uu = tpp.Dot(wu),
                   vv = tpp.Dot(wv);

            umin = min(uu, umin);
            umax = max(uu, umax);
            vmin = min(vv, vmin);
            vmax = max(vv, vmax);
        }

        int i, j, i0, i1, j0, j1;

        i0 = (int)(umin / g);
        i1 = (int)(umax / g);
        j0 = (int)(vmin / g);
        j1 = (int)(vmax / g);

        if(i0 > i1 || i1 - i0 > 400) goto nogrid;
        if(j0 > j1 || j1 - j0 > 400) goto nogrid;

        glLineWidth(1);
        glxColorRGBa(Style::Color(Style::DATUM), 0.3);
        glBegin(GL_LINES);
        for(i = i0 + 1; i < i1; i++) {
            glxVertex3v(wp.Plus(wu.ScaledBy(i*g)).Plus(wv.ScaledBy(j0*g)));
            glxVertex3v(wp.Plus(wu.ScaledBy(i*g)).Plus(wv.ScaledBy(j1*g)));
        }
        for(j = j0 + 1; j < j1; j++) {
            glxVertex3v(wp.Plus(wu.ScaledBy(i0*g)).Plus(wv.ScaledBy(j*g)));
            glxVertex3v(wp.Plus(wu.ScaledBy(i1*g)).Plus(wv.ScaledBy(j*g)));
        }
        glEnd(); 

        // Clear the depth buffer, so that the grid is at the very back of
        // the Z order.
        glClear(GL_DEPTH_BUFFER_BIT); 
nogrid:;
    }

    // Draw the active group; this fills the polygons in a drawing group, and
    // draws the solid mesh.
    (SK.GetGroup(activeGroup))->Draw();

    // Now draw the entities
    if(showHdnLines) glDisable(GL_DEPTH_TEST);
    Entity::DrawAll();

    glDisable(GL_DEPTH_TEST);
    // Draw the constraints
    for(i = 0; i < SK.constraint.n; i++) {
        SK.constraint.elem[i].Draw();
    }

    // Draw the traced path, if one exists
    glLineWidth(Style::Width(Style::ANALYZE));
    glxColorRGB(Style::Color(Style::ANALYZE));
    SContour *sc = &(SS.traced.path);
    glBegin(GL_LINE_STRIP);
    for(i = 0; i < sc->l.n; i++) {
        glxVertex3v(sc->l.elem[i].p);
    }
    glEnd();

    // And the naked edges, if the user did Analyze -> Show Naked Edges.
    glLineWidth(Style::Width(Style::DRAW_ERROR));
    glxColorRGB(Style::Color(Style::DRAW_ERROR));
    glxDrawEdges(&(SS.nakedEdges), true);

    // Then redraw whatever the mouse is hovering over, highlighted.
    glDisable(GL_DEPTH_TEST); 
    glxLockColorTo(Style::Color(Style::HOVERED));
    hover.Draw();

    // And finally draw the selection, same mechanism.
    glxLockColorTo(Style::Color(Style::SELECTED));
    for(i = 0; i < MAX_SELECTED; i++) {
        selection[i].Draw();
    }

    glxUnlockColor();

    // And finally the toolbar.
    if(SS.showToolbar) {
        ToolbarDraw();
    }
}

