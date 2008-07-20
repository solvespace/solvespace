#include "solvespace.h"

//-----------------------------------------------------------------------------
// A navigation bar that always appears at the top of the window, with a
// link to bring us back home.
//-----------------------------------------------------------------------------
void TextWindow::ScreenHome(int link, DWORD v) {
    SS.TW.GoToScreen(SCREEN_LIST_OF_GROUPS);
}
void TextWindow::ShowHeader(bool withNav) {
    ClearScreen();

    char *cd = SS.GW.LockedInWorkplane() ?
                   SS.GetEntity(SS.GW.ActiveWorkplane())->DescriptionString() :
                   "free in 3d";

    // Navigation buttons
    if(withNav) {
        Printf(false, " %Fl%Lh%fhome%E %Bt%Ft wrkpl:%Fd %s",
                    (&TextWindow::ScreenHome),
                    cd);
    } else {
        Printf(false, "      %Bt%Ft wrkpl:%Fd %s", cd);
    }

#define hs(b) ((b) ? 's' : 'h')
    Printf(false, "%Bt%Ftshow: "
           "%Fp%Ll%D%fwrkpls%E "
           "%Fp%Ll%D%fnormals%E "
           "%Fp%Ll%D%fpoints%E "
           "%Fp%Ll%D%fconstraints%E ",
  hs(SS.GW.showWorkplanes), (DWORD)&(SS.GW.showWorkplanes), &(SS.GW.ToggleBool),
  hs(SS.GW.showNormals),    (DWORD)&(SS.GW.showNormals),    &(SS.GW.ToggleBool),
  hs(SS.GW.showPoints),     (DWORD)&(SS.GW.showPoints),     &(SS.GW.ToggleBool),
hs(SS.GW.showConstraints), (DWORD)(&SS.GW.showConstraints), &(SS.GW.ToggleBool) 
    );
    Printf(false, "%Bt%Ft      "
           "%Fp%Ll%D%fshaded%E "
           "%Fp%Ll%D%ffaces%E "
           "%Fp%Ll%D%fmesh%E "
           "%Fp%Ll%D%fhidden-lines%E",
hs(SS.GW.showShaded),      (DWORD)(&SS.GW.showShaded),      &(SS.GW.ToggleBool),
hs(SS.GW.showFaces),       (DWORD)(&SS.GW.showFaces),       &(SS.GW.ToggleBool),
hs(SS.GW.showMesh),        (DWORD)(&SS.GW.showMesh),        &(SS.GW.ToggleBool),
hs(SS.GW.showHdnLines),    (DWORD)(&SS.GW.showHdnLines),    &(SS.GW.ToggleBool)
    );
}

