#include "solvespace.h"
#include <windows.h>
#include <commctrl.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#define TEXT_HEIGHT 18
#define TEXT_WIDTH  10

HINSTANCE Instance;

HWND TextWnd, GraphicsWnd;
HWND TextWndScrollBar;
int TextWndScrollPos;

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
    FillRect(hdc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SelectObject(hdc, FixedFont);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkColor(hdc, RGB(0, 0, 0));

    int h = rect.bottom - rect.top;
    int rows = h / TEXT_HEIGHT;
    rows--;

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
            TextOut(hdc, 4 + c*TEXT_WIDTH, r*TEXT_HEIGHT,
                                            (char *)&(SS.TW.text[rr][c]), 1);
        }
    }

    TextOut(hdc, 4, rows*TEXT_HEIGHT, SS.TW.cmd, SS.TW.MAX_COLS);
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
            dbp("extra=%d", extra);
            break;
        }

        case WM_CHAR:
            SS.TW.KeyPressed(wParam);
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
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

LRESULT CALLBACK GraphicsWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                            LPARAM lParam)
{
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            return 1;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

static void CreateMainWindows(void)
{
    WNDCLASSEX wc;

    // The text window, with a comand line and some textual information
    // about the sketch.
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style            = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_OWNDC |
                          CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)TextWndProc;
    wc.hInstance        = Instance;
    wc.hbrBackground    = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName    = "TextWnd";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = NULL;
    wc.hIconSm          = NULL;
    if(!RegisterClassEx(&wc)) oops();

    TextWnd = CreateWindowEx(0, "TextWnd", "SolveSpace (Command Line)",
        WS_OVERLAPPED | WS_THICKFRAME | WS_CLIPCHILDREN | WS_MAXIMIZEBOX |
        WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX,
        10, 10, 600, 300, NULL, (HMENU)NULL, Instance, NULL);
    if(!TextWnd) oops();

    TextWndScrollBar = CreateWindowEx(0, WC_SCROLLBAR, "", WS_CHILD |
        SBS_VERT | SBS_LEFTALIGN | WS_VISIBLE | WS_CLIPSIBLINGS, 
        200, 100, 100, 100, TextWnd, NULL, Instance, NULL);
    // Force the scrollbar to get resized to the window,
    TextWndProc(TextWnd, WM_SIZE, 0, 0);

    ShowWindow(TextWnd, SW_SHOW);

    // The graphics window, where the sketch is drawn and shown.
    wc.lpfnWndProc      = (WNDPROC)GraphicsWndProc;
    wc.hInstance        = Instance;
    wc.hbrBackground    = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName    = "GraphicsWnd";
    if(!RegisterClassEx(&wc)) oops();

    GraphicsWnd = CreateWindowEx(0, "GraphicsWnd", "SolveSpace (View Sketch)",
        WS_OVERLAPPED | WS_THICKFRAME | WS_CLIPCHILDREN | WS_MAXIMIZEBOX |
        WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX,
        600, 300, 400, 400, NULL, (HMENU)NULL, Instance, NULL);
    if(!GraphicsWnd) oops();

    ShowWindow(GraphicsWnd, SW_SHOW);

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

    // A monospaced font
    FixedFont = CreateFont(TEXT_HEIGHT-1, TEXT_WIDTH, 0, 0, FW_REGULAR, FALSE,
        FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");
    if(!FixedFont)
        FixedFont = (HFONT)GetStockObject(SYSTEM_FONT);

    // Call in to the platform-independent code, and let them do their init
    SS.Init();
   
    // And now it's the message loop. All calls in to the rest of the code
    // will be from the wndprocs.
    MSG msg;
    DWORD ret;
    while(ret = GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
