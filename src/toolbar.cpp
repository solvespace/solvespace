//-----------------------------------------------------------------------------
// The toolbar that appears at the top left of the graphics window, where the
// user can select icons with the mouse, to perform operations equivalent to
// selecting a menu item or using a keyboard shortcut.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "generated/icons.h"

static uint8_t SPACER[1];
static const struct {
    uint8_t     *image;
    int          menu;
    const char  *tip;
} Toolbar[] = {
    { Icon_line,            GraphicsWindow::MNU_LINE_SEGMENT,   "Sketch line segment"                               },
    { Icon_rectangle,       GraphicsWindow::MNU_RECTANGLE,      "Sketch rectangle"                                  },
    { Icon_circle,          GraphicsWindow::MNU_CIRCLE,         "Sketch circle"                                     },
    { Icon_arc,             GraphicsWindow::MNU_ARC,            "Sketch arc of a circle"                            },
    { Icon_text,            GraphicsWindow::MNU_TTF_TEXT,       "Sketch curves from text in a TrueType font"        },
    { Icon_tangent_arc,     GraphicsWindow::MNU_TANGENT_ARC,    "Create tangent arc at selected point"              },
    { Icon_bezier,          GraphicsWindow::MNU_CUBIC,          "Sketch cubic Bezier spline"                        },
    { Icon_point,           GraphicsWindow::MNU_DATUM_POINT,    "Sketch datum point"                                },
    { Icon_construction,    GraphicsWindow::MNU_CONSTRUCTION,   "Toggle construction"                               },
    { Icon_trim,            GraphicsWindow::MNU_SPLIT_CURVES,   "Split lines / curves where they intersect"         },
    { SPACER, 0, 0 },

    { Icon_length,          GraphicsWindow::MNU_DISTANCE_DIA,   "Constrain distance / diameter / length"            },
    { Icon_angle,           GraphicsWindow::MNU_ANGLE,          "Constrain angle"                                   },
    { Icon_horiz,           GraphicsWindow::MNU_HORIZONTAL,     "Constrain to be horizontal"                        },
    { Icon_vert,            GraphicsWindow::MNU_VERTICAL,       "Constrain to be vertical"                          },
    { Icon_parallel,        GraphicsWindow::MNU_PARALLEL,       "Constrain to be parallel or tangent"               },
    { Icon_perpendicular,   GraphicsWindow::MNU_PERPENDICULAR,  "Constrain to be perpendicular"                     },
    { Icon_pointonx,        GraphicsWindow::MNU_ON_ENTITY,      "Constrain point on line / curve / plane / point"   },
    { Icon_symmetric,       GraphicsWindow::MNU_SYMMETRIC,      "Constrain symmetric"                               },
    { Icon_equal,           GraphicsWindow::MNU_EQUAL,          "Constrain equal length / radius / angle"           },
    { Icon_same_orientation,GraphicsWindow::MNU_ORIENTED_SAME,  "Constrain normals in same orientation"             },
    { Icon_other_supp,      GraphicsWindow::MNU_OTHER_ANGLE,    "Other supplementary angle"                         },
    { Icon_ref,             GraphicsWindow::MNU_REFERENCE,      "Toggle reference dimension"                        },
    { SPACER, 0, 0 },

    { Icon_extrude,         GraphicsWindow::MNU_GROUP_EXTRUDE,  "New group extruding active sketch"                 },
    { Icon_sketch_in_plane, GraphicsWindow::MNU_GROUP_WRKPL,    "New group in new workplane (thru given entities)"  },
    { Icon_step_rotate,     GraphicsWindow::MNU_GROUP_ROT,      "New group step and repeat rotating"                },
    { Icon_step_translate,  GraphicsWindow::MNU_GROUP_TRANS,    "New group step and repeat translating"             },
    { Icon_sketch_in_3d,    GraphicsWindow::MNU_GROUP_3D,       "New group in 3d"                                   },
    { Icon_assemble,        GraphicsWindow::MNU_GROUP_IMPORT,   "New group importing / assembling file"             },
    { SPACER, 0, 0 },

    { Icon_in3d,            GraphicsWindow::MNU_NEAREST_ISO,    "Nearest isometric view"                            },
    { Icon_ontoworkplane,   GraphicsWindow::MNU_ONTO_WORKPLANE, "Align view to active workplane"                    },
    { NULL, 0, 0 }
};

void GraphicsWindow::ToolbarDraw(void) {
    ToolbarDrawOrHitTest(0, 0, true, NULL);
}

bool GraphicsWindow::ToolbarMouseMoved(int x, int y) {
    x += ((int)width/2);
    y += ((int)height/2);

    int nh = 0;
    bool withinToolbar = ToolbarDrawOrHitTest(x, y, false, &nh);
    if(!withinToolbar) nh = 0;

    if(nh != toolbarTooltipped) {
        // Don't let the tool tip move around if the mouse moves within the
        // same item.
        toolbarMouseX = x;
        toolbarMouseY = y;
        toolbarTooltipped = 0;
    }

    if(nh != toolbarHovered) {
        toolbarHovered = nh;
        SetTimerFor(1000);
        PaintGraphics();
    }
    // So if we moved off the toolbar, then toolbarHovered is now equal to
    // zero, so it doesn't matter if the tool tip timer expires. And if
    // we moved from one item to another, we reset the timer, so also okay.
    return withinToolbar;
}

