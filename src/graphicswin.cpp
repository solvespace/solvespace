//-----------------------------------------------------------------------------
// Top-level implementation of the program's main window, in which a graphical
// representation of the model is drawn and edited by the user.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#define mView (&GraphicsWindow::MenuView)
#define mEdit (&GraphicsWindow::MenuEdit)
#define mClip (&GraphicsWindow::MenuClipboard)
#define mReq  (&GraphicsWindow::MenuRequest)
#define mCon  (&Constraint::MenuConstrain)
#define mFile (&SolveSpaceUI::MenuFile)
#define mGrp  (&Group::MenuGroup)
#define mAna  (&SolveSpaceUI::MenuAnalyze)
#define mHelp (&SolveSpaceUI::MenuHelp)
#define DEL   DELETE_KEY
#define ESC   ESCAPE_KEY
#define S     SHIFT_MASK
#define C     CTRL_MASK
#define F(k)  (FUNCTION_KEY_BASE+(k))
#define IN    MENU_ITEM_NORMAL
#define IC    MENU_ITEM_CHECK
#define IR    MENU_ITEM_RADIO
const GraphicsWindow::MenuEntry GraphicsWindow::menu[] = {
//level
//   label                          id                  accel    ty   fn
{ 0, "&File",                       0,                  0,       IN, NULL  },
{ 1, "&New",                        MNU_NEW,            C|'N',   IN, mFile },
{ 1, "&Open...",                    MNU_OPEN,           C|'O',   IN, mFile },
{ 1, "Open &Recent",                MNU_OPEN_RECENT,    0,       IN, mFile },
{ 1, "&Save",                       MNU_SAVE,           C|'S',   IN, mFile },
{ 1, "Save &As...",                 MNU_SAVE_AS,        0,       IN, mFile },
{ 1,  NULL,                         0,                  0,       IN, NULL  },
{ 1, "Export &Image...",            MNU_EXPORT_PNG,     0,       IN, mFile },
{ 1, "Export 2d &View...",          MNU_EXPORT_VIEW,    0,       IN, mFile },
{ 1, "Export 2d &Section...",       MNU_EXPORT_SECTION, 0,       IN, mFile },
{ 1, "Export 3d &Wireframe...",     MNU_EXPORT_WIREFRAME, 0,     IN, mFile },
{ 1, "Export Triangle &Mesh...",    MNU_EXPORT_MESH,    0,       IN, mFile },
{ 1, "Export &Surfaces...",         MNU_EXPORT_SURFACES,0,       IN, mFile },
#ifndef __APPLE__
{ 1,  NULL,                         0,                  0,       IN, NULL  },
{ 1, "E&xit",                       MNU_EXIT,           C|'Q',   IN, mFile },
#endif

{ 0, "&Edit",                       0,                  0,       IN, NULL  },
{ 1, "&Undo",                       MNU_UNDO,           C|'Z',   IN, mEdit },
{ 1, "&Redo",                       MNU_REDO,           C|'Y',   IN, mEdit },
{ 1, "Re&generate All",             MNU_REGEN_ALL,      ' ',     IN, mEdit },
{ 1,  NULL,                         0,                  0,       IN, NULL  },
{ 1, "Snap Selection to &Grid",     MNU_SNAP_TO_GRID,   '.',     IN, mEdit },
{ 1, "Rotate Imported &90°",        MNU_ROTATE_90,      '9',     IN, mEdit },
{ 1,  NULL,                         0,                  0,       IN, NULL  },
{ 1, "Cu&t",                        MNU_CUT,            C|'X',   IN, mClip },
{ 1, "&Copy",                       MNU_COPY,           C|'C',   IN, mClip },
{ 1, "&Paste",                      MNU_PASTE,          C|'V',   IN, mClip },
{ 1, "Paste &Transformed...",       MNU_PASTE_TRANSFORM,C|'T',   IN, mClip },
{ 1, "&Delete",                     MNU_DELETE,         DEL,     IN, mClip },
{ 1,  NULL,                         0,                  0,       IN, NULL  },
{ 1, "Select &Edge Chain",          MNU_SELECT_CHAIN,   C|'E',   IN, mEdit },
{ 1, "Select &All",                 MNU_SELECT_ALL,     C|'A',   IN, mEdit },
{ 1, "&Unselect All",               MNU_UNSELECT_ALL,   ESC,     IN, mEdit },

{ 0, "&View",                       0,                  0,       IN, NULL  },
{ 1, "Zoom &In",                    MNU_ZOOM_IN,        '+',     IN, mView },
{ 1, "Zoom &Out",                   MNU_ZOOM_OUT,       '-',     IN, mView },
{ 1, "Zoom To &Fit",                MNU_ZOOM_TO_FIT,    'F',     IN, mView },
{ 1,  NULL,                         0,                  0,       IN, NULL  },
{ 1, "Align View to &Workplane",    MNU_ONTO_WORKPLANE, 'W',     IN, mView },
{ 1, "Nearest &Ortho View",         MNU_NEAREST_ORTHO,  F(2),    IN, mView },
{ 1, "Nearest &Isometric View",     MNU_NEAREST_ISO,    F(3),    IN, mView },
{ 1, "&Center View At Point",       MNU_CENTER_VIEW,    F(4),    IN, mView },
{ 1,  NULL,                         0,                  0,       IN, NULL  },
{ 1, "Show Snap &Grid",             MNU_SHOW_GRID,      '>',     IC, mView },
{ 1, "Use &Perspective Projection", MNU_PERSPECTIVE_PROJ,'`',    IC, mView },
{ 1,  NULL,                         0,                  0,       IN, NULL  },
#if defined(__APPLE__)
{ 1, "Show Menu &Bar",              MNU_SHOW_MENU_BAR,  C|F(12), IC, mView },
#endif
{ 1, "Show &Toolbar",               MNU_SHOW_TOOLBAR,   0,       IC, mView },
{ 1, "Show Text &Window",           MNU_SHOW_TEXT_WND,  '\t',    IC, mView },
{ 1,  NULL,                         0,                  0,       IN, NULL  },
{ 1, "Dimensions in &Inches",       MNU_UNITS_INCHES,   0,       IR, mView },
{ 1, "Dimensions in &Millimeters",  MNU_UNITS_MM,       0,       IR, mView },
#if defined(HAVE_GTK) || defined(__APPLE__)
{ 1,  NULL,                         0,                  0,       IN, NULL  },
{ 1, "&Full Screen",                MNU_FULL_SCREEN,    C|F(11), IC, mView },
#endif

{ 0, "&New Group",                  0,                  0,       IN, NULL  },
{ 1, "Sketch In &3d",               MNU_GROUP_3D,       S|'3',   IN, mGrp  },
{ 1, "Sketch In New &Workplane",    MNU_GROUP_WRKPL,    S|'W',   IN, mGrp  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "Step &Translating",           MNU_GROUP_TRANS,    S|'T',   IN, mGrp  },
{ 1, "Step &Rotating",              MNU_GROUP_ROT,      S|'R',   IN, mGrp  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "E&xtrude",                    MNU_GROUP_EXTRUDE,  S|'X',   IN, mGrp  },
{ 1, "&Lathe",                      MNU_GROUP_LATHE,    S|'L',   IN, mGrp  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "Import / Assemble...",        MNU_GROUP_IMPORT,   S|'I',   IN, mGrp  },
{ 1, "Import Recent",               MNU_GROUP_RECENT,   0,       IN, mGrp  },

{ 0, "&Sketch",                     0,                  0,       IN, NULL  },
{ 1, "In &Workplane",               MNU_SEL_WORKPLANE,  '2',     IR, mReq  },
{ 1, "Anywhere In &3d",             MNU_FREE_IN_3D,     '3',     IR, mReq  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "Datum &Point",                MNU_DATUM_POINT,    'P',     IN, mReq  },
{ 1, "&Workplane",                  MNU_WORKPLANE,      0,       IN, mReq  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "Line &Segment",               MNU_LINE_SEGMENT,   'S',     IN, mReq  },
{ 1, "C&onstruction Line Segment",  MNU_CONSTR_SEGMENT, S|'S',   IN, mReq  },
{ 1, "&Rectangle",                  MNU_RECTANGLE,      'R',     IN, mReq  },
{ 1, "&Circle",                     MNU_CIRCLE,         'C',     IN, mReq  },
{ 1, "&Arc of a Circle",            MNU_ARC,            'A',     IN, mReq  },
{ 1, "&Bezier Cubic Spline",        MNU_CUBIC,          'B',     IN, mReq  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "&Text in TrueType Font",      MNU_TTF_TEXT,       'T',     IN, mReq  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "To&ggle Construction",        MNU_CONSTRUCTION,   'G',     IN, mReq  },
{ 1, "Tangent &Arc at Point",       MNU_TANGENT_ARC,    S|'A',   IN, mReq  },
{ 1, "Split Curves at &Intersection", MNU_SPLIT_CURVES, 'I',     IN, mReq  },

{ 0, "&Constrain",                  0,                  0,       IN, NULL  },
{ 1, "&Distance / Diameter",        MNU_DISTANCE_DIA,   'D',     IN, mCon  },
{ 1, "Re&ference Dimension",        MNU_REF_DISTANCE,   S|'D',   IN, mCon  },
{ 1, "A&ngle",                      MNU_ANGLE,          'N',     IN, mCon  },
{ 1, "Reference An&gle",            MNU_REF_ANGLE,      S|'N',   IN, mCon  },
{ 1, "Other S&upplementary Angle",  MNU_OTHER_ANGLE,    'U',     IN, mCon  },
{ 1, "Toggle R&eference Dim",       MNU_REFERENCE,      'E',     IN, mCon  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "&Horizontal",                 MNU_HORIZONTAL,     'H',     IN, mCon  },
{ 1, "&Vertical",                   MNU_VERTICAL,       'V',     IN, mCon  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "&On Point / Curve / Plane",   MNU_ON_ENTITY,      'O',     IN, mCon  },
{ 1, "E&qual Length / Radius / Angle", MNU_EQUAL,       'Q',     IN, mCon  },
{ 1, "Length Ra&tio",               MNU_RATIO,          'Z',     IN, mCon  },
{ 1, "Length Diff&erence",          MNU_DIFFERENCE,     'J',     IN, mCon  },
{ 1, "At &Midpoint",                MNU_AT_MIDPOINT,    'M',     IN, mCon  },
{ 1, "S&ymmetric",                  MNU_SYMMETRIC,      'Y',     IN, mCon  },
{ 1, "Para&llel / Tangent",         MNU_PARALLEL,       'L',     IN, mCon  },
{ 1, "&Perpendicular",              MNU_PERPENDICULAR,  '[',     IN, mCon  },
{ 1, "Same Orient&ation",           MNU_ORIENTED_SAME,  'X',     IN, mCon  },
{ 1, "Lock Point Where &Dragged",   MNU_WHERE_DRAGGED,  ']',     IN, mCon  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "Comment",                     MNU_COMMENT,        ';',     IN, mCon  },

{ 0, "&Analyze",                    0,                  0,       IN, NULL  },
{ 1, "Measure &Volume",             MNU_VOLUME,         C|S|'V', IN, mAna  },
{ 1, "Measure &Area",               MNU_AREA,           C|S|'A', IN, mAna  },
{ 1, "Show &Interfering Parts",     MNU_INTERFERENCE,   C|S|'I', IN, mAna  },
{ 1, "Show &Naked Edges",           MNU_NAKED_EDGES,    C|S|'N', IN, mAna  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "Show Degrees of &Freedom",    MNU_SHOW_DOF,       C|S|'F', IN, mAna  },
{ 1, NULL,                          0,                  0,       IN, NULL  },
{ 1, "&Trace Point",                MNU_TRACE_PT,       C|S|'T', IN, mAna  },
{ 1, "&Stop Tracing...",            MNU_STOP_TRACING,   C|S|'S', IN, mAna  },
{ 1, "Step &Dimension...",          MNU_STEP_DIM,       C|S|'D', IN, mAna  },

{ 0, "&Help",                       0,                  0,       IN, NULL  },
{ 1, "&Website / Manual",           MNU_WEBSITE,        0,       IN, mHelp },
#ifndef __APPLE__
{ 1, "&About",                      MNU_ABOUT,          0,       IN, mHelp },
#endif

{ -1, 0, 0, 0, IN, 0 }
};

