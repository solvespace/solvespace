#include "solvespace.h"
#include <stdarg.h>

const TextWindow::Color TextWindow::fgColors[] = {
    { 'd', RGB(255, 255, 255) },
    { 'l', RGB(100, 100, 255) },
    { 't', RGB(255, 200,   0) },
    { 'h', RGB( 90,  90,  90) },
    { 's', RGB( 40, 255,  40) },
    { 'm', RGB(200, 200,   0) },
    { 'r', RGB(  0,   0,   0) },
    { 'x', RGB(255,  20,  20) },
    { 'i', RGB(  0, 255, 255) },
    { 0, 0 },
};
const TextWindow::Color TextWindow::bgColors[] = {
    { 'd', RGB(  0,   0,   0) },
    { 't', RGB( 34,  15,  15) },
    { 'a', RGB( 20,  20,  20) },
    { 'r', RGB(255, 255, 255) },
    { 0, 0 },
};

void TextWindow::Init(void) {
    ClearSuper();
}

void TextWindow::ClearSuper(void) {
    HideTextEditControl();
    memset(this, 0, sizeof(*this));
    shown = &(showns[shownIndex]);
    ClearScreen();
    Show();
}

void TextWindow::ClearScreen(void) {
    int i, j;
    for(i = 0; i < MAX_ROWS; i++) {
        for(j = 0; j < MAX_COLS; j++) {
            text[i][j] = ' ';
            meta[i][j].fg = 'd';
            meta[i][j].bg = 'd';
            meta[i][j].link = NOT_A_LINK;
        }
        top[i] = i*2;
    }
    rows = 0;
}

void TextWindow::Printf(bool halfLine, char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);

    if(rows >= MAX_ROWS) return;

    int r, c;
    r = rows;
    top[r] = (r == 0) ? 0 : (top[r-1] + (halfLine ? 3 : 2));
    rows++;

    for(c = 0; c < MAX_COLS; c++) {
        text[r][c] = ' ';
        meta[r][c].link = NOT_A_LINK;
    }

    int fg = 'd', bg = 'd';
    int link = NOT_A_LINK;
    DWORD data = 0;
    LinkFunction *f = NULL, *h = NULL;

    c = 0;
    while(*fmt) {
        char buf[1024];

        if(*fmt == '%') {
            fmt++;
            if(*fmt == '\0') goto done;
            strcpy(buf, "");
            switch(*fmt) {
                case 'd': {
                    int v = va_arg(vl, int);
                    sprintf(buf, "%d", v);
                    break;
                }
                case 'x': {
                    DWORD v = va_arg(vl, DWORD);
                    sprintf(buf, "%08x", v);
                    break;
                }
                case '@': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%.2f", v);
                    break;
                }
                case '2': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%s%.2f", v < 0 ? "" : " ", v);
                    break;
                }
                case '3': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%s%.3f", v < 0 ? "" : " ", v);
                    break;
                }
                case 's': {
                    char *s = va_arg(vl, char *);
                    memcpy(buf, s, min(sizeof(buf), strlen(s)+1));
                    break;
                }
                case 'c': {
                    char v = va_arg(vl, char);
                    sprintf(buf, "%c", v);
                    break;
                }
                case 'E':
                    fg = 'd';
                    // leave the background, though
                    link = NOT_A_LINK;
                    data = 0;
                    f = NULL;
                    h = NULL;
                    break;

                case 'F':
                case 'B': {
                    int color;
                    if(fmt[1] == '\0') goto done;
                    if(fmt[1] == 'p') {
                        color = va_arg(vl, int);
                    } else {
                        color = fmt[1];
                    }
                    if((color < 0 || color > 255) && !(color & 0x80000000)) {
                        color = 0;
                    }
                    if(*fmt == 'F') {
                        fg = color;
                    } else {
                        bg = color;
                    }
                    fmt++;
                    break;
                }
                case 'L':
                    if(fmt[1] == '\0') goto done;
                    fmt++;
                    link = *fmt;
                    break;

                case 'f':
                    f = va_arg(vl, LinkFunction *);
                    break;

                case 'h':
                    h = va_arg(vl, LinkFunction *);
                    break;

                case 'D':
                    data = va_arg(vl, DWORD);
                    break;
                    
                case '%':
                    strcpy(buf, "%");
                    break;
            }
        } else {
            buf[0] = *fmt;
            buf[1]= '\0';
        }

        for(unsigned i = 0; i < strlen(buf); i++) {
            if(c >= MAX_COLS) goto done;
            text[r][c] = buf[i];
            meta[r][c].fg = fg;
            meta[r][c].bg = bg;
            meta[r][c].link = link;
            meta[r][c].data = data;
            meta[r][c].f = f;
            meta[r][c].h = h;
            c++;
        }

        fmt++;
    }
    while(c < MAX_COLS) {
        meta[r][c].fg = fg;
        meta[r][c].bg = bg;
        c++;
    }

