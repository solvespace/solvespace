//-----------------------------------------------------------------------------
// The Cocoa-based implementation of platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#import  <AppKit/AppKit.h>
#include "solvespace.h"

using namespace SolveSpace;

//-----------------------------------------------------------------------------
// Internal AppKit classes
//-----------------------------------------------------------------------------

@interface NSToolTipManager : NSObject
+ (NSToolTipManager *)sharedToolTipManager;
- (void)setInitialToolTipDelay:(double)delay;
- (void)orderOutToolTip;
- (void)_displayTemporaryToolTipForView:(id)arg1 withString:(id)arg2;
@end

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
// Fatal errors
//-----------------------------------------------------------------------------

// This gets put into the "Application Specific Information" field in crash
// reporter dialog.
typedef struct {
  unsigned    version   __attribute__((aligned(8)));
  const char *message   __attribute__((aligned(8)));
  const char *signature __attribute__((aligned(8)));
  const char *backtrace __attribute__((aligned(8)));
  const char *message2  __attribute__((aligned(8)));
  void       *reserved  __attribute__((aligned(8)));
  void       *reserved2 __attribute__((aligned(8)));
} crash_info_t;

#define CRASH_VERSION    4

crash_info_t crashAnnotation __attribute__((section("__DATA,__crash_info"))) = {
    CRASH_VERSION, NULL, NULL, NULL, NULL, NULL, NULL
};

void FatalError(std::string message) {
    crashAnnotation.message = message.c_str();
    abort();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

class SettingsImplCocoa : public Settings {
public:
    NSUserDefaults *userDefaults;

    SettingsImplCocoa() {
        userDefaults = [NSUserDefaults standardUserDefaults];
    }

    void FreezeInt(const std::string &key, uint32_t value) override {
        [userDefaults setInteger:value forKey:Wrap(key)];
    }

    uint32_t ThawInt(const std::string &key, uint32_t defaultValue = 0) override {
        NSString *nsKey = Wrap(key);
        if([userDefaults objectForKey:nsKey]) {
            return [userDefaults integerForKey:nsKey];
        }
        return defaultValue;
    }

    void FreezeBool(const std::string &key, bool value) override {
        [userDefaults setBool:value forKey:Wrap(key)];
    }

    bool ThawBool(const std::string &key, bool defaultValue = false) override {
        NSString *nsKey = Wrap(key);
        if([userDefaults objectForKey:nsKey]) {
            return [userDefaults boolForKey:nsKey];
        }
        return defaultValue;
    }

    void FreezeFloat(const std::string &key, double value) override {
        [userDefaults setDouble:value forKey:Wrap(key)];
    }

    double ThawFloat(const std::string &key, double defaultValue = 0.0) override {
        NSString *nsKey = Wrap(key);
        if([userDefaults objectForKey:nsKey]) {
            return [userDefaults doubleForKey:nsKey];
        }
        return defaultValue;
    }

    void FreezeString(const std::string &key, const std::string &value) override {
        [userDefaults setObject:Wrap(value) forKey:Wrap(key)];
    }

    std::string ThawString(const std::string &key,
                           const std::string &defaultValue = "") override {
        NSObject *nsValue = [userDefaults objectForKey:Wrap(key)];
        if(nsValue && [nsValue isKindOfClass:[NSString class]]) {
            return [(NSString *)nsValue UTF8String];
        }
        return defaultValue;
    }
};

SettingsRef GetSettings() {
    return std::make_shared<SettingsImplCocoa>();
}

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
};

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    static std::shared_ptr<MenuBarImplCocoa> mainMenu;
    if(!mainMenu) {
        mainMenu = std::make_shared<MenuBarImplCocoa>();
    }
    *unique = true;
    return mainMenu;
}

}
}

//-----------------------------------------------------------------------------
// Cocoa NSView and NSWindow extensions
//-----------------------------------------------------------------------------

@interface SSView : NSView
@property Platform::Window *receiver;

@property BOOL acceptsFirstResponder;

@property(readonly, getter=isEditing) BOOL editing;
- (void)startEditing:(NSString *)text at:(NSPoint)origin
        withHeight:(double)fontHeight minWidth:(double)minWidth
        usingMonospace:(BOOL)isMonospace;
