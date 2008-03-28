#include "solvespace.h"
#include <stdarg.h>

const TextWindow::Color TextWindow::colors[] = {
    { COLOR_FG_DEFAULT,         COLOR_BG_DEFAULT,   },  // 0
    { RGB(255,  40,  40),       COLOR_BG_DEFAULT,   },  // 1
    { RGB(255, 255,   0),       COLOR_BG_DEFAULT,   },  // 2
    { RGB( 40, 255,  40),       COLOR_BG_DEFAULT,   },  // 3
    { RGB(  0, 255, 255),       COLOR_BG_DEFAULT,   },  // 4
    { RGB(140, 140, 255),       COLOR_BG_DEFAULT,   },  // 5
    { RGB(255,   0, 255),       COLOR_BG_DEFAULT,   },  // 6
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
    row0 = 0;
    rows = 0;
}

void TextWindow::Printf(char *fmt, ...) {
    va_list vl;
    va_start(vl, fmt);

    int r, c;
    if(rows < MAX_ROWS) {
        r = rows;
        rows++;
    } else {
        r = row0;
        row0++;
    }
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
                    color = *fmt - '0';
                    if(color < 0 || color >= arraylen(colors)) color = 0;
                    break;

                case 'L':
                    if(fmt[1] == '\0') goto done;
                    fmt++;
                    link = *fmt;
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

