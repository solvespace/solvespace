//-----------------------------------------------------------------------------
// The View menu, stuff to snap to certain special views of the model, and to
// display our current view of the model to the user.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void TextWindow::ShowEditView() {
    Printf(true, "%Ft3D VIEW PARAMETERS%E");

    Printf(true,  "%Bd %Ftoverall scale factor%E");
    Printf(false, "%Ba   %# px/%s %Fl%Ll%f[edit]%E",
        SS.GW.scale * SS.MmPerUnit(),
        SS.UnitName(),
        &ScreenChangeViewScale);
    Printf(false, "%Bd   %Fl%Ll%fset to full scale%E",
        &ScreenChangeViewToFullScale);
    Printf(false, "");

    Printf(false, "%Bd %Ftorigin (maps to center of screen)%E");
    Printf(false, "%Ba   (%s, %s, %s) %Fl%Ll%f[edit]%E",
        SS.MmToString(-SS.GW.offset.x).c_str(),
        SS.MmToString(-SS.GW.offset.y).c_str(),
        SS.MmToString(-SS.GW.offset.z).c_str(),
        &ScreenChangeViewOrigin);
    Printf(false, "");

    Vector n = (SS.GW.projRight).Cross(SS.GW.projUp);
    Printf(false, "%Bd %Ftprojection onto screen%E");
    Printf(false, "%Ba   %Ftright%E (%3, %3, %3) %Fl%Ll%f[edit]%E",
        CO(SS.GW.projRight),
        &ScreenChangeViewProjection);
    Printf(false, "%Bd   %Ftup%E    (%3, %3, %3)", CO(SS.GW.projUp));
    Printf(false, "%Ba   %Ftout%E   (%3, %3, %3)", CO(n));
    Printf(false, "");

    Printf(false, "%Ft perspective factor (0 for parallel)%E");
    Printf(false, "%Ba   %# %Fl%Ll%f%D[change]%E",
        SS.cameraTangent*1000,
        &ScreenChangeCameraTangent, 0);

    Printf(false, "");
    Printf(false, "%Ft light direction               intensity");
    for(int i = 0; i < 2; i++) {
        Printf(false, "%Bp   #%d  (%2,%2,%2)%Fl%D%f%Ll[c]%E "
                      "%2 %Fl%D%f%Ll[c]%E",
            (i & 1) ? 'd' : 'a', i,
            CO(SS.lightDir[i]), i, &ScreenChangeLightDirection,
            SS.lightIntensity[i], i, &ScreenChangeLightIntensity);
    }
    Printf(false, "%Ba         ambient lighting     %2 %Fl%f%Ll[c]%E",
        SS.ambientIntensity, &ScreenChangeLightAmbient);

    Printf(false, "");
    Printf(false, "%Ft explode distance%E");
    Printf(false, "%Ba   %s %Fl%Ll%f%D[change]%E",
        SS.MmToString(SS.explodeDistance).c_str(),
        &ScreenChangeExplodeDistance, 0);

}

void TextWindow::ScreenChangeViewScale(int link, uint32_t v) {
    SS.TW.edit.meaning = Edit::VIEW_SCALE;
    SS.TW.ShowEditControl(3, ssprintf("%.3f", SS.GW.scale * SS.MmPerUnit()));
}

void TextWindow::ScreenChangeViewToFullScale(int link, uint32_t v) {
    SS.GW.scale = SS.GW.window->GetPixelDensity() / 25.4;
}

void TextWindow::ScreenChangeViewOrigin(int link, uint32_t v) {
    std::string edit_value =
        ssprintf("%s, %s, %s",
            SS.MmToString(-SS.GW.offset.x, true).c_str(),
            SS.MmToString(-SS.GW.offset.y, true).c_str(),
            SS.MmToString(-SS.GW.offset.z, true).c_str());

    SS.TW.edit.meaning = Edit::VIEW_ORIGIN;
    SS.TW.ShowEditControl(3, edit_value);
}

void TextWindow::ScreenChangeViewProjection(int link, uint32_t v) {
    std::string edit_value =
        ssprintf("%.3f, %.3f, %.3f", CO(SS.GW.projRight));
    SS.TW.edit.meaning = Edit::VIEW_PROJ_RIGHT;
    SS.TW.ShowEditControl(10, edit_value);
}

void TextWindow::ScreenChangeLightDirection(int link, uint32_t v) {
    SS.TW.ShowEditControl(8, ssprintf("%.2f, %.2f, %.2f", CO(SS.lightDir[v])));
    SS.TW.edit.meaning = Edit::LIGHT_DIRECTION;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeLightIntensity(int link, uint32_t v) {
    SS.TW.ShowEditControl(31, ssprintf("%.2f", SS.lightIntensity[v]));
    SS.TW.edit.meaning = Edit::LIGHT_INTENSITY;
    SS.TW.edit.i = v;
}

void TextWindow::ScreenChangeLightAmbient(int link, uint32_t v) {
    SS.TW.ShowEditControl(31, ssprintf("%.2f", SS.ambientIntensity));
    SS.TW.edit.meaning = Edit::LIGHT_AMBIENT;
    SS.TW.edit.i = 0;
}

void TextWindow::ScreenChangeCameraTangent(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, ssprintf("%.3f", 1000*SS.cameraTangent));
    SS.TW.edit.meaning = Edit::CAMERA_TANGENT;
}

void TextWindow::ScreenChangeExplodeDistance(int link, uint32_t v) {
    SS.TW.ShowEditControl(3, SS.MmToString(SS.explodeDistance, true));
    SS.TW.edit.meaning = Edit::EXPLODE_DISTANCE;
}

bool TextWindow::EditControlDoneForView(const std::string &s) {
    switch(edit.meaning) {
        case Edit::VIEW_SCALE: {
            Expr *e = Expr::From(s, /*popUpError=*/true);
            if(e) {
                double v =  e->Eval() / SS.MmPerUnit();
                if(v > LENGTH_EPS) {
                    SS.GW.scale = v;
                } else {
                    Error(_("Scale cannot be zero or negative."));
                }
            }
            break;
        }

        case Edit::VIEW_ORIGIN: {
            Vector pt;
            if(sscanf(s.c_str(), "%lf, %lf, %lf", &pt.x, &pt.y, &pt.z) == 3) {
                pt = pt.ScaledBy(SS.MmPerUnit());
                SS.GW.offset = pt.ScaledBy(-1);
            } else {
                Error(_("Bad format: specify x, y, z"));
            }
            break;
        }

        case Edit::VIEW_PROJ_RIGHT:
        case Edit::VIEW_PROJ_UP: {
            Vector pt;
            if(sscanf(s.c_str(), "%lf, %lf, %lf", &pt.x, &pt.y, &pt.z) != 3) {
                Error(_("Bad format: specify x, y, z"));
                break;
            }
            if(edit.meaning == Edit::VIEW_PROJ_RIGHT) {
                SS.GW.projRight = pt;
                SS.GW.NormalizeProjectionVectors();
                edit.meaning = Edit::VIEW_PROJ_UP;
                HideEditControl();
                ShowEditControl(10, ssprintf("%.3f, %.3f, %.3f", CO(SS.GW.projUp)),
                                editControl.halfRow + 2);
                edit.showAgain = true;
            } else {
                SS.GW.projUp = pt;
                SS.GW.NormalizeProjectionVectors();
            }
            break;
        }

        default:
            return false;
    }
    return true;
}

