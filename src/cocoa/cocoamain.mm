//-----------------------------------------------------------------------------
// Our main() function, and Cocoa-specific stuff to set up our windows and
// otherwise handle our interface to the operating system. Everything
// outside gtk/... should be standard C++ and OpenGL.
//
// Copyright 2015 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#include <mach/mach.h>
#include <mach/clock.h>

#import <AppKit/AppKit.h>

#include <iostream>
#include <map>

#include "solvespace.h"
#include "../unix/gloffscreen.h"
#include <config.h>

using SolveSpace::dbp;

#define GL_CHECK() \
    do { \
        int err = (int)glGetError(); \
        if(err) dbp("%s:%d: glGetError() == 0x%X", __FILE__, __LINE__, err); \
    } while (0)

/* Settings */

namespace SolveSpace {
void CnfFreezeInt(uint32_t val, const std::string &key) {
    [[NSUserDefaults standardUserDefaults]
        setInteger:val forKey:[NSString stringWithUTF8String:key.c_str()]];
}

uint32_t CnfThawInt(uint32_t val, const std::string &key) {
    NSString *nsKey = [NSString stringWithUTF8String:key.c_str()];
    if([[NSUserDefaults standardUserDefaults] objectForKey:nsKey])
        return [[NSUserDefaults standardUserDefaults] integerForKey:nsKey];
    return val;
}

void CnfFreezeFloat(float val, const std::string &key) {
    [[NSUserDefaults standardUserDefaults]
        setFloat:val forKey:[NSString stringWithUTF8String:key.c_str()]];
}

float CnfThawFloat(float val, const std::string &key) {
    NSString *nsKey = [NSString stringWithUTF8String:key.c_str()];
    if([[NSUserDefaults standardUserDefaults] objectForKey:nsKey])
        return [[NSUserDefaults standardUserDefaults] floatForKey:nsKey];
    return val;
}

void CnfFreezeString(const std::string &val, const std::string &key) {
    [[NSUserDefaults standardUserDefaults]
        setObject:[NSString stringWithUTF8String:val.c_str()]
        forKey:[NSString stringWithUTF8String:key.c_str()]];
}

std::string CnfThawString(const std::string &val, const std::string &key) {
    NSString *nsKey = [NSString stringWithUTF8String:key.c_str()];
    if([[NSUserDefaults standardUserDefaults] objectForKey:nsKey]) {
        NSString *nsNewVal = [[NSUserDefaults standardUserDefaults] stringForKey:nsKey];
        return [nsNewVal UTF8String];
    }
    return val;
}
};

/* Timer */

int64_t SolveSpace::GetMilliseconds(void) {
    clock_serv_t cclock;
    mach_timespec_t mts;

    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);

    return mts.tv_sec * 1000 + mts.tv_nsec / 1000000;
}

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
- (void)startEditing:(NSString*)text at:(NSPoint)origin;
- (void)stopEditing;
- (void)didEdit:(NSString*)text;
@end

@implementation GLViewWithEditor
{
    GLOffscreen *offscreen;
    NSOpenGLContext *glContext;
@protected
    NSTextField *editor;
}

- initWithFrame:(NSRect)frameRect {
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
    [editor setTarget:self];
    [editor setAction:@selector(editorAction:)];

    return self;
}

- (void)dealloc {
    delete offscreen;
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

    if(!offscreen)
        offscreen = new GLOffscreen;

    NSSize size = [self convertSizeToBacking:[self bounds].size];
    NSRect bounds = [self convertRectToBacking:[self bounds]];
    offscreen->begin(size.width, size.height);

    [self drawGL];
    GL_CHECK();

    uint8_t *pixels = offscreen->end(![self isFlipped]);
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL, pixels, size.width * size.height * 4, NULL);
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(size.width, size.height, 8, 32,
        size.width * 4, colorspace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
        provider, NULL, true, kCGRenderingIntentDefault);

    CGContextDrawImage((CGContextRef) [[NSGraphicsContext currentContext] graphicsPort],
                       [self bounds], image);
}

