#include <windows.h>
#include <shellapi.h>
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

#define MIN_COLS    45
#define TEXT_HEIGHT 20
#define TEXT_WIDTH  9
#define TEXT_LEFT_MARGIN 4

// For the edit controls
#define EDIT_WIDTH  220
#define EDIT_HEIGHT 21

// The list representing glyph with ASCII code zero, for bitmap fonts
#define BITMAP_GLYPH_BASE 1000

HINSTANCE Instance;

HWND TextWnd;
HWND TextWndScrollBar;
HWND TextEditControl;
int TextEditControlCol, TextEditControlHalfRow;
int TextWndScrollPos; // The scrollbar position, in half-row units
int TextWndHalfRows;  // The height of our window, in half-row units

HWND GraphicsWnd;
HGLRC GraphicsHpgl;
HWND GraphicsEditControl;
struct {
    int x, y;
} LastMousePos;

char RecentFile[MAX_RECENT][MAX_PATH];
HMENU SubMenus[100];
HMENU RecentOpenMenu, RecentImportMenu;

int ClientIsSmallerBy;

HFONT FixedFont, LinkFont;

static void DoMessageBox(char *str, va_list f, BOOL error)
{
    char buf[1024*50];
    vsprintf(buf, str, f);

    EnableWindow(GraphicsWnd, FALSE);
    EnableWindow(TextWnd, FALSE);

    int flags;
    if(error) {
        flags = MB_OK | MB_ICONERROR;
    } else {
        flags = MB_OK | MB_ICONINFORMATION;
    }
    HWND h = GetForegroundWindow();
    MessageBox(h, buf, "SolveSpace", flags);

    EnableWindow(TextWnd, TRUE);
    EnableWindow(GraphicsWnd, TRUE);
    SetForegroundWindow(GraphicsWnd);
}

void Error(char *str, ...)
{
    va_list f;
    va_start(f, str);
    DoMessageBox(str, f, TRUE);
    va_end(f);
}

void Message(char *str, ...)
{
    va_list f;
    va_start(f, str);
    DoMessageBox(str, f, FALSE);
    va_end(f);
}

void CALLBACK TimerCallback(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
    // The timer is periodic, so needs to be killed explicitly.
    KillTimer(GraphicsWnd, 1);
    SS.GW.TimerCallback();
}
void SetTimerFor(int milliseconds)
{
    SetTimer(GraphicsWnd, 1, milliseconds, TimerCallback);
}

void DrawWithBitmapFont(char *str)
{
    // These lists were created in CreateGlContext
    glListBase(BITMAP_GLYPH_BASE); 
    glCallLists(strlen(str), GL_UNSIGNED_BYTE, str);
}
void GetBitmapFontExtent(char *str, int *w, int *h)
{
    // Easy since that's a fixed-width font for now.
    *h = TEXT_HEIGHT;
    *w = TEXT_WIDTH*strlen(str);
}

