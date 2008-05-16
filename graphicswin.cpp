#include <stdarg.h>

#include "solvespace.h"

#define mView (&GraphicsWindow::MenuView)
#define mEdit (&GraphicsWindow::MenuEdit)
#define mReq  (&GraphicsWindow::MenuRequest)
#define mCon  (&Constraint::MenuConstrain)
#define mFile (&SolveSpace::MenuFile)
#define mGrp  (&Group::MenuGroup)
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
{ 1, "Onto Othe&r Side\tCtrl+O",            MNU_OTHER_SIDE,     'R'|C,  mView },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "Show Text &Window\tTab",              MNU_SHOW_TEXT_WND,  '\t',   mView },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "Dimensions in &Inches",               MNU_UNITS_INCHES,   0,      mView },
{ 1, "Dimensions in &Millimeters",          MNU_UNITS_MM,       0,      mView },

{ 0, "&Group",                              0,                  0,      NULL  },
{ 1, "New &Drawing in 3d\tShift+Ctrl+D",    MNU_GROUP_3D,      'D'|S|C, mGrp  },
{ 1, "New Drawing in Workplane\tShift+Ctrl+W",MNU_GROUP_WRKPL, 'W'|S|C, mGrp  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "New Step &Translating\tShift+Ctrl+R", MNU_GROUP_TRANS,    'T'|S|C,mGrp  },
{ 1, "New Step &Rotating\tShift+Ctrl+T",    MNU_GROUP_ROT,      'R'|S|C,mGrp  },
{ 1, NULL,                                  0,                  0,      NULL  },
{ 1, "New Extrusion\tShift+Ctrl+X",         MNU_GROUP_EXTRUDE,  'X'|S|C,mGrp  },
{ 1, NULL,                                  0,                  0,      NULL  },
{ 1, "New Boolean Difference",              0,                  0,      NULL  },
{ 1, "New Boolean Union",                   0,                  0,      NULL  },