- (void)drawGL {
}

@synthesize editing;

- (void)startEditing:(NSString*)text at:(NSPoint)origin {
    if(!self->editing) {
        [self addSubview:editor];
        self->editing = YES;
    }

    [editor setFrameOrigin:origin];
    [editor setStringValue:text];
    [self prepareEditor];
    [[self window] becomeKeyWindow];
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

- (void)prepareEditor {
    [editor setFrameSize:(NSSize){
        .width = 100,
        .height = [editor intrinsicContentSize].height }];
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
    SolveSpace::SS.GW.MouseScroll(point.x, point.y, -[event deltaY]);
}

- (void)mouseExited:(NSEvent*)event {
    SolveSpace::SS.GW.MouseLeave();
}

- (void)keyDown:(NSEvent*)event {
    int chr = 0;
    if(NSString *nsChr = [event charactersIgnoringModifiers])
        chr = [nsChr characterAtIndex:0];

    if(chr == NSDeleteCharacter) /* map delete back to backspace */
        chr = '\b';
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

- (void)startEditing:(NSString*)text at:(NSPoint)xy {
    // Convert to ij (vs. xy) style coordinates
    NSSize size = [self convertSizeToBacking:[self bounds].size];
    NSPoint point = {
        .x = xy.x + size.width / 2,
        .y = xy.y - size.height / 2 + [editor intrinsicContentSize].height
    };
    [super startEditing:text at:[self convertPointFromBacking:point]];
}

- (void)didEdit:(NSString*)text {
    SolveSpace::SS.GW.EditControlDone([text UTF8String]);
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
    *w = size.width;
    *h = size.height;
}

void InvalidateGraphics(void) {
    [GWView setNeedsDisplay:YES];
}

void PaintGraphics(void) {
    [GWView setNeedsDisplay:YES];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, YES);
}

void SetCurrentFilename(const std::string &filename) {
    if(!filename.empty()) {
        [GW setTitleWithRepresentedFilename:[NSString stringWithUTF8String:filename.c_str()]];
    } else {
        [GW setTitle:@"(new sketch)"];
        [GW setRepresentedFilename:@""];
    }
}

void ToggleFullScreen(void) {
    [GW toggleFullScreen:nil];
}

bool FullScreenIsActive(void) {
    return [GWDelegate isFullscreen];
}

void ShowGraphicsEditControl(int x, int y, const std::string &str) {
    [GWView startEditing:[NSString stringWithUTF8String:str.c_str()]
            at:(NSPoint){(CGFloat)x, (CGFloat)y}];
}

void HideGraphicsEditControl(void) {
    [GWView stopEditing];
}

bool GraphicsEditControlIsVisible(void) {
    return [GWView isEditing];
}
}

/* Context menus */

static int contextMenuChoice;

@interface ContextMenuResponder : NSObject
+ (void)handleClick:(id)sender;
@end

@implementation ContextMenuResponder
+ (void)handleClick:(id)sender {
    contextMenuChoice = [sender tag];
}
@end

