#include <stdarg.h>

#include "solvespace.h"

const GraphicsWindow::MenuEntry GraphicsWindow::menu[] = {
    { 0, "&File",                           0,                          NULL },
    { 1, "&New\tCtrl+N",                    0,                          NULL },
    { 1, "&Open...\tCtrl+O",                0,                          NULL },
    { 1, "&Save\tCtrl+S",                   0,                          NULL },
    { 1, "Save &As...",                     0,                          NULL },
    { 1, NULL,                              0,                          NULL },
    { 1, "E&xit",                           0,                          NULL },

    { 0, "&Edit",                           0,                          NULL },
    { 1, "&Undo\tCtrl+Z",                   0,                          NULL },
    { 1, "&Redo\tCtrl+Y",                   0,                          NULL },
    { 0, "&View",                           0,                          NULL },
    { 1, "Zoom &In\t+",                     0,                          NULL },
    { 1, "Zoom &Out\t-",                    0,                          NULL },
    { 1, "Zoom To &Fit\tF",                 0,                          NULL },
    { 1, NULL,                              0,                          NULL },
    { 1, "Dimensions in &Inches",           0,                          NULL },
    { 1, "Dimensions in &Millimeters",      0,                          NULL },

    { 0, "&Sketch",                         0,                          NULL },
    { 1, NULL,                              0,                          NULL },
    { 1, "To&ggle Construction\tG",         0,                          NULL },

    { 0, "&Constrain",                      0,                          NULL },
    { 1, "S&ymmetric\tY",                   0,                          NULL },

    { 0, "&Help",                           0,                          NULL },
    { 1, "&About\t",                        0,                          NULL },
    { -1 },
};

void GraphicsWindow::Init(void) {
    memset(this, 0, sizeof(*this));

    offset.x = offset.y = offset.z = 0;
    scale = 1;
    projRight.x = 1; projRight.y = projRight.z = 0;
    projDown.y = 1; projDown.z = projDown.x = 0;

    show2dCsyss = true;
    showAxes = true;
    showPoints = true;
    showAllGroups = true;
    showConstraints = true;
}

void GraphicsWindow::NormalizeProjectionVectors(void) {
    Vector norm = projRight.Cross(projDown);
    projDown = norm.Cross(projRight);

    projDown = projDown.ScaledBy(1/projDown.Magnitude());
    projRight = projRight.ScaledBy(1/projRight.Magnitude());
}

Point2d GraphicsWindow::ProjectPoint(Vector p) {
    p = p.Plus(offset);

    Point2d r;
    r.x = p.Dot(projRight) * scale;
    r.y = p.Dot(projDown) * scale;
    return r;
}

void GraphicsWindow::MouseMoved(double x, double y, bool leftDown,
            bool middleDown, bool rightDown, bool shiftDown, bool ctrlDown)
{
    Point2d mp = { x, y };

    if(middleDown) {
        hover.Clear();

        double dx = (x - orig.mouse.x) / scale;
        double dy = (y - orig.mouse.y) / scale;

        if(shiftDown) {
            offset.x = orig.offset.x + dx*projRight.x + dy*projDown.x;
            offset.y = orig.offset.y + dx*projRight.y + dy*projDown.y;
            offset.z = orig.offset.z + dx*projRight.z + dy*projDown.z;
        } else if(ctrlDown) {
            double theta = atan2(orig.mouse.y, orig.mouse.x);
            theta -= atan2(y, x);

            Vector normal = orig.projRight.Cross(orig.projDown);
            projRight = orig.projRight.RotatedAbout(normal, theta);
            projDown = orig.projDown.RotatedAbout(normal, theta);

            NormalizeProjectionVectors();
        } else {
            double s = 0.3*(PI/180); // degrees per pixel
            projRight = orig.projRight.RotatedAbout(orig.projDown, -s*dx);
            projDown = orig.projDown.RotatedAbout(orig.projRight, s*dy);

            NormalizeProjectionVectors();
        }

        orig.projRight = projRight;
        orig.projDown = projDown;
        orig.offset = offset;
        orig.mouse.x = x;
        orig.mouse.y = y;

        InvalidateGraphics();
    } else {
        // No mouse buttons are pressed. We just need to do our usual hit
        // testing, to see if anything ought to be hovered.
        Selection s;
        HitTestMakeSelection(mp, &s);
        if(!s.Equals(&hover)) {
            hover = s;
            InvalidateGraphics();
        }
    }
}

