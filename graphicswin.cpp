#include "solvespace.h"

#define mView (&GraphicsWindow::MenuView)
#define mEdit (&GraphicsWindow::MenuEdit)
#define mReq  (&GraphicsWindow::MenuRequest)
#define mCon  (&Constraint::MenuConstrain)
#define mFile (&SolveSpace::MenuFile)
#define mGrp  (&Group::MenuGroup)
#define mAna  (&SolveSpace::MenuAnalyze)
#define S 0x100
#define C 0x200
const GraphicsWindow::MenuEntry GraphicsWindow::menu[] = {
{ 0, "&File",                               0,                          NULL  },
{ 1, "&New\tCtrl+N",                        MNU_NEW,            'N'|C,  mFile },
{ 1, "&Open...\tCtrl+O",                    MNU_OPEN,           'O'|C,  mFile },
{10, "Open &Recent",                        MNU_OPEN_RECENT,    0,      mFile },
{ 1, "&Save\tCtrl+S",                       MNU_SAVE,           'S'|C,  mFile },
{ 1, "Save &As...",                         MNU_SAVE_AS,        0,      mFile },
{ 1,  NULL,                                 0,                  0,      NULL  },
{ 1, "Export &Image...",                    MNU_EXPORT_PNG,     0,      mFile },
{ 1, "Export &DXF...",                      MNU_EXPORT_DXF,     0,      mFile },
{ 1, "Export &Mesh...",                     MNU_EXPORT_MESH,    0,      mFile },
{ 1,  NULL,                                 0,                  0,      NULL  },
{ 1, "E&xit",                               MNU_EXIT,           0,      mFile },

{ 0, "&Edit",                               0,                          NULL  },
{ 1, "&Undo\tCtrl+Z",                       MNU_UNDO,           'Z'|C,  mEdit },
{ 1, "&Redo\tCtrl+Y",                       MNU_REDO,           'Y'|C,  mEdit },
{ 1, "Re&generate All\tSpace",              MNU_REGEN_ALL,      ' ',    mEdit },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "&Delete\tDel",                        MNU_DELETE,         127,    mEdit },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "&Unselect All\tEsc",                  MNU_UNSELECT_ALL,   27,     mEdit },

{ 0, "&View",                               0,                          NULL  },
{ 1, "Zoom &In\t+",                         MNU_ZOOM_IN,        '+',    mView },
{ 1, "Zoom &Out\t-",                        MNU_ZOOM_OUT,       '-',    mView },
{ 1, "Zoom To &Fit\tF",                     MNU_ZOOM_TO_FIT,    'F',    mView },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "Show Text &Window\tTab",              MNU_SHOW_TEXT_WND,  '\t',   mView },
{ 1,  NULL,                                 0,                          NULL  },
{ 1, "Dimensions in &Inches",               MNU_UNITS_INCHES,   0,      mView },
{ 1, "Dimensions in &Millimeters",          MNU_UNITS_MM,       0,      mView },