{ 0, "&Request",                            0,                          NULL  },
{ 1, "Draw in &Workplane\tW",               MNU_SEL_WORKPLANE,  'W',    mReq  },
{ 1, "Draw Anywhere in 3d\tQ",              MNU_FREE_IN_3D,     'Q',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Datum &Point\tP",                     MNU_DATUM_POINT,    'P',    mReq  },
{ 1, "&Workplane (Coordinate S&ystem)\tY",  MNU_WORKPLANE,      'Y',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Line &Segment\tS",                    MNU_LINE_SEGMENT,   'S',    mReq  },
{ 1, "&Rectangle\tR",                       MNU_RECTANGLE,      'R',    mReq  },
{ 1, "&Circle\tC",                          MNU_CIRCLE,         'C',    mReq  },
{ 1, "&Arc of a Circle\tA",                 MNU_ARC,            'A',    mReq  },
{ 1, "&Cubic Segment\t3",                   MNU_CUBIC,          '3',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Sym&bolic Variable\tB",               0,                  'B',    mReq  },
{ 1, "&Import From File...\tI",             0,                  'I',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "To&ggle Construction\tG",             MNU_CONSTRUCTION,   'G',    mReq  },

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
{ 1, "Length Ra&tio\tShift+T",              MNU_RATIO,          'T'|S,  mCon  },
{ 1, "At &Midpoint\tShift+M",               MNU_AT_MIDPOINT,    'M'|S,  mCon  },
{ 1, "S&ymmetric\tShift+Y",                 MNU_SYMMETRIC,      'Y'|S,  mCon  },
{ 1, "Para&llel\tShift+L",                  MNU_PARALLEL,       'L'|S,  mCon  },
{ 1, "Same O&rientation\tShift+R",          MNU_ORIENTED_SAME,  'R'|S,  mCon  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Sym&bolic Equation\tShift+B",         0,                  'B'|S,  NULL  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Sol&ve Automatically\tShift+Tab",     MNU_SOLVE_AUTO,     '\t'|S, mCon  },
{ 1, "Solve Once Now\tSpace",               MNU_SOLVE_NOW,      ' ',    mCon  },

{ 0, "&Help",                               0,                          NULL  },
{ 1, "&About\t",                            0,                          NULL  },
{ -1  },
};

void GraphicsWindow::Init(void) {
    memset(this, 0, sizeof(*this));

    offset.x = offset.y = offset.z = 0;
    scale = 5;
    projRight.x = 1; projRight.y = projRight.z = 0;
    projUp.y = 1; projUp.z = projUp.x = 0;

    EnsureValidActives();

    // Start locked on to the XY plane.
    hRequest r = Request::HREQUEST_REFERENCE_XY;
    activeWorkplane = r.entity(0);

    showWorkplanes = true;
    showNormals = true;
    showPoints = true;
    showConstraints = true;
    showSolids = true;
    showHdnLines = false;
    showSolids = true;

    solving = SOLVE_ALWAYS;

    showTextWindow = true;
    ShowTextWindow(showTextWindow);
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

void GraphicsWindow::AnimateOnto(Quaternion quatf, Vector offsetf) {
    // Get our initial orientation and translation.
    Quaternion quat0 = Quaternion::MakeFrom(projRight, projUp);
    Vector offset0 = offset;

    // Make sure we take the shorter of the two possible paths.
    double mp = (quatf.Minus(quat0)).Magnitude();
    double mm = (quatf.Plus(quat0)).Magnitude();
    if(mp > mm) {
        quatf = quatf.ScaledBy(-1);
        mp = mm;
    }
    double mo = (offset0.Minus(offsetf)).Magnitude()*scale;

    // Animate transition, unless it's a tiny move.
    SDWORD dt = (mp < 0.01 && mo < 10) ? (-20) :
                    (SDWORD)(100 + 1000*mp + 0.4*mo);
    SDWORD tn, t0 = GetMilliseconds();
    double s = 0;
    Quaternion dq = quatf.Times(quat0.Inverse());
    do {
        offset = (offset0.ScaledBy(1 - s)).Plus(offsetf.ScaledBy(s));
        Quaternion quat = (dq.ToThe(s)).Times(quat0);
        quat = quat.WithMagnitude(1);

        projRight = quat.RotationU();
        projUp    = quat.RotationV();
        PaintGraphics();

        tn = GetMilliseconds();
        s = (tn - t0)/((double)dt);
    } while((tn - t0) < dt);

    projRight = quatf.RotationU();
    projUp = quatf.RotationV();
    offset = offsetf;
    InvalidateGraphics();
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

        case MNU_OTHER_SIDE: {
            Quaternion quatf = Quaternion::MakeFrom(
                SS.GW.projRight.ScaledBy(-1), SS.GW.projUp.ScaledBy(1));
            Vector ru = quatf.RotationU();
            Vector rv = quatf.RotationV();
            SS.GW.AnimateOnto(quatf, SS.GW.offset);
            break;
        }

        case MNU_SHOW_TEXT_WND:
            SS.GW.showTextWindow = !SS.GW.showTextWindow;
            SS.GW.EnsureValidActives();
            break;

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
    if(activeWorkplane.v != Entity::FREE_IN_3D.v && 
                       !SS.entity.FindByIdNoOops(activeWorkplane))
    {
        activeWorkplane = Entity::FREE_IN_3D;
        change = true;
    }

    bool in3d = (activeWorkplane.v == Entity::FREE_IN_3D.v);
    CheckMenuById(MNU_FREE_IN_3D, in3d);
    CheckMenuById(MNU_SEL_WORKPLANE, !in3d);

    // And update the checked state for various menus
    switch(viewUnits) {
        case UNIT_MM:
        case UNIT_INCHES:
            break;
        default:
            viewUnits = UNIT_MM;
    }
    CheckMenuById(MNU_UNITS_MM, viewUnits == UNIT_MM);
    CheckMenuById(MNU_UNITS_INCHES, viewUnits == UNIT_INCHES);

    ShowTextWindow(SS.GW.showTextWindow);
    CheckMenuById(MNU_SHOW_TEXT_WND, SS.GW.showTextWindow);

    CheckMenuById(MNU_SOLVE_AUTO, (SS.GW.solving == SOLVE_ALWAYS));
}

void GraphicsWindow::MenuEdit(int id) {
    switch(id) {
        case MNU_UNSELECT_ALL:
            HideGraphicsEditControl();
            SS.GW.ClearSelection();
            SS.GW.ClearPending();
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
                if(s->entity.v && s->entity.isFromRequest()) {
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

            SS.GenerateAll(SS.GW.solving == SOLVE_ALWAYS);
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
        case MNU_SEL_WORKPLANE: {
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 1 && SS.GW.gs.workplanes == 1) {
                SS.GW.activeWorkplane = SS.GW.gs.entity[0];
                SS.GW.ClearSelection();
            }

            if(SS.GW.activeWorkplane.v == Entity::FREE_IN_3D.v) {
                Error("Select workplane (e.g., the XY plane) "
                      "before locking on.");
                break;
            }
            // Align the view with the selected workplane
            Entity *e = SS.GetEntity(SS.GW.activeWorkplane);
            Quaternion quatf = e->Normal()->NormalGetNum();
            Vector offsetf = (e->WorkplaneGetOffset()).ScaledBy(-1);
            SS.GW.AnimateOnto(quatf, offsetf);
            SS.GW.EnsureValidActives();
            SS.TW.Show();
            break;
        }
        case MNU_FREE_IN_3D:
            SS.GW.activeWorkplane = Entity::FREE_IN_3D;
            SS.GW.EnsureValidActives();
            SS.TW.Show();
            break;
            
        case MNU_DATUM_POINT: s = "click to place datum point"; goto c;
        case MNU_LINE_SEGMENT: s = "click first point of line segment"; goto c;
        case MNU_CUBIC: s = "click first point of cubic segment"; goto c;
        case MNU_CIRCLE: s = "click center of circle"; goto c;
        case MNU_ARC: s = "click point on arc (draws anti-clockwise)"; goto c;
        case MNU_WORKPLANE: s = "click origin of workplane"; goto c;
        case MNU_RECTANGLE: s = "click one corner of rectangular"; goto c;
c:
            SS.GW.pending.operation = id;
            SS.GW.pending.description = s;
            SS.TW.Show();
            break;

        case MNU_CONSTRUCTION: {
            SS.GW.GroupSelection();
            int i;
            for(i = 0; i < SS.GW.gs.entities; i++) {
                hEntity he = SS.GW.gs.entity[i];
                if(!he.isFromRequest()) continue;
                Request *r = SS.GetRequest(he.request());
                r->construction = !(r->construction);
            }
            SS.GW.ClearSelection();
            SS.GenerateAll(SS.GW.solving == GraphicsWindow::SOLVE_ALWAYS);
            break;
        }

        default: oops();
    }
}

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

    Point2d mp = { x, y };

    // If the middle button is down, then mouse movement is used to pan and
    // rotate our view. This wins over everything else.
    if(middleDown) {
        hover.Clear();

        double dx = (x - orig.mouse.x) / scale;
        double dy = (y - orig.mouse.y) / scale;

        // When the view is locked, permit only translation (pan).
        if(!(shiftDown || ctrlDown)) {
            offset.x = orig.offset.x + dx*projRight.x + dy*projUp.x;
            offset.y = orig.offset.y + dx*projRight.y + dy*projUp.y;
            offset.z = orig.offset.z + dx*projRight.z + dy*projUp.z;
        } else if(ctrlDown) {
            double theta = atan2(orig.mouse.y, orig.mouse.x);
            theta -= atan2(y, x);

            Vector normal = orig.projRight.Cross(orig.projUp);
            projRight = orig.projRight.RotatedAbout(normal, theta);
            projUp = orig.projUp.RotatedAbout(normal, theta);

            NormalizeProjectionVectors();
        } else {
            double s = 0.3*(PI/180)*scale; // degrees per pixel
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
        } else {
            // Otherwise, just hit test and give up
            HitTestMakeSelection(mp);
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
                    u = u.RotatedAbout(orig.projUp, -s*dx);
                    u = u.RotatedAbout(orig.projRight, s*dy);
                    v = v.RotatedAbout(orig.projUp, -s*dx);
                    v = v.RotatedAbout(orig.projRight, s*dy);
                }
                q = Quaternion::MakeFrom(u, v);
                p->PointForceQuaternionTo(q);
                // Let's rotate about the selected point; so fix up the
                // translation so that that point didn't move.
                p->PointForceTo(p3);
                orig.mouse = mp;
            } else {
                UpdateDraggedPoint(pending.point, x, y);
                HitTestMakeSelection(mp);
            }
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
            break;
        }
        case DRAGGING_NEW_RADIUS:
        case DRAGGING_RADIUS: {
            Entity *circle = SS.GetEntity(pending.circle);
            Vector center = SS.GetEntity(circle->point[0])->PointGetNum();
            Point2d c2 = ProjectPoint(center);
            double r = c2.DistanceTo(mp)/scale;
            SS.GetEntity(circle->distance)->DistanceForceTo(r);
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

                Vector normal = orig.projRight.Cross(orig.projUp);
                u = u.RotatedAbout(normal, -theta);
                v = v.RotatedAbout(normal, -theta);
            } else {
                double dx = -(x - orig.mouse.x);
                double dy = -(y - orig.mouse.y);
                double s = 0.3*(PI/180); // degrees per pixel
                u = u.RotatedAbout(orig.projUp, -s*dx);
                u = u.RotatedAbout(orig.projRight, s*dy);
                v = v.RotatedAbout(orig.projUp, -s*dx);
                v = v.RotatedAbout(orig.projRight, s*dy);
            }
            orig.mouse = mp;
            normal->NormalForceTo(Quaternion::MakeFrom(u, v));
            break;
        }

        default: oops();
    }
    SS.GenerateAll(solving == SOLVE_ALWAYS);
    havePainted = false;
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

