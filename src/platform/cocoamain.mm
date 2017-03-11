//-----------------------------------------------------------------------------
// Our main() function, and Cocoa-specific stuff to set up our windows and
// otherwise handle our interface to the operating system. Everything
// outside platform/... should be standard C++ and OpenGL.
//
// Copyright 2015 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#include <mach/mach.h>
#include <mach/clock.h>
#import  <AppKit/AppKit.h>

#include "solvespace.h"

using SolveSpace::dbp;

/* Utility functions */

static NSString* Wrap(const std::string &s) {
    return [NSString stringWithUTF8String:s.c_str()];
}

/* Settings */

namespace SolveSpace {
void CnfFreezeInt(uint32_t val, const std::string &key) {
    [[NSUserDefaults standardUserDefaults]
        setInteger:val forKey:Wrap(key)];
}

uint32_t CnfThawInt(uint32_t val, const std::string &key) {
    NSString *nsKey = Wrap(key);
    if([[NSUserDefaults standardUserDefaults] objectForKey:nsKey])
        return [[NSUserDefaults standardUserDefaults] integerForKey:nsKey];
    return val;
}

void CnfFreezeFloat(float val, const std::string &key) {
    [[NSUserDefaults standardUserDefaults]
        setFloat:val forKey:Wrap(key)];
}

float CnfThawFloat(float val, const std::string &key) {
    NSString *nsKey = Wrap(key);
    if([[NSUserDefaults standardUserDefaults] objectForKey:nsKey])
        return [[NSUserDefaults standardUserDefaults] floatForKey:nsKey];
    return val;
}

void CnfFreezeString(const std::string &val, const std::string &key) {
    [[NSUserDefaults standardUserDefaults]
        setObject:Wrap(val)
        forKey:Wrap(key)];
}

std::string CnfThawString(const std::string &val, const std::string &key) {
    NSString *nsKey = Wrap(key);
    if([[NSUserDefaults standardUserDefaults] objectForKey:nsKey]) {
        NSString *nsNewVal = [[NSUserDefaults standardUserDefaults] stringForKey:nsKey];
        return [nsNewVal UTF8String];
    }
    return val;
}
};

/* Timer */

@interface DeferredHandler : NSObject
+ (void) runLater:(id)dummy;
+ (void) runCallback;
+ (void) doAutosave;
@end

@implementation DeferredHandler
+ (void) runLater:(id)dummy {
    SolveSpace::SS.DoLater();
}
+ (void) runCallback {
    SolveSpace::SS.GW.TimerCallback();
    SolveSpace::SS.TW.TimerCallback();
}
+ (void) doAutosave {
    SolveSpace::SS.Autosave();
}
@end

static void Schedule(SEL selector, double interval) {
    NSMethodSignature *signature = [[DeferredHandler class]
        methodSignatureForSelector:selector];
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setSelector:selector];
    [invocation setTarget:[DeferredHandler class]];
    [NSTimer scheduledTimerWithTimeInterval:interval
        invocation:invocation repeats:NO];
}

void SolveSpace::SetTimerFor(int milliseconds) {
    Schedule(@selector(runCallback), milliseconds / 1000.0);
}

void SolveSpace::SetAutosaveTimerFor(int minutes) {
    Schedule(@selector(doAutosave), minutes * 60.0);
}

void SolveSpace::ScheduleLater() {
    [[NSRunLoop currentRunLoop]
        performSelector:@selector(runLater:)
        target:[DeferredHandler class] argument:nil
        order:0 modes:@[NSDefaultRunLoopMode]];
}

/* OpenGL view */

@interface GLViewWithEditor : NSView
- (void)drawGL;

@property BOOL wantsBackingStoreScaling;

@property(readonly, getter=isEditing) BOOL editing;
- (void)startEditing:(NSString*)text at:(NSPoint)origin withHeight:(double)fontHeight
        usingMonospace:(BOOL)isMonospace;
- (void)stopEditing;
- (void)didEdit:(NSString*)text;
@end

@implementation GLViewWithEditor
{
    SolveSpace::GlOffscreen offscreen;
    NSOpenGLContext *glContext;
@protected
    NSTextField *editor;
}

- (id)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    [self setWantsLayer:YES];

    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFADepthSize, 24,
        0
    };
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    glContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:NULL];

    editor = [[NSTextField alloc] init];
    [editor setEditable:YES];
    [[editor cell] setWraps:NO];
    [[editor cell] setScrollable:YES];
    [editor setBezeled:NO];
    [editor setTarget:self];
    [editor setAction:@selector(editorAction:)];

    return self;
}

- (void)dealloc {
    offscreen.Clear();
}

