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
#define TN    MenuKind::NORMAL
#define TC    MenuKind::CHECK
#define TR    MenuKind::RADIO
const GraphicsWindow::MenuEntry GraphicsWindow::menu[] = {
//level
//   label                              id                         accel    ty   fn
{ 0, N_("&File"),                       Command::NONE,             0,       TN, NULL  },
{ 1, N_("&New"),                        Command::NEW,              C|'N',   TN, mFile },
{ 1, N_("&Open..."),                    Command::OPEN,             C|'O',   TN, mFile },
{ 1, N_("Open &Recent"),                Command::OPEN_RECENT,      0,       TN, mFile },
{ 1, N_("&Save"),                       Command::SAVE,             C|'S',   TN, mFile },
{ 1, N_("Save &As..."),                 Command::SAVE_AS,          0,       TN, mFile },
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("Export &Image..."),            Command::EXPORT_PNG,       0,       TN, mFile },
{ 1, N_("Export 2d &View..."),          Command::EXPORT_VIEW,      0,       TN, mFile },
{ 1, N_("Export 2d &Section..."),       Command::EXPORT_SECTION,   0,       TN, mFile },
{ 1, N_("Export 3d &Wireframe..."),     Command::EXPORT_WIREFRAME, 0,       TN, mFile },
{ 1, N_("Export Triangle &Mesh..."),    Command::EXPORT_MESH,      0,       TN, mFile },
{ 1, N_("Export &Surfaces..."),         Command::EXPORT_SURFACES,  0,       TN, mFile },
{ 1, N_("Im&port..."),                  Command::IMPORT,           0,       TN, mFile },
#ifndef __APPLE__
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("E&xit"),                       Command::EXIT,             C|'Q',   TN, mFile },
#endif

{ 0, N_("&Edit"),                       Command::NONE,             0,       TN, NULL  },
{ 1, N_("&Undo"),                       Command::UNDO,             C|'Z',   TN, mEdit },
{ 1, N_("&Redo"),                       Command::REDO,             C|'Y',   TN, mEdit },
{ 1, N_("Re&generate All"),             Command::REGEN_ALL,        ' ',     TN, mEdit },
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("Snap Selection to &Grid"),     Command::SNAP_TO_GRID,     '.',     TN, mEdit },
{ 1, N_("Rotate Imported &90°"),        Command::ROTATE_90,        '9',     TN, mEdit },
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("Cu&t"),                        Command::CUT,              C|'X',   TN, mClip },
{ 1, N_("&Copy"),                       Command::COPY,             C|'C',   TN, mClip },
{ 1, N_("&Paste"),                      Command::PASTE,            C|'V',   TN, mClip },
{ 1, N_("Paste &Transformed..."),       Command::PASTE_TRANSFORM,  C|'T',   TN, mClip },
{ 1, N_("&Delete"),                     Command::DELETE,           DEL,     TN, mClip },
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("Select &Edge Chain"),          Command::SELECT_CHAIN,     C|'E',   TN, mEdit },
{ 1, N_("Select &All"),                 Command::SELECT_ALL,       C|'A',   TN, mEdit },
{ 1, N_("&Unselect All"),               Command::UNSELECT_ALL,     ESC,     TN, mEdit },

{ 0, N_("&View"),                       Command::NONE,             0,       TN, NULL  },
{ 1, N_("Zoom &In"),                    Command::ZOOM_IN,          '+',     TN, mView },
{ 1, N_("Zoom &Out"),                   Command::ZOOM_OUT,         '-',     TN, mView },
{ 1, N_("Zoom To &Fit"),                Command::ZOOM_TO_FIT,      'F',     TN, mView },
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("Align View to &Workplane"),    Command::ONTO_WORKPLANE,   'W',     TN, mView },
{ 1, N_("Nearest &Ortho View"),         Command::NEAREST_ORTHO,    F(2),    TN, mView },
{ 1, N_("Nearest &Isometric View"),     Command::NEAREST_ISO,      F(3),    TN, mView },
{ 1, N_("&Center View At Point"),       Command::CENTER_VIEW,      F(4),    TN, mView },
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("Show Snap &Grid"),             Command::SHOW_GRID,        '>',     TC, mView },
{ 1, N_("Use &Perspective Projection"), Command::PERSPECTIVE_PROJ, '`',    TC, mView },
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("Show &Toolbar"),               Command::SHOW_TOOLBAR,     0,       TC, mView },
{ 1, N_("Show Property Bro&wser"),      Command::SHOW_TEXT_WND,    '\t',    TC, mView },
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("Dimensions in &Inches"),       Command::UNITS_INCHES,     0,       TR, mView },
{ 1, N_("Dimensions in &Millimeters"),  Command::UNITS_MM,         0,       TR, mView },
{ 1,  NULL,                             Command::NONE,             0,       TN, NULL  },
{ 1, N_("&Full Screen"),                Command::FULL_SCREEN,      C|F(11), TC, mView },