{ 0, "&New Group",                          0,                  0,      NULL  },
{ 1, "Sketch In &3d\tShift+3",              MNU_GROUP_3D,       '3'|S,  mGrp  },
{ 1, "Sketch In New &Workplane\tShift+W",   MNU_GROUP_WRKPL,    'W'|S,  mGrp  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Step &Translating\tShift+T",          MNU_GROUP_TRANS,    'T'|S,  mGrp  },
{ 1, "Step &Rotating\tShift+R",             MNU_GROUP_ROT,      'R'|S,  mGrp  },
{ 1, NULL,                                  0,                  0,      NULL  },
{ 1, "E&xtrude\tShift+X",                   MNU_GROUP_EXTRUDE,  'X'|S,  mGrp  },
{ 1, "&Lathe\tShift+L",                     MNU_GROUP_LATHE,    'L'|S,  mGrp  },
{ 1, "&Sweep\tShift+S",                     MNU_GROUP_SWEEP,    'S'|S,  mGrp  },
{ 1, "&Helical Sweep\tShift+H",             MNU_GROUP_HELICAL,  'H'|S,  mGrp  },
{ 1, NULL,                                  0,                  0,      NULL  },
{ 1, "Import / Assemble...\tShift+I",       MNU_GROUP_IMPORT,   'I'|S,  mGrp  },
{11, "Import Recent",                       MNU_GROUP_RECENT,   0,      mGrp  },

{ 0, "&Sketch",                             0,                          NULL  },
{ 1, "In &Workplane\tW",                    MNU_SEL_WORKPLANE,  'W',    mReq  },
{ 1, "Anywhere In &3d\t3",                  MNU_FREE_IN_3D,     '3',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Datum &Point\tP",                     MNU_DATUM_POINT,    'P',    mReq  },
{ 1, "&Workplane",                          MNU_WORKPLANE,      0,      mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Line &Segment\tS",                    MNU_LINE_SEGMENT,   'S',    mReq  },
{ 1, "&Rectangle\tR",                       MNU_RECTANGLE,      'R',    mReq  },
{ 1, "&Circle\tC",                          MNU_CIRCLE,         'C',    mReq  },
{ 1, "&Arc of a Circle\tA",                 MNU_ARC,            'A',    mReq  },
{ 1, "&Bezier Cubic Segment\tB",            MNU_CUBIC,          'B',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "&Text in TrueType Font\tT",           MNU_TTF_TEXT,       'T',    mReq  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "To&ggle Construction\tG",             MNU_CONSTRUCTION,   'G',    mReq  },

{ 0, "&Constrain",                          0,                          NULL  },
{ 1, "&Distance / Diameter\tD",             MNU_DISTANCE_DIA,   'D',    mCon  },
{ 1, "A&ngle\tN",                           MNU_ANGLE,          'N',    mCon  },
{ 1, "Other S&upplementary Angle\tU",       MNU_OTHER_ANGLE,    'U',    mCon  },
{ 1, "Toggle R&eference Dim\tE",            MNU_REFERENCE,      'E',    mCon  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "&Horizontal\tH",                      MNU_HORIZONTAL,     'H',    mCon  },
{ 1, "&Vertical\tV",                        MNU_VERTICAL,       'V',    mCon  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "&On Point / Curve / Plane\tO",        MNU_ON_ENTITY,      'O',    mCon  },
{ 1, "E&qual Length / Radius\tQ",           MNU_EQUAL,          'Q',    mCon  },
{ 1, "Length Ra&tio\tZ",                    MNU_RATIO,          'Z',    mCon  },
{ 1, "At &Midpoint\tM",                     MNU_AT_MIDPOINT,    'M',    mCon  },
{ 1, "S&ymmetric\tY",                       MNU_SYMMETRIC,      'Y',    mCon  },
{ 1, "Para&llel / Tangent\tL",              MNU_PARALLEL,       'L',    mCon  },
{ 1, "&Perpendicular\t[",                   MNU_PERPENDICULAR,  '[',    mCon  },
{ 1, "Same Orient&ation\tX",                MNU_ORIENTED_SAME,  'X',    mCon  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Comment\t;",                          MNU_COMMENT,        ';',    mCon  },

{ 0, "&Analyze",                            0,                          NULL  },
{ 1, "Measure &Volume",                     MNU_VOLUME,         0,      mAna  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "&Trace Point\tCtrl+Shift+T",          MNU_TRACE_PT,       'T'|S|C,mAna  },
{ 1, "&Stop Tracing...\tCtrl+Shift+S",      MNU_STOP_TRACING,   'S'|S|C,mAna  },
{ 1, "Step &Dimension...\tCtrl+Shift+D",    MNU_STEP_DIM,       'D'|S|C,mAna  },

{ 0, "&Help",                               0,                          NULL  },
{ 1, "&Load License...",                    0,                          NULL  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "&Website / Manual",                   0,                          NULL  },
{ 1, "&About",                              0,                          NULL  },
{ -1  },
};

void GraphicsWindow::Init(void) {
    memset(this, 0, sizeof(*this));

    scale = 5;
    offset    = Vector::From(0, 0, 0);
    projRight = Vector::From(1, 0, 0);
    projUp    = Vector::From(0, 1, 0);

    // And with the last group active
    activeGroup = SS.group.elem[SS.group.n-1].h;
    SS.GetGroup(activeGroup)->Activate();

    showWorkplanes = false;
    showNormals = true;
    showPoints = true;
    showConstraints = true;
    showHdnLines = false;
    showShaded = true;
    showMesh = false;

    showTextWindow = true;
    ShowTextWindow(showTextWindow);

    // Do this last, so that all the menus get updated correctly.
    EnsureValidActives();
}

void GraphicsWindow::NormalizeProjectionVectors(void) {
    Vector norm = projRight.Cross(projUp);
    projUp = norm.Cross(projRight);

    projUp = projUp.ScaledBy(1/projUp.Magnitude());
    projRight = projRight.ScaledBy(1/projRight.Magnitude());
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

    *w = 1 + r.z*SS.cameraTangent*scale;
    return r;
}

void GraphicsWindow::AnimateOntoWorkplane(void) {   
    if(!LockedInWorkplane()) return;

    Entity *w = SS.GetEntity(ActiveWorkplane());
    Quaternion quatf = w->Normal()->NormalGetNum();
    Vector offsetf = (SS.GetEntity(w->point[0])->PointGetNum()).ScaledBy(-1);

    // Get our initial orientation and translation.
    Quaternion quat0 = Quaternion::From(projRight, projUp);
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

void GraphicsWindow::HandlePointForZoomToFit(Vector p,
                        Point2d *pmax, Point2d *pmin, double *wmin, bool div)
{
    double w;
    Vector pp = ProjectPoint4(p, &w);
    if(div) {
        pp = pp.ScaledBy(1.0/w);
    }

    pmax->x = max(pmax->x, pp.x);
    pmax->y = max(pmax->y, pp.y);
    pmin->x = min(pmin->x, pp.x);
    pmin->y = min(pmin->y, pp.y);
    *wmin = min(*wmin, w);
}
void GraphicsWindow::LoopOverPoints(
                        Point2d *pmax, Point2d *pmin, double *wmin, bool div)
{
    HandlePointForZoomToFit(Vector::From(0, 0, 0), pmax, pmin, wmin, div);

    int i, j;
    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        if(!e->IsPoint()) continue;
        if(!e->IsVisible()) continue;
        HandlePointForZoomToFit(e->PointGetNum(), pmax, pmin, wmin, div);
    }
    Group *g = SS.GetGroup(activeGroup);
    for(i = 0; i < g->runningMesh.l.n; i++) {
        STriangle *tr = &(g->runningMesh.l.elem[i]);
        HandlePointForZoomToFit(tr->a, pmax, pmin, wmin, div);
        HandlePointForZoomToFit(tr->b, pmax, pmin, wmin, div);
        HandlePointForZoomToFit(tr->c, pmax, pmin, wmin, div);
    }
    for(i = 0; i < g->poly.l.n; i++) {
        SContour *sc = &(g->poly.l.elem[i]);
        for(j = 0; j < sc->l.n; j++) {
            HandlePointForZoomToFit(sc->l.elem[j].p, pmax, pmin, wmin, div);
        }
    }
}
void GraphicsWindow::ZoomToFit(void) {
    // On the first run, ignore perspective.
    Point2d pmax = { -1e12, -1e12 }, pmin = { 1e12, 1e12 };
    double wmin = 1;
    LoopOverPoints(&pmax, &pmin, &wmin, false);

    double xm = (pmax.x + pmin.x)/2, ym = (pmax.y + pmin.y)/2;
    double dx = pmax.x - pmin.x, dy = pmax.y - pmin.y;

    offset = offset.Plus(projRight.ScaledBy(-xm)).Plus(
                         projUp.   ScaledBy(-ym));
  
    // And based on this, we calculate the scale and offset
    if(dx == 0 && dy == 0) {
        scale = 5;
    } else {
        double scalex = 1e12, scaley = 1e12;
        if(dx != 0) scalex = 0.9*width /dx;
        if(dy != 0) scaley = 0.9*height/dy;
        scale = min(scalex, scaley);

        scale = min(100, scale);
        scale = max(0.001, scale);
    }

    // Then do another run, considering the perspective.
    pmax.x = -1e12; pmax.y = -1e12;
    pmin.x =  1e12; pmin.y =  1e12;
    wmin = 1;
    LoopOverPoints(&pmax, &pmin, &wmin, true);

    // Adjust the scale so that no points are behind the camera
    if(wmin < 0.1) {
        double k = SS.cameraTangent;
        // w = 1+k*scale*z
        double zmin = (wmin - 1)/(k*scale);
        // 0.1 = 1 + k*scale*zmin
        // (0.1 - 1)/(k*zmin) = scale
        scale = min(scale, (0.1 - 1)/(k*zmin));
    }
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
            SS.GW.ZoomToFit();
            break;

        case MNU_SHOW_TEXT_WND:
            SS.GW.showTextWindow = !SS.GW.showTextWindow;
            SS.GW.EnsureValidActives();
            break;

        case MNU_UNITS_MM:
            SS.viewUnits = SolveSpace::UNIT_MM;
            SS.later.showTW = true;
            SS.GW.EnsureValidActives();
            break;

        case MNU_UNITS_INCHES:
            SS.viewUnits = SolveSpace::UNIT_INCHES;
            SS.later.showTW = true;
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
        if(i >= SS.group.n) {
            // This can happen if the user deletes all the groups in the
            // sketch. It's difficult to prevent that, because the last
            // group might have been deleted automatically, because it failed
            // a dependency. There needs to be something, so create a plane
            // drawing group and activate that. They should never be able
            // to delete the references, though.
            activeGroup = SS.CreateDefaultDrawingGroup();
        } else {
            activeGroup = SS.group.elem[i].h;
        }
        SS.GetGroup(activeGroup)->Activate();
        change = true;
    }

    // The active coordinate system must also exist.
    if(LockedInWorkplane()) {
        Entity *e = SS.entity.FindByIdNoOops(ActiveWorkplane());
        if(e) {
            hGroup hgw = e->group;
            if(hgw.v != activeGroup.v && SS.GroupsInOrder(activeGroup, hgw)) {
                // The active workplane is in a group that comes after the
                // active group; so any request or constraint will fail.
                SetWorkplaneFreeIn3d();
                change = true;
            }
        } else {
            SetWorkplaneFreeIn3d();
            change = true;
        }
    }

    // And update the checked state for various menus
    bool locked = LockedInWorkplane();
    CheckMenuById(MNU_FREE_IN_3D, !locked);
    CheckMenuById(MNU_SEL_WORKPLANE, locked);

    SS.UndoEnableMenus();

    switch(SS.viewUnits) {
        case SolveSpace::UNIT_MM:
        case SolveSpace::UNIT_INCHES:
            break;
        default:
            SS.viewUnits = SolveSpace::UNIT_MM;
    }
    CheckMenuById(MNU_UNITS_MM, SS.viewUnits == SolveSpace::UNIT_MM);
    CheckMenuById(MNU_UNITS_INCHES, SS.viewUnits == SolveSpace::UNIT_INCHES);

    ShowTextWindow(SS.GW.showTextWindow);
    CheckMenuById(MNU_SHOW_TEXT_WND, SS.GW.showTextWindow);

    if(change) SS.later.showTW = true;
}

void GraphicsWindow::SetWorkplaneFreeIn3d(void) {
    SS.GetGroup(activeGroup)->activeWorkplane = Entity::FREE_IN_3D;
}
hEntity GraphicsWindow::ActiveWorkplane(void) {
    Group *g = SS.group.FindByIdNoOops(activeGroup);
    if(g) {
        return g->activeWorkplane;
    } else {
        return Entity::FREE_IN_3D;
    }
}
bool GraphicsWindow::LockedInWorkplane(void) {
    return (SS.GW.ActiveWorkplane().v != Entity::FREE_IN_3D.v);
}

void GraphicsWindow::MenuEdit(int id) {
    switch(id) {
        case MNU_UNSELECT_ALL:
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 0 && SS.GW.pending.operation == 0) {
                if(!TextEditControlIsVisible()) {
                    SS.TW.ClearSuper();
                }
            }
            SS.GW.ClearSuper();
            HideTextEditControl();
            break;

        case MNU_DELETE: {
            SS.UndoRemember();

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
                }
                if(s->constraint.v) {
                    SS.constraint.Tag(s->constraint, 1);
                }
            }
            SS.request.RemoveTagged();
            SS.constraint.RemoveTagged();

            // An edit might be in progress for the just-deleted item. So
            // now it's not.
            HideGraphicsEditControl();
            HideTextEditControl();
            // And clear out the selection, which could contain that item.
            SS.GW.ClearSuper();
            // And regenerate to get rid of what it generates, plus anything
            // that references it (since the regen code checks for that).
            SS.GenerateAll(0, INT_MAX);
            SS.GW.EnsureValidActives();
            SS.later.showTW = true;
            break;
        }

        case MNU_UNDO:
            SS.UndoUndo();
            break;
        
        case MNU_REDO:
            SS.UndoRedo();
            break;

        case MNU_REGEN_ALL:
            SS.ReloadAllImported();
            SS.GenerateAll(0, INT_MAX);
            SS.later.showTW = true;
            break;

        default: oops();
    }
}