//-----------------------------------------------------------------------------
// The screen that shows a list of every group in the sketch, with options
// to hide or show them, and to view them in detail. This is our home page.
//-----------------------------------------------------------------------------
void TextWindow::ScreenSelectGroup(int link, DWORD v) {
    SS.TW.GoToScreen(SCREEN_GROUP_INFO);
    SS.TW.shown.group.v = v;
}
void TextWindow::ScreenToggleGroupShown(int link, DWORD v) {
    hGroup hg = { v };
    Group *g = SS.GetGroup(hg);
    g->visible = !(g->visible);
    // If a group was just shown, then it might not have been generated
    // previously, so regenerate.
    SS.GenerateAll();
}
void TextWindow::ScreenShowGroupsSpecial(int link, DWORD v) {
    int i;
    for(i = 0; i < SS.group.n; i++) {
        Group *g = &(SS.group.elem[i]);

        if(link == 's') {
            g->visible = true;
        } else {
            g->visible = false;
        }
    }
}
void TextWindow::ScreenActivateGroup(int link, DWORD v) {
    hGroup hg = { v };
    Group *g = SS.GetGroup(hg);
    g->visible = true;
    SS.GW.activeGroup.v = v;
    SS.GetGroup(SS.GW.activeGroup)->Activate();
    SS.GW.ClearSuper();
}
void TextWindow::ReportHowGroupSolved(hGroup hg) {
    SS.GW.ClearSuper();
    SS.TW.GoToScreen(SCREEN_GROUP_SOLVE_INFO);
    SS.TW.shown.group.v = hg.v;
    SS.later.showTW = true;
}
void TextWindow::ScreenHowGroupSolved(int link, DWORD v) {
    if(SS.GW.activeGroup.v != v) {
        ScreenActivateGroup(link, v);
    }
    SS.TW.GoToScreen(SCREEN_GROUP_SOLVE_INFO);
    SS.TW.shown.group.v = v;
}
void TextWindow::ScreenShowConfiguration(int link, DWORD v) {
    SS.TW.GoToScreen(SCREEN_CONFIGURATION);
}
void TextWindow::ShowListOfGroups(void) {
    Printf(true, "%Ftactv  show  ok  group-name%E");
    int i;
    bool afterActive = false;
    for(i = 0; i < SS.group.n; i++) {
        Group *g = &(SS.group.elem[i]);
        char *s = g->DescriptionString();
        bool active = (g->h.v == SS.GW.activeGroup.v);
        bool shown = g->visible;
        bool ok = (g->solved.how == Group::SOLVED_OKAY);
        bool ref = (g->h.v == Group::HGROUP_REFERENCES.v);
        Printf(false, "%Bp%Fd "
               "%Fp%D%f%s%Ll%s%E%s  "
               "%Fp%D%f%Ll%s%E%Fh%s%E   "
               "%Fp%D%f%s%Ll%s%E  "
               "%Fl%Ll%D%f%s",
            // Alternate between light and dark backgrounds, for readability
            (i & 1) ? 'd' : 'a',
            // Link that activates the group
            active ? 's' : 'h', g->h.v, (&TextWindow::ScreenActivateGroup),
                active ? "yes" : (ref ? "  " : ""),
                active ? "" : (ref ? "" : "no"),
                active ? "" : " ",
            // Link that hides or shows the group
            shown ? 's' : 'h', g->h.v, (&TextWindow::ScreenToggleGroupShown),
                afterActive ? "" :    (shown ? "yes" : "no"),
                afterActive ? " - " : (shown ? "" : " "),
            // Link to the errors, if a problem occured while solving
            ok ? 's' : 'x', g->h.v, (&TextWindow::ScreenHowGroupSolved),
                ok ? "ok" : "",
                ok ? "" : "NO",
            // Link to a screen that gives more details on the group
            g->h.v, (&TextWindow::ScreenSelectGroup), s);

        if(active) afterActive = true;
    }

    Printf(true,  "  %Fl%Ls%fshow all%E / %Fl%Lh%fhide all%E",
        &(TextWindow::ScreenShowGroupsSpecial),
        &(TextWindow::ScreenShowGroupsSpecial));
    Printf(false,  "  %Fl%Ls%fconfiguration%E",
        &(TextWindow::ScreenShowConfiguration));
}


