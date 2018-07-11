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

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

class MenuImplWin32;

class MenuItemImplWin32 : public MenuItem {
public:
    std::shared_ptr<MenuImplWin32> menu;

    HMENU Handle();

    MENUITEMINFOW GetInfo(UINT mask) {
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask  = mask;
        sscheck(GetMenuItemInfoW(Handle(), (UINT_PTR)this, FALSE, &mii));
        return mii;
    }

    void SetAccelerator(KeyboardEvent accel) override {
        MENUITEMINFOW mii = GetInfo(MIIM_TYPE);

        std::wstring nameW(mii.cch, L'\0');
        mii.dwTypeData = &nameW[0];
        mii.cch++;
        sscheck(GetMenuItemInfoW(Handle(), (UINT_PTR)this, FALSE, &mii));

        std::string name = Narrow(nameW);
        if(name.find('\t') != std::string::npos) {
            name = name.substr(0, name.find('\t'));
        }
        name += '\t';
        name += AcceleratorDescription(accel);

        nameW = Widen(name);
        mii.fMask      = MIIM_STRING;
        mii.dwTypeData = &nameW[0];
        sscheck(SetMenuItemInfoW(Handle(), (UINT_PTR)this, FALSE, &mii));
    }

    void SetIndicator(Indicator type) override {
        MENUITEMINFOW mii = GetInfo(MIIM_FTYPE);
        switch(type) {
            case Indicator::NONE:
            case Indicator::CHECK_MARK:
                mii.fType &= ~MFT_RADIOCHECK;
                break;

            case Indicator::RADIO_MARK:
                mii.fType |= MFT_RADIOCHECK;
                break;
        }
        sscheck(SetMenuItemInfoW(Handle(), (UINT_PTR)this, FALSE, &mii));
    }

    void SetActive(bool active) override {
        MENUITEMINFOW mii = GetInfo(MIIM_STATE);
        if(active) {
            mii.fState |= MFS_CHECKED;
        } else {
            mii.fState &= ~MFS_CHECKED;
        }
        sscheck(SetMenuItemInfoW(Handle(), (UINT_PTR)this, FALSE, &mii));
    }

    void SetEnabled(bool enabled) override {
        MENUITEMINFOW mii = GetInfo(MIIM_STATE);
        if(enabled) {
            mii.fState &= ~(MFS_DISABLED|MFS_GRAYED);
        } else {
            mii.fState |= MFS_DISABLED|MFS_GRAYED;
        }
        sscheck(SetMenuItemInfoW(Handle(), (UINT_PTR)this, FALSE, &mii));
    }
};

void TriggerMenu(int id) {
    MenuItemImplWin32 *menuItem = (MenuItemImplWin32 *)id;
    if(menuItem->onTrigger) {
        menuItem->onTrigger();
    }
}

int64_t contextMenuCancelTime = 0;

class MenuImplWin32 : public Menu {
public:
    HMENU hMenu;

    std::weak_ptr<MenuImplWin32> weakThis;
    std::vector<std::shared_ptr<MenuItemImplWin32>> menuItems;
    std::vector<std::shared_ptr<MenuImplWin32>>     subMenus;

    MenuImplWin32() {
        sscheck(hMenu = CreatePopupMenu());

        MENUINFO mi = {};
        mi.cbSize  = sizeof(mi);
        mi.fMask   = MIM_STYLE;
        mi.dwStyle = MNS_NOTIFYBYPOS;
        sscheck(SetMenuInfo(hMenu, &mi));
    }

    MenuItemRef AddItem(const std::string &label,
                        std::function<void()> onTrigger = NULL) override {
        auto menuItem = std::make_shared<MenuItemImplWin32>();
        menuItem->menu = weakThis.lock();
        menuItem->onTrigger = onTrigger;
        menuItems.push_back(menuItem);

        sscheck(AppendMenuW(hMenu, MF_STRING, (UINT_PTR)&*menuItem, Widen(label).c_str()));

        return menuItem;
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplWin32>();
        subMenu->weakThis = subMenu;
        subMenus.push_back(subMenu);

        sscheck(AppendMenuW(hMenu, MF_STRING|MF_POPUP,
                            (UINT_PTR)subMenu->hMenu, Widen(label).c_str()));

        return subMenu;
    }

    void AddSeparator() override {
        sscheck(AppendMenuW(hMenu, MF_SEPARATOR, 0, L""));
    }

    void PopUp() override {
        POINT pt;
        sscheck(GetCursorPos(&pt));
        int id = TrackPopupMenu(hMenu, TPM_TOPALIGN|TPM_RIGHTBUTTON|TPM_RETURNCMD,
                                pt.x, pt.y, 0, GetActiveWindow(), NULL);
        if(id == 0) {
            contextMenuCancelTime = GetMilliseconds();
        } else {
            TriggerMenu(id);
        }
    }

    void Clear() override {
        for(int n = GetMenuItemCount(hMenu) - 1; n >= 0; n--) {
            sscheck(RemoveMenu(hMenu, n, MF_BYPOSITION));
        }
        menuItems.clear();
        subMenus.clear();
    }

    ~MenuImplWin32() {
        Clear();
        sscheck(DestroyMenu(hMenu));
    }
};

HMENU MenuItemImplWin32::Handle() {
    return menu->hMenu;
}

MenuRef CreateMenu() {
    auto menu = std::make_shared<MenuImplWin32>();
    // std::enable_shared_from_this fails for some reason, not sure why
    menu->weakThis = menu;
    return menu;
}

class MenuBarImplWin32 : public MenuBar {
public:
    HMENU hMenuBar;

    std::vector<std::shared_ptr<MenuImplWin32>> subMenus;

    MenuBarImplWin32() {
        sscheck(hMenuBar = ::CreateMenu());
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplWin32>();
        subMenu->weakThis = subMenu;
        subMenus.push_back(subMenu);

        sscheck(AppendMenuW(hMenuBar, MF_STRING|MF_POPUP,
                            (UINT_PTR)subMenu->hMenu, Widen(label).c_str()));

        return subMenu;
    }

    void Clear() override {
        for(int n = GetMenuItemCount(hMenuBar) - 1; n >= 0; n--) {
            sscheck(RemoveMenu(hMenuBar, n, MF_BYPOSITION));
        }
        subMenus.clear();
    }

    ~MenuBarImplWin32() {
        Clear();
        sscheck(DestroyMenu(hMenuBar));
    }

    void *NativePtr() override {
        return hMenuBar;
    }
};

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    *unique = false;
    return std::make_shared<MenuBarImplWin32>();
}

}
}
