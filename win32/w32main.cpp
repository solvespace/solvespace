#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <gl/gl.h> 
#include <gl/glu.h> 
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "solvespace.h"

#define FREEZE_SUBKEY "SolveSpace"
#include "freeze.h"

#define MIN_COLS    42
#define TEXT_HEIGHT 18
#define TEXT_WIDTH  9

HINSTANCE Instance;

HWND TextWnd;
HWND TextWndScrollBar;
int TextWndScrollPos;
int TextWndRows;

HWND GraphicsWnd;
HWND GraphicsEditControl;
HMENU SubMenus[100];
struct {
    int x, y;
} LastMousePos;

int ClientIsSmallerBy;

HFONT FixedFont, LinkFont;

void dbp(char *str, ...)
{
    va_list f;
    static char buf[1024*50];
    va_start(f, str);
    vsprintf(buf, str, f);
    OutputDebugString(buf);
    va_end(f);
}

void Error(char *str, ...)
{
    va_list f;
    char buf[1024];
    va_start(f, str);
    vsprintf(buf, str, f);
    va_end(f);

    HWND h = GetForegroundWindow();
    MessageBox(h, buf, "SolveSpace Error", MB_OK | MB_ICONERROR);
}

//-----------------------------------------------------------------------------
// A separate heap, on which we allocate expressions. Maybe a bit faster,
// since no fragmentation issues whatsoever, and it also makes it possible
// to be sloppy with our memory management, and just free everything at once
// at the end.
//-----------------------------------------------------------------------------
static HANDLE Heap;
Expr *AllocExpr(void)
{
    Expr *v = (Expr *)HeapAlloc(Heap, HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY, 
                                                                 sizeof(Expr));
    if(!v) oops();
    memset(v, 0, sizeof(*v));
    return v;
}
void FreeAllExprs(void)
{
    if(Heap) HeapDestroy(Heap);
    Heap = HeapCreate(HEAP_NO_SERIALIZE, 1024*1024*20, 0);
}

void *MemRealloc(void *p, int n) { return realloc(p, n); }
void *MemAlloc(int n) { return malloc(n); }
void MemFree(void *p) { free(p); }

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
    SetBkColor(backDc, SS.TW.COLOR_BG_DEFAULT);

    int rows = height / TEXT_HEIGHT;
    TextWndRows = rows;

    TextWndScrollPos = min(TextWndScrollPos, SS.TW.rows - rows);
    TextWndScrollPos = max(TextWndScrollPos, 0);

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

        for(c = 0; c < SS.TW.MAX_COLS; c++) {
            char v = '0' + (c % 10);
            int color = SS.TW.meta[r][c].color;
            SetTextColor(backDc, SS.TW.colors[color].fg);
            SetBkColor(backDc, SS.TW.colors[color].bg);

            if(SS.TW.meta[r][c].link) {
                SelectObject(backDc, LinkFont);
            } else {
                SelectObject(backDc, FixedFont);
            }

            int x = 4 + c*TEXT_WIDTH;
            int y = (r-TextWndScrollPos)*TEXT_HEIGHT + 1 + (r >= 3 ? 9 : 0);

            HBRUSH b = CreateSolidBrush(SS.TW.colors[color].bg);
            RECT a;
            a.left = x; a.right = x+TEXT_WIDTH;
            a.top = y; a.bottom = y+TEXT_HEIGHT;
            FillRect(backDc, &a, b);
            DeleteObject(b);

            TextOut(backDc, x, y, (char *)&(SS.TW.text[r][c]), 1);
        }
    }

    // And commit the back buffer
    BitBlt(hdc, 0, 0, width, height, backDc, 0, 0, SRCCOPY);
    DeleteObject(backBitmap);
    DeleteObject(hbr);
    DeleteDC(backDc);
}

