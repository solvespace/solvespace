//-----------------------------------------------------------------------------
// Our platform support functions for the headless (no OpenGL) test runner.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include <cairo.h>

namespace SolveSpace {

//-----------------------------------------------------------------------------
// Rendering
//-----------------------------------------------------------------------------

std::shared_ptr<ViewportCanvas> CreateRenderer() {
    return std::make_shared<CairoPixmapRenderer>();
}

namespace Platform {

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

class SettingsImplDummy : public Settings {
public:
    void FreezeInt(const std::string &key, uint32_t value) {}

    uint32_t ThawInt(const std::string &key, uint32_t defaultValue = 0) {
        return defaultValue;
    }

    void FreezeFloat(const std::string &key, double value) {}

    double ThawFloat(const std::string &key, double defaultValue = 0.0) {
        return defaultValue;
    }

    void FreezeString(const std::string &key, const std::string &value) {}

    std::string ThawString(const std::string &key,
                           const std::string &defaultValue = "") {
        return defaultValue;
    }
};

SettingsRef GetSettings() {
    static std::shared_ptr<SettingsImplDummy> settings =
                std::make_shared<SettingsImplDummy>();
    return settings;
}

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

class TimerImplDummy : public Timer {
public:
    void WindUp(unsigned milliseconds) override {}
};

TimerRef CreateTimer() {
    return std::unique_ptr<Timer>(new TimerImplDummy);
}

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

MenuRef CreateMenu() {
    return std::shared_ptr<Menu>();
}

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    *unique = false;
    return std::shared_ptr<MenuBar>();
}

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    return std::shared_ptr<Window>();
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

void Exit() {
    exit(0);
}

}

//-----------------------------------------------------------------------------
// Dialogs
//-----------------------------------------------------------------------------

bool GetOpenFile(Platform::Path *filename, const std::string &activeOrEmpty,
                 const FileFilter filters[]) {
    ssassert(false, "Not implemented");
}
bool GetSaveFile(Platform::Path *filename, const std::string &activeOrEmpty,
                 const FileFilter filters[]) {
    ssassert(false, "Not implemented");
}
DialogChoice SaveFileYesNoCancel() {
    ssassert(false, "Not implemented");
}
DialogChoice LoadAutosaveYesNo() {
    ssassert(false, "Not implemented");
}
DialogChoice LocateImportedFileYesNoCancel(const Platform::Path &filename,
                                           bool canCancel) {
    ssassert(false, "Not implemented");
}
void DoMessageBox(const char *message, int rows, int cols, bool error) {
    dbp("%s box: %s", error ? "error" : "message", message);
    ssassert(false, "Not implemented");
}
void OpenWebsite(const char *url) {
    ssassert(false, "Not implemented");
}

//-----------------------------------------------------------------------------
// Resources
//-----------------------------------------------------------------------------

std::vector<Platform::Path> fontFiles;
std::vector<Platform::Path> GetFontFiles() {
    return fontFiles;
}

}