done:
    va_end(vl);
}

#define gs (SS.GW.gs)
void TextWindow::Show(void) {
    if(!(SS.GW.pending.operation)) SS.GW.ClearPending();

    SS.GW.GroupSelection();

    if(SS.GW.pending.description) {
        // A pending operation (that must be completed with the mouse in
        // the graphics window) will preempt our usual display.
        HideTextEditControl();
        ShowHeader(false);
        Printf(false, "");
        Printf(false, "%s", SS.GW.pending.description);
    } else if(gs.n > 0) {
        HideTextEditControl();
        ShowHeader(false);
        DescribeSelection();
    } else {
        ShowHeader(true);
        switch(shown->screen) {
            default:
                shown->screen = SCREEN_LIST_OF_GROUPS;
                // fall through
            case SCREEN_LIST_OF_GROUPS:     ShowListOfGroups();     break;
            case SCREEN_GROUP_INFO:         ShowGroupInfo();        break;
            case SCREEN_GROUP_SOLVE_INFO:   ShowGroupSolveInfo();   break;
            case SCREEN_CONFIGURATION:      ShowConfiguration();    break;
        }
    }
    InvalidateText();
}

void TextWindow::ScreenUnselectAll(int link, DWORD v) {
    GraphicsWindow::MenuEdit(GraphicsWindow::MNU_UNSELECT_ALL);
}