void HandleTextWindowScrollBar(WPARAM wParam, LPARAM lParam)
{
    int prevPos = TextWndScrollPos;
    switch(LOWORD(wParam)) {
        case SB_LINEUP:         TextWndScrollPos--; break;
        case SB_PAGEUP:         TextWndScrollPos -= 4; break;

        case SB_LINEDOWN:       TextWndScrollPos++; break;
        case SB_PAGEDOWN:       TextWndScrollPos += 4; break;

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
            hc += TEXT_HEIGHT/2;
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
            int tooNarrow = (MIN_COLS*TEXT_WIDTH) - (r->right - r->left);
            if(tooNarrow >= 0) {
                switch(wParam) {
                    case WMSZ_RIGHT:
                    case WMSZ_BOTTOMRIGHT:
                    case WMSZ_TOPRIGHT:
                        r->right += tooNarrow;
                        break;

                    case WMSZ_LEFT:
                    case WMSZ_BOTTOMLEFT:
                    case WMSZ_TOPLEFT:
                        r->left -= tooNarrow;
                        break;
                }
            }
            break;
        }

        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // Find the corresponding character in the text buffer
            int r = (y / TEXT_HEIGHT);
            int c = (x / TEXT_WIDTH);
            if(msg == WM_MOUSEMOVE && r >= TextWndRows) {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                break;
            }
            r += TextWndScrollPos;

            if(msg == WM_MOUSEMOVE) {
                if(SS.TW.meta[r][c].link) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                } else {
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                }
            } else {
                if(SS.TW.meta[r][c].link && SS.TW.meta[r][c].f) {
                    (SS.TW.meta[r][c].f)(
                        SS.TW.meta[r][c].link,
                        SS.TW.meta[r][c].data
                    );
                }
            }
            break;
        }
        
        case WM_SIZE: {
            RECT r;
            GetWindowRect(TextWndScrollBar, &r);
            int sw = r.right - r.left;
            GetClientRect(hwnd, &r);
            MoveWindow(TextWndScrollBar, r.right - sw, r.top, sw,
                (r.bottom - r.top), TRUE);
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

static BOOL ProcessKeyDown(WPARAM wParam)
{
    if(GraphicsEditControlIsVisible() && wParam != VK_ESCAPE) {
        if(wParam == VK_RETURN) {
            char s[1024];
            memset(s, 0, sizeof(s));
            SendMessage(GraphicsEditControl, WM_GETTEXT, 900, (LPARAM)s);
            SS.GW.EditControlDone(s);
            return TRUE;
        } else {
            return FALSE;
        }
    }

    int c;
    switch(wParam) {
        case VK_OEM_PLUS:       c = '+';    break;
        case VK_OEM_MINUS:      c = '-';    break;
        case VK_ESCAPE:         c = 27;     break;
        case VK_OEM_4:          c = '[';    break;
        case VK_OEM_6:          c = ']';    break;
        case VK_OEM_5:          c = '\\';   break;
        case VK_SPACE:          c = ' ';    break;
        case VK_DELETE:         c = 127;    break;

        default:
            c = wParam;
            break;
    }
    if(GetAsyncKeyState(VK_SHIFT) & 0x8000)   c |= 0x100;
    if(GetAsyncKeyState(VK_CONTROL) & 0x8000) c |= 0x200;

    for(int i = 0; SS.GW.menu[i].level >= 0; i++) {
        if(c == SS.GW.menu[i].accel) {
            (SS.GW.menu[i].fn)((GraphicsWindow::MenuId)SS.GW.menu[i].id);
            break;
        }
    }

    // No accelerator; process the key as normal.
    return FALSE;
}

static HGLRC CreateGlContext(HDC hdc)
{
    PIXELFORMATDESCRIPTOR pfd;
    int pixelFormat; 

    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR); 
    pfd.nVersion = 1; 
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |  
                        PFD_DOUBLEBUFFER; 
    pfd.dwLayerMask = PFD_MAIN_PLANE; 
    pfd.iPixelType = PFD_TYPE_RGBA; 
    pfd.cColorBits = 8; 
    pfd.cDepthBits = 16; 
    pfd.cAccumBits = 0; 
    pfd.cStencilBits = 0; 
 
    pixelFormat = ChoosePixelFormat(hdc, &pfd); 
    if(!pixelFormat) oops();
 
    if(!SetPixelFormat(hdc, pixelFormat, &pfd)) oops();

    HGLRC hgrc = wglCreateContext(hdc); 
    wglMakeCurrent(hdc, hgrc); 

    return hgrc;
}

void InvalidateGraphics(void)
{
    InvalidateRect(GraphicsWnd, NULL, FALSE);
}
static void PaintGraphicsWithHdc(HDC hdc)
{
    HGLRC hgrc = CreateGlContext(hdc);

    RECT r;
    GetClientRect(GraphicsWnd, &r);
    int w = r.right - r.left;
    int h = r.bottom - r.top;

    SS.GW.Paint(w, h);

    SwapBuffers(hdc);

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hgrc);
}
void PaintGraphics(void)
{
    HDC hdc = GetDC(GraphicsWnd);
    PaintGraphicsWithHdc(hdc);
}
SDWORD GetMilliseconds(void)
{
    return (SDWORD)GetTickCount();
}

