#include "solvespace.h"
#include <stdarg.h>

#define COLOR_BG_HEADER     RGB(50, 20, 50)
const TextWindow::Color TextWindow::colors[] = {
    { 'd', RGB(255, 255, 255),       COLOR_BG_DEFAULT,   },  // default
    { 'l', RGB(100, 100, 255),       COLOR_BG_DEFAULT,   },  // link

    // These are for the header
    { 'D', RGB(255, 255, 255),       COLOR_BG_HEADER,    },  // default
    { 'H', RGB(170,   0,   0),       COLOR_BG_HEADER,    },  // hidden
    { 'S', RGB( 40, 255,  40),       COLOR_BG_HEADER,    },  // shown
    { 'M', RGB(200, 200,   0),       COLOR_BG_HEADER,    },  // mixed h/s
    { 'T', RGB(255, 200,  40),       COLOR_BG_HEADER,    },  // title

    { 0, 0, 0 },
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
            meta[i][j].color = 'd';
            meta[i][j].link = NOT_A_LINK;
        }
    }
    rows = 0;
}

void TextWindow::Printf(char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);

    if(rows >= MAX_ROWS) return;

    int r, c;
    r = rows;
    rows++;

    for(c = 0; c < MAX_COLS; c++) {
        text[r][c] = ' ';
        meta[r][c].link = NOT_A_LINK;
    }

    int color = 'd';
    int link = NOT_A_LINK;
    DWORD data = 0;
    LinkFunction *f = NULL;

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
                    color = 'd';
                    link = NOT_A_LINK;
                    data = 0;
                    f = NULL;
                    break;

                case 'C':
                    if(fmt[1] == '\0') goto done;
                    fmt++;
                    if(*fmt == 'p') {
                        color = va_arg(vl, int);
                    } else {
                        color = *fmt;
                    }
                    if(color < 0 || color > 255) color = 0;
                    break;

                case 'L':
                    if(fmt[1] == '\0') goto done;
                    fmt++;
                    link = *fmt;
                    break;

                case 'f':
                    f = va_arg(vl, LinkFunction *);
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
            meta[r][c].color = color;
            meta[r][c].link = link;
            meta[r][c].data = data;
            meta[r][c].f = f;
            c++;
        }

        fmt++;
    }
    while(c < MAX_COLS) {
        meta[r][c].color = color;
        c++;
    }

done:
    va_end(vl);
}

void TextWindow::Show(void) {
    if(!(SS.GW.pendingOperation)) SS.GW.pendingDescription = NULL;

    ShowHeader();

    if(SS.GW.pendingDescription) {
        // A pending operation (that must be completed with the mouse in
        // the graphics window) will preempt our usual display.
        Printf("");
        Printf("%s", SS.GW.pendingDescription);
    } else {
        switch(shown->screen) {
            default:
                shown->screen = SCREEN_LIST_OF_GROUPS;
                // fall through
            case SCREEN_LIST_OF_GROUPS: ShowListOfGroups();     break;
            case SCREEN_GROUP_INFO:     ShowGroupInfo();        break;
            case SCREEN_REQUEST_INFO:   ShowRequestInfo();      break;
        }
    }
    InvalidateText();
}

void TextWindow::OneScreenForward(void) {
    SS.TW.shownIndex++;
    if(SS.TW.shownIndex >= HISTORY_LEN) SS.TW.shownIndex = 0;
    SS.TW.shown = &(SS.TW.showns[SS.TW.shownIndex]);
    history++;
}

