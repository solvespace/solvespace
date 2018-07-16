//-----------------------------------------------------------------------------
// Our WinMain() functions, and Win32-specific stuff to set up our windows
// and otherwise handle our interface to the operating system. Everything
// outside platform/... should be standard C++ and gl.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include <time.h>

#include "config.h"
#include "solvespace.h"

// Include after solvespace.h to avoid identifier clashes.
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>

#ifdef HAVE_SPACEWARE
#   include <si.h>
#   include <siapp.h>
#   undef uint32_t  // thanks but no thanks
#endif

using Platform::Narrow;
using Platform::Widen;

HFONT FixedFont;

#ifdef HAVE_SPACEWARE
// The 6-DOF input device.
SiHdl SpaceNavigator = SI_NO_HANDLE;
#endif

//-----------------------------------------------------------------------------
// Utility routines
//-----------------------------------------------------------------------------
std::wstring Title(const std::string &s) {
    return Widen("SolveSpace - " + s);
}

//-----------------------------------------------------------------------------
// Routines to display message boxes on screen. Do our own, instead of using
// MessageBox, because that is not consistent from version to version and
// there's word wrap problems.
//-----------------------------------------------------------------------------

HWND MessageWnd, OkButton;
bool MessageDone;
int MessageWidth, MessageHeight;
const char *MessageString;