void InvalidateText(void)
{
    InvalidateRect(TextWnd, NULL, FALSE);
}

void ShowGraphicsEditControl(int x, int y, char *s)
{
    RECT r;
    GetClientRect(GraphicsWnd, &r);
    x = x + (r.right - r.left)/2;
    y = (r.bottom - r.top)/2 - y;

    // (x, y) are the bottom left, but the edit control is placed by its
    // top left corner
    y -= 20;

    MoveWindow(GraphicsEditControl, x, y, 220, 21, TRUE);
    ShowWindow(GraphicsEditControl, SW_SHOW);
    SendMessage(GraphicsEditControl, WM_SETTEXT, 0, (LPARAM)s);
    SendMessage(GraphicsEditControl, EM_SETSEL, 0, strlen(s));
    SetFocus(GraphicsEditControl);
}
void HideGraphicsEditControl(void)
{
    ShowWindow(GraphicsEditControl, SW_HIDE);
}
BOOL GraphicsEditControlIsVisible(void)
{
    return IsWindowVisible(GraphicsEditControl);
}

LRESULT CALLBACK GraphicsWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                            LPARAM lParam)
{
    switch (msg) {
        case WM_ERASEBKGND:
            break;

        case WM_SIZE:
            InvalidateRect(GraphicsWnd, NULL, FALSE);
            break;

        case WM_PAINT: {
            InvalidateRect(GraphicsWnd, NULL, FALSE);
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            PaintGraphicsWithHdc(hdc);

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            RECT r;
            GetClientRect(GraphicsWnd, &r);
            x = x - (r.right - r.left)/2;
            y = (r.bottom - r.top)/2 - y;

            LastMousePos.x = x;
            LastMousePos.y = y;

            if(msg == WM_LBUTTONDOWN) {
                SS.GW.MouseLeftDown(x, y);
            } else if(msg == WM_LBUTTONUP) {
                SS.GW.MouseLeftUp(x, y);
            } else if(msg == WM_LBUTTONDBLCLK) {
                SS.GW.MouseLeftDoubleClick(x, y);
            } else if(msg == WM_MBUTTONDOWN) {
                SS.GW.MouseMiddleDown(x, y);
            } else if(msg == WM_MOUSEMOVE) {
                SS.GW.MouseMoved(x, y,
                    !!(wParam & MK_LBUTTON),
                    !!(wParam & MK_MBUTTON),
                    !!(wParam & MK_RBUTTON),
                    !!(wParam & MK_SHIFT),
                    !!(wParam & MK_CONTROL));
            } else {
                oops();
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            SS.GW.MouseScroll(LastMousePos.x, LastMousePos.y, delta);
            break;
        }
        case WM_COMMAND: {
            if(HIWORD(wParam) == 0) {
                int id = LOWORD(wParam);
                for(int i = 0; SS.GW.menu[i].level >= 0; i++) {
                    if(id == SS.GW.menu[i].id) {
                        (SS.GW.menu[i].fn)((GraphicsWindow::MenuId)id);
                        break;
                    }
                }
            }
            break;
        }

        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            return 1;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

//-----------------------------------------------------------------------------
// Common dialog routines, to open or save a file.
//-----------------------------------------------------------------------------
BOOL GetOpenFile(char *file, char *defExtension, char *selPattern)
{
    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = Instance;
    ofn.hwndOwner = GraphicsWnd;
    ofn.lpstrFilter = selPattern;
    ofn.lpstrDefExt = defExtension;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    EnableWindow(GraphicsWnd, FALSE);
    BOOL r = GetOpenFileName(&ofn);
    EnableWindow(GraphicsWnd, TRUE);
    SetForegroundWindow(GraphicsWnd);
    return r;
}
BOOL GetSaveFile(char *file, char *defExtension, char *selPattern)
{
    OPENFILENAME ofn;

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = Instance;
    ofn.hwndOwner = GraphicsWnd;
    ofn.lpstrFilter = selPattern;
    ofn.lpstrDefExt = defExtension;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

    EnableWindow(GraphicsWnd, FALSE);
    BOOL r = GetSaveFileName(&ofn);
    EnableWindow(GraphicsWnd, TRUE);
    SetForegroundWindow(GraphicsWnd);
    return r;
}
int SaveFileYesNoCancel(void)
{
    return MessageBox(GraphicsWnd, 
        "The program has changed since it was last saved.\r\n\r\n"
        "Do you want to save the changes?", "SolveSpace",
        MB_YESNOCANCEL | MB_ICONWARNING);
}


static void MenuById(int id, BOOL yes, BOOL check)
{
    int i;
    int subMenu = -1;

    for(i = 0; SS.GW.menu[i].level >= 0; i++) {
        if(SS.GW.menu[i].level == 0) subMenu++;
        
        if(SS.GW.menu[i].id == id) {
            if(subMenu < 0) oops();
            if(subMenu >= (sizeof(SubMenus)/sizeof(SubMenus[0]))) oops();

            if(check) {
                CheckMenuItem(SubMenus[subMenu], id,
                            yes ? MF_CHECKED : MF_UNCHECKED);
            } else {
                EnableMenuItem(SubMenus[subMenu], id,
                            yes ? MF_ENABLED : MF_GRAYED);
            }
            return;
        }
    }
    oops();
}
void CheckMenuById(int id, BOOL checked)
{
    MenuById(id, checked, TRUE);
}
void EnableMenuById(int id, BOOL enabled)
{
    MenuById(id, enabled, FALSE);
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
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1); 
    wc.lpszClassName    = "GraphicsWnd";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = NULL;
    wc.hIconSm          = NULL;
    if(!RegisterClassEx(&wc)) oops();

    HMENU top = CreateGraphicsWindowMenus();
    GraphicsWnd = CreateWindowEx(0, "GraphicsWnd",
        "SolveSpace (Graphics Window)",
        WS_OVERLAPPED | WS_THICKFRAME | WS_CLIPCHILDREN | WS_MAXIMIZEBOX |
        WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX | WS_CLIPSIBLINGS,
        600, 300, 200, 200, NULL, top, Instance, NULL);
    if(!GraphicsWnd) oops();

    GraphicsEditControl = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, "",
        WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS,
        50, 50, 100, 21, GraphicsWnd, NULL, Instance, NULL);
    SendMessage(GraphicsEditControl, WM_SETFONT, (WPARAM)FixedFont, TRUE);


    // The text window, with a comand line and some textual information
    // about the sketch.
    wc.style           &= ~CS_DBLCLKS;
    wc.lpfnWndProc      = (WNDPROC)TextWndProc;
    wc.hbrBackground    = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName    = "TextWnd";
    wc.hCursor          = NULL;
    if(!RegisterClassEx(&wc)) oops();

    // We get the desired Alt+Tab behaviour by specifying that the text
    // window is a child of the graphics window.
    TextWnd = CreateWindowEx(0, 
        "TextWnd", "SolveSpace (Text Window)",
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

    InitCommonControls();

    // A monospaced font
    FixedFont = CreateFont(TEXT_HEIGHT-2, TEXT_WIDTH, 0, 0, FW_REGULAR, FALSE,
        FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");
    LinkFont = CreateFont(TEXT_HEIGHT-2, TEXT_WIDTH, 0, 0, FW_REGULAR, FALSE,
        TRUE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");
    if(!FixedFont)
        FixedFont = (HFONT)GetStockObject(SYSTEM_FONT);
    if(!LinkFont)
        LinkFont = (HFONT)GetStockObject(SYSTEM_FONT);

    // Create the root windows: one for control, with text, and one for
    // the graphics
    CreateMainWindows();

    ThawWindowPos(TextWnd);
    ThawWindowPos(GraphicsWnd);

    // Create the heap that we use to store Exprs.
    FreeAllExprs();

    // Call in to the platform-independent code, and let them do their init
    SS.Init(lpCmdLine);

    ShowWindow(TextWnd, SW_SHOWNOACTIVATE);
    ShowWindow(GraphicsWnd, SW_SHOW);
   
    // And now it's the message loop. All calls in to the rest of the code
    // will be from the wndprocs.
    MSG msg;
    DWORD ret;
    while(ret = GetMessage(&msg, NULL, 0, 0)) {
        if(msg.message == WM_KEYDOWN) {
            if(ProcessKeyDown(msg.wParam)) continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    FreezeWindowPos(TextWnd);
    FreezeWindowPos(GraphicsWnd);

    return 0;
}