void OpenWebsite(char *url) {
    ShellExecute(GraphicsWnd, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

void ExitNow(void) {
    PostQuitMessage(0);
}

//-----------------------------------------------------------------------------
// Helpers so that we can read/write registry keys from the platform-
// independent code.
//-----------------------------------------------------------------------------
void CnfFreezeString(char *str, char *name)
    { FreezeStringF(str, FREEZE_SUBKEY, name); }

void CnfFreezeDWORD(DWORD v, char *name)
    { FreezeDWORDF(v, FREEZE_SUBKEY, name); }

void CnfFreezeFloat(float v, char *name)
    { FreezeDWORDF(*((DWORD *)&v), FREEZE_SUBKEY, name); }

void CnfThawString(char *str, int maxLen, char *name)
    { ThawStringF(str, maxLen, FREEZE_SUBKEY, name); }

DWORD CnfThawDWORD(DWORD v, char *name)
    { return ThawDWORDF(v, FREEZE_SUBKEY, name); }

float CnfThawFloat(float v, char *name) {
    DWORD d = ThawDWORDF(*((DWORD *)&v), FREEZE_SUBKEY, name); 
    return *((float *)&d);
}

void SetWindowTitle(char *str) {
    SetWindowText(GraphicsWnd, str);
}

static void PaintTextWnd(HDC hdc)
{
    int i;

    static BOOL MadeBrushes = FALSE;
    static COLORREF BgColor[256];
    static COLORREF FgColor[256];
    static HBRUSH   BgBrush[256];
    static HBRUSH   FillBrush;
    if(!MadeBrushes) {
        // Generate the color table.
        for(i = 0; SS.TW.fgColors[i].c != 0; i++) {
            int c = SS.TW.fgColors[i].c;
            if(c < 0 || c > 255) oops();
            FgColor[c] = SS.TW.fgColors[i].color;
        }
        for(i = 0; SS.TW.bgColors[i].c != 0; i++) {
            int c = SS.TW.bgColors[i].c;
            if(c < 0 || c > 255) oops();
            BgColor[c] = SS.TW.bgColors[i].color;
            BgBrush[c] = CreateSolidBrush(BgColor[c]);
        }
        FillBrush = CreateSolidBrush(RGB(0, 0, 0));
        MadeBrushes = TRUE;
    }

    RECT rect;
    GetClientRect(TextWnd, &rect);
    // Set up the back-buffer
    HDC backDc = CreateCompatibleDC(hdc);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    HBITMAP backBitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(backDc, backBitmap);

    FillRect(backDc, &rect, FillBrush);

    SelectObject(backDc, FixedFont);
    SetBkColor(backDc, RGB(0, 0, 0));

    int halfRows = height / (TEXT_HEIGHT/2);
    TextWndHalfRows = halfRows;

    int bottom = SS.TW.top[SS.TW.rows-1] + 2;
    TextWndScrollPos = min(TextWndScrollPos, bottom - halfRows);
    TextWndScrollPos = max(TextWndScrollPos, 0);

    // Let's set up the scroll bar first
    SCROLLINFO si;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_DISABLENOSCROLL | SIF_ALL;
    si.nMin = 0;
    si.nMax = SS.TW.top[SS.TW.rows - 1] + 1;
    si.nPos = TextWndScrollPos;
    si.nPage = halfRows;
    SetScrollInfo(TextWndScrollBar, SB_CTL, &si, TRUE);

    int r, c;
    for(r = 0; r < SS.TW.rows; r++) {
        int top = SS.TW.top[r];
        if(top < (TextWndScrollPos-1)) continue;
        if(top > TextWndScrollPos+halfRows) break;

        for(c = 0; c < min((width/TEXT_WIDTH)+1, SS.TW.MAX_COLS); c++) {
            int fg = SS.TW.meta[r][c].fg;
            int bg = SS.TW.meta[r][c].bg;

            SetTextColor(backDc, FgColor[fg]);

            HBRUSH bgb;
            if(bg & 0x80000000) {
                bgb = (HBRUSH)GetStockObject(BLACK_BRUSH);
                SetBkColor(backDc, bg & 0xffffff);
            } else {
                bgb = BgBrush[bg];
                SetBkColor(backDc, BgColor[bg]);
            }

            if(SS.TW.meta[r][c].link && SS.TW.meta[r][c].link != 'n') {
                SelectObject(backDc, LinkFont);
            } else {
                SelectObject(backDc, FixedFont);
            }

            int x = TEXT_LEFT_MARGIN + c*TEXT_WIDTH;
            int y = (top-TextWndScrollPos)*(TEXT_HEIGHT/2);

            RECT a;
            a.left = x; a.right = x+TEXT_WIDTH;
            a.top = y; a.bottom = y+TEXT_HEIGHT;
            FillRect(backDc, &a, bgb);

            TextOut(backDc, x, y+2, (char *)&(SS.TW.text[r][c]), 1);
        }
    }

    // And commit the back buffer
    BitBlt(hdc, 0, 0, width, height, backDc, 0, 0, SRCCOPY);
    DeleteObject(backBitmap);
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
    int bottom = SS.TW.top[SS.TW.rows-1] + 2;
    TextWndScrollPos = min(TextWndScrollPos, bottom - TextWndHalfRows);
    TextWndScrollPos = max(TextWndScrollPos, 0);
    if(prevPos != TextWndScrollPos) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        si.nPos = TextWndScrollPos;
        SetScrollInfo(TextWndScrollBar, SB_CTL, &si, TRUE);

        if(TextEditControlIsVisible()) {
            int x = TEXT_LEFT_MARGIN + TEXT_WIDTH*TextEditControlCol;
            int y = (TextEditControlHalfRow - TextWndScrollPos)*(TEXT_HEIGHT/2);
            MoveWindow(TextEditControl, x, y, EDIT_WIDTH, EDIT_HEIGHT, TRUE);
        }
        InvalidateRect(TextWnd, NULL, FALSE);
    }
}

void MouseWheel(int delta) {
    POINT pt;
    GetCursorPos(&pt);
    HWND hw = WindowFromPoint(pt);
    
    // Make the mousewheel work according to which window the mouse is
    // over, not according to which window is active.
    bool inTextWindow;
    if(hw == TextWnd) {
        inTextWindow = true;
    } else if(hw == GraphicsWnd) {
        inTextWindow = false;
    } else if(GetForegroundWindow() == TextWnd) {
        inTextWindow = true;
    } else {
        inTextWindow = false;
    }

    if(inTextWindow) {
        int i;
        for(i = 0; i < abs(delta/40); i++) {
            HandleTextWindowScrollBar(delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        }
    } else {
        SS.GW.MouseScroll(LastMousePos.x, LastMousePos.y, delta);
    }
}

LRESULT CALLBACK TextWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_ERASEBKGND:
            break;

        case WM_CLOSE:
        case WM_DESTROY:
            SolveSpace::MenuFile(GraphicsWindow::MNU_EXIT);
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
            int extra = hc % (TEXT_HEIGHT/2);
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
            if(TextEditControlIsVisible() || GraphicsEditControlIsVisible()) {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                break;
            }
            GraphicsWindow::Selection ps = SS.GW.hover;
            SS.GW.hover.Clear();

            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // Find the corresponding character in the text buffer
            int c = (x / TEXT_WIDTH);
            int hh = (TEXT_HEIGHT)/2;
            y += TextWndScrollPos*hh;
            int r;
            for(r = 0; r < SS.TW.rows; r++) {
                if(y >= SS.TW.top[r]*hh && y <= (SS.TW.top[r]+2)*hh) {
                    break;
                }
            }
            if(r >= SS.TW.rows) {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                goto done;
            }

#define META (SS.TW.meta[r][c])
            if(msg == WM_MOUSEMOVE) {
                if(META.link) {
                    SetCursor(LoadCursor(NULL, IDC_HAND));
                    if(META.h) {
                        (META.h)(META.link, META.data);
                    }
                } else {
                    SetCursor(LoadCursor(NULL, IDC_ARROW));
                }
            } else {
                if(META.link && META.f) {
                    (META.f)(META.link, META.data);
                    SS.TW.Show();
                    InvalidateGraphics();
                }
            }
done:
            if(!ps.Equals(&(SS.GW.hover))) {
                InvalidateGraphics();
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
            // If the window is growing, then the scrollbar position may
            // be moving, so it's as if we're dragging the scrollbar.
            HandleTextWindowScrollBar(-1, -1);
            InvalidateRect(TextWnd, NULL, FALSE);
            break;
        }

        case WM_MOUSEWHEEL:
            MouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            break;

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
    if(TextEditControlIsVisible() && wParam != VK_ESCAPE) {
        if(wParam == VK_RETURN) {
            char s[1024];
            memset(s, 0, sizeof(s));
            SendMessage(TextEditControl, WM_GETTEXT, 900, (LPARAM)s);
            SS.TW.EditControlDone(s);
        } else {
            return FALSE;
        }
    }

    if(wParam == VK_BACK && !GraphicsEditControlIsVisible()) {
        TextWindow::ScreenHome(0, 0);
        SS.TW.Show();
        return TRUE;
    }

    int c;
    switch(wParam) {
        case VK_OEM_PLUS:       c = '+';    break;
        case VK_OEM_MINUS:      c = '-';    break;
        case VK_ESCAPE:         c = 27;     break;
        case VK_OEM_1:          c = ';';    break;
        case VK_OEM_4:          c = '[';    break;
        case VK_OEM_6:          c = ']';    break;
        case VK_OEM_5:          c = '\\';   break;
        case VK_SPACE:          c = ' ';    break;
        case VK_DELETE:         c = 127;    break;
        case VK_TAB:            c = '\t';   break;

        case VK_F1:
        case VK_F2:
        case VK_F3:
        case VK_F4:
        case VK_F5:
        case VK_F6:
        case VK_F7:
        case VK_F8:
        case VK_F9:
        case VK_F10:
        case VK_F11:
        case VK_F12:            c = (wParam - VK_F1) + 0xf1; break;

        // These overlap with some character codes that I'm using, so
        // don't let them trigger by accident.
        case VK_F16:
        case VK_INSERT:
        case VK_EXECUTE:
        case VK_APPS:
        case VK_LWIN:
        case VK_RWIN:           return FALSE;

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

void ShowTextWindow(BOOL visible)
{
    ShowWindow(TextWnd, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

static void CreateGlContext(void)
{   
    HDC hdc = GetDC(GraphicsWnd);

    PIXELFORMATDESCRIPTOR pfd;
    int pixelFormat;

    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR); 
    pfd.nVersion = 1; 
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |  
                        PFD_DOUBLEBUFFER; 
    pfd.dwLayerMask = PFD_MAIN_PLANE; 
    pfd.iPixelType = PFD_TYPE_RGBA; 
    pfd.cColorBits = 32; 
    pfd.cDepthBits = 24; 
    pfd.cAccumBits = 0; 
    pfd.cStencilBits = 0; 
 
    pixelFormat = ChoosePixelFormat(hdc, &pfd); 
    if(!pixelFormat) oops();
 
    if(!SetPixelFormat(hdc, pixelFormat, &pfd)) oops();

    GraphicsHpgl = wglCreateContext(hdc); 
    wglMakeCurrent(hdc, GraphicsHpgl);

    // Create a bitmap font in a display list, for DrawWithBitmapFont().
    SelectObject(hdc, FixedFont);
    wglUseFontBitmaps(hdc, 0, 255, BITMAP_GLYPH_BASE); 
}

void InvalidateGraphics(void)
{
    InvalidateRect(GraphicsWnd, NULL, FALSE);
}
void GetGraphicsWindowSize(int *w, int *h)
{
    RECT r;
    GetClientRect(GraphicsWnd, &r);
    *w = r.right - r.left;
    *h = r.bottom - r.top;
}
void PaintGraphics(void)
{
    int w, h;
    GetGraphicsWindowSize(&w, &h);

    SS.GW.Paint(w, h);
    SwapBuffers(GetDC(GraphicsWnd));
}

SDWORD GetMilliseconds(void)
{
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t);
    QueryPerformanceFrequency(&f);
    LONGLONG d = t.QuadPart/(f.QuadPart/1000);
    return (SDWORD)d;
}

void InvalidateText(void)
{
    InvalidateRect(TextWnd, NULL, FALSE);
}

static void ShowEditControl(HWND h, int x, int y, char *s) {
    MoveWindow(h, x, y, EDIT_WIDTH, EDIT_HEIGHT, TRUE);
    ShowWindow(h, SW_SHOW);
    SendMessage(h, WM_SETTEXT, 0, (LPARAM)s);
    SendMessage(h, EM_SETSEL, 0, strlen(s));
    SetFocus(h);
}
void ShowTextEditControl(int hr, int c, char *s)
{
    if(TextEditControlIsVisible() || GraphicsEditControlIsVisible()) return;

    int x = TEXT_LEFT_MARGIN + TEXT_WIDTH*c;
    int y = (hr - TextWndScrollPos)*(TEXT_HEIGHT/2);
    TextEditControlCol = c;
    TextEditControlHalfRow = hr;
    ShowEditControl(TextEditControl, x, y, s);
}
void HideTextEditControl(void)
{
    ShowWindow(TextEditControl, SW_HIDE);
}
BOOL TextEditControlIsVisible(void)
{
    return IsWindowVisible(TextEditControl);
}
void ShowGraphicsEditControl(int x, int y, char *s)
{
    if(TextEditControlIsVisible() || GraphicsEditControlIsVisible()) return;

    RECT r;
    GetClientRect(GraphicsWnd, &r);
    x = x + (r.right - r.left)/2;
    y = (r.bottom - r.top)/2 - y;

    // (x, y) are the bottom left, but the edit control is placed by its
    // top left corner
    y -= 20;

    ShowEditControl(GraphicsEditControl, x, y, s);
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
            PaintGraphics();
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            break;
        }

        case WM_MOUSELEAVE:
            SS.GW.MouseLeave();
            break;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // We need this in order to get the WM_MOUSELEAVE
            TRACKMOUSEEVENT tme;
            ZERO(&tme);
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = GraphicsWnd;
            TrackMouseEvent(&tme);

            // Convert to xy (vs. ij) style coordinates, with (0, 0) at center
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
            } else if(msg == WM_MBUTTONDOWN || msg == WM_RBUTTONDOWN) {
                SS.GW.MouseMiddleOrRightDown(x, y);
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
        case WM_MOUSEWHEEL:
            MouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
            break;

        case WM_COMMAND: {
            if(HIWORD(wParam) == 0) {
                int id = LOWORD(wParam);
                if((id >= RECENT_OPEN && id < (RECENT_OPEN + MAX_RECENT))) {
                    SolveSpace::MenuFile(id);
                    break;
                }
                if((id >= RECENT_IMPORT && id < (RECENT_IMPORT + MAX_RECENT))) {
                    Group::MenuGroup(id);
                    break;
                }
                int i;
                for(i = 0; SS.GW.menu[i].level >= 0; i++) {
                    if(id == SS.GW.menu[i].id) {
                        (SS.GW.menu[i].fn)((GraphicsWindow::MenuId)id);
                        break;
                    }
                }
                if(SS.GW.menu[i].level < 0) oops();
            }
            break;
        }

        case WM_CLOSE:
        case WM_DESTROY:
            SolveSpace::MenuFile(GraphicsWindow::MNU_EXIT);
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
    EnableWindow(TextWnd, FALSE);

    BOOL r = GetOpenFileName(&ofn);

    EnableWindow(TextWnd, TRUE);
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
    EnableWindow(TextWnd, FALSE);

    BOOL r = GetSaveFileName(&ofn);

    EnableWindow(TextWnd, TRUE);
    EnableWindow(GraphicsWnd, TRUE);
    SetForegroundWindow(GraphicsWnd);

    return r;
}
int SaveFileYesNoCancel(void)
{
    EnableWindow(GraphicsWnd, FALSE);
    EnableWindow(TextWnd, FALSE);

    int r = MessageBox(GraphicsWnd, 
        "The program has changed since it was last saved.\r\n\r\n"
        "Do you want to save the changes?", "SolveSpace",
        MB_YESNOCANCEL | MB_ICONWARNING);

    EnableWindow(TextWnd, TRUE);
    EnableWindow(GraphicsWnd, TRUE);
    SetForegroundWindow(GraphicsWnd);

    return r;
}

void LoadAllFontFiles(void)
{
    WIN32_FIND_DATA wfd;
    char dir[MAX_PATH];
    GetWindowsDirectory(dir, MAX_PATH - 30);
    strcat(dir, "\\fonts\\*.ttf");

    HANDLE h = FindFirstFile(dir, &wfd);

    while(h != INVALID_HANDLE_VALUE) {
        TtfFont tf;
        ZERO(&tf);

        char fullPath[MAX_PATH];
        GetWindowsDirectory(fullPath, MAX_PATH - (30 + strlen(wfd.cFileName)));
        strcat(fullPath, "\\fonts\\");
        strcat(fullPath, wfd.cFileName);

        strcpy(tf.fontFile, fullPath);
        SS.fonts.l.Add(&tf);

        if(!FindNextFile(h, &wfd)) break;
    }
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
static void DoRecent(HMENU m, int base)
{
    while(DeleteMenu(m, 0, MF_BYPOSITION))
        ;
    int i, c = 0;
    for(i = 0; i < MAX_RECENT; i++) {
        char *s = RecentFile[i];
        if(*s) {
            AppendMenu(m, MF_STRING, base+i, s);
            c++;
        }
    }
    if(c == 0) AppendMenu(m, MF_STRING | MF_GRAYED, 0, "(no recent files)");
}
void RefreshRecentMenus(void)
{
    DoRecent(RecentOpenMenu,   RECENT_OPEN);
    DoRecent(RecentImportMenu, RECENT_IMPORT);
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
        } else if(SS.GW.menu[i].level == 1) {
            if(SS.GW.menu[i].label) {
                AppendMenu(m, MF_STRING, SS.GW.menu[i].id, SS.GW.menu[i].label);
            } else {
                AppendMenu(m, MF_SEPARATOR, SS.GW.menu[i].id, "");
            }
        } else if(SS.GW.menu[i].level == 10) {
            RecentOpenMenu = CreateMenu();
            AppendMenu(m, MF_STRING | MF_POPUP,
                (UINT_PTR)RecentOpenMenu, SS.GW.menu[i].label);
        } else if(SS.GW.menu[i].level == 11) {
            RecentImportMenu = CreateMenu();
            AppendMenu(m, MF_STRING | MF_POPUP,
                (UINT_PTR)RecentImportMenu, SS.GW.menu[i].label);
        } else oops();
    }
    RefreshRecentMenus();

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
    wc.hIcon            = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 32, 32, 0);
    wc.hIconSm          = (HICON)LoadImage(Instance, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 16, 16, 0);
    if(!RegisterClassEx(&wc)) oops();

    HMENU top = CreateGraphicsWindowMenus();
    GraphicsWnd = CreateWindowEx(0, "GraphicsWnd",
        "SolveSpace (Graphics Window)",
        WS_OVERLAPPED | WS_THICKFRAME | WS_CLIPCHILDREN | WS_MAXIMIZEBOX |
        WS_MINIMIZEBOX | WS_SYSMENU | WS_SIZEBOX | WS_CLIPSIBLINGS,
        50, 50, 900, 600, NULL, top, Instance, NULL);
    if(!GraphicsWnd) oops();

    CreateGlContext();

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
        "TextWnd", "SolveSpace (Text Window)", WS_THICKFRAME | WS_CLIPCHILDREN,
        650, 500, 420, 300, GraphicsWnd, (HMENU)NULL, Instance, NULL);
    if(!TextWnd) oops();

    TextWndScrollBar = CreateWindowEx(0, WC_SCROLLBAR, "", WS_CHILD |
        SBS_VERT | SBS_LEFTALIGN | WS_VISIBLE | WS_CLIPSIBLINGS, 
        200, 100, 100, 100, TextWnd, NULL, Instance, NULL);
    // Force the scrollbar to get resized to the window,
    TextWndProc(TextWnd, WM_SIZE, 0, 0);

    TextEditControl = CreateWindowEx(WS_EX_CLIENTEDGE, WC_EDIT, "",
        WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP | WS_CLIPSIBLINGS,
        50, 50, 100, 21, TextWnd, NULL, Instance, NULL);
    SendMessage(TextEditControl, WM_SETFONT, (WPARAM)FixedFont, TRUE);


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
    FixedFont = CreateFont(TEXT_HEIGHT-4, TEXT_WIDTH, 0, 0, FW_REGULAR, FALSE,
        FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, "Lucida Console");
    LinkFont = CreateFont(TEXT_HEIGHT-4, TEXT_WIDTH, 0, 0, FW_REGULAR, FALSE,
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

    // Create the heaps for all dynamic memory (AllocTemporary, MemAlloc)
    InitHeaps();

    // A filename may have been specified on the command line; if so, then
    // strip any quotation marks, and make it absolute.
    char file[MAX_PATH] = "";
    if(strlen(lpCmdLine)+1 < MAX_PATH) {
        char *s = lpCmdLine;
        while(*s == ' ' || *s == '"') s++;
        strcpy(file, s);
        s = strrchr(file, '"');
        if(s) *s = '\0';
    }
    if(*file != '\0') {
        GetAbsoluteFilename(file);
    }
    
    // Call in to the platform-independent code, and let them do their init
    SS.Init(file);

    ShowWindow(TextWnd, SW_SHOWNOACTIVATE);
    ShowWindow(GraphicsWnd, SW_SHOW);
   
    // And now it's the message loop. All calls in to the rest of the code
    // will be from the wndprocs.
    MSG msg;
    DWORD ret;
    while(ret = GetMessage(&msg, NULL, 0, 0)) {
        if(msg.message == WM_KEYDOWN) {
            if(ProcessKeyDown(msg.wParam)) goto done;
        }
        if(msg.message == WM_SYSKEYDOWN && msg.hwnd == TextWnd) {
            // If the user presses the Alt key when the text window has focus,
            // then that should probably go to the graphics window instead.
            SetForegroundWindow(GraphicsWnd);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
done:
        SS.DoLater();
    }

    // Write everything back to the registry
    FreezeWindowPos(TextWnd);
    FreezeWindowPos(GraphicsWnd);
    return 0;
}