#undef DEL
#undef ESC
#undef S
#undef C
#undef F
#undef IN
#undef IC
#undef IR

bool SolveSpace::MakeAcceleratorLabel(int accel, char *out) {
    if(!accel) {
        out[0] = '\0';
        return false;
    }

    const char *ctrl  = accel & GraphicsWindow::CTRL_MASK  ? "Ctrl+"  : "";
    const char *shift = accel & GraphicsWindow::SHIFT_MASK ? "Shift+" : "";

    char buf[8];
    buf[0] = (char)(accel & 0xff);
    buf[1] = '\0';

    if(accel >= GraphicsWindow::FUNCTION_KEY_BASE + 1 &&
       accel <= GraphicsWindow::FUNCTION_KEY_BASE + 12) {
        sprintf(buf, "F%d", accel - GraphicsWindow::FUNCTION_KEY_BASE);
    }

    const char *key = buf;

    switch(accel) {
        case '\t': key = "Tab";   break;
        case  ' ': key = "Space"; break;

        case GraphicsWindow::ESCAPE_KEY: key = "Esc"; break;
        case GraphicsWindow::DELETE_KEY: key = "Del"; break;
    }

    if(key[0] <  '!' || key[0] >  '~') oops();
    if(key[0] >= 'a' && key[0] <= 'z') oops();

    sprintf(out, "%s%s%s", ctrl, shift, key);
    return true;
}

