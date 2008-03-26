#include "solvespace.h"
#include <stdarg.h>

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