namespace SolveSpace {
NSMenu *contextMenu, *contextSubmenu;

void AddContextMenuItem(const char *label, int id_) {
    NSMenuItem *menuItem;
    if(label) {
        menuItem = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:label]
            action:@selector(handleClick:) keyEquivalent:@""];
        [menuItem setTarget:[ContextMenuResponder class]];
        [menuItem setTag:id_];
    } else {
        menuItem = [NSMenuItem separatorItem];
    }

    if(id_ == CONTEXT_SUBMENU) {
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

void CreateContextSubmenu(void) {
    if(contextSubmenu) oops();

    contextSubmenu = [[NSMenu alloc] initWithTitle:@""];
}

int ShowContextMenu(void) {
    if(!contextMenu)
        return -1;

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
    int id_ = [sender tag];
    if(id_ >= RECENT_OPEN && id_ < (RECENT_OPEN + MAX_RECENT))
        SolveSpace::SolveSpaceUI::MenuFile(id_);
    else if(id_ >= RECENT_IMPORT && id_ < (RECENT_IMPORT + MAX_RECENT))
        SolveSpace::Group::MenuGroup(id_);
}
@end

namespace SolveSpace {
std::map<int, NSMenuItem*> mainMenuItems;

void InitMainMenu(NSMenu *mainMenu) {
    NSMenuItem *menuItem = NULL;
    NSMenu *levels[5] = {mainMenu, 0};
    NSString *label;

    const GraphicsWindow::MenuEntry *entry = &GraphicsWindow::menu[0];
    int current_level = 0;
    while(entry->level >= 0) {
        if(entry->level > current_level) {
            NSMenu *menu = [[NSMenu alloc] initWithTitle:label];
            [menu setAutoenablesItems:NO];
            [menuItem setSubmenu:menu];

            if(entry->level >= sizeof(levels) / sizeof(levels[0]))
                oops();

            levels[entry->level] = menu;
        }

        current_level = entry->level;

        if(entry->label) {
            /* OS X does not support mnemonics */
            label = [[NSString stringWithUTF8String:entry->label]
                stringByReplacingOccurrencesOfString:@"&" withString:@""];

            unichar accel_char = entry->accel &
                ~(GraphicsWindow::SHIFT_MASK | GraphicsWindow::CTRL_MASK);
            if(accel_char > GraphicsWindow::FUNCTION_KEY_BASE &&
                    accel_char <= GraphicsWindow::FUNCTION_KEY_BASE + 12)
                accel_char = NSF1FunctionKey + (accel_char - GraphicsWindow::FUNCTION_KEY_BASE - 1);
            NSString *accel = [NSString stringWithCharacters:&accel_char length:1];

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

        mainMenuItems[entry->id] = menuItem;

        ++entry;
    }
}

void EnableMenuById(int id_, bool enabled) {
    [mainMenuItems[id_] setEnabled:enabled];
}

void CheckMenuById(int id_, bool checked) {
    [mainMenuItems[id_] setState:(checked ? NSOnState : NSOffState)];
}

void RadioMenuById(int id_, bool selected) {
    CheckMenuById(id_, selected);
}

static void RefreshRecentMenu(int id_, int base) {
    NSMenuItem *recent = mainMenuItems[id_];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    [recent setSubmenu:menu];

    if(std::string(RecentFile[0]).empty()) {
        NSMenuItem *placeholder = [[NSMenuItem alloc]
            initWithTitle:@"(no recent files)" action:nil keyEquivalent:@""];
        [placeholder setEnabled:NO];
        [menu addItem:placeholder];
    } else {
        for(int i = 0; i < MAX_RECENT; i++) {
            if(std::string(RecentFile[i]).empty())
                break;

            NSMenuItem *item = [[NSMenuItem alloc]
                initWithTitle:[[NSString stringWithUTF8String:RecentFile[i].c_str()]
                    stringByAbbreviatingWithTildeInPath]
                action:nil keyEquivalent:@""];
            [item setTag:(base + i)];
            [item setAction:@selector(handleRecent:)];
            [item setTarget:[MainMenuResponder class]];
            [menu addItem:item];
        }
    }
}

void RefreshRecentMenus(void) {
    RefreshRecentMenu(GraphicsWindow::MNU_OPEN_RECENT, RECENT_OPEN);
    RefreshRecentMenu(GraphicsWindow::MNU_GROUP_RECENT, RECENT_IMPORT);
}

void ToggleMenuBar(void) {
    [NSMenu setMenuBarVisible:![NSMenu menuBarVisible]];
}

bool MenuBarIsVisible(void) {
    return [NSMenu menuBarVisible];
}
}

/* Save/load */

bool SolveSpace::GetOpenFile(std::string &file, const std::string &defExtension,
                             const char *selPattern) {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    NSMutableArray *filters = [[NSMutableArray alloc] init];
    for(NSString *filter in [[NSString stringWithUTF8String:selPattern]
            componentsSeparatedByString:@"\n"]) {
        [filters addObjectsFromArray:
            [[[filter componentsSeparatedByString:@"\t"] objectAtIndex:1]
                componentsSeparatedByString:@","]];
    }
    [filters removeObjectIdenticalTo:@"*"];
    [panel setAllowedFileTypes:filters];

    if([panel runModal] == NSFileHandlingPanelOKButton) {
        file = [[NSFileManager defaultManager]
            fileSystemRepresentationWithPath:[[panel URL] path]];
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

bool SolveSpace::GetSaveFile(std::string &file, const std::string &defExtension,
                             const char *selPattern) {
    NSSavePanel *panel = [NSSavePanel savePanel];

    SaveFormatController *controller =
        [[SaveFormatController alloc] initWithNibName:@"SaveFormatAccessory" bundle:nil];
    [controller setPanel:panel];
    [panel setAccessoryView:[controller view]];

    NSMutableArray *extensions = [[NSMutableArray alloc] init];
    [controller setExtensions:extensions];

    NSPopUpButton *button = [controller button];
    [button removeAllItems];
    for(NSString *filter in [[NSString stringWithUTF8String:selPattern]
            componentsSeparatedByString:@"\n"]) {
        NSArray *filterParts = [filter componentsSeparatedByString:@"\t"];
        NSString *filterName = [filterParts objectAtIndex:0];
        NSArray *filterExtensions = [[filterParts objectAtIndex:1]
            componentsSeparatedByString:@","];
        [button addItemWithTitle:
            [[NSString alloc] initWithFormat:@"%@ (%@)", filterName,
                [filterExtensions componentsJoinedByString:@", "]]];
        [extensions addObject:[filterExtensions objectAtIndex:0]];
    }

    int extensionIndex = 0;
    if(defExtension != "") {
        extensionIndex = [extensions indexOfObject:
            [NSString stringWithUTF8String:defExtension.c_str()]];
    }

    [button selectItemAtIndex:extensionIndex];
    [panel setNameFieldStringValue:[@"untitled"
        stringByAppendingPathExtension:[extensions objectAtIndex:extensionIndex]]];

    if([panel runModal] == NSFileHandlingPanelOKButton) {
        file = [[NSFileManager defaultManager]
            fileSystemRepresentationWithPath:[[panel URL] path]];
        return true;
    } else {
        return false;
    }
}

SolveSpace::DialogChoice SolveSpace::SaveFileYesNoCancel(void) {
    NSAlert *alert = [[NSAlert alloc] init];
    if(!std::string(SolveSpace::SS.saveFile).empty()) {
        [alert setMessageText:
            [[@"Do you want to save the changes you made to the sketch “"
             stringByAppendingString:
                [[NSString stringWithUTF8String:SolveSpace::SS.saveFile.c_str()]
                    stringByAbbreviatingWithTildeInPath]]
             stringByAppendingString:@"”?"]];
    } else {
        [alert setMessageText:@"Do you want to save the changes you made to the new sketch?"];
    }
    [alert setInformativeText:@"Your changes will be lost if you don't save them."];
    [alert addButtonWithTitle:@"Save"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert addButtonWithTitle:@"Don't Save"];
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

SolveSpace::DialogChoice SolveSpace::LoadAutosaveYesNo(void) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:
        @"An autosave file is availible for this project."];
    [alert setInformativeText:
        @"Do you want to load the autosave file instead?"];
    [alert addButtonWithTitle:@"Load"];
    [alert addButtonWithTitle:@"Don't Load"];
    switch([alert runModal]) {
        case NSAlertFirstButtonReturn:
        return DIALOG_YES;
        case NSAlertSecondButtonReturn:
        default:
        return DIALOG_NO;
    }
}

SolveSpace::DialogChoice SolveSpace::LocateImportedFileYesNoCancel(
                            const std::string &filename, bool canCancel) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithUTF8String:
        ("The imported file " + filename + " is not present.").c_str()]];
    [alert setInformativeText:
        @"Do you want to locate it manually?\n"
         "If you select \"No\", any geometry that depends on "
         "the missing file will be removed."];
    [alert addButtonWithTitle:@"Yes"];
    if(canCancel)
        [alert addButtonWithTitle:@"Cancel"];
    [alert addButtonWithTitle:@"No"];
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
    point.y = -point.y;
    [super startEditing:text at:point];
    [[self window] makeKeyWindow];
    [[self window] makeFirstResponder:editor];
}