//-----------------------------------------------------------------------------
// The screen that shows information about a specific group, and allows the
// user to edit various things about it.
//-----------------------------------------------------------------------------
void TextWindow::ScreenHoverConstraint(int link, DWORD v) {
    if(!SS.GW.showConstraints) return;

    hConstraint hc = { v };
    Constraint *c = SS.GetConstraint(hc);
    if(c->group.v != SS.GW.activeGroup.v) {
        // Only constraints in the active group are visible
        return;
    }
    SS.GW.hover.Clear();
    SS.GW.hover.constraint = hc;
    SS.GW.hover.emphasized = true;
}
void TextWindow::ScreenHoverRequest(int link, DWORD v) {
    SS.GW.hover.Clear();
    hRequest hr = { v };
    SS.GW.hover.entity = hr.entity(0);
    SS.GW.hover.emphasized = true;
}
void TextWindow::ScreenSelectConstraint(int link, DWORD v) {
    SS.GW.ClearSelection();
    SS.GW.selection[0].constraint.v = v;
}
void TextWindow::ScreenSelectRequest(int link, DWORD v) {
    hRequest hr = { v };
    SS.GW.ClearSelection();
    SS.GW.selection[0].entity = hr.entity(0);
}
void TextWindow::ScreenChangeOneOrTwoSides(int link, DWORD v) {
    SS.UndoRemember();

    Group *g = SS.GetGroup(SS.TW.shown.group);
    if(g->subtype == Group::ONE_SIDED) {
        g->subtype = Group::TWO_SIDED;
    } else if(g->subtype == Group::TWO_SIDED) {
        g->subtype = Group::ONE_SIDED;
    } else oops();
    SS.MarkGroupDirty(g->h);
    SS.GenerateAll();
    SS.GW.ClearSuper();
}
void TextWindow::ScreenChangeSkipFirst(int link, DWORD v) {
    SS.UndoRemember();

    Group *g = SS.GetGroup(SS.TW.shown.group);
    (g->skipFirst) = !(g->skipFirst);
    SS.MarkGroupDirty(g->h);
    SS.GenerateAll();
    SS.GW.ClearSuper();
}
void TextWindow::ScreenChangeMeshCombine(int link, DWORD v) {
    SS.UndoRemember();

    Group *g = SS.GetGroup(SS.TW.shown.group);
    g->meshCombine = v;
    SS.MarkGroupDirty(g->h);
    SS.GenerateAll();
    SS.GW.ClearSuper();
}
void TextWindow::ScreenChangeRightLeftHanded(int link, DWORD v) {
    SS.UndoRemember();

    Group *g = SS.GetGroup(SS.TW.shown.group);
    if(g->subtype == Group::RIGHT_HANDED) {
        g->subtype = Group::LEFT_HANDED;
    } else {
        g->subtype = Group::RIGHT_HANDED;
    }
    SS.MarkGroupDirty(g->h);
    SS.GenerateAll();
    SS.GW.ClearSuper();
}
void TextWindow::ScreenChangeHelixParameter(int link, DWORD v) {
    Group *g = SS.GetGroup(SS.TW.shown.group);
    char str[1024];
    int r;
    if(link == 't') {
        sprintf(str, "%.3f", g->valA);
        SS.TW.edit.meaning = EDIT_HELIX_TURNS;
        r = 12;
    } else if(link == 'p') {
        strcpy(str, SS.MmToString(g->valB));
        SS.TW.edit.meaning = EDIT_HELIX_PITCH;
        r = 14;
    } else if(link == 'r') {
        strcpy(str, SS.MmToString(g->valC));
        SS.TW.edit.meaning = EDIT_HELIX_DRADIUS;
        r = 16;
    } else oops();
    SS.TW.edit.group.v = v;
    ShowTextEditControl(r, 9, str);
}
void TextWindow::ScreenColor(int link, DWORD v) {
    SS.UndoRemember();

    Group *g = SS.GetGroup(SS.TW.shown.group);
    if(v < 0 || v >= SS.MODEL_COLORS) return;
    g->color = SS.modelColor[v];
    SS.MarkGroupDirty(g->h);
    SS.GenerateAll();
    SS.GW.ClearSuper();
}
void TextWindow::ScreenChangeExprA(int link, DWORD v) {
    Group *g = SS.GetGroup(SS.TW.shown.group);

    // There's an extra line for the skipFirst parameter in one-sided groups.
    int r = (g->subtype == Group::ONE_SIDED) ? 15 : 13;

    char str[1024];
    sprintf(str, "%d", (int)g->valA);
    ShowTextEditControl(r, 9, str);
    SS.TW.edit.meaning = EDIT_TIMES_REPEATED;
    SS.TW.edit.group.v = v;
}
void TextWindow::ScreenChangeGroupName(int link, DWORD v) {
    Group *g = SS.GetGroup(SS.TW.shown.group);
    ShowTextEditControl(7, 14, g->DescriptionString()+5);
    SS.TW.edit.meaning = EDIT_GROUP_NAME;
    SS.TW.edit.group.v = v;
}
void TextWindow::ScreenDeleteGroup(int link, DWORD v) {
    SS.UndoRemember();

    hGroup hg = SS.TW.shown.group;
    if(hg.v == SS.GW.activeGroup.v) {
        Error("This group is currently active; activate a different group "
              "before proceeding.");
        return;
    }
    SS.group.RemoveById(SS.TW.shown.group);
    // This is a major change, so let's re-solve everything.
    SS.TW.ClearSuper();
    SS.GW.ClearSuper();
    SS.GenerateAll(0, INT_MAX);
}
void TextWindow::ShowGroupInfo(void) {
    Group *g = SS.group.FindById(shown.group);
    char *s, *s2, *s3;

    if(shown.group.v == Group::HGROUP_REFERENCES.v) {
        Printf(true, "%FtGROUP    %E%s", g->DescriptionString());
    } else {
        Printf(true, "%FtGROUP    %E%s "
                     "[%Fl%Ll%D%frename%E/%Fl%Ll%D%fdel%E]",
            g->DescriptionString(),
            g->h.v, &TextWindow::ScreenChangeGroupName,
            g->h.v, &TextWindow::ScreenDeleteGroup);
    }

    if(g->type == Group::EXTRUDE) {
        s = "EXTRUDE ";
    } else if(g->type == Group::TRANSLATE) {
        s = "TRANSLATE";
        s2 ="REPEAT  ";
        s3 ="START   ";
    } else if(g->type == Group::ROTATE) {
        s = "ROTATE  ";
        s2 ="REPEAT  ";
        s3 ="START   ";
    }

    if(g->type == Group::EXTRUDE || g->type == Group::ROTATE ||
       g->type == Group::TRANSLATE)
    {
        bool one = (g->subtype == Group::ONE_SIDED);
        Printf(true, "%Ft%s%E %Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E", s,
            &TextWindow::ScreenChangeOneOrTwoSides,
            (one ? "" : "one side"), (one ? "one side" : ""),
            &TextWindow::ScreenChangeOneOrTwoSides,
            (!one ? "" : "two sides"), (!one ? "two sides" : ""));
    } 
    
    if(g->type == Group::LATHE) {
        Printf(true, "%FtLATHE");
    }
    
    if(g->type == Group::SWEEP) {
        Printf(true, "%FtSWEEP");
    }
    
    if(g->type == Group::HELICAL_SWEEP) {
        bool rh = (g->subtype == Group::RIGHT_HANDED);
        Printf(true,
            "%FtHELICAL%E  %Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E",
                &ScreenChangeRightLeftHanded,
                (rh ? "" : "right-hand"), (rh ? "right-hand" : ""),
                &ScreenChangeRightLeftHanded,
                (!rh ? "" : "left-hand"), (!rh ? "left-hand" : ""));
        Printf(false, "%FtTHROUGH%E  %@ turns %Fl%Lt%D%f[change]%E",
            g->valA, g->h.v, &ScreenChangeHelixParameter);
        Printf(false, "%FtPITCH%E    %s axially per turn %Fl%Lp%D%f[change]%E",
            SS.MmToString(g->valB), g->h.v, &ScreenChangeHelixParameter);
        Printf(false, "%FtdRADIUS%E  %s radially per turn %Fl%Lr%D%f[change]%E",
            SS.MmToString(g->valC), g->h.v, &ScreenChangeHelixParameter);
    }
    
    if(g->type == Group::ROTATE || g->type == Group::TRANSLATE) {
        bool space;
        if(g->subtype == Group::ONE_SIDED) {
            bool skip = g->skipFirst;
            Printf(true, "%Ft%s%E %Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E",
                s3,
                &ScreenChangeSkipFirst,
                (!skip ? "" : "with original"), (!skip ? "with original" : ""),
                &ScreenChangeSkipFirst,
                (skip ? "":"with copy #1"), (skip ? "with copy #1":""));
            space = false;
        } else {
            space = true;
        }

        int times = (int)(g->valA);
        Printf(space, "%Ft%s%E %d time%s %Fl%Ll%D%f[change]%E",
            s2, times, times == 1 ? "" : "s",
            g->h.v, &TextWindow::ScreenChangeExprA);
    }
    
    if(g->type == Group::IMPORTED) {
        Printf(true, "%FtIMPORT%E  '%s'", g->impFileRel);
    }

    if(g->type == Group::EXTRUDE ||
       g->type == Group::LATHE ||
       g->type == Group::SWEEP ||
       g->type == Group::HELICAL_SWEEP ||
       g->type == Group::IMPORTED)
    {
        bool un   = (g->meshCombine == Group::COMBINE_AS_UNION);
        bool diff = (g->meshCombine == Group::COMBINE_AS_DIFFERENCE);
        bool asy  = (g->meshCombine == Group::COMBINE_AS_ASSEMBLE);
        bool asa  = (g->type == Group::IMPORTED);

        Printf((g->type == Group::HELICAL_SWEEP),
            "%FtMERGE AS%E %Fh%f%D%Ll%s%E%Fs%s%E / %Fh%f%D%Ll%s%E%Fs%s%E %s "
            "%Fh%f%D%Ll%s%E%Fs%s%E",
            &TextWindow::ScreenChangeMeshCombine,
            Group::COMBINE_AS_UNION,
            (un ? "" : "union"), (un ? "union" : ""),
            &TextWindow::ScreenChangeMeshCombine,
            Group::COMBINE_AS_DIFFERENCE,
            (diff ? "" : "difference"), (diff ? "difference" : ""),
            asa ? "/" : "",
            &TextWindow::ScreenChangeMeshCombine,
            Group::COMBINE_AS_ASSEMBLE,
            (asy || !asa ? "" : "assemble"), (asy && asa ? "assemble" : ""));
    }
    if(g->type == Group::IMPORTED && g->meshError.yes) {
        Printf(false, "%Fx         the parts interfere!");
    }

    if(g->type == Group::EXTRUDE ||
       g->type == Group::LATHE ||
       g->type == Group::SWEEP ||
       g->type == Group::HELICAL_SWEEP)
    {
#define TWOX(v) v v
        Printf(true, "%FtM_COLOR%E  " TWOX(TWOX(TWOX("%Bp%D%f%Ln  %Bd%E  "))),
            0x80000000 | SS.modelColor[0], 0, &TextWindow::ScreenColor,
            0x80000000 | SS.modelColor[1], 1, &TextWindow::ScreenColor,
            0x80000000 | SS.modelColor[2], 2, &TextWindow::ScreenColor,
            0x80000000 | SS.modelColor[3], 3, &TextWindow::ScreenColor,
            0x80000000 | SS.modelColor[4], 4, &TextWindow::ScreenColor,
            0x80000000 | SS.modelColor[5], 5, &TextWindow::ScreenColor,
            0x80000000 | SS.modelColor[6], 6, &TextWindow::ScreenColor,
            0x80000000 | SS.modelColor[7], 7, &TextWindow::ScreenColor);
    }

    // Leave more space if the group has configuration stuff above the req/
    // constraint list (as all but the drawing groups do).
    if(g->type == Group::DRAWING_3D || g->type == Group::DRAWING_WORKPLANE) {
        Printf(true, "%Ftrequests in group");
    } else {
        Printf(false, "");
        Printf(false, "%Ftrequests in group");
    }

    int i, a = 0;
    for(i = 0; i < SS.request.n; i++) {
        Request *r = &(SS.request.elem[i]);

        if(r->group.v == shown.group.v) {
            char *s = r->DescriptionString();
            Printf(false, "%Bp   %Fl%Ll%D%f%h%s%E",
                (a & 1) ? 'd' : 'a',
                r->h.v, (&TextWindow::ScreenSelectRequest),
                &(TextWindow::ScreenHoverRequest), s);
            a++;
        }
    }
    if(a == 0) Printf(false, "%Ba   (none)");

    a = 0;
    Printf(true, "%Ftconstraints in group (%d DOF)", g->solved.dof);
    for(i = 0; i < SS.constraint.n; i++) {
        Constraint *c = &(SS.constraint.elem[i]);

        if(c->group.v == shown.group.v) {
            char *s = c->DescriptionString();
            Printf(false, "%Bp   %Fl%Ll%D%f%h%s%E %s",
                (a & 1) ? 'd' : 'a',
                c->h.v, (&TextWindow::ScreenSelectConstraint),
                (&TextWindow::ScreenHoverConstraint), s,
                c->reference ? "(ref)" : "");
            a++;
        }
    }
    if(a == 0) Printf(false, "%Ba   (none)");
}

