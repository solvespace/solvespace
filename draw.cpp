#include "solvespace.h"

void GraphicsWindow::UpdateDraggedPoint(hEntity hp, double mx, double my) {
    Entity *p = SS.GetEntity(hp);
    Vector pos = p->PointGetNum();
    UpdateDraggedNum(&pos, mx, my);
    p->PointForceTo(pos);
}

void GraphicsWindow::UpdateDraggedNum(Vector *pos, double mx, double my) {
    *pos = pos->Plus(projRight.ScaledBy((mx - orig.mouse.x)/scale));
    *pos = pos->Plus(projUp.ScaledBy((my - orig.mouse.y)/scale));

    orig.mouse.x = mx;
    orig.mouse.y = my;
    InvalidateGraphics();
}

void GraphicsWindow::MouseMoved(double x, double y, bool leftDown,
            bool middleDown, bool rightDown, bool shiftDown, bool ctrlDown)
{
    if(GraphicsEditControlIsVisible()) return;
    if(rightDown) {
        middleDown = true;
        shiftDown = !shiftDown;
    }

    if(SS.showToolbar) {
        if(ToolbarMouseMoved((int)x, (int)y)) {
            hover.Clear();
            return;
        }
    }

    Point2d mp = { x, y };

    // If the middle button is down, then mouse movement is used to pan and
    // rotate our view. This wins over everything else.
    if(middleDown) {
        hover.Clear();

        double dx = (x - orig.mouse.x) / scale;
        double dy = (y - orig.mouse.y) / scale;

        // When the view is locked, permit only translation (pan).
        if(!(shiftDown || ctrlDown)) {
            double s = 0.3*(PI/180)*scale; // degrees per pixel
            projRight = orig.projRight.RotatedAbout(orig.projUp, -s*dx);
            projUp = orig.projUp.RotatedAbout(orig.projRight, s*dy);

            NormalizeProjectionVectors();
        } else if(ctrlDown) {
            double theta = atan2(orig.mouse.y, orig.mouse.x);
            theta -= atan2(y, x);

            Vector normal = orig.projRight.Cross(orig.projUp);
            projRight = orig.projRight.RotatedAbout(normal, theta);
            projUp = orig.projUp.RotatedAbout(normal, theta);

            NormalizeProjectionVectors();
        } else {
            offset.x = orig.offset.x + dx*projRight.x + dy*projUp.x;
            offset.y = orig.offset.y + dx*projRight.y + dy*projUp.y;
            offset.z = orig.offset.z + dx*projRight.z + dy*projUp.z;
        }

        orig.projRight = projRight;
        orig.projUp = projUp;
        orig.offset = offset;
        orig.mouse.x = x;
        orig.mouse.y = y;

        InvalidateGraphics();
        return;
    }
 
    if(pending.operation == 0) {
        double dm = orig.mouse.DistanceTo(mp);
        // If we're currently not doing anything, then see if we should
        // start dragging something.
        if(leftDown && dm > 3) {
            if(hover.entity.v) {
                Entity *e = SS.GetEntity(hover.entity);
                if(e->IsPoint()) {
                    // Start dragging this point.
                    ClearSelection();
                    pending.point = hover.entity;
                    pending.operation = DRAGGING_POINT;
                } else if(e->type == Entity::CIRCLE) {
                    // Drag the radius.
                    ClearSelection();
                    pending.circle = hover.entity;
                    pending.operation = DRAGGING_RADIUS;
                } else if(e->IsNormal()) {
                    ClearSelection();
                    pending.normal = hover.entity;
                    pending.operation = DRAGGING_NORMAL;
                }
            } else if(hover.constraint.v && 
                            SS.GetConstraint(hover.constraint)->HasLabel())
            {
                ClearSelection();
                pending.constraint = hover.constraint;
                pending.operation = DRAGGING_CONSTRAINT;
            }
            if(pending.operation != 0) {
                // We just started a drag, so remember for the undo before
                // the drag changes anything.
                SS.UndoRemember();
            }
        } else {
            // Otherwise, just hit test and give up; but don't hit test
            // if the mouse is down, because then the user could hover
            // a point, mouse down (thus selecting it), and drag, in an
            // effort to drag the point, but instead hover a different
            // entity before we move far enough to start the drag.
            if(!leftDown) HitTestMakeSelection(mp);
        }
        return;
    }

    // If the user has started an operation from the menu, but not
    // completed it, then just do the selection.
    if(pending.operation < FIRST_PENDING) {
        HitTestMakeSelection(mp);
        return;
    }

    // We're currently dragging something; so do that. But if we haven't
    // painted since the last time we solved, do nothing, because there's
    // no sense solving a frame and not displaying it.
    if(!havePainted) return;
    switch(pending.operation) {
        case DRAGGING_CONSTRAINT: {
            Constraint *c = SS.constraint.FindById(pending.constraint);
            UpdateDraggedNum(&(c->disp.offset), x, y);
            break;
        }
        case DRAGGING_NEW_LINE_POINT:
            HitTestMakeSelection(mp);
            // and fall through
        case DRAGGING_NEW_POINT:
        case DRAGGING_POINT: {
            Entity *p = SS.GetEntity(pending.point);
            if((p->type == Entity::POINT_N_ROT_TRANS) &&
               (shiftDown || ctrlDown))
            {
                // These points also come with a rotation, which the user can
                // edit by pressing shift or control.
                Quaternion q = p->PointGetQuaternion();
                Vector p3 = p->PointGetNum();
                Point2d p2 = ProjectPoint(p3);

                Vector u = q.RotationU(), v = q.RotationV();
                if(ctrlDown) {
                    double d = mp.DistanceTo(p2);
                    if(d < 25) {
                        // Don't start dragging the position about the normal
                        // until we're a little ways out, to get a reasonable
                        // reference pos
                        orig.mouse = mp;
                        break;
                    }
                    double theta = atan2(orig.mouse.y-p2.y, orig.mouse.x-p2.x);
                    theta -= atan2(y-p2.y, x-p2.x);

                    Vector gn = projRight.Cross(projUp);
                    u = u.RotatedAbout(gn, -theta);
                    v = v.RotatedAbout(gn, -theta);
                } else {
                    double dx = -(x - orig.mouse.x);
                    double dy = -(y - orig.mouse.y);
                    double s = 0.3*(PI/180); // degrees per pixel
                    u = u.RotatedAbout(projUp, -s*dx);
                    u = u.RotatedAbout(projRight, s*dy);
                    v = v.RotatedAbout(projUp, -s*dx);
                    v = v.RotatedAbout(projRight, s*dy);
                }
                q = Quaternion::From(u, v);
                p->PointForceQuaternionTo(q);
                // Let's rotate about the selected point; so fix up the
                // translation so that that point didn't move.
                p->PointForceTo(p3);
                orig.mouse = mp;
            } else {
                UpdateDraggedPoint(pending.point, x, y);
                HitTestMakeSelection(mp);
            }
            SS.MarkGroupDirtyByEntity(pending.point);
            break;
        }
        case DRAGGING_NEW_CUBIC_POINT: {
            UpdateDraggedPoint(pending.point, x, y);
            HitTestMakeSelection(mp);

            hRequest hr = pending.point.request();
            Vector p0 = SS.GetEntity(hr.entity(1))->PointGetNum();
            Vector p3 = SS.GetEntity(hr.entity(4))->PointGetNum();
            Vector p1 = p0.ScaledBy(2.0/3).Plus(p3.ScaledBy(1.0/3));
            SS.GetEntity(hr.entity(2))->PointForceTo(p1);
            Vector p2 = p0.ScaledBy(1.0/3).Plus(p3.ScaledBy(2.0/3));
            SS.GetEntity(hr.entity(3))->PointForceTo(p2);

            SS.MarkGroupDirtyByEntity(pending.point);
            break;
        }
        case DRAGGING_NEW_ARC_POINT: {
            UpdateDraggedPoint(pending.point, x, y);
            HitTestMakeSelection(mp);

            hRequest hr = pending.point.request();
            Vector ona = SS.GetEntity(hr.entity(2))->PointGetNum();
            Vector onb = SS.GetEntity(hr.entity(3))->PointGetNum();
            Vector center = (ona.Plus(onb)).ScaledBy(0.5);

            SS.GetEntity(hr.entity(1))->PointForceTo(center);

            SS.MarkGroupDirtyByEntity(pending.point);
            break;
        }
        case DRAGGING_NEW_RADIUS:
        case DRAGGING_RADIUS: {
            Entity *circle = SS.GetEntity(pending.circle);
            Vector center = SS.GetEntity(circle->point[0])->PointGetNum();
            Point2d c2 = ProjectPoint(center);
            double r = c2.DistanceTo(mp)/scale;
            SS.GetEntity(circle->distance)->DistanceForceTo(r);

            SS.MarkGroupDirtyByEntity(pending.circle);
            break;
        }

        case DRAGGING_NORMAL: {
            Entity *normal = SS.GetEntity(pending.normal);
            Vector p = SS.GetEntity(normal->point[0])->PointGetNum();
            Point2d p2 = ProjectPoint(p);

            Quaternion q = normal->NormalGetNum();
            Vector u = q.RotationU(), v = q.RotationV();

            if(ctrlDown) {
                double theta = atan2(orig.mouse.y-p2.y, orig.mouse.x-p2.x);
                theta -= atan2(y-p2.y, x-p2.x);

                Vector normal = projRight.Cross(projUp);
                u = u.RotatedAbout(normal, -theta);
                v = v.RotatedAbout(normal, -theta);
            } else {
                double dx = -(x - orig.mouse.x);
                double dy = -(y - orig.mouse.y);
                double s = 0.3*(PI/180); // degrees per pixel
                u = u.RotatedAbout(projUp, -s*dx);
                u = u.RotatedAbout(projRight, s*dy);
                v = v.RotatedAbout(projUp, -s*dx);
                v = v.RotatedAbout(projRight, s*dy);
            }
            orig.mouse = mp;
            normal->NormalForceTo(Quaternion::From(u, v));

            SS.MarkGroupDirtyByEntity(pending.normal);
            break;
        }

        default: oops();
    }
    if(pending.operation != 0 && pending.operation != DRAGGING_CONSTRAINT) {
        SS.GenerateAll();
    }
    havePainted = false;
}