- (void)stopEditing;
- (void)didEdit:(NSString *)text;

@property double scrollerMin;
@property double scrollerMax;
@end

@implementation SSView
{
    GlOffscreen         offscreen;
    NSOpenGLContext    *glContext;
    NSTrackingArea     *trackingArea;
    NSTextField        *editor;
}

- (id)initWithFrame:(NSRect)frameRect {
    if(self = [super initWithFrame:frameRect]) {
        self.wantsLayer = YES;

        NSOpenGLPixelFormatAttribute attrs[] = {
            NSOpenGLPFAColorSize, 24,
            NSOpenGLPFADepthSize, 24,
            0
        };
        NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        glContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:NULL];

        editor = [[NSTextField alloc] init];
        editor.editable = YES;
        [[editor cell] setWraps:NO];
        [[editor cell] setScrollable:YES];
        editor.bezeled = NO;
        editor.target = self;
        editor.action = @selector(didEdit:);
    }
    return self;
}

- (void)dealloc {
    offscreen.Clear();
}

- (BOOL)isFlipped {
    return YES;
}

@synthesize receiver;

- (void)drawRect:(NSRect)aRect {
    [glContext makeCurrentContext];

    NSSize size   = [self convertSizeToBacking:self.bounds.size];
    int    width  = (int)size.width,
           height = (int)size.height;
    offscreen.Render(width, height, [&] {
        if(receiver->onRender) {
            receiver->onRender();
        }
    });

    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL, &offscreen.data[0], width * height * 4, NULL);
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(width, height, 8, 32,
        width * 4, colorspace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
        provider, NULL, true, kCGRenderingIntentDefault);

    CGContextDrawImage((CGContextRef) [[NSGraphicsContext currentContext] graphicsPort],
                       [self bounds], image);

    CGImageRelease(image);
    CGDataProviderRelease(provider);
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

- (void)updateTrackingAreas {
    [self removeTrackingArea:trackingArea];
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
        options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                 ([self acceptsFirstResponder]
                  ? NSTrackingActiveInKeyWindow
                  : NSTrackingActiveAlways))
        owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
    [super updateTrackingAreas];
}

- (Platform::MouseEvent)convertMouseEvent:(NSEvent *)nsEvent {
    Platform::MouseEvent event = {};

    NSPoint nsPoint = [self convertPoint:nsEvent.locationInWindow fromView:self];
    event.x = nsPoint.x;
    event.y = self.bounds.size.height - nsPoint.y;

    NSUInteger nsFlags = [nsEvent modifierFlags];
    if(nsFlags & NSShiftKeyMask)   event.shiftDown   = true;
    if(nsFlags & NSCommandKeyMask) event.controlDown = true;

    return event;
}

- (void)mouseMotionEvent:(NSEvent *)nsEvent {
    using Platform::MouseEvent;

    MouseEvent event = [self convertMouseEvent:nsEvent];
    event.type   = MouseEvent::Type::MOTION;
    event.button = MouseEvent::Button::NONE;

    NSUInteger nsButtons = [NSEvent pressedMouseButtons];
    if(nsButtons & (1 << 0)) {
        event.button = MouseEvent::Button::LEFT;
    } else if(nsButtons & (1 << 1)) {
        event.button = MouseEvent::Button::RIGHT;
    } else if(nsButtons & (1 << 2)) {
        event.button = MouseEvent::Button::MIDDLE;
    }

    if(receiver->onMouseEvent) {
        receiver->onMouseEvent(event);
    }
}

- (void)mouseMoved:(NSEvent *)nsEvent {
    [self mouseMotionEvent:nsEvent];
    [super mouseMoved:nsEvent];
}

- (void)mouseDragged:(NSEvent *)nsEvent {
    [self mouseMotionEvent:nsEvent];
}

- (void)otherMouseDragged:(NSEvent *)nsEvent {
    [self mouseMotionEvent:nsEvent];
}

- (void)rightMouseDragged:(NSEvent *)nsEvent {
    [self mouseMotionEvent:nsEvent];
}

