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
    memset(this, 0, sizeof(*this));
    shown = &(showns[shownIndex]);

    ClearScreen();
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
                case 's': {
                    char *s = va_arg(vl, char *);
                    memcpy(buf, s, min(sizeof(buf), strlen(s)+1));
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
                    if(color < 0 || color > 255) color = 0;
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

void TextWindow::Show(void) {
    if(!(SS.GW.pending.operation)) SS.GW.ClearPending();

    SS.GW.GroupSelection();
#define gs (SS.GW.gs)

    ShowHeader();

    if(SS.GW.pending.description) {
        // A pending operation (that must be completed with the mouse in
        // the graphics window) will preempt our usual display.
        Printf(false, "");
        Printf(false, "%s", SS.GW.pending.description);
    } else {
        switch(shown->screen) {
            default:
                shown->screen = SCREEN_LIST_OF_GROUPS;
                // fall through
            case SCREEN_LIST_OF_GROUPS:     ShowListOfGroups();     break;
            case SCREEN_GROUP_INFO:         ShowGroupInfo();        break;
            case SCREEN_GROUP_SOLVE_INFO:   ShowGroupSolveInfo();   break;
        }
    }
    InvalidateText();
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
void TextWindow::ShowHeader(void) {
    ClearScreen();

    char *cd = (SS.GW.activeWorkplane.v == Entity::FREE_IN_3D.v) ?
               "free in 3d" :
               SS.GetEntity(SS.GW.activeWorkplane)->DescriptionString();

    // Navigation buttons
    if(SS.GW.pending.description) {
        Printf(false, "             %Bt%Ft workplane:%Fd %s", cd);
    } else {
        Printf(false, " %Lb%f<<%E   %Lh%fhome%E   %Bt%Ft workplane:%Fd %s",
                    (&TextWindow::ScreenNavigation),
                    (&TextWindow::ScreenNavigation),
                    cd);
    }

    int datumColor;
    if(SS.GW.showWorkplanes && SS.GW.showNormals && SS.GW.showPoints) {
        datumColor = 's'; // shown
    } else if(!(SS.GW.showWorkplanes || SS.GW.showNormals || SS.GW.showPoints)){
        datumColor = 'h'; // hidden
    } else {
        datumColor = 'm'; // mixed
    }

#define hs(b) ((b) ? 's' : 'h')
    Printf(false, "%Bt%Ftshow: "
           "%Fp%Ll%D%fworkplanes%E "
           "%Fp%Ll%D%fnormals%E "
           "%Fp%Ll%D%fpoints%E "
           "%Fp%Ll%fany-datum%E",
  hs(SS.GW.showWorkplanes), (DWORD)&(SS.GW.showWorkplanes), &(SS.GW.ToggleBool),
  hs(SS.GW.showNormals),    (DWORD)&(SS.GW.showNormals),    &(SS.GW.ToggleBool),
  hs(SS.GW.showPoints),     (DWORD)&(SS.GW.showPoints),     &(SS.GW.ToggleBool),
        datumColor, &(SS.GW.ToggleAnyDatumShown)
    );
    Printf(false, "%Bt%Ft      "
           "%Fp%Ll%D%fconstraints%E "
           "%Fp%Ll%D%fsolids%E "
           "%Fp%Ll%D%fhidden-lines%E",
hs(SS.GW.showConstraints), (DWORD)(&SS.GW.showConstraints), &(SS.GW.ToggleBool),
hs(SS.GW.showSolids),      (DWORD)(&SS.GW.showSolids),      &(SS.GW.ToggleBool),
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
    SS.GW.GeneratePerSolving();
}
void TextWindow::ScreenShowGroupsSpecial(int link, DWORD v) {
    int i;
    bool before = true;
    for(i = 0; i < SS.group.n; i++) {
        Group *g = &(SS.group.elem[i]);

        if(g->h.v == SS.GW.activeGroup.v) {
            before = false;
        } else if(before && link == 's') {
            g->visible = true;
        } else if(!before && link == 'h') {
            g->visible = false;
        }
    }
}
void TextWindow::ScreenActivateGroup(int link, DWORD v) {
    hGroup hg = { v };
    Group *g = SS.GetGroup(hg);
    g->visible = true;
    SS.GW.activeGroup.v = v;
    if(g->type == Group::DRAWING_WORKPLANE) {
        // If we're activating an in-workplane drawing, then activate that
        // workplane too.
        SS.GW.activeWorkplane = g->h.entity(0);
    }
    SS.GW.ClearSuper();
}
void TextWindow::ReportHowGroupSolved(hGroup hg) {
    SS.TW.OneScreenForwardTo(SCREEN_GROUP_SOLVE_INFO);
    SS.TW.shown->group.v = hg.v;
    SS.TW.Show();
}
void TextWindow::ScreenHowGroupSolved(int link, DWORD v) {
    if(SS.GW.activeGroup.v != v) {
        ScreenActivateGroup(link, v);
    }
    SS.TW.OneScreenForwardTo(SCREEN_GROUP_SOLVE_INFO);
    SS.TW.shown->group.v = v;
}
void TextWindow::ShowListOfGroups(void) {
    Printf(true, "%Ftactv  show  ok  group-name%E");
    int i;
    for(i = 0; i < SS.group.n; i++) {
        Group *g = &(SS.group.elem[i]);
        char *s = g->DescriptionString();
        bool active = (g->h.v == SS.GW.activeGroup.v);
        bool shown = g->visible;
        bool ok = (g->solved.how == Group::SOLVED_OKAY);
        bool ref = (g->h.v == Group::HGROUP_REFERENCES.v);
        Printf(false, "%Bp%Fd "
               "%Fp%D%f%s%Ll%s%E%s  "
               "%Fp%D%f%Ll%s%E%s   "
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
                shown ? "yes" : "no",
                shown ? "" : " ",
            // Link to the errors, if a problem occured while solving
            ok ? 's' : 'x', g->h.v, (&TextWindow::ScreenHowGroupSolved),
                ok ? "ok" : "",
                ok ? "" : "NO",
            // Link to a screen that gives more details on the group
            g->h.v, (&TextWindow::ScreenSelectGroup), s);
    }

    Printf(true,  "  %Fl%Ls%fshow all groups before active%E",
        &(TextWindow::ScreenShowGroupsSpecial));
    Printf(false, "  %Fl%Lh%fhide all groups after active%E",
        &(TextWindow::ScreenShowGroupsSpecial));
}


void TextWindow::ScreenHoverConstraint(int link, DWORD v) {
    SS.GW.hover.Clear();
    SS.GW.hover.constraint.v = v;
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
void TextWindow::ScreenChangeExtrudeSides(int link, DWORD v) {
    Group *g = SS.GetGroup(SS.TW.shown->group);
    if(g->subtype == Group::EXTRUDE_ONE_SIDED) {
        g->subtype = Group::EXTRUDE_TWO_SIDED;
    } else if(g->subtype == Group::EXTRUDE_TWO_SIDED) {
        g->subtype = Group::EXTRUDE_ONE_SIDED;
    } else oops();
    SS.GW.GeneratePerSolving();
    SS.GW.ClearSuper();
}
void TextWindow::ScreenChangeMeshCombine(int link, DWORD v) {
    Group *g = SS.GetGroup(SS.TW.shown->group);
    if(g->meshCombine == Group::COMBINE_AS_DIFFERENCE) {
        g->meshCombine = Group::COMBINE_AS_UNION;
    } else if(g->meshCombine == Group::COMBINE_AS_UNION) {
        g->meshCombine = Group::COMBINE_AS_DIFFERENCE;
    } else oops();
    SS.GW.GeneratePerSolving();
    SS.GW.ClearSuper();
}
void TextWindow::ShowGroupInfo(void) {
    Group *g = SS.group.FindById(shown->group);
    char *s;
    if(SS.GW.activeGroup.v == shown->group.v) {
        s = "active ";
    } else if(shown->group.v == Group::HGROUP_REFERENCES.v) {
        s = "special ";
    } else {
        s = "";
    }
    Printf(true, "%Ft%sGROUP   %E%s", s, g->DescriptionString());

    if(g->type == Group::EXTRUDE) {
        bool one = (g->subtype == Group::EXTRUDE_ONE_SIDED);
        Printf(true, "%FtEXTRUDE%E %Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E",
            &TextWindow::ScreenChangeExtrudeSides,
            (one ? "" : "one side"), (one ? "one-side" : ""),
            &TextWindow::ScreenChangeExtrudeSides,
            (!one ? "" : "two sides"), (!one ? "two sides" : ""));

        bool diff = (g->meshCombine == Group::COMBINE_AS_DIFFERENCE);
        Printf(false, "%FtCOMBINE%E %Fh%f%Ll%s%E%Fs%s%E / %Fh%f%Ll%s%E%Fs%s%E",
            &TextWindow::ScreenChangeMeshCombine,
            (!diff ? "" : "as union"), (!diff ? "as union" : ""),
            &TextWindow::ScreenChangeMeshCombine,
            (diff ? "" : "as difference"), (diff ? "as difference" : ""));
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
    Printf(true, "%FtGROUP   %E%s", g->DescriptionString());

    switch(g->solved.how) {
        case Group::SOLVED_OKAY:
            Printf(true, "   %Fsgroup solved okay%E");
            break;

        case Group::DIDNT_CONVERGE:
            Printf(true, "   %FxSOLVE FAILED!%Fd no convergence");
            break;

        case Group::SINGULAR_JACOBIAN: {
            Printf(true, "%FxSOLVE FAILED!%Fd inconsistent system");
            Printf(true, "remove any one of these to fix it");
            for(int i = 0; i < g->solved.remove.n; i++) {
                hConstraint hc = g->solved.remove.elem[i];
                Constraint *c = SS.GetConstraint(hc);
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

void TextWindow::ShowRequestInfo(void) {
    Request *r = SS.GetRequest(shown->request);

    char *s;
    switch(r->type) {
        case Request::WORKPLANE:     s = "workplane";                break;
        case Request::DATUM_POINT:   s = "datum point";              break;
        case Request::LINE_SEGMENT:  s = "line segment";             break;
        default: oops();
    }
    Printf(false, "[[request for %s]]", s);
}

void TextWindow::ShowConstraintInfo(void) {
    Constraint *c = SS.GetConstraint(shown->constraint);

    Printf(false, "[[constraint]]");
}


