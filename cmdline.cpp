#include "solvespace.h"
#include <stdarg.h>

#define COLOR_BG_HEADER     RGB(50, 20, 50)
const TextWindow::Color TextWindow::colors[] = {
    { RGB(255, 255, 255),       COLOR_BG_DEFAULT,   },  // 0

    { RGB(170,   0,   0),       COLOR_BG_HEADER,    },  // 1    hidden label
    { RGB( 40, 255,  40),       COLOR_BG_HEADER,    },  // 2    shown label
    { RGB(200, 200,   0),       COLOR_BG_HEADER,    },  // 3    mixed label
    { RGB(255, 200,  40),       COLOR_BG_HEADER,    },  // 4    header text
    { RGB(  0,   0,   0),       COLOR_BG_DEFAULT,   },  // 5
    { RGB(  0,   0,   0),       COLOR_BG_DEFAULT,   },  // 6
    { RGB(  0,   0,   0),       COLOR_BG_DEFAULT,   },  // 7

    { RGB(255, 255, 255),       COLOR_BG_DEFAULT,   },  // 8    title
    { RGB(100, 100, 255),       COLOR_BG_DEFAULT,   },  // 9    link
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
            meta[i][j].color = COLOR_DEFAULT;
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

    int color = COLOR_DEFAULT;
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
                    color = COLOR_DEFAULT;
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
                        color = *fmt - '0';
                    }
                    if(color < 0 || color >= arraylen(colors)) color = 0;
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
                shown->screen = SCREEN_GROUP_LIST;
                // fall through
            case SCREEN_GROUP_LIST:
                ShowGroupList();
                break;

            case SCREEN_REQUEST_LIST:
                ShowRequestList();
                break;
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

void TextWindow::ScreenNavigaton(int link, DWORD v) {
    switch(link) {
        default:
        case 'h':
            SS.TW.OneScreenForward();
            SS.TW.shown->screen = SCREEN_GROUP_LIST;
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

    SS.GW.EnsureValidActiveGroup();

    if(SS.GW.pendingDescription) {
        Printf("");
    } else {
        // Navigation buttons
        Printf(" %Lb%f<<%E   %Lh%fhome%E   group:%s",
            (DWORD)(&TextWindow::ScreenNavigaton),
            (DWORD)(&TextWindow::ScreenNavigaton),
            SS.group.FindById(SS.GW.activeGroup)->DescriptionString());
    }

    int datumColor;
    if(SS.GW.show2dCsyss && SS.GW.showAxes && SS.GW.showPoints) {
        datumColor = COLOR_MEANS_SHOWN;
    } else if(!(SS.GW.show2dCsyss || SS.GW.showAxes || SS.GW.showPoints)) {
        datumColor = COLOR_MEANS_HIDDEN;
    } else {
        datumColor = COLOR_MEANS_MIXED;
    }

#define hs(b) ((b) ? COLOR_MEANS_SHOWN : COLOR_MEANS_HIDDEN)
    Printf("%C4show: "
           "%Cp%Ll%D%f2d-csys%E%C4  "
           "%Cp%Ll%D%faxes%E%C4  "
           "%Cp%Ll%D%fpoints%E%C4  "
           "%Cp%Ll%fany-datum%E%C4",
        hs(SS.GW.show2dCsyss), (DWORD)&(SS.GW.show2dCsyss), &(SS.GW.ToggleBool),
        hs(SS.GW.showAxes),    (DWORD)&(SS.GW.showAxes),    &(SS.GW.ToggleBool),
        hs(SS.GW.showPoints),  (DWORD)&(SS.GW.showPoints),  &(SS.GW.ToggleBool),
        datumColor, &(SS.GW.ToggleAnyDatumShown)
    );
    Printf("%C4      "
           "%Cp%Ll%D%fall-groups%E%C4  "
           "%Cp%Ll%D%fconstraints%E%C4",
        hs(SS.GW.showAllGroups),   (DWORD)(&SS.GW.showAllGroups),
            &(SS.GW.ToggleBool),
        hs(SS.GW.showConstraints), (DWORD)(&SS.GW.showConstraints),
            &(SS.GW.ToggleBool)
    );
}

void TextWindow::ShowGroupList(void) {
    Printf("%C8[[click group to view requests]]%E");
    int i;
    for(i = 0; i <= SS.group.elems; i++) {
        DWORD v;
        char *s;
        if(i == SS.group.elems) {
            s = "all requests from all groups";
            v = 0;
        } else {
            Group *g = &(SS.group.elem[i].t);
            s = g->DescriptionString();
            v = g->h.v;
        }
        Printf("  %C9%Ll%D%f%s%E", v, (DWORD)(&TextWindow::ScreenSelectGroup), s);
    }
}
void TextWindow::ScreenSelectGroup(int link, DWORD v) {
    SS.TW.OneScreenForward();

    SS.TW.shown->screen = SCREEN_REQUEST_LIST;
    SS.TW.shown->group.v = v;

    SS.TW.Show();
}

void TextWindow::ShowRequestList(void) {
    if(shown->group.v == 0) {
        Printf("%C8[[requests in all groups]]%E");
    } else {
        Group *g = SS.group.FindById(shown->group);
        Printf("%C8[[requests in group %s]]%E", g->DescriptionString());
    }

    int i;
    for(i = 0; i < SS.request.elems; i++) {
        Request *r = &(SS.request.elem[i].t);

        if(r->group.v == shown->group.v || shown->group.v == 0) {
            char *s = r->DescriptionString();
            Printf("  %s", s);
        }
    }
}

