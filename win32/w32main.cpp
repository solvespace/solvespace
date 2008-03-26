#include <windows.h>
#include <commctrl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "solvespace.h"

#define FREEZE_SUBKEY "SolveSpace"
#include "freeze.h"

#define TEXT_HEIGHT 18
#define TEXT_WIDTH  10

HINSTANCE Instance;

HWND TextWnd, GraphicsWnd;
HWND TextWndScrollBar;
int TextWndScrollPos;
int TextWndRows;

HMENU SubMenus[100];

int ClientIsSmallerBy;

HFONT FixedFont;

void dbp(char *str, ...)
{
    va_list f;
    char buf[1024];
    va_start(f, str);
    vsprintf(buf, str, f);
    OutputDebugString(buf);
    OutputDebugString("\n");
}

static void PaintTextWnd(HDC hdc)
{
    RECT rect;
    GetClientRect(TextWnd, &rect);

    // Set up the back-buffer
    HDC backDc = CreateCompatibleDC(hdc);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    HBITMAP backBitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(backDc, backBitmap);

    HBRUSH hbr = CreateSolidBrush(SS.TW.COLOR_BG_DEFAULT);
    FillRect(backDc, &rect, hbr);

    SelectObject(backDc, FixedFont);
    SetTextColor(backDc, SS.TW.COLOR_FG_DEFAULT);
    SetBkColor(backDc, SS.TW.COLOR_BG_DEFAULT);

    int rows = height / TEXT_HEIGHT;
    rows--;
    TextWndRows = rows;

    // Let's set up the scroll bar first
    SCROLLINFO si;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_DISABLENOSCROLL | SIF_ALL;
    si.nMin = 0;
    si.nMax = SS.TW.rows - 1;
    si.nPos = TextWndScrollPos;
    si.nPage = rows;
    SetScrollInfo(TextWndScrollBar, SB_CTL, &si, TRUE);

    int r, c;
    for(r = TextWndScrollPos; r < (TextWndScrollPos+rows); r++) {
        if(r < 0) continue;
        if(r >= SS.TW.MAX_ROWS) continue;
        int rr = (r + SS.TW.row0);
        while(rr >= SS.TW.MAX_ROWS) rr -= SS.TW.MAX_ROWS;

        for(c = 0; c < SS.TW.MAX_COLS; c++) {
            char v = '0' + (c % 10);
            TextOut(backDc, 4 + c*TEXT_WIDTH, (r-TextWndScrollPos)*TEXT_HEIGHT,
                                            (char *)&(SS.TW.text[rr][c]), 1);
        }
    }

    SetTextColor(backDc, SS.TW.COLOR_FG_CMDLINE);
    SetBkColor(backDc, SS.TW.COLOR_BG_CMDLINE);
    TextOut(backDc, 4, rows*TEXT_HEIGHT, SS.TW.cmd, SS.TW.MAX_COLS);

    HPEN cpen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
    SelectObject(backDc, cpen);
    int y = (rows+1)*TEXT_HEIGHT - 3;
    MoveToEx(backDc, 4+(SS.TW.cmdInsert*TEXT_WIDTH), y, NULL);
    LineTo(backDc, 4+(SS.TW.cmdInsert*TEXT_WIDTH)+TEXT_WIDTH, y);

    // And commit the back buffer
    BitBlt(hdc, 0, 0, width, height, backDc, 0, 0, SRCCOPY);
    DeleteObject(backBitmap);
    DeleteObject(hbr);
    DeleteObject(cpen);
    DeleteDC(backDc);
}

void HandleTextWindowScrollBar(WPARAM wParam, LPARAM lParam)
{
    int prevPos = TextWndScrollPos;
    switch(LOWORD(wParam)) {
        case SB_LINEUP:
        case SB_PAGEUP:         TextWndScrollPos--; break;

        case SB_LINEDOWN:
        case SB_PAGEDOWN:       TextWndScrollPos++; break;

        case SB_TOP:            TextWndScrollPos = 0; break;

        case SB_BOTTOM:         TextWndScrollPos = SS.TW.rows; break;

        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:  TextWndScrollPos = HIWORD(wParam); break;
    }
    TextWndScrollPos = max(0, TextWndScrollPos);
    TextWndScrollPos = min(SS.TW.rows - TextWndRows, TextWndScrollPos);
    if(prevPos != TextWndScrollPos) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        si.nPos = TextWndScrollPos;
        SetScrollInfo(TextWndScrollBar, SB_CTL, &si, TRUE);

        InvalidateRect(TextWnd, NULL, FALSE);
    }
}