//-----------------------------------------------------------------------------
// The screen that's displayed when the sketch fails to solve. A report of
// what failed, and (if the problem is a singular Jacobian) a list of
// constraints that could be removed to fix it.
//-----------------------------------------------------------------------------
void TextWindow::ShowGroupSolveInfo(void) {
    Group *g = SS.group.FindById(shown.group);
    if(g->solved.how == Group::SOLVED_OKAY) {
        // Go back to the default group info screen
        shown.screen = SCREEN_GROUP_INFO;
        Show();
        return;
    }

    Printf(true, "%FtGROUP   %E%s", g->DescriptionString());
    switch(g->solved.how) {
        case Group::DIDNT_CONVERGE:
            Printf(true, "   %FxSOLVE FAILED!%Fd no convergence");
            break;

        case Group::SINGULAR_JACOBIAN: {
            Printf(true, "%FxSOLVE FAILED!%Fd inconsistent system");
            Printf(true, "remove any one of these to fix it");
            for(int i = 0; i < g->solved.remove.n; i++) {
                hConstraint hc = g->solved.remove.elem[i];
                Constraint *c = SS.constraint.FindByIdNoOops(hc);
                if(!c) continue;

                Printf(false, "%Bp   %Fl%Ll%D%f%h%s%E",
                    (i & 1) ? 'd' : 'a',
                    c->h.v, (&TextWindow::ScreenSelectConstraint),
                    (&TextWindow::ScreenHoverConstraint),
                    c->DescriptionString());
            }
            break;
        }
    }

    Printf(true,  "It may be possible to fix the problem ");
    Printf(false, "by selecting Edit -> Undo.");
}