#define CONVERT1(name, to_from) \
    - (NS##name)convert##name##to_from##Backing:(NS##name)input { \
        return _wantsBackingStoreScaling ? [super convert##name##to_from##Backing:input] : input; }
#define CONVERT(name) CONVERT1(name, To) CONVERT1(name, From)
CONVERT(Size)
CONVERT(Rect)
#undef CONVERT
#undef CONVERT1

- (NSPoint)convertPointToBacking:(NSPoint)input {
    if(_wantsBackingStoreScaling) return [super convertPointToBacking:input];
    else {
        input.y *= -1;
        return input;
    }
}

- (NSPoint)convertPointFromBacking:(NSPoint)input {
    if(_wantsBackingStoreScaling) return [super convertPointFromBacking:input];
    else {
        input.y *= -1;
        return input;
    }
}

- (void)drawRect:(NSRect)aRect {
    [glContext makeCurrentContext];

    NSSize size   = [self convertSizeToBacking:[self bounds].size];
    int    width  = (int)size.width,
           height = (int)size.height;
    offscreen.Render(width, height, [&] { [self drawGL]; });

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

- (void)drawGL {
}

@synthesize editing;

- (void)startEditing:(NSString*)text at:(NSPoint)origin withHeight:(double)fontHeight
        usingMonospace:(BOOL)isMonospace {
    if(!self->editing) {
        [self addSubview:editor];
        self->editing = YES;
    }

    NSFont *font;
    if(isMonospace)
        font = [NSFont fontWithName:@"Monaco" size:fontHeight];
    else
        font = [NSFont controlContentFontOfSize:fontHeight];
    [editor setFont:font];

    origin.x -= 3; /* left padding; no way to get it from NSTextField */
    origin.y -= [editor intrinsicContentSize].height;
    origin.y += [editor baselineOffsetFromBottom];

    [editor setFrameOrigin:origin];
    [editor setStringValue:text];
    [[self window] makeFirstResponder:editor];
}

- (void)stopEditing {
    if(self->editing) {
        [editor removeFromSuperview];
        self->editing = NO;
    }
}

- (void)editorAction:(id)sender {
    [self didEdit:[editor stringValue]];
    [self stopEditing];
}

- (void)didEdit:(NSString*)text {
}
@end

/* Graphics window */

@interface GraphicsWindowView : GLViewWithEditor
{
    NSTrackingArea *trackingArea;
}

@property(readonly) NSEvent *lastContextMenuEvent;
@end

@implementation GraphicsWindowView
- (BOOL)isFlipped {
    return YES;
}

- (void)drawGL {
    SolveSpace::SS.GW.Paint();
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void) createTrackingArea {
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
        options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                 NSTrackingActiveInKeyWindow)
        owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
}

- (void) updateTrackingAreas
{
    [self removeTrackingArea:trackingArea];
    [self createTrackingArea];
    [super updateTrackingAreas];
}

- (void)mouseMoved:(NSEvent*)event {
    NSPoint point = [self ij_to_xy:[self convertPoint:[event locationInWindow] fromView:nil]];
    NSUInteger flags = [event modifierFlags];
    NSUInteger buttons = [NSEvent pressedMouseButtons];
    SolveSpace::SS.GW.MouseMoved(point.x, point.y,
        buttons & (1 << 0),
        buttons & (1 << 2),
        buttons & (1 << 1),
        flags & NSShiftKeyMask,
        flags & NSCommandKeyMask);
}

- (void)mouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent*)event {
    [self mouseMoved:event];
}

- (void)mouseDown:(NSEvent*)event {
    NSPoint point = [self ij_to_xy:[self convertPoint:[event locationInWindow] fromView:nil]];
    if([event clickCount] == 1)
        SolveSpace::SS.GW.MouseLeftDown(point.x, point.y);
    else if([event clickCount] == 2)
        SolveSpace::SS.GW.MouseLeftDoubleClick(point.x, point.y);
}

- (void)rightMouseDown:(NSEvent*)event {
    NSPoint point = [self ij_to_xy:[self convertPoint:[event locationInWindow] fromView:nil]];
    SolveSpace::SS.GW.MouseMiddleOrRightDown(point.x, point.y);
}

- (void)otherMouseDown:(NSEvent*)event {
    [self rightMouseDown:event];
}

- (void)mouseUp:(NSEvent*)event {
    NSPoint point = [self ij_to_xy:[self convertPoint:[event locationInWindow] fromView:nil]];
    SolveSpace::SS.GW.MouseLeftUp(point.x, point.y);
}

- (void)rightMouseUp:(NSEvent*)event {
    NSPoint point = [self ij_to_xy:[self convertPoint:[event locationInWindow] fromView:nil]];
    self->_lastContextMenuEvent = event;
    SolveSpace::SS.GW.MouseRightUp(point.x, point.y);
}

- (void)scrollWheel:(NSEvent*)event {
    NSPoint point = [self ij_to_xy:[self convertPoint:[event locationInWindow] fromView:nil]];
    SolveSpace::SS.GW.MouseScroll(point.x, point.y, (int)-[event deltaY]);
}

- (void)mouseExited:(NSEvent*)event {
    SolveSpace::SS.GW.MouseLeave();
}

- (void)keyDown:(NSEvent*)event {
    int chr = 0;
    if(NSString *nsChr = [event charactersIgnoringModifiers])
        chr = [nsChr characterAtIndex:0];

    if(chr >= NSF1FunctionKey && chr <= NSF12FunctionKey)
        chr = SolveSpace::GraphicsWindow::FUNCTION_KEY_BASE + (chr - NSF1FunctionKey);

    NSUInteger flags = [event modifierFlags];
    if(flags & NSShiftKeyMask)
        chr |= SolveSpace::GraphicsWindow::SHIFT_MASK;
    if(flags & NSCommandKeyMask)
        chr |= SolveSpace::GraphicsWindow::CTRL_MASK;

    // override builtin behavior: "focus on next cell", "close window"
    if(chr == '\t' || chr == '\x1b')
        [[NSApp mainMenu] performKeyEquivalent:event];
    else if(!chr || !SolveSpace::SS.GW.KeyDown(chr))
        [super keyDown:event];
}

- (void)startEditing:(NSString*)text at:(NSPoint)xy withHeight:(double)fontHeight
        withMinWidthInChars:(int)minWidthChars {
    // Convert to ij (vs. xy) style coordinates
    NSSize size = [self convertSizeToBacking:[self bounds].size];
    NSPoint point = {
        .x = xy.x + size.width / 2,
        .y = xy.y - size.height / 2
    };
    [[self window] makeKeyWindow];
    [super startEditing:text at:[self convertPointFromBacking:point]
           withHeight:fontHeight usingMonospace:FALSE];
    [self prepareEditorWithMinWidthInChars:minWidthChars];
}

- (void)prepareEditorWithMinWidthInChars:(int)minWidthChars {
    NSFont *font = [editor font];
    NSGlyph glyphA = [font glyphWithName:@"a"];
    ssassert(glyphA != (NSGlyph)-1, "Expected font to have U+0061");
    CGFloat glyphAWidth = [font advancementForGlyph:glyphA].width;

    [editor sizeToFit];

    NSSize frameSize = [editor frame].size;
    frameSize.width = std::max(frameSize.width, glyphAWidth * minWidthChars);
    [editor setFrameSize:frameSize];
}

- (void)didEdit:(NSString*)text {
    SolveSpace::SS.GW.EditControlDone([text UTF8String]);
    [self setNeedsDisplay:YES];
}

- (void)cancelOperation:(id)sender {
    [self stopEditing];
}

- (NSPoint)ij_to_xy:(NSPoint)ij {
    // Convert to xy (vs. ij) style coordinates,
    // with (0, 0) at center
    NSSize size = [self bounds].size;
    return [self convertPointToBacking:(NSPoint){
        .x = ij.x - size.width / 2, .y = ij.y - size.height / 2 }];
}
@end

@interface GraphicsWindowDelegate : NSObject<NSWindowDelegate>
- (BOOL)windowShouldClose:(id)sender;

@property(readonly, getter=isFullscreen) BOOL fullscreen;
- (void)windowDidEnterFullScreen:(NSNotification *)notification;
- (void)windowDidExitFullScreen:(NSNotification *)notification;
@end

@implementation GraphicsWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    [NSApp terminate:sender];
    return FALSE; /* in case NSApp changes its mind */
}