- (void)mouseButtonEvent:(NSEvent *)nsEvent withType:(Platform::MouseEvent::Type)type {
    using Platform::MouseEvent;

    MouseEvent event = [self convertMouseEvent:nsEvent];
    event.type = type;
    if([nsEvent buttonNumber] == 0) {
        event.button = MouseEvent::Button::LEFT;
    } else if([nsEvent buttonNumber] == 1) {
        event.button = MouseEvent::Button::RIGHT;
    } else if([nsEvent buttonNumber] == 2) {
        event.button = MouseEvent::Button::MIDDLE;
    } else return;

    if(receiver->onMouseEvent) {
        receiver->onMouseEvent(event);
    }
}

- (void)mouseDownEvent:(NSEvent *)nsEvent {
    using Platform::MouseEvent;

    MouseEvent::Type type;
    if([nsEvent clickCount] == 1) {
        type = MouseEvent::Type::PRESS;
    } else {
        type = MouseEvent::Type::DBL_PRESS;
    }
    [self mouseButtonEvent:nsEvent withType:type];
}

- (void)mouseUpEvent:(NSEvent *)nsEvent {
    using Platform::MouseEvent;

    [self mouseButtonEvent:nsEvent withType:(MouseEvent::Type::RELEASE)];
}

- (void)mouseDown:(NSEvent *)nsEvent {
    [self mouseDownEvent:nsEvent];
}

- (void)otherMouseDown:(NSEvent *)nsEvent {
    [self mouseDownEvent:nsEvent];
}

- (void)rightMouseDown:(NSEvent *)nsEvent {
    [self mouseDownEvent:nsEvent];
    [super rightMouseDown:nsEvent];
}

- (void)mouseUp:(NSEvent *)nsEvent {
    [self mouseUpEvent:nsEvent];
}

- (void)otherMouseUp:(NSEvent *)nsEvent {
    [self mouseUpEvent:nsEvent];
}

- (void)rightMouseUp:(NSEvent *)nsEvent {
    [self mouseUpEvent:nsEvent];
}

- (void)scrollWheel:(NSEvent *)nsEvent {
    using Platform::MouseEvent;

    MouseEvent event = [self convertMouseEvent:nsEvent];
    event.type = MouseEvent::Type::SCROLL_VERT;
    event.scrollDelta = [nsEvent deltaY];

    if(receiver->onMouseEvent) {
        receiver->onMouseEvent(event);
    }
}

- (void)mouseExited:(NSEvent *)nsEvent {
    using Platform::MouseEvent;

    MouseEvent event = [self convertMouseEvent:nsEvent];
    event.type = MouseEvent::Type::LEAVE;

    if(receiver->onMouseEvent) {
        receiver->onMouseEvent(event);
    }
}

- (Platform::KeyboardEvent)convertKeyboardEvent:(NSEvent *)nsEvent {
    using Platform::KeyboardEvent;

    KeyboardEvent event = {};

    NSUInteger nsFlags = [nsEvent modifierFlags];
    if(nsFlags & NSShiftKeyMask)
        event.shiftDown = true;
    if(nsFlags & NSCommandKeyMask)
        event.controlDown = true;

    unichar chr = 0;
    if(NSString *nsChr = [[nsEvent charactersIgnoringModifiers] lowercaseString]) {
        chr = [nsChr characterAtIndex:0];
    }
    if(chr >= NSF1FunctionKey && chr <= NSF12FunctionKey) {
        event.key = KeyboardEvent::Key::FUNCTION;
        event.num = chr - NSF1FunctionKey + 1;
    } else {
        event.key = KeyboardEvent::Key::CHARACTER;
        event.chr = chr;
    }

    return event;
}

- (void)keyDown:(NSEvent *)nsEvent {
    using Platform::KeyboardEvent;

    if([NSEvent modifierFlags] & ~(NSShiftKeyMask|NSCommandKeyMask)) {
        [super keyDown:nsEvent];
        return;
    }

    KeyboardEvent event = [self convertKeyboardEvent:nsEvent];
    event.type = KeyboardEvent::Type::PRESS;

    if(receiver->onKeyboardEvent) {
        receiver->onKeyboardEvent(event);
        return;
    }

    [super keyDown:nsEvent];
}

