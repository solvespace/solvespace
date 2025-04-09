//-----------------------------------------------------------------------------
// An abstraction for platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------

#ifndef SOLVESPACE_GUI_H
#define SOLVESPACE_GUI_H

#include <set>
#include <string>

namespace SolveSpace {
class RgbaColor;

namespace Platform {

//-----------------------------------------------------------------------------
// Events
//-----------------------------------------------------------------------------

// A mouse input event.
class MouseEvent {
public:
    enum class Type {
        MOTION,
        PRESS,
        DBL_PRESS,
        RELEASE,
        SCROLL_VERT,
        LEAVE,
    };

    enum class Button {
        NONE,
        LEFT,
        MIDDLE,
        RIGHT,
    };

    Type        type;
    double      x;
    double      y;
    bool        shiftDown;
    bool        controlDown;
    union {
        Button      button;       // for Type::{MOTION,PRESS,DBL_PRESS,RELEASE}
        double      scrollDelta;  // for Type::SCROLL_VERT
    };
};

// A 3-DOF input event.
struct SixDofEvent {
    enum class Type {
        MOTION,
        PRESS,
        RELEASE,
    };

    enum class Button {
        FIT,
    };

    Type   type;
    bool   shiftDown;
    bool   controlDown;
    double translationX, translationY, translationZ; // for Type::MOTION
    double rotationX,    rotationY,    rotationZ;    // for Type::MOTION
    Button button;                                   // for Type::{PRESS,RELEASE}
};

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
        return type == other.type && key == other.key &&
            shiftDown == other.shiftDown && controlDown == other.controlDown &&
            ((key == Key::CHARACTER && chr == other.chr) ||
             (key == Key::FUNCTION && num == other.num));
    }
};

// A touch gesture input event.
struct TouchGestureEvent {
    enum class Type {
        ROTATE,
        ZOOM,
        PAN,
        SWIPE,
        PINCH
    };

    Type type;
    double x, y;
    double rotation_angle;        // For rotation gestures, in radians
    double rotation_angle_delta;  // Change in rotation angle
    double zoom_scale;            // For zoom gestures
    double pan_delta_x, pan_delta_y;  // For pan gestures
    double swipe_velocity_x, swipe_velocity_y;  // For swipe gestures
    double pinch_scale;           // For pinch gestures
};

std::string AcceleratorDescription(const KeyboardEvent &accel);

//-----------------------------------------------------------------------------
// Interfaces
//-----------------------------------------------------------------------------

// Handling fatal errors.
[[noreturn]]
void FatalError(const std::string &message);

// A native settings store.
class Settings {
public:
    virtual ~Settings() = default;

    virtual void FreezeInt(const std::string &key, uint32_t value) = 0;
    virtual uint32_t ThawInt(const std::string &key, uint32_t defaultValue = 0) = 0;

    virtual void FreezeFloat(const std::string &key, double value) = 0;
    virtual double ThawFloat(const std::string &key, double defaultValue = 0.0) = 0;

    virtual void FreezeString(const std::string &key, const std::string &value) = 0;
    virtual std::string ThawString(const std::string &key,
                                   const std::string &defaultValue = "") = 0;

    virtual void FreezeBool(const std::string &key, bool value);
    virtual bool ThawBool(const std::string &key, bool defaultValue = false);

    virtual void FreezeColor(const std::string &key, RgbaColor value);
    virtual RgbaColor ThawColor(const std::string &key, RgbaColor defaultValue);
};

typedef std::shared_ptr<Settings> SettingsRef;

SettingsRef GetSettings();

// A native single-shot timer.
class Timer {
public:
    std::function<void()>   onTimeout;

    virtual ~Timer() = default;

    virtual void RunAfter(unsigned milliseconds) = 0;
    virtual void RunAfterNextFrame() { RunAfter(1); }
    virtual void RunAfterProcessingEvents() { RunAfter(0); }
};

typedef std::shared_ptr<Timer> TimerRef;

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

    virtual ~MenuItem() = default;

    virtual void SetAccelerator(KeyboardEvent accel) = 0;
    virtual void SetIndicator(Indicator type) = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual void SetActive(bool active) = 0;
};

typedef std::shared_ptr<MenuItem> MenuItemRef;

// A native menu.
class Menu {
public:
    virtual ~Menu() = default;