@synthesize fullscreen;
- (void)windowDidEnterFullScreen:(NSNotification *)notification {
    fullscreen = true;
    /* Update the menus */
    SolveSpace::SS.GW.EnsureValidActives();
}
- (void)windowDidExitFullScreen:(NSNotification *)notification {
    fullscreen = false;
    /* Update the menus */
    SolveSpace::SS.GW.EnsureValidActives();
}
@end

static NSWindow *GW;
static GraphicsWindowView *GWView;
static GraphicsWindowDelegate *GWDelegate;

namespace SolveSpace {
void InitGraphicsWindow() {
    GW = [[NSWindow alloc] init];
    GWDelegate = [[GraphicsWindowDelegate alloc] init];
    [GW setDelegate:GWDelegate];
    [GW setStyleMask:(NSTitledWindowMask | NSClosableWindowMask |
                      NSMiniaturizableWindowMask | NSResizableWindowMask)];
    [GW setFrameAutosaveName:@"GraphicsWindow"];
    [GW setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
    if(![GW setFrameUsingName:[GW frameAutosaveName]])
        [GW setContentSize:(NSSize){ .width = 600, .height = 600 }];
    GWView = [[GraphicsWindowView alloc] init];
    [GW setContentView:GWView];
}

void GetGraphicsWindowSize(int *w, int *h) {
    NSSize size = [GWView convertSizeToBacking:[GWView frame].size];
    *w = (int)size.width;
    *h = (int)size.height;
}

void InvalidateGraphics() {
    [GWView setNeedsDisplay:YES];
}

void PaintGraphics() {
    [GWView setNeedsDisplay:YES];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, YES);
}

void SetCurrentFilename(const Platform::Path &filename) {
    if(!filename.IsEmpty()) {
        [GW setTitleWithRepresentedFilename:Wrap(filename.raw)];
    } else {
        [GW setTitle:Wrap(C_("title", "(new sketch)"))];
        [GW setRepresentedFilename:@""];
    }
}

void ToggleFullScreen() {
    [GW toggleFullScreen:nil];
}

bool FullScreenIsActive() {
    return [GWDelegate isFullscreen];
}

void ShowGraphicsEditControl(int x, int y, int fontHeight, int minWidthChars,
                             const std::string &str) {
    [GWView startEditing:Wrap(str)
            at:(NSPoint){(CGFloat)x, (CGFloat)y}
            withHeight:fontHeight
            withMinWidthInChars:minWidthChars];
}

void HideGraphicsEditControl() {
    [GWView stopEditing];
}

bool GraphicsEditControlIsVisible() {
    return [GWView isEditing];
}
}

/* Context menus */

static SolveSpace::ContextCommand contextMenuChoice;

@interface ContextMenuResponder : NSObject
+ (void)handleClick:(id)sender;
@end

@implementation ContextMenuResponder
+ (void)handleClick:(id)sender {
    contextMenuChoice = (SolveSpace::ContextCommand)[sender tag];
}
@end

namespace SolveSpace {
NSMenu *contextMenu, *contextSubmenu;

void AddContextMenuItem(const char *label, ContextCommand cmd) {
    NSMenuItem *menuItem;
    if(label) {
        menuItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:label]
            action:@selector(handleClick:) keyEquivalent:@""];
        [menuItem setTarget:[ContextMenuResponder class]];
        [menuItem setTag:(NSInteger)cmd];
    } else {
        menuItem = [NSMenuItem separatorItem];
    }

    if(cmd == SolveSpace::ContextCommand::SUBMENU) {
        [menuItem setSubmenu:contextSubmenu];
        contextSubmenu = nil;
    }

    if(contextSubmenu) {
        [contextSubmenu addItem:menuItem];
    } else {
        if(!contextMenu) {
            contextMenu = [[NSMenu alloc]
                initWithTitle:[NSString stringWithUTF8String:label]];
        }

        [contextMenu addItem:menuItem];
    }
}