LRESULT CALLBACK TextWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintTextWnd(hdc);
            EndPaint(hwnd, &ps);
            break;
        }
        
        case WM_SIZING: {
            RECT *r = (RECT *)lParam;
            int hc = (r->bottom - r->top) - ClientIsSmallerBy;
            int extra = hc % TEXT_HEIGHT;
            switch(wParam) {
                case WMSZ_BOTTOM:
                case WMSZ_BOTTOMLEFT:
                case WMSZ_BOTTOMRIGHT:
                    r->bottom -= extra;
                    break;

                case WMSZ_TOP:
                case WMSZ_TOPLEFT:
                case WMSZ_TOPRIGHT:
                    r->top += extra;
                    break;
            }
            hc = (r->bottom - r->top) - ClientIsSmallerBy;
            extra = hc % TEXT_HEIGHT;
            break;
        }

        case WM_CHAR:
            SS.TW.KeyPressed(wParam);
            HandleTextWindowScrollBar(SB_BOTTOM, 0);
            InvalidateRect(TextWnd, NULL, FALSE);
            break;

        case WM_SIZE: {
            RECT r;
            GetWindowRect(TextWndScrollBar, &r);
            int sw = r.right - r.left;
            GetClientRect(hwnd, &r);
            MoveWindow(TextWndScrollBar, r.right - sw, r.top, sw,
                (r.bottom - r.top) - TEXT_HEIGHT, TRUE);
            InvalidateRect(TextWnd, NULL, FALSE);
            break;
        }

        case WM_VSCROLL:
            HandleTextWindowScrollBar(wParam, lParam);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

LRESULT CALLBACK GraphicsWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                            LPARAM lParam)
{
    switch (msg) {
        case WM_CHAR:
            SS.TW.KeyPressed(wParam);
            SetForegroundWindow(TextWnd);
            HandleTextWindowScrollBar(SB_BOTTOM, 0);
            InvalidateRect(TextWnd, NULL, FALSE);
            break;

        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            return 1;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

HMENU CreateGraphicsWindowMenus(void)
{
    HMENU top = CreateMenu();
    HMENU m;

    int i;
    int subMenu = 0;
    
    for(i = 0; SS.GW.menu[i].level >= 0; i++) {
        if(SS.GW.menu[i].level == 0) {
            m = CreateMenu();
            AppendMenu(top, MF_STRING | MF_POPUP, (UINT_PTR)m, 
                                                        SS.GW.menu[i].label);

            if(subMenu >= arraylen(SubMenus)) oops();
            SubMenus[subMenu] = m;
            subMenu++;
        } else {
            if(SS.GW.menu[i].label) {
                AppendMenu(m, MF_STRING, SS.GW.menu[i].id, SS.GW.menu[i].label);
            } else {
                AppendMenu(m, MF_SEPARATOR, SS.GW.menu[i].id, "");
            }
        }
    }

    return top;
}

static void CreateMainWindows(void)
{
    WNDCLASSEX wc;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);

    // The graphics window, where the sketch is drawn and shown.
    wc.style            = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_OWNDC |
                          CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)GraphicsWndProc;
    wc.hbrBackground    = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName    = "GraphicsWnd";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = NULL;
    wc.hIconSm          = NULL;
    if(!RegisterClassEx(&wc)) oops();

    HMENU top = CreateGraphicsWindowMenus();
    GraphicsWnd = CreateWindowEx(0, "GraphicsWnd", "SolveSpace (View Sketch)",
        WS_OVERLAPPED | WS_THICKFRAME | WS_CLIPCHILDREN | WS_MAXIMIZEBOX |
        WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX,
        600, 300, 400, 400, NULL, top, Instance, NULL);
    if(!GraphicsWnd) oops();


    // The text window, with a comand line and some textual information
    // about the sketch.
    wc.lpfnWndProc      = (WNDPROC)TextWndProc;
    wc.hbrBackground    = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName    = "TextWnd";
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    if(!RegisterClassEx(&wc)) oops();

    // We get the desired Alt+Tab behaviour by specifying that the text
    // window is a child of the graphics window.
    TextWnd = CreateWindowEx(0, 
        "TextWnd", "SolveSpace (Command Line)",
        WS_THICKFRAME | WS_CLIPCHILDREN,
        10, 10, 600, 300, GraphicsWnd, (HMENU)NULL, Instance, NULL);
    if(!TextWnd) oops();

    TextWndScrollBar = CreateWindowEx(0, WC_SCROLLBAR, "", WS_CHILD |
        SBS_VERT | SBS_LEFTALIGN | WS_VISIBLE | WS_CLIPSIBLINGS, 
        200, 100, 100, 100, TextWnd, NULL, Instance, NULL);
    // Force the scrollbar to get resized to the window,
    TextWndProc(TextWnd, WM_SIZE, 0, 0);


    RECT r, rc;
    GetWindowRect(TextWnd, &r);
    GetClientRect(TextWnd, &rc);
    ClientIsSmallerBy = (r.bottom - r.top) - (rc.bottom - rc.top);
}

//-----------------------------------------------------------------------------
// Entry point into the program.
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, INT nCmdShow)
{
    Instance = hInstance;

    // Create the root windows: one for control, with text, and one for
    // the graphics
    CreateMainWindows();

    ThawWindowPos(TextWnd);
    ThawWindowPos(GraphicsWnd);

    // A monospaced font
    FixedFont = CreateFont(TEXT_HEIGHT-1, TEXT_WIDTH, 0, 0, FW_REGULAR, FALSE,
        FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");
    if(!FixedFont)
        FixedFont = (HFONT)GetStockObject(SYSTEM_FONT);

    // Call in to the platform-independent code, and let them do their init
    SS.Init();

    ShowWindow(TextWnd, SW_SHOWNOACTIVATE);
    ShowWindow(GraphicsWnd, SW_SHOW);
   
    // And now it's the message loop. All calls in to the rest of the code
    // will be from the wndprocs.
    MSG msg;
    DWORD ret;
    while(ret = GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    FreezeWindowPos(TextWnd);
    FreezeWindowPos(GraphicsWnd);

    return 0;
}
