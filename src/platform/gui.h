//-----------------------------------------------------------------------------
// An abstraction for platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_GUI_H
#define SOLVESPACE_GUI_H

namespace Platform {

//-----------------------------------------------------------------------------
// Events
//-----------------------------------------------------------------------------

// A keyboard input event.
struct KeyboardEvent {
    enum class Type {
        PRESS,
        RELEASE,
    };

    enum class Key {
        CHARACTER,
        FUNCTION,
    };

    Type        type;
    Key         key;
    union {
        char32_t    chr; // for Key::CHARACTER
        int         num; // for Key::FUNCTION
    };
    bool        shiftDown;
    bool        controlDown;

    bool Equals(const KeyboardEvent &other) {
        return type == other.type && key == other.key && num == other.num &&
            shiftDown == other.shiftDown && controlDown == other.controlDown;
    }
};

std::string AcceleratorDescription(const KeyboardEvent &accel);

//-----------------------------------------------------------------------------
// Interfaces
//-----------------------------------------------------------------------------

// A native single-shot timer.
class Timer {
public:
    std::function<void()>   onTimeout;

    virtual ~Timer() {}

    virtual void WindUp(unsigned milliseconds) = 0;
};

typedef std::unique_ptr<Timer> TimerRef;

TimerRef CreateTimer();

// A native menu item.
class MenuItem {
public:
    enum class Indicator {
        NONE,
        CHECK_MARK,
        RADIO_MARK,
    };

    std::function<void()>   onTrigger;

    virtual ~MenuItem() {}

    virtual void SetAccelerator(KeyboardEvent accel) = 0;
    virtual void SetIndicator(Indicator type) = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual void SetActive(bool active) = 0;
};

typedef std::shared_ptr<MenuItem> MenuItemRef;

// A native menu.
class Menu {
public:
    virtual ~Menu() {}

    virtual std::shared_ptr<MenuItem> AddItem(
        const std::string &label, std::function<void()> onTrigger = std::function<void()>()) = 0;
    virtual std::shared_ptr<Menu> AddSubMenu(const std::string &label) = 0;
    virtual void AddSeparator() = 0;

    virtual void PopUp() = 0;

    virtual void Clear() = 0;
};

typedef std::shared_ptr<Menu> MenuRef;

// A native menu bar.
class MenuBar {
public:
    virtual ~MenuBar() {}

    virtual std::shared_ptr<Menu> AddSubMenu(const std::string &label) = 0;

    virtual void Clear() = 0;
    virtual void *NativePtr() = 0;
};

typedef std::shared_ptr<MenuBar> MenuBarRef;

MenuRef CreateMenu();
MenuBarRef GetOrCreateMainMenu(bool *unique);

}

#endif