void GraphicsWindow::MenuRequest(int id) {
    char *s;
    switch(id) {
        case MNU_SEL_WORKPLANE: {
            SS.GW.GroupSelection();
            Group *g = SS.GetGroup(SS.GW.activeGroup);

            if(SS.GW.gs.n == 1 && SS.GW.gs.workplanes == 1) {
                // A user-selected workplane
                g->activeWorkplane = SS.GW.gs.entity[0];
            } else if(g->type == Group::DRAWING_WORKPLANE) {
                // The group's default workplane
                g->activeWorkplane = g->h.entity(0);
            }

            if(!SS.GW.LockedInWorkplane()) {
                Error("Select workplane (e.g., the XY plane) "
                      "before locking on.");
                break;
            }
            // Align the view with the selected workplane
            SS.GW.AnimateOntoWorkplane();
            SS.GW.ClearSuper();
            SS.later.showTW = true;
            break;
        }
        case MNU_FREE_IN_3D:
            SS.GW.SetWorkplaneFreeIn3d();
            SS.GW.EnsureValidActives();
            SS.later.showTW = true;
            break;

        case MNU_ARC: {
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 1 && SS.GW.gs.points == 1) {
                if(!SS.GW.LockedInWorkplane()) {
                    Error("Must be sketching in workplane to create tangent "
                          "arc.");
                    break;
                }

                // Find two line segments that join at the specified point,
                // and blend them with a tangent arc. First, find the
                // requests that generate the line segments.
                Vector pshared = SS.GetEntity(SS.GW.gs.point[0])->PointGetNum();
                SS.GW.ClearSelection();

                int i, c = 0;
                Entity *line[2];
                Request *lineReq[2];
                bool point1[2];
                for(i = 0; i < SS.request.n; i++) {
                    Request *r = &(SS.request.elem[i]);
                    if(r->group.v != SS.GW.activeGroup.v) continue;
                    if(r->type != Request::LINE_SEGMENT) continue;
                    if(r->construction) continue;

                    Entity *e = SS.GetEntity(r->h.entity(0));
                    Vector p0 = SS.GetEntity(e->point[0])->PointGetNum(),
                           p1 = SS.GetEntity(e->point[1])->PointGetNum();
                    
                    if(p0.Equals(pshared) || p1.Equals(pshared)) {
                        if(c < 2) {
                            line[c] = e;
                            lineReq[c] = r;
                            point1[c] = (p1.Equals(pshared));
                        }
                        c++;
                    }
                }
                if(c != 2) {
                    Error("To create a tangent arc, select a point where "
                          "two non-construction line segments join.");
                    break;
                }

                SS.UndoRemember();

                Entity *wrkpl = SS.GetEntity(SS.GW.ActiveWorkplane());
                Vector wn = wrkpl->Normal()->NormalN();

                hEntity hshared = (line[0])->point[point1[0] ? 1 : 0],
                        hother0 = (line[0])->point[point1[0] ? 0 : 1],
                        hother1 = (line[1])->point[point1[1] ? 0 : 1];

                Vector pother0 = SS.GetEntity(hother0)->PointGetNum(),
                       pother1 = SS.GetEntity(hother1)->PointGetNum();

                Vector v0shared = pshared.Minus(pother0),
                       v1shared = pshared.Minus(pother1);

                hEntity srcline0 = (line[0])->h,
                        srcline1 = (line[1])->h;

                (lineReq[0])->construction = true;
                (lineReq[1])->construction = true;

                // And thereafter we mustn't touch the entity or req ptrs,
                // because the new requests/entities we add might force a
                // realloc.
                memset(line, 0, sizeof(line));
                memset(lineReq, 0, sizeof(lineReq));

                // The sign of vv determines whether shortest distance is
                // clockwise or anti-clockwise.
                Vector v = (wn.Cross(v0shared)).WithMagnitude(1);
                double vv = v1shared.Dot(v);

                double dot = (v0shared.WithMagnitude(1)).Dot(
                              v1shared.WithMagnitude(1));
                double theta = acos(dot);
                double r = 200/SS.GW.scale;
                // Set the radius so that no more than one third of the 
                // line segment disappears.
                r = min(r, v0shared.Magnitude()*tan(theta/2)/3);
                r = min(r, v1shared.Magnitude()*tan(theta/2)/3);
                double el = r/tan(theta/2);

                hRequest rln0 = SS.GW.AddRequest(Request::LINE_SEGMENT, false),
                         rln1 = SS.GW.AddRequest(Request::LINE_SEGMENT, false);
                hRequest rarc = SS.GW.AddRequest(Request::ARC_OF_CIRCLE, false);

                Entity *ln0 = SS.GetEntity(rln0.entity(0)),
                       *ln1 = SS.GetEntity(rln1.entity(0));
                Entity *arc = SS.GetEntity(rarc.entity(0));

                SS.GetEntity(ln0->point[0])->PointForceTo(pother0);
                Constraint::ConstrainCoincident(ln0->point[0], hother0);
                SS.GetEntity(ln1->point[0])->PointForceTo(pother1);
                Constraint::ConstrainCoincident(ln1->point[0], hother1);

                Vector arc0 = pshared.Minus(v0shared.WithMagnitude(el));
                Vector arc1 = pshared.Minus(v1shared.WithMagnitude(el));

                SS.GetEntity(ln0->point[1])->PointForceTo(arc0);
                SS.GetEntity(ln1->point[1])->PointForceTo(arc1);

                Constraint::Constrain(Constraint::PT_ON_LINE,
                    ln0->point[1], Entity::NO_ENTITY, srcline0);
                Constraint::Constrain(Constraint::PT_ON_LINE,
                    ln1->point[1], Entity::NO_ENTITY, srcline1);

                Vector center = arc0;
                int a, b;
                if(vv < 0) {
                    a = 1; b = 2;
                    center = center.Minus(v0shared.Cross(wn).WithMagnitude(r));
                } else {
                    a = 2; b = 1;
                    center = center.Plus(v0shared.Cross(wn).WithMagnitude(r));
                }

                SS.GetEntity(arc->point[0])->PointForceTo(center);
                SS.GetEntity(arc->point[a])->PointForceTo(arc0);
                SS.GetEntity(arc->point[b])->PointForceTo(arc1);

                Constraint::ConstrainCoincident(arc->point[a], ln0->point[1]);
                Constraint::ConstrainCoincident(arc->point[b], ln1->point[1]);

                Constraint::Constrain(Constraint::ARC_LINE_TANGENT,
                    Entity::NO_ENTITY, Entity::NO_ENTITY,
                    arc->h, ln0->h, (a==2));
                Constraint::Constrain(Constraint::ARC_LINE_TANGENT,
                    Entity::NO_ENTITY, Entity::NO_ENTITY,
                    arc->h, ln1->h, (b==2));

                SS.later.generateAll = true;
            } else {
                s = "click point on arc (draws anti-clockwise)";
                goto c;
            }
            break;
        }
        case MNU_DATUM_POINT: s = "click to place datum point"; goto c;
        case MNU_LINE_SEGMENT: s = "click first point of line segment"; goto c;
        case MNU_CUBIC: s = "click first point of cubic segment"; goto c;
        case MNU_CIRCLE: s = "click center of circle"; goto c;
        case MNU_WORKPLANE: s = "click origin of workplane"; goto c;
        case MNU_RECTANGLE: s = "click one corner of rectangle"; goto c;
        case MNU_TTF_TEXT: s = "click top left of text"; goto c;
c:
            SS.GW.pending.operation = id;
            SS.GW.pending.description = s;
            SS.later.showTW = true;
            break;

        case MNU_CONSTRUCTION: {
            SS.GW.GroupSelection();
            int i;
            for(i = 0; i < SS.GW.gs.entities; i++) {
                hEntity he = SS.GW.gs.entity[i];
                if(!he.isFromRequest()) continue;
                Request *r = SS.GetRequest(he.request());
                r->construction = !(r->construction);
                SS.MarkGroupDirty(r->group);
            }
            SS.GW.ClearSelection();
            SS.GenerateAll();
            break;
        }

        default: oops();
    }
}

void GraphicsWindow::ClearSuper(void) {
    HideGraphicsEditControl();
    ClearPending();
    ClearSelection();
    hover.Clear();
    EnsureValidActives();
}

void GraphicsWindow::ToggleBool(int link, DWORD v) {
    bool *vb = (bool *)v;
    *vb = !*vb;

    // The faces are shown as special stippling on the shaded triangle mesh,
    // so not meaningful to show them and hide the shaded.
    if(!SS.GW.showShaded) SS.GW.showFaces = false;

    SS.GenerateAll();
    InvalidateGraphics();
    SS.later.showTW = true;
}

