//-----------------------------------------------------------------------------
// The root function to paint our graphics window, after setting up all the
// views and such appropriately. Also contains all the stuff to manage the
// selection.
//
// Copyright 2008-2013 Jonathan Westhues.
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

bool GraphicsWindow::Selection::HasEndpoints(void) {
    if(!entity.v) return false;
    Entity *e = SK.GetEntity(entity);
    return e->HasEndpoints();
}

void GraphicsWindow::Selection::Clear(void) {
    entity.v = constraint.v = 0;
    emphasized = false;
}

void GraphicsWindow::Selection::Draw(void) {
    Vector refp = Vector::From(0, 0, 0);
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

        ssglLineWidth(40);
        RgbaColor rgb = Style::Color(Style::HOVERED);
        glColor4d(rgb.redF(), rgb.greenF(), rgb.blueF(), 0.2);
        glBegin(GL_LINES);
            ssglVertex3v(topLeft);
            ssglVertex3v(refp);
        glEnd();
        ssglLineWidth(1);
    }
}

void GraphicsWindow::ClearSelection(void) {
    selection.Clear();
    SS.ScheduleShowTW();
    InvalidateGraphics();
}

void GraphicsWindow::ClearNonexistentSelectionItems(void) {
    bool change = false;
    Selection *s;
    selection.ClearTags();
    for(s = selection.First(); s; s = selection.NextAfter(s)) {
        if(s->constraint.v && !(SK.constraint.FindByIdNoOops(s->constraint))) {
            s->tag = 1;
            change = true;
        }
        if(s->entity.v && !(SK.entity.FindByIdNoOops(s->entity))) {
            s->tag = 1;
            change = true;
        }
    }
    selection.RemoveTagged();
    if(change) InvalidateGraphics();
}