void TextWindow::DescribeSelection(void) {
    Entity *e;
    Vector p;
    int i;
    Printf(false, "");

    if(gs.n == 1 && (gs.points == 1 || gs.entities == 1)) {
        e = SS.GetEntity(gs.points == 1 ? gs.point[0] : gs.entity[0]);

#define COP  SS.GW.ToString(p.x), SS.GW.ToString(p.y), SS.GW.ToString(p.z)
#define PT_AS_STR "(%Fi%s%E, %Fi%s%E, %Fi%s%E)"
#define PT_AS_NUM "(%Fi%3%E, %Fi%3%E, %Fi%3%E)"
        switch(e->type) {
            case Entity::POINT_IN_3D:
            case Entity::POINT_IN_2D:
            case Entity::POINT_N_TRANS:
            case Entity::POINT_N_ROT_TRANS:
            case Entity::POINT_N_COPY:
            case Entity::POINT_N_ROT_AA:
                p = e->PointGetNum();
                Printf(false, "%FtPOINT%E at " PT_AS_STR, COP);
                break;

            case Entity::NORMAL_IN_3D:
            case Entity::NORMAL_IN_2D:
            case Entity::NORMAL_N_COPY:
            case Entity::NORMAL_N_ROT:
            case Entity::NORMAL_N_ROT_AA: {
                Quaternion q = e->NormalGetNum();
                p = q.RotationN();
                Printf(false, "%FtNORMAL / COORDINATE SYSTEM%E");
                Printf(true, "  basis n = " PT_AS_NUM, CO(p));
                p = q.RotationU();
                Printf(false, "        u = " PT_AS_NUM, CO(p));
                p = q.RotationV();
                Printf(false, "        v = " PT_AS_NUM, CO(p));
                break;
            }
            case Entity::WORKPLANE: {
                p = SS.GetEntity(e->point[0])->PointGetNum();
                Printf(false, "%FtWORKPLANE%E");
                Printf(true, "   origin = " PT_AS_STR, COP);
                Quaternion q = e->Normal()->NormalGetNum();
                p = q.RotationN();
                Printf(true, "   normal = " PT_AS_NUM, CO(p));
                break;
            }
            case Entity::LINE_SEGMENT: {
                Vector p0 = SS.GetEntity(e->point[0])->PointGetNum();
                p = p0;
                Printf(false, "%FtLINE SEGMENT%E");
                Printf(true, "   thru " PT_AS_STR, COP);
                Vector p1 = SS.GetEntity(e->point[1])->PointGetNum();
                p = p1;
                Printf(false, "        " PT_AS_STR, COP);
                Printf(true,  "   len = %Fi%s%E",
                    SS.GW.ToString((p1.Minus(p0).Magnitude())));
                break;
            }
            case Entity::CUBIC:
                p = SS.GetEntity(e->point[0])->PointGetNum();
                Printf(false, "%FtCUBIC BEZIER CURVE%E");
                for(i = 0; i <= 3; i++) {
                    p = SS.GetEntity(e->point[i])->PointGetNum();
                    Printf((i==0), "   p%c = " PT_AS_STR, '0'+i, COP);
                }
                break;
            case Entity::ARC_OF_CIRCLE: {
                Printf(false, "%FtARC OF A CIRCLE%E");
                p = SS.GetEntity(e->point[0])->PointGetNum();
                Printf(true, "     center = " PT_AS_STR, COP);
                p = SS.GetEntity(e->point[1])->PointGetNum();
                Printf(true,"  endpoints = " PT_AS_STR, COP);
                p = SS.GetEntity(e->point[2])->PointGetNum();
                Printf(false,"              " PT_AS_STR, COP);
                double r = e->CircleGetRadiusNum();
                Printf(true, "   diameter =  %Fi%s", SS.GW.ToString(r*2));
                Printf(false, "     radius =  %Fi%s", SS.GW.ToString(r));
                break;
            }
            case Entity::CIRCLE: {
                Printf(false, "%FtCIRCLE%E");
                p = SS.GetEntity(e->point[0])->PointGetNum();
                Printf(true, "     center = " PT_AS_STR, COP);
                double r = e->CircleGetRadiusNum();
                Printf(true, "   diameter =  %Fi%s", SS.GW.ToString(r*2));
                Printf(false, "     radius =  %Fi%s", SS.GW.ToString(r));
                break;
            }
            case Entity::FACE_NORMAL_PT:
            case Entity::FACE_XPROD:
            case Entity::FACE_N_ROT_TRANS:
                Printf(false, "%FtPLANE FACE%E");
                p = e->FaceGetPointNum();
                Printf(true, "   through = " PT_AS_STR, COP);
                p = e->FaceGetNormalNum();
                Printf(false, "    normal = " PT_AS_STR, COP);
                break;

            default:
                Printf(true, "%Ft?? ENTITY%E");
                break;
        }

        Group *g = SS.GetGroup(e->group);
        Printf(false, "");
        Printf(false, "%FtIN GROUP%E      %s", g->DescriptionString());
        if(e->workplane.v == Entity::FREE_IN_3D.v) {
            Printf(false, "%FtNO WORKPLANE (FREE IN 3D)%E"); 
        } else {
            Entity *w = SS.GetEntity(e->workplane);
            Printf(false, "%FtIN WORKPLANE%E  %s", w->DescriptionString());
        }
    } else if(gs.n == 2 && gs.points == 2) {
        Printf(false, "%FtTWO POINTS");
        Vector p0 = SS.GetEntity(gs.point[0])->PointGetNum(); p = p0;
        Printf(true, "   at " PT_AS_STR, COP);
        Vector p1 = SS.GetEntity(gs.point[1])->PointGetNum(); p = p1;
        Printf(false, "      " PT_AS_STR, COP);
        double d = (p1.Minus(p0)).Magnitude();
        Printf(true, "  d = %Fi%s", SS.GW.ToString(d));
    } else {
        Printf(true, "%FtSELECTED:%E %d item%s", gs.n, gs.n == 1 ? "" : "s");
    }

    Printf(true, "%Fl%f%Ll(unselect all)%E", &TextWindow::ScreenUnselectAll);
}

void TextWindow::OneScreenForwardTo(int screen) {
    SS.TW.shownIndex++;
    if(SS.TW.shownIndex >= HISTORY_LEN) SS.TW.shownIndex = 0;
    SS.TW.shown = &(SS.TW.showns[SS.TW.shownIndex]);
    history++;

    if(screen >= 0) shown->screen = screen;
}

