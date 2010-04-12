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
    { 'g', RGB(160, 160, 160) },
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
                case '#': {
                    double v = va_arg(vl, double);
                    sprintf(buf, "%.3f", v);
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
                    if(*fmt == 'p') {
                        link = va_arg(vl, int);
                    } else {
                        link = *fmt;
                    }
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
        Printf(true, "%Fl%f%Ll(cancel operation)%E",
            &TextWindow::ScreenUnselectAll);
    } else if((gs.n > 0 || gs.constraints > 0) && 
                                    shown.screen != SCREEN_PASTE_TRANSFORMED)
    {
        if(edit.meaning != EDIT_TTF_TEXT) HideTextEditControl();
        ShowHeader(false);
        DescribeSelection();
    } else {
        if(edit.meaning == EDIT_TTF_TEXT) HideTextEditControl();
        ShowHeader(true);
        switch(shown.screen) {
            default:
                shown.screen = SCREEN_LIST_OF_GROUPS;
                // fall through
            case SCREEN_LIST_OF_GROUPS:     ShowListOfGroups();     break;
            case SCREEN_GROUP_INFO:         ShowGroupInfo();        break;
            case SCREEN_GROUP_SOLVE_INFO:   ShowGroupSolveInfo();   break;
            case SCREEN_CONFIGURATION:      ShowConfiguration();    break;
            case SCREEN_STEP_DIMENSION:     ShowStepDimension();    break;
            case SCREEN_LIST_OF_STYLES:     ShowListOfStyles();     break;
            case SCREEN_STYLE_INFO:         ShowStyleInfo();        break;
            case SCREEN_PASTE_TRANSFORMED:  ShowPasteTransformed(); break;
            case SCREEN_EDIT_VIEW:          ShowEditView();         break;
        }
    }
    Printf(false, "");
    InvalidateText();
}

