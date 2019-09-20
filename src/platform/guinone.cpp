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
// Fatal errors
//-----------------------------------------------------------------------------

void FatalError(std::string message) {
    fprintf(stderr, "%s", message.c_str());

#if !defined(LIBRARY) && defined(HAVE_BACKTRACE)
    static void *ptrs[1024] = {};
    size_t nptrs = backtrace(ptrs, sizeof(ptrs) / sizeof(ptrs[0]));
    char **syms = backtrace_symbols(ptrs, nptrs);

    fprintf(stderr, "Backtrace:\n");
    if(syms != NULL) {
        for(size_t i = 0; i < nptrs; i++) {
            fprintf(stderr, "%2zu: %s\n", i, syms[i]);
        }
    } else {
        for(size_t i = 0; i < nptrs; i++) {
            fprintf(stderr, "%2zu: %p\n", i, ptrs[i]);
        }
    }
#else
    fprintf(stderr, "Backtrace support not compiled in.\n");
#endif

    abort();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

class SettingsImplDummy final : public Settings {
public:
    void FreezeInt(const std::string &key, uint32_t value) override {
    }

    uint32_t ThawInt(const std::string &key, uint32_t defaultValue = 0) override {
        return defaultValue;
    }

    void FreezeFloat(const std::string &key, double value) override {
    }

    double ThawFloat(const std::string &key, double defaultValue = 0.0) override {
        return defaultValue;
    }

    void FreezeString(const std::string &key, const std::string &value) override {
    }

    std::string ThawString(const std::string &key, const std::string &defaultValue = "") override {
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

class TimerImplDummy final : public Timer {
public:
    void RunAfter(unsigned milliseconds) override {}
};

TimerRef CreateTimer() {
    return std::make_shared<TimerImplDummy>();
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

void Request3DConnexionEventsForWindow(WindowRef window) {}

//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::shared_ptr<MessageDialog>();
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    return std::shared_ptr<FileDialog>();
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    return std::shared_ptr<FileDialog>();
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Platform::Path> fontFiles;
std::vector<Platform::Path> GetFontFiles() {
    return fontFiles;
}

void OpenInBrowser(const std::string &url) {}

void InitGui(int argc, char **argv) {}

void RunGui() {}

void ExitGui() {
    exit(0);
}

void ClearGui() {}

}

}