static LRESULT CALLBACK MessageProc(HWND hwnd, UINT msg, WPARAM wParam,
    LPARAM lParam)
{
    switch (msg) {
        case WM_COMMAND:
            if((HWND)lParam == OkButton && wParam == BN_CLICKED) {
                MessageDone = true;
            }
            break;

        case WM_CLOSE:
        case WM_DESTROY:
            MessageDone = true;
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            SelectObject(hdc, FixedFont);
            SetTextColor(hdc, 0x000000);
            SetBkMode(hdc, TRANSPARENT);
            RECT rc;
            SetRect(&rc, 10, 10, MessageWidth, MessageHeight);
            std::wstring text = Widen(MessageString);
            DrawText(hdc, text.c_str(), text.length(), &rc, DT_LEFT | DT_WORDBREAK);
            EndPaint(hwnd, &ps);
            break;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 1;
}

HWND CreateWindowClient(DWORD exStyle, const wchar_t *className, const wchar_t *windowName,
    DWORD style, int x, int y, int width, int height, HWND parent,
    HMENU menu, HINSTANCE instance, void *param)
{
    HWND h = CreateWindowExW(exStyle, className, windowName, style, x, y,
        width, height, parent, menu, instance, param);

    RECT r;
    GetClientRect(h, &r);
    width = width - (r.right - width);
    height = height - (r.bottom - height);

    SetWindowPos(h, HWND_TOP, x, y, width, height, 0);

    return h;
}

void SolveSpace::DoMessageBox(const char *str, int rows, int cols, bool error)
{
    EnableWindow((HWND)SS.GW.window->NativePtr(), FALSE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), FALSE);

    // Register the window class for our dialog.
    WNDCLASSEX wc = {};
    wc.cbSize           = sizeof(wc);
    wc.style            = CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW | CS_OWNDC;
    wc.lpfnWndProc      = (WNDPROC)MessageProc;
    wc.hInstance        = NULL;
    wc.hbrBackground    = (HBRUSH)COLOR_BTNSHADOW;
    wc.lpszClassName    = L"MessageWnd";
    wc.lpszMenuName     = NULL;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon            = (HICON)LoadImage(NULL, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 32, 32, 0);
    wc.hIconSm          = (HICON)LoadImage(NULL, MAKEINTRESOURCE(4000),
                            IMAGE_ICON, 16, 16, 0);
    RegisterClassEx(&wc);

    // Create the window.
    MessageString = str;
    RECT r;
    GetWindowRect((HWND)SS.GW.window->NativePtr(), &r);
    int width  = cols*SS.TW.CHAR_WIDTH_ + 20,
        height = rows*SS.TW.LINE_HEIGHT + 60;
    MessageWidth = width;
    MessageHeight = height;
    MessageWnd = CreateWindowClient(0, L"MessageWnd",
        Title(error ? C_("title", "Error") : C_("title", "Message")).c_str(),
        WS_OVERLAPPED | WS_SYSMENU,
        r.left + 100, r.top + 100, width, height, NULL, NULL, NULL, NULL);

    OkButton = CreateWindowExW(0, WC_BUTTON, Widen(C_("button", "OK")).c_str(),
        WS_CHILD | WS_TABSTOP | WS_CLIPSIBLINGS | WS_VISIBLE | BS_DEFPUSHBUTTON,
        (width - 70)/2, rows*SS.TW.LINE_HEIGHT + 20,
        70, 25, MessageWnd, NULL, NULL, NULL);
    SendMessage(OkButton, WM_SETFONT, (WPARAM)FixedFont, true);

    ShowWindow(MessageWnd, true);
    SetFocus(OkButton);

    MSG msg;
    DWORD ret;
    MessageDone = false;
    while((ret = GetMessage(&msg, NULL, 0, 0)) != 0 && !MessageDone) {
        if((msg.message == WM_KEYDOWN &&
               (msg.wParam == VK_RETURN ||
                msg.wParam == VK_ESCAPE)) ||
            (msg.message == WM_KEYUP &&
               (msg.wParam == VK_SPACE)))
        {
            MessageDone = true;
            break;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    MessageString = NULL;
    DestroyWindow(MessageWnd);

    EnableWindow((HWND)SS.GW.window->NativePtr(), TRUE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), TRUE);
    SetForegroundWindow((HWND)SS.GW.window->NativePtr());
}

void SolveSpace::OpenWebsite(const char *url) {
    ShellExecuteW((HWND)SS.GW.window->NativePtr(),
                  L"open", Widen(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
}

//-----------------------------------------------------------------------------
// Common dialog routines, to open or save a file.
//-----------------------------------------------------------------------------
static std::string ConvertFilters(const FileFilter ssFilters[]) {
    std::string filter;
    for(const FileFilter *ssFilter = ssFilters; ssFilter->name; ssFilter++) {
        std::string desc, patterns;
        for(const char *const *ssPattern = ssFilter->patterns; *ssPattern; ssPattern++) {
            std::string pattern = "*." + std::string(*ssPattern);
            if(desc == "")
                desc = pattern;
            else
                desc += ", " + pattern;
            if(patterns == "")
                patterns = pattern;
            else
                patterns += ";" + pattern;
        }
        filter += std::string(ssFilter->name) + " (" + desc + ")" + '\0';
        filter += patterns + '\0';
    }
    filter += '\0';
    return filter;
}

static bool OpenSaveFile(bool isOpen, Platform::Path *filename, const std::string &defExtension,
                         const FileFilter filters[]) {
    std::string activeExtension = defExtension;
    if(activeExtension == "") {
        activeExtension = filters[0].patterns[0];
    }

    std::wstring initialFilenameW;
    if(filename->IsEmpty()) {
        initialFilenameW = Widen("untitled");
    } else {
        initialFilenameW = Widen(filename->Parent().Join(filename->FileStem()).raw);
    }
    std::wstring selPatternW = Widen(ConvertFilters(filters));
    std::wstring defExtensionW = Widen(defExtension);

    // UNC paths may be as long as 32767 characters.
    // Unfortunately, the Get*FileName API does not provide any way to use it
    // except with a preallocated buffer of fixed size, so we use something
    // reasonably large.
    const int len = 32768;
    wchar_t filenameC[len] = {};
    wcsncpy(filenameC, initialFilenameW.c_str(), len - 1);

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hInstance = NULL;
    ofn.hwndOwner = (HWND)SS.GW.window->NativePtr();
    ofn.lpstrFilter = selPatternW.c_str();
    ofn.lpstrDefExt = defExtensionW.c_str();
    ofn.lpstrFile = filenameC;
    ofn.nMaxFile = len;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    EnableWindow((HWND)SS.GW.window->NativePtr(), FALSE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), FALSE);

    BOOL r;
    if(isOpen) {
        r = GetOpenFileNameW(&ofn);
    } else {
        r = GetSaveFileNameW(&ofn);
    }

    EnableWindow((HWND)SS.GW.window->NativePtr(), TRUE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), TRUE);
    SetForegroundWindow((HWND)SS.GW.window->NativePtr());

    if(r) *filename = Platform::Path::From(Narrow(filenameC));
    return r ? true : false;
}

bool SolveSpace::GetOpenFile(Platform::Path *filename, const std::string &defExtension,
                             const FileFilter filters[])
{
    return OpenSaveFile(/*isOpen=*/true, filename, defExtension, filters);
}

bool SolveSpace::GetSaveFile(Platform::Path *filename, const std::string &defExtension,
                             const FileFilter filters[])
{
    return OpenSaveFile(/*isOpen=*/false, filename, defExtension, filters);
}

DialogChoice SolveSpace::SaveFileYesNoCancel()
{
    EnableWindow((HWND)SS.GW.window->NativePtr(), FALSE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), FALSE);

    int r = MessageBoxW((HWND)SS.GW.window->NativePtr(),
        Widen(_("The file has changed since it was last saved.\n\n"
                "Do you want to save the changes?")).c_str(),
        Title(C_("title", "Modified File")).c_str(),
        MB_YESNOCANCEL | MB_ICONWARNING);

    EnableWindow((HWND)SS.GW.window->NativePtr(), TRUE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), TRUE);
    SetForegroundWindow((HWND)SS.GW.window->NativePtr());

    switch(r) {
        case IDYES:
        return DIALOG_YES;
        case IDNO:
        return DIALOG_NO;
        case IDCANCEL:
        default:
        return DIALOG_CANCEL;
    }
}

