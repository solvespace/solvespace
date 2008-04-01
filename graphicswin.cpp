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
    offset.x = offset.y = offset.z = 0.9;
    scale = 1;
    projRight.x = 1; projRight.y = projRight.z = 0;
    projDown.y = 1; projDown.z = projDown.x = 0;

}

void GraphicsWindow::NormalizeProjectionVectors(void) {
    Vector norm = projRight.Cross(projDown);
    projDown = norm.Cross(projRight);

    projDown = projDown.ScaledBy(1/projDown.Magnitude());
    projRight = projRight.ScaledBy(1/projRight.Magnitude());
}

void GraphicsWindow::MouseMoved(double x, double y, bool leftDown,
            bool middleDown, bool rightDown, bool shiftDown, bool ctrlDown)
{
    if(middleDown) {
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

            orig.projRight = projRight;
            orig.projDown = projDown;
            orig.mouse.x = x;
            orig.mouse.y = y;
        }

        Invalidate();
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

    Invalidate();
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

    glEnable(GL_DEPTH_TEST); 

    glClearIndex((GLfloat)0);
    glClearDepth(1.0); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 

    Entity e;
    e.Draw();
}