void GraphicsWindow::ClearPending(void) {
    memset(&pending, 0, sizeof(pending));
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
        if(e->h.request().v == pending.point.request().v) continue;

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

            // And some aux counts too
            switch(e->type) {
                case Entity::WORKPLANE:     (gs.workplanes)++; break;
                case Entity::LINE_SEGMENT:  (gs.lineSegments)++; break;

                case Entity::ARC_OF_CIRCLE:
                case Entity::CIRCLE:        (gs.circlesOrArcs)++; break;
            }
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
    r.workplane = activeWorkplane;
    r.type = type;
    SS.request.AddAndAssignId(&r);

    // We must regenerate the parameters, so that the code that tries to
    // place this request's entities where the mouse is can do so. But
    // we mustn't try to solve until reasonable values have been supplied
    // for these new parameters, or else we'll get a numerical blowup.
    SS.GenerateAll(false);

    return r.h;
}

bool GraphicsWindow::ConstrainPointByHovered(hEntity pt) {
    if(!hover.entity.v) return false;

    Entity *e = SS.GetEntity(hover.entity);
    if(e->IsPoint()) {
        Constraint::ConstrainCoincident(e->h, pt);
        return true;
    }
    if(e->IsWorkplane()) {
        Constraint::Constrain(Constraint::PT_IN_PLANE,
            pt, Entity::NO_ENTITY, e->h);
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

            ClearSelection(); hover.Clear();

            pending.operation = 0;
            break;

        case MNU_LINE_SEGMENT:
            hr = AddRequest(Request::LINE_SEGMENT);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(1));

            ClearSelection(); hover.Clear();

            pending.operation = DRAGGING_NEW_LINE_POINT;
            pending.point = hr.entity(2);
            pending.description = "click to place next point of line";
            SS.GetEntity(pending.point)->PointForceTo(v);
            break;

        case MNU_RECTANGLE: {
            hRequest lns[4];
            int i;
            for(i = 0; i < 4; i++) {
                lns[i] = AddRequest(Request::LINE_SEGMENT);
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
                Quaternion::MakeFrom(SS.GW.projRight, SS.GW.projUp));
            ConstrainPointByHovered(hr.entity(1));

            ClearSelection(); hover.Clear();

            ClearPending();
            pending.operation = DRAGGING_NEW_RADIUS;
            pending.circle = hr.entity(0);
            pending.description = "click to set radius";
            SS.GetParam(hr.param(0))->val = 0;
            break;

        case MNU_ARC:
            if(SS.GW.activeWorkplane.v == Entity::FREE_IN_3D.v) {
                Error("Can't draw arc in 3d; select a workplane first.");
                ClearPending();
                break;
            }
            hr = AddRequest(Request::ARC_OF_CIRCLE);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            SS.GetEntity(hr.entity(2))->PointForceTo(v);
            SS.GetEntity(hr.entity(3))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(2));

            ClearSelection(); hover.Clear();

            ClearPending();
            pending.operation = DRAGGING_NEW_ARC_POINT;
            pending.point = hr.entity(3);
            pending.description = "click to place point";
            break;

        case MNU_CUBIC:
            hr = AddRequest(Request::CUBIC);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            SS.GetEntity(hr.entity(2))->PointForceTo(v);
            SS.GetEntity(hr.entity(3))->PointForceTo(v);
            SS.GetEntity(hr.entity(4))->PointForceTo(v);
            ConstrainPointByHovered(hr.entity(1));

            ClearSelection(); hover.Clear();

            pending.operation = DRAGGING_NEW_CUBIC_POINT;
            pending.point = hr.entity(4);
            pending.description = "click to place next point of cubic";
            break;

        case MNU_WORKPLANE:
            hr = AddRequest(Request::WORKPLANE);
            SS.GetEntity(hr.entity(1))->PointForceTo(v);
            SS.GetEntity(hr.entity(32))->NormalForceTo( 
                Quaternion::MakeFrom(SS.GW.projRight, SS.GW.projUp));
            ConstrainPointByHovered(hr.entity(1));

            ClearSelection(); hover.Clear();
            ClearPending();
            break;

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

            // Constrain the line segments to share an endpoint
            Constraint::ConstrainCoincident(pending.point, hr.entity(1));

            // And drag an endpoint of the new line segment
            pending.operation = DRAGGING_NEW_LINE_POINT;
            pending.point = hr.entity(2);
            pending.description = "click to place next point of next line";
            SS.GetEntity(pending.point)->PointForceTo(v);

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

            for(i = 0; i < MAX_SELECTED; i++) {
                if(selection[i].IsEmpty()) {
                    selection[i] = hover;
                    break;
                }
            }
            break;
        }
    }
    SS.GenerateAll(SS.GW.solving == SOLVE_ALWAYS);

    SS.TW.Show();
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
        SS.GenerateAll(solving == SOLVE_ALWAYS);
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

    SS.GenerateAll(SS.GW.solving == SOLVE_ALWAYS);
    InvalidateGraphics();
    SS.TW.Show();
}