- (void)stopEditing {
    [super stopEditing];
    [GW makeKeyWindow];
}

- (void)didEdit:(NSString*)text {
    SolveSpace::SS.TW.EditControlDone([text UTF8String]);
}

- (void)prepareEditor {
    [editor setFrameSize:(NSSize){
        .width = [self bounds].size.width - [editor frame].origin.x,
        .height = [editor intrinsicContentSize].height }];
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
    SolveSpace::GraphicsWindow::MenuView(SolveSpace::GraphicsWindow::MNU_SHOW_TEXT_WND);
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
    [TW setTitle:@"Browser"];
    [TW setFrameAutosaveName:@"TextWindow"];
    [TW setFloatingPanel:YES];
    [TW setBecomesKeyOnlyIfNeeded:YES];
    [GW addChildWindow:TW ordered:NSWindowAbove];

    NSScrollView *scrollView = [[NSScrollView alloc] init];
    [TW setContentView:scrollView];
    [scrollView setBackgroundColor:[NSColor blackColor]];
    [scrollView setHasVerticalScroller:YES];
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
    *w = size.width;
    *h = size.height;
}

void InvalidateText(void) {
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
    return [TWView startEditing:[NSString stringWithUTF8String:str.c_str()]
                   at:(NSPoint){(CGFloat)x, (CGFloat)y}];
}