void CreateContextSubmenu() {
    ssassert(!contextSubmenu, "Unexpected nested submenu");

    contextSubmenu = [[NSMenu alloc] initWithTitle:@""];
}

ContextCommand ShowContextMenu() {
    if(!contextMenu)
        return ContextCommand::CANCELLED;

    [NSMenu popUpContextMenu:contextMenu
        withEvent:[GWView lastContextMenuEvent] forView:GWView];

    contextMenu = nil;

    return contextMenuChoice;
}
};

/* Main menu */

@interface MainMenuResponder : NSObject
+ (void)handleStatic:(id)sender;
+ (void)handleRecent:(id)sender;
@end

@implementation MainMenuResponder
+ (void)handleStatic:(id)sender {
    SolveSpace::GraphicsWindow::MenuEntry *entry =
        (SolveSpace::GraphicsWindow::MenuEntry*)[sender tag];

    if(entry->fn && ![(NSMenuItem*)sender hasSubmenu])
        entry->fn(entry->id);
}

+ (void)handleRecent:(id)sender {
    uint32_t cmd = [sender tag];
    if(cmd >= (uint32_t)SolveSpace::Command::RECENT_OPEN &&
       cmd < ((uint32_t)SolveSpace::Command::RECENT_OPEN + SolveSpace::MAX_RECENT)) {
        SolveSpace::SolveSpaceUI::MenuFile((SolveSpace::Command)cmd);
    } else if(cmd >= (uint32_t)SolveSpace::Command::RECENT_LINK &&
              cmd < ((uint32_t)SolveSpace::Command::RECENT_LINK + SolveSpace::MAX_RECENT)) {
        SolveSpace::Group::MenuGroup((SolveSpace::Command)cmd);
    }
}

+ (void)handleLocale:(id)sender {
    uint32_t offset = [sender tag];
    SolveSpace::SolveSpaceUI::MenuHelp(
        (SolveSpace::Command)((uint32_t)SolveSpace::Command::LOCALE + offset));
}
@end

namespace SolveSpace {
std::map<uint32_t, NSMenuItem*> mainMenuItems;

void InitMainMenu(NSMenu *mainMenu) {
    NSMenuItem *menuItem = NULL;
    NSMenu *levels[5] = {mainMenu, 0};
    NSString *label;

    while([mainMenu numberOfItems] != 1) {
        [mainMenu removeItemAtIndex:1];
    }

    const GraphicsWindow::MenuEntry *entry = &GraphicsWindow::menu[0];
    int current_level = 0;
    while(entry->level >= 0) {
        if(entry->level > current_level) {
            NSMenu *menu = [[NSMenu alloc] initWithTitle:label];
            [menu setAutoenablesItems:NO];
            [menuItem setSubmenu:menu];

            ssassert((unsigned)entry->level < sizeof(levels) / sizeof(levels[0]),
                     "Unexpected depth of menu nesting");

            levels[entry->level] = menu;
        }

        current_level = entry->level;

        if(entry->label) {
            label = Wrap(Translate(entry->label));
            /* OS X does not support mnemonics */
            label = [label stringByReplacingOccurrencesOfString:@"&" withString:@""];

            unichar accelChar = entry->accel &
                ~(GraphicsWindow::SHIFT_MASK | GraphicsWindow::CTRL_MASK);
            if(accelChar > GraphicsWindow::FUNCTION_KEY_BASE &&
                    accelChar <= GraphicsWindow::FUNCTION_KEY_BASE + 12) {
                accelChar = NSF1FunctionKey + (accelChar - GraphicsWindow::FUNCTION_KEY_BASE - 1);
            } else if(accelChar == GraphicsWindow::DELETE_KEY) {
                accelChar = NSBackspaceCharacter;
            }
            NSString *accel = [NSString stringWithCharacters:&accelChar length:1];

            menuItem = [levels[entry->level] addItemWithTitle:label
                action:NULL keyEquivalent:[accel lowercaseString]];

            NSUInteger modifierMask = 0;
            if(entry->accel & GraphicsWindow::SHIFT_MASK)
                modifierMask |= NSShiftKeyMask;
            else if(entry->accel & GraphicsWindow::CTRL_MASK)
                modifierMask |= NSCommandKeyMask;
            [menuItem setKeyEquivalentModifierMask:modifierMask];

            [menuItem setTag:(NSInteger)entry];
            [menuItem setTarget:[MainMenuResponder class]];
            [menuItem setAction:@selector(handleStatic:)];
        } else {
            [levels[entry->level] addItem:[NSMenuItem separatorItem]];
        }

        if(entry->id == Command::LOCALE) {
            NSMenu *localeMenu = [[NSMenu alloc] initWithTitle:label];
            [menuItem setSubmenu:localeMenu];

            size_t i = 0;
            for(auto locale : Locales()) {
                NSMenuItem *localeMenuItem =
                    [localeMenu addItemWithTitle:
                            Wrap(locale.displayName)
                        action:NULL keyEquivalent:@""];
                [localeMenuItem setTag:(NSInteger)i++];
                [localeMenuItem setTarget:[MainMenuResponder class]];
                [localeMenuItem setAction:@selector(handleLocale:)];
            }
        }

        mainMenuItems[(uint32_t)entry->id] = menuItem;

        ++entry;
    }
}

void EnableMenuByCmd(SolveSpace::Command cmd, bool enabled) {
    [mainMenuItems[(uint32_t)cmd] setEnabled:enabled];
}

void CheckMenuByCmd(SolveSpace::Command cmd, bool checked) {
    [mainMenuItems[(uint32_t)cmd] setState:(checked ? NSOnState : NSOffState)];
}

void RadioMenuByCmd(SolveSpace::Command cmd, bool selected) {
    CheckMenuByCmd(cmd, selected);
}

static void RefreshRecentMenu(SolveSpace::Command cmd, SolveSpace::Command base) {
    NSMenuItem *recent = mainMenuItems[(uint32_t)cmd];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    [recent setSubmenu:menu];

    if(RecentFile[0].IsEmpty()) {
        NSMenuItem *placeholder = [[NSMenuItem alloc]
            initWithTitle:Wrap(_("(no recent files)")) action:nil keyEquivalent:@""];
        [placeholder setEnabled:NO];
        [menu addItem:placeholder];
    } else {
        for(size_t i = 0; i < MAX_RECENT; i++) {
            if(RecentFile[i].IsEmpty()) break;

            NSMenuItem *item = [[NSMenuItem alloc]
                initWithTitle:[Wrap(RecentFile[i].raw)
                    stringByAbbreviatingWithTildeInPath]
                action:nil keyEquivalent:@""];
            [item setTag:((uint32_t)base + i)];
            [item setAction:@selector(handleRecent:)];
            [item setTarget:[MainMenuResponder class]];
            [menu addItem:item];
        }
    }
}

void RefreshRecentMenus() {
    RefreshRecentMenu(Command::OPEN_RECENT, Command::RECENT_OPEN);
    RefreshRecentMenu(Command::GROUP_RECENT, Command::RECENT_LINK);
}

void ToggleMenuBar() {
    [NSMenu setMenuBarVisible:![NSMenu menuBarVisible]];
}

bool MenuBarIsVisible() {
    return [NSMenu menuBarVisible];
}
}