void GraphicsWindow::Init(void) {
    scale = 5;
    offset    = Vector::From(0, 0, 0);
    projRight = Vector::From(1, 0, 0);
    projUp    = Vector::From(0, 1, 0);

    // Make sure those are valid; could get a mouse move without a mouse
    // down if someone depresses the button, then drags into our window.
    orig.projRight = projRight;
    orig.projUp = projUp;

    // And with the last group active
    activeGroup = SK.group.elem[SK.group.n-1].h;
    SK.GetGroup(activeGroup)->Activate();

    showWorkplanes = false;
    showNormals = true;
    showPoints = true;
    showConstraints = true;
    showHdnLines = false;
    showShaded = true;
    showEdges = true;
    showMesh = false;

    showTextWindow = true;
    ShowTextWindow(showTextWindow);

    showSnapGrid = false;
    context.active = false;

    // Do this last, so that all the menus get updated correctly.
    EnsureValidActives();
}

void GraphicsWindow::AnimateOntoWorkplane(void) {
    if(!LockedInWorkplane()) return;

    Entity *w = SK.GetEntity(ActiveWorkplane());
    Quaternion quatf = w->Normal()->NormalGetNum();
    Vector offsetf = (SK.GetEntity(w->point[0])->PointGetNum()).ScaledBy(-1);

    AnimateOnto(quatf, offsetf);
}

void GraphicsWindow::AnimateOnto(Quaternion quatf, Vector offsetf) {
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
    int32_t dt = (mp < 0.01 && mo < 10) ? (-20) :
                     (int32_t)(100 + 1000*mp + 0.4*mo);
    // Don't ever animate for longer than 2000 ms; we can get absurdly
    // long translations (as measured in pixels) if the user zooms out, moves,
    // and then zooms in again.
    if(dt > 2000) dt = 2000;
    int64_t tn, t0 = GetMilliseconds();
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
    // If the view screen is open, then we need to refresh it.
    SS.ScheduleShowTW();
}