//-----------------------------------------------------------------------------
// For the configuration screen, setup items that are not specific to the
// file being edited right now.
//-----------------------------------------------------------------------------
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
void TextWindow::ScreenChangeCameraTangent(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.3f", 1000*SS.cameraTangent);
    ShowTextEditControl(43, 3, str);
    SS.TW.edit.meaning = EDIT_CAMERA_TANGENT;
}
void TextWindow::ScreenChangeEdgeColor(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.3f, %.3f, %.3f", 
        REDf(SS.edgeColor), GREENf(SS.edgeColor), BLUEf(SS.edgeColor));

    ShowTextEditControl(49, 3, str);
    SS.TW.edit.meaning = EDIT_EDGE_COLOR;
}
void TextWindow::ScreenChangeExportScale(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.3f", (double)SS.exportScale);

    ShowTextEditControl(55, 3, str);
    SS.TW.edit.meaning = EDIT_EXPORT_SCALE;
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
        SS.GetGroup(SS.GW.activeGroup)->runningMesh.l.n);

    Printf(false, "");
    Printf(false, "%Ft perspective factor (0 for parallel)%E");
    Printf(false, "%Ba   %3 %Fl%Ll%f%D[change]%E",
        SS.cameraTangent*1000,
        &ScreenChangeCameraTangent, 0);

    Printf(false, "");
    Printf(false, "%Ft edge color r,g,b (0,0,0 for no edges)%E");
    Printf(false, "%Ba    %@, %@, %@ %Fl%Ll%f%D[change]%E",
        REDf(SS.edgeColor), GREENf(SS.edgeColor), BLUEf(SS.edgeColor),
        &ScreenChangeEdgeColor, 0);

    Printf(false, "");
    Printf(false, "%Ft export scale factor (1.0=mm, 25.4=inch)");
    Printf(false, "%Ba   %3 %Fl%Ll%f%D[change]%E",
        (double)SS.exportScale,
        &ScreenChangeExportScale, 0);
}