/* Save/load */

bool SolveSpace::GetOpenFile(Platform::Path *filename, const std::string &defExtension,
                             const FileFilter ssFilters[]) {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSMutableArray *filters = [[NSMutableArray alloc] init];
    for(const FileFilter *ssFilter = ssFilters; ssFilter->name; ssFilter++) {
        for(const char *const *ssPattern = ssFilter->patterns; *ssPattern; ssPattern++) {
            [filters addObject:[NSString stringWithUTF8String:*ssPattern]];
        }
    }
    [filters removeObjectIdenticalTo:@"*"];
    [panel setAllowedFileTypes:filters];

    if([panel runModal] == NSFileHandlingPanelOKButton) {
        *filename = Platform::Path::From(
            [[NSFileManager defaultManager]
                fileSystemRepresentationWithPath:[[panel URL] path]]);
        return true;
    } else {
        return false;
    }
}

@interface SaveFormatController : NSViewController
@property NSSavePanel *panel;
@property NSArray *extensions;
@property (nonatomic) IBOutlet NSPopUpButton *button;
@property (nonatomic) NSInteger index;
@end

@implementation SaveFormatController
@synthesize panel, extensions, button, index;
- (void)setIndex:(NSInteger)newIndex {
    self->index = newIndex;
    NSString *extension = [extensions objectAtIndex:newIndex];
    if(![extension isEqual:@"*"]) {
        NSString *filename = [panel nameFieldStringValue];
        NSString *basename = [[filename componentsSeparatedByString:@"."] objectAtIndex:0];
        [panel setNameFieldStringValue:[basename stringByAppendingPathExtension:extension]];
    }
}
@end

bool SolveSpace::GetSaveFile(Platform::Path *filename, const std::string &defExtension,
                             const FileFilter ssFilters[]) {
    NSSavePanel *panel = [NSSavePanel savePanel];

    SaveFormatController *controller =
        [[SaveFormatController alloc] initWithNibName:@"SaveFormatAccessory" bundle:nil];
    [controller setPanel:panel];
    [panel setAccessoryView:[controller view]];

    NSMutableArray *extensions = [[NSMutableArray alloc] init];

    NSPopUpButton *button = [controller button];
    [button removeAllItems];
    for(const FileFilter *ssFilter = ssFilters; ssFilter->name; ssFilter++) {
        std::string desc;
        for(const char *const *ssPattern = ssFilter->patterns; *ssPattern; ssPattern++) {
            if(desc == "") {
                desc = *ssPattern;
            } else {
                desc += ", ";
                desc += *ssPattern;
            }
        }
        std::string title = Translate(ssFilter->name) + " (" + desc + ")";
        [button addItemWithTitle:Wrap(title)];
        [extensions addObject:[NSString stringWithUTF8String:ssFilter->patterns[0]]];
    }
    [panel setAllowedFileTypes:extensions];
    [controller setExtensions:extensions];

    int extensionIndex = 0;
    if(defExtension != "") {
        extensionIndex = [extensions indexOfObject:Wrap(defExtension)];
        if(extensionIndex == -1) {
            extensionIndex = 0;
        }
    }
    [button selectItemAtIndex:extensionIndex];

    if(filename->IsEmpty()) {
        [panel setNameFieldStringValue:
            [Wrap(_("untitled"))
                stringByAppendingPathExtension:[extensions objectAtIndex:extensionIndex]]];
    } else {
        [panel setDirectoryURL:
            [NSURL fileURLWithPath:Wrap(filename->Parent().raw)
                   isDirectory:NO]];
        [panel setNameFieldStringValue:
            [Wrap(filename->FileStem())
                stringByAppendingPathExtension:[extensions objectAtIndex:extensionIndex]]];
    }

    if([panel runModal] == NSFileHandlingPanelOKButton) {
        *filename = Platform::Path::From(
            [[NSFileManager defaultManager]
                fileSystemRepresentationWithPath:[[panel URL] path]]);
        return true;
    } else {
        return false;
    }
}

