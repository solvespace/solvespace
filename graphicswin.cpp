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
{10, "&Open Recent",                        MNU_OPEN_RECENT,    0,      mFile },
{ 1, "&Save\tCtrl+S",                       MNU_SAVE,           'S'|C,  mFile },
{ 1, "Save &As...",                         MNU_SAVE_AS,        0,      mFile },
{ 1,  NULL,                                 0,                  0,      NULL  },
{ 1, "E&xit",                               MNU_EXIT,           0,      mFile },

{ 0, "&Edit",                               0,                          NULL  },
{ 1, "&Undo\tCtrl+Z",                       MNU_UNDO,           'Z'|C,  mEdit },
{ 1, "&Redo\tCtrl+Y",                       MNU_REDO,           'Y'|C,  mEdit },
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
{ 1, "&Drawing in 3d\tShift+Ctrl+D",        MNU_GROUP_3D,      'D'|S|C, mGrp  },
{ 1, "Drawing in Workplane\tShift+Ctrl+W",  MNU_GROUP_WRKPL,   'W'|S|C, mGrp  },
{ 1, NULL,                                  0,                          NULL  },
{ 1, "Step &Translating\tShift+Ctrl+R",     MNU_GROUP_TRANS,    'T'|S|C,mGrp  },
{ 1, "Step &Rotating\tShift+Ctrl+T",        MNU_GROUP_ROT,      'R'|S|C,mGrp  },
{ 1, NULL,                                  0,                  0,      NULL  },
{ 1, "Extrude\tShift+Ctrl+X",               MNU_GROUP_EXTRUDE,  'X'|S|C,mGrp  },
{ 1, "Lathe\tShift+Ctrl+L",                 MNU_GROUP_LATHE,    'L'|S|C,mGrp  },
{ 1, NULL,                                  0,                  0,      NULL  },
{ 1, "Import / Assemble...\tShift+Ctrl+I",  MNU_GROUP_IMPORT,   'I'|S|C,mGrp  },
{11, "Import Recent",                       MNU_GROUP_RECENT,   0,      mGrp  },

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
{ 1, NULL,                                  0,                          NULL  },
{ 1, "To&ggle Construction\tG",             MNU_CONSTRUCTION,   'G',    mReq  },

{ 0, "&Constrain",                          0,                          NULL  },
{ 1, "&Distance / Diameter\tShift+D",       MNU_DISTANCE_DIA,   'D'|S,  mCon  },
{ 1, "A&ngle\tShift+N",                     MNU_ANGLE,          'N'|S,  mCon  },
{ 1, "Other S&upplementary Angle\tShift+U", MNU_OTHER_ANGLE,    'U'|S,  mCon  },
{ 1, "Toggle &Reference Dim\tShift+R",      MNU_REFERENCE,      'R'|S,  mCon  },
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
{ 1, "Same Orient&ation\tShift+A",          MNU_ORIENTED_SAME,  'A'|S,  mCon  },
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
    scale = 5;
    projRight.x = 1; projRight.y = projRight.z = 0;
    projUp.y = 1; projUp.z = projUp.x = 0;

    // And with the latest visible group active, or failing that the first
    // group after the references
    int i;
    for(i = 0; i < SS.group.n; i++) {
        Group *g = &(SS.group.elem[i]);
        if(i == 1 || g->visible) activeGroup = g->h;
    }
    SS.GetGroup(activeGroup)->Activate();

    EnsureValidActives();

    showWorkplanes = true;
    showNormals = true;
    showPoints = true;
    showConstraints = true;
    showHdnLines = false;
    showShaded = true;
    showMesh = false;

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
                                             Point2d *pmax, Point2d *pmin)
{
    Point2d p2 = ProjectPoint(p);
    pmax->x = max(pmax->x, p2.x);
    pmax->y = max(pmax->y, p2.y);
    pmin->x = min(pmin->x, p2.x);
    pmin->y = min(pmin->y, p2.y);
}
void GraphicsWindow::ZoomToFit(void) {
    int i, j;
    Point2d pmax = { -1e12, -1e12 }, pmin = { 1e12, 1e12 };

    HandlePointForZoomToFit(Vector::From(0, 0, 0), &pmax, &pmin);

    for(i = 0; i < SS.entity.n; i++) {
        Entity *e = &(SS.entity.elem[i]);
        if(!e->IsPoint()) continue;
        if(!e->IsVisible()) continue;
        HandlePointForZoomToFit(e->PointGetNum(), &pmax, &pmin);
    }
    Group *g = SS.GetGroup(activeGroup);
    for(i = 0; i < g->mesh.l.n; i++) {
        STriangle *tr = &(g->mesh.l.elem[i]);
        HandlePointForZoomToFit(tr->a, &pmax, &pmin);
        HandlePointForZoomToFit(tr->b, &pmax, &pmin);
        HandlePointForZoomToFit(tr->c, &pmax, &pmin);
    }
    for(i = 0; i < g->poly.l.n; i++) {
        SContour *sc = &(g->poly.l.elem[i]);
        for(j = 0; j < sc->l.n; j++) {
            HandlePointForZoomToFit(sc->l.elem[j].p, &pmax, &pmin);
        }
    }

    pmax = pmax.ScaledBy(1/scale);
    pmin = pmin.ScaledBy(1/scale);
    double xm = (pmax.x + pmin.x)/2, ym = (pmax.y + pmin.y)/2;
    double dx = pmax.x - pmin.x, dy = pmax.y - pmin.y;

    offset = offset.Plus(projRight.ScaledBy(-xm)).Plus(
                         projUp.   ScaledBy(-ym));
   
    if(dx == 0 && dy == 0) {
        scale = 5;
    } else {
        double scalex = 1e12, scaley = 1e12;
        if(dx != 0) scalex = 0.9*width /dx;
        if(dy != 0) scaley = 0.9*height/dy;
        scale = min(100, min(scalex, scaley));
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

char *GraphicsWindow::ToString(double v) {
    static int WhichBuf;
    static char Bufs[8][128];

    WhichBuf++;
    if(WhichBuf >= 8 || WhichBuf < 0) WhichBuf = 0;

    char *s = Bufs[WhichBuf];
    sprintf(s, "%.3f", v);
    return s;
}
double GraphicsWindow::FromString(char *s) {
    return atof(s);
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

