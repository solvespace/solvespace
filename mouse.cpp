//-----------------------------------------------------------------------------
// Anything relating to mouse, keyboard, or 6-DOF mouse input.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void GraphicsWindow::UpdateDraggedPoint(hEntity hp, double mx, double my) {
    Entity *p = SK.GetEntity(hp);
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
    if(context.active) return;

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

    if(rightDown && orig.mouse.DistanceTo(mp) < 5 && !orig.startedMoving) {
        // Avoid accidentally panning (or rotating if shift is down) if the
        // user wants a context menu.
        return;
    }
    orig.startedMoving = true;

    // If the middle button is down, then mouse movement is used to pan and
    // rotate our view. This wins over everything else.
    if(middleDown) {
        hover.Clear();

        double dx = (x - orig.mouse.x) / scale;
        double dy = (y - orig.mouse.y) / scale;

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
                Entity *e = SK.GetEntity(hover.entity);
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
                            SK.GetConstraint(hover.constraint)->HasLabel())
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
            Constraint *c = SK.constraint.FindById(pending.constraint);
            UpdateDraggedNum(&(c->disp.offset), x, y);
            break;
        }
        case DRAGGING_NEW_LINE_POINT:
            HitTestMakeSelection(mp);
            // and fall through
        case DRAGGING_NEW_POINT:
        case DRAGGING_POINT: {
            Entity *p = SK.GetEntity(pending.point);
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
            Vector p0 = SK.GetEntity(hr.entity(1))->PointGetNum();
            Vector p3 = SK.GetEntity(hr.entity(4))->PointGetNum();
            Vector p1 = p0.ScaledBy(2.0/3).Plus(p3.ScaledBy(1.0/3));
            SK.GetEntity(hr.entity(2))->PointForceTo(p1);
            Vector p2 = p0.ScaledBy(1.0/3).Plus(p3.ScaledBy(2.0/3));
            SK.GetEntity(hr.entity(3))->PointForceTo(p2);

            SS.MarkGroupDirtyByEntity(pending.point);
            break;
        }
        case DRAGGING_NEW_ARC_POINT: {
            UpdateDraggedPoint(pending.point, x, y);
            HitTestMakeSelection(mp);

            hRequest hr = pending.point.request();
            Vector ona = SK.GetEntity(hr.entity(2))->PointGetNum();
            Vector onb = SK.GetEntity(hr.entity(3))->PointGetNum();
            Vector center = (ona.Plus(onb)).ScaledBy(0.5);

            SK.GetEntity(hr.entity(1))->PointForceTo(center);

            SS.MarkGroupDirtyByEntity(pending.point);
            break;
        }
        case DRAGGING_NEW_RADIUS:
        case DRAGGING_RADIUS: {
            Entity *circle = SK.GetEntity(pending.circle);
            Vector center = SK.GetEntity(circle->point[0])->PointGetNum();
            Point2d c2 = ProjectPoint(center);
            double r = c2.DistanceTo(mp)/scale;
            SK.GetEntity(circle->distance)->DistanceForceTo(r);

            SS.MarkGroupDirtyByEntity(pending.circle);
            break;
        }

        case DRAGGING_NORMAL: {
            Entity *normal = SK.GetEntity(pending.normal);
            Vector p = SK.GetEntity(normal->point[0])->PointGetNum();
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

    orig.offset = offset;
    orig.projUp = projUp;
    orig.projRight = projRight;
    orig.mouse.x = x;
    orig.mouse.y = y;
    orig.startedMoving = false;
}

void GraphicsWindow::ContextMenuListStyles(void) {
    CreateContextSubmenu();
    Style *s;
    bool empty = true;
    for(s = SK.style.First(); s; s = SK.style.NextAfter(s)) {
        if(s->h.v < Style::FIRST_CUSTOM) continue;

        AddContextMenuItem(s->DescriptionString(), CMNU_FIRST_STYLE + s->h.v);
        empty = false;
    }

    if(!empty) AddContextMenuItem(NULL, CONTEXT_SEPARATOR);

    AddContextMenuItem("No Style", CMNU_NO_STYLE);
    AddContextMenuItem("Newly Created Custom Style...", CMNU_NEW_CUSTOM_STYLE);
}

