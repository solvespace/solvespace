#include <stdarg.h>

#include "solvespace.h"

#define mView (&GraphicsWindow::MenuView)
#define mEdit (&GraphicsWindow::MenuEdit)
#define mReq  (&GraphicsWindow::MenuRequest)
#define mCon  (&Constraint::MenuConstrain)
#define mFile (&SolveSpace::MenuFile)
#define S 0x100
#define C 0x200
const GraphicsWindow::MenuEntry GraphicsWindow::menu[] = {
{ 0, "&File",                               0,                          NULL  },
{ 1, "&New\tCtrl+N",                        MNU_NEW,            'N'|C,  mFile },
{ 1, "&Open...\tCtrl+O",                    MNU_OPEN,           'O'|C,  mFile },
{ 1, "&Save\tCtrl+S",                       MNU_SAVE,           'S'|C,  mFile },
{ 1, "Save &As...",                         MNU_SAVE_AS,        0,      mFile },
{ 1,  NULL,                                 0,                  0,      NULL  },
{ 1, "E&xit",                               MNU_EXIT,           0,      mFile },

{ 0, "&Edit",                               0,                          NULL  },
{ 1, "&Undo\tCtrl+Z",                       0,                          NULL  },
{ 1, "&Redo\tCtrl+Y",                       0,                          NULL  },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "&Delete\tDel",                        MNU_DELETE,         127,    mEdit },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "&Unselect All\tEsc",                  MNU_UNSELECT_ALL,   27,     mEdit },

{ 0, "&View",                               0,                          NULL  },
{ 1, "Zoom &In\t+",                         MNU_ZOOM_IN,        '+',    mView },
{ 1, "Zoom &Out\t-",                        MNU_ZOOM_OUT,       '-',    mView },
{ 1, "Zoom To &Fit\tF",                     MNU_ZOOM_TO_FIT,    'F',    mView },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "&Onto Plane / Coordinate System\tO",  MNU_ORIENT_ONTO,    'O',    mView },
{ 1, "&Lock Orientation\tL",                MNU_LOCK_VIEW,      'L',    mView },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "Dimensions in &Inches",               MNU_UNITS_INCHES,   0,      mView },
{ 1, "Dimensions in &Millimeters",          MNU_UNITS_MM,       0,      mView },

{ 0, "&Group",                              0,                  0,      NULL  },
{ 1, "New &Drawing Group",                  0,                  0,      NULL  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "New Step and Repeat &Translating",    0,                  0,      NULL  },
{ 1, "New Step and Repeat &Rotating",       0,                  0,      NULL  },
{ 1, NULL,                                  0,                  0,      NULL  },
{ 1, "New Extrusion",                       0,                  0,      NULL  },
{ 1, NULL,                                  0,                  0,      NULL  },
{ 1, "New Boolean Difference",              0,                  0,      NULL  },
{ 1, "New Boolean Union",                   0,                  0,      NULL  },