void TextWindow::ScreenNavigation(int link, DWORD v) {
    switch(link) {
        default:
        case 'h':
            SS.TW.OneScreenForwardTo(SCREEN_LIST_OF_GROUPS);
            break;

        case 'b':
            if(SS.TW.history > 0) {
                SS.TW.shownIndex--;
                if(SS.TW.shownIndex < 0) SS.TW.shownIndex = (HISTORY_LEN-1);
                SS.TW.shown = &(SS.TW.showns[SS.TW.shownIndex]);
                SS.TW.history--;
            }
            break;

        case 'f':
            SS.TW.OneScreenForwardTo(-1);
            break;
    }
}
void TextWindow::ShowHeader(bool withNav) {
    ClearScreen();

    char *cd = SS.GW.LockedInWorkplane() ?
                   SS.GetEntity(SS.GW.ActiveWorkplane())->DescriptionString() :
                   "free in 3d";

    // Navigation buttons
    if(withNav) {
        Printf(false, " %Lb%f<<%E   %Lh%fhome%E   %Bt%Ft wrkpl:%Fd %s",
                    (&TextWindow::ScreenNavigation),
                    (&TextWindow::ScreenNavigation),
                    cd);
    } else {
        Printf(false, "             %Bt%Ft wrkpl:%Fd %s", cd);
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

void TextWindow::ScreenSelectGroup(int link, DWORD v) {
    SS.TW.OneScreenForwardTo(SCREEN_GROUP_INFO);
    SS.TW.shown->group.v = v;
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
    SS.TW.OneScreenForwardTo(SCREEN_GROUP_SOLVE_INFO);
    SS.TW.shown->group.v = hg.v;
    SS.later.showTW = true;
}
void TextWindow::ScreenHowGroupSolved(int link, DWORD v) {
    if(SS.GW.activeGroup.v != v) {
        ScreenActivateGroup(link, v);
    }
    SS.TW.OneScreenForwardTo(SCREEN_GROUP_SOLVE_INFO);
    SS.TW.shown->group.v = v;
}
void TextWindow::ScreenShowConfiguration(int link, DWORD v) {
    SS.TW.OneScreenForwardTo(SCREEN_CONFIGURATION);
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

    Group *g = SS.GetGroup(SS.TW.shown->group);
    if(g->subtype == Group::ONE_SIDED) {
        g->subtype = Group::TWO_SIDED;
    } else if(g->subtype == Group::TWO_SIDED) {
        g->subtype = Group::ONE_SIDED;
    } else oops();
    SS.MarkGroupDirty(g->h);
    SS.GenerateAll();
    SS.GW.ClearSuper();
}
void TextWindow::ScreenChangeMeshCombine(int link, DWORD v) {
    SS.UndoRemember();

    Group *g = SS.GetGroup(SS.TW.shown->group);
    g->meshCombine = v;
    SS.MarkGroupDirty(g->h);
    SS.GenerateAll();
    SS.GW.ClearSuper();
}
void TextWindow::ScreenColor(int link, DWORD v) {
    SS.UndoRemember();

    Group *g = SS.GetGroup(SS.TW.shown->group);
    if(v < 0 || v >= SS.MODEL_COLORS) return;
    g->color = SS.modelColor[v];
    SS.MarkGroupDirty(g->h);
    SS.GenerateAll();
    SS.GW.ClearSuper();
}
void TextWindow::ScreenChangeExprA(int link, DWORD v) {
    Group *g = SS.GetGroup(SS.TW.shown->group);
    ShowTextEditControl(13, 10, g->exprA->Print());
    SS.TW.edit.meaning = EDIT_TIMES_REPEATED;
    SS.TW.edit.group.v = v;
}
void TextWindow::ScreenChangeGroupName(int link, DWORD v) {
    Group *g = SS.GetGroup(SS.TW.shown->group);
    ShowTextEditControl(7, 14, g->DescriptionString()+5);
    SS.TW.edit.meaning = EDIT_GROUP_NAME;
    SS.TW.edit.group.v = v;
}
void TextWindow::ScreenDeleteGroup(int link, DWORD v) {
    SS.UndoRemember();

    hGroup hg = SS.TW.shown->group;
    if(hg.v == SS.GW.activeGroup.v) {
        Error("This group is currently active; activate a different group "
              "before proceeding.");
        return;
    }
    SS.group.RemoveById(SS.TW.shown->group);
    // This is a major change, so let's re-solve everything.
    SS.TW.ClearSuper();
    SS.GW.ClearSuper();
    SS.GenerateAll(0, INT_MAX);
}
void TextWindow::ShowGroupInfo(void) {
    Group *g = SS.group.FindById(shown->group);
    char *s, *s2;

    if(shown->group.v == Group::HGROUP_REFERENCES.v) {
        Printf(true, "%FtGROUP    %E%s", g->DescriptionString());
    } else {
        Printf(true, "%FtGROUP    %E%s "
                     "[%Fl%Ll%D%frename%E/%Fl%Ll%D%fdel%E]",
            g->DescriptionString(),
            g->h.v, &TextWindow::ScreenChangeGroupName,
            g->h.v, &TextWindow::ScreenDeleteGroup);
    }

    if(g->type == Group::IMPORTED) {
        Printf(true, "%FtIMPORT%E  '%s'", g->impFile);
    }

    if(g->type == Group::EXTRUDE) {
        s = "EXTRUDE ";
    } else if(g->type == Group::TRANSLATE) {
        s = "TRANSLATE";
        s2 ="REPEAT   ";
    } else if(g->type == Group::ROTATE) {
        s = "ROTATE";
        s2 ="REPEAT";
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

    if(g->type == Group::ROTATE || g->type == Group::TRANSLATE) {
        int times = (int)(g->exprA->Eval());
        Printf(true, "%Ft%s%E %d time%s %Fl%Ll%D%f[change]%E",
            s2, times, times == 1 ? "" : "s",
            g->h.v, &TextWindow::ScreenChangeExprA);
    }

    if(g->type == Group::EXTRUDE ||
       g->type == Group::LATHE ||
       g->type == Group::IMPORTED)
    {
        bool un   = (g->meshCombine == Group::COMBINE_AS_UNION);
        bool diff = (g->meshCombine == Group::COMBINE_AS_DIFFERENCE);
        bool asy  = (g->meshCombine == Group::COMBINE_AS_ASSEMBLE);
        bool asa  = (g->type == Group::IMPORTED);

        Printf(false,
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

    if(g->type == Group::EXTRUDE || g->type == Group::LATHE) {
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

    Printf(true, "%Ftrequests in group");

    int i, a = 0;
    for(i = 0; i < SS.request.n; i++) {
        Request *r = &(SS.request.elem[i]);

        if(r->group.v == shown->group.v) {
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
    Printf(true, "%Ftconstraints in group");
    for(i = 0; i < SS.constraint.n; i++) {
        Constraint *c = &(SS.constraint.elem[i]);

        if(c->group.v == shown->group.v) {
            char *s = c->DescriptionString();
            Printf(false, "%Bp   %Fl%Ll%D%f%h%s%E",
                (a & 1) ? 'd' : 'a',
                c->h.v, (&TextWindow::ScreenSelectConstraint),
                (&TextWindow::ScreenHoverConstraint), s);
            a++;
        }
    }
    if(a == 0) Printf(false, "%Ba   (none)");
}

void TextWindow::ShowGroupSolveInfo(void) {
    Group *g = SS.group.FindById(shown->group);
    if(g->solved.how == Group::SOLVED_OKAY) {
        // Go back to the default group info screen
        shown->screen = SCREEN_GROUP_INFO;
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
}

void TextWindow::ScreenChangeLightPosition(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.2f, %.2f, %.2f", CO(SS.lightPos[v]));
    ShowTextEditControl(29+2*v, 8, str);
    SS.TW.edit.meaning = EDIT_LIGHT_POSITION;
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
void TextWindow::ScreenChangeMeshTolerance(int link, DWORD v) {
    char str[1024];
    sprintf(str, "%.2f", SS.meshTol);
    ShowTextEditControl(37, 3, str);
    SS.TW.edit.meaning = EDIT_MESH_TOLERANCE;
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
    Printf(false, "%Ft light position                intensity");
    for(i = 0; i < 2; i++) {
        Printf(false, "%Bp   #%d  (%2,%2,%2)%Fl%D%f%Ll[c]%E "
                      "%2 %Fl%D%f%Ll[c]%E",
            (i & 1) ? 'd' : 'a', i,
            CO(SS.lightPos[i]), i, &ScreenChangeLightPosition,
            SS.lightIntensity[i], i, &ScreenChangeLightIntensity);
    }

    Printf(false, "");
    Printf(false, "%Ft mesh tolerance (smaller is finer)%E");
    Printf(false, "%Ba   %2 %Fl%Ll%f%D[change]%E; now %d triangles",
        SS.meshTol,
        &ScreenChangeMeshTolerance, 0,
        SS.group.elem[SS.group.n-1].mesh.l.n);
}

void TextWindow::EditControlDone(char *s) {
    switch(edit.meaning) {
        case EDIT_TIMES_REPEATED: {
            Expr *e = Expr::From(s);
            if(e) {
                SS.UndoRemember();

                Group *g = SS.GetGroup(edit.group);
                Expr::FreeKeep(&(g->exprA));
                g->exprA = e->DeepCopyKeep();

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
        case EDIT_LIGHT_POSITION: {
            double x, y, z;
            if(sscanf(s, "%lf, %lf, %lf", &x, &y, &z)==3) {
                SS.lightPos[edit.i] = Vector::From(x, y, z);
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
        case EDIT_MESH_TOLERANCE: {
            SS.meshTol = min(10, max(0.1, atof(s)));
            SS.GenerateAll(0, INT_MAX);
            break;
        }
    }
    SS.later.showTW = true;
    HideTextEditControl();
    edit.meaning = EDIT_NOTHING;
}