void GraphicsWindow::MouseRightUp(double x, double y) {
    // Don't show a context menu if the user is right-clicking the toolbar,
    // or if they are finishing a pan.
    if(ToolbarMouseMoved((int)x, (int)y)) return;
    if(orig.startedMoving) return;

    if(context.active) return;
    context.active = true;

    if(pending.operation == DRAGGING_NEW_LINE_POINT) {
        // Special case; use a right click to stop drawing lines, since
        // a left click would draw another one. This is quicker and more
        // intuitive than hitting escape.
        ClearPending();
    }

    GroupSelection();
    if(hover.IsEmpty() && gs.n == 0 && gs.constraints == 0) {
        // No reason to display a context menu.
        goto done;
    }

    // We can either work on the selection (like the functions are designed to)
    // or on the hovered item. In the latter case we can fudge things by just
    // selecting the hovered item, and then applying our operation to the
    // selection.
    bool toggleForStyles = false,
         toggleForGroupInfo = false,
         toggleForDelete = false,
         toggleForStyleInfo = false;

    if(!hover.IsEmpty()) {
        AddContextMenuItem("Toggle Hovered Item Selection", 
            CMNU_TOGGLE_SELECTION);
    }

    if(gs.stylables > 0) {
        ContextMenuListStyles();
        AddContextMenuItem("Assign Selection to Style", CONTEXT_SUBMENU);
    } else if(gs.n == 0 && gs.constraints == 0 && hover.IsStylable()) {
        ContextMenuListStyles();
        AddContextMenuItem("Assign Hovered Item to Style", CONTEXT_SUBMENU);
        toggleForStyles = true;
    }

    if(gs.n + gs.constraints == 1) {
        AddContextMenuItem("Group Info for Selected Item", CMNU_GROUP_INFO);
    } else if(!hover.IsEmpty() && gs.n == 0 && gs.constraints == 0) {
        AddContextMenuItem("Group Info for Hovered Item", CMNU_GROUP_INFO);
        toggleForGroupInfo = true;
    }

    if(gs.n + gs.constraints == 1 && gs.stylables == 1) {
        AddContextMenuItem("Style Info for Selected Item", CMNU_STYLE_INFO);
    } else if(hover.IsStylable() && gs.n == 0 && gs.constraints == 0) {
        AddContextMenuItem("Style Info for Hovered Item", CMNU_STYLE_INFO);
        toggleForStyleInfo = true;
    }

    if(hover.constraint.v && gs.n == 0 && gs.constraints == 0) {
        Constraint *c = SK.GetConstraint(hover.constraint);
        if(c->HasLabel() && c->type != Constraint::COMMENT) {
            AddContextMenuItem("Toggle Reference Dimension",
                CMNU_REFERENCE_DIM);
        }
        if(c->type == Constraint::ANGLE ||
           c->type == Constraint::EQUAL_ANGLE)
        {
            AddContextMenuItem("Other Supplementary Angle",
                CMNU_OTHER_ANGLE);
        }
    }

    if(gs.n == 0 && gs.constraints == 0 &&
        (hover.constraint.v &&
          SK.GetConstraint(hover.constraint)->type == Constraint::COMMENT) ||
        (hover.entity.v &&
          SK.GetEntity(hover.entity)->IsPoint()))
    {
        AddContextMenuItem("Snap to Grid", CMNU_SNAP_TO_GRID);
    }

    if(gs.n > 0 || gs.constraints > 0) {
        AddContextMenuItem(NULL, CONTEXT_SEPARATOR);
        AddContextMenuItem("Delete Selection", CMNU_DELETE_SEL);
        AddContextMenuItem("Unselect All", CMNU_UNSELECT_ALL);
    } else if(!hover.IsEmpty()) {
        AddContextMenuItem(NULL, CONTEXT_SEPARATOR);
        AddContextMenuItem("Delete Hovered Item", CMNU_DELETE_SEL);
        toggleForDelete = true;
    }

    int ret = ShowContextMenu();
    switch(ret) {
        case CMNU_TOGGLE_SELECTION:
            ToggleSelectionStateOfHovered();
            break;

        case CMNU_UNSELECT_ALL:
            MenuEdit(MNU_UNSELECT_ALL);
            break;

        case CMNU_DELETE_SEL:
            if(toggleForDelete) ToggleSelectionStateOfHovered();
            MenuEdit(MNU_DELETE);
            break;

        case CMNU_REFERENCE_DIM:
            ToggleSelectionStateOfHovered();
            Constraint::MenuConstrain(MNU_REFERENCE);
            break;

        case CMNU_OTHER_ANGLE:
            ToggleSelectionStateOfHovered();
            Constraint::MenuConstrain(MNU_OTHER_ANGLE);
            break;

        case CMNU_SNAP_TO_GRID:
            ToggleSelectionStateOfHovered();
            MenuEdit(MNU_SNAP_TO_GRID);
            break;

        case CMNU_GROUP_INFO: {
            if(toggleForGroupInfo) ToggleSelectionStateOfHovered();

            hGroup hg;
            GroupSelection();
            if(gs.entities == 1) {
                hg = SK.GetEntity(gs.entity[0])->group;
            } else if(gs.points == 1) {
                hg = SK.GetEntity(gs.point[0])->group;
            } else if(gs.constraints == 1) {
                hg = SK.GetConstraint(gs.constraint[0])->group;
            } else {
                break;
            }
            ClearSelection();

            SS.TW.GoToScreen(TextWindow::SCREEN_GROUP_INFO);
            SS.TW.shown.group = hg;
            SS.later.showTW = true;
            break;
        }

        case CMNU_STYLE_INFO: {
            if(toggleForStyleInfo) ToggleSelectionStateOfHovered();

            hStyle hs;
            GroupSelection();
            if(gs.entities == 1) {
                hs = Style::ForEntity(gs.entity[0]);
            } else if(gs.points == 1) {
                hs = Style::ForEntity(gs.point[0]);
            } else if(gs.constraints == 1) {
                hs = SK.GetConstraint(gs.constraint[0])->disp.style;
                if(!hs.v) hs.v = Style::CONSTRAINT;
            } else {
                break;
            }
            ClearSelection();

            SS.TW.GoToScreen(TextWindow::SCREEN_STYLE_INFO);
            SS.TW.shown.style = hs;
            SS.later.showTW = true;
            break;
        }

        case CMNU_NEW_CUSTOM_STYLE: {
            if(toggleForStyles) ToggleSelectionStateOfHovered();
            DWORD v = Style::CreateCustomStyle();
            Style::AssignSelectionToStyle(v);
            break;
        }

        case CMNU_NO_STYLE:
            if(toggleForStyles) ToggleSelectionStateOfHovered();
            Style::AssignSelectionToStyle(0);
            break;

        default:
            if(ret >= CMNU_FIRST_STYLE) {
                if(toggleForStyles) ToggleSelectionStateOfHovered();
                Style::AssignSelectionToStyle(ret - CMNU_FIRST_STYLE);
            }
            // otherwise it was probably cancelled, so do nothing
            break;
    }

done:
    context.active = false;
}

