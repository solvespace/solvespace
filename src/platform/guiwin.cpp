//-----------------------------------------------------------------------------
// The Win32-based implementation of platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"
// Include after solvespace.h to avoid identifier clashes.
#include <windows.h>

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Windows API bridging
//-----------------------------------------------------------------------------

#define sscheck(expr) do {                                                    \
        SetLastError(0);                                                      \
        if(!(expr))                                                           \
            CheckLastError(__FILE__, __LINE__, __func__, #expr);              \
    } while(0)

void CheckLastError(const char *file, int line, const char *function, const char *expr) {
    if(GetLastError() != S_OK) {
        LPWSTR messageW;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPWSTR)&messageW, 0, NULL);

        std::string message;
        message += ssprintf("File %s, line %u, function %s:\n", file, line, function);
        message += ssprintf("Win32 API call failed: %s.\n", expr);
        message += ssprintf("Error: %s", Narrow(messageW).c_str());
        FatalError(message);
    }
}

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

class TimerImplWin32 : public Timer {
public:
    static HWND WindowHandle() {
        static HWND hTimerWnd;
        if(hTimerWnd == NULL) {
            sscheck(hTimerWnd = CreateWindowExW(0, L"Message", NULL, 0, 0, 0, 0, 0,
                                                HWND_MESSAGE, NULL, NULL, NULL));
        }
        return hTimerWnd;
    }

    static void CALLBACK TimerFunc(HWND hwnd, UINT msg, UINT_PTR event, DWORD time) {
        sscheck(KillTimer(WindowHandle(), event));

        TimerImplWin32 *timer = (TimerImplWin32*)event;
        if(timer->onTimeout) {
            timer->onTimeout();
        }
    }

    void WindUp(unsigned milliseconds) override {
        // FIXME(platform/gui): use SetCoalescableTimer when it's available (8+)
        sscheck(SetTimer(WindowHandle(), (UINT_PTR)this,
                         milliseconds, &TimerImplWin32::TimerFunc));
    }

    ~TimerImplWin32() {
        // FIXME(platform/gui): there's a race condition here--WM_TIMER messages already
        // posted to the queue are not removed, so this destructor is at most "best effort".
        KillTimer(WindowHandle(), (UINT_PTR)this);
    }
};

TimerRef CreateTimer() {
    return std::unique_ptr<TimerImplWin32>(new TimerImplWin32);
}

}
}