SolveSpace::DialogChoice SolveSpace::SaveFileYesNoCancel() {
    NSAlert *alert = [[NSAlert alloc] init];
    if(!SolveSpace::SS.saveFile.IsEmpty()) {
        [alert setMessageText:
            [[@"Do you want to save the changes you made to the sketch “"
             stringByAppendingString:
                [Wrap(SolveSpace::SS.saveFile.raw)
                    stringByAbbreviatingWithTildeInPath]]
             stringByAppendingString:@"”?"]];
    } else {
        [alert setMessageText:
            Wrap(_("Do you want to save the changes you made to the new sketch?"))];
    }
    [alert setInformativeText:Wrap(_("Your changes will be lost if you don't save them."))];
    [alert addButtonWithTitle:Wrap(C_("button", "Save"))];
    [alert addButtonWithTitle:Wrap(C_("button", "Cancel"))];
    [alert addButtonWithTitle:Wrap(C_("button", "Don't Save"))];
    switch([alert runModal]) {
        case NSAlertFirstButtonReturn:
        return DIALOG_YES;
        case NSAlertSecondButtonReturn:
        default:
        return DIALOG_CANCEL;
        case NSAlertThirdButtonReturn:
        return DIALOG_NO;
    }
}

SolveSpace::DialogChoice SolveSpace::LoadAutosaveYesNo() {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:
        Wrap(_("An autosave file is availible for this project."))];
    [alert setInformativeText:
        Wrap(_("Do you want to load the autosave file instead?"))];
    [alert addButtonWithTitle:Wrap(C_("button", "Load"))];
    [alert addButtonWithTitle:Wrap(C_("button", "Don't Load"))];
    switch([alert runModal]) {
        case NSAlertFirstButtonReturn:
        return DIALOG_YES;
        case NSAlertSecondButtonReturn:
        default:
        return DIALOG_NO;
    }
}

SolveSpace::DialogChoice SolveSpace::LocateImportedFileYesNoCancel(
                            const Platform::Path &filename, bool canCancel) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:
        Wrap("The linked file “" + filename.raw + "” is not present.")];
    [alert setInformativeText:
        Wrap(_("Do you want to locate it manually?\n"
               "If you select “No”, any geometry that depends on "
               "the missing file will be removed."))];
    [alert addButtonWithTitle:Wrap(C_("button", "Yes"))];
    if(canCancel)
        [alert addButtonWithTitle:Wrap(C_("button", "Cancel"))];
    [alert addButtonWithTitle:Wrap(C_("button", "No"))];
    switch([alert runModal]) {
        case NSAlertFirstButtonReturn:
        return DIALOG_YES;
        case NSAlertSecondButtonReturn:
        default:
        if(canCancel)
            return DIALOG_CANCEL;
        /* fallthrough */
        case NSAlertThirdButtonReturn:
        return DIALOG_NO;
    }
}

/* Text window */

@interface TextWindowView : GLViewWithEditor
{
    NSTrackingArea *trackingArea;
}

@property (nonatomic, getter=isCursorHand) BOOL cursorHand;
@end

@implementation TextWindowView
- (BOOL)isFlipped {
    return YES;
}

- (void)drawGL {
    SolveSpace::SS.TW.Paint();
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    return YES;
}

- (void) createTrackingArea {
    trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
        options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                 NSTrackingActiveAlways)
        owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
}

- (void) updateTrackingAreas
{
    [self removeTrackingArea:trackingArea];
    [self createTrackingArea];
    [super updateTrackingAreas];
}

- (void)mouseMoved:(NSEvent*)event {
    NSPoint point = [self convertPointToBacking:
        [self convertPoint:[event locationInWindow] fromView:nil]];
    SolveSpace::SS.TW.MouseEvent(/*leftClick*/ false, /*leftDown*/ false,
                                 point.x, -point.y);
}

- (void)mouseDown:(NSEvent*)event {
    NSPoint point = [self convertPointToBacking:
        [self convertPoint:[event locationInWindow] fromView:nil]];
    SolveSpace::SS.TW.MouseEvent(/*leftClick*/ true, /*leftDown*/ true,
                                 point.x, -point.y);
}

- (void)mouseDragged:(NSEvent*)event {
    NSPoint point = [self convertPointToBacking:
        [self convertPoint:[event locationInWindow] fromView:nil]];
    SolveSpace::SS.TW.MouseEvent(/*leftClick*/ false, /*leftDown*/ true,
                                 point.x, -point.y);
}

- (void)setCursorHand:(BOOL)cursorHand {
    if(_cursorHand != cursorHand) {
        if(cursorHand)
            [[NSCursor pointingHandCursor] push];
        else
            [NSCursor pop];
    }
    _cursorHand = cursorHand;
}

- (void)mouseExited:(NSEvent*)event {
    [self setCursorHand:FALSE];
    SolveSpace::SS.TW.MouseLeave();
}

- (void)startEditing:(NSString*)text at:(NSPoint)point {
    point = [self convertPointFromBacking:point];
    point.y = -point.y + 2;
    [[self window] makeKeyWindow];
    [super startEditing:text at:point withHeight:15.0 usingMonospace:TRUE];
    [editor setFrameSize:(NSSize){
        .width = [self bounds].size.width - [editor frame].origin.x,
        .height = [editor intrinsicContentSize].height }];
}