DialogChoice SolveSpace::LoadAutosaveYesNo()
{
    EnableWindow((HWND)SS.GW.window->NativePtr(), FALSE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), FALSE);

    int r = MessageBoxW((HWND)SS.GW.window->NativePtr(),
        Widen(_("An autosave file is available for this project.\n\n"
                "Do you want to load the autosave file instead?")).c_str(),
        Title(C_("title", "Autosave Available")).c_str(),
        MB_YESNO | MB_ICONWARNING);

    EnableWindow((HWND)SS.GW.window->NativePtr(), TRUE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), TRUE);
    SetForegroundWindow((HWND)SS.GW.window->NativePtr());

    switch (r) {
        case IDYES:
        return DIALOG_YES;
        case IDNO:
        default:
        return DIALOG_NO;
    }
}

DialogChoice SolveSpace::LocateImportedFileYesNoCancel(const Platform::Path &filename,
                                                       bool canCancel) {
    EnableWindow((HWND)SS.GW.window->NativePtr(), FALSE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), FALSE);

    std::string message =
        "The linked file " + filename.raw + " is not present.\n\n"
        "Do you want to locate it manually?\n\n"
        "If you select \"No\", any geometry that depends on "
        "the missing file will be removed.";

    int r = MessageBoxW((HWND)SS.GW.window->NativePtr(),
        Widen(message).c_str(),
        Title(C_("title", "Missing File")).c_str(),
        (canCancel ? MB_YESNOCANCEL : MB_YESNO) | MB_ICONWARNING);

    EnableWindow((HWND)SS.GW.window->NativePtr(), TRUE);
    EnableWindow((HWND)SS.TW.window->NativePtr(), TRUE);
    SetForegroundWindow((HWND)SS.GW.window->NativePtr());

    switch(r) {
        case IDYES:
        return DIALOG_YES;
        case IDNO:
        return DIALOG_NO;
        case IDCANCEL:
        default:
        return DIALOG_CANCEL;
    }
}

std::vector<Platform::Path> SolveSpace::GetFontFiles() {
    std::vector<Platform::Path> fonts;

    std::wstring fontsDirW(MAX_PATH, '\0');
    fontsDirW.resize(GetWindowsDirectoryW(&fontsDirW[0], fontsDirW.length()));
    fontsDirW += L"\\fonts\\";
    Platform::Path fontsDir = Platform::Path::From(Narrow(fontsDirW));

    WIN32_FIND_DATA wfd;
    HANDLE h = FindFirstFileW((fontsDirW + L"*").c_str(), &wfd);
    while(h != INVALID_HANDLE_VALUE) {
        fonts.push_back(fontsDir.Join(Narrow(wfd.cFileName)));
        if(!FindNextFileW(h, &wfd)) break;
    }

    return fonts;
}