//-----------------------------------------------------------------------------
// Is this entity/constraint selected?
//-----------------------------------------------------------------------------
bool GraphicsWindow::IsSelected(hEntity he) {
    Selection s = {};
    s.entity = he;
    return IsSelected(&s);
}
bool GraphicsWindow::IsSelected(Selection *st) {
    Selection *s;
    for(s = selection.First(); s; s = selection.NextAfter(s)) {
        if(s->Equals(st)) {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
// Unselect an item, if it is selected. We can either unselect just that item,
// or also unselect any coincident points. The latter is useful if the user
// somehow selects two coincident points (like with select all), because it
// would otherwise be impossible to de-select the lower of the two.
//-----------------------------------------------------------------------------
void GraphicsWindow::MakeUnselected(hEntity he, bool coincidentPointTrick) {
    Selection stog = {};
    stog.entity = he;
    MakeUnselected(&stog, coincidentPointTrick);
}
void GraphicsWindow::MakeUnselected(Selection *stog, bool coincidentPointTrick){
    if(stog->IsEmpty()) return;

    Selection *s;

    // If an item was selected, then we just un-select it.
    bool wasSelected = false;
    selection.ClearTags();
    for(s = selection.First(); s; s = selection.NextAfter(s)) {
        if(s->Equals(stog)) {
            s->tag = 1;
        }
    }
    // If two points are coincident, then it's impossible to hover one of
    // them. But make sure to deselect both, to avoid mysterious seeming
    // inability to deselect if the bottom one did somehow get selected.
    if(stog->entity.v && coincidentPointTrick) {
        Entity *e = SK.GetEntity(stog->entity);
        if(e->IsPoint()) {
            Vector ep = e->PointGetNum();
            for(s = selection.First(); s; s = selection.NextAfter(s)) {
                if(!s->entity.v) continue;
                if(s->entity.v == stog->entity.v) continue;
                Entity *se = SK.GetEntity(s->entity);
                if(!se->IsPoint()) continue;
                if(ep.Equals(se->PointGetNum())) {
                    s->tag = 1;
                }
            }
        }
    }
    selection.RemoveTagged();
}

//-----------------------------------------------------------------------------
// Select an item, if it isn't selected already.
//-----------------------------------------------------------------------------
void GraphicsWindow::MakeSelected(hEntity he) {
    Selection stog = {};
    stog.entity = he;
    MakeSelected(&stog);
}
void GraphicsWindow::MakeSelected(Selection *stog) {
    if(stog->IsEmpty()) return;
    if(IsSelected(stog)) return;

    if(stog->entity.v != 0 && SK.GetEntity(stog->entity)->IsFace()) {
        // In the interest of speed for the triangle drawing code,
        // only two faces may be selected at a time.
        int c = 0;
        Selection *s;
        selection.ClearTags();
        for(s = selection.First(); s; s = selection.NextAfter(s)) {
            hEntity he = s->entity;
            if(he.v != 0 && SK.GetEntity(he)->IsFace()) {
                c++;
                if(c >= 2) s->tag = 1;
            }
        }
        selection.RemoveTagged();
    }

    selection.Add(stog);
}

//-----------------------------------------------------------------------------
// Select everything that lies within the marquee view-aligned rectangle. For
// points, we test by the point location. For normals, we test by the normal's
// associated point. For anything else, we test by any piecewise linear edge.
//-----------------------------------------------------------------------------
void GraphicsWindow::SelectByMarquee(void) {
    Point2d begin = ProjectPoint(orig.marqueePoint);
    double xmin = min(orig.mouse.x, begin.x),
           xmax = max(orig.mouse.x, begin.x),
           ymin = min(orig.mouse.y, begin.y),
           ymax = max(orig.mouse.y, begin.y);

    Entity *e;
    for(e = SK.entity.First(); e; e = SK.entity.NextAfter(e)) {
        if(e->group.v != SS.GW.activeGroup.v) continue;
        if(e->IsFace() || e->IsDistance()) continue;
        if(!e->IsVisible()) continue;

        if(e->IsPoint() || e->IsNormal()) {
            Vector p = e->IsPoint() ? e->PointGetNum() :
                                      SK.GetEntity(e->point[0])->PointGetNum();
            Point2d pp = ProjectPoint(p);
            if(pp.x >= xmin && pp.x <= xmax &&
               pp.y >= ymin && pp.y <= ymax)
            {
                MakeSelected(e->h);
            }
        } else {
            // Use the 3d bounding box test routines, to avoid duplication;
            // so let our bounding square become a bounding box that certainly
            // includes the z = 0 plane.
            Vector ptMin = Vector::From(xmin, ymin, -1),
                   ptMax = Vector::From(xmax, ymax, 1);
            SEdgeList sel = {};
            e->GenerateEdges(&sel, true);
            SEdge *se;
            for(se = sel.l.First(); se; se = sel.l.NextAfter(se)) {
                Point2d ppa = ProjectPoint(se->a),
                        ppb = ProjectPoint(se->b);
                Vector  ptA = Vector::From(ppa.x, ppa.y, 0),
                        ptB = Vector::From(ppb.x, ppb.y, 0);
                if(Vector::BoundingBoxIntersectsLine(ptMax, ptMin,
                                                     ptA, ptB, true) ||
                   !ptA.OutsideAndNotOn(ptMax, ptMin) ||
                   !ptB.OutsideAndNotOn(ptMax, ptMin))
                {
                    MakeSelected(e->h);
                    break;
                }
            }
            sel.Clear();
        }
    }
}

//-----------------------------------------------------------------------------
// Sort the selection according to various critieria: the entities and
// constraints separately, counts of certain types of entities (circles,
// lines, etc.), and so on.
//-----------------------------------------------------------------------------
void GraphicsWindow::GroupSelection(void) {
    gs = {};
    int i;
    for(i = 0; i < selection.n && i < MAX_SELECTED; i++) {
        Selection *s = &(selection.elem[i]);
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

            if(e->HasEndpoints()) {
                (gs.withEndpoints)++;
            }

            // And some aux counts too
            switch(e->type) {
                case Entity::WORKPLANE:      (gs.workplanes)++; break;
                case Entity::LINE_SEGMENT:   (gs.lineSegments)++; break;
                case Entity::CUBIC:          (gs.cubics)++; break;
                case Entity::CUBIC_PERIODIC: (gs.periodicCubics)++; break;

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
    Selection s = {};

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
            s = {};
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
                s = {};
                s.constraint = SK.constraint.elem[i].h;
                dmin = d;
            }
        }

        // Faces, from the triangle mesh; these are lowest priority
        if(s.constraint.v == 0 && s.entity.v == 0 && showShaded && showFaces) {
            Group *g = SK.GetGroup(activeGroup);
            SMesh *m = &(g->displayMesh);

            uint32_t v = m->FirstIntersectionWith(mp);
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

//-----------------------------------------------------------------------------
// Return a point in the plane parallel to the screen and through the offset,
// that projects onto the specified (x, y) coordinates.
//-----------------------------------------------------------------------------
Vector GraphicsWindow::UnProjectPoint(Point2d p) {
    Vector orig = offset.ScaledBy(-1);

    // Note that we ignoring the effects of perspective. Since our returned
    // point has the same component normal to the screen as the offset, it
    // will have z = 0 after the rotation is applied, thus w = 1. So this is
    // correct.
    orig = orig.Plus(projRight.ScaledBy(p.x / scale)).Plus(
                     projUp.   ScaledBy(p.y / scale));
    return orig;
}

void GraphicsWindow::NormalizeProjectionVectors(void) {
    if(projRight.Magnitude() < LENGTH_EPS) {
        projRight = Vector::From(1, 0, 0);
    }

    Vector norm = projRight.Cross(projUp);
    // If projRight and projUp somehow ended up parallel, then pick an
    // arbitrary projUp normal to projRight.
    if(norm.Magnitude() < LENGTH_EPS) {
        norm = projRight.Normal(0);
    }
    projUp = norm.Cross(projRight);

    projUp = projUp.WithMagnitude(1);
    projRight = projRight.WithMagnitude(1);
}

Vector GraphicsWindow::VectorFromProjs(Vector rightUpForward) {
    Vector n = projRight.Cross(projUp);

    Vector r = (projRight.ScaledBy(rightUpForward.x));
    r =  r.Plus(projUp.ScaledBy(rightUpForward.y));
    r =  r.Plus(n.ScaledBy(rightUpForward.z));
    return r;
}

void GraphicsWindow::Paint(void) {
    int i;
    havePainted = true;

    int w, h;
    GetGraphicsWindowSize(&w, &h);
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
        glClearColor(SS.backgroundColor.redF(),
                     SS.backgroundColor.greenF(),
                     SS.backgroundColor.blueF(), 1.0f);
    } else {
        // Draw a different background whenever we're having solve problems.
        RgbaColor rgb = Style::Color(Style::DRAW_ERROR);
        glClearColor(0.4f*rgb.redF(), 0.4f*rgb.greenF(), 0.4f*rgb.blueF(), 1.0f);
        // And show the text window, which has info to debug it
        ForceTextWindowShown();
    }
    glClear(GL_COLOR_BUFFER_BIT);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);

    if(SS.bgImage.fromFile) {
        // If a background image is loaded, then we draw it now as a texture.
        // This handles the resizing for us nicely.
        glBindTexture(GL_TEXTURE_2D, TEXTURE_BACKGROUND_IMG);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                     SS.bgImage.rw, SS.bgImage.rh,
                     0,
                     GL_RGB, GL_UNSIGNED_BYTE,
                     SS.bgImage.fromFile);

        double tw = ((double)SS.bgImage.w) / SS.bgImage.rw,
               th = ((double)SS.bgImage.h) / SS.bgImage.rh;

        double mmw = SS.bgImage.w / SS.bgImage.scale,
               mmh = SS.bgImage.h / SS.bgImage.scale;

        Vector origin = SS.bgImage.origin;
        origin = origin.DotInToCsys(projRight, projUp, n);
        // Place the depth of our origin at the point that corresponds to
        // w = 1, so that it's unaffected by perspective.
        origin.z = (offset.ScaledBy(-1)).Dot(n);
        origin = origin.ScaleOutOfCsys(projRight, projUp, n);

        // Place the background at the very back of the Z order, though, by
        // mucking with the depth range.
        glDepthRange(1, 1);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
            glTexCoord2d(0, 0);
            ssglVertex3v(origin);

            glTexCoord2d(0, th);
            ssglVertex3v(origin.Plus(projUp.ScaledBy(mmh)));

            glTexCoord2d(tw, th);
            ssglVertex3v(origin.Plus(projRight.ScaledBy(mmw).Plus(
                                     projUp.   ScaledBy(mmh))));

            glTexCoord2d(tw, 0);
            ssglVertex3v(origin.Plus(projRight.ScaledBy(mmw)));
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
    ssglDepthRangeOffset(0);

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

    GLfloat ambient[4] = { (float)SS.ambientIntensity,
                           (float)SS.ambientIntensity,
                           (float)SS.ambientIntensity, 1 };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    ssglUnlockColor();

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

        ssglLineWidth(1);
        ssglColorRGBa(Style::Color(Style::DATUM), 0.3);
        glBegin(GL_LINES);
        for(i = i0 + 1; i < i1; i++) {
            ssglVertex3v(wp.Plus(wu.ScaledBy(i*g)).Plus(wv.ScaledBy(j0*g)));
            ssglVertex3v(wp.Plus(wu.ScaledBy(i*g)).Plus(wv.ScaledBy(j1*g)));
        }
        for(j = j0 + 1; j < j1; j++) {
            ssglVertex3v(wp.Plus(wu.ScaledBy(i0*g)).Plus(wv.ScaledBy(j*g)));
            ssglVertex3v(wp.Plus(wu.ScaledBy(i1*g)).Plus(wv.ScaledBy(j*g)));
        }
        glEnd();

        // Clear the depth buffer, so that the grid is at the very back of
        // the Z order.
        glClear(GL_DEPTH_BUFFER_BIT);
nogrid:;
    }

    // Draw the active group; this does stuff like the mesh and edges.
    (SK.GetGroup(activeGroup))->Draw();

    // Now draw the entities
    if(showHdnLines) glDisable(GL_DEPTH_TEST);
    Entity::DrawAll();

    // Draw filled paths in all groups, when those filled paths were requested
    // specially by assigning a style with a fill color, or when the filled
    // paths are just being filled by default. This should go last, to make
    // the transparency work.
    Group *g;
    for(g = SK.group.First(); g; g = SK.group.NextAfter(g)) {
        if(!(g->IsVisible())) continue;
        g->DrawFilledPaths();
    }


    glDisable(GL_DEPTH_TEST);
    // Draw the constraints
    for(i = 0; i < SK.constraint.n; i++) {
        SK.constraint.elem[i].Draw();
    }

    // Draw the "pending" constraint, i.e. a constraint that would be
    // placed on a line that is almost horizontal or vertical
    if(SS.GW.pending.operation == DRAGGING_NEW_LINE_POINT) {
        SuggestedConstraint suggested =
            SS.GW.SuggestLineConstraint(SS.GW.pending.request);
        if(suggested != GraphicsWindow::SUGGESTED_NONE) {
            Constraint c = {};
            c.group = SS.GW.activeGroup;
            c.workplane = SS.GW.ActiveWorkplane();
            c.type = suggested;
            c.ptA = Entity::NO_ENTITY;
            c.ptB = Entity::NO_ENTITY;
            c.entityA = SS.GW.pending.request.entity(0);
            c.entityB = Entity::NO_ENTITY;
            c.other = false;
            c.other2 = false;
            // Only draw.
            c.Draw();
        }
    }

    // Draw the traced path, if one exists
    ssglLineWidth(Style::Width(Style::ANALYZE));
    ssglColorRGB(Style::Color(Style::ANALYZE));
    SContour *sc = &(SS.traced.path);
    glBegin(GL_LINE_STRIP);
    for(i = 0; i < sc->l.n; i++) {
        ssglVertex3v(sc->l.elem[i].p);
    }
    glEnd();

    // And the naked edges, if the user did Analyze -> Show Naked Edges.
    ssglLineWidth(Style::Width(Style::DRAW_ERROR));
    ssglColorRGB(Style::Color(Style::DRAW_ERROR));
    ssglDrawEdges(&(SS.nakedEdges), true);

    // Then redraw whatever the mouse is hovering over, highlighted.
    glDisable(GL_DEPTH_TEST);
    ssglLockColorTo(Style::Color(Style::HOVERED));
    hover.Draw();

    // And finally draw the selection, same mechanism.
    ssglLockColorTo(Style::Color(Style::SELECTED));
    for(Selection *s = selection.First(); s; s = selection.NextAfter(s)) {
        s->Draw();
    }

    ssglUnlockColor();

    // If a marquee selection is in progress, then draw the selection
    // rectangle, as an outline and a transparent fill.
    if(pending.operation == DRAGGING_MARQUEE) {
        Point2d begin = ProjectPoint(orig.marqueePoint);
        double xmin = min(orig.mouse.x, begin.x),
               xmax = max(orig.mouse.x, begin.x),
               ymin = min(orig.mouse.y, begin.y),
               ymax = max(orig.mouse.y, begin.y);

        Vector tl = UnProjectPoint(Point2d::From(xmin, ymin)),
               tr = UnProjectPoint(Point2d::From(xmax, ymin)),
               br = UnProjectPoint(Point2d::From(xmax, ymax)),
               bl = UnProjectPoint(Point2d::From(xmin, ymax));

        ssglLineWidth((GLfloat)1.3);
        ssglColorRGB(Style::Color(Style::HOVERED));
        glBegin(GL_LINE_LOOP);
            ssglVertex3v(tl);
            ssglVertex3v(tr);
            ssglVertex3v(br);
            ssglVertex3v(bl);
        glEnd();
        ssglColorRGBa(Style::Color(Style::HOVERED), 0.10);
        glBegin(GL_QUADS);
            ssglVertex3v(tl);
            ssglVertex3v(tr);
            ssglVertex3v(br);
            ssglVertex3v(bl);
        glEnd();
    }

    // An extra line, used to indicate the origin when rotating within the
    // plane of the monitor.
    if(SS.extraLine.draw) {
        ssglLineWidth(1);
        ssglLockColorTo(Style::Color(Style::DATUM));
        glBegin(GL_LINES);
            ssglVertex3v(SS.extraLine.ptA);
            ssglVertex3v(SS.extraLine.ptB);
        glEnd();
    }

    // A note to indicate the origin in the just-exported file.
    if(SS.justExportedInfo.draw) {
        ssglColorRGB(Style::Color(Style::DATUM));
        Vector p = SS.justExportedInfo.pt,
               u = SS.justExportedInfo.u,
               v = SS.justExportedInfo.v;

        ssglLineWidth(1.5);
        glBegin(GL_LINES);
            ssglVertex3v(p.Plus(u.WithMagnitude(-15/scale)));
            ssglVertex3v(p.Plus(u.WithMagnitude(30/scale)));
            ssglVertex3v(p.Plus(v.WithMagnitude(-15/scale)));
            ssglVertex3v(p.Plus(v.WithMagnitude(30/scale)));
        glEnd();

        ssglWriteText("(x, y) = (0, 0) for file just exported",
            DEFAULT_TEXT_HEIGHT,
            p.Plus(u.ScaledBy(10/scale)).Plus(v.ScaledBy(10/scale)),
            u, v, NULL, NULL);
        ssglWriteText("press Esc to clear this message",
            DEFAULT_TEXT_HEIGHT,
            p.Plus(u.ScaledBy(40/scale)).Plus(
                   v.ScaledBy(-(DEFAULT_TEXT_HEIGHT)/scale)),
            u, v, NULL, NULL);
    }

    // And finally the toolbar.
    if(SS.showToolbar) {
        ToolbarDraw();
    }
}