bool GraphicsWindow::Selection::Equals(Selection *b) {
    if(point.v !=  b->point.v)  return false;
    if(entity.v != b->entity.v) return false;
    return true;
}
bool GraphicsWindow::Selection::IsEmpty(void) {
    if(point.v)  return false;
    if(entity.v) return false;
    return true;
}
void GraphicsWindow::Selection::Clear(void) {
    point.v = entity.v = 0;
}
void GraphicsWindow::Selection::Draw(void) {
    if(point.v)  SS.point. FindById(point )->Draw();
    if(entity.v) SS.entity.FindById(entity)->Draw();
}

void GraphicsWindow::HitTestMakeSelection(Point2d mp, Selection *dest) {
    int i;
    double d, dmin = 1e12;

    dest->point.v = 0;
    dest->entity.v = 0;

    for(i = 0; i < SS.entity.elems; i++) {
        d = SS.entity.elem[i].t.GetDistance(mp);
        if(d < 10 && d < dmin) {
            dest->point.v = 0;
            dest->entity = SS.entity.elem[i].t.h;
        }
    }

    for(i = 0; i < SS.point.elems; i++) {
        d = SS.point.elem[i].t.GetDistance(mp);
        if(d < 10 && d < dmin) {
            dest->entity.v = 0;
            dest->point = SS.point.elem[i].t.h;
        }
    }
}

void GraphicsWindow::MouseMiddleDown(double x, double y) {
    orig.offset = offset;
    orig.projDown = projDown;
    orig.projRight = projRight;
    orig.mouse.x = x;
    orig.mouse.y = y;
}

void GraphicsWindow::MouseLeftDown(double x, double y) {
    // Make sure the hover is up to date.
    MouseMoved(x, y, false, false, false, false, false);

    if(!hover.IsEmpty()) {
        int i;
        for(i = 0; i < MAX_SELECTED; i++) {
            if(selection[i].Equals(&hover)) {
                selection[i].Clear();
                goto done;
            }
        }
        for(i = 0; i < MAX_SELECTED; i++) {
            if(selection[i].IsEmpty()) {
                selection[i] = hover;
                goto done;
            }
        }
        // I guess we ran out of slots, oh well.
done:
        InvalidateGraphics();
    }
}

void GraphicsWindow::MouseScroll(double x, double y, int delta) {
    double offsetRight = offset.Dot(projRight);
    double offsetDown = offset.Dot(projDown);

    double righti = x/scale - offsetRight;
    double downi = y/scale - offsetDown;

    if(delta > 0) {
        scale *= 1.3;
    } else {
        scale /= 1.3;
    }

    double rightf = x/scale - offsetRight;
    double downf = y/scale - offsetDown;

    offset = offset.Plus(projRight.ScaledBy(rightf - righti));
    offset = offset.Plus(projDown.ScaledBy(downf - downi));

    InvalidateGraphics();
}

void GraphicsWindow::ToggleBool(int link, DWORD v) {
    bool *vb = (bool *)v;
    *vb = !*vb;

    InvalidateGraphics();
    SS.TW.Show();
}

void GraphicsWindow::ToggleAnyDatumShown(int link, DWORD v) {
    bool t = !(SS.GW.show2dCsyss && SS.GW.showAxes && SS.GW.showPoints);
    SS.GW.show2dCsyss = t;
    SS.GW.showAxes = t;
    SS.GW.showPoints = t;

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
    double ty = projDown.Dot(offset);
    double mat[16];
    MakeMatrix(mat, projRight.x,    projRight.y,    projRight.z,    tx,
                    projDown.x,     projDown.y,     projDown.z,     ty,
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


    int i;

    // First, draw the entire scene.
    glColor3f(1, 1, 1);
    for(i = 0; i < SS.entity.elems; i++) {
        SS.entity.elem[i].t.Draw();
    }
    glColor3f(0, 0.8f, 0);
    for(i = 0; i < SS.point.elems; i++) {
        SS.point.elem[i].t.Draw();
    }

    // Then redraw whatever the mouse is hovering over, highlighted. Have
    // to disable the depth test, so that we can overdraw.
    glDisable(GL_DEPTH_TEST); 
    glColor3f(1, 1, 0);
    hover.Draw();

    // And finally draw the selection, same mechanism.
    glColor3f(1, 0, 0);
    for(i = 0; i < MAX_SELECTED; i++) {
        selection[i].Draw();
    }
}

