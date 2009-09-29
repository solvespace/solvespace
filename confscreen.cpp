//-----------------------------------------------------------------------------
// For the configuration screen, setup items that are not specific to the
// file being edited right now.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void TextWindow::ScreenChangeLightDirection(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.2f, %.2f, %.2f", CO(SS.lightDir[v]));
    ShowTextEditControl(29+2*v, 8, str);
    SS.TW.edit.meaning = EDIT_LIGHT_DIRECTION;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeLightIntensity(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.2f", SS.lightIntensity[v]);
    ShowTextEditControl(29+2*v, 30, str);
    SS.TW.edit.meaning = EDIT_LIGHT_INTENSITY;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeColor(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.2f, %.2f, %.2f",
        REDf(SS.modelColor[v]),
        GREENf(SS.modelColor[v]),
        BLUEf(SS.modelColor[v]));
    ShowTextEditControl(9+2*v, 12, str);
    SS.TW.edit.meaning = EDIT_COLOR;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeChordTolerance(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.2f", SS.chordTol);
    ShowTextEditControl(37, 3, str);
    SS.TW.edit.meaning = EDIT_CHORD_TOLERANCE;
}

void TextWindow::ScreenChangeMaxSegments(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%d", SS.maxSegments);
    ShowTextEditControl(41, 3, str);
    SS.TW.edit.meaning = EDIT_MAX_SEGMENTS;
}

void TextWindow::ScreenChangeCameraTangent(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.3f", 1000*SS.cameraTangent);
    ShowTextEditControl(47, 3, str);
    SS.TW.edit.meaning = EDIT_CAMERA_TANGENT;
}

void TextWindow::ScreenChangeGridSpacing(int link, DWORD v) {
    ShowTextEditControl(51, 3, SS.MmToString(SS.gridSpacing));
    SS.TW.edit.meaning = EDIT_GRID_SPACING;
}

void TextWindow::ScreenChangeExportScale(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.3f", (double)SS.exportScale);

    ShowTextEditControl(57, 3, str);
    SS.TW.edit.meaning = EDIT_EXPORT_SCALE;
}

void TextWindow::ScreenChangeExportOffset(int link, DWORD v) {
    ShowTextEditControl(61, 3, SS.MmToString(SS.exportOffset));
    SS.TW.edit.meaning = EDIT_EXPORT_OFFSET;
}

void TextWindow::ScreenChangeFixExportColors(int link, DWORD v) {
    SS.fixExportColors = !SS.fixExportColors;
}

void TextWindow::ScreenChangeBackFaces(int link, DWORD v) {
    SS.drawBackFaces = !SS.drawBackFaces;
    InvalidateGraphics();
}

void TextWindow::ScreenChangeShadedTriangles(int link, DWORD v) {
    SS.exportShadedTriangles = !SS.exportShadedTriangles;
    InvalidateGraphics();
}

void TextWindow::ScreenChangePwlCurves(int link, DWORD v) {
    SS.exportPwlCurves = !SS.exportPwlCurves;
    InvalidateGraphics();
}

void TextWindow::ScreenChangeCanvasSizeAuto(int link, DWORD v) {
    SS.exportCanvasSizeAuto = !SS.exportCanvasSizeAuto;
    InvalidateGraphics();
}

void TextWindow::ScreenChangeCanvasSize(int link, DWORD v) {
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
    int row = 75, col;
    if(v < 10) {
        row += v*2;
        col = 11;
    } else {
        row += (v - 10)*2;
        col = 13;
    }
    ShowTextEditControl(row, col, SS.MmToString(d));
    SS.TW.edit.meaning = EDIT_CANVAS_SIZE;
    SS.TW.edit.i = v;
}

void TextWindow::ShowConfiguration(void) {
    int i;
    Printf(true, "%Ft material   color-(r, g, b)");
    
    for(i = 0; i < SS.MODEL_COLORS; i++) {
        Printf(false, "%Bp   #%d:  %Bp  %Bp  (%@, %@, %@) %f%D%Ll%Fl[change]%E",
            (i & 1) ? 'd' : 'a',
            i, 0x80000000 | SS.modelColor[i],
            (i & 1) ? 'd' : 'a',
            REDf(SS.modelColor[i]),
            GREENf(SS.modelColor[i]),
            BLUEf(SS.modelColor[i]),
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
    Printf(false, "%Ba   %2 %Fl%Ll%f%D[change]%E; now %d triangles",
        SS.chordTol,
        &ScreenChangeChordTolerance, 0,
        SK.GetGroup(SS.GW.activeGroup)->displayMesh.l.n);
    Printf(false, "%Ft max piecewise linear segments%E");
    Printf(false, "%Ba    %d %Fl%Ll%f[change]%E",
        SS.maxSegments,
        &ScreenChangeMaxSegments);

    Printf(false, "");
    Printf(false, "%Ft perspective factor (0 for parallel)%E");
    Printf(false, "%Ba   %3 %Fl%Ll%f%D[change]%E",
        SS.cameraTangent*1000,
        &ScreenChangeCameraTangent, 0);
    Printf(false, "%Ft snap grid spacing%E");
    Printf(false, "%Ba   %s %Fl%Ll%f%D[change]%E",
        SS.MmToString(SS.gridSpacing),
        &ScreenChangeGridSpacing, 0);

    Printf(false, "");
    Printf(false, "%Ft export scale factor (1.0=mm, 25.4=inch)");
    Printf(false, "%Ba   %3 %Fl%Ll%f%D[change]%E",
        (double)SS.exportScale,
        &ScreenChangeExportScale, 0);
    Printf(false, "%Ft cutter radius offset (0=no offset) ");
    Printf(false, "%Ba   %s %Fl%Ll%f%D[change]%E",
        SS.MmToString(SS.exportOffset),
        &ScreenChangeExportOffset, 0);

    Printf(false, "");
    Printf(false, "%Ft export shaded 2d triangles: "
                  "%Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E",
        &ScreenChangeShadedTriangles,
        (SS.exportShadedTriangles ? "" : "yes"),
        (SS.exportShadedTriangles ? "yes" : ""),
        &ScreenChangeShadedTriangles,
        (!SS.exportShadedTriangles ? "" : "no"),
        (!SS.exportShadedTriangles ? "no" : ""));
    if(fabs(SS.exportOffset) > LENGTH_EPS) {
        Printf(false, "%Ft curves as piecewise linear:%E %Fsyes%Ft "
                       "(since cutter radius is not zero)");
    } else {
        Printf(false, "%Ft curves as piecewise linear: "
                      "%Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E",
            &ScreenChangePwlCurves,
            (SS.exportPwlCurves ? "" : "yes"),
            (SS.exportPwlCurves ? "yes" : ""),
            &ScreenChangePwlCurves,
            (!SS.exportPwlCurves ? "" : "no"),
            (!SS.exportPwlCurves ? "no" : ""));
    }

    Printf(false, "");
    Printf(false, "%Ft export canvas size: "
                  "%Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E",
        &ScreenChangeCanvasSizeAuto,
        (!SS.exportCanvasSizeAuto ? "" : "fixed"),
        (!SS.exportCanvasSizeAuto ? "fixed" : ""),
        &ScreenChangeCanvasSizeAuto,
        (SS.exportCanvasSizeAuto ? "" : "auto"),
        (SS.exportCanvasSizeAuto ? "auto" : ""));
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
    Printf(false, "%Ft fix white exported lines: "
                  "%Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E",
        &ScreenChangeFixExportColors,
        ( SS.fixExportColors ? "" : "yes"), ( SS.fixExportColors ? "yes" : ""),
        &ScreenChangeFixExportColors,
        (!SS.fixExportColors ? "" : "no"),  (!SS.fixExportColors ? "no"  : ""));

    Printf(false, "%Ft draw triangle back faces: "
                  "%Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E",
        &ScreenChangeBackFaces,
        (SS.drawBackFaces ? "" : "yes"), (SS.drawBackFaces ? "yes" : ""),
        &ScreenChangeBackFaces,
        (!SS.drawBackFaces ? "" : "no"), (!SS.drawBackFaces ? "no" : ""));
    
    Printf(false, "");
    Printf(false, " %Ftgl vendor   %E%s", glGetString(GL_VENDOR));
    Printf(false, " %Ft   renderer %E%s", glGetString(GL_RENDERER));
    Printf(false, " %Ft   version  %E%s", glGetString(GL_VERSION));
}

bool TextWindow::EditControlDoneForConfiguration(char *s) {
    switch(edit.meaning) {
        case EDIT_LIGHT_INTENSITY:
            SS.lightIntensity[edit.i] = min(1, max(0, atof(s)));
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
            double r, g, b;
            if(sscanf(s, "%lf, %lf, %lf", &r, &g, &b)==3) {
                SS.modelColor[edit.i] = RGB(r*255, g*255, b*255);
            } else {
                Error("Bad format: specify color as r, g, b");
            }
            break;
        }
        case EDIT_CHORD_TOLERANCE: {
            SS.chordTol = min(10, max(0.1, atof(s)));
            SS.GenerateAll(0, INT_MAX);
            break;
        }
        case EDIT_MAX_SEGMENTS: {
            SS.maxSegments = min(1000, max(7, atoi(s)));
            SS.GenerateAll(0, INT_MAX);
            break;
        }
        case EDIT_CAMERA_TANGENT: {
            SS.cameraTangent = (min(2, max(0, atof(s))))/1000.0;
            if(SS.forceParallelProj) {
                Message("The perspective factor will have no effect until you "
                        "disable View -> Force Parallel Projection.");
            }
            InvalidateGraphics();
            break;
        }
        case EDIT_GRID_SPACING: {
            SS.gridSpacing = (float)min(1e4, max(1e-3, SS.StringToMm(s)));
            InvalidateGraphics();
            break;
        }
        case EDIT_EXPORT_SCALE: {
            Expr *e = Expr::From(s);
            if(e) {
                double ev = e->Eval();
                if(fabs(ev) < 0.001 || isnan(ev)) {
                    Error("Export scale must not be zero!");
                } else {
                    SS.exportScale = (float)ev;
                }
            } else {
                Error("Not a valid number or expression: '%s'", s);
            }
            break;
        }
        case EDIT_EXPORT_OFFSET: {
            Expr *e = Expr::From(s);
            if(e) {
                double ev = SS.ExprToMm(e);
                if(isnan(ev) || ev < 0) {
                    Error("Cutter radius offset must not be negative!");
                } else {
                    SS.exportOffset = (float)ev;
                }
            } else {
                Error("Not a valid number or expression: '%s'", s);
            }
            break;
        }
        case EDIT_CANVAS_SIZE: {
            Expr *e = Expr::From(s);
            if(!e) {
                Error("Not a valid number or expression: '%s'", s);
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

        default: return false;
    }
    return true;
}