{ 0, N_("&New Group"),                  Command::NONE,             0,       TN, NULL  },
{ 1, N_("Sketch In &3d"),               Command::GROUP_3D,         S|'3',   TN, mGrp  },
{ 1, N_("Sketch In New &Workplane"),    Command::GROUP_WRKPL,      S|'W',   TN, mGrp  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("Step &Translating"),           Command::GROUP_TRANS,      S|'T',   TN, mGrp  },
{ 1, N_("Step &Rotating"),              Command::GROUP_ROT,        S|'R',   TN, mGrp  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("E&xtrude"),                    Command::GROUP_EXTRUDE,    S|'X',   TN, mGrp  },
{ 1, N_("&Lathe"),                      Command::GROUP_LATHE,      S|'L',   TN, mGrp  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("Link / Assemble..."),          Command::GROUP_LINK,       S|'I',   TN, mGrp  },
{ 1, N_("Link Recent"),                 Command::GROUP_RECENT,     0,       TN, mGrp  },

{ 0, N_("&Sketch"),                     Command::NONE,             0,       TN, NULL  },
{ 1, N_("In &Workplane"),               Command::SEL_WORKPLANE,    '2',     TR, mReq  },
{ 1, N_("Anywhere In &3d"),             Command::FREE_IN_3D,       '3',     TR, mReq  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("Datum &Point"),                Command::DATUM_POINT,      'P',     TN, mReq  },
{ 1, N_("&Workplane"),                  Command::WORKPLANE,        0,       TN, mReq  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("Line &Segment"),               Command::LINE_SEGMENT,     'S',     TN, mReq  },
{ 1, N_("C&onstruction Line Segment"),  Command::CONSTR_SEGMENT,   S|'S',   TN, mReq  },
{ 1, N_("&Rectangle"),                  Command::RECTANGLE,        'R',     TN, mReq  },
{ 1, N_("&Circle"),                     Command::CIRCLE,           'C',     TN, mReq  },
{ 1, N_("&Arc of a Circle"),            Command::ARC,              'A',     TN, mReq  },
{ 1, N_("&Bezier Cubic Spline"),        Command::CUBIC,            'B',     TN, mReq  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("&Text in TrueType Font"),      Command::TTF_TEXT,         'T',     TN, mReq  },
{ 1, N_("&Image"),                      Command::IMAGE,            0,       TN, mReq  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("To&ggle Construction"),        Command::CONSTRUCTION,     'G',     TN, mReq  },
{ 1, N_("Tangent &Arc at Point"),       Command::TANGENT_ARC,      S|'A',   TN, mReq  },
{ 1, N_("Split Curves at &Intersection"), Command::SPLIT_CURVES,   'I',     TN, mReq  },

{ 0, N_("&Constrain"),                  Command::NONE,             0,       TN, NULL  },
{ 1, N_("&Distance / Diameter"),        Command::DISTANCE_DIA,     'D',     TN, mCon  },
{ 1, N_("Re&ference Dimension"),        Command::REF_DISTANCE,     S|'D',   TN, mCon  },
{ 1, N_("A&ngle"),                      Command::ANGLE,            'N',     TN, mCon  },
{ 1, N_("Reference An&gle"),            Command::REF_ANGLE,        S|'N',   TN, mCon  },
{ 1, N_("Other S&upplementary Angle"),  Command::OTHER_ANGLE,      'U',     TN, mCon  },
{ 1, N_("Toggle R&eference Dim"),       Command::REFERENCE,        'E',     TN, mCon  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("&Horizontal"),                 Command::HORIZONTAL,       'H',     TN, mCon  },
{ 1, N_("&Vertical"),                   Command::VERTICAL,         'V',     TN, mCon  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("&On Point / Curve / Plane"),   Command::ON_ENTITY,        'O',     TN, mCon  },
{ 1, N_("E&qual Length / Radius / Angle"), Command::EQUAL,         'Q',     TN, mCon  },
{ 1, N_("Length Ra&tio"),               Command::RATIO,            'Z',     TN, mCon  },
{ 1, N_("Length Diff&erence"),          Command::DIFFERENCE,       'J',     TN, mCon  },
{ 1, N_("At &Midpoint"),                Command::AT_MIDPOINT,      'M',     TN, mCon  },
{ 1, N_("S&ymmetric"),                  Command::SYMMETRIC,        'Y',     TN, mCon  },
{ 1, N_("Para&llel / Tangent"),         Command::PARALLEL,         'L',     TN, mCon  },
{ 1, N_("&Perpendicular"),              Command::PERPENDICULAR,    '[',     TN, mCon  },
{ 1, N_("Same Orient&ation"),           Command::ORIENTED_SAME,    'X',     TN, mCon  },
{ 1, N_("Lock Point Where &Dragged"),   Command::WHERE_DRAGGED,    ']',     TN, mCon  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("Comment"),                     Command::COMMENT,          ';',     TN, mCon  },

