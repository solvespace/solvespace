//-----------------------------------------------------------------------------
// Anything relating to mouse, keyboard, or 6-DOF mouse input.
//
// Copyright 2008-2013 Jonathan Westhues.
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
}

void GraphicsWindow::AddPointToDraggedList(hEntity hp) {
    Entity *p = SK.GetEntity(hp);
    // If an entity and its points are both selected, then its points could
    // end up in the list twice. This would be bad, because it would move
    // twice as far as the mouse pointer...
    List<hEntity> *lhe = &(pending.points);
    for(hEntity *hee = lhe->First(); hee; hee = lhe->NextAfter(hee)) {
        if(hee->v == hp.v) {
            // Exact same point.
            return;
        }
        Entity *pe = SK.GetEntity(*hee);
        if(pe->type == p->type &&
           pe->type != Entity::POINT_IN_2D &&
           pe->type != Entity::POINT_IN_3D &&
           pe->group.v == p->group.v)
        {
            // Transform-type point, from the same group. So it handles the
            // same unknowns.
            return;
        }
    }
    pending.points.Add(&hp);
}

void GraphicsWindow::StartDraggingByEntity(hEntity he) {
    Entity *e = SK.GetEntity(he);
    if(e->IsPoint()) {
        AddPointToDraggedList(e->h);
    } else if(e->type == Entity::LINE_SEGMENT ||
              e->type == Entity::ARC_OF_CIRCLE ||
              e->type == Entity::CUBIC ||
              e->type == Entity::CUBIC_PERIODIC ||
              e->type == Entity::CIRCLE ||
              e->type == Entity::TTF_TEXT)
    {
        int pts;
        EntReqTable::GetEntityInfo(e->type, e->extraPoints,
            NULL, &pts, NULL, NULL);
        for(int i = 0; i < pts; i++) {
            AddPointToDraggedList(e->point[i]);
        }
    }
}

void GraphicsWindow::StartDraggingBySelection(void) {
    List<Selection> *ls = &(selection);
    for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
        if(!s->entity.v) continue;

        StartDraggingByEntity(s->entity);
    }
    // The user might select a point, and then click it again to start
    // dragging; but the point just got unselected by that click. So drag
    // the hovered item too, and they'll always have it.
    if(hover.entity.v) StartDraggingByEntity(hover.entity);
}