void HideTextEditControl(void) {
    return [TWView stopEditing];
}

bool TextEditControlIsVisible(void) {
    return [TWView isEditing];
}
};

/* Miscellanea */

void SolveSpace::DoMessageBox(const char *str, int rows, int cols, bool error) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setAlertStyle:(error ? NSWarningAlertStyle : NSInformationalAlertStyle)];

    /* do some additional formatting of the message these are
       heuristics, but they are made failsafe and lead to nice results. */
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

void SolveSpace::LoadAllFontFiles(void) {
    NSArray *fontNames = [[NSFontManager sharedFontManager] availableFonts];
    for(NSString *fontName in fontNames) {
        CTFontDescriptorRef fontRef =
            CTFontDescriptorCreateWithNameAndSize ((__bridge CFStringRef)fontName, 10.0);
        CFURLRef url = (CFURLRef)CTFontDescriptorCopyAttribute(fontRef, kCTFontURLAttribute);
        NSString *fontPath = [NSString stringWithString:[(NSURL *)CFBridgingRelease(url) path]];
        if([[fontPath pathExtension] isEqual:@"ttf"]) {
            TtfFont tf = {};
            tf.fontFile = [[NSFileManager defaultManager]
                fileSystemRepresentationWithPath:fontPath];
            SS.fonts.l.Add(&tf);
        }
    }
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
    SolveSpace::SK.Clear();
    SolveSpace::SS.Clear();
    SolveSpace::SS.Exit();
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename {
    return SolveSpace::SS.OpenFile([filename UTF8String]);
}

- (IBAction)preferences:(id)sender {
    SolveSpace::SS.TW.GoToScreen(SolveSpace::TextWindow::SCREEN_CONFIGURATION);
    SolveSpace::SS.ScheduleShowTW();
}
@end

void SolveSpace::ExitNow(void) {
    [NSApp stop:nil];
}

int main(int argc, const char *argv[]) {
    [NSApplication sharedApplication];
    ApplicationDelegate *delegate = [[ApplicationDelegate alloc] init];
    [NSApp setDelegate:delegate];

    SolveSpace::InitGraphicsWindow();
    SolveSpace::InitTextWindow();
    [[NSBundle mainBundle] loadNibNamed:@"MainMenu" owner:nil topLevelObjects:nil];
    SolveSpace::InitMainMenu([NSApp mainMenu]);

    SolveSpace::SS.Init();

    [GW makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];

    return 0;
}
