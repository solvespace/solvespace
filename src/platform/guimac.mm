//-----------------------------------------------------------------------------
// The Cocoa-based implementation of platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#import  <AppKit/AppKit.h>
#include "solvespace.h"

//-----------------------------------------------------------------------------
// Objective-C bridging
//-----------------------------------------------------------------------------

static NSString* Wrap(const std::string &s) {
    return [NSString stringWithUTF8String:s.c_str()];
}

@interface SSFunction : NSObject
- (SSFunction *)initWithFunction:(std::function<void ()> *)aFunc;
- (void)run;
@end

@implementation SSFunction
{
    std::function<void ()> *func;
}

- (SSFunction *)initWithFunction:(std::function<void ()> *)aFunc {
    if(self = [super init]) {
        func = aFunc;
    }
    return self;
}

- (void)run {
    if(*func) (*func)();
}
@end

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

class TimerImplCocoa : public Timer {
public:
    NSTimer *timer;

    TimerImplCocoa() : timer(NULL) {}

    void WindUp(unsigned milliseconds) override {
        SSFunction *callback = [[SSFunction alloc] initWithFunction:&this->onTimeout];
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:
            [callback methodSignatureForSelector:@selector(run)]];
        invocation.target = callback;
        invocation.selector = @selector(run);

        if(timer != NULL) {
            [timer invalidate];
        }
        timer = [NSTimer scheduledTimerWithTimeInterval:(milliseconds / 1000.0)
            invocation:invocation repeats:NO];
    }

    ~TimerImplCocoa() {
        if(timer != NULL) {
            [timer invalidate];
        }
    }
};

TimerRef CreateTimer() {
    return std::unique_ptr<TimerImplCocoa>(new TimerImplCocoa);
}

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

static std::string PrepareMenuLabel(std::string label) {
    // OS X does not support mnemonics
    label.erase(std::remove(label.begin(), label.end(), '&'), label.end());
    return label;
}

class MenuItemImplCocoa : public MenuItem {
public:
    SSFunction *ssFunction;
    NSMenuItem *nsMenuItem;

    MenuItemImplCocoa() {
        ssFunction = [[SSFunction alloc] initWithFunction:&onTrigger];
        nsMenuItem = [[NSMenuItem alloc] initWithTitle:@""
            action:@selector(run) keyEquivalent:@""];
        nsMenuItem.target = ssFunction;
    }

    void SetAccelerator(KeyboardEvent accel) override {
        unichar accelChar;
        switch(accel.key) {
            case KeyboardEvent::Key::CHARACTER:
                if(accel.chr == NSDeleteCharacter) {
                    accelChar = NSBackspaceCharacter;
                } else {
                    accelChar = accel.chr;
                }
                break;

            case KeyboardEvent::Key::FUNCTION:
                accelChar = NSF1FunctionKey + accel.num - 1;
                break;
        }
        nsMenuItem.keyEquivalent = [[NSString alloc] initWithCharacters:&accelChar length:1];

        NSUInteger modifierMask = 0;
        if(accel.shiftDown)
            modifierMask |= NSShiftKeyMask;
        if(accel.controlDown)
            modifierMask |= NSCommandKeyMask;
        nsMenuItem.keyEquivalentModifierMask = modifierMask;
    }

    void SetIndicator(Indicator state) override {
        // macOS does not support radio menu items
    }

    void SetActive(bool active) override {
        nsMenuItem.state = active ? NSOnState : NSOffState;
    }

    void SetEnabled(bool enabled) override {
        nsMenuItem.enabled = enabled;
    }
};

class MenuImplCocoa : public Menu {
public:
    NSMenu *nsMenu;

    std::vector<std::shared_ptr<MenuItemImplCocoa>> menuItems;
    std::vector<std::shared_ptr<MenuImplCocoa>>     subMenus;

    MenuImplCocoa() {
        nsMenu = [[NSMenu alloc] initWithTitle:@""];
        [nsMenu setAutoenablesItems:NO];
    }

    MenuItemRef AddItem(const std::string &label,
                        std::function<void()> onTrigger = NULL) override {
        auto menuItem = std::make_shared<MenuItemImplCocoa>();
        menuItems.push_back(menuItem);

        menuItem->onTrigger = onTrigger;
        [menuItem->nsMenuItem setTitle:Wrap(PrepareMenuLabel(label))];
        [nsMenu addItem:menuItem->nsMenuItem];

        return menuItem;
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplCocoa>();
        subMenus.push_back(subMenu);

        NSMenuItem *nsMenuItem =
            [nsMenu addItemWithTitle:Wrap(PrepareMenuLabel(label)) action:nil keyEquivalent:@""];
        [nsMenu setSubmenu:subMenu->nsMenu forItem:nsMenuItem];

        return subMenu;
    }

    void AddSeparator() override {
        [nsMenu addItem:[NSMenuItem separatorItem]];
    }

    void PopUp() override {
        [NSMenu popUpContextMenu:nsMenu withEvent:[NSApp currentEvent] forView:nil];
    }

    void Clear() override {
        [nsMenu removeAllItems];
        menuItems.clear();
        subMenus.clear();
    }
};

MenuRef CreateMenu() {
    return std::make_shared<MenuImplCocoa>();
}

class MenuBarImplCocoa : public MenuBar {
public:
    NSMenu *nsMenuBar;

    std::vector<std::shared_ptr<MenuImplCocoa>> subMenus;

    MenuBarImplCocoa() {
        nsMenuBar = [NSApp mainMenu];
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplCocoa>();
        subMenus.push_back(subMenu);

        NSMenuItem *nsMenuItem = [nsMenuBar addItemWithTitle:@"" action:nil keyEquivalent:@""];
        [subMenu->nsMenu setTitle:Wrap(PrepareMenuLabel(label))];
        [nsMenuBar setSubmenu:subMenu->nsMenu forItem:nsMenuItem];

        return subMenu;
    }

    void Clear() override {
        while([nsMenuBar numberOfItems] != 1) {
            [nsMenuBar removeItemAtIndex:1];
        }
        subMenus.clear();
    }

    void *NativePtr() override {
        return NULL;
    }
};

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    static std::shared_ptr<MenuBarImplCocoa> mainMenu;
    if(!mainMenu) {
        mainMenu = std::make_shared<MenuBarImplCocoa>();
    }
    *unique = true;
    return mainMenu;
}

void SetMainMenu(MenuBarRef menuBar) {
}

}
}