hRequest GraphicsWindow::AddRequest(int type) {
    return AddRequest(type, true);
}
hRequest GraphicsWindow::AddRequest(int type, bool rememberForUndo) {
    if(rememberForUndo) SS.UndoRemember();

    Request r;
    memset(&r, 0, sizeof(r));
    r.group = activeGroup;
    Group *g = SK.GetGroup(activeGroup);
    if(g->type == Group::DRAWING_3D || g->type == Group::DRAWING_WORKPLANE) {
        r.construction = false;
    } else {
        r.construction = true;
    }
    r.workplane = ActiveWorkplane();
    r.type = type;
    SK.request.AddAndAssignId(&r);

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

    Entity *e = SK.GetEntity(hover.entity);
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
            SK.GetEntity(hr.entity(0))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(0));

            ClearSuper();
            break;

        case MNU_LINE_SEGMENT:
            hr = AddRequest(Request::LINE_SEGMENT);
            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(1));

            ClearSuper();

            pending.operation = DRAGGING_NEW_LINE_POINT;
            pending.point = hr.entity(2);
            pending.description = "click to place next point of line";
            SK.GetEntity(pending.point)->PointForceTo(v);
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
                SK.GetEntity(lns[i].entity(1))->PointForceTo(v);
                SK.GetEntity(lns[i].entity(2))->PointForceTo(v);
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
            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            SK.GetEntity(hr.entity(32))->NormalForceTo(
                Quaternion::From(SS.GW.projRight, SS.GW.projUp));
            ConstrainPointByHovered(hr.entity(1));

            ClearSuper();

            pending.operation = DRAGGING_NEW_RADIUS;
            pending.circle = hr.entity(0);
            pending.description = "click to set radius";
            SK.GetParam(hr.param(0))->val = 0;
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
            SK.GetEntity(hr.entity(1))->PointForceTo(v.Minus(adj));
            SK.GetEntity(hr.entity(2))->PointForceTo(v);
            SK.GetEntity(hr.entity(3))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(2));

            ClearSuper();

            pending.operation = DRAGGING_NEW_ARC_POINT;
            pending.point = hr.entity(3);
            pending.description = "click to place point";
            break;
        }
        case MNU_CUBIC:
            hr = AddRequest(Request::CUBIC);
            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            SK.GetEntity(hr.entity(2))->PointForceTo(v);
            SK.GetEntity(hr.entity(3))->PointForceTo(v);
            SK.GetEntity(hr.entity(4))->PointForceTo(v);
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
            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            SK.GetEntity(hr.entity(32))->NormalForceTo( 
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
            Request *r = SK.GetRequest(hr);
            r->str.strcpy("Abc");
            r->font.strcpy("arial.ttf");

            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            SK.GetEntity(hr.entity(2))->PointForceTo(v);

            pending.operation = DRAGGING_NEW_POINT;
            pending.point = hr.entity(2);
            pending.description = "click to place bottom left of text";
            break;
        }

        case MNU_COMMENT: {
            ClearSuper();
            Constraint c;
            ZERO(&c);
            c.group       = SS.GW.activeGroup;
            c.workplane   = SS.GW.ActiveWorkplane();
            c.type        = Constraint::COMMENT;
            c.disp.offset = v;
            c.comment.strcpy("NEW COMMENT -- DOUBLE-CLICK TO EDIT");
            Constraint::AddConstraint(&c);
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
            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            SK.GetEntity(hr.entity(2))->PointForceTo(v);

            // Constrain the line segments to share an endpoint
            Constraint::ConstrainCoincident(pending.point, hr.entity(1));

            // And drag an endpoint of the new line segment
            pending.operation = DRAGGING_NEW_LINE_POINT;
            pending.point = hr.entity(2);
            pending.description = "click to place next point of next line";

            break;
        }

        case 0:
        default:
            ClearPending();
            ToggleSelectionStateOfHovered();
            break;
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

        Constraint *c = SK.GetConstraint(constraintBeingEdited);
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
    Constraint *c = SK.GetConstraint(constraintBeingEdited);

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