void GraphicsWindow::ClearPending(void) {
    memset(&pending, 0, sizeof(pending));
}

void GraphicsWindow::MouseMiddleOrRightDown(double x, double y) {
    if(GraphicsEditControlIsVisible()) return;

    if(pending.operation == DRAGGING_NEW_LINE_POINT) {
        // Special case; use a middle or right click to stop drawing lines,
        // since a left click would draw another one. This is quicker and
        // more intuitive than hitting escape.
        ClearPending();
    }

    orig.offset = offset;
    orig.projUp = projUp;
    orig.projRight = projRight;
    orig.mouse.x = x;
    orig.mouse.y = y;
}

hRequest GraphicsWindow::AddRequest(int type) {
    return AddRequest(type, true);
}
hRequest GraphicsWindow::AddRequest(int type, bool rememberForUndo) {
    if(rememberForUndo) SS.UndoRemember();

    Request r;
    memset(&r, 0, sizeof(r));
    r.group = activeGroup;
    Group *g = SS.GetGroup(activeGroup);
    if(g->type == Group::DRAWING_3D || g->type == Group::DRAWING_WORKPLANE) {
        r.construction = false;
    } else {
        r.construction = true;
    }
    r.workplane = ActiveWorkplane();
    r.type = type;
    SS.request.AddAndAssignId(&r);

    // We must regenerate the parameters, so that the code that tries to
    // place this request's entities where the mouse is can do so. But
    // we mustn't try to solve until reasonable values have been supplied
    // for these new parameters, or else we'll get a numerical blowup.
    SS.GenerateAll(-1, -1);
    SS.MarkGroupDirty(r.group);
    return r.h;
}