- (void)keyUp:(NSEvent *)nsEvent {
    using Platform::KeyboardEvent;

    if([NSEvent modifierFlags] & ~(NSShiftKeyMask|NSCommandKeyMask)) {
        [super keyUp:nsEvent];
        return;
    }

    KeyboardEvent event = [self convertKeyboardEvent:nsEvent];
    event.type = KeyboardEvent::Type::RELEASE;

    if(receiver->onKeyboardEvent) {
        receiver->onKeyboardEvent(event);
        return;
    }

    [super keyUp:nsEvent];
}

@synthesize editing;

- (void)startEditing:(NSString *)text at:(NSPoint)origin withHeight:(double)fontHeight
        minWidth:(double)minWidth usingMonospace:(BOOL)isMonospace {
    if(!editing) {
        [self addSubview:editor];
        editing = YES;
    }

    if(isMonospace) {
        editor.font = [NSFont fontWithName:@"Monaco" size:fontHeight];
    } else {
        editor.font = [NSFont controlContentFontOfSize:fontHeight];
    }

    origin.x -= 3; /* left padding; no way to get it from NSTextField */
    origin.y -= [editor intrinsicContentSize].height;
    origin.y += [editor baselineOffsetFromBottom];

    [editor setFrameOrigin:origin];
    [editor setStringValue:text];
    [editor sizeToFit];

    NSSize frameSize = [editor frame].size;
    frameSize.width = std::max(frameSize.width, minWidth);
    [editor setFrameSize:frameSize];

    [[self window] makeFirstResponder:editor];
    [[self window] makeKeyWindow];
}

- (void)stopEditing {
    if(editing) {
        [editor removeFromSuperview];
        [[self window] makeFirstResponder:self];
        editing = NO;
    }
}

- (void)didEdit:(id)sender {
    if(receiver->onEditingDone) {
        receiver->onEditingDone([[editor stringValue] UTF8String]);
    }
}

- (void)cancelOperation:(id)sender {
    using Platform::KeyboardEvent;

    if(receiver->onKeyboardEvent) {
        KeyboardEvent event = {};
        event.key = KeyboardEvent::Key::CHARACTER;
        event.chr = '\e';
        event.type = KeyboardEvent::Type::PRESS;
        receiver->onKeyboardEvent(event);
        event.type = KeyboardEvent::Type::RELEASE;
        receiver->onKeyboardEvent(event);
    }
}

@synthesize scrollerMin;
@synthesize scrollerMax;

- (void)didScroll:(NSScroller *)sender {
    if(receiver->onScrollbarAdjusted) {
        double pos = scrollerMin + [sender doubleValue] * (scrollerMax - scrollerMin);
        receiver->onScrollbarAdjusted(pos);
    }
}
@end

@interface SSWindowDelegate : NSObject<NSWindowDelegate>
@property Platform::Window *receiver;

- (BOOL)windowShouldClose:(id)sender;

@property(readonly, getter=isFullScreen) BOOL fullScreen;
- (void)windowDidEnterFullScreen:(NSNotification *)notification;
- (void)windowDidExitFullScreen:(NSNotification *)notification;
@end

@implementation SSWindowDelegate
@synthesize receiver;

- (BOOL)windowShouldClose:(id)sender {
    if(receiver->onClose) {
        receiver->onClose();
    }
    return NO;
}

@synthesize fullScreen;

- (void)windowDidEnterFullScreen:(NSNotification *)notification {
    fullScreen = true;
    if(receiver->onFullScreen) {
        receiver->onFullScreen(fullScreen);
    }
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
    fullScreen = false;
    if(receiver->onFullScreen) {
        receiver->onFullScreen(fullScreen);
    }
}
@end

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

class WindowImplCocoa : public Window {
public:
    NSWindow         *nsWindow;
    SSWindowDelegate *ssWindowDelegate;
    SSView           *ssView;
    NSScroller       *nsScroller;
    NSView           *nsContainer;

    NSArray *nsConstraintsWithScrollbar;
    NSArray *nsConstraintsWithoutScrollbar;

    double minWidth  = 100.0;
    double minHeight = 100.0;

    NSString         *nsToolTip;