bool GraphicsWindow::KeyDown(int c) {
    if(c == ('h' - 'a') + 1) {
        // Treat backspace identically to escape.
        MenuEdit(MNU_UNSELECT_ALL);
        return true;
    }

    return false;
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
    // Un-hover everything when the mouse leaves our window, unless there's
    // currently a context menu shown.
    if(!context.active) {
        hover.Clear();
        toolbarTooltipped = 0;
        toolbarHovered = 0;
        PaintGraphics();
    }
    if(pending.operation == DRAGGING_POINT) {
        ClearPending();
    }
}

void GraphicsWindow::SpaceNavigatorMoved(double tx, double ty, double tz,
                                         double rx, double ry, double rz,
                                         bool shiftDown)
{
    if(!havePainted) return;
    Vector out = projRight.Cross(projUp);

    // rotation vector is axis of rotation, and its magnitude is angle
    Vector aa = Vector::From(rx, ry, rz);
    // but it's given with respect to screen projection frame
    aa = aa.ScaleOutOfCsys(projRight, projUp, out);
    double aam = aa.Magnitude();
    if(aam != 0.0) aa = aa.WithMagnitude(1);

    // This can either transform our view, or transform an imported part.
    GroupSelection();
    Entity *e = NULL;
    Group *g = NULL;
    if(gs.points == 1   && gs.n == 1) e = SK.GetEntity(gs.point [0]);
    if(gs.entities == 1 && gs.n == 1) e = SK.GetEntity(gs.entity[0]);
    if(e) g = SK.GetGroup(e->group);
    if(g && g->type == Group::IMPORTED && !shiftDown) {
        // Apply the transformation to an imported part. Gain down the Z
        // axis, since it's hard to see what you're doing on that one since
        // it's normal to the screen.
        Vector t = projRight.ScaledBy(tx/scale).Plus(
                   projUp   .ScaledBy(ty/scale).Plus(
                   out      .ScaledBy(0.1*tz/scale)));
        Quaternion q = Quaternion::From(aa, aam);

        // If we go five seconds without SpaceNavigator input, or if we've
        // switched groups, then consider that a new action and save an undo
        // point.
        SDWORD now = GetMilliseconds();
        if(now - lastSpaceNavigatorTime > 5000 ||
           lastSpaceNavigatorGroup.v != g->h.v)
        {
            SS.UndoRemember();
        }

        g->TransformImportedBy(t, q);

        lastSpaceNavigatorTime = now;
        lastSpaceNavigatorGroup = g->h;
        SS.MarkGroupDirty(g->h);
        SS.later.generateAll = true;
    } else {
        // Apply the transformation to the view of the everything. The
        // x and y components are translation; but z component is scale,
        // not translation, or else it would do nothing in a parallel
        // projection
        offset = offset.Plus(projRight.ScaledBy(tx/scale));
        offset = offset.Plus(projUp.ScaledBy(ty/scale));
        scale *= exp(0.001*tz); 

        if(aam != 0.0) {
            projRight = projRight.RotatedAbout(aa, -aam);
            projUp    = projUp.   RotatedAbout(aa, -aam);
            NormalizeProjectionVectors();
        }
    }

    havePainted = false;
    InvalidateGraphics();
}

void GraphicsWindow::SpaceNavigatorButtonUp(void) {
    ZoomToFit(false);
    InvalidateGraphics();
}