void GraphicsWindow::HandlePointForZoomToFit(Vector p,
                        Point2d *pmax, Point2d *pmin, double *wmin, bool div)
{
    double w;
    Vector pp = ProjectPoint4(p, &w);
    // If div is true, then we calculate a perspective projection of the point.
    // If not, then we do a parallel projection regardless of the current
    // scale factor.
    if(div) {
        pp = pp.ScaledBy(1.0/w);
    }

    pmax->x = max(pmax->x, pp.x);
    pmax->y = max(pmax->y, pp.y);
    pmin->x = min(pmin->x, pp.x);
    pmin->y = min(pmin->y, pp.y);
    *wmin = min(*wmin, w);
}
void GraphicsWindow::LoopOverPoints(Point2d *pmax, Point2d *pmin, double *wmin,
                        bool div, bool includingInvisibles)
{
    HandlePointForZoomToFit(Vector::From(0, 0, 0), pmax, pmin, wmin, div);

    int i, j;
    for(i = 0; i < SK.entity.n; i++) {
        Entity *e = &(SK.entity.elem[i]);
        if(!(e->IsVisible() || includingInvisibles)) continue;
        if(e->IsPoint()) {
            HandlePointForZoomToFit(e->PointGetNum(), pmax, pmin, wmin, div);
        } else if(e->type == Entity::CIRCLE) {
            // Lots of entities can extend outside the bbox of their points,
            // but circles are particularly bad. We want to get things halfway
            // reasonable without the mesh, because a zoom to fit is used to
            // set the zoom level to set the chord tol.
            double r = e->CircleGetRadiusNum();
            Vector c = SK.GetEntity(e->point[0])->PointGetNum();
            Quaternion q = SK.GetEntity(e->normal)->NormalGetNum();
            for(j = 0; j < 4; j++) {
                Vector p = (j == 0) ? (c.Plus(q.RotationU().ScaledBy( r))) :
                           (j == 1) ? (c.Plus(q.RotationU().ScaledBy(-r))) :
                           (j == 2) ? (c.Plus(q.RotationV().ScaledBy( r))) :
                                      (c.Plus(q.RotationV().ScaledBy(-r)));
                HandlePointForZoomToFit(p, pmax, pmin, wmin, div);
            }
        }
    }

    Group *g = SK.GetGroup(activeGroup);
    g->GenerateDisplayItems();
    for(i = 0; i < g->displayMesh.l.n; i++) {
        STriangle *tr = &(g->displayMesh.l.elem[i]);
        HandlePointForZoomToFit(tr->a, pmax, pmin, wmin, div);
        HandlePointForZoomToFit(tr->b, pmax, pmin, wmin, div);
        HandlePointForZoomToFit(tr->c, pmax, pmin, wmin, div);
    }
    for(i = 0; i < g->polyLoops.l.n; i++) {
        SContour *sc = &(g->polyLoops.l.elem[i]);
        for(j = 0; j < sc->l.n; j++) {
            HandlePointForZoomToFit(sc->l.elem[j].p, pmax, pmin, wmin, div);
        }
    }
}
void GraphicsWindow::ZoomToFit(bool includingInvisibles) {
    // On the first run, ignore perspective.
    Point2d pmax = { -1e12, -1e12 }, pmin = { 1e12, 1e12 };
    double wmin = 1;
    LoopOverPoints(&pmax, &pmin, &wmin, false, includingInvisibles);

    double xm = (pmax.x + pmin.x)/2, ym = (pmax.y + pmin.y)/2;
    double dx = pmax.x - pmin.x, dy = pmax.y - pmin.y;

    offset = offset.Plus(projRight.ScaledBy(-xm)).Plus(
                         projUp.   ScaledBy(-ym));

    // And based on this, we calculate the scale and offset
    if(EXACT(dx == 0 && dy == 0)) {
        scale = 5;
    } else {
        double scalex = 1e12, scaley = 1e12;
        if(EXACT(dx != 0)) scalex = 0.9*width /dx;
        if(EXACT(dy != 0)) scaley = 0.9*height/dy;
        scale = min(scalex, scaley);

        scale = min(300.0, scale);
        scale = max(0.003, scale);
    }

    // Then do another run, considering the perspective.
    pmax.x = -1e12; pmax.y = -1e12;
    pmin.x =  1e12; pmin.y =  1e12;
    wmin = 1;
    LoopOverPoints(&pmax, &pmin, &wmin, true, includingInvisibles);

    // Adjust the scale so that no points are behind the camera
    if(wmin < 0.1) {
        double k = SS.CameraTangent();
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
            SS.ScheduleShowTW();
            break;

        case MNU_ZOOM_OUT:
            SS.GW.scale /= 1.2;
            SS.ScheduleShowTW();
            break;

        case MNU_ZOOM_TO_FIT:
            SS.GW.ZoomToFit(false);
            SS.ScheduleShowTW();
            break;

        case MNU_SHOW_GRID:
            SS.GW.showSnapGrid = !SS.GW.showSnapGrid;
            if(SS.GW.showSnapGrid && !SS.GW.LockedInWorkplane()) {
                Message("No workplane is active, so the grid will not "
                        "appear.");
            }
            SS.GW.EnsureValidActives();
            InvalidateGraphics();
            break;

        case MNU_PERSPECTIVE_PROJ:
            SS.usePerspectiveProj = !SS.usePerspectiveProj;
            if(SS.cameraTangent < 1e-6) {
                Error("The perspective factor is set to zero, so the view will "
                      "always be a parallel projection.\n\n"
                      "For a perspective projection, modify the perspective "
                      "factor in the configuration screen. A value around 0.3 "
                      "is typical.");
            }
            SS.GW.EnsureValidActives();
            InvalidateGraphics();
            break;

        case MNU_ONTO_WORKPLANE:
            if(SS.GW.LockedInWorkplane()) {
                SS.GW.AnimateOntoWorkplane();
                SS.GW.ClearSuper();
                SS.ScheduleShowTW();
                break;
            }  // if not in 2d mode fall through and use ORTHO logic
        case MNU_NEAREST_ORTHO:
        case MNU_NEAREST_ISO: {
            static const Vector ortho[3] = {
                Vector::From(1, 0, 0),
                Vector::From(0, 1, 0),
                Vector::From(0, 0, 1)
            };
            double sqrt2 = sqrt(2.0), sqrt6 = sqrt(6.0);
            Quaternion quat0 = Quaternion::From(SS.GW.projRight, SS.GW.projUp);
            Quaternion quatf = quat0;
            double dmin = 1e10;

            // There are 24 possible views; 3*2*2*2
            int i, j, negi, negj;
            for(i = 0; i < 3; i++) {
                for(j = 0; j < 3; j++) {
                    if(i == j) continue;
                    for(negi = 0; negi < 2; negi++) {
                        for(negj = 0; negj < 2; negj++) {
                            Vector ou = ortho[i], ov = ortho[j];
                            if(negi) ou = ou.ScaledBy(-1);
                            if(negj) ov = ov.ScaledBy(-1);
                            Vector on = ou.Cross(ov);

                            Vector u, v;
                            if(id == MNU_NEAREST_ORTHO || id == MNU_ONTO_WORKPLANE) {
                                u = ou;
                                v = ov;
                            } else {
                                u =
                                    ou.ScaledBy(1/sqrt2).Plus(
                                    on.ScaledBy(-1/sqrt2));
                                v =
                                    ou.ScaledBy(-1/sqrt6).Plus(
                                    ov.ScaledBy(2/sqrt6).Plus(
                                    on.ScaledBy(-1/sqrt6)));
                            }

                            Quaternion quatt = Quaternion::From(u, v);
                            double d = min(
                                (quatt.Minus(quat0)).Magnitude(),
                                (quatt.Plus(quat0)).Magnitude());
                            if(d < dmin) {
                                dmin = d;
                                quatf = quatt;
                            }
                        }
                    }
                }
            }

            SS.GW.AnimateOnto(quatf, SS.GW.offset);
            break;
        }

        case MNU_CENTER_VIEW:
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 1 && SS.GW.gs.points == 1) {
                Quaternion quat0;
                // Offset is the selected point, quaternion is same as before
                Vector pt = SK.GetEntity(SS.GW.gs.point[0])->PointGetNum();
                quat0 = Quaternion::From(SS.GW.projRight, SS.GW.projUp);
                SS.GW.AnimateOnto(quat0, pt.ScaledBy(-1));
                SS.GW.ClearSelection();
            } else {
                Error("Select a point; this point will become the center "
                      "of the view on screen.");
            }
            break;

        case MNU_SHOW_MENU_BAR:
            ToggleMenuBar();
            SS.GW.EnsureValidActives();
            InvalidateGraphics();
            break;

        case MNU_SHOW_TOOLBAR:
            SS.showToolbar = !SS.showToolbar;
            SS.GW.EnsureValidActives();
            InvalidateGraphics();
            break;

        case MNU_SHOW_TEXT_WND:
            SS.GW.showTextWindow = !SS.GW.showTextWindow;
            SS.GW.EnsureValidActives();
            break;

        case MNU_UNITS_INCHES:
            SS.viewUnits = SolveSpaceUI::UNIT_INCHES;
            SS.ScheduleShowTW();
            SS.GW.EnsureValidActives();
            break;

        case MNU_UNITS_MM:
            SS.viewUnits = SolveSpaceUI::UNIT_MM;
            SS.ScheduleShowTW();
            SS.GW.EnsureValidActives();
            break;

        case MNU_FULL_SCREEN:
            ToggleFullScreen();
            SS.GW.EnsureValidActives();
            break;

        default: oops();
    }
    InvalidateGraphics();
}