    WindowImplCocoa(Window::Kind kind, std::shared_ptr<WindowImplCocoa> parentWindow) {
        ssView = [[SSView alloc] init];
        ssView.translatesAutoresizingMaskIntoConstraints = NO;
        ssView.receiver = this;

        nsScroller = [[NSScroller alloc] initWithFrame:NSMakeRect(0, 0, 0, 100)];
        nsScroller.translatesAutoresizingMaskIntoConstraints = NO;
        nsScroller.enabled = YES;
        nsScroller.scrollerStyle = NSScrollerStyleOverlay;
        nsScroller.knobStyle = NSScrollerKnobStyleLight;
        nsScroller.action = @selector(didScroll:);
        nsScroller.target = ssView;
        nsScroller.continuous = YES;

        nsContainer = [[NSView alloc] init];
        [nsContainer addSubview:ssView];
        [nsContainer addSubview:nsScroller];

        NSDictionary *views = NSDictionaryOfVariableBindings(ssView, nsScroller);
        nsConstraintsWithoutScrollbar = [NSLayoutConstraint
            constraintsWithVisualFormat:@"H:|[ssView]|"
            options:0 metrics:nil views:views];
        [nsContainer addConstraints:nsConstraintsWithoutScrollbar];
        nsConstraintsWithScrollbar = [NSLayoutConstraint
            constraintsWithVisualFormat:@"H:|[ssView]-0-[nsScroller(11)]|"
            options:0 metrics:nil views:views];
        [nsContainer addConstraints:[NSLayoutConstraint
            constraintsWithVisualFormat:@"V:|[ssView]|"
            options:0 metrics:nil views:views]];
        [nsContainer addConstraints:[NSLayoutConstraint
            constraintsWithVisualFormat:@"V:|[nsScroller]|"
            options:0 metrics:nil views:views]];

        switch(kind) {
            case Window::Kind::TOPLEVEL:
                nsWindow = [[NSWindow alloc] init];
                nsWindow.styleMask = NSTitledWindowMask | NSResizableWindowMask |
                                     NSClosableWindowMask | NSMiniaturizableWindowMask;
                nsWindow.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
                ssView.acceptsFirstResponder = YES;
                break;

            case Window::Kind::TOOL:
                NSPanel *nsPanel = [[NSPanel alloc] init];
                nsPanel.styleMask = NSTitledWindowMask | NSResizableWindowMask |
                                    NSClosableWindowMask | NSUtilityWindowMask;
                [nsPanel standardWindowButton:NSWindowMiniaturizeButton].hidden = YES;
                [nsPanel standardWindowButton:NSWindowZoomButton].hidden = YES;
                nsPanel.floatingPanel = YES;
                nsPanel.becomesKeyOnlyIfNeeded = YES;
                nsWindow = nsPanel;
                break;
        }

        ssWindowDelegate = [[SSWindowDelegate alloc] init];
        ssWindowDelegate.receiver = this;
        nsWindow.delegate = ssWindowDelegate;

        nsWindow.backgroundColor = [NSColor blackColor];
        nsWindow.contentView = nsContainer;
    }