{ 0, N_("&Analyze"),                    Command::NONE,             0,       TN, NULL  },
{ 1, N_("Measure &Volume"),             Command::VOLUME,           C|S|'V', TN, mAna  },
{ 1, N_("Measure A&rea"),               Command::AREA,             C|S|'A', TN, mAna  },
{ 1, N_("Measure &Perimeter"),          Command::PERIMETER,        C|S|'P', TN, mAna  },
{ 1, N_("Show &Interfering Parts"),     Command::INTERFERENCE,     C|S|'I', TN, mAna  },
{ 1, N_("Show &Naked Edges"),           Command::NAKED_EDGES,      C|S|'N', TN, mAna  },
{ 1, N_("Show &Center of Mass"),        Command::CENTER_OF_MASS,      C|S|'C', TN, mAna  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("Show Degrees of &Freedom"),    Command::SHOW_DOF,         C|S|'F', TN, mAna  },
{ 1, NULL,                              Command::NONE,             0,       TN, NULL  },
{ 1, N_("&Trace Point"),                Command::TRACE_PT,         C|S|'T', TN, mAna  },
{ 1, N_("&Stop Tracing..."),            Command::STOP_TRACING,     C|S|'S', TN, mAna  },
{ 1, N_("Step &Dimension..."),          Command::STEP_DIM,         C|S|'D', TN, mAna  },

{ 0, N_("&Help"),                       Command::NONE,             0,       TN, NULL  },
{ 1, N_("&Website / Manual"),           Command::WEBSITE,          0,       TN, mHelp },
{ 1, N_("&Language"),                   Command::LOCALE,           0,       TN, mHelp },
#ifndef __APPLE__
{ 1, N_("&About"),                      Command::ABOUT,            0,       TN, mHelp },
#endif
{ -1, 0,                                Command::NONE,             0,       TN, 0     }
};

#undef DEL
#undef ESC
#undef S
#undef C
#undef F
#undef TN
#undef TC
#undef TR

std::string SolveSpace::MakeAcceleratorLabel(int accel) {
    if(!accel) return "";

    std::string label;
    if(accel & GraphicsWindow::CTRL_MASK) {
        label += "Ctrl+";
    }
    if(accel & GraphicsWindow::SHIFT_MASK) {
        label += "Shift+";
    }
    accel &= ~(GraphicsWindow::CTRL_MASK | GraphicsWindow::SHIFT_MASK);
    if(accel >= GraphicsWindow::FUNCTION_KEY_BASE + 1 &&
       accel <= GraphicsWindow::FUNCTION_KEY_BASE + 12) {
        label += ssprintf("F%d", accel - GraphicsWindow::FUNCTION_KEY_BASE);
    } else if(accel == '\t') {
        label += "Tab";
    } else if(accel == ' ') {
        label += "Space";
    } else if(accel == GraphicsWindow::ESCAPE_KEY) {
        label += "Esc";
    } else if(accel == GraphicsWindow::DELETE_KEY) {
        label += "Del";
    } else {
        label += (char)(accel & 0xff);
    }
    return label;
}