{ 0, "&Request",                            0,                          NULL  },
{ 1, "Dra&w in 2d Coordinate System\tW",    MNU_SEL_CSYS,       'W',    mReq  },
{ 1, "Draw Anywhere in 3d\tQ",              MNU_NO_CSYS,        'Q',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Datum &Point\tP",                     MNU_DATUM_POINT,    'P',    mReq  },
{ 1, "Datum A&xis\tX",                      0,                  'X',    mReq  },
{ 1, "Datum Pla&ne\tN",                     0,                  'N',    mReq  },
{ 1, "2d Coordinate S&ystem\tY",            0,                  'Y',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Line &Segment\tS",                    MNU_LINE_SEGMENT,   'S',    mReq  },
{ 1, "&Rectangle\tR",                       MNU_RECTANGLE,      'R',    mReq  },
{ 1, "&Circle\tC",                          0,                  'C',    mReq  },
{ 1, "&Arc of a Circle\tA",                 0,                  'A',    mReq  },
{ 1, "&Cubic Segment\t3",                   0,                  '3',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Sym&bolic Variable\tB",               0,                  'B',    mReq  },
{ 1, "&Import From File...\tI",             0,                  'I',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "To&ggle Construction\tG",             0,                  'G',    NULL  },

{ 0, "&Constrain",                          0,                          NULL  },
{ 1, "&Distance / Diameter\tShift+D",       MNU_DISTANCE_DIA,   'D'|S,  mCon  },
{ 1, "A&ngle\tShift+N",                     0,                  'N'|S,  NULL  },
{ 1, "Other S&upplementary Angle\tShift+U", 0,                  'U'|S,  NULL  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "&Horizontal\tShift+H",                MNU_HORIZONTAL,     'H'|S,  mCon  },
{ 1, "&Vertical\tShift+V",                  MNU_VERTICAL,       'V'|S,  mCon  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "&On Point / Curve / Plane\tShift+O",  MNU_ON_ENTITY,      'O'|S,  mCon  },
{ 1, "E&qual Length / Radius\tShift+Q",     MNU_EQUAL,          'Q'|S,  mCon  },
{ 1, "At &Midpoint\tShift+M",               0,                  'M'|S,  NULL  },
{ 1, "S&ymmetric\tShift+Y",                 0,                  'Y'|S,  NULL  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Sym&bolic Equation\tShift+B",         0,                  'B'|S,  NULL  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Solve Once Now\tSpace",               MNU_SOLVE_NOW,      ' ',    mCon  },

{ 0, "&Help",                               0,                          NULL  },
{ 1, "&About\t",                            0,                          NULL  },
{ -1  },
};

void GraphicsWindow::Init(void) {
    memset(this, 0, sizeof(*this));

    offset.x = offset.y = offset.z = 0;
    scale = 1;
    projRight.x = 1; projRight.y = projRight.z = 0;
    projUp.y = 1; projUp.z = projUp.x = 0;

    EnsureValidActives();

    show2dCsyss = true;
    showAxes = true;
    showPoints = true;
    showAllGroups = true;
    showConstraints = true;
}

void GraphicsWindow::NormalizeProjectionVectors(void) {
    Vector norm = projRight.Cross(projUp);
    projUp = norm.Cross(projRight);

    projUp = projUp.ScaledBy(1/projUp.Magnitude());
    projRight = projRight.ScaledBy(1/projRight.Magnitude());
}

Point2d GraphicsWindow::ProjectPoint(Vector p) {
    p = p.Plus(offset);

    Point2d r;
    r.x = p.Dot(projRight) * scale;
    r.y = p.Dot(projUp) * scale;
    return r;
}

void GraphicsWindow::MenuView(int id) {
    switch(id) {
        case MNU_ZOOM_IN:
            SS.GW.scale *= 1.2;
            break;

        case MNU_ZOOM_OUT:
            SS.GW.scale /= 1.2;
            break;

        case MNU_ZOOM_TO_FIT:
            break;

        case MNU_LOCK_VIEW:
            SS.GW.viewLocked = !SS.GW.viewLocked;
            SS.GW.EnsureValidActives();
            break;

        case MNU_ORIENT_ONTO: {
            SS.GW.GroupSelection();
            Entity *e = NULL;
            if(SS.GW.gs.n == 1 && SS.GW.gs.csyss == 1) {
                e = SS.GetEntity(SS.GW.gs.entity[0]);
            } else if(SS.GW.activeCsys.v != Entity::NO_CSYS.v) {
                e = SS.GetEntity(SS.GW.activeCsys);
            }
            if(e) {
                // A quaternion with our original rotation
                Quaternion quat0 = Quaternion::MakeFrom(
                    SS.GW.projRight, SS.GW.projUp);
                // And with our final rotation
                Vector pr, pu;
                e->Csys2dGetBasisVectors(&pr, &pu);
                Quaternion quatf = Quaternion::MakeFrom(pr, pu);
                // Make sure we take the shorter of the two possible paths.
                double mp = (quatf.Minus(quat0)).Magnitude();
                double mm = (quatf.Plus(quat0)).Magnitude();
                if(mp > mm) {
                    quatf = quatf.ScaledBy(-1);
                    mp = mm;
                }

                // And also get the offsets.
                Vector offset0 = SS.GW.offset;
                Vector offsetf = SS.GetEntity(e->assoc[0])->PointGetCoords();

                // Animate transition, unless it's a tiny move.
                SDWORD dt = (mp < 0.01) ? (-20) : (SDWORD)(100 + 1000*mp);
                SDWORD tn, t0 = GetMilliseconds();
                double s = 0;
                do {
                    SS.GW.offset =
                        (offset0.ScaledBy(1 - s)).Plus(offsetf.ScaledBy(s));
                    Quaternion quat =
                        (quat0.ScaledBy(1 - s)).Plus(quatf.ScaledBy(s));
                    quat = quat.WithMagnitude(1);
                    SS.GW.projRight = quat.RotationU();
                    SS.GW.projUp    = quat.RotationV();
                    PaintGraphics();

                    tn = GetMilliseconds();
                    s = (tn - t0)/((double)dt);
                } while((tn - t0) < dt);
                SS.GW.projRight = pr;
                SS.GW.projUp = pu;
                SS.GW.offset = offsetf;

                SS.GW.hover.Clear();
                SS.GW.ClearSelection();
                InvalidateGraphics();
            } else {
                Error("Select plane or coordinate system before orienting.");
            }
            break;
        }

        case MNU_UNITS_MM:
            SS.GW.viewUnits = UNIT_MM;
            SS.GW.EnsureValidActives();
            break;

        case MNU_UNITS_INCHES:
            SS.GW.viewUnits = UNIT_INCHES;
            SS.GW.EnsureValidActives();
            break;

        default: oops();
    }
    InvalidateGraphics();
}

void GraphicsWindow::EnsureValidActives(void) {
    bool change = false;
    // The active group must exist, and not be the references.
    Group *g = SS.group.FindByIdNoOops(activeGroup);
    if((!g) || (g->h.v == Group::HGROUP_REFERENCES.v)) {
        int i;
        for(i = 0; i < SS.group.n; i++) {
            if(SS.group.elem[i].h.v != Group::HGROUP_REFERENCES.v) {
                break;
            }
        }
        if(i >= SS.group.n) oops();
        activeGroup = SS.group.elem[i].h;
        change = true;
    }

    // The active coordinate system must also exist.
    if(activeCsys.v != Entity::NO_CSYS.v && 
                       !SS.entity.FindByIdNoOops(activeCsys))
    {
        activeCsys = Entity::NO_CSYS;
        change = true;
    }
    if(change) SS.TW.Show();

    bool in3d = (activeCsys.v == Entity::NO_CSYS.v);
    CheckMenuById(MNU_NO_CSYS, in3d);
    CheckMenuById(MNU_SEL_CSYS, !in3d);

    // And update the checked state for various menus
    CheckMenuById(MNU_LOCK_VIEW, viewLocked);
    switch(viewUnits) {
        case UNIT_MM:
        case UNIT_INCHES:
            break;
        default:
            viewUnits = UNIT_MM;
    }
    CheckMenuById(MNU_UNITS_MM, viewUnits == UNIT_MM);
    CheckMenuById(MNU_UNITS_INCHES, viewUnits == UNIT_INCHES);
}

void GraphicsWindow::MenuEdit(int id) {
    switch(id) {
        case MNU_UNSELECT_ALL:
            HideGraphicsEditControl();
            SS.GW.ClearSelection();
            SS.GW.pendingOperation = 0;
            SS.GW.pendingDescription = NULL;
            SS.GW.pendingPoint.v = 0;
            SS.GW.pendingConstraint.v = 0;
            SS.TW.ScreenNavigation('h', 0);
            SS.TW.Show();
            break;

        case MNU_DELETE: {
            int i;
            SS.request.ClearTags();
            SS.constraint.ClearTags();
            for(i = 0; i < MAX_SELECTED; i++) {
                Selection *s = &(SS.GW.selection[i]);
                hRequest r; r.v = 0;
                if(s->entity.v) {
                    r = s->entity.request();
                }
                if(r.v && !r.IsFromReferences()) {
                    SS.request.Tag(r, 1);
                    int j;
                    for(j = 0; j < SS.constraint.n; j++) {
                        Constraint *c = &(SS.constraint.elem[j]);
                        if(((c->ptA).request().v == r.v) ||
                           ((c->ptB).request().v == r.v) ||
                           ((c->ptC).request().v == r.v) ||
                           ((c->entityA).request().v == r.v) ||
                           ((c->entityB).request().v == r.v))
                        {
                            SS.constraint.Tag(c->h, 1);
                        }
                    }
                }
                if(s->constraint.v) {
                    SS.constraint.Tag(s->constraint, 1);
                }
            }
            SS.request.RemoveTagged();
            SS.constraint.RemoveTagged();

            SS.GenerateAll();
            SS.GW.ClearSelection();
            SS.GW.hover.Clear();
            break;
        }

        default: oops();
    }
}

void GraphicsWindow::MenuRequest(int id) {
    char *s;
    switch(id) {
        case MNU_SEL_CSYS:
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 1 && SS.GW.gs.csyss == 1) {
                SS.GW.activeCsys = SS.GW.gs.entity[0];
                SS.GW.ClearSelection();
            } else {
                Error("Select 2d coordinate system (e.g., the XY plane) "
                      "before locking on.");
            }
            SS.GW.EnsureValidActives();
            SS.TW.Show();
            break;

        case MNU_NO_CSYS:
            SS.GW.activeCsys = Entity::NO_CSYS;
            SS.GW.EnsureValidActives();
            SS.TW.Show();
            break;
            
        case MNU_DATUM_POINT: s = "click to place datum point"; goto c;
        case MNU_LINE_SEGMENT: s = "click first point of line segment"; goto c;
c:
            SS.GW.pendingOperation = id;
            SS.GW.pendingDescription = s;
            SS.TW.Show();
            break;

        default: oops();
    }
}

void GraphicsWindow::UpdateDraggedEntity(hEntity hp, double mx, double my) {
    Entity *p = SS.GetEntity(hp);
    Vector pos = p->PointGetCoords();
    UpdateDraggedPoint(&pos, mx, my);
    p->PointForceTo(pos);
}

void GraphicsWindow::UpdateDraggedPoint(Vector *pos, double mx, double my) {
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

    Point2d mp = { x, y };

    if(middleDown) {
        hover.Clear();

        double dx = (x - orig.mouse.x) / scale;
        double dy = (y - orig.mouse.y) / scale;

        // When the view is locked, permit only translation (pan).
        if(!(shiftDown || ctrlDown) || viewLocked) {
            offset.x = orig.offset.x + dx*projRight.x + dy*projUp.x;
            offset.y = orig.offset.y + dx*projRight.y + dy*projUp.y;
            offset.z = orig.offset.z + dx*projRight.z + dy*projUp.z;
        } else if(ctrlDown && !viewLocked) {
            double theta = atan2(orig.mouse.y, orig.mouse.x);
            theta -= atan2(y, x);

            Vector normal = orig.projRight.Cross(orig.projUp);
            projRight = orig.projRight.RotatedAbout(normal, theta);
            projUp = orig.projUp.RotatedAbout(normal, theta);

            NormalizeProjectionVectors();
        } else if(!viewLocked) {
            double s = 0.3*(PI/180); // degrees per pixel
            projRight = orig.projRight.RotatedAbout(orig.projUp, -s*dx);
            projUp = orig.projUp.RotatedAbout(orig.projRight, s*dy);

            NormalizeProjectionVectors();
        }

        orig.projRight = projRight;
        orig.projUp = projUp;
        orig.offset = offset;
        orig.mouse.x = x;
        orig.mouse.y = y;

        InvalidateGraphics();
    } else if(leftDown) {
        // We are left-dragging. This is often used to drag points, or
        // constraint labels.
        double dm = orig.mouse.DistanceTo(mp);
        // Don't start a drag until we've moved some threshold distance from
        // the mouse-down point, to avoid accidental drags.
        double dmt = 3;
        if(pendingOperation == 0) {
            if(hover.entity.v && 
               SS.GetEntity(hover.entity)->IsPoint() && 
               !SS.GetEntity(hover.entity)->PointIsFromReferences())
            {
                if(dm > dmt) {
                    // Start dragging this point.
                    ClearSelection();
                    pendingPoint = hover.entity;
                    pendingOperation = DRAGGING_POINT;
                }
            } else if(hover.constraint.v && 
                            SS.GetConstraint(hover.constraint)->HasLabel())
            {
                if(dm > dmt) {
                    ClearSelection();
                    pendingConstraint = hover.constraint;
                    pendingOperation = DRAGGING_CONSTRAINT;
                }
            }
        } else if(pendingOperation == DRAGGING_POINT ||
                  pendingOperation == DRAGGING_NEW_POINT ||
                  pendingOperation == DRAGGING_NEW_LINE_POINT)
        {
            UpdateDraggedEntity(pendingPoint, x, y);
        } else if(pendingOperation == DRAGGING_CONSTRAINT) {
            Constraint *c = SS.constraint.FindById(pendingConstraint);
            UpdateDraggedPoint(&(c->disp.offset), x, y);
        }
    } else {
        // No buttons pressed.
        if(pendingOperation == DRAGGING_NEW_POINT ||
           pendingOperation == DRAGGING_NEW_LINE_POINT)
        {
            UpdateDraggedEntity(pendingPoint, x, y);
            HitTestMakeSelection(mp);
        } else {
            // Do our usual hit testing, for the selection.
            HitTestMakeSelection(mp);
        }
    }
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
}
void GraphicsWindow::Selection::Draw(void) {
    if(entity.v)     SS.GetEntity    (entity    )->Draw(-1);
    if(constraint.v) SS.GetConstraint(constraint)->Draw();
}

void GraphicsWindow::HitTestMakeSelection(Point2d mp) {
    int i;
    double d, dmin = 1e12;
    Selection s;
    memset(&s, 0, sizeof(s));

    // Do the entities
    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        // Don't hover whatever's being dragged.
        if(e->h.request().v == pendingPoint.request().v) continue;

        d = e->GetDistance(mp);
        if(d < 10 && d < dmin) {
            memset(&s, 0, sizeof(s));
            s.entity = e->h;
            dmin = d;
        }
    }

    // Constraints
    for(i = 0; i < SS.constraint.n; i++) {
        d = SS.constraint.elem[i].GetDistance(mp);
        if(d < 10 && d < dmin) {
            memset(&s, 0, sizeof(s));
            s.constraint = SS.constraint.elem[i].h;
            dmin = d;
        }
    }

    if(!s.Equals(&hover)) {
        hover = s;
        InvalidateGraphics();
    }
}

