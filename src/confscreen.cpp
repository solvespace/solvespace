//-----------------------------------------------------------------------------
// For the configuration screen, setup items that are not specific to the
// file being edited right now.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void TextWindow::ScreenChangeLightDirection(int link, uint32_t v) {
    char str[1024];
    sprintf(str, "%.2f, %.2f, %.2f", CO(SS.lightDir[v]));
    SS.TW.ShowEditControl(29+2*v, 8, str);
    SS.TW.edit.meaning = EDIT_LIGHT_DIRECTION;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeLightIntensity(int link, uint32_t v) {
    char str[1024];
    sprintf(str, "%.2f", SS.lightIntensity[v]);
    SS.TW.ShowEditControl(29+2*v, 31, str);
    SS.TW.edit.meaning = EDIT_LIGHT_INTENSITY;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeColor(int link, uint32_t v) {
    SS.TW.ShowEditControlWithColorPicker(9+2*v, 13, SS.modelColor[v]);

    SS.TW.edit.meaning = EDIT_COLOR;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeChordTolerance(int link, uint32_t v) {
    char str[1024];
    sprintf(str, "%.2f", SS.chordTol);
    SS.TW.ShowEditControl(37, 3, str);
    SS.TW.edit.meaning = EDIT_CHORD_TOLERANCE;
}

void TextWindow::ScreenChangeMaxSegments(int link, uint32_t v) {
    char str[1024];
    sprintf(str, "%d", SS.maxSegments);
    SS.TW.ShowEditControl(41, 3, str);
    SS.TW.edit.meaning = EDIT_MAX_SEGMENTS;
}

void TextWindow::ScreenChangeCameraTangent(int link, uint32_t v) {
    char str[1024];
    sprintf(str, "%.3f", 1000*SS.cameraTangent);
    SS.TW.ShowEditControl(47, 3, str);
    SS.TW.edit.meaning = EDIT_CAMERA_TANGENT;
}

void TextWindow::ScreenChangeGridSpacing(int link, uint32_t v) {
    SS.TW.ShowEditControl(51, 3, SS.MmToString(SS.gridSpacing));
    SS.TW.edit.meaning = EDIT_GRID_SPACING;
}

void TextWindow::ScreenChangeDigitsAfterDecimal(int link, uint32_t v) {
    char buf[128];
    sprintf(buf, "%d", SS.UnitDigitsAfterDecimal());
    SS.TW.ShowEditControl(55, 3, buf);
    SS.TW.edit.meaning = EDIT_DIGITS_AFTER_DECIMAL;
}

void TextWindow::ScreenChangeExportScale(int link, uint32_t v) {
    char str[1024];
    sprintf(str, "%.3f", (double)SS.exportScale);

    SS.TW.ShowEditControl(61, 5, str);
    SS.TW.edit.meaning = EDIT_EXPORT_SCALE;
}

void TextWindow::ScreenChangeExportOffset(int link, uint32_t v) {
    SS.TW.ShowEditControl(65, 3, SS.MmToString(SS.exportOffset));
    SS.TW.edit.meaning = EDIT_EXPORT_OFFSET;
}

void TextWindow::ScreenChangeFixExportColors(int link, uint32_t v) {
    SS.fixExportColors = !SS.fixExportColors;
}

void TextWindow::ScreenChangeBackFaces(int link, uint32_t v) {
    SS.drawBackFaces = !SS.drawBackFaces;
    InvalidateGraphics();
}

void TextWindow::ScreenChangeCheckClosedContour(int link, uint32_t v) {
    SS.checkClosedContour = !SS.checkClosedContour;
    InvalidateGraphics();
}

void TextWindow::ScreenChangeShadedTriangles(int link, uint32_t v) {
    SS.exportShadedTriangles = !SS.exportShadedTriangles;
    InvalidateGraphics();
}

void TextWindow::ScreenChangePwlCurves(int link, uint32_t v) {
    SS.exportPwlCurves = !SS.exportPwlCurves;
    InvalidateGraphics();
}

void TextWindow::ScreenChangeCanvasSizeAuto(int link, uint32_t v) {
    if(link == 't') {
        SS.exportCanvasSizeAuto = true;
    } else {
        SS.exportCanvasSizeAuto = false;
    }
    InvalidateGraphics();
}

void TextWindow::ScreenChangeCanvasSize(int link, uint32_t v) {
    double d;
    switch(v) {
        case  0: d = SS.exportMargin.left;      break;
        case  1: d = SS.exportMargin.right;     break;
        case  2: d = SS.exportMargin.bottom;    break;
        case  3: d = SS.exportMargin.top;       break;

        case 10: d = SS.exportCanvas.width;     break;
        case 11: d = SS.exportCanvas.height;    break;
        case 12: d = SS.exportCanvas.dx;        break;
        case 13: d = SS.exportCanvas.dy;        break;

        default: return;
    }
    int row = 81, col;
    if(v < 10) {
        row += v*2;
        col = 11;
    } else {
        row += (v - 10)*2;
        col = 13;
    }
    SS.TW.ShowEditControl(row, col, SS.MmToString(d));
    SS.TW.edit.meaning = EDIT_CANVAS_SIZE;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeGCodeParameter(int link, uint32_t v) {
    char buf[1024] = "";
    int row = 93;
    switch(link) {
        case 'd':
            SS.TW.edit.meaning = EDIT_G_CODE_DEPTH;
            strcpy(buf, SS.MmToString(SS.gCode.depth));
            row += 0;
            break;

        case 's':
            SS.TW.edit.meaning = EDIT_G_CODE_PASSES;
            sprintf(buf, "%d", SS.gCode.passes);
            row += 2;
            break;

        case 'F':
            SS.TW.edit.meaning = EDIT_G_CODE_FEED;
            strcpy(buf, SS.MmToString(SS.gCode.feed));
            row += 4;
            break;

        case 'P':
            SS.TW.edit.meaning = EDIT_G_CODE_PLUNGE_FEED;
            strcpy(buf, SS.MmToString(SS.gCode.plungeFeed));
            row += 6;
            break;
    }
    SS.TW.ShowEditControl(row, 14, buf);
}

void TextWindow::ScreenChangeAutosaveInterval(int link, uint32_t v) {
    char str[1024];
    sprintf(str, "%d", SS.autosaveInterval);

    SS.TW.ShowEditControl(111, 3, str);
    SS.TW.edit.meaning = EDIT_AUTOSAVE_INTERVAL;
}

void TextWindow::ShowConfiguration(void) {
    int i;
    Printf(true, "%Ft user color (r, g, b)");

    for(i = 0; i < SS.MODEL_COLORS; i++) {
        Printf(false, "%Bp   #%d:  %Bz  %Bp  (%@, %@, %@) %f%D%Ll%Fl[change]%E",
            (i & 1) ? 'd' : 'a',
            i, &SS.modelColor[i],
            (i & 1) ? 'd' : 'a',
            SS.modelColor[i].redF(),
            SS.modelColor[i].greenF(),
            SS.modelColor[i].blueF(),
            &ScreenChangeColor, i);
    }

    Printf(false, "");
    Printf(false, "%Ft light direction               intensity");
    for(i = 0; i < 2; i++) {
        Printf(false, "%Bp   #%d  (%2,%2,%2)%Fl%D%f%Ll[c]%E "
                      "%2 %Fl%D%f%Ll[c]%E",
            (i & 1) ? 'd' : 'a', i,
            CO(SS.lightDir[i]), i, &ScreenChangeLightDirection,
            SS.lightIntensity[i], i, &ScreenChangeLightIntensity);
    }

    Printf(false, "");
    Printf(false, "%Ft chord tolerance (in screen pixels)%E");
    Printf(false, "%Ba   %@ %Fl%Ll%f%D[change]%E; now %d triangles",
        SS.chordTol,
        &ScreenChangeChordTolerance, 0,
        SK.GetGroup(SS.GW.activeGroup)->displayMesh.l.n);
    Printf(false, "%Ft max piecewise linear segments%E");
    Printf(false, "%Ba   %d %Fl%Ll%f[change]%E",
        SS.maxSegments,
        &ScreenChangeMaxSegments);

    Printf(false, "");
    Printf(false, "%Ft perspective factor (0 for parallel)%E");
    Printf(false, "%Ba   %# %Fl%Ll%f%D[change]%E",
        SS.cameraTangent*1000,
        &ScreenChangeCameraTangent, 0);
    Printf(false, "%Ft snap grid spacing%E");
    Printf(false, "%Ba   %s %Fl%Ll%f%D[change]%E",
        SS.MmToString(SS.gridSpacing),
        &ScreenChangeGridSpacing, 0);
    Printf(false, "%Ft digits after decimal point to show%E");
    Printf(false, "%Ba   %d %Fl%Ll%f%D[change]%E (e.g. '%s')",
        SS.UnitDigitsAfterDecimal(),
        &ScreenChangeDigitsAfterDecimal, 0,
        SS.MmToString(SS.StringToMm("1.23456789")));

    Printf(false, "");
    Printf(false, "%Ft export scale factor (1:1=mm, 1:25.4=inch)");
    Printf(false, "%Ba   1:%# %Fl%Ll%f%D[change]%E",
        (double)SS.exportScale,
        &ScreenChangeExportScale, 0);
    Printf(false, "%Ft cutter radius offset (0=no offset) ");
    Printf(false, "%Ba   %s %Fl%Ll%f%D[change]%E",
        SS.MmToString(SS.exportOffset),
        &ScreenChangeExportOffset, 0);

    Printf(false, "");
    Printf(false, "  %Fd%f%Ll%s  export shaded 2d triangles%E",
        &ScreenChangeShadedTriangles,
        SS.exportShadedTriangles ? CHECK_TRUE : CHECK_FALSE);
    if(fabs(SS.exportOffset) > LENGTH_EPS) {
        Printf(false, "  %Fd%s  curves as piecewise linear%E "
                      "(since cutter radius is not zero)", CHECK_TRUE);
    } else {
        Printf(false, "  %Fd%f%Ll%s  export curves as piecewise linear%E",
            &ScreenChangePwlCurves,
            SS.exportPwlCurves ? CHECK_TRUE : CHECK_FALSE);
    }
    Printf(false, "  %Fd%f%Ll%s  fix white exported lines%E",
        &ScreenChangeFixExportColors,
        SS.fixExportColors ? CHECK_TRUE : CHECK_FALSE);

    Printf(false, "");
    Printf(false, "%Ft export canvas size:  "
                  "%f%Fd%Lf%s fixed%E  "
                  "%f%Fd%Lt%s auto%E",
        &ScreenChangeCanvasSizeAuto,
        !SS.exportCanvasSizeAuto ? RADIO_TRUE : RADIO_FALSE,
        &ScreenChangeCanvasSizeAuto,
        SS.exportCanvasSizeAuto ? RADIO_TRUE : RADIO_FALSE);

    if(SS.exportCanvasSizeAuto) {
        Printf(false, "%Ft (by margins around exported geometry)");
        Printf(false, "%Ba%Ft   left:   %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportMargin.left), &ScreenChangeCanvasSize, 0);
        Printf(false, "%Bd%Ft   right:  %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportMargin.right), &ScreenChangeCanvasSize, 1);
        Printf(false, "%Ba%Ft   bottom: %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportMargin.bottom), &ScreenChangeCanvasSize, 2);
        Printf(false, "%Bd%Ft   top:    %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportMargin.top), &ScreenChangeCanvasSize, 3);
    } else {
        Printf(false, "%Ft (by absolute dimensions and offsets)");
        Printf(false, "%Ba%Ft   width:    %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportCanvas.width), &ScreenChangeCanvasSize, 10);
        Printf(false, "%Bd%Ft   height:   %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportCanvas.height), &ScreenChangeCanvasSize, 11);
        Printf(false, "%Ba%Ft   offset x: %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportCanvas.dx), &ScreenChangeCanvasSize, 12);
        Printf(false, "%Bd%Ft   offset y: %Fd%s %Fl%Ll%f%D[change]%E",
            SS.MmToString(SS.exportCanvas.dy), &ScreenChangeCanvasSize, 13);
    }

    Printf(false, "");
    Printf(false, "%Ft exported g code parameters");
    Printf(false, "%Ba%Ft   depth:     %Fd%s %Fl%Ld%f[change]%E",
        SS.MmToString(SS.gCode.depth), &ScreenChangeGCodeParameter);
    Printf(false, "%Bd%Ft   passes:    %Fd%d %Fl%Ls%f[change]%E",
        SS.gCode.passes, &ScreenChangeGCodeParameter);
    Printf(false, "%Ba%Ft   feed:      %Fd%s %Fl%LF%f[change]%E",
        SS.MmToString(SS.gCode.feed), &ScreenChangeGCodeParameter);
    Printf(false, "%Bd%Ft   plunge fd: %Fd%s %Fl%LP%f[change]%E",
        SS.MmToString(SS.gCode.plungeFeed), &ScreenChangeGCodeParameter);

    Printf(false, "");
    Printf(false, "  %Fd%f%Ll%s  draw triangle back faces in red%E",
        &ScreenChangeBackFaces,
        SS.drawBackFaces ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Ll%s  check sketch for closed contour%E",
        &ScreenChangeCheckClosedContour,
        SS.checkClosedContour ? CHECK_TRUE : CHECK_FALSE);

    Printf(false, "");
    Printf(false, "%Ft autosave interval (in minutes)%E");
    Printf(false, "%Ba   %d %Fl%Ll%f[change]%E",
        SS.autosaveInterval, &ScreenChangeAutosaveInterval);

    Printf(false, "");
    Printf(false, " %Ftgl vendor   %E%s", glGetString(GL_VENDOR));
    Printf(false, " %Ft   renderer %E%s", glGetString(GL_RENDERER));
    Printf(false, " %Ft   version  %E%s", glGetString(GL_VERSION));
}

bool TextWindow::EditControlDoneForConfiguration(const char *s) {
    switch(edit.meaning) {
        case EDIT_LIGHT_INTENSITY:
            SS.lightIntensity[edit.i] = min(1.0, max(0.0, atof(s)));
            InvalidateGraphics();
            break;

        case EDIT_LIGHT_DIRECTION: {
            double x, y, z;
            if(sscanf(s, "%lf, %lf, %lf", &x, &y, &z)==3) {
                SS.lightDir[edit.i] = Vector::From(x, y, z);
            } else {
                Error("Bad format: specify coordinates as x, y, z");
            }
            InvalidateGraphics();
            break;
        }
        case EDIT_COLOR: {
            Vector rgb;
            if(sscanf(s, "%lf, %lf, %lf", &rgb.x, &rgb.y, &rgb.z)==3) {
                rgb = rgb.ClampWithin(0, 1);
                SS.modelColor[edit.i] = RGBf(rgb.x, rgb.y, rgb.z);
            } else {
                Error("Bad format: specify color as r, g, b");
            }
            break;
        }
        case EDIT_CHORD_TOLERANCE: {
            SS.chordTol = min(10.0, max(0.1, atof(s)));
            SS.GenerateAll(0, INT_MAX);
            break;
        }
        case EDIT_MAX_SEGMENTS: {
            SS.maxSegments = min(1000, max(7, atoi(s)));
            SS.GenerateAll(0, INT_MAX);
            break;
        }
        case EDIT_CAMERA_TANGENT: {
            SS.cameraTangent = (min(2.0, max(0.0, atof(s))))/1000.0;
            if(!SS.usePerspectiveProj) {
                Message("The perspective factor will have no effect until you "
                        "enable View -> Use Perspective Projection.");
            }
            InvalidateGraphics();
            break;
        }
        case EDIT_GRID_SPACING: {
            SS.gridSpacing = (float)min(1e4, max(1e-3, SS.StringToMm(s)));
            InvalidateGraphics();
            break;
        }
        case EDIT_DIGITS_AFTER_DECIMAL: {
            int v = atoi(s);
            if(v < 0 || v > 8) {
                Error("Specify between 0 and 8 digits after the decimal.");
            } else {
                SS.SetUnitDigitsAfterDecimal(v);
            }
            InvalidateGraphics();
            break;
        }
        case EDIT_EXPORT_SCALE: {
            Expr *e = Expr::From(s, true);
            if(e) {
                double ev = e->Eval();
                if(fabs(ev) < 0.001 || isnan(ev)) {
                    Error("Export scale must not be zero!");
                } else {
                    SS.exportScale = (float)ev;
                }
            }
            break;
        }
        case EDIT_EXPORT_OFFSET: {
            Expr *e = Expr::From(s, true);
            if(e) {
                double ev = SS.ExprToMm(e);
                if(isnan(ev) || ev < 0) {
                    Error("Cutter radius offset must not be negative!");
                } else {
                    SS.exportOffset = (float)ev;
                }
            }
            break;
        }
        case EDIT_CANVAS_SIZE: {
            Expr *e = Expr::From(s, true);
            if(!e) {
                break;
            }
            float d = (float)SS.ExprToMm(e);
            switch(edit.i) {
                case  0: SS.exportMargin.left   = d;    break;
                case  1: SS.exportMargin.right  = d;    break;
                case  2: SS.exportMargin.bottom = d;    break;
                case  3: SS.exportMargin.top    = d;    break;

                case 10: SS.exportCanvas.width  = d;    break;
                case 11: SS.exportCanvas.height = d;    break;
                case 12: SS.exportCanvas.dx     = d;    break;
                case 13: SS.exportCanvas.dy     = d;    break;
            }
            break;
        }
        case EDIT_G_CODE_DEPTH: {
            Expr *e = Expr::From(s, true);
            if(e) SS.gCode.depth = (float)SS.ExprToMm(e);
            break;
        }
        case EDIT_G_CODE_PASSES: {
            Expr *e = Expr::From(s, true);
            if(e) SS.gCode.passes = (int)(e->Eval());
            SS.gCode.passes = max(1, min(1000, SS.gCode.passes));
            break;
        }
        case EDIT_G_CODE_FEED: {
            Expr *e = Expr::From(s, true);
            if(e) SS.gCode.feed = (float)SS.ExprToMm(e);
            break;
        }
        case EDIT_G_CODE_PLUNGE_FEED: {
            Expr *e = Expr::From(s, true);
            if(e) SS.gCode.plungeFeed = (float)SS.ExprToMm(e);
            break;
        }
        case EDIT_AUTOSAVE_INTERVAL: {
            int interval;
            if(sscanf(s, "%d", &interval)==1) {
                if(interval >= 1) {
                    SS.autosaveInterval = interval;
                    SetAutosaveTimerFor(interval);
                } else {
                    Error("Bad value: autosave interval should be positive");
                }
            } else {
                Error("Bad format: specify interval in integral minutes");
            }
        }

        default: return false;
    }
    return true;
}