bool GraphicsWindow::ToolbarMouseDown(int x, int y) {
    x += ((int)width/2);
    y += ((int)height/2);

    int nh = -1;
    bool withinToolbar = ToolbarDrawOrHitTest(x, y, false, &nh);
    // They might have clicked within the toolbar, but not on a button.
    if(withinToolbar && nh >= 0) {
        for(int i = 0; SS.GW.menu[i].level >= 0; i++) {
            if(nh == SS.GW.menu[i].id) {
                (SS.GW.menu[i].fn)((GraphicsWindow::MenuId)SS.GW.menu[i].id);
                break;
            }
        }
    }
    return withinToolbar;
}

bool GraphicsWindow::ToolbarDrawOrHitTest(int mx, int my,
                                          bool paint, int *menuHit)
{
    int i;
    int x = 17, y = (int)(height - 52);

    int fudge = 8;
    int h = 32*15 + 3*16 + fudge;
    int aleft = 0, aright = 66, atop = y+16+fudge/2, abot = y+16-h;

    bool withinToolbar =
        (mx >= aleft && mx <= aright && my <= atop && my >= abot);

    if(!paint && !withinToolbar) {
        // This gets called every MouseMove event, so return quickly.
        return false;
    }

    if(paint) {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glTranslated(-1, -1, 0);
        glScaled(2.0/width, 2.0/height, 0);
        glDisable(GL_LIGHTING);

        double c = 30.0/255;
        glColor4d(c, c, c, 1.0);
        ssglAxisAlignedQuad(aleft, aright, atop, abot);
    }

    struct {
        bool show;
        const char *str;
    } toolTip = { false, NULL };

    bool leftpos = true;
    for(i = 0; Toolbar[i].image; i++) {
        if(Toolbar[i].image == SPACER) {
            if(!leftpos) {
                leftpos = true;
                y -= 32;
                x -= 32;
            }
            y -= 16;

            if(paint) {
                // Draw a separator bar in a slightly different color.
                int divw = 30, divh = 2;
                glColor4d(0.17, 0.17, 0.17, 1);
                x += 16;
                y += 24;
                ssglAxisAlignedQuad(x+divw, x-divw, y+divh, y-divh);
                x -= 16;
                y -= 24;
            }

            continue;
        }

        if(paint) {
            glRasterPos2i(x - 12, y - 12);
            glDrawPixels(24, 24, GL_RGB, GL_UNSIGNED_BYTE, Toolbar[i].image);

            if(toolbarHovered == Toolbar[i].menu ||
               pending.operation == Toolbar[i].menu) {
                // Highlight the hovered or pending item.
                glColor4d(1, 1, 0, 0.3);
                int boxhw = 15;
                ssglAxisAlignedQuad(x+boxhw, x-boxhw, y+boxhw, y-boxhw);
            }

            if(toolbarTooltipped == Toolbar[i].menu) {
                // Display the tool tip for this item; postpone till later
                // so that no one draws over us. Don't need position since
                // that's just wherever the mouse is.
                toolTip.show = true;
                toolTip.str = Toolbar[i].tip;
            }
        } else {
            int boxhw = 16;
            if(mx < (x+boxhw) && mx > (x - boxhw) &&
               my < (y+boxhw) && my > (y - boxhw))
            {
                if(menuHit) *menuHit = Toolbar[i].menu;
            }
        }

        if(leftpos) {
            x += 32;
            leftpos = false;
        } else {
            x -= 32;
            y -= 32;
            leftpos = true;
        }
    }

    if(paint) {
        // Do this last so that nothing can draw over it.
        if(toolTip.show) {
            ssglInitializeBitmapFont();
            char str[1024];
            if(strlen(toolTip.str) >= 200) oops();
            strcpy(str, toolTip.str);

            for(i = 0; SS.GW.menu[i].level >= 0; i++) {
                if(toolbarTooltipped == SS.GW.menu[i].id) {
                    char accelbuf[40];
                    if(MakeAcceleratorLabel(SS.GW.menu[i].accel, accelbuf)) {
                        sprintf(str+strlen(str), " (%s)", accelbuf);
                    }
                    break;
                }
            }

            int tw = (int)strlen(str)*SS.TW.CHAR_WIDTH + 10,
                th = SS.TW.LINE_HEIGHT + 2;

            double ox = toolbarMouseX + 3, oy = toolbarMouseY + 3;
            glLineWidth(1);
            glColor4d(1.0, 1.0, 0.6, 1.0);
            ssglAxisAlignedQuad(ox, ox+tw, oy, oy+th);
            glColor4d(0.0, 0.0, 0.0, 1.0);
            ssglAxisAlignedLineLoop(ox, ox+tw, oy, oy+th);

            glColor4d(0, 0, 0, 1);
            glPushMatrix();
                glTranslated(ox+5, oy+3, 0);
                glScaled(1, -1, 1);
                ssglBitmapText(str, Vector::From(0, 0, 0));
            glPopMatrix();
        }
        ssglDepthRangeLockToFront(false);
    }

    return withinToolbar;
}

void GraphicsWindow::TimerCallback(void) {
    SS.GW.toolbarTooltipped = SS.GW.toolbarHovered;
    PaintGraphics();
}

