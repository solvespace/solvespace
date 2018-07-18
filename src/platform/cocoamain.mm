//-----------------------------------------------------------------------------
// Our main() function, and Cocoa-specific stuff to set up our windows and
// otherwise handle our interface to the operating system. Everything
// outside platform/... should be standard C++ and OpenGL.
//
// Copyright 2015 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#include "solvespace.h"
#import  <AppKit/AppKit.h>

using SolveSpace::dbp;

/* Miscellanea */

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
- (IBAction)preferences:(id)sender;
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
@end

@implementation ApplicationDelegate
- (IBAction)preferences:(id)sender {
    SolveSpace::SS.TW.GoToScreen(SolveSpace::TextWindow::Screen::CONFIGURATION);
    SolveSpace::SS.ScheduleShowTW();
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename {
    SolveSpace::Platform::Path path = SolveSpace::Platform::Path::From([filename UTF8String]);
    return SolveSpace::SS.Load(path.Expand(/*fromCurrentDirectory=*/true));
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    [[[NSApp mainWindow] delegate] windowShouldClose:nil];
    return NSTerminateCancel;
}

- (void)applicationTerminatePrompt {
    SolveSpace::SS.MenuFile(SolveSpace::Command::EXIT);
}
@end


int main(int argc, const char *argv[]) {
    ApplicationDelegate *delegate = [[ApplicationDelegate alloc] init];
    [[NSApplication sharedApplication] setDelegate:delegate];

    [[NSBundle mainBundle] loadNibNamed:@"MainMenu" owner:nil topLevelObjects:nil];

    NSArray *languages = [NSLocale preferredLanguages];
    for(NSString *language in languages) {
        if(SolveSpace::SetLocale([language UTF8String])) break;
    }
    if([languages count] == 0) {
        SolveSpace::SetLocale("en_US");
    }

    SolveSpace::Platform::Open3DConnexion();
    SolveSpace::SS.Init();

    [NSApp run];

    SolveSpace::Platform::Close3DConnexion();
    SolveSpace::SK.Clear();
    SolveSpace::SS.Clear();

    return 0;
}