//-----------------------------------------------------------------------------
// When we're stepping a dimension. User specifies the finish value, and
// how many steps to take in between current and finish, re-solving each
// time.
//-----------------------------------------------------------------------------
void TextWindow::ScreenStepDimFinish(int link, DWORD v) {
    SS.TW.edit.meaning = EDIT_STEP_DIM_FINISH;
    char s[1024];
    if(SS.TW.shown.dimIsDistance) {
        strcpy(s, SS.MmToString(SS.TW.shown.dimFinish));
    } else {
        sprintf(s, "%.3f", SS.TW.shown.dimFinish);
    }
    ShowTextEditControl(12, 11, s);
}
void TextWindow::ScreenStepDimSteps(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%d", SS.TW.shown.dimSteps);
    SS.TW.edit.meaning = EDIT_STEP_DIM_STEPS;
    ShowTextEditControl(14, 11, str);
}
void TextWindow::ScreenStepDimGo(int link, DWORD v) {
    hConstraint hc = SS.TW.shown.constraint;
    Constraint *c = SS.constraint.FindByIdNoOops(hc);
    if(c) {
        SS.UndoRemember();
        double start = c->valA, finish = SS.TW.shown.dimFinish;
        int i, n = SS.TW.shown.dimSteps;
        for(i = 1; i <= n; i++) {
            c = SS.GetConstraint(hc);
            c->valA = start + ((finish - start)*i)/n;
            SS.MarkGroupDirty(c->group);
            SS.GenerateAll();
            if(!SS.AllGroupsOkay()) {
                // Failed to solve, so quit
                break;
            }
            PaintGraphics();
        }
    }
    InvalidateGraphics();
    SS.TW.GoToScreen(SCREEN_LIST_OF_GROUPS);
}
void TextWindow::ShowStepDimension(void) {
    Constraint *c = SS.constraint.FindByIdNoOops(shown.constraint);
    if(!c) {
        shown.screen = SCREEN_LIST_OF_GROUPS;
        Show();
        return;
    }

    Printf(true, "%FtSTEP DIMENSION%E %s", c->DescriptionString());

    if(shown.dimIsDistance) {
        Printf(true,  "%Ba  %FtSTART%E    %s", SS.MmToString(c->valA));
        Printf(false, "%Bd  %FtFINISH%E   %s %Fl%Ll%f[change]%E",
            SS.MmToString(shown.dimFinish), &ScreenStepDimFinish);
    } else {
        Printf(true,  "%Ba  %FtSTART%E    %@", c->valA);
        Printf(false, "%Bd  %FtFINISH%E   %@ %Fl%Ll%f[change]%E",
            shown.dimFinish, &ScreenStepDimFinish);
    }
    Printf(false, "%Ba  %FtSTEPS%E    %d %Fl%Ll%f%D[change]%E",
        shown.dimSteps, &ScreenStepDimSteps);

    Printf(true, " %Fl%Ll%fstep dimension now%E", &ScreenStepDimGo);

    Printf(true, "(or %Fl%Ll%fcancel operation%E)", &ScreenHome);
}