bool GraphicsWindow::ConstrainPointByHovered(hEntity pt) {
    if(!hover.entity.v) return false;

    Entity *e = SS.GetEntity(hover.entity);
    if(e->IsPoint()) {
        Constraint::ConstrainCoincident(e->h, pt);
        return true;
    }
    if(e->IsCircle()) {
        Constraint::Constrain(Constraint::PT_ON_CIRCLE, 
            pt, Entity::NO_ENTITY, e->h);
        return true;
    }
    if(e->type == Entity::LINE_SEGMENT) {
        Constraint::Constrain(Constraint::PT_ON_LINE,
            pt, Entity::NO_ENTITY, e->h);
        return true;
    }

    return false;
}

void GraphicsWindow::MouseLeftDown(double mx, double my) {
    if(GraphicsEditControlIsVisible()) return;
    HideTextEditControl();

    if(SS.showToolbar) {
        if(ToolbarMouseDown((int)mx, (int)my)) return;
    }

    // Make sure the hover is up to date.
    MouseMoved(mx, my, false, false, false, false, false);
    orig.mouse.x = mx;
    orig.mouse.y = my;

    // The current mouse location
    Vector v = offset.ScaledBy(-1);
    v = v.Plus(projRight.ScaledBy(mx/scale));
    v = v.Plus(projUp.ScaledBy(my/scale));

    hRequest hr;
    switch(pending.operation) {
        case MNU_DATUM_POINT:
            hr = AddRequest(Request::DATUM_POINT);
            SS.GetEntity(hr.entity(0))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(0));

            ClearSuper();

            pending.operation = 0;
            break;

        case MNU_LINE_SEGMENT:
            hr = AddRequest(Request::LINE_SEGMENT);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(1));

            ClearSuper();

            pending.operation = DRAGGING_NEW_LINE_POINT;
            pending.point = hr.entity(2);
            pending.description = "click to place next point of line";
            SS.GetEntity(pending.point)->PointForceTo(v);
            break;

        case MNU_RECTANGLE: {
            if(!SS.GW.LockedInWorkplane()) {
                Error("Can't draw rectangle in 3d; select a workplane first.");
                ClearSuper();
                break;
            }
            hRequest lns[4];
            int i;
            SS.UndoRemember();
            for(i = 0; i < 4; i++) {
                lns[i] = AddRequest(Request::LINE_SEGMENT, false);
            }
            for(i = 0; i < 4; i++) {
                Constraint::ConstrainCoincident(
                    lns[i].entity(1), lns[(i+1)%4].entity(2));
                SS.GetEntity(lns[i].entity(1))->PointForceTo(v);
                SS.GetEntity(lns[i].entity(2))->PointForceTo(v);
            }
            for(i = 0; i < 4; i++) {
                Constraint::Constrain(
                    (i % 2) ? Constraint::HORIZONTAL : Constraint::VERTICAL,
                    Entity::NO_ENTITY, Entity::NO_ENTITY,  
                    lns[i].entity(0));
            }
            ConstrainPointByHovered(lns[2].entity(1));

            pending.operation = DRAGGING_NEW_POINT;
            pending.point = lns[1].entity(2);
            pending.description = "click to place other corner of rectangle";
            break;
        }
        case MNU_CIRCLE:
            hr = AddRequest(Request::CIRCLE);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            SS.GetEntity(hr.entity(32))->NormalForceTo(
                Quaternion::From(SS.GW.projRight, SS.GW.projUp));
            ConstrainPointByHovered(hr.entity(1));

            ClearSuper();

            pending.operation = DRAGGING_NEW_RADIUS;
            pending.circle = hr.entity(0);
            pending.description = "click to set radius";
            SS.GetParam(hr.param(0))->val = 0;
            break;

        case MNU_ARC: {
            if(!SS.GW.LockedInWorkplane()) {
                Error("Can't draw arc in 3d; select a workplane first.");
                ClearPending();
                break;
            }
            hr = AddRequest(Request::ARC_OF_CIRCLE);
            // This fudge factor stops us from immediately failing to solve
            // because of the arc's implicit (equal radius) tangent.
            Vector adj = SS.GW.projRight.WithMagnitude(2/SS.GW.scale);
            SS.GetEntity(hr.entity(1))->PointForceTo(v.Minus(adj));
            SS.GetEntity(hr.entity(2))->PointForceTo(v);
            SS.GetEntity(hr.entity(3))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(2));

            ClearSuper();

            pending.operation = DRAGGING_NEW_ARC_POINT;
            pending.point = hr.entity(3);
            pending.description = "click to place point";
            break;
        }
        case MNU_CUBIC:
            hr = AddRequest(Request::CUBIC);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            SS.GetEntity(hr.entity(2))->PointForceTo(v);
            SS.GetEntity(hr.entity(3))->PointForceTo(v);
            SS.GetEntity(hr.entity(4))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(1));

            ClearSuper();

            pending.operation = DRAGGING_NEW_CUBIC_POINT;
            pending.point = hr.entity(4);
            pending.description = "click to place next point of cubic";
            break;

        case MNU_WORKPLANE:
            if(LockedInWorkplane()) {
                Error("Sketching in a workplane already; sketch in 3d before "
                      "creating new workplane.");
                ClearSuper();
                break;
            }
            hr = AddRequest(Request::WORKPLANE);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            SS.GetEntity(hr.entity(32))->NormalForceTo( 
                Quaternion::From(SS.GW.projRight, SS.GW.projUp));
            ConstrainPointByHovered(hr.entity(1));

            ClearSuper();
            break;

        case MNU_TTF_TEXT: {
            if(!SS.GW.LockedInWorkplane()) {
                Error("Can't draw text in 3d; select a workplane first.");
                ClearSuper();
                break;
            }
            hr = AddRequest(Request::TTF_TEXT);
            Request *r = SS.GetRequest(hr);
            r->str.strcpy("Abc");
            r->font.strcpy("arial.ttf");

            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            SS.GetEntity(hr.entity(2))->PointForceTo(v);

            pending.operation = DRAGGING_NEW_POINT;
            pending.point = hr.entity(2);
            pending.description = "click to place bottom left of text";
            break;
        }

        case DRAGGING_RADIUS:
        case DRAGGING_NEW_POINT:
            // The MouseMoved event has already dragged it as desired.
            ClearPending();
            break;

        case DRAGGING_NEW_ARC_POINT:
        case DRAGGING_NEW_CUBIC_POINT:
            ConstrainPointByHovered(pending.point);
            ClearPending();
            break;

        case DRAGGING_NEW_LINE_POINT: {
            if(ConstrainPointByHovered(pending.point)) {
                ClearPending();
                break;
            }
            // Create a new line segment, so that we continue drawing.
            hRequest hr = AddRequest(Request::LINE_SEGMENT);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            SS.GetEntity(hr.entity(2))->PointForceTo(v);

            // Constrain the line segments to share an endpoint
            Constraint::ConstrainCoincident(pending.point, hr.entity(1));

            // And drag an endpoint of the new line segment
            pending.operation = DRAGGING_NEW_LINE_POINT;
            pending.point = hr.entity(2);
            pending.description = "click to place next point of next line";

            break;
        }

        case 0:
        default: {
            ClearPending();

            if(hover.IsEmpty()) break;

            // If an item is hovered, then by clicking on it, we toggle its
            // selection state.
            int i;
            for(i = 0; i < MAX_SELECTED; i++) {
                if(selection[i].Equals(&hover)) {
                    selection[i].Clear();
                    break;
                }
            }
            if(i != MAX_SELECTED) break;

            if(hover.entity.v != 0 && SS.GetEntity(hover.entity)->IsFace()) {
                // In the interest of speed for the triangle drawing code,
                // only two faces may be selected at a time.
                int c = 0;
                for(i = 0; i < MAX_SELECTED; i++) {
                    hEntity he = selection[i].entity;
                    if(he.v != 0 && SS.GetEntity(he)->IsFace()) {
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
            break;
        }
    }

    SS.later.showTW = true;
    InvalidateGraphics();
}

void GraphicsWindow::MouseLeftUp(double mx, double my) {
    switch(pending.operation) {
        case DRAGGING_POINT:
        case DRAGGING_CONSTRAINT:
        case DRAGGING_NORMAL:
        case DRAGGING_RADIUS:
            ClearPending();
            break;

        default:
            break;  // do nothing
    }
}

void GraphicsWindow::MouseLeftDoubleClick(double mx, double my) {
    if(GraphicsEditControlIsVisible()) return;
    HideTextEditControl();

    if(hover.constraint.v) {
        constraintBeingEdited = hover.constraint;
        ClearSuper();

        Constraint *c = SS.GetConstraint(constraintBeingEdited);
        if(c->reference) {
            // Not meaningful to edit a reference dimension
            return;
        }

        Vector p3 = c->GetLabelPos();
        Point2d p2 = ProjectPoint(p3);

        char s[1024];
        switch(c->type) {
            case Constraint::COMMENT:
                strcpy(s, c->comment.str);
                break;

            case Constraint::ANGLE:
            case Constraint::LENGTH_RATIO:
                sprintf(s, "%.3f", c->valA);
                break;

            default:
                strcpy(s, SS.MmToString(fabs(c->valA)));
                break;
        }
        ShowGraphicsEditControl((int)p2.x, (int)p2.y-4, s);
    }
}

void GraphicsWindow::EditControlDone(char *s) {
    HideGraphicsEditControl();
    Constraint *c = SS.GetConstraint(constraintBeingEdited);

    if(c->type == Constraint::COMMENT) {
        SS.UndoRemember();
        c->comment.strcpy(s);
        return;
    }

    Expr *e = Expr::From(s);
    if(e) {
        SS.UndoRemember();

        switch(c->type) {
            case Constraint::PT_LINE_DISTANCE:
            case Constraint::PT_FACE_DISTANCE:
            case Constraint::PT_PLANE_DISTANCE: {
                // The sign is not displayed to the user, but this is a signed
                // distance internally. To flip the sign, the user enters a
                // negative distance.
                bool wasNeg = (c->valA < 0);
                if(wasNeg) {
                    c->valA = -SS.ExprToMm(e);
                } else {
                    c->valA = SS.ExprToMm(e);
                }
                break;
            }
            case Constraint::ANGLE:
            case Constraint::LENGTH_RATIO:
                // These don't get the units conversion for distance, and
                // they're always positive
                c->valA = fabs(e->Eval());
                break;

            default:
                // These are always positive, and they get the units conversion.
                c->valA = fabs(SS.ExprToMm(e));
                break;
        }
        SS.MarkGroupDirty(c->group);
        SS.GenerateAll();
    } else {
        Error("Not a valid number or expression: '%s'", s);
    }
}

void GraphicsWindow::MouseScroll(double x, double y, int delta) {
    double offsetRight = offset.Dot(projRight);
    double offsetUp = offset.Dot(projUp);

    double righti = x/scale - offsetRight;
    double upi = y/scale - offsetUp;

    if(delta > 0) {
        scale *= 1.2;
    } else {
        scale /= 1.2;
    }

    double rightf = x/scale - offsetRight;
    double upf = y/scale - offsetUp;

    offset = offset.Plus(projRight.ScaledBy(rightf - righti));
    offset = offset.Plus(projUp.ScaledBy(upf - upi));

    InvalidateGraphics();
}

void GraphicsWindow::MouseLeave(void) {
    // Un-hover everything when the mouse leaves our window.
    hover.Clear();
    toolbarTooltipped = 0;
    toolbarHovered = 0;
    PaintGraphics();
}

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

void GraphicsWindow::Selection::Clear(void) {
    entity.v = constraint.v = 0;
    emphasized = false;
}

void GraphicsWindow::Selection::Draw(void) {
    Vector refp;
    if(entity.v) {
        glLineWidth(1.5);
        Entity *e = SS.GetEntity(entity);
        e->Draw();
        if(emphasized) refp = e->GetReferencePos();
        glLineWidth(1);
    }
    if(constraint.v) {
        Constraint *c = SS.GetConstraint(constraint);
        c->Draw();
        if(emphasized) refp = c->GetReferencePos();
    }
    if(emphasized && (constraint.v || entity.v)) {
        double s = 0.501/SS.GW.scale;
        Vector topLeft =       SS.GW.projRight.ScaledBy(-SS.GW.width*s);
        topLeft = topLeft.Plus(SS.GW.projUp.ScaledBy(SS.GW.height*s));
        topLeft = topLeft.Minus(SS.GW.offset);

        glLineWidth(40);
        glColor4d(1.0, 1.0, 0, 0.2);
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
        if(s->constraint.v && !(SS.constraint.FindByIdNoOops(s->constraint))) {
            s->constraint.v = 0;
            change = true;
        }
        if(s->entity.v && !(SS.entity.FindByIdNoOops(s->entity))) {
            s->entity.v = 0;
            change = true;
        }
    }
    if(change) InvalidateGraphics();
}

void GraphicsWindow::GroupSelection(void) {
    memset(&gs, 0, sizeof(gs));
    int i;
    for(i = 0; i < MAX_SELECTED; i++) {
        Selection *s = &(selection[i]);
        if(s->entity.v) {
            (gs.n)++;

            Entity *e = SS.entity.FindById(s->entity);
            // A list of points, and a list of all entities that aren't points.
            if(e->IsPoint()) {
                gs.point[(gs.points)++] = s->entity;
            } else {
                gs.entity[(gs.entities)++] = s->entity;
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

                case Entity::ARC_OF_CIRCLE:
                    (gs.circlesOrArcs)++;
                    (gs.arcs)++;
                    break;

                case Entity::CIRCLE:        (gs.circlesOrArcs)++; break;
            }
        }
        if(s->constraint.v) {
            gs.constraint[(gs.constraints)++] = s->constraint;
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
    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        // Don't hover whatever's being dragged.
        if(e->h.request().v == pending.point.request().v) continue;

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
        for(i = 0; i < SS.constraint.n; i++) {
            d = SS.constraint.elem[i].GetDistance(mp);
            if(d < 10 && d < dmin) {
                memset(&s, 0, sizeof(s));
                s.constraint = SS.constraint.elem[i].h;
                dmin = d;
            }
        }

        // Faces, from the triangle mesh; these are lowest priority
        if(s.constraint.v == 0 && s.entity.v == 0 && showShaded && showFaces) {
            SMesh *m = &((SS.GetGroup(activeGroup))->runningMesh);
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
    double clp = SS.cameraTangent*scale;
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
        glClearColor(0, 0, 0, 1.0f);
    } else {
        // Draw a red background whenever we're having solve problems.
        glClearColor(0.4f, 0, 0, 1.0f);
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

    GLfloat ambient[4] = { 0.4f, 0.4f, 0.4f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    glxUnlockColor();

    // Draw the active group; this fills the polygons in a drawing group, and
    // draws the solid mesh.
    (SS.GetGroup(activeGroup))->Draw();

    // Now draw the entities
    if(showHdnLines) glDisable(GL_DEPTH_TEST);
    Entity::DrawAll();

    glDisable(GL_DEPTH_TEST);
    // Draw the constraints
    for(i = 0; i < SS.constraint.n; i++) {
        SS.constraint.elem[i].Draw();
    }

    // Draw the traced path, if one exists
    glLineWidth(1);
    glColor3d(0, 1, 1);
    SContour *sc = &(SS.traced.path);
    glBegin(GL_LINE_STRIP);
    for(i = 0; i < sc->l.n; i++) {
        glxVertex3v(sc->l.elem[i].p);
    }
    glEnd();

    // And the naked edges, if the user did Analyze -> Show Naked Edges.
    glLineWidth(7);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(4, 0x5555);
    glColor3d(1, 0, 0);
    glxDrawEdges(&(SS.nakedEdges));
    glLineStipple(4, 0xaaaa);
    glColor3d(0, 0, 0);
    glxDrawEdges(&(SS.nakedEdges));
    glDisable(GL_LINE_STIPPLE);

    // Then redraw whatever the mouse is hovering over, highlighted.
    glDisable(GL_DEPTH_TEST); 
    glxLockColorTo(1, 1, 0);
    hover.Draw();

    // And finally draw the selection, same mechanism.
    glxLockColorTo(1, 0, 0);
    for(i = 0; i < MAX_SELECTED; i++) {
        selection[i].Draw();
    }

    // And finally the toolbar.
    if(SS.showToolbar) {
        ToolbarDraw();
    }
}