void GraphicsWindow::ClearSelection(void) {
    for(int i = 0; i < MAX_SELECTED; i++) {
        selection[i].Clear();
    }
    InvalidateGraphics();
}

void GraphicsWindow::GroupSelection(void) {
    memset(&gs, 0, sizeof(gs));
    int i;
    for(i = 0; i < MAX_SELECTED; i++) {
        Selection *s = &(selection[i]);
        if(s->entity.v) {
            (gs.n)++;

            Entity *e = SS.entity.FindById(s->entity);
            if(e->IsPoint()) {
                gs.point[(gs.points)++] = s->entity;
            } else {
                gs.entity[(gs.entities)++] = s->entity;
            }
            switch(e->type) {
                case Entity::CSYS_2D:       (gs.csyss)++; break;
                case Entity::LINE_SEGMENT:  (gs.lineSegments)++; break;
            }
            if(e->HasPlane()) (gs.planes)++;
        }
    }
}

void GraphicsWindow::MouseMiddleDown(double x, double y) {
    if(GraphicsEditControlIsVisible()) return;

    orig.offset = offset;
    orig.projUp = projUp;
    orig.projRight = projRight;
    orig.mouse.x = x;
    orig.mouse.y = y;
}

hRequest GraphicsWindow::AddRequest(int type) {
    Request r;
    memset(&r, 0, sizeof(r));
    r.group = activeGroup;
    r.csys = activeCsys;
    r.type = type;
    SS.request.AddAndAssignId(&r);
    SS.GenerateAll();
    return r.h;
}