void TextWindow::ScreenNavigation(int link, DWORD v) {
    switch(link) {
        default:
        case 'h':
            SS.TW.OneScreenForward();
            SS.TW.shown->screen = SCREEN_LIST_OF_GROUPS;
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
            SS.TW.OneScreenForward();
            break;
    }
    SS.TW.Show();
}
void TextWindow::ShowHeader(void) {
    ClearScreen();

    SS.GW.EnsureValidActives();

    if(SS.GW.pendingDescription) {
        Printf("             %CT group:%s",
            SS.group.FindById(SS.GW.activeGroup)->DescriptionString());
    } else {
        // Navigation buttons
        char *cd;
        if(SS.GW.activeWorkplane.v == Entity::FREE_IN_3D.v) {
            cd = "free in 3d";
        } else {
            cd = SS.GetEntity(SS.GW.activeWorkplane)->DescriptionString();
        }
        Printf(" %Lb%f<<%E   %Lh%fhome%E   %CT workplane:%CD %s",
            (DWORD)(&TextWindow::ScreenNavigation),
            (DWORD)(&TextWindow::ScreenNavigation),
            cd);
    }

    int datumColor;
    if(SS.GW.showWorkplanes && SS.GW.showAxes && SS.GW.showPoints) {
        datumColor = 'S'; // shown
    } else if(!(SS.GW.showWorkplanes || SS.GW.showAxes || SS.GW.showPoints)) {
        datumColor = 'H'; // hidden
    } else {
        datumColor = 'M'; // mixed
    }

#define hs(b) ((b) ? 'S' : 'H')
    Printf("%CTshow: "
           "%Cp%Ll%D%fworkplane%E%CT  "
           "%Cp%Ll%D%faxes%E%CT  "
           "%Cp%Ll%D%fpoints%E%CT  "
           "%Cp%Ll%fany-datum%E%CT",
  hs(SS.GW.showWorkplanes), (DWORD)&(SS.GW.showWorkplanes), &(SS.GW.ToggleBool),
  hs(SS.GW.showAxes),       (DWORD)&(SS.GW.showAxes),       &(SS.GW.ToggleBool),
  hs(SS.GW.showPoints),     (DWORD)&(SS.GW.showPoints),     &(SS.GW.ToggleBool),
        datumColor, &(SS.GW.ToggleAnyDatumShown)
    );
    Printf("%CT      "
           "%Cp%Ll%D%fall-groups%E%CT  "
           "%Cp%Ll%D%fconstraints%E%CT",
        hs(SS.GW.showAllGroups),   (DWORD)(&SS.GW.showAllGroups),
            &(SS.GW.ToggleBool),
        hs(SS.GW.showConstraints), (DWORD)(&SS.GW.showConstraints),
            &(SS.GW.ToggleBool)
    );
}

void TextWindow::ShowListOfGroups(void) {
    Printf("%Cd[[all groups in sketch follow]]%E");
    int i;
    for(i = 0; i < SS.group.n; i++) {
        char *s;
        Group *g = &(SS.group.elem[i]);
        s = g->DescriptionString();
        Printf("  %Cl%Ll%D%f%s%E",
            g->h.v, (DWORD)(&TextWindow::ScreenSelectGroup), s);
    }
}
void TextWindow::ScreenSelectGroup(int link, DWORD v) {
    SS.TW.OneScreenForward();

    SS.TW.shown->screen = SCREEN_GROUP_INFO;
    SS.TW.shown->group.v = v;

    SS.TW.Show();
}

void TextWindow::ScreenSelectRequest(int link, DWORD v) {
    SS.TW.OneScreenForward();

    SS.TW.shown->screen = SCREEN_REQUEST_INFO;
    SS.TW.shown->request.v = v;

    SS.TW.Show();
}
void TextWindow::ShowGroupInfo(void) {
    Group *g = SS.group.FindById(shown->group);
    if(SS.GW.activeGroup.v == shown->group.v) {
        Printf("%Cd[[this is the active group]]");
    } else if(shown->group.v == Group::HGROUP_REFERENCES.v) {
        Printf("%Cd[[this group contains the references]]");
    } else {
        Printf("%Cd[[not active; %Cl%Llactivate this group%E%Cd]]");
    }
    Printf("%Cd[[requests in group %s]]%E", g->DescriptionString());

    int i;
    for(i = 0; i < SS.request.n; i++) {
        Request *r = &(SS.request.elem[i]);

        if(r->group.v == shown->group.v) {
            char *s = r->DescriptionString();
            Printf("  %Cl%Ll%D%f%s%E",
                r->h.v, (DWORD)(&TextWindow::ScreenSelectRequest), s);
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
    Printf("%Cd[[request for %s]]%E", s);
}


