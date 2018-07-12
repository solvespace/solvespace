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
// Settings
//-----------------------------------------------------------------------------

class Setting {
public:
    enum class Type {
        Undefined,
        Int,
        Float,
        String
    };

    Type        type;
    int         valueInt;
    float       valueFloat;
    std::string valueString;

    void CheckType(Type expectedType) {
        ssassert(type == Setting::Type::Undefined ||
                 type == expectedType, "Wrong setting type");
        type = expectedType;
    }
};

std::map<std::string, Setting> settings;

void CnfFreezeInt(uint32_t val, const std::string &key) {
    Setting &setting = settings[key];
    setting.CheckType(Setting::Type::Int);
    setting.valueInt = val;
}
uint32_t CnfThawInt(uint32_t val, const std::string &key) {
    if(settings.find(key) != settings.end()) {
        Setting &setting = settings[key];
        setting.CheckType(Setting::Type::Int);
        val = setting.valueInt;
    }
    return val;
}

void CnfFreezeFloat(float val, const std::string &key) {
    Setting &setting = settings[key];
    setting.CheckType(Setting::Type::Float);
    setting.valueFloat = val;
}
float CnfThawFloat(float val, const std::string &key) {
    if(settings.find(key) != settings.end()) {
        Setting &setting = settings[key];
        setting.CheckType(Setting::Type::Float);
        val = setting.valueFloat;
    }
    return val;
}

void CnfFreezeString(const std::string &val, const std::string &key) {
    Setting &setting = settings[key];
    setting.CheckType(Setting::Type::String);
    setting.valueString = val;
}
std::string CnfThawString(const std::string &val, const std::string &key) {
    std::string ret = val;
    if(settings.find(key) != settings.end()) {
        Setting &setting = settings[key];
        setting.CheckType(Setting::Type::String);
        ret = setting.valueString;
    }
    return ret;
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