void GraphicsWindow::MouseLeftDown(double mx, double my) {
    if(GraphicsEditControlIsVisible()) return;

    // Make sure the hover is up to date.
    MouseMoved(mx, my, false, false, false, false, false);
    orig.mouse.x = mx;
    orig.mouse.y = my;

    // The current mouse location
    Vector v = offset.ScaledBy(-1);
    v = v.Plus(projRight.ScaledBy(mx/scale));
    v = v.Plus(projUp.ScaledBy(my/scale));

    hRequest hr;
    switch(pendingOperation) {
        case MNU_DATUM_POINT:
            hr = AddRequest(Request::DATUM_POINT);
            SS.GetEntity(hr.entity(0))->PointForceTo(v);

            ClearSelection(); hover.Clear();

            pendingOperation = 0;
            break;

        case MNU_LINE_SEGMENT:
            hr = AddRequest(Request::LINE_SEGMENT);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            if(hover.entity.v && SS.GetEntity(hover.entity)->IsPoint()) {
                Constraint::ConstrainCoincident(hover.entity, hr.entity(1));
            }

            ClearSelection(); hover.Clear();

            pendingOperation = DRAGGING_NEW_LINE_POINT;
            pendingPoint = hr.entity(2);
            pendingDescription = "click to place next point of line";
            SS.GetEntity(pendingPoint)->PointForceTo(v);
            break;

        case DRAGGING_NEW_POINT:
            // The MouseMoved event has already dragged it under the cursor.
            pendingOperation = 0;
            pendingPoint.v = 0;
            break;

        case DRAGGING_NEW_LINE_POINT: {
            if(hover.entity.v && SS.GetEntity(hover.entity)->IsPoint()) {
                Constraint::ConstrainCoincident(pendingPoint, hover.entity);
                pendingOperation = 0;
                pendingPoint.v = 0;
                break;
            }
            // Create a new line segment, so that we continue drawing.
            hRequest hr = AddRequest(Request::LINE_SEGMENT);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);

            // Constrain the line segments to share an endpoint
            Constraint::ConstrainCoincident(pendingPoint, hr.entity(1));

            // And drag an endpoint of the new line segment
            pendingOperation = DRAGGING_NEW_LINE_POINT;
            pendingPoint = hr.entity(2);
            pendingDescription = "click to place next point of next line";
            SS.GetEntity(pendingPoint)->PointForceTo(v);

            break;
        }

        case 0:
        default: {
            pendingOperation = 0;

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

            for(i = 0; i < MAX_SELECTED; i++) {
                if(selection[i].IsEmpty()) {
                    selection[i] = hover;
                    break;
                }
            }
            break;
        }
    }

    SS.TW.Show();
    InvalidateGraphics();
}