#ifdef HAVE_SPACEWARE
//-----------------------------------------------------------------------------
// Test if a message comes from the SpaceNavigator device. If yes, dispatch
// it appropriately and return true. Otherwise, do nothing and return false.
//-----------------------------------------------------------------------------
static bool ProcessSpaceNavigatorMsg(MSG *msg) {
    if(SpaceNavigator == SI_NO_HANDLE) return false;

    SiGetEventData sged;
    SiSpwEvent sse;

    SiGetEventWinInit(&sged, msg->message, msg->wParam, msg->lParam);
    int ret = SiGetEvent(SpaceNavigator, 0, &sged, &sse);
    if(ret == SI_NOT_EVENT) return false;
    // So the device is a SpaceNavigator event, or a SpaceNavigator error.

    if(ret == SI_IS_EVENT) {
        if(sse.type == SI_MOTION_EVENT) {
            // The Z axis translation and rotation are both
            // backwards in the default mapping.
            double tx =  sse.u.spwData.mData[SI_TX]*1.0,
                   ty =  sse.u.spwData.mData[SI_TY]*1.0,
                   tz = -sse.u.spwData.mData[SI_TZ]*1.0,
                   rx =  sse.u.spwData.mData[SI_RX]*0.001,
                   ry =  sse.u.spwData.mData[SI_RY]*0.001,
                   rz = -sse.u.spwData.mData[SI_RZ]*0.001;
            SS.GW.SpaceNavigatorMoved(tx, ty, tz, rx, ry, rz,
                !!(GetAsyncKeyState(VK_SHIFT) & 0x8000));
        } else if(sse.type == SI_BUTTON_EVENT) {
            int button;
            button = SiButtonReleased(&sse);
            if(button == SI_APP_FIT_BUTTON) SS.GW.SpaceNavigatorButtonUp();
        }
    }
    return true;
}
#endif // HAVE_SPACEWARE

//-----------------------------------------------------------------------------
// Entry point into the program.
//-----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, INT nCmdShow)
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_STANDARD_CLASSES|ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // A monospaced font
    FixedFont = CreateFontW(SS.TW.CHAR_HEIGHT, SS.TW.CHAR_WIDTH_, 0, 0,
        FW_REGULAR, false,
        false, false, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FF_DONTCARE, L"Lucida Console");
    if(!FixedFont)
        FixedFont = (HFONT)GetStockObject(SYSTEM_FONT);

    std::vector<std::string> args = InitPlatform(0, NULL);

#ifdef HAVE_SPACEWARE
    // Initialize the SpaceBall, if present. Test if the driver is running
    // first, to avoid a long timeout if it's not.
    HWND swdc = FindWindowW(L"SpaceWare Driver Class", NULL);
    if(swdc != NULL) {
        SiOpenData sod;
        SiInitialize();
        SiOpenWinInit(&sod, (HWND)SS.GW.window->NativePtr());
        SpaceNavigator = SiOpen("GraphicsWnd", SI_ANY_DEVICE, SI_NO_MASK, SI_EVENT, &sod);
        SiSetUiMode(SpaceNavigator, SI_UI_NO_CONTROLS);
    }
#endif

    // Use the user default locale, then fall back to English.
    if(!SetLocale((uint16_t)GetUserDefaultLCID())) {
        SetLocale("en_US");
    }

    // Call in to the platform-independent code, and let them do their init
    SS.Init();

    // A filename may have been specified on the command line.
    if(args.size() >= 2) {
        SS.Load(Platform::Path::From(args[1]).Expand(/*fromCurrentDirectory=*/true));
    }

    // And now it's the message loop. All calls in to the rest of the code
    // will be from the wndprocs.
    MSG msg;
    DWORD ret;
    while((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
#ifdef HAVE_SPACEWARE
        // Is it a message from the six degree of freedom input device?
        if(ProcessSpaceNavigatorMsg(&msg)) continue;
#endif

        // None of the above; so just a normal message to process.
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

#ifdef HAVE_SPACEWARE
    if(swdc != NULL) {
        if(SpaceNavigator != SI_NO_HANDLE) SiClose(SpaceNavigator);
        SiTerminate();
    }
#endif

    // Free the memory we've used; anything that remains is a leak.
    SK.Clear();
    SS.Clear();

    return 0;
}