void GraphicsWindow::Init() {
    canvas = CreateRenderer();
    if(canvas) {
        persistentCanvas = canvas->CreateBatch();
        persistentDirty = true;
    }

    scale = 5;
    offset    = Vector::From(0, 0, 0);
    projRight = Vector::From(1, 0, 0);
    projUp    = Vector::From(0, 1, 0);

    // Make sure those are valid; could get a mouse move without a mouse
    // down if someone depresses the button, then drags into our window.
    orig.projRight = projRight;
    orig.projUp = projUp;

    // And with the last group active
    activeGroup = SK.groupOrder.elem[SK.groupOrder.n - 1];
    SK.GetGroup(activeGroup)->Activate();

    showWorkplanes = false;
    showNormals = true;
    showPoints = true;
    showConstraints = true;
    showShaded = true;
    showEdges = true;
    showMesh = false;
    showOutlines = false;
    drawOccludedAs = DrawOccludedAs::INVISIBLE;

    showTextWindow = true;
    ShowTextWindow(showTextWindow);

    showSnapGrid = false;
    context.active = false;

    // Do this last, so that all the menus get updated correctly.
    ClearSuper();
}

void GraphicsWindow::AnimateOntoWorkplane() {
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

void GraphicsWindow::HandlePointForZoomToFit(Vector p, Point2d *pmax, Point2d *pmin,
                                             double *wmin, bool usePerspective)
{
    double w;
    Vector pp = ProjectPoint4(p, &w);
    // If usePerspective is true, then we calculate a perspective projection of the point.
    // If not, then we do a parallel projection regardless of the current
    // scale factor.
    if(usePerspective) {
        pp = pp.ScaledBy(1.0/w);
    }

    pmax->x = max(pmax->x, pp.x);
    pmax->y = max(pmax->y, pp.y);
    pmin->x = min(pmin->x, pp.x);
    pmin->y = min(pmin->y, pp.y);
    *wmin = min(*wmin, w);
}
void GraphicsWindow::LoopOverPoints(const std::vector<Entity *> &entities,
                                    const std::vector<Constraint *> &constraints,
                                    const std::vector<hEntity> &faces,
                                    Point2d *pmax, Point2d *pmin, double *wmin,
                                    bool usePerspective, bool includeMesh) {

    for(Entity *e : entities) {
        if(e->IsPoint()) {
            HandlePointForZoomToFit(e->PointGetNum(), pmax, pmin, wmin, usePerspective);
        } else if(e->type == Entity::Type::CIRCLE) {
            // Lots of entities can extend outside the bbox of their points,
            // but circles are particularly bad. We want to get things halfway
            // reasonable without the mesh, because a zoom to fit is used to
            // set the zoom level to set the chord tol.
            double r = e->CircleGetRadiusNum();
            Vector c = SK.GetEntity(e->point[0])->PointGetNum();
            Quaternion q = SK.GetEntity(e->normal)->NormalGetNum();
            for(int j = 0; j < 4; j++) {
                Vector p = (j == 0) ? (c.Plus(q.RotationU().ScaledBy( r))) :
                           (j == 1) ? (c.Plus(q.RotationU().ScaledBy(-r))) :
                           (j == 2) ? (c.Plus(q.RotationV().ScaledBy( r))) :
                                      (c.Plus(q.RotationV().ScaledBy(-r)));
                HandlePointForZoomToFit(p, pmax, pmin, wmin, usePerspective);
            }
        } else {
            // We have to iterate children points, because we can select entites without points
            for(int i = 0; i < MAX_POINTS_IN_ENTITY; i++) {
                if(e->point[i].v == 0) break;
                Vector p = SK.GetEntity(e->point[i])->PointGetNum();
                HandlePointForZoomToFit(p, pmax, pmin, wmin, usePerspective);
            }
        }
    }

    for(Constraint *c : constraints) {
        std::vector<Vector> refs;
        c->GetReferencePoints(GetCamera(), &refs);
        for(Vector p : refs) {
            HandlePointForZoomToFit(p, pmax, pmin, wmin, usePerspective);
        }
    }

    if(!includeMesh && faces.empty()) return;

    Group *g = SK.GetGroup(activeGroup);
    g->GenerateDisplayItems();
    for(int i = 0; i < g->displayMesh.l.n; i++) {
        STriangle *tr = &(g->displayMesh.l.elem[i]);
        if(!includeMesh) {
            bool found = false;
            for(const hEntity &face : faces) {
                if(face.v != tr->meta.face) continue;
                found = true;
                break;
            }
            if(!found) continue;
        }
        HandlePointForZoomToFit(tr->a, pmax, pmin, wmin, usePerspective);
        HandlePointForZoomToFit(tr->b, pmax, pmin, wmin, usePerspective);
        HandlePointForZoomToFit(tr->c, pmax, pmin, wmin, usePerspective);
    }
    if(!includeMesh) return;
    for(int i = 0; i < g->polyLoops.l.n; i++) {
        SContour *sc = &(g->polyLoops.l.elem[i]);
        for(int j = 0; j < sc->l.n; j++) {
            HandlePointForZoomToFit(sc->l.elem[j].p, pmax, pmin, wmin, usePerspective);
        }
    }
}
void GraphicsWindow::ZoomToFit(bool includingInvisibles, bool useSelection) {
    std::vector<Entity *> entities;
    std::vector<Constraint *> constraints;
    std::vector<hEntity> faces;

    if(useSelection) {
        for(int i = 0; i < selection.n; i++) {
            Selection *s = &selection.elem[i];
            if(s->entity.v != 0) {
                Entity *e = SK.entity.FindById(s->entity);
                if(e->IsFace()) {
                    faces.push_back(e->h);
                    continue;
                }
                entities.push_back(e);
            }
            if(s->constraint.v != 0) {
                Constraint *c = SK.constraint.FindById(s->constraint);
                constraints.push_back(c);
            }
        }
    }

    bool selectionUsed = !entities.empty() || !constraints.empty() || !faces.empty();

    if(!selectionUsed) {
        for(Entity &e : SK.entity) {
            // we don't want to handle separate points, because we will iterate them inside entities.
            if(e.IsPoint()) continue;
            if(!includingInvisibles && !e.IsVisible()) continue;
            entities.push_back(&e);
        }

        for(Constraint &c : SK.constraint) {
            if(!c.IsVisible()) continue;
            constraints.push_back(&c);
        }
    }

    // On the first run, ignore perspective.
    Point2d pmax = { -1e12, -1e12 }, pmin = { 1e12, 1e12 };
    double wmin = 1;
    LoopOverPoints(entities, constraints, faces, &pmax, &pmin, &wmin,
                   /*usePerspective=*/false, /*includeMesh=*/!selectionUsed);

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
    LoopOverPoints(entities, constraints, faces, &pmax, &pmin, &wmin,
                   /*usePerspective=*/true, /*includeMesh=*/!selectionUsed);

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

void GraphicsWindow::MenuView(Command id) {
    switch(id) {
        case Command::ZOOM_IN:
            SS.GW.scale *= 1.2;
            SS.ScheduleShowTW();
            break;

        case Command::ZOOM_OUT:
            SS.GW.scale /= 1.2;
            SS.ScheduleShowTW();
            break;

        case Command::ZOOM_TO_FIT:
            SS.GW.ZoomToFit(/*includingInvisibles=*/false, /*useSelection=*/true);
            SS.ScheduleShowTW();
            break;

        case Command::SHOW_GRID:
            SS.GW.showSnapGrid = !SS.GW.showSnapGrid;
            if(SS.GW.showSnapGrid && !SS.GW.LockedInWorkplane()) {
                Message(_("No workplane is active, so the grid will not appear."));
            }
            SS.GW.EnsureValidActives();
            InvalidateGraphics();
            break;

        case Command::PERSPECTIVE_PROJ:
            SS.usePerspectiveProj = !SS.usePerspectiveProj;
            if(SS.cameraTangent < 1e-6) {
                Error(_("The perspective factor is set to zero, so the view will "
                        "always be a parallel projection.\n\n"
                        "For a perspective projection, modify the perspective "
                        "factor in the configuration screen. A value around 0.3 "
                        "is typical."));
            }
            SS.GW.EnsureValidActives();
            InvalidateGraphics();
            break;

        case Command::ONTO_WORKPLANE:
            if(SS.GW.LockedInWorkplane()) {
                SS.GW.AnimateOntoWorkplane();
                SS.ScheduleShowTW();
                break;
            }  // if not in 2d mode fall through and use ORTHO logic
        case Command::NEAREST_ORTHO:
        case Command::NEAREST_ISO: {
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
                            if(id == Command::NEAREST_ORTHO || id == Command::ONTO_WORKPLANE) {
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

        case Command::CENTER_VIEW:
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 1 && SS.GW.gs.points == 1) {
                Quaternion quat0;
                // Offset is the selected point, quaternion is same as before
                Vector pt = SK.GetEntity(SS.GW.gs.point[0])->PointGetNum();
                quat0 = Quaternion::From(SS.GW.projRight, SS.GW.projUp);
                SS.GW.AnimateOnto(quat0, pt.ScaledBy(-1));
                SS.GW.ClearSelection();
            } else {
                Error(_("Select a point; this point will become the center "
                        "of the view on screen."));
            }
            break;

        case Command::SHOW_TOOLBAR:
            SS.showToolbar = !SS.showToolbar;
            SS.GW.EnsureValidActives();
            InvalidateGraphics();
            break;

        case Command::SHOW_TEXT_WND:
            SS.GW.showTextWindow = !SS.GW.showTextWindow;
            SS.GW.EnsureValidActives();
            break;

        case Command::UNITS_INCHES:
            SS.viewUnits = Unit::INCHES;
            SS.ScheduleShowTW();
            SS.GW.EnsureValidActives();
            break;

        case Command::UNITS_MM:
            SS.viewUnits = Unit::MM;
            SS.ScheduleShowTW();
            SS.GW.EnsureValidActives();
            break;

        case Command::FULL_SCREEN:
            ToggleFullScreen();
            SS.GW.EnsureValidActives();
            break;

        default: ssassert(false, "Unexpected menu ID");
    }
    InvalidateGraphics();
}

void GraphicsWindow::EnsureValidActives() {
    bool change = false;
    // The active group must exist, and not be the references.
    Group *g = SK.group.FindByIdNoOops(activeGroup);
    if((!g) || (g->h.v == Group::HGROUP_REFERENCES.v)) {
        int i;
        for(i = 0; i < SK.groupOrder.n; i++) {
            if(SK.groupOrder.elem[i].v != Group::HGROUP_REFERENCES.v) {
                break;
            }
        }
        if(i >= SK.groupOrder.n) {
            // This can happen if the user deletes all the groups in the
            // sketch. It's difficult to prevent that, because the last
            // group might have been deleted automatically, because it failed
            // a dependency. There needs to be something, so create a plane
            // drawing group and activate that. They should never be able
            // to delete the references, though.
            activeGroup = SS.CreateDefaultDrawingGroup();
            // We've created the default group, but not the workplane entity;
            // do it now so that drawing mode isn't switched to "Free in 3d".
            SS.GenerateAll(SolveSpaceUI::Generate::ALL);
        } else {
            activeGroup = SK.groupOrder.elem[i];
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
    RadioMenuByCmd(Command::FREE_IN_3D, !locked);
    RadioMenuByCmd(Command::SEL_WORKPLANE, locked);

    SS.UndoEnableMenus();

    switch(SS.viewUnits) {
        case Unit::MM:
        case Unit::INCHES:
            break;
        default:
            SS.viewUnits = Unit::MM;
            break;
    }
    RadioMenuByCmd(Command::UNITS_MM, SS.viewUnits == Unit::MM);
    RadioMenuByCmd(Command::UNITS_INCHES, SS.viewUnits == Unit::INCHES);

    ShowTextWindow(SS.GW.showTextWindow);
    CheckMenuByCmd(Command::SHOW_TEXT_WND, /*checked=*/SS.GW.showTextWindow);

    CheckMenuByCmd(Command::SHOW_TOOLBAR, /*checked=*/SS.showToolbar);
    CheckMenuByCmd(Command::PERSPECTIVE_PROJ, /*checked=*/SS.usePerspectiveProj);
    CheckMenuByCmd(Command::SHOW_GRID,/*checked=*/SS.GW.showSnapGrid);
    CheckMenuByCmd(Command::FULL_SCREEN, /*checked=*/FullScreenIsActive());

    if(change) SS.ScheduleShowTW();
}

void GraphicsWindow::SetWorkplaneFreeIn3d() {
    SK.GetGroup(activeGroup)->activeWorkplane = Entity::FREE_IN_3D;
}
hEntity GraphicsWindow::ActiveWorkplane() {
    Group *g = SK.group.FindByIdNoOops(activeGroup);
    if(g) {
        return g->activeWorkplane;
    } else {
        return Entity::FREE_IN_3D;
    }
}
bool GraphicsWindow::LockedInWorkplane() {
    return (SS.GW.ActiveWorkplane().v != Entity::FREE_IN_3D.v);
}

void GraphicsWindow::ForceTextWindowShown() {
    if(!showTextWindow) {
        showTextWindow = true;
        CheckMenuByCmd(Command::SHOW_TEXT_WND, /*checked=*/true);
        ShowTextWindow(true);
    }
}

void GraphicsWindow::DeleteTaggedRequests() {
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
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
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

void GraphicsWindow::MenuEdit(Command id) {
    switch(id) {
        case Command::UNSELECT_ALL:
            SS.GW.GroupSelection();
            // If there's nothing selected to de-select, and no operation
            // to cancel, then perhaps they want to return to the home
            // screen in the text window.
            if(SS.GW.gs.n               == 0 &&
               SS.GW.gs.constraints     == 0 &&
               SS.GW.pending.operation  == Pending::NONE)
            {
                if(!(TextEditControlIsVisible() ||
                     GraphicsEditControlIsVisible()))
                {
                    if(SS.TW.shown.screen == TextWindow::Screen::STYLE_INFO) {
                        SS.TW.GoToScreen(TextWindow::Screen::LIST_OF_STYLES);
                    } else {
                        SS.TW.ClearSuper();
                    }
                }
            }
            SS.GW.ClearSuper();
            SS.TW.HideEditControl();
            SS.nakedEdges.Clear();
            SS.justExportedInfo.draw = false;
            SS.centerOfMass.draw = false;
            // This clears the marks drawn to indicate which points are
            // still free to drag.
            Param *p;
            for(p = SK.param.First(); p; p = SK.param.NextAfter(p)) {
                p->free = false;
            }
            if(SS.exportMode) {
                SS.exportMode = false;
                SS.GenerateAll(SolveSpaceUI::Generate::ALL);
            }
            SS.GW.persistentDirty = true;
            break;

        case Command::SELECT_ALL: {
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

        case Command::SELECT_CHAIN: {
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
                Error(_("No additional entities share endpoints with the selected entities."));
            }
            InvalidateGraphics();
            SS.ScheduleShowTW();
            break;
        }

        case Command::ROTATE_90: {
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
            if(g->type != Group::Type::LINKED) {
                Error(_("To use this command, select a point or other "
                        "entity from an linked part, or make a link "
                        "group the active group."));
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
            break;
        }

        case Command::SNAP_TO_GRID: {
            if(!SS.GW.LockedInWorkplane()) {
                Error(_("No workplane is active. Activate a workplane "
                        "(with Sketch -> In Workplane) to define the plane "
                        "for the snap grid."));
                break;
            }
            SS.GW.GroupSelection();
            if(SS.GW.gs.points == 0 && SS.GW.gs.constraintLabels == 0) {
                Error(_("Can't snap these items to grid; select points, "
                        "text comments, or constraints with a label. "
                        "To snap a line, select its endpoints."));
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
                    std::vector<Vector> refs;
                    c->GetReferencePoints(SS.GW.GetCamera(), &refs);
                    c->disp.offset = c->disp.offset.Plus(SS.GW.SnapToGrid(refs[0]).Minus(refs[0]));
                }
            }
            // Regenerate, with these points marked as dragged so that they
            // get placed as close as possible to our snap grid.
            SS.GW.ClearPending();

            SS.GW.ClearSelection();
            InvalidateGraphics();
            break;
        }

        case Command::UNDO:
            SS.UndoUndo();
            break;

        case Command::REDO:
            SS.UndoRedo();
            break;

        case Command::REGEN_ALL:
            SS.images.clear();
            SS.ReloadAllLinked(SS.saveFile);
            SS.GenerateAll(SolveSpaceUI::Generate::UNTIL_ACTIVE);
            SS.ScheduleShowTW();
            break;

        default: ssassert(false, "Unexpected menu ID");
    }
}

void GraphicsWindow::MenuRequest(Command id) {
    const char *s;
    switch(id) {
        case Command::SEL_WORKPLANE: {
            SS.GW.GroupSelection();
            Group *g = SK.GetGroup(SS.GW.activeGroup);

            if(SS.GW.gs.n == 1 && SS.GW.gs.workplanes == 1) {
                // A user-selected workplane
                g->activeWorkplane = SS.GW.gs.entity[0];
            } else if(g->type == Group::Type::DRAWING_WORKPLANE) {
                // The group's default workplane
                g->activeWorkplane = g->h.entity(0);
                Message(_("No workplane selected. Activating default workplane "
                          "for this group."));
            }

            if(!SS.GW.LockedInWorkplane()) {
                Error(_("No workplane is selected, and the active group does "
                        "not have a default workplane. Try selecting a "
                        "workplane, or activating a sketch-in-new-workplane "
                        "group."));
                break;
            }
            // Align the view with the selected workplane
            SS.GW.AnimateOntoWorkplane();
            SS.GW.ClearSuper();
            SS.ScheduleShowTW();
            break;
        }
        case Command::FREE_IN_3D:
            SS.GW.SetWorkplaneFreeIn3d();
            SS.GW.EnsureValidActives();
            SS.ScheduleShowTW();
            InvalidateGraphics();
            break;

        case Command::TANGENT_ARC:
            SS.GW.GroupSelection();
            if(SS.GW.gs.n == 1 && SS.GW.gs.points == 1) {
                SS.GW.MakeTangentArc();
            } else if(SS.GW.gs.n != 0) {
                Error(_("Bad selection for tangent arc at point. Select a "
                        "single point, or select nothing to set up arc "
                        "parameters."));
            } else {
                SS.TW.GoToScreen(TextWindow::Screen::TANGENT_ARC);
                SS.GW.ForceTextWindowShown();
                SS.ScheduleShowTW();
                InvalidateGraphics(); // repaint toolbar
            }
            break;

        case Command::ARC: s = _("click point on arc (draws anti-clockwise)"); goto c;
        case Command::DATUM_POINT: s = _("click to place datum point"); goto c;
        case Command::LINE_SEGMENT: s = _("click first point of line segment"); goto c;
        case Command::CONSTR_SEGMENT:
            s = _("click first point of construction line segment"); goto c;
        case Command::CUBIC: s = _("click first point of cubic segment"); goto c;
        case Command::CIRCLE: s = _("click center of circle"); goto c;
        case Command::WORKPLANE: s = _("click origin of workplane"); goto c;
        case Command::RECTANGLE: s = _("click one corner of rectangle"); goto c;
        case Command::TTF_TEXT: s = _("click top left of text"); goto c;
        case Command::IMAGE:
            if(!SS.ReloadLinkedImage(SS.saveFile, &SS.GW.pending.filename,
                                     /*canCancel=*/true)) {
                return;
            }
            s = _("click top left of image"); goto c;
c:
            SS.GW.pending.operation = GraphicsWindow::Pending::COMMAND;
            SS.GW.pending.command = id;
            SS.GW.pending.description = s;
            SS.ScheduleShowTW();
            InvalidateGraphics(); // repaint toolbar
            break;

        case Command::CONSTRUCTION: {
            SS.UndoRemember();
            SS.GW.GroupSelection();
            if(SS.GW.gs.entities == 0) {
                Error(_("No entities are selected. Select entities before "
                        "trying to toggle their construction state."));
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
            break;
        }

        case Command::SPLIT_CURVES:
            SS.GW.SplitLinesOrCurves();
            break;

        default: ssassert(false, "Unexpected menu ID");
    }
}

void GraphicsWindow::ClearSuper() {
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

    // If the mesh or edges were previously hidden, they haven't been generated,
    // and if we are going to show them, we need to generate them first.
    Group *g = SK.GetGroup(SS.GW.activeGroup);
    if(*v && (g->displayOutlines.l.n == 0 && (v == &showEdges || v == &showOutlines))) {
        SS.GenerateAll(SolveSpaceUI::Generate::UNTIL_ACTIVE);
    }

    SS.GW.persistentDirty = true;
    InvalidateGraphics();
    SS.ScheduleShowTW();
}

bool GraphicsWindow::SuggestLineConstraint(hRequest request, Constraint::Type *type) {
    if(LockedInWorkplane()) {
        Entity *ptA = SK.GetEntity(request.entity(1)),
               *ptB = SK.GetEntity(request.entity(2));

        Expr *au, *av, *bu, *bv;

        ptA->PointGetExprsInWorkplane(ActiveWorkplane(), &au, &av);
        ptB->PointGetExprsInWorkplane(ActiveWorkplane(), &bu, &bv);

        double du = au->Minus(bu)->Eval();
        double dv = av->Minus(bv)->Eval();

        const double TOLERANCE_RATIO = 0.02;
        if(fabs(dv) > LENGTH_EPS && fabs(du / dv) < TOLERANCE_RATIO) {
            *type = Constraint::Type::VERTICAL;
            return true;
        } else if(fabs(du) > LENGTH_EPS && fabs(dv / du) < TOLERANCE_RATIO) {
            *type = Constraint::Type::HORIZONTAL;
            return true;
        }
    }
    return false;
}
