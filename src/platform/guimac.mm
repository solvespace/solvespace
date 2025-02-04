//-----------------------------------------------------------------------------
// The Cocoa-based implementation of platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"
#import  <AppKit/AppKit.h>

using namespace SolveSpace;

//-----------------------------------------------------------------------------
// Internal AppKit classes
//-----------------------------------------------------------------------------

@interface NSToolTipManager : NSObject
+ (NSToolTipManager *)sharedToolTipManager;
- (void)setInitialToolTipDelay:(double)delay;
- (void)orderOutToolTip;
- (void)abortToolTip;
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
// Utility functions
//-----------------------------------------------------------------------------

static std::string PrepareMnemonics(std::string label) {
    // OS X does not support mnemonics
    label.erase(std::remove(label.begin(), label.end(), '&'), label.end());
    return label;
}

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

void FatalError(const std::string &message) {
    crashAnnotation.message = message.c_str();
    abort();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

class SettingsImplCocoa final : public Settings {
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

class TimerImplCocoa final : public Timer {
public:
    NSTimer *timer;

    TimerImplCocoa() : timer(NULL) {}

    void RunAfter(unsigned milliseconds) override {
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
    return std::make_shared<TimerImplCocoa>();
}

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

class MenuItemImplCocoa final : public MenuItem {
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
            modifierMask |= NSEventModifierFlagShift;
        if(accel.controlDown)
            modifierMask |= NSEventModifierFlagCommand;
        nsMenuItem.keyEquivalentModifierMask = modifierMask;
    }

    void SetIndicator(Indicator state) override {
        // macOS does not support radio menu items
    }

    void SetActive(bool active) override {
        nsMenuItem.state = active ? NSControlStateValueOn : NSControlStateValueOff;
    }

    void SetEnabled(bool enabled) override {
        nsMenuItem.enabled = enabled;
    }
};

class MenuImplCocoa final : public Menu {
public:
    NSMenu *nsMenu;

    std::vector<std::shared_ptr<MenuItemImplCocoa>> menuItems;
    std::vector<std::shared_ptr<MenuImplCocoa>>     subMenus;

    MenuImplCocoa() {
        nsMenu = [[NSMenu alloc] initWithTitle:@""];
        [nsMenu setAutoenablesItems:NO];
    }

    MenuItemRef AddItem(const std::string &label,
                        std::function<void()> onTrigger = NULL,
                        bool mnemonics = true) override {
        auto menuItem = std::make_shared<MenuItemImplCocoa>();
        menuItems.push_back(menuItem);

        menuItem->onTrigger = onTrigger;
        [menuItem->nsMenuItem setTitle:Wrap(mnemonics ? PrepareMnemonics(label) : label)];
        [nsMenu addItem:menuItem->nsMenuItem];

        return menuItem;
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplCocoa>();
        subMenus.push_back(subMenu);

        NSMenuItem *nsMenuItem =
            [nsMenu addItemWithTitle:Wrap(PrepareMnemonics(label)) action:nil keyEquivalent:@""];
        [nsMenu setSubmenu:subMenu->nsMenu forItem:nsMenuItem];

        return subMenu;
    }

    void AddSeparator() override {
        [nsMenu addItem:[NSMenuItem separatorItem]];
    }

    void PopUp() override {
        NSEvent* event = [NSApp currentEvent];
        [NSMenu popUpContextMenu:nsMenu withEvent:event forView:event.window.contentView];
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

class MenuBarImplCocoa final : public MenuBar {
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
        [subMenu->nsMenu setTitle:Wrap(PrepareMnemonics(label))];
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

@interface SSView : NSOpenGLView
@property Platform::Window *receiver;

@property BOOL acceptsFirstResponder;

@property(readonly, getter=isEditing) BOOL editing;
- (void)startEditing:(NSString *)text at:(NSPoint)origin
        withHeight:(double)fontHeight minWidth:(double)minWidth
        usingMonospace:(BOOL)isMonospace;
- (void)stopEditing;
- (void)didEdit:(NSString *)text;

@property double scrollerMin;
@property double scrollerSize;
@property double pageSize;

@end

@implementation SSView
{
    NSTrackingArea     *trackingArea;
    NSTextField        *editor;
    double             magnificationGestureCurrentZ;
    double             rotationGestureCurrent;
    Point2d            trackpadPositionShift;
    bool               inTrackpadScrollGesture;
    int                activeTrackpadTouches;
    bool               scrollFromTrackpadTouch;
    Platform::Window::Kind kind;
}

@synthesize acceptsFirstResponder;

- (id)initWithKind:(Platform::Window::Kind)aKind {
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFADepthSize, 24,
        0
    };
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    if(self = [super initWithFrame:NSMakeRect(0, 0, 0, 0) pixelFormat:pixelFormat]) {
        self.wantsBestResolutionOpenGLSurface = YES;
        self.wantsLayer = YES;
        editor = [[NSTextField alloc] init];
        editor.editable = YES;
        [[editor cell] setWraps:NO];
        [[editor cell] setScrollable:YES];
        editor.bezeled = NO;
        editor.target = self;
        editor.action = @selector(didEdit:);

        inTrackpadScrollGesture = false;
        activeTrackpadTouches = 0;
        scrollFromTrackpadTouch = false;
        self.acceptsTouchEvents = YES;
        kind = aKind;
        if(kind == Platform::Window::Kind::TOPLEVEL) {
            NSGestureRecognizer *mag = [[NSMagnificationGestureRecognizer alloc] initWithTarget:self
                action:@selector(magnifyGesture:)];
            [self addGestureRecognizer:mag];

            NSRotationGestureRecognizer* rot = [[NSRotationGestureRecognizer alloc] initWithTarget:self
                action:@selector(rotateGesture:)];
            [self addGestureRecognizer:rot];
        }
    }
    return self;
}

- (void)dealloc {
}

- (BOOL)isFlipped {
    return YES;
}

@synthesize receiver;

- (void)drawRect:(NSRect)aRect {
    [[self openGLContext] makeCurrentContext];
    if(receiver->onRender) {
        receiver->onRender();
    }
    [[self openGLContext] flushBuffer];
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

    NSPoint nsPoint = [self convertPoint:nsEvent.locationInWindow fromView:nil];
    event.x = nsPoint.x;
    event.y = nsPoint.y;

    NSUInteger nsFlags = [nsEvent modifierFlags];
    if(nsFlags & NSEventModifierFlagShift)   event.shiftDown   = true;
    if(nsFlags & NSEventModifierFlagCommand) event.controlDown = true;

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

- (void)mouseMotionEvent:(NSEvent *)nsEvent withButton:(Platform::MouseEvent::Button)button {
    using Platform::MouseEvent;

    MouseEvent event = [self convertMouseEvent:nsEvent];
    event.type   = MouseEvent::Type::MOTION;
    event.button = button;

    if(receiver->onMouseEvent) {
        receiver->onMouseEvent(event);
    }
}

- (void)mouseMoved:(NSEvent *)nsEvent {
    [self mouseMotionEvent:nsEvent];
    [super mouseMoved:nsEvent];
}

- (void)mouseDragged:(NSEvent *)nsEvent {
    [self mouseMotionEvent:nsEvent withButton:Platform::MouseEvent::Button::LEFT];
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
    if(nsEvent.phase == NSEventPhaseBegan) {
        // If this scroll began on trackpad then touchesBeganWithEvent was called prior to this
        // event and we have at least one active trackpad touch. We store this information so we
        // can handle scroll originating from trackpad differently below.
        scrollFromTrackpadTouch = activeTrackpadTouches > 0 &&
            nsEvent.subtype == NSEventSubtypeTabletPoint &&
            kind == Platform::Window::Kind::TOPLEVEL;
    }
    // Check if we are scrolling on trackpad and handle things differently.
    if(scrollFromTrackpadTouch) {
        // This is how Cocoa represents 2 finger trackpad drag gestures, rather than going via
        // NSPanGestureRecognizer which is how you might expect this to work... We complicate this
        // further by also handling shift-two-finger-drag to mean rotate. Fortunately we're using
        // shift in the same way as right-mouse-button MouseEvent does (to converts a pan to a
        // rotate) so we get the rotate support for free. It's a bit ugly having to fake mouse
        // events and track the deviation from the actual mouse cursor with trackpadPositionShift,
        // but in lieu of an event API that allows us to request a rotate/pan with relative
        // coordinates, it's the best we can do.
        event.button = MouseEvent::Button::RIGHT;
        // Make sure control (actually cmd) isn't passed through, ctrl-right-click-drag has special
        // meaning as rotate which we don't want to inadvertently trigger.
        event.controlDown = false;
        if(nsEvent.scrollingDeltaX == 0 && nsEvent.scrollingDeltaY == 0) {
            // Cocoa represents the point where the user lifts their fingers off (and any inertial
            // scrolling has finished) by an event with scrollingDeltaX and scrollingDeltaY both 0.
            // Sometimes you also get a zero scroll at the start of a two-finger-rotate (probably
            // reflecting the internal implementation of that being a cancelled possible pan
            // gesture), which is why this conditional is structured the way it is.
            if(inTrackpadScrollGesture) {
                event.x += trackpadPositionShift.x;
                event.y += trackpadPositionShift.y;
                event.type = MouseEvent::Type::RELEASE;
                receiver->onMouseEvent(event);
                inTrackpadScrollGesture = false;
                trackpadPositionShift = Point2d::From(0, 0);
            }
            return;
        } else if(!inTrackpadScrollGesture) {
            inTrackpadScrollGesture = true;
            trackpadPositionShift = Point2d::From(0, 0);
            event.type = MouseEvent::Type::PRESS;
            receiver->onMouseEvent(event);
            // And drop through
        }

        trackpadPositionShift.x += nsEvent.scrollingDeltaX;
        trackpadPositionShift.y += nsEvent.scrollingDeltaY;
        event.type = MouseEvent::Type::MOTION;
        event.x += trackpadPositionShift.x;
        event.y += trackpadPositionShift.y;
        receiver->onMouseEvent(event);
        return;
    }

    event.type = MouseEvent::Type::SCROLL_VERT;

    bool isPrecise = [nsEvent hasPreciseScrollingDeltas];
    event.scrollDelta = [nsEvent scrollingDeltaY] / (isPrecise ? 50 : 5);

    receiver->onMouseEvent(event);
}

- (void)touchesBeganWithEvent:(NSEvent *)event {
    activeTrackpadTouches++;
}

- (void)touchesEndedWithEvent:(NSEvent *)event {
    activeTrackpadTouches--;
}

- (void)touchesCancelledWithEvent:(NSEvent *)event {
    activeTrackpadTouches--;
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
    if(nsFlags & NSEventModifierFlagShift)
        event.shiftDown = true;
    if(nsFlags & NSEventModifierFlagCommand)
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

    if([NSEvent modifierFlags] & ~(NSEventModifierFlagShift|NSEventModifierFlagCommand)) {
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

    if([NSEvent modifierFlags] & ~(NSEventModifierFlagShift|NSEventModifierFlagCommand)) {
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

- (void)magnifyGesture:(NSMagnificationGestureRecognizer *)gesture {
    // The onSixDofEvent API doesn't allow us to specify the scaling's origin, so for expediency
    // we fake out a scrollwheel MouseEvent with a suitably-scaled scrollDelta with a bit of
    // absolute-to-relative positioning conversion tracked using magnificationGestureCurrentZ.

    if(gesture.state == NSGestureRecognizerStateBegan) {
        magnificationGestureCurrentZ = 0.0;
    }

    // Magic number to make gesture.magnification align roughly with what scrollDelta expects
    constexpr double kScale = 10.0;
    double z = ((double)gesture.magnification * kScale);
    double zdelta = z - magnificationGestureCurrentZ;
    magnificationGestureCurrentZ = z;

    using Platform::MouseEvent;
    MouseEvent event = {};
    event.type = MouseEvent::Type::SCROLL_VERT;
    NSPoint nsPoint = [gesture locationInView:self];
    event.x = nsPoint.x;
    event.y = nsPoint.y;
    event.scrollDelta = zdelta;
    if(receiver->onMouseEvent) {
        receiver->onMouseEvent(event);
    }
}

- (void)rotateGesture:(NSRotationGestureRecognizer *)gesture {
    if(gesture.state == NSGestureRecognizerStateBegan) {
        rotationGestureCurrent = 0.0;
    }
    double rotation = gesture.rotation;
    double rotationDelta = rotation - rotationGestureCurrent;
    rotationGestureCurrent = rotation;

    using Platform::SixDofEvent;
    SixDofEvent event = {};
    event.type = SixDofEvent::Type::MOTION;
    event.rotationZ = rotationDelta;
    if(receiver->onSixDofEvent) {
        receiver->onSixDofEvent(event);
    }
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
@synthesize scrollerSize;
@synthesize pageSize;

- (void)didScroll:(NSScroller *)sender {
    double pos;
    switch(sender.hitPart) {
        case NSScrollerKnob:
        case NSScrollerKnobSlot:
            pos = receiver->GetScrollbarPosition();
            break;
        case NSScrollerDecrementPage:
            pos = receiver->GetScrollbarPosition() - pageSize;
            break;
        case NSScrollerIncrementPage:
            pos = receiver->GetScrollbarPosition() + pageSize;
            break;
        default:
            return;
    }

    if(receiver->onScrollbarAdjusted) {
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

class WindowImplCocoa final : public Window {
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
        ssView = [[SSView alloc] initWithKind:kind];
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
                nsWindow.styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskResizable |
                NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
                nsWindow.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
                ssView.acceptsFirstResponder = YES;
                break;

            case Window::Kind::TOOL:
                NSPanel *nsPanel = [[NSPanel alloc] init];
                nsPanel.styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskResizable |
                NSWindowStyleMaskClosable | NSWindowStyleMaskUtilityWindow;
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

    double GetDevicePixelRatio() override {
        NSSize unitSize = { 1.0f, 0.0f };
        unitSize = [ssView convertSizeToBacking:unitSize];
        return unitSize.width;
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
        NSSize nsSize = ssView.frame.size;
        *width  = nsSize.width;
        *height = nsSize.height;
    }

    void SetMinContentSize(double width, double height) override {
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

    void SetTooltip(const std::string &newText, double x, double y, double w, double h) override {
        NSString *nsNewText = Wrap(newText);
        if(![nsToolTip isEqualToString:nsNewText]) {
            nsToolTip = nsNewText;

            NSToolTipManager *nsToolTipManager = [NSToolTipManager sharedToolTipManager];
            if(newText.empty()) {
                if ([nsToolTipManager respondsToSelector:@selector(abortToolTip)]) {
                    [nsToolTipManager abortToolTip];
                } else {
                    [nsToolTipManager orderOutToolTip];
                }
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
        ssView.scrollerSize = max + 1 - min;
        ssView.pageSize = pageSize;
        nsScroller.knobProportion = pageSize / ssView.scrollerSize;
        nsScroller.hidden = pageSize >= ssView.scrollerSize;
    }

    double GetScrollbarPosition() override {
        // Platform::Window scrollbar positions are in the range [min, max+1 - pageSize] inclusive,
        // and Cocoa scrollbars are from 0.0 to 1.0 inclusive, so we have to apply some scaling and
        // transforming. (scrollerSize is max+1-min, see ConfigureScrollbar above)
        return ssView.scrollerMin +
            nsScroller.doubleValue * (ssView.scrollerSize - ssView.pageSize);
    }

    void SetScrollbarPosition(double pos) override {
        nsScroller.doubleValue = (pos - ssView.scrollerMin) / ( ssView.scrollerSize - ssView.pageSize);
    }

    void Invalidate() override {
        ssView.needsDisplay = YES;
    }
};

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    return std::make_shared<WindowImplCocoa>(kind,
                std::static_pointer_cast<WindowImplCocoa>(parentWindow));
}

//-----------------------------------------------------------------------------
// 3DConnexion support
//-----------------------------------------------------------------------------

// Normally we would just link to the 3DconnexionClient framework.
//
// We don't want to (are not allowed to) distribute the official framework, so we're trying
// to use the one  installed on the users' computer. There are some different versions of
// the framework, the official one and re-implementations using an open source driver for
// older devices (spacenav-plus). So weak-linking isn't an option, either. The only remaining
// way is using  CFBundle to dynamically load the library at runtime, and also detect its
// availability.
//
// We're also defining everything needed from the 3DconnexionClientAPI, so we're not depending
// on the API headers.

#pragma pack(push,2)

enum {
    kConnexionClientModeTakeOver = 1,
    kConnexionClientModePlugin = 2
};

#define kConnexionMsgDeviceState '3dSR'
#define kConnexionMaskButtons 0x00FF
#define kConnexionMaskAxis 0x3F00

typedef struct {
    uint16_t version;
    uint16_t client;
    uint16_t command;
    int16_t param;
    int32_t value;
    UInt64 time;
    uint8_t report[8];
    uint16_t buttons8;
    int16_t axis[6];
    uint16_t address;
    uint32_t buttons;
} ConnexionDeviceState, *ConnexionDeviceStatePtr;

#pragma pack(pop)

typedef void (*ConnexionAddedHandlerProc)(io_connect_t);
typedef void (*ConnexionRemovedHandlerProc)(io_connect_t);
typedef void (*ConnexionMessageHandlerProc)(io_connect_t, natural_t, void *);

typedef OSErr (*InstallConnexionHandlersProc)(ConnexionMessageHandlerProc, ConnexionAddedHandlerProc, ConnexionRemovedHandlerProc);
typedef void (*CleanupConnexionHandlersProc)(void);
typedef UInt16 (*RegisterConnexionClientProc)(UInt32, UInt8 *, UInt16, UInt32);
typedef void (*UnregisterConnexionClientProc)(UInt16);

static CFBundleRef spaceBundle = nil;
static InstallConnexionHandlersProc installConnexionHandlers = NULL;
static CleanupConnexionHandlersProc cleanupConnexionHandlers = NULL;
static RegisterConnexionClientProc registerConnexionClient = NULL;
static UnregisterConnexionClientProc unregisterConnexionClient = NULL;
static UInt32 connexionSignature = 'SoSp';
static UInt8 *connexionName = (UInt8 *)"\x10SolveSpace";
static UInt16 connexionClient = 0;

static std::vector<std::weak_ptr<Window>> connexionWindows;
static bool connexionShiftIsDown = false;
static bool connexionCommandIsDown = false;

static void ConnexionAdded(io_connect_t con) {}
static void ConnexionRemoved(io_connect_t con) {}
static void ConnexionMessage(io_connect_t con, natural_t type, void *arg) {
    if (type != kConnexionMsgDeviceState) {
        return;
    }

    ConnexionDeviceState *device = (ConnexionDeviceState *)arg;

    dispatch_async(dispatch_get_main_queue(), ^(void){
        SixDofEvent event = {};
        event.type         = SixDofEvent::Type::MOTION;
        event.translationX = (double)device->axis[0] * -0.25;
        event.translationY = (double)device->axis[1] * -0.25;
        event.translationZ = (double)device->axis[2] *  0.25;
        event.rotationX    = (double)device->axis[3] * -0.0005;
        event.rotationY    = (double)device->axis[4] * -0.0005;
        event.rotationZ    = (double)device->axis[5] * -0.0005;
        event.shiftDown    = connexionShiftIsDown;
        event.controlDown  = connexionCommandIsDown;

        for(auto window : connexionWindows) {
            if(auto windowStrong = window.lock()) {
                if(windowStrong->onSixDofEvent) {
                    windowStrong->onSixDofEvent(event);
                }
            }
        }
    });
}

void Open3DConnexion() {
    NSString *bundlePath = @"/Library/Frameworks/3DconnexionClient.framework";
    NSURL *bundleURL = [NSURL fileURLWithPath:bundlePath];
    spaceBundle = CFBundleCreate(kCFAllocatorDefault, (__bridge CFURLRef)bundleURL);

    // Don't continue if no driver is installed on this machine
    if(spaceBundle == nil) {
        return;
    }

    installConnexionHandlers = (InstallConnexionHandlersProc)
                CFBundleGetFunctionPointerForName(spaceBundle,
                        CFSTR("InstallConnexionHandlers"));

    cleanupConnexionHandlers = (CleanupConnexionHandlersProc)
                CFBundleGetFunctionPointerForName(spaceBundle,
                        CFSTR("CleanupConnexionHandlers"));

    registerConnexionClient = (RegisterConnexionClientProc)
                CFBundleGetFunctionPointerForName(spaceBundle,
                        CFSTR("RegisterConnexionClient"));

    unregisterConnexionClient = (UnregisterConnexionClientProc)
                CFBundleGetFunctionPointerForName(spaceBundle,
                        CFSTR("UnregisterConnexionClient"));

    // Only continue if all required symbols have been loaded
    if((installConnexionHandlers == NULL) || (cleanupConnexionHandlers == NULL)
            || (registerConnexionClient == NULL) || (unregisterConnexionClient == NULL)) {
        CFRelease(spaceBundle);
        spaceBundle = nil;
        return;
    }

    installConnexionHandlers(&ConnexionMessage, &ConnexionAdded, &ConnexionRemoved);
    connexionClient = registerConnexionClient(connexionSignature, connexionName,
            kConnexionClientModeTakeOver, kConnexionMaskButtons | kConnexionMaskAxis);

    [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskKeyDown | NSEventMaskFlagsChanged)
                                          handler:^(NSEvent *event) {
        connexionShiftIsDown = (event.modifierFlags & NSEventModifierFlagShift);
        connexionCommandIsDown = (event.modifierFlags & NSEventModifierFlagCommand);
        return event;
    }];

    [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskKeyUp | NSEventMaskFlagsChanged)
                                          handler:^(NSEvent *event) {
        connexionShiftIsDown = (event.modifierFlags & NSEventModifierFlagShift);
        connexionCommandIsDown = (event.modifierFlags & NSEventModifierFlagCommand);
        return event;
    }];
}

void Close3DConnexion() {
    if(spaceBundle == nil) {
        return;
    }

    unregisterConnexionClient(connexionClient);
    cleanupConnexionHandlers();

    CFRelease(spaceBundle);
}

void Request3DConnexionEventsForWindow(WindowRef window) {
    connexionWindows.push_back(window);
}

//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------

class MessageDialogImplCocoa final : public MessageDialog {
public:
    NSAlert  *nsAlert  = [[NSAlert alloc] init];
    NSWindow *nsWindow;

    std::vector<Response> responses;

    void SetType(Type type) override {
        switch(type) {
            case Type::INFORMATION:
            case Type::QUESTION:
                nsAlert.alertStyle = NSAlertStyleInformational;
                break;

            case Type::WARNING:
            case Type::ERROR:
                nsAlert.alertStyle = NSAlertStyleWarning;
                break;
        }
    }

    void SetTitle(std::string title) override {
        [nsAlert.window setTitle:Wrap(title)];
    }

    void SetMessage(std::string message) override {
        nsAlert.messageText = Wrap(message);
    }

    void SetDescription(std::string description) override {
        nsAlert.informativeText = Wrap(description);
    }

    void AddButton(std::string label, Response response, bool isDefault) override {
        NSButton *nsButton = [nsAlert addButtonWithTitle:Wrap(PrepareMnemonics(label))];
        if(!isDefault && [nsButton.keyEquivalent isEqualToString:@"\n"]) {
            nsButton.keyEquivalent = @"";
        } else if(response == Response::CANCEL) {
            nsButton.keyEquivalent = @"\e";
        }
        responses.push_back(response);
    }

    Response RunModal() override {
        // FIXME(platform/gui): figure out a way to run the alert as a sheet
        NSModalResponse nsResponse = [nsAlert runModal];
        ssassert(nsResponse >= NSAlertFirstButtonReturn &&
                 nsResponse <= NSAlertFirstButtonReturn + (long)responses.size(),
                 "Unexpected response");
        return responses[nsResponse - NSAlertFirstButtonReturn];
    }
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    std::shared_ptr<MessageDialogImplCocoa> dialog = std::make_shared<MessageDialogImplCocoa>();
    dialog->nsWindow = std::static_pointer_cast<WindowImplCocoa>(parentWindow)->nsWindow;
    return dialog;
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

}
}

@interface SSSaveFormatAccessory : NSViewController
@property NSSavePanel      *panel;
@property NSMutableArray   *filters;

@property(nonatomic) NSInteger               index;
@property(nonatomic) IBOutlet NSTextField   *textField;
@property(nonatomic) IBOutlet NSPopUpButton *button;
@end

@implementation SSSaveFormatAccessory
@synthesize panel, filters, button;

- (void)setIndex:(NSInteger)newIndex {
    self->_index = newIndex;
    NSMutableArray *filter = [filters objectAtIndex:newIndex];
    NSString *extension = [filter objectAtIndex:0];
    if(![extension isEqual:@"*"]) {
        NSString *filename = panel.nameFieldStringValue;
        NSString *basename = [[filename componentsSeparatedByString:@"."] objectAtIndex:0];
        panel.nameFieldStringValue = [basename stringByAppendingPathExtension:extension];
    }
    [panel setAllowedFileTypes:filter];
}
@end

namespace SolveSpace {
namespace Platform {

class FileDialogImplCocoa : public FileDialog {
public:
    NSSavePanel *nsPanel = nil;

    void SetTitle(std::string title) override {
        nsPanel.title = Wrap(title);
    }

    void SetCurrentName(std::string name) override {
        nsPanel.nameFieldStringValue = Wrap(name);
    }

    Platform::Path GetFilename() override {
        return Platform::Path::From(nsPanel.URL.fileSystemRepresentation);
    }

    void SetFilename(Platform::Path path) override {
        nsPanel.directoryURL =
            [NSURL fileURLWithPath:Wrap(path.Parent().raw) isDirectory:YES];
        nsPanel.nameFieldStringValue = Wrap(path.FileStem());
    }

    void SuggestFilename(Platform::Path path) override {
        SetFilename(path.WithExtension(""));
    }

    void FreezeChoices(SettingsRef settings, const std::string &key) override {
        if (nsPanel.directoryURL != nil) {
            settings->FreezeString("Dialog_" + key + "_Folder",
                                   [nsPanel.directoryURL.absoluteString UTF8String]);
        }
    }

    void ThawChoices(SettingsRef settings, const std::string &key) override {
        nsPanel.directoryURL =
            [NSURL URLWithString:Wrap(settings->ThawString("Dialog_" + key + "_Folder", ""))];
    }

    bool RunModal() override {
        if([nsPanel runModal] == NSModalResponseOK) {
            return true;
        } else {
            return false;
        }
    }
};

class OpenFileDialogImplCocoa final : public FileDialogImplCocoa {
public:
    NSMutableArray *nsFilter = [[NSMutableArray alloc] init];

    OpenFileDialogImplCocoa() {
        SetTitle(C_("title", "Open File"));
    }

    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        for(auto extension : extensions) {
            [nsFilter addObject:Wrap(extension)];
        }
        [nsPanel setAllowedFileTypes:nsFilter];
    }
};

class SaveFileDialogImplCocoa final : public FileDialogImplCocoa {
public:
    NSMutableArray         *nsFilters   = [[NSMutableArray alloc] init];
    SSSaveFormatAccessory  *ssAccessory = nil;

    SaveFileDialogImplCocoa() {
        SetTitle(C_("title", "Save File"));
    }

    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        NSMutableArray *nsFilter = [[NSMutableArray alloc] init];
        for(auto extension : extensions) {
            [nsFilter addObject:Wrap(extension)];
        }
        if(nsFilters.count == 0) {
            [nsPanel setAllowedFileTypes:nsFilter];
        }
        [nsFilters addObject:nsFilter];

        std::string desc;
        for(auto extension : extensions) {
            if(!desc.empty()) desc += ", ";
            desc += extension;
        }
        std::string title = name + " (" + desc + ")";
        if(nsFilters.count == 1) {
            [ssAccessory.button removeAllItems];
        }
        [ssAccessory.button addItemWithTitle:Wrap(title)];
        [ssAccessory.button synchronizeTitleAndSelectedItem];
    }

    void FreezeChoices(SettingsRef settings, const std::string &key) override {
        FileDialogImplCocoa::FreezeChoices(settings, key);
        settings->FreezeInt("Dialog_" + key + "_Filter", ssAccessory.index);
    }

    void ThawChoices(SettingsRef settings, const std::string &key) override {
        FileDialogImplCocoa::ThawChoices(settings, key);
        ssAccessory.index = settings->ThawInt("Dialog_" + key + "_Filter", 0);
    }

    bool RunModal() override {
        if(nsFilters.count == 1) {
            nsPanel.accessoryView = nil;
        }

        if(nsPanel.nameFieldStringValue.length == 0) {
            nsPanel.nameFieldStringValue = Wrap(_("untitled"));
        }

        return FileDialogImplCocoa::RunModal();
    }
};

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    NSOpenPanel *nsPanel = [NSOpenPanel openPanel];
    nsPanel.canSelectHiddenExtension = YES;

    std::shared_ptr<OpenFileDialogImplCocoa> dialog = std::make_shared<OpenFileDialogImplCocoa>();
    dialog->nsPanel = nsPanel;

    return dialog;
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    NSSavePanel *nsPanel = [NSSavePanel savePanel];
    nsPanel.canSelectHiddenExtension = YES;

    SSSaveFormatAccessory *ssAccessory =
        [[SSSaveFormatAccessory alloc] initWithNibName:@"SaveFormatAccessory" bundle:nil];
    ssAccessory.panel = nsPanel;
    nsPanel.accessoryView = [ssAccessory view];

    std::shared_ptr<SaveFileDialogImplCocoa> dialog = std::make_shared<SaveFileDialogImplCocoa>();
    dialog->nsPanel = nsPanel;
    dialog->ssAccessory = ssAccessory;
    ssAccessory.filters = dialog->nsFilters;

    return dialog;
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Platform::Path> GetFontFiles() {
    std::vector<SolveSpace::Platform::Path> fonts;

    NSArray *fontNames = [[NSFontManager sharedFontManager] availableFonts];
    for(NSString *fontName in fontNames) {
        CTFontDescriptorRef fontRef =
            CTFontDescriptorCreateWithNameAndSize ((__bridge CFStringRef)fontName, 10.0);
        CFURLRef url = (CFURLRef)CTFontDescriptorCopyAttribute(fontRef, kCTFontURLAttribute);
        NSString *fontPath = [NSString stringWithString:[(NSURL *)CFBridgingRelease(url) path]];
        fonts.push_back(
            Platform::Path::From([[NSFileManager defaultManager]
                fileSystemRepresentationWithPath:fontPath]));
    }

    return fonts;
}

void OpenInBrowser(const std::string &url) {
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:Wrap(url)]];
}

}
}

@interface SSApplicationDelegate : NSObject<NSApplicationDelegate>
- (IBAction)preferences:(id)sender;
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;

@property BOOL exiting;

@end

@implementation SSApplicationDelegate

@synthesize exiting;

- (id)init {
    if (self = [super init]) {
        self.exiting = false;
    }
    return self;
}

- (IBAction)preferences:(id)sender {
    if (!SS.GW.showTextWindow) {
        SolveSpace::SS.GW.MenuView(SolveSpace::Command::SHOW_TEXT_WND);
    }
    SolveSpace::SS.TW.GoToScreen(SolveSpace::TextWindow::Screen::CONFIGURATION);
    SolveSpace::SS.ScheduleShowTW();
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename {
    SolveSpace::Platform::Path path = SolveSpace::Platform::Path::From([filename UTF8String]);
    return SolveSpace::SS.Load(path.Expand(/*fromCurrentDirectory=*/true));
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    if(!SS.unsaved) {
        return NSTerminateNow;
    } else {
        [self performSelectorOnMainThread:@selector(applicationTerminatePrompt) withObject:nil
            waitUntilDone:NO modes:@[NSDefaultRunLoopMode, NSModalPanelRunLoopMode]];
        return NSTerminateLater;
    }
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    if(!exiting) {
        // Prevent the Platform::ExitGui() call from SolveSpaceUI::Exit()
        // triggering another terminate
        exiting = true;
        // Now let SS save settings etc
        SS.Exit();
    }
}

- (void)applicationTerminatePrompt {
    [NSApp replyToApplicationShouldTerminate:SS.OkayToStartNewFile()];
}
@end

namespace SolveSpace {
namespace Platform {

static SSApplicationDelegate *ssDelegate;

std::vector<std::string> InitGui(int argc, char **argv) {
    std::vector<std::string> args = InitCli(argc, argv);
    if(args.size() >= 2 && args[1].find("-psn_") == 0) {
        // For unknown reasons, Finder passes a Carbon PSN (Process Serial Number) argument
        // when a freshly downloaded application is run for the first time. Remove it so
        // that it isn't interpreted as a filename.
        args.erase(args.begin() + 1);
    }

    ssDelegate = [[SSApplicationDelegate alloc] init];
    NSApplication.sharedApplication.delegate = ssDelegate;

    // Setting this prevents "Show Tab Bar" and "Show All Tabs" items from being
    // automagically added to the View menu
    NSWindow.allowsAutomaticWindowTabbing = NO;

    // And this prevents the duplicate "Enter Full Screen" menu item, see
    // https://stackoverflow.com/questions/52154977/how-to-get-rid-of-enter-full-screen-menu-item
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSFullScreenMenuItemEverywhere"];

    [NSBundle.mainBundle loadNibNamed:@"MainMenu" owner:nil topLevelObjects:nil];

    NSArray *languages = NSLocale.preferredLanguages;
    for(NSString *language in languages) {
        if(SolveSpace::SetLocale([language UTF8String])) break;
    }
    if(languages.count == 0) {
        SolveSpace::SetLocale("en_US");
    }

    return args;
}

void RunGui() {
    [NSApp run];
}

void ExitGui() {
    if(!ssDelegate.exiting) {
        ssDelegate.exiting = true;
        [NSApp terminate:nil];
    }
}

void ClearGui() {}

}
}