void GraphicsWindow::ToggleAnyDatumShown(int link, DWORD v) {
    bool t = !(SS.GW.showWorkplanes && SS.GW.showNormals && SS.GW.showPoints);
    SS.GW.showWorkplanes = t;
    SS.GW.showNormals = t;
    SS.GW.showPoints = t;

    SS.GenerateAll(SS.GW.solving == SOLVE_ALWAYS);
    InvalidateGraphics();
    SS.TW.Show();
}

Vector GraphicsWindow::VectorFromProjs(double right, double up, double fwd) {
    Vector n = projRight.Cross(projUp);
    Vector r = offset.ScaledBy(-1);
    r = r.Plus(projRight.ScaledBy(right));
    r = r.Plus(projUp.ScaledBy(up));
    r = r.Plus(n.ScaledBy(fwd));
    return r;
}

void GraphicsWindow::Paint(int w, int h) {
    havePainted = true;
    width = w; height = h;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();

    glScaled(scale*2.0/w, scale*2.0/h, scale*1.0/10000);

    double tx = projRight.Dot(offset);
    double ty = projUp.Dot(offset);
    Vector n = projUp.Cross(projRight);
    double tz = n.Dot(offset);
    double mat[16];
    MakeMatrix(mat, projRight.x,    projRight.y,    projRight.z,    tx,
                    projUp.x,       projUp.y,       projUp.z,       ty,
                    n.x,            n.y,            n.z,            tz,
                    0,              0,              0,              1);
    glMultMatrixd(mat);

    glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity();

    glShadeModel(GL_SMOOTH);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_DEPTH_TEST); 
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_NORMALIZE);

    glClearDepth(1.0); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

    Vector light = VectorFromProjs(-0.49*w/scale, 0.49*h/scale, 0);
    GLfloat lightPos[4] =
        { (GLfloat)light.x, (GLfloat)light.y, (GLfloat)light.z, 0 };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glEnable(GL_LIGHT0);

    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 1);
    GLfloat ambient[4] = { 0.4f, 0.4f, 0.4f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    glxUnlockColor();

    int i, a;
    // Draw the groups; this fills the polygons, if requested.
    if(showSolids) {
        for(i = 0; i < SS.group.n; i++) {
            SS.group.elem[i].Draw();
        }
    }

    // First, draw the entire scene. We don't necessarily want to draw
    // things with normal z-buffering behaviour; e.g. we always want to
    // draw a line segment in front of a reference. So we have three draw
    // levels, and only the first gets normal depth testing.
    for(a = 0; a <= 2; a++) {
        // Three levels: 0 least prominent (e.g. a reference workplane), 1 is
        // middle (e.g. line segment), 2 is always in front (e.g. point).
        if(a >= 1 && showHdnLines) glDisable(GL_DEPTH_TEST);
        for(i = 0; i < SS.entity.n; i++) {
            SS.entity.elem[i].Draw(a);
        }
    }

    glDisable(GL_DEPTH_TEST);
    // Draw the constraints
    for(i = 0; i < SS.constraint.n; i++) {
        SS.constraint.elem[i].Draw();
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

