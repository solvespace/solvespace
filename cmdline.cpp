#include "solvespace.h"
#include <stdarg.h>

const TextWindow::Color TextWindow::colors[] = {
    { COLOR_FG_DEFAULT,         COLOR_BG_DEFAULT,   },  // 0
    { RGB(170,   0,   0),       COLOR_BG_DEFAULT,   },  // 1
    { RGB( 40, 255,  40),       COLOR_BG_DEFAULT,   },  // 2
    { RGB(200, 200,   0),       COLOR_BG_DEFAULT,   },  // 3
    { RGB(255, 200,  40),       COLOR_BG_DEFAULT,   },  // 4
    { RGB(255,  40,  40),       COLOR_BG_DEFAULT,   },  // 5
    { RGB(  0, 255, 255),       COLOR_BG_DEFAULT,   },  // 6
    { RGB(255,   0, 255),       COLOR_BG_DEFAULT,   },  // 7
};

void TextWindow::Init(void) {
    ClearScreen();
    ClearCommand();
}

void TextWindow::ClearScreen(void) {
    int i, j;
    for(i = 0; i < MAX_ROWS; i++) {
        for(j = 0; j < MAX_COLS; j++) {
            text[i][j] = ' ';
            meta[i][j].color = COLOR_NORMAL;
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

    int color = COLOR_NORMAL;
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
                    color = COLOR_NORMAL;
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

done:
    va_end(vl);
}

void TextWindow::ClearCommand(void) {
    int j;
    for(j = 0; j < MAX_COLS; j++) {
        cmd[j] = ' ';
    }
    memcpy(cmd, "+> ", 3);
    cmdLen = 0;
    cmdInsert = 3;
}

void TextWindow::ProcessCommand(char *cmd)
{
    Printf("%C2command:%E '%s' done %C3(green)%E %C5%LaLink%E", cmd);
}

void TextWindow::KeyPressed(int c) {
    if(cmdLen >= MAX_COLS - 10) {
        ClearCommand();
        return;
    }

    if(c == '\n' || c == '\r') {
        cmd[cmdLen+3] = '\0';
        ProcessCommand(cmd+3);

        ClearCommand();
        return;
    } else if(c == 27) {
        ClearCommand();
    } else if(c == 'l' - 'a' + 1) {
        ClearScreen();
    } else if(c == '\b') {
        // backspace, delete from insertion point
        if(cmdInsert <= 3) return;
        memmove(cmd+cmdInsert-1, cmd+cmdInsert, MAX_COLS-cmdInsert);
        cmdLen--;
        cmdInsert--;
    } else {
        cmd[cmdInsert] = c;
        cmdInsert++;
        cmdLen++;
    }
}

void TextWindow::Show(void) {
    ShowRequestList();

    InvalidateText();
}

void TextWindow::ShowHeader(void) {
    ClearScreen();

    int datumColor;
    if(SS.GW.show2dCsyss && SS.GW.showAxes && SS.GW.showPoints) {
        datumColor = COLOR_MEANS_SHOWN;
    } else if(!(SS.GW.show2dCsyss || SS.GW.showAxes || SS.GW.showPoints)) {
        datumColor = COLOR_MEANS_HIDDEN;
    } else {
        datumColor = COLOR_MEANS_MIXED;
    }

#define hs(b) ((b) ? COLOR_MEANS_SHOWN : COLOR_MEANS_HIDDEN)
    Printf("show: "
           "%Cp%Ll%D%f2d-csys%E  "
           "%Cp%Ll%D%faxes%E  "
           "%Cp%Ll%D%fpoints%E  "
           "%Cp%Ll%fany-datum%E",
        hs(SS.GW.show2dCsyss), (DWORD)&(SS.GW.show2dCsyss), &(SS.GW.ToggleBool),
        hs(SS.GW.showAxes),    (DWORD)&(SS.GW.showAxes),    &(SS.GW.ToggleBool),
        hs(SS.GW.showPoints),  (DWORD)&(SS.GW.showPoints),  &(SS.GW.ToggleBool),
        datumColor, &(SS.GW.ToggleAnyDatumShown)
    );
    Printf("      "
           "%Cp%Ll%D%fall-groups%E  "
           "%Cp%Ll%D%fconstraints%E",
        hs(SS.GW.showAllGroups),   (DWORD)(&SS.GW.showAllGroups),
            &(SS.GW.ToggleBool),
        hs(SS.GW.showConstraints), (DWORD)(&SS.GW.showConstraints),
            &(SS.GW.ToggleBool)
    );
}

void TextWindow::ShowGroupList(void) {
    ShowHeader();

    Printf("%C4[[all groups in sketch]]%E");
    int i;
    for(i = 0; i < SS.group.elems; i++) {
        Group *g = &(SS.group.elem[i].t);
        if(g->name.str[0]) {
            Printf("  %s", g->name.str);
        } else {
            Printf("  unnamed");
        }
    }
}

void TextWindow::ShowRequestList(void) {
    ShowHeader();

    Printf("%C4[[all requests in sketch]]%E");
    int i;
    for(i = 0; i < SS.request.elems; i++) {
        Request *r = &(SS.request.elem[i].t);

        if(r->name.str[0]) {
            Printf("  %s", r->name.str);
        } else {
            Printf("  unnamed");
        }
    }
}