void GraphicsWindow::EnsureValidActives(void) {
    bool change = false;
    // The active group must exist, and not be the references.
    Group *g = SK.group.FindByIdNoOops(activeGroup);
    if((!g) || (g->h.v == Group::HGROUP_REFERENCES.v)) {
        int i;
        for(i = 0; i < SK.group.n; i++) {
            if(SK.group.elem[i].h.v != Group::HGROUP_REFERENCES.v) {
                break;
            }
        }
        if(i >= SK.group.n) {
            // This can happen if the user deletes all the groups in the
            // sketch. It's difficult to prevent that, because the last
            // group might have been deleted automatically, because it failed
            // a dependency. There needs to be something, so create a plane
            // drawing group and activate that. They should never be able
            // to delete the references, though.
            activeGroup = SS.CreateDefaultDrawingGroup();
        } else {
            activeGroup = SK.group.elem[i].h;
        }
        SK.GetGroup(activeGroup)->Activate();
        change = true;
    }

    // The active coordinate system must also exist.
    if(LockedInWorkplane()) {
        Entity *e = SK.entity.FindByIdNoOops(ActiveWorkplane());
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
    RadioMenuById(MNU_FREE_IN_3D, !locked);
    RadioMenuById(MNU_SEL_WORKPLANE, locked);

    SS.UndoEnableMenus();

    switch(SS.viewUnits) {
        case SolveSpaceUI::UNIT_MM:
        case SolveSpaceUI::UNIT_INCHES:
            break;
        default:
            SS.viewUnits = SolveSpaceUI::UNIT_MM;
            break;
    }
    RadioMenuById(MNU_UNITS_MM, SS.viewUnits == SolveSpaceUI::UNIT_MM);
    RadioMenuById(MNU_UNITS_INCHES, SS.viewUnits == SolveSpaceUI::UNIT_INCHES);

    ShowTextWindow(SS.GW.showTextWindow);
    CheckMenuById(MNU_SHOW_TEXT_WND, SS.GW.showTextWindow);

#if defined(__APPLE__)
    CheckMenuById(MNU_SHOW_MENU_BAR, MenuBarIsVisible());
#endif
    CheckMenuById(MNU_SHOW_TOOLBAR, SS.showToolbar);
    CheckMenuById(MNU_PERSPECTIVE_PROJ, SS.usePerspectiveProj);
    CheckMenuById(MNU_SHOW_GRID, SS.GW.showSnapGrid);
#if defined(HAVE_GTK) || defined(__APPLE__)
    CheckMenuById(MNU_FULL_SCREEN, FullScreenIsActive());
#endif

    if(change) SS.ScheduleShowTW();
}

void GraphicsWindow::SetWorkplaneFreeIn3d(void) {
    SK.GetGroup(activeGroup)->activeWorkplane = Entity::FREE_IN_3D;
}
hEntity GraphicsWindow::ActiveWorkplane(void) {
    Group *g = SK.group.FindByIdNoOops(activeGroup);
    if(g) {
        return g->activeWorkplane;
    } else {
        return Entity::FREE_IN_3D;
    }
}
bool GraphicsWindow::LockedInWorkplane(void) {
    return (SS.GW.ActiveWorkplane().v != Entity::FREE_IN_3D.v);
}

void GraphicsWindow::ForceTextWindowShown(void) {
    if(!showTextWindow) {
        showTextWindow = true;
        CheckMenuById(MNU_SHOW_TEXT_WND, true);
        ShowTextWindow(true);
    }
}

void GraphicsWindow::DeleteTaggedRequests(void) {
    // Rewrite any point-coincident constraints that were affected by this
    // deletion.
    Request *r;
    for(r = SK.request.First(); r; r = SK.request.NextAfter(r)) {
        if(!r->tag) continue;
        FixConstraintsForRequestBeingDeleted(r->h);
    }
    // and then delete the tagged requests.
    SK.request.RemoveTagged();

    // An edit might be in progress for the just-deleted item. So
    // now it's not.
    HideGraphicsEditControl();
    SS.TW.HideEditControl();
    // And clear out the selection, which could contain that item.
    ClearSuper();
    // And regenerate to get rid of what it generates, plus anything
    // that references it (since the regen code checks for that).
    SS.GenerateAll(0, INT_MAX);
    EnsureValidActives();
    SS.ScheduleShowTW();
}

Vector GraphicsWindow::SnapToGrid(Vector p) {
    if(!LockedInWorkplane()) return p;

    EntityBase *wrkpl = SK.GetEntity(ActiveWorkplane()),
               *norm  = wrkpl->Normal();
    Vector wo = SK.GetEntity(wrkpl->point[0])->PointGetNum(),
           wu = norm->NormalU(),
           wv = norm->NormalV(),
           wn = norm->NormalN();

    Vector pp = (p.Minus(wo)).DotInToCsys(wu, wv, wn);
    pp.x = floor((pp.x / SS.gridSpacing) + 0.5)*SS.gridSpacing;
    pp.y = floor((pp.y / SS.gridSpacing) + 0.5)*SS.gridSpacing;
    pp.z = 0;

    return pp.ScaleOutOfCsys(wu, wv, wn).Plus(wo);
}

void GraphicsWindow::MenuEdit(int id) {
    switch(id) {
        case MNU_UNSELECT_ALL:
            SS.GW.GroupSelection();
            // If there's nothing selected to de-select, and no operation
            // to cancel, then perhaps they want to return to the home
            // screen in the text window.
            if(SS.GW.gs.n               == 0 &&
               SS.GW.gs.constraints     == 0 &&
               SS.GW.pending.operation  == 0)
            {
                if(!(TextEditControlIsVisible() ||
                     GraphicsEditControlIsVisible()))
                {
                    if(SS.TW.shown.screen == TextWindow::SCREEN_STYLE_INFO) {
                        SS.TW.GoToScreen(TextWindow::SCREEN_LIST_OF_STYLES);
                    } else {
                        SS.TW.ClearSuper();
                    }
                }
            }
            SS.GW.ClearSuper();
            SS.TW.HideEditControl();
            SS.nakedEdges.Clear();
            SS.justExportedInfo.draw = false;
            // This clears the marks drawn to indicate which points are
            // still free to drag.
            Param *p;
            for(p = SK.param.First(); p; p = SK.param.NextAfter(p)) {
                p->free = false;
            }
            break;

        case MNU_SELECT_ALL: {
            Entity *e;
            for(e = SK.entity.First(); e; e = SK.entity.NextAfter(e)) {
                if(e->group.v != SS.GW.activeGroup.v) continue;
                if(e->IsFace() || e->IsDistance()) continue;
                if(!e->IsVisible()) continue;

                SS.GW.MakeSelected(e->h);
            }
            InvalidateGraphics();
            SS.ScheduleShowTW();
            break;
        }

        case MNU_SELECT_CHAIN: {
            Entity *e;
            int newlySelected = 0;
            bool didSomething;
            do {
                didSomething = false;
                for(e = SK.entity.First(); e; e = SK.entity.NextAfter(e)) {
                    if(e->group.v != SS.GW.activeGroup.v) continue;
                    if(!e->HasEndpoints()) continue;
                    if(!e->IsVisible()) continue;

                    Vector st = e->EndpointStart(),
                           fi = e->EndpointFinish();

                    bool onChain = false, alreadySelected = false;
                    List<Selection> *ls = &(SS.GW.selection);
                    for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
                        if(!s->entity.v) continue;
                        if(s->entity.v == e->h.v) {
                            alreadySelected = true;
                            continue;
                        }
                        Entity *se = SK.GetEntity(s->entity);
                        if(!se->HasEndpoints()) continue;

                        Vector sst = se->EndpointStart(),
                               sfi = se->EndpointFinish();

                        if(sst.Equals(st) || sst.Equals(fi) ||
                           sfi.Equals(st) || sfi.Equals(fi))
                        {
                            onChain = true;
                        }
                    }
                    if(onChain && !alreadySelected) {
                        SS.GW.MakeSelected(e->h);
                        newlySelected++;
                        didSomething = true;
                    }
                }
            } while(didSomething);
            if(newlySelected == 0) {
                Error("No additional entities share endpoints with the "
                      "selected entities.");
            }
            InvalidateGraphics();
            SS.ScheduleShowTW();
            break;
        }

        case MNU_ROTATE_90: {
            SS.GW.GroupSelection();
            Entity *e = NULL;
            if(SS.GW.gs.n == 1 && SS.GW.gs.points == 1) {
                e = SK.GetEntity(SS.GW.gs.point[0]);
            } else if(SS.GW.gs.n == 1 && SS.GW.gs.entities == 1) {
                e = SK.GetEntity(SS.GW.gs.entity[0]);
            }
            SS.GW.ClearSelection();

            hGroup hg = e ? e->group : SS.GW.activeGroup;
            Group *g = SK.GetGroup(hg);
            if(g->type != Group::IMPORTED) {
                Error("To use this command, select a point or other "
                      "entity from an imported part, or make an import "
                      "group the active group.");
                break;
            }


            SS.UndoRemember();
            // Rotate by ninety degrees about the coordinate axis closest
            // to the screen normal.
            Vector norm = SS.GW.projRight.Cross(SS.GW.projUp);
            norm = norm.ClosestOrtho();
            norm = norm.WithMagnitude(1);
            Quaternion qaa = Quaternion::From(norm, PI/2);

            g->TransformImportedBy(Vector::From(0, 0, 0), qaa);

            // and regenerate as necessary.
            SS.MarkGroupDirty(hg);
            SS.GenerateAll();
            break;
        }

        case MNU_SNAP_TO_GRID: {
            if(!SS.GW.LockedInWorkplane()) {
                Error("No workplane is active. Select a workplane to define "
                      "the plane for the snap grid.");
                break;
            }
            SS.GW.GroupSelection();
            if(SS.GW.gs.points == 0 && SS.GW.gs.comments == 0) {
                Error("Can't snap these items to grid; select points or "
                      "text comments. To snap a line, select its endpoints.");
                break;
            }
            SS.UndoRemember();

            List<Selection> *ls = &(SS.GW.selection);
            for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
                if(s->entity.v) {
                    hEntity hp = s->entity;
                    Entity *ep = SK.GetEntity(hp);
                    if(!ep->IsPoint()) continue;

                    Vector p = ep->PointGetNum();
                    ep->PointForceTo(SS.GW.SnapToGrid(p));
                    SS.GW.pending.points.Add(&hp);
                    SS.MarkGroupDirty(ep->group);
                } else if(s->constraint.v) {
                    Constraint *c = SK.GetConstraint(s->constraint);
                    if(c->type != Constraint::COMMENT) continue;

                    c->disp.offset = SS.GW.SnapToGrid(c->disp.offset);
                }
            }
            // Regenerate, with these points marked as dragged so that they
            // get placed as close as possible to our snap grid.
            SS.GenerateAll();
            SS.GW.ClearPending();

            SS.GW.ClearSelection();
            InvalidateGraphics();
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
            SS.ScheduleShowTW();
            break;

        default: oops();
    }
}