- (void)stopEditing {
    [super stopEditing];
    [GW makeKeyWindow];
}

- (void)didEdit:(NSString*)text {
    SolveSpace::SS.TW.EditControlDone([text UTF8String]);
}

- (void)cancelOperation:(id)sender {
    [self stopEditing];
}
@end

@interface TextWindowDelegate : NSObject<NSWindowDelegate>
- (BOOL)windowShouldClose:(id)sender;
- (void)windowDidResize:(NSNotification *)notification;
@end

@implementation TextWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    SolveSpace::GraphicsWindow::MenuView(SolveSpace::Command::SHOW_TEXT_WND);
    return NO;
}

- (void)windowDidResize:(NSNotification *)notification {
    NSClipView *view = [[[notification object] contentView] contentView];
    NSView *document = [view documentView];
    NSSize size = [document frame].size;
    size.width = [view frame].size.width;
    [document setFrameSize:size];
}
@end

static NSPanel *TW;
static TextWindowView *TWView;
static TextWindowDelegate *TWDelegate;

namespace SolveSpace {
void InitTextWindow() {
    TW = [[NSPanel alloc] init];
    TWDelegate = [[TextWindowDelegate alloc] init];
    [TW setStyleMask:(NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask |
                      NSUtilityWindowMask)];
    [[TW standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
    [[TW standardWindowButton:NSWindowZoomButton] setHidden:YES];
    [TW setFrameAutosaveName:@"TextWindow"];
    [TW setFloatingPanel:YES];
    [TW setBecomesKeyOnlyIfNeeded:YES];

    NSScrollView *scrollView = [[NSScrollView alloc] init];
    [TW setContentView:scrollView];
    [scrollView setBackgroundColor:[NSColor blackColor]];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setScrollerKnobStyle:NSScrollerKnobStyleLight];
    [[scrollView contentView] setCopiesOnScroll:YES];

    TWView = [[TextWindowView alloc] init];
    [scrollView setDocumentView:TWView];

    [TW setDelegate:TWDelegate];
    if(![TW setFrameUsingName:[TW frameAutosaveName]])
        [TW setContentSize:(NSSize){ .width = 420, .height = 300 }];
    [TWView setFrame:[[scrollView contentView] frame]];
}

void ShowTextWindow(bool visible) {
    if(visible)
        [TW orderFront:nil];
    else
        [TW close];
}

void GetTextWindowSize(int *w, int *h) {
    NSSize size = [TWView convertSizeToBacking:[TWView frame].size];
    *w = (int)size.width;
    *h = (int)size.height;
}

double GetScreenDpi() {
    NSScreen *screen = [NSScreen mainScreen];
    NSDictionary *description = [screen deviceDescription];
    NSSize displayPixelSize = [[description objectForKey:NSDeviceSize] sizeValue];
    CGSize displayPhysicalSize = CGDisplayScreenSize(
                [[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);
    return (displayPixelSize.width / displayPhysicalSize.width) * 25.4f;
}

void InvalidateText() {
    NSSize size = [TWView convertSizeToBacking:[TWView frame].size];
    size.height = (SS.TW.top[SS.TW.rows - 1] + 1) * TextWindow::LINE_HEIGHT / 2;
    [TWView setFrameSize:[TWView convertSizeFromBacking:size]];
    [TWView setNeedsDisplay:YES];
}

void MoveTextScrollbarTo(int pos, int maxPos, int page) {
    /* unused; we draw the entire text window and scroll in Cocoa */
}

void SetMousePointerToHand(bool is_hand) {
    [TWView setCursorHand:is_hand];
}

void ShowTextEditControl(int x, int y, const std::string &str) {
    return [TWView startEditing:Wrap(str)
                   at:(NSPoint){(CGFloat)x, (CGFloat)y}];
}

void HideTextEditControl() {
    return [TWView stopEditing];
}

bool TextEditControlIsVisible() {
    return [TWView isEditing];
}
};

/* Miscellanea */

void SolveSpace::DoMessageBox(const char *str, int rows, int cols, bool error) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setAlertStyle:(error ? NSWarningAlertStyle : NSInformationalAlertStyle)];
    [alert addButtonWithTitle:Wrap(C_("button", "OK"))];

    /* do some additional formatting of the message;
       these are heuristics, but they are made failsafe and lead to nice results. */
    NSString *input = [NSString stringWithUTF8String:str];
    NSRange dot = [input rangeOfCharacterFromSet:
        [NSCharacterSet characterSetWithCharactersInString:@".:"]];
    if(dot.location != NSNotFound) {
        [alert setMessageText:[[input substringToIndex:dot.location + 1]
            stringByReplacingOccurrencesOfString:@"\n" withString:@" "]];
        [alert setInformativeText:
            [[input substringFromIndex:dot.location + 1]
                stringByTrimmingCharactersInSet:[NSCharacterSet newlineCharacterSet]]];
    } else {
        [alert setMessageText:[input
            stringByReplacingOccurrencesOfString:@"\n" withString:@" "]];
    }

    [alert runModal];
}

void SolveSpace::OpenWebsite(const char *url) {
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:[NSString stringWithUTF8String:url]]];
}