    virtual std::shared_ptr<MenuItem> AddItem(
        const std::string &label, std::function<void()> onTrigger = std::function<void()>(),
        bool mnemonics = true) = 0;
    virtual std::shared_ptr<Menu> AddSubMenu(const std::string &label) = 0;
    virtual void AddSeparator() = 0;

    virtual void PopUp() = 0;

    virtual void Clear() = 0;
};

typedef std::shared_ptr<Menu> MenuRef;

// A native menu bar.
class MenuBar {
public:
    virtual ~MenuBar() = default;

    virtual std::shared_ptr<Menu> AddSubMenu(const std::string &label) = 0;

    virtual void Clear() = 0;
};

typedef std::shared_ptr<MenuBar> MenuBarRef;

MenuRef CreateMenu();
MenuBarRef GetOrCreateMainMenu(bool *unique);

// A native top-level window, with an OpenGL context, and an editor overlay.
class Window {
public:
    enum class Kind {
        TOPLEVEL,
        TOOL,
    };

    enum class Cursor {
        POINTER,
        HAND
    };

    std::function<void()>               onClose;
    std::function<void(bool)>           onFullScreen;
    std::function<bool(MouseEvent)>     onMouseEvent;
    std::function<void(SixDofEvent)>    onSixDofEvent;
    std::function<bool(KeyboardEvent)>  onKeyboardEvent;
    std::function<void(std::string)>    onEditingDone;
    std::function<void(double)>         onScrollbarAdjusted;
    std::function<void()>               onContextLost;
    std::function<void()>               onRender;
    std::function<void(const char*)>    onFileDrop;
    std::function<void(TouchGestureEvent)> onTouchGesture;
    std::function<std::string()>        onDragExport;
    std::function<void()>               onDragExportCleanup;
    std::function<void(int)>            onScaleFactorChanged;

    virtual ~Window() = default;

    // Returns physical display DPI.
    virtual double GetPixelDensity() = 0;
    // Returns raster graphics and coordinate scale (already applied on the platform side),
    // i.e. size of logical pixel in physical pixels, or device pixel ratio.
    virtual double GetDevicePixelRatio() = 0;
    // Returns (fractional) font scale, to be applied on top of (integral) device pixel ratio.
    virtual double GetDeviceFontScale() {
        return GetPixelDensity() / GetDevicePixelRatio() / 96.0;
    }

    virtual bool IsVisible() = 0;
    virtual void SetVisible(bool visible) = 0;
    virtual void Focus() = 0;

    virtual bool IsFullScreen() = 0;
    virtual void SetFullScreen(bool fullScreen) = 0;

    virtual void SetTitle(const std::string &title) = 0;
    virtual bool SetTitleForFilename(const Path &filename) { return false; }

    virtual void SetMenuBar(MenuBarRef menuBar) = 0;

    virtual void GetContentSize(double *width, double *height) = 0;
    virtual void SetMinContentSize(double width, double height) = 0;

    virtual void FreezePosition(SettingsRef settings, const std::string &key) = 0;
    virtual void ThawPosition(SettingsRef settings, const std::string &key) = 0;

    virtual void SetCursor(Cursor cursor) = 0;
    virtual void SetTooltip(const std::string &text, double x, double y,
                            double width, double height) = 0;

    virtual bool IsEditorVisible() = 0;
    virtual void ShowEditor(double x, double y, double fontHeight, double minWidth,
                            bool isMonospace, const std::string &text) = 0;
    virtual void HideEditor() = 0;

    virtual void SetScrollbarVisible(bool visible) = 0;
    virtual void ConfigureScrollbar(double min, double max, double pageSize) = 0;
    virtual double GetScrollbarPosition() = 0;
    virtual void SetScrollbarPosition(double pos) = 0;

    virtual void Invalidate() = 0;
};

typedef std::shared_ptr<Window> WindowRef;

WindowRef CreateWindow(Window::Kind kind = Window::Kind::TOPLEVEL,
                       WindowRef parentWindow = NULL);

// 3DConnexion support.
void Open3DConnexion();
void Close3DConnexion();
void Request3DConnexionEventsForWindow(WindowRef window);

// A native dialog that asks for one choice out of several.
class MessageDialog {
public:
    enum class Type {
        INFORMATION,
        QUESTION,
        WARNING,
        ERROR
    };

