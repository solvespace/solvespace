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

using Platform::Narrow;
using Platform::Widen;

//-----------------------------------------------------------------------------
// Utility routines
//-----------------------------------------------------------------------------
void SolveSpace::OpenWebsite(const char *url) {
    ShellExecuteW((HWND)SS.GW.window->NativePtr(),
                  L"open", Widen(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
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

    std::vector<std::string> args = InitPlatform(0, NULL);

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
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Free the memory we've used; anything that remains is a leak.
    SK.Clear();
    SS.Clear();

    return 0;
}