void GraphicsWindow::MouseMoved(double x, double y, bool leftDown,
            bool middleDown, bool rightDown, bool shiftDown, bool ctrlDown)
{
    if(GraphicsEditControlIsVisible()) return;
    if(context.active) return;

    SS.extraLine.draw = false;

    if(!orig.mouseDown) {
        // If someone drags the mouse into our window with the left button
        // already depressed, then we don't have our starting point; so
        // don't try.
        leftDown = false;
    }

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

    if(!leftDown && (pending.operation == DRAGGING_POINTS ||
                     pending.operation == DRAGGING_MARQUEE))
    {
        ClearPending();
        InvalidateGraphics();
    }

    Point2d mp = Point2d::From(x, y);
    currentMousePosition = mp;

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
            SS.extraLine.draw = true;
            SS.extraLine.ptA = UnProjectPoint(Point2d::From(0, 0));
            SS.extraLine.ptB = UnProjectPoint(mp);

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

        if(SS.TW.shown.screen == TextWindow::SCREEN_EDIT_VIEW) {
            if(havePainted) {
                SS.ScheduleShowTW();
            }
        }
        InvalidateGraphics();
        havePainted = false;
        return;
    }

    if(pending.operation == 0) {
        double dm = orig.mouse.DistanceTo(mp);
        // If we're currently not doing anything, then see if we should
        // start dragging something.
        if(leftDown && dm > 3) {
            Entity *e = NULL;
            if(hover.entity.v) e = SK.GetEntity(hover.entity);
            if(e && e->type != Entity::WORKPLANE) {
                Entity *e = SK.GetEntity(hover.entity);
                if(e->type == Entity::CIRCLE && selection.n <= 1) {
                    // Drag the radius.
                    ClearSelection();
                    pending.circle = hover.entity;
                    pending.operation = DRAGGING_RADIUS;
                } else if(e->IsNormal()) {
                    ClearSelection();
                    pending.normal = hover.entity;
                    pending.operation = DRAGGING_NORMAL;
                } else {
                    if(!hoverWasSelectedOnMousedown) {
                        // The user clicked an unselected entity, which
                        // means they're dragging just the hovered thing,
                        // not the full selection. So clear all the selection
                        // except that entity.
                        ClearSelection();
                        MakeSelected(e->h);
                    }
                    StartDraggingBySelection();
                    if(!hoverWasSelectedOnMousedown) {
                        // And then clear the selection again, since they
                        // probably didn't want that selected if they just
                        // were dragging it.
                        ClearSelection();
                    }
                    hover.Clear();
                    pending.operation = DRAGGING_POINTS;
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
            } else {
                if(!hover.constraint.v) {
                    // That's just marquee selection, which should not cause
                    // an undo remember.
                    if(dm > 10) {
                        if(hover.entity.v) {
                            // Avoid accidentally selecting workplanes when
                            // starting drags.
                            MakeUnselected(hover.entity, false);
                            hover.Clear();
                        }
                        pending.operation = DRAGGING_MARQUEE;
                        orig.marqueePoint =
                            UnProjectPoint(orig.mouseOnButtonDown);
                    }
                }
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
    if(!havePainted) {
        if(pending.operation == DRAGGING_POINTS && ctrlDown) {
            SS.extraLine.ptA = UnProjectPoint(orig.mouseOnButtonDown);
            SS.extraLine.ptB = UnProjectPoint(mp);
            SS.extraLine.draw = true;
        }
        return;
    }
    switch(pending.operation) {
        case DRAGGING_CONSTRAINT: {
            Constraint *c = SK.constraint.FindById(pending.constraint);
            UpdateDraggedNum(&(c->disp.offset), x, y);
            orig.mouse = mp;
            InvalidateGraphics();
            break;
        }

        case DRAGGING_NEW_LINE_POINT:
        case DRAGGING_NEW_POINT:
            UpdateDraggedPoint(pending.point, x, y);
            HitTestMakeSelection(mp);
            SS.MarkGroupDirtyByEntity(pending.point);
            orig.mouse = mp;
            InvalidateGraphics();
            break;

        case DRAGGING_POINTS:
            if(shiftDown || ctrlDown) {
                // Edit the rotation associated with a POINT_N_ROT_TRANS,
                // either within (ctrlDown) or out of (shiftDown) the plane
                // of the screen. So first get the rotation to apply, in qt.
                Quaternion qt;
                if(ctrlDown) {
                    double d = mp.DistanceTo(orig.mouseOnButtonDown);
                    if(d < 25) {
                        // Don't start dragging the position about the normal
                        // until we're a little ways out, to get a reasonable
                        // reference pos
                        orig.mouse = mp;
                        break;
                    }
                    double theta = atan2(orig.mouse.y-orig.mouseOnButtonDown.y,
                                         orig.mouse.x-orig.mouseOnButtonDown.x);
                    theta -= atan2(y-orig.mouseOnButtonDown.y,
                                   x-orig.mouseOnButtonDown.x);

                    Vector gn = projRight.Cross(projUp);
                    qt = Quaternion::From(gn, -theta);

                    SS.extraLine.draw = true;
                    SS.extraLine.ptA = UnProjectPoint(orig.mouseOnButtonDown);
                    SS.extraLine.ptB = UnProjectPoint(mp);
                } else {
                    double dx = -(x - orig.mouse.x);
                    double dy = -(y - orig.mouse.y);
                    double s = 0.3*(PI/180); // degrees per pixel
                    qt = Quaternion::From(projUp,   -s*dx).Times(
                         Quaternion::From(projRight, s*dy));
                }
                orig.mouse = mp;

                // Now apply this rotation to the points being dragged.
                List<hEntity> *lhe = &(pending.points);
                for(hEntity *he = lhe->First(); he; he = lhe->NextAfter(he)) {
                    Entity *e = SK.GetEntity(*he);
                    if(e->type != Entity::POINT_N_ROT_TRANS) {
                        if(ctrlDown) {
                            Vector p = e->PointGetNum();
                            p = p.Minus(SS.extraLine.ptA);
                            p = qt.Rotate(p);
                            p = p.Plus(SS.extraLine.ptA);
                            e->PointForceTo(p);
                            SS.MarkGroupDirtyByEntity(e->h);
                        }
                        continue;
                    }

                    Quaternion q = e->PointGetQuaternion();
                    Vector     p = e->PointGetNum();
                    q = qt.Times(q);
                    e->PointForceQuaternionTo(q);
                    // Let's rotate about the selected point; so fix up the
                    // translation so that that point didn't move.
                    e->PointForceTo(p);
                    SS.MarkGroupDirtyByEntity(e->h);
                }
            } else {
                List<hEntity> *lhe = &(pending.points);
                for(hEntity *he = lhe->First(); he; he = lhe->NextAfter(he)) {
                    UpdateDraggedPoint(*he, x, y);
                    SS.MarkGroupDirtyByEntity(*he);
                }
                orig.mouse = mp;
            }
            break;

        case DRAGGING_NEW_CUBIC_POINT: {
            UpdateDraggedPoint(pending.point, x, y);
            HitTestMakeSelection(mp);

            hRequest hr = pending.point.request();
            if(pending.point.v == hr.entity(4).v) {
                // The very first segment; dragging final point drags both
                // tangent points.
                Vector p0 = SK.GetEntity(hr.entity(1))->PointGetNum(),
                       p3 = SK.GetEntity(hr.entity(4))->PointGetNum(),
                       p1 = p0.ScaledBy(2.0/3).Plus(p3.ScaledBy(1.0/3)),
                       p2 = p0.ScaledBy(1.0/3).Plus(p3.ScaledBy(2.0/3));
                SK.GetEntity(hr.entity(1+1))->PointForceTo(p1);
                SK.GetEntity(hr.entity(1+2))->PointForceTo(p2);
            } else {
                // A subsequent segment; dragging point drags only final
                // tangent point.
                int i = SK.GetEntity(hr.entity(0))->extraPoints;
                Vector pn   = SK.GetEntity(hr.entity(4+i))->PointGetNum(),
                       pnm2 = SK.GetEntity(hr.entity(2+i))->PointGetNum(),
                       pnm1 = (pn.Plus(pnm2)).ScaledBy(0.5);
                SK.GetEntity(hr.entity(3+i))->PointForceTo(pnm1);
            }

            orig.mouse = mp;
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

            orig.mouse = mp;
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

        case DRAGGING_MARQUEE:
            orig.mouse = mp;
            InvalidateGraphics();
            break;

        default: oops();
    }

    if(pending.operation != 0 &&
       pending.operation != DRAGGING_CONSTRAINT &&
       pending.operation != DRAGGING_MARQUEE)
    {
        SS.GenerateAll();
    }
    havePainted = false;
}

void GraphicsWindow::ClearPending(void) {
    pending.points.Clear();
    pending = {};
    SS.ScheduleShowTW();
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
    SS.extraLine.draw = false;
    InvalidateGraphics();

    // Don't show a context menu if the user is right-clicking the toolbar,
    // or if they are finishing a pan.
    if(ToolbarMouseMoved((int)x, (int)y)) return;
    if(orig.startedMoving) return;

    if(context.active) return;

    if(pending.operation == DRAGGING_NEW_LINE_POINT) {
        SuggestedConstraint suggested =
            SS.GW.SuggestLineConstraint(SS.GW.pending.request);
        if(suggested != SUGGESTED_NONE) {
            Constraint::Constrain(suggested,
                Entity::NO_ENTITY, Entity::NO_ENTITY, pending.request.entity(0));
        }
    }

    if(pending.operation == DRAGGING_NEW_LINE_POINT ||
       pending.operation == DRAGGING_NEW_CUBIC_POINT)
    {
        // Special case; use a right click to stop drawing lines, since
        // a left click would draw another one. This is quicker and more
        // intuitive than hitting escape. Likewise for new cubic segments.
        ClearPending();
        return;
    }

    context.active = true;

    if(!hover.IsEmpty()) {
        MakeSelected(&hover);
        SS.ScheduleShowTW();
    }
    GroupSelection();

    bool itemsSelected = (gs.n > 0 || gs.constraints > 0);

    if(itemsSelected) {
        if(gs.stylables > 0) {
            ContextMenuListStyles();
            AddContextMenuItem("Assign to Style", CONTEXT_SUBMENU);
        }
        if(gs.n + gs.constraints == 1) {
            AddContextMenuItem("Group Info", CMNU_GROUP_INFO);
        }
        if(gs.n + gs.constraints == 1 && gs.stylables == 1) {
            AddContextMenuItem("Style Info", CMNU_STYLE_INFO);
        }
        if(gs.withEndpoints > 0) {
            AddContextMenuItem("Select Edge Chain", CMNU_SELECT_CHAIN);
        }
        if(gs.constraints == 1 && gs.n == 0) {
            Constraint *c = SK.GetConstraint(gs.constraint[0]);
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
        if(gs.comments > 0 || gs.points > 0) {
            AddContextMenuItem("Snap to Grid", CMNU_SNAP_TO_GRID);
        }

        if(gs.points == 1) {
            Entity *p = SK.GetEntity(gs.point[0]);
            Constraint *c;
            IdList<Constraint,hConstraint> *lc = &(SK.constraint);
            for(c = lc->First(); c; c = lc->NextAfter(c)) {
                if(c->type != Constraint::POINTS_COINCIDENT) continue;
                if(c->ptA.v == p->h.v || c->ptB.v == p->h.v) {
                    break;
                }
            }
            if(c) {
                AddContextMenuItem("Delete Point-Coincident Constraint",
                                   CMNU_DEL_COINCIDENT);
            }
        }
        AddContextMenuItem(NULL, CONTEXT_SEPARATOR);
        if(LockedInWorkplane()) {
            AddContextMenuItem("Cut",  CMNU_CUT_SEL);
            AddContextMenuItem("Copy", CMNU_COPY_SEL);
        }
    }

    if(SS.clipboard.r.n > 0 && LockedInWorkplane()) {
        AddContextMenuItem("Paste", CMNU_PASTE_SEL);
    }

    if(itemsSelected) {
        AddContextMenuItem("Delete", CMNU_DELETE_SEL);
        AddContextMenuItem(NULL, CONTEXT_SEPARATOR);
        AddContextMenuItem("Unselect All", CMNU_UNSELECT_ALL);
    }
    // If only one item is selected, then it must be the one that we just
    // selected from the hovered item; in which case unselect all and hovered
    // are equivalent.
    if(!hover.IsEmpty() && selection.n > 1) {
        AddContextMenuItem("Unselect Hovered", CMNU_UNSELECT_HOVERED);
    }

    int ret = ShowContextMenu();
    switch(ret) {
        case CMNU_UNSELECT_ALL:
            MenuEdit(MNU_UNSELECT_ALL);
            break;

        case CMNU_UNSELECT_HOVERED:
            if(!hover.IsEmpty()) {
                MakeUnselected(&hover, true);
            }
            break;

        case CMNU_SELECT_CHAIN:
            MenuEdit(MNU_SELECT_CHAIN);
            break;

        case CMNU_CUT_SEL:
            MenuClipboard(MNU_CUT);
            break;

        case CMNU_COPY_SEL:
            MenuClipboard(MNU_COPY);
            break;

        case CMNU_PASTE_SEL:
            MenuClipboard(MNU_PASTE);
            break;

        case CMNU_DELETE_SEL:
            MenuClipboard(MNU_DELETE);
            break;

        case CMNU_REFERENCE_DIM:
            Constraint::MenuConstrain(MNU_REFERENCE);
            break;

        case CMNU_OTHER_ANGLE:
            Constraint::MenuConstrain(MNU_OTHER_ANGLE);
            break;

        case CMNU_DEL_COINCIDENT: {
            SS.UndoRemember();
            if(!gs.point[0].v) break;
            Entity *p = SK.GetEntity(gs.point[0]);
            if(!p->IsPoint()) break;

            SK.constraint.ClearTags();
            Constraint *c;
            for(c = SK.constraint.First(); c; c = SK.constraint.NextAfter(c)) {
                if(c->type != Constraint::POINTS_COINCIDENT) continue;
                if(c->ptA.v == p->h.v || c->ptB.v == p->h.v) {
                    c->tag = 1;
                }
            }
            SK.constraint.RemoveTagged();
            ClearSelection();
            break;
        }

        case CMNU_SNAP_TO_GRID:
            MenuEdit(MNU_SNAP_TO_GRID);
            break;

        case CMNU_GROUP_INFO: {
            hGroup hg;
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
            SS.ScheduleShowTW();
            break;
        }

        case CMNU_STYLE_INFO: {
            hStyle hs;
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
            SS.ScheduleShowTW();
            break;
        }

        case CMNU_NEW_CUSTOM_STYLE: {
            uint32_t v = Style::CreateCustomStyle();
            Style::AssignSelectionToStyle(v);
            break;
        }

        case CMNU_NO_STYLE:
            Style::AssignSelectionToStyle(0);
            break;

        default:
            if(ret >= CMNU_FIRST_STYLE) {
                Style::AssignSelectionToStyle(ret - CMNU_FIRST_STYLE);
            } else {
                // otherwise it was cancelled, so do nothing
                contextMenuCancelTime = GetMilliseconds();
            }
            break;
    }

    context.active = false;
    SS.ScheduleShowTW();
}

hRequest GraphicsWindow::AddRequest(int type) {
    return AddRequest(type, true);
}
hRequest GraphicsWindow::AddRequest(int type, bool rememberForUndo) {
    if(rememberForUndo) SS.UndoRemember();

    Request r = {};
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
    orig.mouseDown = true;

    if(GraphicsEditControlIsVisible()) {
        orig.mouse = Point2d::From(mx, my);
        orig.mouseOnButtonDown = orig.mouse;
        HideGraphicsEditControl();
        return;
    }
    SS.TW.HideEditControl();

    if(SS.showToolbar) {
        if(ToolbarMouseDown((int)mx, (int)my)) return;
    }

    // Make sure the hover is up to date.
    MouseMoved(mx, my, false, false, false, false, false);
    orig.mouse.x = mx;
    orig.mouse.y = my;
    orig.mouseOnButtonDown = orig.mouse;

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
        case MNU_CONSTR_SEGMENT:
            hr = AddRequest(Request::LINE_SEGMENT);
            SK.GetRequest(hr)->construction = (pending.operation == MNU_CONSTR_SEGMENT);
            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(1));

            ClearSuper();

            pending.operation = DRAGGING_NEW_LINE_POINT;
            pending.request = hr;
            pending.point = hr.entity(2);
            pending.description = "click next point of line, or press Esc";
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
            // Centered where we clicked
            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            // Normal to the screen
            SK.GetEntity(hr.entity(32))->NormalForceTo(
                Quaternion::From(SS.GW.projRight, SS.GW.projUp));
            // Initial radius zero
            SK.GetEntity(hr.entity(64))->DistanceForceTo(0);

            ConstrainPointByHovered(hr.entity(1));

            ClearSuper();

            pending.operation = DRAGGING_NEW_RADIUS;
            pending.circle = hr.entity(0);
            pending.description = "click to set radius";
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
            pending.description = "click next point of cubic, or press Esc";
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
            Constraint c = {};
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
            ConstrainPointByHovered(pending.point);
            ClearPending();
            break;

        case DRAGGING_NEW_CUBIC_POINT: {
            hRequest hr = pending.point.request();
            Request *r = SK.GetRequest(hr);

            if(hover.entity.v == hr.entity(1).v && r->extraPoints >= 2) {
                // They want the endpoints coincident, which means a periodic
                // spline instead.
                r->type = Request::CUBIC_PERIODIC;
                // Remove the off-curve control points, which are no longer
                // needed here; so move [2,ep+1] down, skipping first pt.
                int i;
                for(i = 2; i <= r->extraPoints+1; i++) {
                    SK.GetEntity(hr.entity((i-1)+1))->PointForceTo(
                        SK.GetEntity(hr.entity(i+1))->PointGetNum());
                }
                // and move ep+3 down by two, skipping both
                SK.GetEntity(hr.entity((r->extraPoints+1)+1))->PointForceTo(
                  SK.GetEntity(hr.entity((r->extraPoints+3)+1))->PointGetNum());
                r->extraPoints -= 2;
                // And we're done.
                SS.MarkGroupDirty(r->group);
                SS.ScheduleGenerateAll();
                ClearPending();
                break;
            }

            if(ConstrainPointByHovered(pending.point)) {
                ClearPending();
                break;
            }

            Entity e;
            if(r->extraPoints >= (int)arraylen(e.point) - 4) {
                ClearPending();
                break;
            }

            (SK.GetRequest(hr)->extraPoints)++;
            SS.GenerateAll(-1, -1);

            int ep = r->extraPoints;
            Vector last = SK.GetEntity(hr.entity(3+ep))->PointGetNum();

            SK.GetEntity(hr.entity(2+ep))->PointForceTo(last);
            SK.GetEntity(hr.entity(3+ep))->PointForceTo(v);
            SK.GetEntity(hr.entity(4+ep))->PointForceTo(v);
            pending.point = hr.entity(4+ep);
            break;
        }

        case DRAGGING_NEW_LINE_POINT: {
            // Constrain the line segment horizontal or vertical if close enough
            SuggestedConstraint suggested =
                SS.GW.SuggestLineConstraint(SS.GW.pending.request);
            if(suggested != SUGGESTED_NONE) {
                Constraint::Constrain(suggested,
                    Entity::NO_ENTITY, Entity::NO_ENTITY, pending.request.entity(0));
            }

            if(hover.entity.v) {
                Entity *e = SK.GetEntity(hover.entity);
                if(e->IsPoint()) {
                    hRequest hrl = pending.point.request();
                    Entity *sp = SK.GetEntity(hrl.entity(1));
                    if(( e->PointGetNum()).Equals(
                       (sp->PointGetNum())))
                    {
                        // If we constrained by the hovered point, then we
                        // would create a zero-length line segment. That's
                        // not good, so just stop drawing.
                        ClearPending();
                        break;
                    }
                }
            }

            if(ConstrainPointByHovered(pending.point)) {
                ClearPending();
                break;
            }

            // Create a new line segment, so that we continue drawing.
            hRequest hr = AddRequest(Request::LINE_SEGMENT);
            SK.GetRequest(hr)->construction = SK.GetRequest(pending.request)->construction;
            SK.GetEntity(hr.entity(1))->PointForceTo(v);
            // Displace the second point of the new line segment slightly,
            // to avoid creating zero-length edge warnings.
            SK.GetEntity(hr.entity(2))->PointForceTo(
                v.Plus(projRight.ScaledBy(0.5/scale)));

            // Constrain the line segments to share an endpoint
            Constraint::ConstrainCoincident(pending.point, hr.entity(1));

            // And drag an endpoint of the new line segment
            pending.operation = DRAGGING_NEW_LINE_POINT;
            pending.request = hr;
            pending.point = hr.entity(2);
            pending.description = "click next point of line, or press Esc";

            break;
        }

        case 0:
        default:
            ClearPending();
            if(!hover.IsEmpty()) {
                hoverWasSelectedOnMousedown = IsSelected(&hover);
                MakeSelected(&hover);
            }
            break;
    }

    SS.ScheduleShowTW();
    InvalidateGraphics();
}

void GraphicsWindow::MouseLeftUp(double mx, double my) {
    orig.mouseDown = false;
    hoverWasSelectedOnMousedown = false;

    switch(pending.operation) {
        case DRAGGING_POINTS:
            SS.extraLine.draw = false;
            // fall through
        case DRAGGING_CONSTRAINT:
        case DRAGGING_NORMAL:
        case DRAGGING_RADIUS:
            ClearPending();
            InvalidateGraphics();
            break;

        case DRAGGING_MARQUEE:
            SelectByMarquee();
            ClearPending();
            InvalidateGraphics();
            break;

        case 0:
            // We need to clear the selection here, and not in the mouse down
            // event, since a mouse down without anything hovered could also
            // be the start of marquee selection. But don't do that on the
            // left click to cancel a context menu. The time delay is an ugly
            // hack.
            if(hover.IsEmpty() &&
                (contextMenuCancelTime == 0 ||
                 (GetMilliseconds() - contextMenuCancelTime) > 200))
            {
                ClearSelection();
            }
            break;

        default:
            break;  // do nothing
    }
}

void GraphicsWindow::MouseLeftDoubleClick(double mx, double my) {
    if(GraphicsEditControlIsVisible()) return;
    SS.TW.HideEditControl();

    if(hover.constraint.v) {
        constraintBeingEdited = hover.constraint;
        ClearSuper();

        Constraint *c = SK.GetConstraint(constraintBeingEdited);
        if(!c->HasLabel()) {
            // Not meaningful to edit a constraint without a dimension
            return;
        }
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
            case Constraint::LENGTH_DIFFERENCE:
                sprintf(s, "%.3f", c->valA);
                break;

            default: {
                double v = fabs(c->valA);

                // If displayed as radius, also edit as radius.
                if(c->type == Constraint::DIAMETER && c->other)
                    v /= 2;

                char *def = SS.MmToString(v);
                double eps = 1e-12;
                if(fabs(SS.StringToMm(def) - v) < eps) {
                    // Show value with default number of digits after decimal,
                    // which is at least enough to represent it exactly.
                    strcpy(s, def);
                } else {
                    // Show value with as many digits after decimal as
                    // required to represent it exactly, up to 10.
                    v /= SS.MmPerUnit();
                    int i;
                    for(i = 0; i <= 10; i++) {
                        sprintf(s, "%.*f", i, v);
                        if(fabs(atof(s) - v) < eps) break;
                    }
                }
                break;
            }
        }
        ShowGraphicsEditControl((int)p2.x, (int)p2.y-4, s);
    }
}

void GraphicsWindow::EditControlDone(const char *s) {
    HideGraphicsEditControl();
    Constraint *c = SK.GetConstraint(constraintBeingEdited);

    if(c->type == Constraint::COMMENT) {
        SS.UndoRemember();
        c->comment.strcpy(s);
        return;
    }

    Expr *e = Expr::From(s, true);
    if(e) {
        SS.UndoRemember();

        switch(c->type) {
            case Constraint::PROJ_PT_DISTANCE:
            case Constraint::PT_LINE_DISTANCE:
            case Constraint::PT_FACE_DISTANCE:
            case Constraint::PT_PLANE_DISTANCE:
            case Constraint::LENGTH_DIFFERENCE: {
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

            case Constraint::DIAMETER:
                c->valA = fabs(SS.ExprToMm(e));

                // If displayed and edited as radius, convert back
                // to diameter
                if(c->other)
                    c->valA *= 2;
                break;

            default:
                // These are always positive, and they get the units conversion.
                c->valA = fabs(SS.ExprToMm(e));
                break;
        }
        SS.MarkGroupDirty(c->group);
        SS.GenerateAll();
    }
}

bool GraphicsWindow::KeyDown(int c) {
    if(c == '\b') {
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
    } else if(delta < 0) {
        scale /= 1.2;
    } else return;

    double rightf = x/scale - offsetRight;
    double upf = y/scale - offsetUp;

    offset = offset.Plus(projRight.ScaledBy(rightf - righti));
    offset = offset.Plus(projUp.ScaledBy(upf - upi));

    if(SS.TW.shown.screen == TextWindow::SCREEN_EDIT_VIEW) {
        if(havePainted) {
            SS.ScheduleShowTW();
        }
    }
    havePainted = false;
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
    SS.extraLine.draw = false;
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
    if(aam > 0.0) aa = aa.WithMagnitude(1);

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
        int64_t now = GetMilliseconds();
        if(now - lastSpaceNavigatorTime > 5000 ||
           lastSpaceNavigatorGroup.v != g->h.v)
        {
            SS.UndoRemember();
        }

        g->TransformImportedBy(t, q);

        lastSpaceNavigatorTime = now;
        lastSpaceNavigatorGroup = g->h;
        SS.MarkGroupDirty(g->h);
        SS.ScheduleGenerateAll();
    } else {
        // Apply the transformation to the view of the everything. The
        // x and y components are translation; but z component is scale,
        // not translation, or else it would do nothing in a parallel
        // projection
        offset = offset.Plus(projRight.ScaledBy(tx/scale));
        offset = offset.Plus(projUp.ScaledBy(ty/scale));
        scale *= exp(0.001*tz);

        if(aam > 0.0) {
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