    enum class Response {
        NONE,
        OK,
        YES,
        NO,
        CANCEL
    };

    std::function<void(Response)> onResponse;

    virtual ~MessageDialog() = default;

    virtual void SetType(Type type) = 0;
    virtual void SetTitle(std::string title) = 0;
    virtual void SetMessage(std::string message) = 0;
    virtual void SetDescription(std::string description) = 0;

    virtual void AddButton(std::string label, Response response, bool isDefault = false) = 0;

    virtual Response RunModal() = 0;
    virtual void ShowModal() {
        Response response = RunModal();
        if(onResponse) {
            onResponse(response);
        }
    }
};

typedef std::shared_ptr<MessageDialog> MessageDialogRef;

MessageDialogRef CreateMessageDialog(WindowRef parentWindow);

// A file filter.
struct FileFilter {
    std::string                 name;
    std::vector<std::string>    extensions;
};

// SolveSpace's native file format
extern std::vector<FileFilter> SolveSpaceModelFileFilters;
// SolveSpace's linkable file formats
extern std::vector<FileFilter> SolveSpaceLinkFileFilters;
// Raster image
extern std::vector<FileFilter> RasterFileFilters;
// Triangle mesh
extern std::vector<FileFilter> MeshFileFilters;
// NURBS surfaces
extern std::vector<FileFilter> SurfaceFileFilters;
// 2d vector (lines and curves) format
extern std::vector<FileFilter> VectorFileFilters;
// 3d vector (wireframe lines and curves) format
extern std::vector<FileFilter> Vector3dFileFilters;
// Any importable format
extern std::vector<FileFilter> ImportFileFilters;
// Comma-separated value, like a spreadsheet would use
extern std::vector<FileFilter> CsvFileFilters;

// A native dialog that asks to choose a file.
class FileDialog {
public:
    virtual ~FileDialog() = default;

    virtual void SetTitle(std::string title) = 0;
    virtual void SetCurrentName(std::string name) = 0;

    virtual Platform::Path GetFilename() = 0;
    virtual void SetFilename(Platform::Path path) = 0;
    virtual void SuggestFilename(Platform::Path path) = 0;

    virtual void AddFilter(std::string name, std::vector<std::string> extensions) = 0;
    void AddFilter(const FileFilter &filter);
    void AddFilters(const std::vector<FileFilter> &filters);

    virtual void FreezeChoices(SettingsRef settings, const std::string &key) = 0;
    virtual void ThawChoices(SettingsRef settings, const std::string &key) = 0;

    virtual bool RunModal() = 0;
};

typedef std::shared_ptr<FileDialog> FileDialogRef;

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow);
FileDialogRef CreateSaveFileDialog(WindowRef parentWindow);

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Platform::Path> GetFontFiles();
void OpenInBrowser(const std::string &url);
void ShowColorPicker(const RgbaColor& initialColor, std::function<void(const RgbaColor&)> onColorSelected);

// Check if current text direction is RTL
inline bool IsRTL() {
    static bool checked = false;
    static bool is_rtl = false;
    
    if (!checked) {
#if defined(USE_GTK4) && defined(HAVE_GTKMM)
        // In GTK builds, check the current locale
        try {
            // Get current locale and check if it's RTL
            std::string locale = Glib::get_language_names()[0];
            
            // Only include languages that use RTL scripts
            std::set<std::string> rtl_langs = {"ar", "he", "fa", "ur", "dv", "ha", "khw", "ks", "ps", "sd", "ug", "yi"};
            
            // For Kurdish, check if it's specifically Sorani Kurdish (ckb) which uses RTL
            // Kurmanji Kurdish (ku_TR) uses Latin script (LTR)
            bool is_sorani_kurdish = locale.length() >= 3 && locale.substr(0, 3) == "ckb";
            
            is_rtl = (locale.length() >= 2 && rtl_langs.find(locale.substr(0, 2)) != rtl_langs.end()) || is_sorani_kurdish;
        } catch (...) {
            // If there's any error, default to LTR
            is_rtl = false;
        }
#else
        // In non-GTK builds, default to LTR
        is_rtl = false;
#endif
        checked = true;
    }
    
    return is_rtl;
}

std::vector<std::string> InitGui(int argc, char **argv);
void RunGui();
void ExitGui();
void ClearGui();

}
} // namespace SolveSpace

#endif