    double GetPixelDensity() override {
        NSDictionary *description = nsWindow.screen.deviceDescription;
        NSSize displayPixelSize = [[description objectForKey:NSDeviceSize] sizeValue];
        CGSize displayPhysicalSize = CGDisplayScreenSize(
                    [[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);
        return (displayPixelSize.width / displayPhysicalSize.width) * 25.4f;
    }

    int GetDevicePixelRatio() override {
        NSSize unitSize = { 1.0f, 0.0f };
        unitSize = [ssView convertSizeToBacking:unitSize];
        return (int)unitSize.width;
    }

    bool IsVisible() override {
        return ![nsWindow isVisible];
    }

    void SetVisible(bool visible) override {
        if(visible) {
            [nsWindow orderFront:nil];
        } else {
            [nsWindow close];
        }
    }

    void Focus() override {
        [nsWindow makeKeyAndOrderFront:nil];
    }

    bool IsFullScreen() override {
        return ssWindowDelegate.fullScreen;
    }

    void SetFullScreen(bool fullScreen) override {
        if(fullScreen != IsFullScreen()) {
            [nsWindow toggleFullScreen:nil];
        }
    }

    void SetTitle(const std::string &title) override {
        nsWindow.representedFilename = @"";
        nsWindow.title = Wrap(title);
    }

    bool SetTitleForFilename(const Path &filename) override {
        [nsWindow setTitleWithRepresentedFilename:Wrap(filename.raw)];
        return true;
    }

    void SetMenuBar(MenuBarRef newMenuBar) override {
        // Doesn't do anything, since we have an unique global menu bar.
    }

    void GetContentSize(double *width, double *height) override {
        NSSize nsSize = [ssView frame].size;
        *width  = nsSize.width;
        *height = nsSize.height;
    }

    void SetMinContentSize(double width, double height) {
        NSSize nsMinSize;
        nsMinSize.width  = width;
        nsMinSize.height = height;
        [nsWindow setContentMinSize:nsMinSize];
        [nsWindow setContentSize:nsMinSize];
    }

    void FreezePosition(SettingsRef _settings, const std::string &key) override {
        [nsWindow saveFrameUsingName:Wrap(key)];
    }

    void ThawPosition(SettingsRef _settings, const std::string &key) override {
        [nsWindow setFrameUsingName:Wrap(key)];
    }

    void SetCursor(Cursor cursor) override {
        NSCursor *nsNewCursor;
        switch(cursor) {
            case Cursor::POINTER: nsNewCursor = [NSCursor arrowCursor];        break;
            case Cursor::HAND:    nsNewCursor = [NSCursor pointingHandCursor]; break;
        }

        if([NSCursor currentCursor] != nsNewCursor) {
            [nsNewCursor set];
        }
    }

    void SetTooltip(const std::string &newText) override {
        NSString *nsNewText = Wrap(newText);
        if(![[ssView toolTip] isEqualToString:nsNewText]) {
            [ssView setToolTip:nsNewText];

            NSToolTipManager *nsToolTipManager = [NSToolTipManager sharedToolTipManager];
            if(newText.empty()) {
                [nsToolTipManager orderOutToolTip];
            } else {
                [nsToolTipManager _displayTemporaryToolTipForView:ssView withString:Wrap(newText)];
            }
        }
    }

    bool IsEditorVisible() override {
        return [ssView isEditing];
    }

    void ShowEditor(double x, double y, double fontHeight, double minWidth,
                    bool isMonospace, const std::string &text) override {
        [ssView startEditing:Wrap(text) at:(NSPoint){(CGFloat)x, (CGFloat)y}
            withHeight:fontHeight minWidth:minWidth usingMonospace:isMonospace];
    }

    void HideEditor() override {
        [ssView stopEditing];
    }

    void SetScrollbarVisible(bool visible) override {
        if(visible) {
            [nsContainer removeConstraints:nsConstraintsWithoutScrollbar];
            [nsContainer addConstraints:nsConstraintsWithScrollbar];
        } else {
            [nsContainer removeConstraints:nsConstraintsWithScrollbar];
            [nsContainer addConstraints:nsConstraintsWithoutScrollbar];
        }
    }

    void ConfigureScrollbar(double min, double max, double pageSize) override {
        ssView.scrollerMin = min;
        ssView.scrollerMax = max - pageSize;
        [nsScroller setKnobProportion:(pageSize / (ssView.scrollerMax - ssView.scrollerMin))];
    }

    double GetScrollbarPosition() override {
        return ssView.scrollerMin +
            [nsScroller doubleValue] * (ssView.scrollerMax - ssView.scrollerMin);
    }

    void SetScrollbarPosition(double pos) override {
        if(pos > ssView.scrollerMax) {
            pos = ssView.scrollerMax;
        }
        [nsScroller setDoubleValue:(pos / (ssView.scrollerMax - ssView.scrollerMin))];
        if(onScrollbarAdjusted) {
            onScrollbarAdjusted(pos);
        }
    }

    void Invalidate() override {
        ssView.needsDisplay = YES;
    }

    void Redraw() override {
        Invalidate();
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, YES);
    }

    void *NativePtr() override {
        return (__bridge void *)ssView;
    }
};

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    return std::make_shared<WindowImplCocoa>(kind,
                std::static_pointer_cast<WindowImplCocoa>(parentWindow));
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

void Exit() {
    [NSApp setDelegate:nil];
    [NSApp terminate:nil];
}

}
}