std::vector<SolveSpace::Platform::Path> SolveSpace::GetFontFiles() {
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

/* Application lifecycle */

@interface ApplicationDelegate : NSObject<NSApplicationDelegate>
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
- (void)applicationWillTerminate:(NSNotification *)aNotification;
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
- (IBAction)preferences:(id)sender;
@end

@implementation ApplicationDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    if(SolveSpace::SS.OkayToStartNewFile())
        return NSTerminateNow;
    else
        return NSTerminateCancel;
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
    SolveSpace::SS.Exit();
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename {
    SolveSpace::Platform::Path path = SolveSpace::Platform::Path::From([filename UTF8String]);
    return SolveSpace::SS.Load(path.Expand(/*fromCurrentDirectory=*/true));
}

- (IBAction)preferences:(id)sender {
    SolveSpace::SS.TW.GoToScreen(SolveSpace::TextWindow::Screen::CONFIGURATION);
    SolveSpace::SS.ScheduleShowTW();
}
@end

void SolveSpace::RefreshLocale() {
    SS.UpdateWindowTitle();
    SolveSpace::InitMainMenu([NSApp mainMenu]);
    RefreshRecentMenus();

    [TW setTitle:Wrap(C_("title", "Property Browser"))];
}

void SolveSpace::ExitNow() {
    [NSApp stop:nil];
}

/*
 * Normally we would just link to the 3DconnexionClient framework.
 * We don't want to (are not allowed to) distribute the official
 * framework, so we're trying to use the one installed on the users
 * computer. There are some different versions of the framework,
 * the official one and re-implementations using an open source driver
 * for older devices (spacenav-plus). So weak-linking isn't an option,
 * either. The only remaining way is using CFBundle to dynamically
 * load the library at runtime, and also detect its availability.
 *
 * We're also defining everything needed from the 3DconnexionClientAPI,
 * so we're not depending on the API headers.
 */

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

static BOOL connexionShiftIsDown = NO;
static UInt16 connexionClient = 0;
static UInt32 connexionSignature = 'SoSp';
static UInt8 *connexionName = (UInt8 *)"SolveSpace";
static CFBundleRef spaceBundle = NULL;
static InstallConnexionHandlersProc installConnexionHandlers = NULL;
static CleanupConnexionHandlersProc cleanupConnexionHandlers = NULL;
static RegisterConnexionClientProc registerConnexionClient = NULL;
static UnregisterConnexionClientProc unregisterConnexionClient = NULL;

static void connexionAdded(io_connect_t con) {}
static void connexionRemoved(io_connect_t con) {}
static void connexionMessage(io_connect_t con, natural_t type, void *arg) {
    if (type != kConnexionMsgDeviceState) {
        return;
    }

    ConnexionDeviceState *device = (ConnexionDeviceState *)arg;

    dispatch_async(dispatch_get_main_queue(), ^(void){
        SolveSpace::SS.GW.SpaceNavigatorMoved(
            (double)device->axis[0] * -0.25,
            (double)device->axis[1] * -0.25,
            (double)device->axis[2] * 0.25,
            (double)device->axis[3] * -0.0005,
            (double)device->axis[4] * -0.0005,
            (double)device->axis[5] * -0.0005,
            (connexionShiftIsDown == YES) ? 1 : 0
        );
    });
}

static void connexionInit() {
    NSString *bundlePath = @"/Library/Frameworks/3DconnexionClient.framework";
    NSURL *bundleURL = [NSURL fileURLWithPath:bundlePath];
    spaceBundle = CFBundleCreate(kCFAllocatorDefault, (__bridge CFURLRef)bundleURL);

    // Don't continue if no Spacemouse driver is installed on this machine
    if (spaceBundle == NULL) {
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
    if ((installConnexionHandlers == NULL) || (cleanupConnexionHandlers == NULL)
            || (registerConnexionClient == NULL) || (unregisterConnexionClient == NULL)) {
        CFRelease(spaceBundle);
        spaceBundle = NULL;
        return;
    }

    installConnexionHandlers(&connexionMessage, &connexionAdded, &connexionRemoved);
    connexionClient = registerConnexionClient(connexionSignature, connexionName,
            kConnexionClientModeTakeOver, kConnexionMaskButtons | kConnexionMaskAxis);

    // Monitor modifier flags to detect Shift button state changes
    [NSEvent addLocalMonitorForEventsMatchingMask:(NSKeyDownMask | NSFlagsChangedMask)
                                          handler:^(NSEvent *event) {
        if (event.modifierFlags & NSShiftKeyMask) {
            connexionShiftIsDown = YES;
        }
        return event;
    }];

    [NSEvent addLocalMonitorForEventsMatchingMask:(NSKeyUpMask | NSFlagsChangedMask)
                                          handler:^(NSEvent *event) {
        if (!(event.modifierFlags & NSShiftKeyMask)) {
            connexionShiftIsDown = NO;
        }
        return event;
    }];
}

static void connexionClose() {
    if (spaceBundle == NULL) {
        return;
    }

    unregisterConnexionClient(connexionClient);
    cleanupConnexionHandlers();

    CFRelease(spaceBundle);
}

int main(int argc, const char *argv[]) {
    [NSApplication sharedApplication];
    ApplicationDelegate *delegate = [[ApplicationDelegate alloc] init];
    [NSApp setDelegate:delegate];

    SolveSpace::InitGraphicsWindow();
    SolveSpace::InitTextWindow();
    [[NSBundle mainBundle] loadNibNamed:@"MainMenu" owner:nil topLevelObjects:nil];

    NSArray *languages = [NSLocale preferredLanguages];
    for(NSString *language in languages) {
        dbp("%s", ([language UTF8String]));
        if(SolveSpace::SetLocale([language UTF8String])) break;
    }
    if([languages count] == 0) {
        SolveSpace::SetLocale("en_US");
    }

    connexionInit();
    SolveSpace::SS.Init();

    [GW makeKeyAndOrderFront:nil];
    [NSApp run];

    connexionClose();
    SolveSpace::SK.Clear();
    SolveSpace::SS.Clear();

    return 0;
}