//-----------------------------------------------------------------------------
// The edit control is visible, and the user just pressed enter.
//-----------------------------------------------------------------------------
void TextWindow::EditControlDone(char *s) {
    switch(edit.meaning) {
        case EDIT_TIMES_REPEATED: {
            Expr *e = Expr::From(s);
            if(e) {
                SS.UndoRemember();

                double ev = e->Eval();
                if((int)ev < 1) {
                    Error("Can't repeat fewer than 1 time.");
                    break;
                }

                Group *g = SS.GetGroup(edit.group);
                g->valA = ev;

                if(g->type == Group::ROTATE) {
                    int i, c = 0;
                    for(i = 0; i < SS.constraint.n; i++) {
                        if(SS.constraint.elem[i].group.v == g->h.v) c++;
                    }
                    // If the group does not contain any constraints, then
                    // set the numerical guess to space the copies uniformly
                    // over one rotation. Don't touch the guess if we're
                    // already constrained, because that would break
                    // convergence.
                    if(c == 0) {
                        SS.GetParam(g->h.param(3))->val = PI/(2*ev);
                    }
                }

                SS.MarkGroupDirty(g->h);
                SS.later.generateAll = true;
            } else {
                Error("Not a valid number or expression: '%s'", s);
            }
            break;
        }
        case EDIT_GROUP_NAME: {
            char *t;
            bool invalid = false;
            for(t = s; *t; t++) {
                if(!(isalnum(*t) || *t == '-' || *t == '_')) {
                    invalid = true;
                }
            }
            if(invalid || !*s) {
                Error("Invalid characters. Allowed are: A-Z a-z 0-9 _ -");
            } else {
                SS.UndoRemember();

                Group *g = SS.GetGroup(edit.group);
                g->name.strcpy(s);
            }
            SS.unsaved = true;
            break;
        }
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
        case EDIT_CAMERA_TANGENT: {
            SS.cameraTangent = (min(2, max(0, atof(s))))/1000.0;
            InvalidateGraphics();
            break;
        }
        case EDIT_EDGE_COLOR: {
            double r, g, b;
            if(sscanf(s, "%lf, %lf, %lf", &r, &g, &b)==3) {
                SS.edgeColor = RGB(r*255, g*255, b*255);
            } else {
                Error("Bad format: specify color as r, g, b");
            }
            SS.GenerateAll(0, INT_MAX);
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
        case EDIT_HELIX_TURNS:
        case EDIT_HELIX_PITCH:
        case EDIT_HELIX_DRADIUS: {
            SS.UndoRemember();
            Group *g = SS.GetGroup(edit.group);
            Expr *e = Expr::From(s);
            if(!e) {
                Error("Not a valid number or expression: '%s'", s);
                break;
            }
            if(edit.meaning == EDIT_HELIX_TURNS) {
                g->valA = min(30, fabs(e->Eval()));
            } else if(edit.meaning == EDIT_HELIX_PITCH) {
                g->valB = SS.ExprToMm(e);
            } else {
                g->valC = SS.ExprToMm(e);
            }
            SS.MarkGroupDirty(g->h);
            SS.later.generateAll = true;
            break;
        }
        case EDIT_TTF_TEXT: {
            SS.UndoRemember();
            Request *r = SS.request.FindByIdNoOops(edit.request);
            if(r) {
                r->str.strcpy(s);
                SS.MarkGroupDirty(r->group);
                SS.later.generateAll = true;
            }
            break;
        }

        case EDIT_STEP_DIM_FINISH:
            if(shown.dimIsDistance) {
                shown.dimFinish = SS.StringToMm(s);
            } else {
                shown.dimFinish = atof(s);
            }
            break;

        case EDIT_STEP_DIM_STEPS:
            shown.dimSteps = min(300, max(1, atoi(s)));
            break;
    }
    InvalidateGraphics();
    SS.later.showTW = true;
    HideTextEditControl();
    edit.meaning = EDIT_NOTHING;
}