void GraphicsWindow::MenuRequest(int id) {
    const char *s;
    switch(id) {
        case MNU_SEL_WORKPLANE: {
            SS.GW.GroupSelection();
            Group *g = SK.GetGroup(SS.GW.activeGroup);

            if(SS.GW.gs.n == 1 && SS.GW.gs.workplanes == 1) {
                // A user-selected workplane
                g->activeWorkplane = SS.GW.gs.entity[0];
            } else if(g->type == Group::DRAWING_WORKPLANE) {
                // The group's default workplane
                g->activeWorkplane = g->h.entity(0);
                Message("No workplane selected. Activating default workplane "
                        "for this group.");
            }

            if(!SS.GW.LockedInWorkplane()) {
                Error("No workplane is selected, and the active group does "
                      "not have a default workplane. Try selecting a "
                      "workplane, or activating a sketch-in-new-workplane "
                      "group.");
                break;
            }
            // Align the view with the selected workplane
            SS.GW.AnimateOntoWorkplane();
            SS.GW.ClearSuper();
            SS.ScheduleShowTW();
            break;
        }
        case MNU_FREE_IN_3D:
            SS.GW.SetWorkplaneFreeIn3d();
            SS.GW.EnsureValidActives();
            SS.ScheduleShowTW();
            InvalidateGraphics();
            break;

        case MNU_TANGENT_ARC:
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 1 && SS.GW.gs.points == 1) {
                SS.GW.MakeTangentArc();
            } else if(SS.GW.gs.n != 0) {
                Error("Bad selection for tangent arc at point. Select a "
                      "single point, or select nothing to set up arc "
                      "parameters.");
            } else {
                SS.TW.GoToScreen(TextWindow::SCREEN_TANGENT_ARC);
                SS.GW.ForceTextWindowShown();
                SS.ScheduleShowTW();
                InvalidateGraphics(); // repaint toolbar
            }
            break;

        case MNU_ARC: s = "click point on arc (draws anti-clockwise)"; goto c;
        case MNU_DATUM_POINT: s = "click to place datum point"; goto c;
        case MNU_LINE_SEGMENT: s = "click first point of line segment"; goto c;
        case MNU_CONSTR_SEGMENT: s = "click first point of construction line segment"; goto c;
        case MNU_CUBIC: s = "click first point of cubic segment"; goto c;
        case MNU_CIRCLE: s = "click center of circle"; goto c;
        case MNU_WORKPLANE: s = "click origin of workplane"; goto c;
        case MNU_RECTANGLE: s = "click one corner of rectangle"; goto c;
        case MNU_TTF_TEXT: s = "click top left of text"; goto c;
c:
            SS.GW.pending.operation = id;
            SS.GW.pending.description = s;
            SS.ScheduleShowTW();
            InvalidateGraphics(); // repaint toolbar
            break;

        case MNU_CONSTRUCTION: {
            SS.UndoRemember();
            SS.GW.GroupSelection();
            if(SS.GW.gs.entities == 0) {
                Error("No entities are selected. Select entities before "
                      "trying to toggle their construction state.");
            }
            int i;
            for(i = 0; i < SS.GW.gs.entities; i++) {
                hEntity he = SS.GW.gs.entity[i];
                if(!he.isFromRequest()) continue;
                Request *r = SK.GetRequest(he.request());
                r->construction = !(r->construction);
                SS.MarkGroupDirty(r->group);
            }
            SS.GW.ClearSelection();
            SS.GenerateAll();
            break;
        }

        case MNU_SPLIT_CURVES:
            SS.GW.SplitLinesOrCurves();
            break;

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

void GraphicsWindow::ToggleBool(bool *v) {
    *v = !*v;

    // The faces are shown as special stippling on the shaded triangle mesh,
    // so not meaningful to show them and hide the shaded.
    if(!showShaded) showFaces = false;

    // We might need to regenerate the mesh and edge list, since the edges
    // wouldn't have been generated if they were previously hidden.
    if(showEdges) (SK.GetGroup(activeGroup))->displayDirty = true;

    SS.GenerateAll();
    InvalidateGraphics();
    SS.ScheduleShowTW();
}

GraphicsWindow::SuggestedConstraint GraphicsWindow::SuggestLineConstraint(hRequest request) {
    if(LockedInWorkplane()) {
        Entity *ptA = SK.GetEntity(request.entity(1)),
               *ptB = SK.GetEntity(request.entity(2));

        Expr *au, *av, *bu, *bv;

        ptA->PointGetExprsInWorkplane(ActiveWorkplane(), &au, &av);
        ptB->PointGetExprsInWorkplane(ActiveWorkplane(), &bu, &bv);

        double du = au->Minus(bu)->Eval();
        double dv = av->Minus(bv)->Eval();

        const double TOLERANCE_RATIO = 0.02;
        if(fabs(dv) > LENGTH_EPS && fabs(du / dv) < TOLERANCE_RATIO)
            return SUGGESTED_VERTICAL;
        else if(fabs(du) > LENGTH_EPS && fabs(dv / du) < TOLERANCE_RATIO)
            return SUGGESTED_HORIZONTAL;
        else
            return SUGGESTED_NONE;
    } else {
        return SUGGESTED_NONE;
    }
}