void GraphicsWindow::MouseLeftUp(double mx, double my) {
    switch(pendingOperation) {
        case DRAGGING_POINT:
        case DRAGGING_CONSTRAINT:
            pendingOperation = 0;
            pendingPoint.v = 0;
            pendingConstraint.v = 0;
            break;

        default:
            break;  // do nothing
    }
}

void GraphicsWindow::MouseLeftDoubleClick(double mx, double my) {
    if(GraphicsEditControlIsVisible()) return;

    if(hover.constraint.v) {
        ClearSelection();
        Constraint *c = SS.GetConstraint(hover.constraint);
        Vector p3 = c->GetLabelPos();
        Point2d p2 = ProjectPoint(p3);
        ShowGraphicsEditControl((int)p2.x, (int)p2.y, c->exprA->Print());
        constraintBeingEdited = hover.constraint;
    }
}

void GraphicsWindow::EditControlDone(char *s) {
    Expr *e = Expr::FromString(s);
    if(e) {
        Constraint *c = SS.GetConstraint(constraintBeingEdited);
        Expr::FreeKeep(&(c->exprA));
        c->exprA = e->DeepCopyKeep();
        HideGraphicsEditControl();
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

void GraphicsWindow::ToggleBool(int link, DWORD v) {
    bool *vb = (bool *)v;
    *vb = !*vb;

    SS.GenerateAll();
    InvalidateGraphics();
    SS.TW.Show();
}

void GraphicsWindow::ToggleAnyDatumShown(int link, DWORD v) {
    bool t = !(SS.GW.show2dCsyss && SS.GW.showAxes && SS.GW.showPoints);
    SS.GW.show2dCsyss = t;
    SS.GW.showAxes = t;
    SS.GW.showPoints = t;

    SS.GenerateAll();
    InvalidateGraphics();
    SS.TW.Show();
}

void GraphicsWindow::Paint(int w, int h) {
    width = w; height = h;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity();

    glScaled(scale*2.0/w, scale*2.0/h, 0);

    double tx = projRight.Dot(offset);
    double ty = projUp.Dot(offset);
    double mat[16];
    MakeMatrix(mat, projRight.x,    projRight.y,    projRight.z,    tx,
                    projUp.x,       projUp.y,       projUp.z,       ty,
                    0,              0,              0,              0,
                    0,              0,              0,              1);
    glMultMatrixd(mat);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_DEPTH_TEST); 
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glClearIndex((GLfloat)0);
    glClearDepth(1.0); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 


    int i, a;

    // First, draw the entire scene. We don't necessarily want to draw
    // things with normal z-buffering behaviour; e.g. we always want to
    // draw a line segment in front of a reference. So we have three draw
    // levels, and only the first gets normal depth testing.
    glxUnlockColor();
    for(a = 0; a <= 2; a++) {
        // Three levels: 0 least prominent (e.g. a reference csys), 1 is
        // middle (e.g. line segment), 2 is always in front (e.g. point).
        if(a == 1) glDisable(GL_DEPTH_TEST);
        for(i = 0; i < SS.entity.n; i++) {
            SS.entity.elem[i].Draw(a);
        }
    }

    // Draw the constraints
    for(i = 0; i < SS.constraint.n; i++) {
        SS.constraint.elem[i].Draw();
    }

    // Draw the groups; this fills the polygons, if requested.
    for(i = 0; i < SS.group.n; i++) {
        SS.group.elem[i].Draw();
    }

    // Then redraw whatever the mouse is hovering over, highlighted.
    glDisable(GL_DEPTH_TEST); 
    glxLockColorTo(1, 1, 0);
    hover.Draw();

    // And finally draw the selection, same mechanism.
    glxLockColorTo(1, 0, 0);
    for(i = 0; i < MAX_SELECTED; i++) {
        selection[i].Draw();
    }
}

