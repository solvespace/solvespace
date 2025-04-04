//-----------------------------------------------------------------------------
//
// Loosely based on guigtk.cpp by whitequark
// Commonwealth copyright Erkin Alp Güney 2025
// Human involvement below copyrightability threshold outside the commonwealth
//-----------------------------------------------------------------------------

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json-c/json_object.h>
#include <json-c/json_util.h>

#include <glibmm/convert.h>
#include <glibmm/main.h>

#include <gtkmm/adjustment.h>
#include <gtkmm/application.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/entry.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/fixed.h>
#include <gtkmm/glarea.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/popover.h>
#include <gtkmm/scrollbar.h>
#include <gtkmm/separator.h>
#include <gtkmm/tooltip.h>
#include <gtkmm/window.h>

#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gestureclick.h>

#include "config.h"
#if defined(HAVE_GTK_FILECHOOSERNATIVE)
#   include <gtkmm/filechoosernative.h>
#endif

#if defined(HAVE_SPACEWARE)
#   include <spnav.h>
#   include <gdk/gdk.h>
#   if defined(GDK_WINDOWING_WAYLAND)
#       include <gdk/wayland/gdkwayland.h>
#   endif
#   if GTK_CHECK_VERSION(3, 20, 0)
#       include <gdkmm/seat.h>
#   else
#       include <gdkmm/devicemanager.h>
#   endif
#endif

#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Utility functions
//-----------------------------------------------------------------------------

static std::string PrepareMnemonics(std::string label) {
    std::replace(label.begin(), label.end(), '&', '_');
    return label;
}

static std::string PrepareTitle(const std::string &title) {
    return title + " — SolveSpace";
}

//-----------------------------------------------------------------------------
// Fatal errors
//-----------------------------------------------------------------------------

void FatalError(const std::string &message) {
    fprintf(stderr, "%s", message.c_str());
    abort();
}

//-----------------------------------------------------------------------------
// Settings
//-----------------------------------------------------------------------------

class SettingsImplGtk final : public Settings {
public:
    // Why aren't we using GSettings? Two reasons. It doesn't allow to easily see whether
    // the setting had the default value, and it requires to install a schema globally.
    Path         _path;
    json_object *_json = NULL;

    static Path GetConfigPath() {
        Path configHome;
        if(getenv("XDG_CONFIG_HOME")) {
            configHome = Path::From(getenv("XDG_CONFIG_HOME"));
        } else if(getenv("HOME")) {
            configHome = Path::From(getenv("HOME")).Join(".config");
        } else {
            dbp("neither XDG_CONFIG_HOME nor HOME are set");
            return Path::From("");
        }
        if(!configHome.IsEmpty()) {
            configHome = configHome.Join("solvespace");
        }

        const char *configHomeC = configHome.raw.c_str();
        struct stat st;
        if(stat(configHomeC, &st)) {
            if(errno == ENOENT) {
                if(mkdir(configHomeC, 0777)) {
                    dbp("cannot mkdir %s: %s", configHomeC, strerror(errno));
                    return Path::From("");
                }
            } else {
                dbp("cannot stat %s: %s", configHomeC, strerror(errno));
                return Path::From("");
            }
        } else if(!S_ISDIR(st.st_mode)) {
            dbp("%s is not a directory", configHomeC);
            return Path::From("");
        }

        return configHome.Join("settings.json");
    }

    SettingsImplGtk() {
        _path = GetConfigPath();
        if(_path.IsEmpty()) {
            dbp("settings will not be saved");
        } else {
            _json = json_object_from_file(_path.raw.c_str());
            if(!_json && errno != ENOENT) {
                dbp("cannot load settings: %s", strerror(errno));
            }
        }

        if(_json == NULL) {
            _json = json_object_new_object();
        }
    }

    ~SettingsImplGtk() override {
        if(!_path.IsEmpty()) {
            // json-c <0.12 has the first argument non-const
            if(json_object_to_file_ext((char *)_path.raw.c_str(), _json,
                                       JSON_C_TO_STRING_PRETTY)) {
                dbp("cannot save settings: %s", strerror(errno));
            }
        }

        json_object_put(_json);
    }

    void FreezeInt(const std::string &key, uint32_t value) override {
        struct json_object *jsonValue = json_object_new_int(value);
        json_object_object_add(_json, key.c_str(), jsonValue);
    }

    uint32_t ThawInt(const std::string &key, uint32_t defaultValue) override {
        struct json_object *jsonValue;
        if(json_object_object_get_ex(_json, key.c_str(), &jsonValue)) {
            return json_object_get_int(jsonValue);
        }
        return defaultValue;
    }

    void FreezeBool(const std::string &key, bool value) override {
        struct json_object *jsonValue = json_object_new_boolean(value);
        json_object_object_add(_json, key.c_str(), jsonValue);
    }

    bool ThawBool(const std::string &key, bool defaultValue) override {
        struct json_object *jsonValue;
        if(json_object_object_get_ex(_json, key.c_str(), &jsonValue)) {
            return json_object_get_boolean(jsonValue);
        }
        return defaultValue;
    }

    void FreezeFloat(const std::string &key, double value) override {
        struct json_object *jsonValue = json_object_new_double(value);
        json_object_object_add(_json, key.c_str(), jsonValue);
    }

    double ThawFloat(const std::string &key, double defaultValue) override {
        struct json_object *jsonValue;
        if(json_object_object_get_ex(_json, key.c_str(), &jsonValue)) {
            return json_object_get_double(jsonValue);
        }
        return defaultValue;
    }

    void FreezeString(const std::string &key, const std::string &value) override {
        struct json_object *jsonValue = json_object_new_string(value.c_str());
        json_object_object_add(_json, key.c_str(), jsonValue);
    }

    std::string ThawString(const std::string &key,
                           const std::string &defaultValue = "") override {
        struct json_object *jsonValue;
        if(json_object_object_get_ex(_json, key.c_str(), &jsonValue)) {
            return json_object_get_string(jsonValue);
        }
        return defaultValue;
    }
};

SettingsRef GetSettings() {
    static std::shared_ptr<SettingsImplGtk> settings;
    if(!settings) {
        settings = std::make_shared<SettingsImplGtk>();
    }
    return settings;
}

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

class TimerImplGtk final : public Timer {
public:
    sigc::connection    _connection;

    void RunAfter(unsigned milliseconds) override {
        if(!_connection.empty()) {
            _connection.disconnect();
        }

        auto handler = [this]() -> bool {
            if(this->onTimeout) {
                this->onTimeout();
            }
            return false;
        };
        // Note: asan warnings about new-delete-type-mismatch are false positives here:
        // https://gitlab.gnome.org/GNOME/gtkmm/-/issues/65
        // Pass new_delete_type_mismatch=0 to ASAN_OPTIONS to disable those warnings.
        // Unfortunately they won't go away until upgrading to gtkmm4
        _connection = Glib::signal_timeout().connect(handler, milliseconds);
    }
};

TimerRef CreateTimer() {
    return std::make_shared<TimerImplGtk>();
}

//-----------------------------------------------------------------------------
// GTK menu extensions
//-----------------------------------------------------------------------------

class GtkMenuItem : public Gtk::CheckButton {
    Platform::MenuItem *_receiver;
    bool                _has_indicator;
    bool                _synthetic_event;
    sigc::connection    _activate_connection;

public:
    GtkMenuItem(Platform::MenuItem *receiver) :
        _receiver(receiver), _has_indicator(false), _synthetic_event(false) {
        
        _activate_connection = signal_toggled().connect(
            [this]() {
                if(!_synthetic_event && _receiver->onTrigger) {
                    _receiver->onTrigger();
                }
            }, false);
    }

    void set_accel_key(const Gtk::AccelKey &accel_key) {
    }

    bool has_indicator() const {
        return _has_indicator;
    }

    void set_has_indicator(bool has_indicator) {
        _has_indicator = has_indicator;
    }

    void set_active(bool active) {
        if(get_active() == active)
            return;

        _synthetic_event = true;
        Gtk::CheckButton::set_active(active);
        _synthetic_event = false;
    }
};

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

class MenuItemImplGtk final : public MenuItem {
public:
    GtkMenuItem gtkMenuItem;
    std::string actionName;  // Add actionName member for GTK4 compatibility
    std::function<void()> onTrigger;

    MenuItemImplGtk() : gtkMenuItem(this) {}

    void SetAccelerator(KeyboardEvent accel) override {
        guint accelKey = 0;
        if(accel.key == KeyboardEvent::Key::CHARACTER) {
            if(accel.chr == '\t') {
                accelKey = GDK_KEY_Tab;
            } else if(accel.chr == '\x1b') {
                accelKey = GDK_KEY_Escape;
            } else if(accel.chr == '\x7f') {
                accelKey = GDK_KEY_Delete;
            } else {
                accelKey = gdk_unicode_to_keyval(accel.chr);
            }
        } else if(accel.key == KeyboardEvent::Key::FUNCTION) {
            accelKey = GDK_KEY_F1 + accel.num - 1;
        }

        Gdk::ModifierType accelMods = {};
        if(accel.shiftDown) {
            accelMods |= Gdk::ModifierType::SHIFT_MASK;
        }
        if(accel.controlDown) {
            accelMods |= Gdk::ModifierType::CONTROL_MASK;
        }

        gtkMenuItem.set_accel_key(Gtk::AccelKey(accelKey, accelMods));
    }

    void SetIndicator(Indicator type) override {
        switch(type) {
            case Indicator::NONE:
                gtkMenuItem.set_has_indicator(false);
                break;

            case Indicator::CHECK_MARK:
                gtkMenuItem.set_has_indicator(true);
                break;

            case Indicator::RADIO_MARK:
                gtkMenuItem.set_has_indicator(true);
                break;
        }
    }

    void SetActive(bool active) override {
        ssassert(gtkMenuItem.has_indicator(),
                 "Cannot change state of a menu item without indicator");
        gtkMenuItem.set_active(active);
    }

    void SetEnabled(bool enabled) override {
        gtkMenuItem.set_sensitive(enabled);
    }
};

class MenuImplGtk final : public Menu {
public:
    Glib::RefPtr<Gio::Menu> gioMenu;
    Gtk::Popover gtkMenu;
    std::vector<std::shared_ptr<MenuItemImplGtk>> menuItems;
    std::vector<std::shared_ptr<MenuImplGtk>> subMenus;
    
    MenuImplGtk() {
        gioMenu = Gio::Menu::create();
        auto menuBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        gtkMenu.set_child(*menuBox);
    }

    MenuItemRef AddItem(const std::string &label,
                        std::function<void()> onTrigger = NULL,
                        bool mnemonics = true) override {
        auto menuItem = std::make_shared<MenuItemImplGtk>();
        menuItems.push_back(menuItem);

        std::string itemLabel = mnemonics ? PrepareMnemonics(label) : label;
        
        std::string actionName = "app.action" + std::to_string(menuItems.size());
        
        auto gioMenuItem = Gio::MenuItem::create(itemLabel, actionName);
        gioMenu->append_item(gioMenuItem);
        
        menuItem->actionName = actionName;
        menuItem->onTrigger = onTrigger;
        
        return menuItem;
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplGtk>();
        subMenus.push_back(subMenu);

        std::string itemLabel = PrepareMnemonics(label);
        
        gioMenu->append_submenu(itemLabel, subMenu->gioMenu);
        
        return subMenu;
    }

    void AddSeparator() override {
        auto section = Gio::Menu::create();
        gioMenu->append_section("", section);
    }

    void PopUp() override {
        gtkMenu.set_visible(true);
        
        Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
        auto signal = gtkMenu.signal_closed().connect([&]() { 
            loop->quit(); 
        });
        
        loop->run();
        signal.disconnect();
    }

    void Clear() override {
        while (gioMenu->get_n_items() > 0) {
            gioMenu->remove(0);
        }
        
        menuItems.clear();
        subMenus.clear();
    }
};

MenuRef CreateMenu() {
    return std::make_shared<MenuImplGtk>();
}

class MenuBarImplGtk final : public MenuBar {
public:
    Glib::RefPtr<Gio::Menu> gioMenuBar;
    Gtk::Box gtkMenuBar;
    std::vector<std::shared_ptr<MenuImplGtk>> subMenus;
    
    MenuBarImplGtk() : gtkMenuBar(Gtk::Orientation::HORIZONTAL) {
        gioMenuBar = Gio::Menu::create();
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplGtk>();
        subMenus.push_back(subMenu);

        std::string itemLabel = PrepareMnemonics(label);
        
        gioMenuBar->append_submenu(itemLabel, subMenu->gioMenu);
        
        return subMenu;
    }

    void Clear() override {
        while (gioMenuBar->get_n_items() > 0) {
            gioMenuBar->remove(0);
        }
        
        subMenus.clear();
    }
};

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    *unique = false;
    return std::make_shared<MenuBarImplGtk>();
}

//-----------------------------------------------------------------------------
// GTK GL and window extensions
//-----------------------------------------------------------------------------

class GtkGLWidget : public Gtk::GLArea {
    Window *_receiver;

public:
    GtkGLWidget(Platform::Window *receiver) : _receiver(receiver) {
        set_has_depth_buffer(true);
        set_can_focus(true);
        setup_event_controllers();
    }

protected:
    // Work around a bug fixed in GTKMM 3.22:
    // https://mail.gnome.org/archives/gtkmm-list/2016-April/msg00020.html
    Glib::RefPtr<Gdk::GLContext> on_create_context() override {
        return get_native()->get_surface()->create_gl_context();
    }

    bool on_render(const Glib::RefPtr<Gdk::GLContext> &context) override {
        if(_receiver->onRender) {
            _receiver->onRender();
        }
        return true;
    }

    bool process_pointer_event(MouseEvent::Type type, double x, double y,
                               GdkModifierType state, guint button = 0, double scroll_delta = 0) {
        MouseEvent event = {};
        event.type = type;
        event.x = x;
        event.y = y;
        if(button == 1 || (state & GDK_BUTTON1_MASK) != 0) {
            event.button = MouseEvent::Button::LEFT;
        } else if(button == 2 || (state & GDK_BUTTON2_MASK) != 0) {
            event.button = MouseEvent::Button::MIDDLE;
        } else if(button == 3 || (state & GDK_BUTTON3_MASK) != 0) {
            event.button = MouseEvent::Button::RIGHT;
        }
        if((state & GDK_SHIFT_MASK) != 0) {
            event.shiftDown = true;
        }
        if((state & GDK_CONTROL_MASK) != 0) {
            event.controlDown = true;
        }
        if(scroll_delta != 0) {
            event.scrollDelta = scroll_delta;
        }

        if(_receiver->onMouseEvent) {
            return _receiver->onMouseEvent(event);
        }

        return false;
    }

    bool process_key_event(KeyboardEvent::Type type, guint keyval, GdkModifierType state) {
        KeyboardEvent event = {};
        event.type = type;

        if((state & (GDK_MODIFIER_MASK)) & ~(GDK_SHIFT_MASK|GDK_CONTROL_MASK)) {
            return false;
        }

        event.shiftDown   = (state & GDK_SHIFT_MASK)   != 0;
        event.controlDown = (state & GDK_CONTROL_MASK) != 0;

        char32_t chr = gdk_keyval_to_unicode(gdk_keyval_to_lower(keyval));
        if(chr != 0) {
            event.key = KeyboardEvent::Key::CHARACTER;
            event.chr = chr;
        } else if(keyval >= GDK_KEY_F1 &&
                  keyval <= GDK_KEY_F12) {
            event.key = KeyboardEvent::Key::FUNCTION;
            event.num = keyval - GDK_KEY_F1 + 1;
        } else {
            return false;
        }

        if(_receiver->onKeyboardEvent) {
            return _receiver->onKeyboardEvent(event);
        }

        return false;
    }

    void setup_event_controllers() {
        auto motion_controller = Gtk::EventControllerMotion::create();
        motion_controller->signal_motion().connect(
            [this, motion_controller](double x, double y) {
                auto state = motion_controller->get_current_event_state();
                process_pointer_event(MouseEvent::Type::MOTION, x, y, static_cast<GdkModifierType>(state));
                return true;
            });
        motion_controller->signal_leave().connect(
            [this]() {
                double x, y;
                get_pointer_position(x, y);
                process_pointer_event(MouseEvent::Type::LEAVE, x, y, GdkModifierType(0));
                return true;
            });
        add_controller(motion_controller);

        auto gesture_click = Gtk::GestureClick::create();
        gesture_click->set_button(0); // Listen for any button
        gesture_click->signal_pressed().connect(
            [this, gesture_click](int n_press, double x, double y) {
                auto state = gesture_click->get_current_event_state();
                guint button = gesture_click->get_current_button();
                process_pointer_event(
                    n_press > 1 ? MouseEvent::Type::DBL_PRESS : MouseEvent::Type::PRESS, 
                    x, y, static_cast<GdkModifierType>(state), button);
                grab_focus(); // Ensure we get keyboard focus on click
                return true;
            });
        gesture_click->signal_released().connect(
            [this, gesture_click](int n_press, double x, double y) {
                auto state = gesture_click->get_current_event_state();
                guint button = gesture_click->get_current_button();
                process_pointer_event(MouseEvent::Type::RELEASE, x, y, static_cast<GdkModifierType>(state), button);
                return true;
            });
        add_controller(gesture_click);

        auto scroll_controller = Gtk::EventControllerScroll::create();
        scroll_controller->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
        scroll_controller->signal_scroll().connect(
            [this, scroll_controller](double dx, double dy) {
                double x, y;
                get_pointer_position(x, y);
                auto state = scroll_controller->get_current_event_state();
                process_pointer_event(MouseEvent::Type::SCROLL_VERT, x, y, static_cast<GdkModifierType>(state), 0, -dy);
                return true;
            });
        add_controller(scroll_controller);

        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                GdkModifierType gdk_state = static_cast<GdkModifierType>(state);
                return process_key_event(KeyboardEvent::Type::PRESS, keyval, gdk_state);
            });
        key_controller->signal_key_released().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                GdkModifierType gdk_state = static_cast<GdkModifierType>(state);
                return process_key_event(KeyboardEvent::Type::RELEASE, keyval, gdk_state);
            });
        add_controller(key_controller);
        
        auto focus_controller = Gtk::EventControllerFocus::create();
        add_controller(focus_controller);
    }

    void get_pointer_position(double &x, double &y) {
        auto display = get_display();
        auto seat = display->get_default_seat();
        auto device = seat->get_pointer();
        
        auto surface = get_native()->get_surface();
        double root_x, root_y;
        Gdk::ModifierType mask;
        surface->get_device_position(device, root_x, root_y, mask);
        
        x = root_x;
        y = root_y;
    }
};

class GtkEditorOverlay : public Gtk::Fixed {
    Window      *_receiver;
    GtkGLWidget _gl_widget;
    Gtk::Entry  _entry;
    Glib::RefPtr<Gtk::EventControllerKey> _key_controller;

public:
    GtkEditorOverlay(Platform::Window *receiver) : _receiver(receiver), _gl_widget(receiver) {
        put(_gl_widget, 0, 0);

        _entry.set_visible(false);
        _entry.set_has_frame(false);
        put(_entry, 0, 0);  // We'll position it properly later

        _entry.signal_activate().
            connect(sigc::mem_fun(*this, &GtkEditorOverlay::on_activate));
            
        _key_controller = Gtk::EventControllerKey::create();
        _key_controller->signal_key_pressed().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                GdkModifierType gdk_state = static_cast<GdkModifierType>(state);
                return on_key_pressed(keyval, keycode, gdk_state);
            }, false);
        _key_controller->signal_key_released().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                GdkModifierType gdk_state = static_cast<GdkModifierType>(state);
                return on_key_released(keyval, keycode, gdk_state);
            }, false);
        add_controller(_key_controller);
        
        auto size_controller = Gtk::EventControllerMotion::create();
        add_controller(size_controller);
        
        on_size_allocate();
    }

    bool is_editing() const {
        return _entry.get_visible();
    }

    void start_editing(int x, int y, int font_height, int min_width, bool is_monospace,
                       const std::string &val) {
        Pango::FontDescription font_desc;
        font_desc.set_family(is_monospace ? "monospace" : "normal");
        font_desc.set_absolute_size(font_height * Pango::SCALE);
        
        auto css_provider = Gtk::CssProvider::create();
        std::string css_data = "entry { font-family: ";
        css_data += (is_monospace ? "monospace" : "normal");
        css_data += "; font-size: ";
        css_data += std::to_string(font_height);
        css_data += "px; padding: 0; margin: 0; background: transparent; }";
        css_provider->load_from_data(css_data);
        
        _entry.get_style_context()->add_provider(css_provider, 
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            
        _entry.add_css_class("solvespace-editor-entry");

        // The y coordinate denotes baseline.
        Pango::FontMetrics font_metrics = get_pango_context()->get_metrics(font_desc);
        y -= font_metrics.get_ascent() / Pango::SCALE;

        Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create(get_pango_context());
        layout->set_font_description(font_desc);
        // Add one extra char width to avoid scrolling.
        layout->set_text(val + " ");
        int width = layout->get_logical_extents().get_width();

        Gtk::Border margin;
        margin.set_left(0);
        margin.set_right(0);
        margin.set_top(0);
        margin.set_bottom(0);
        
        Gtk::Border border;
        border.set_left(1);
        border.set_right(1);
        border.set_top(1);
        border.set_bottom(1);
        
        Gtk::Border padding;
        padding.set_left(2);
        padding.set_right(2);
        padding.set_top(2);
        padding.set_bottom(2);
        
        put(_entry,
            x - margin.get_left() - border.get_left() - padding.get_left(),
            y - margin.get_top()  - border.get_top()  - padding.get_top());

        int fitWidth = width / Pango::SCALE + padding.get_left() + padding.get_right();
        _entry.set_size_request(max(fitWidth, min_width), -1);
        queue_resize();

        _entry.set_text(val);

        if(!_entry.get_visible()) {
            _entry.set_visible(true);
            _entry.grab_focus();

            _entry.grab_focus();
        }
    }

    void stop_editing() {
        if(_entry.get_visible()) {
            _entry.set_visible(false);
            _gl_widget.grab_focus();
        }
    }

    GtkGLWidget &get_gl_widget() {
        return _gl_widget;
    }

protected:
    bool on_key_pressed(guint keyval, guint keycode, GdkModifierType state) {
        if(is_editing()) {
            if(keyval == GDK_KEY_Escape) {
                stop_editing();
                return true;
            }
            return false; // Let the entry handle it
        }
        return false;
    }

    bool on_key_released(guint keyval, guint keycode, GdkModifierType state) {
        if(is_editing()) {
            return false; // Let the entry handle it
        }
        return false;
    }

    void on_size_allocate() {
        Gtk::Allocation allocation = get_allocation();
        int baseline = -1; // Default baseline value

        _gl_widget.size_allocate(allocation, baseline);

        if(_entry.get_visible()) {
            int entry_width, entry_height, min_height, natural_height;
            _entry.get_size_request(entry_width, entry_height);
            int min_baseline, natural_baseline;
            _entry.measure(Gtk::Orientation::VERTICAL, -1, min_height, natural_height, min_baseline, natural_baseline);
            
            Gtk::Allocation entry_allocation = _entry.get_allocation();
            int x = entry_allocation.get_x();
            int y = entry_allocation.get_y();
            
            _entry.size_allocate(
                Gdk::Rectangle(x, y, entry_width > 0 ? entry_width : 100, natural_height),
                -1);
        }
    }

    void on_activate() {
        if(_receiver->onEditingDone) {
            _receiver->onEditingDone(_entry.get_text());
        }
    }
};

class GtkWindow : public Gtk::Window {
    Platform::Window   *_receiver;
    Gtk::Box            _vbox;
    Gtk::HeaderBar     *menu_bar = NULL;
    Gtk::Box            _hbox;
    GtkEditorOverlay    _editor_overlay;
    Gtk::Scrollbar      _scrollbar;
    bool                _is_under_cursor = false;
    bool                _is_fullscreen = false;
    std::string         _tooltip_text;
    Gdk::Rectangle      _tooltip_area;
    Glib::RefPtr<Gtk::EventControllerMotion> _motion_controller;

public:
    GtkWindow(Platform::Window *receiver) : 
        _receiver(receiver), 
        _vbox(Gtk::Orientation::VERTICAL),
        _hbox(Gtk::Orientation::HORIZONTAL),
        _editor_overlay(receiver),
        _scrollbar() {
        _scrollbar.set_orientation(Gtk::Orientation::VERTICAL);
        
        _hbox.set_hexpand(true);
        _hbox.set_vexpand(true);
        _editor_overlay.set_hexpand(true);
        _editor_overlay.set_vexpand(true);
        
        _hbox.append(_editor_overlay);
        _hbox.append(_scrollbar);
        _vbox.append(_hbox);
        set_child(_vbox);

        _vbox.set_visible(true);
        _hbox.set_visible(true);
        _editor_overlay.set_visible(true);
        get_gl_widget().set_visible(true);

        auto adjustment = Gtk::Adjustment::create(0.0, 0.0, 100.0, 1.0, 10.0, 10.0);
        _scrollbar.set_adjustment(adjustment);
        adjustment->signal_value_changed().
            connect(sigc::mem_fun(*this, &GtkWindow::on_scrollbar_value_changed), false);

        get_gl_widget().set_has_tooltip(true);
        get_gl_widget().signal_query_tooltip().
            connect(sigc::mem_fun(*this, &GtkWindow::on_query_tooltip), false);
            
        setup_event_controllers();
    }

    bool is_full_screen() const {
        return _is_fullscreen;
    }

    Gtk::HeaderBar *get_menu_bar() const {
        return menu_bar;
    }

    void set_menu_bar(Gtk::HeaderBar *menu_bar_ptr) {
        if(menu_bar) {
            _vbox.remove(*menu_bar);
        }
        menu_bar = menu_bar_ptr;
        if(menu_bar) {
            menu_bar->set_visible(true);
            _vbox.prepend(*menu_bar);
        }
    }

    GtkEditorOverlay &get_editor_overlay() {
        return _editor_overlay;
    }

    GtkGLWidget &get_gl_widget() {
        return _editor_overlay.get_gl_widget();
    }

    Gtk::Scrollbar &get_scrollbar() {
        return _scrollbar;
    }

    void set_tooltip(const std::string &text, const Gdk::Rectangle &rect) {
        if(_tooltip_text != text) {
            _tooltip_text = text;
            _tooltip_area = rect;
            get_gl_widget().trigger_tooltip_query();
        }
    }

protected:
    void setup_event_controllers() {
        _motion_controller = Gtk::EventControllerMotion::create();
        _motion_controller->signal_enter().connect(
            [this](double x, double y) -> void {
                _is_under_cursor = true;
            });
        _motion_controller->signal_leave().connect(
            [this]() -> void {
                _is_under_cursor = false;
            });
        add_controller(_motion_controller);
        
        signal_close_request().connect(
            [this]() -> bool {
                if(_receiver->onClose) {
                    _receiver->onClose();
                    return true; // Prevent default close behavior
                }
                return false;
            }, false);
    }

    bool on_query_tooltip(int x, int y, bool keyboard_tooltip,
                          const Glib::RefPtr<Gtk::Tooltip> &tooltip) {
        tooltip->set_text(_tooltip_text);
        tooltip->set_tip_area(_tooltip_area);
        return !_tooltip_text.empty() && (keyboard_tooltip || _is_under_cursor);
    }

    void on_fullscreen_changed() {
        _is_fullscreen = is_fullscreen();
        if(_receiver->onFullScreen) {
            _receiver->onFullScreen(_is_fullscreen);
        }
    }

    void on_scrollbar_value_changed() {
        if(_receiver->onScrollbarAdjusted) {
            _receiver->onScrollbarAdjusted(_scrollbar.get_adjustment()->get_value());
        }
    }
};

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

class WindowImplGtk final : public Window {
public:
    GtkWindow       gtkWindow;
    MenuBarRef      menuBar;

    Glib::Property<bool> visible_property;
    
    WindowImplGtk(Window::Kind kind) : 
        gtkWindow(this),
        visible_property(*this, "visible", false)
    {
        switch(kind) {
            case Kind::TOPLEVEL:
                break;

            case Kind::TOOL:
                gtkWindow.set_modal(true);
                gtkWindow.set_deletable(false);
                break;
        }

        auto icon = LoadPng("freedesktop/solvespace-48x48.png");
        gtkWindow.set_icon_name("solvespace");
        
        visible_property.signal_changed().connect([this]() {
            if (visible_property.get_value()) {
                gtkWindow.show();
            } else {
                gtkWindow.hide();
            }
        });
    }

    double GetPixelDensity() override {
        return gtkWindow.get_scale_factor();
    }

    double GetDevicePixelRatio() override {
        return gtkWindow.get_scale_factor();
    }

    bool IsVisible() override {
        return gtkWindow.is_visible();
    }

    void SetVisible(bool visible) override {
        visible_property.set_value(visible);
    }

    void Focus() override {
        gtkWindow.present();
    }

    bool IsFullScreen() override {
        return gtkWindow.is_full_screen();
    }

    void SetFullScreen(bool fullScreen) override {
        if(fullScreen) {
            gtkWindow.fullscreen();
        } else {
            gtkWindow.unfullscreen();
        }
    }

    void SetTitle(const std::string &title) override {
        gtkWindow.set_title(PrepareTitle(title));
    }

    void SetMenuBar(MenuBarRef newMenuBar) override {
        if(newMenuBar) {
            auto headerBar = Gtk::make_managed<Gtk::HeaderBar>();
            headerBar->set_show_title_buttons(true);
            
            auto menuBarImpl = std::static_pointer_cast<MenuBarImplGtk>(newMenuBar);
            
            int menuIndex = 0;
            for (const auto& subMenu : menuBarImpl->subMenus) {
                auto menuButton = Gtk::make_managed<Gtk::MenuButton>();
                
                menuButton->set_label("Menu");
                
                if (subMenu->gioMenu->get_n_items() > 0) {
                    menuButton->set_label("Menu " + std::to_string(menuIndex+1));
                }
                
                menuIndex++;
                
                auto popover = Gtk::make_managed<Gtk::Popover>();
                menuButton->set_popover(*popover);
                
                auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
                box->set_margin_start(4);
                box->set_margin_end(4);
                box->set_margin_top(4);
                box->set_margin_bottom(4);
                box->set_spacing(2);
                
                for (int i = 0; i < subMenu->gioMenu->get_n_items(); i++) {
                    Glib::ustring itemLabel = "Item " + std::to_string(i+1);
                    
                    auto item = Gtk::make_managed<Gtk::Button>();
                    item->set_label(itemLabel);
                    item->set_has_frame(false);
                    item->add_css_class("flat");
                    item->set_halign(Gtk::Align::FILL);
                    
                    if (i < static_cast<int>(subMenu->menuItems.size()) && subMenu->menuItems[i]->onTrigger) {
                        item->signal_clicked().connect([popover, onTrigger = subMenu->menuItems[i]->onTrigger]() {
                            popover->popdown();
                            onTrigger();
                        });
                    }
                    
                    box->append(*item);
                }
                
                popover->set_child(*box);
                
                
                headerBar->pack_start(*menuButton);
            }
            
            gtkWindow.set_titlebar(*headerBar);
        } else {
            auto headerBar = Gtk::make_managed<Gtk::HeaderBar>();
            headerBar->set_show_title_buttons(true);
            gtkWindow.set_titlebar(*headerBar);
        }
        menuBar = newMenuBar;
    }

    void GetContentSize(double *width, double *height) override {
        *width  = gtkWindow.get_gl_widget().get_allocated_width();
        *height = gtkWindow.get_gl_widget().get_allocated_height();
    }

    void SetMinContentSize(double width, double height) override {
        gtkWindow.get_gl_widget().set_size_request((int)width, (int)height);
    }

    void FreezePosition(SettingsRef settings, const std::string &key) override {
        if(!gtkWindow.is_visible()) return;

        int left = 0, top = 0;
        Gtk::Allocation allocation = gtkWindow.get_allocation();
        left = allocation.get_x();
        top = allocation.get_y();
        
        int width = gtkWindow.get_width();
        int height = gtkWindow.get_height();
        bool isMaximized = gtkWindow.is_maximized();

        settings->FreezeInt(key + "_Left",       left);
        settings->FreezeInt(key + "_Top",        top);
        settings->FreezeInt(key + "_Width",      width);
        settings->FreezeInt(key + "_Height",     height);
        settings->FreezeBool(key + "_Maximized", isMaximized);
    }

    void ThawPosition(SettingsRef settings, const std::string &key) override {
        int left = 0, top = 0;
        int width = gtkWindow.get_width();
        int height = gtkWindow.get_height();

        left   = settings->ThawInt(key + "_Left",   left);
        top    = settings->ThawInt(key + "_Top",    top);
        width  = settings->ThawInt(key + "_Width",  width);
        height = settings->ThawInt(key + "_Height", height);

        gtkWindow.set_default_size(width, height);
        

        if(settings->ThawBool(key + "_Maximized", false)) {
            gtkWindow.maximize();
        }
    }

    void SetCursor(Cursor cursorType) override {
        std::string cursor_name;
        switch(cursorType) {
            case Cursor::POINTER: cursor_name = "default"; break;
            case Cursor::HAND:    cursor_name = "pointer"; break;
            default: ssassert(false, "Unexpected cursor");
        }

        auto display = gtkWindow.get_display();
        auto gdk_cursor = Gdk::Cursor::create(cursor_name);
        gtkWindow.get_gl_widget().set_cursor(gdk_cursor);
    }

    void SetTooltip(const std::string &text, double x, double y,
                    double width, double height) override {
        gtkWindow.set_tooltip(text, { (int)x, (int)y, (int)width, (int)height });
    }

    bool IsEditorVisible() override {
        return gtkWindow.get_editor_overlay().is_editing();
    }

    void ShowEditor(double x, double y, double fontHeight, double minWidth,
                    bool isMonospace, const std::string &text) override {
        gtkWindow.get_editor_overlay().start_editing(
            (int)x, (int)y, (int)fontHeight, (int)minWidth, isMonospace, text);
    }

    void HideEditor() override {
        gtkWindow.get_editor_overlay().stop_editing();
    }

    void SetScrollbarVisible(bool visible) override {
        if(visible) {
            gtkWindow.get_scrollbar().show();
        } else {
            gtkWindow.get_scrollbar().hide();
        }
    }

    void ConfigureScrollbar(double min, double max, double pageSize) override {
        auto adjustment = Gtk::Adjustment::create(
            gtkWindow.get_scrollbar().get_adjustment()->get_value(), // value
            min,                    // lower
            max,                    // upper
            1,                      // step_increment
            4,                      // page_increment
            pageSize                // page_size
        );
        gtkWindow.get_scrollbar().set_adjustment(adjustment);
    }

    double GetScrollbarPosition() override {
        return gtkWindow.get_scrollbar().get_adjustment()->get_value();
    }

    void SetScrollbarPosition(double pos) override {
        gtkWindow.get_scrollbar().get_adjustment()->set_value(pos);
    }

    void Invalidate() override {
        gtkWindow.get_gl_widget().queue_render();
    }
};

WindowRef CreateWindow(Window::Kind kind, WindowRef parentWindow) {
    auto window = std::make_shared<WindowImplGtk>(kind);
    if(parentWindow) {
        window->gtkWindow.set_transient_for(
            std::static_pointer_cast<WindowImplGtk>(parentWindow)->gtkWindow);
    }
    return window;
}

//-----------------------------------------------------------------------------
// 3DConnexion support
//-----------------------------------------------------------------------------

void Open3DConnexion() {}
void Close3DConnexion() {}

#if defined(HAVE_SPACEWARE) && (defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND))
static void ProcessSpnavEvent(WindowImplGtk *window, const spnav_event &spnavEvent, bool shiftDown, bool controlDown) {
    switch(spnavEvent.type) {
        case SPNAV_EVENT_MOTION: {
            SixDofEvent event = {};
            event.type = SixDofEvent::Type::MOTION;
            event.translationX = (double)spnavEvent.motion.x;
            event.translationY = (double)spnavEvent.motion.y;
            event.translationZ = (double)spnavEvent.motion.z  * -1.0;
            event.rotationX    = (double)spnavEvent.motion.rx *  0.001;
            event.rotationY    = (double)spnavEvent.motion.ry *  0.001;
            event.rotationZ    = (double)spnavEvent.motion.rz * -0.001;
            event.shiftDown    = shiftDown;
            event.controlDown  = controlDown;
            if(window->onSixDofEvent) {
                window->onSixDofEvent(event);
            }
            break;
        }

        case SPNAV_EVENT_BUTTON:
            SixDofEvent event = {};
            if(spnavEvent.button.press) {
                event.type = SixDofEvent::Type::PRESS;
            } else {
                event.type = SixDofEvent::Type::RELEASE;
            }
            switch(spnavEvent.button.bnum) {
                case 0:  event.button = SixDofEvent::Button::FIT; break;
                default: return;
            }
            event.shiftDown   = shiftDown;
            event.controlDown = controlDown;
            if(window->onSixDofEvent) {
                window->onSixDofEvent(event);
            }
            break;
    }
}

[[maybe_unused]]
static bool HandleSpnavXEvent(XEvent *xEvent, gpointer data) {
    WindowImplGtk *window = (WindowImplGtk *)data;
    bool shiftDown   = (xEvent->xmotion.state & ShiftMask)   != 0;
    bool controlDown = (xEvent->xmotion.state & ControlMask) != 0;

    spnav_event spnavEvent;
    if(spnav_x11_event(xEvent, &spnavEvent)) {
        ProcessSpnavEvent(window, spnavEvent, shiftDown, controlDown);
        return true; // Event handled
    }
    return false; // Event not handled
}

static gboolean ConsumeSpnavQueue(GIOChannel *, GIOCondition, gpointer data) {
    WindowImplGtk *window = (WindowImplGtk *)data;
    
    auto display = window->gtkWindow.get_display();
    
    // We don't get modifier state through the socket.
    Gdk::ModifierType mask{};
    
    auto seat = display->get_default_seat();
    auto device = seat->get_pointer();
    
    auto keyboard = seat->get_keyboard();
    if (keyboard) {
        mask = keyboard->get_modifier_state();
    }
    
    bool shiftDown   = ((static_cast<int>(mask) & static_cast<int>(Gdk::ModifierType::SHIFT_MASK)) != 0);
    bool controlDown = ((static_cast<int>(mask) & static_cast<int>(Gdk::ModifierType::CONTROL_MASK)) != 0);

    spnav_event spnavEvent;
    while(spnav_poll_event(&spnavEvent)) {
        ProcessSpnavEvent(window, spnavEvent, shiftDown, controlDown);
    }
    return TRUE;
}

void Request3DConnexionEventsForWindow(WindowRef window) {
    std::shared_ptr<WindowImplGtk> windowImpl =
        std::static_pointer_cast<WindowImplGtk>(window);

    if(spnav_open() != -1) {
        g_io_add_watch(g_io_channel_unix_new(spnav_fd()), G_IO_IN,
                       ConsumeSpnavQueue, windowImpl.get());
    }
}
#endif // HAVE_SPACEWARE && (GDK_WINDOWING_X11 || GDK_WINDOWING_WAYLAND)

//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------

class MessageDialogImplGtk;

static std::vector<std::shared_ptr<MessageDialogImplGtk>> shownMessageDialogs;

class MessageDialogImplGtk final : public MessageDialog,
                                   public std::enable_shared_from_this<MessageDialogImplGtk> {
public:
    Gtk::Image         gtkImage;
    Gtk::MessageDialog gtkDialog;

    MessageDialogImplGtk(Gtk::Window &parent)
        : gtkDialog(parent, "", /*use_markup=*/false, Gtk::MessageType::INFO,
                    Gtk::ButtonsType::NONE, /*modal=*/true)
    {
        SetTitle("Message");
    }

    void SetType(Type type) override {
        const char* icon_name = "dialog-information";
        
        switch(type) {
            case Type::INFORMATION:
                icon_name = "dialog-information";
                gtkDialog.set_message_type(Gtk::MessageType::INFO);
                break;

            case Type::QUESTION:
                icon_name = "dialog-question";
                gtkDialog.set_message_type(Gtk::MessageType::QUESTION);
                break;

            case Type::WARNING:
                icon_name = "dialog-warning";
                gtkDialog.set_message_type(Gtk::MessageType::WARNING);
                break;

            case Type::ERROR:
                icon_name = "dialog-error";
                gtkDialog.set_message_type(Gtk::MessageType::ERROR);
                break;
        }
        
        gtkImage.set_from_icon_name(icon_name);
        gtkImage.set_icon_size(Gtk::IconSize::LARGE);
        gtkImage.add_css_class("dialog-icon");
        
        auto content_area = gtkDialog.get_content_area();
        content_area->set_margin_start(12);
        content_area->set_margin_end(12);
        content_area->set_margin_top(12);
        content_area->set_margin_bottom(12);
        content_area->set_spacing(12);
        
        content_area->append(gtkImage);
        gtkImage.set_visible(true);
    }

    void SetTitle(std::string title) override {
        gtkDialog.set_title(PrepareTitle(title));
    }

    void SetMessage(std::string message) override {
        gtkDialog.set_message(message);
    }

    void SetDescription(std::string description) override {
        gtkDialog.set_secondary_text(description);
    }

    void AddButton(std::string label, Response response, bool isDefault) override {
        int responseId = 0;
        switch(response) {
            case Response::NONE:   ssassert(false, "Unexpected response");
            case Response::OK:     responseId = Gtk::ResponseType::OK;     break;
            case Response::YES:    responseId = Gtk::ResponseType::YES;    break;
            case Response::NO:     responseId = Gtk::ResponseType::NO;     break;
            case Response::CANCEL: responseId = Gtk::ResponseType::CANCEL; break;
        }
        gtkDialog.add_button(PrepareMnemonics(label), responseId);
        if(isDefault) {
            gtkDialog.set_default_response(responseId);
        }
    }

    Response ProcessResponse(int gtkResponse) {
        Response response;
        switch(gtkResponse) {
            case Gtk::ResponseType::OK:     response = Response::OK;     break;
            case Gtk::ResponseType::YES:    response = Response::YES;    break;
            case Gtk::ResponseType::NO:     response = Response::NO;     break;
            case Gtk::ResponseType::CANCEL: response = Response::CANCEL; break;

            case Gtk::ResponseType::NONE:
            case Gtk::ResponseType::CLOSE:
            case Gtk::ResponseType::DELETE_EVENT:
                response = Response::NONE;
                break;

            default: ssassert(false, "Unexpected response");
        }

        if(onResponse) {
            onResponse(response);
        }
        return response;
    }

    void ShowModal() override {
        gtkDialog.signal_hide().connect([this]() -> void {
            auto it = std::remove(shownMessageDialogs.begin(), shownMessageDialogs.end(),
                                  shared_from_this());
            shownMessageDialogs.erase(it);
        });
        shownMessageDialogs.push_back(shared_from_this());

        gtkDialog.signal_response().connect([this](int gtkResponse) -> void {
            ProcessResponse(gtkResponse);
            gtkDialog.hide();
        });
        gtkDialog.show();
    }

    Response RunModal() override {
        gtkDialog.show();
        int response = Gtk::ResponseType::NONE;
        
        auto conn = gtkDialog.signal_response().connect(
            [&response](int r) {
                response = r;
            });
            
        auto loop = Glib::MainLoop::create();
        gtkDialog.signal_close_request().connect(
            [&loop]() -> bool {
                loop->quit();
                return true;
            }, false);
            
        loop->run();
        
        conn.disconnect();
        
        return ProcessResponse(response);
    }
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::make_shared<MessageDialogImplGtk>(
                std::static_pointer_cast<WindowImplGtk>(parentWindow)->gtkWindow);
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

class FileDialogImplGtk : public FileDialog {
public:
    Gtk::FileChooser           *gtkChooser;
    std::vector<std::string>    extensions;
    std::vector<Glib::RefPtr<Gtk::FileFilter>> filterObjects;

    void InitFileChooser(Gtk::FileChooser &chooser) {
        gtkChooser = &chooser;
        if (auto dialog = dynamic_cast<Gtk::FileChooserDialog*>(gtkChooser)) {
            dialog->signal_response().connect(
                [this](int response) { 
                    if (response == Gtk::ResponseType::OK) {
                        this->FilterChanged(); 
                    }
                }, false);
        }
    }

    void SetCurrentName(std::string name) override {
        gtkChooser->set_current_name(name);
    }

    Platform::Path GetFilename() override {
        return Path::From(gtkChooser->get_file()->get_path());
    }

    void SetFilename(Platform::Path path) override {
        gtkChooser->set_file(Gio::File::create_for_path(path.raw));
    }

    void SuggestFilename(Platform::Path path) override {
        gtkChooser->set_current_name(path.FileStem()+"."+GetExtension());
    }

    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        Glib::RefPtr<Gtk::FileFilter> gtkFilter = Gtk::FileFilter::create();
        Glib::ustring desc;
        for(auto extension : extensions) {
            Glib::ustring pattern = "*";
            if(!extension.empty()) {
                pattern = "*." + extension;
                gtkFilter->add_pattern(pattern);
                gtkFilter->add_pattern(Glib::ustring(pattern).uppercase());
            }
            if(!desc.empty()) {
                desc += ", ";
            }
            desc += pattern;
        }
        gtkFilter->set_name(name + " (" + desc + ")");

        this->extensions.push_back(extensions.front());
        this->filterObjects.push_back(gtkFilter);
        gtkChooser->add_filter(gtkFilter);
    }

    std::string GetExtension() {
        auto currentFilter = gtkChooser->get_filter();
        for (size_t i = 0; i < extensions.size() && i < filterObjects.size(); i++) {
            if (filterObjects[i] == currentFilter) {
                return extensions[i];
            }
        }
        return extensions.empty() ? "" : extensions.front();
    }

    void SetExtension(std::string extension) {
        size_t extensionIndex =
            std::find(extensions.begin(), extensions.end(), extension) -
            extensions.begin();
        if(extensionIndex < extensions.size() && extensionIndex < filterObjects.size()) {
            gtkChooser->set_filter(filterObjects[extensionIndex]);
        } else if (!filterObjects.empty()) {
            gtkChooser->set_filter(filterObjects.front());
        }
    }

    void FilterChanged() {
        std::string extension = GetExtension();
        if(extension.empty())
            return;

        if(gtkChooser->get_file()) {
            Platform::Path path = GetFilename();
            if(gtkChooser->get_action() != Gtk::FileChooser::Action::OPEN) {
                SetCurrentName(path.WithExtension(extension).FileName());
            }
        }
    }

    void FreezeChoices(SettingsRef settings, const std::string &key) override {
        auto folder = gtkChooser->get_current_folder();
        if(folder) {
            settings->FreezeString("Dialog_" + key + "_Folder", folder->get_path());
        }
        settings->FreezeString("Dialog_" + key + "_Filter", GetExtension());
    }

    void ThawChoices(SettingsRef settings, const std::string &key) override {
        std::string folder_path = settings->ThawString("Dialog_" + key + "_Folder");
        if(!folder_path.empty()) {
            gtkChooser->set_current_folder(Gio::File::create_for_path(folder_path));
        }
        SetExtension(settings->ThawString("Dialog_" + key + "_Filter"));
    }

    void CheckForUntitledFile() {
        if(gtkChooser->get_action() == Gtk::FileChooser::Action::SAVE &&
                Path::From(gtkChooser->get_current_name()).FileStem().empty()) {
            gtkChooser->set_current_name(std::string(_("untitled")) + "." + GetExtension());
        }
    }
};

class FileDialogGtkImplGtk final : public FileDialogImplGtk {
public:
    Gtk::FileChooserDialog      gtkDialog;

    FileDialogGtkImplGtk(Gtk::Window &gtkParent, bool isSave)
        : gtkDialog(isSave ? C_("title", "Save File")
                           : C_("title", "Open File"),
                    isSave ? Gtk::FileChooser::Action::SAVE
                           : Gtk::FileChooser::Action::OPEN) {
        gtkDialog.set_transient_for(gtkParent);
        gtkDialog.set_modal(true);
        gtkDialog.add_button(C_("button", "_Cancel"), Gtk::ResponseType::CANCEL);
        gtkDialog.add_button(isSave ? C_("button", "_Save")
                                    : C_("button", "_Open"), Gtk::ResponseType::OK);
        gtkDialog.set_default_response(Gtk::ResponseType::OK);
        if(isSave) {
            gtkDialog.set_current_name("untitled");
        }
        InitFileChooser(gtkDialog);
    }

    void SetTitle(std::string title) override {
        gtkDialog.set_title(PrepareTitle(title));
    }

    bool RunModal() override {
        CheckForUntitledFile();
        
        bool result = false;
        gtkDialog.signal_response().connect([this, &result](int response) {
            if (response == Gtk::ResponseType::OK) {
                result = true;
            }
            gtkDialog.hide();
        });
        
        gtkDialog.show();
        
        auto context = gtkDialog.get_display()->get_app_launch_context();
        while (gtkDialog.is_visible()) {
            g_main_context_iteration(nullptr, TRUE);
        }
        
        return result;
    }
};

#if defined(HAVE_GTK_FILECHOOSERNATIVE)

class FileDialogNativeImplGtk final : public FileDialogImplGtk {
public:
    Glib::RefPtr<Gtk::FileChooserNative> gtkNative;

    FileDialogNativeImplGtk(Gtk::Window &gtkParent, bool isSave) {
        gtkNative = Gtk::FileChooserNative::create(
            isSave ? C_("title", "Save File")
                   : C_("title", "Open File"),
            gtkParent,
            isSave ? Gtk::FileChooser::Action::SAVE
                   : Gtk::FileChooser::Action::OPEN,
            isSave ? C_("button", "_Save")
                   : C_("button", "_Open"),
            C_("button", "_Cancel"));
        if(isSave) {
            gtkNative->set_current_name("untitled");
        }
        InitFileChooser(*gtkNative);
    }

    void SetTitle(std::string title) override {
        gtkNative->set_title(PrepareTitle(title));
    }

    bool RunModal() override {
        CheckForUntitledFile();
        
        gtkNative->set_modal(true);
        
        auto loop = Glib::MainLoop::create();
        auto response_id = Gtk::ResponseType::CANCEL;
        
        auto response_handler = gtkNative->signal_response().connect(
            [&](int response) {
                response_id = static_cast<Gtk::ResponseType>(response);
                loop->quit();
            });
        
        gtkNative->show();
        loop->run();
        
        response_handler.disconnect();
        
        return response_id == Gtk::ResponseType::ACCEPT;
    }
};

#endif

#if defined(HAVE_GTK_FILECHOOSERNATIVE)
#   define FILE_DIALOG_IMPL FileDialogNativeImplGtk
#else
#   define FILE_DIALOG_IMPL FileDialogGtkImplGtk
#endif

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    Gtk::Window &gtkParent = std::static_pointer_cast<WindowImplGtk>(parentWindow)->gtkWindow;
    return std::make_shared<FILE_DIALOG_IMPL>(gtkParent, /*isSave=*/false);
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    Gtk::Window &gtkParent = std::static_pointer_cast<WindowImplGtk>(parentWindow)->gtkWindow;
    return std::make_shared<FILE_DIALOG_IMPL>(gtkParent, /*isSave=*/true);
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Platform::Path> GetFontFiles() {
    std::vector<Platform::Path> fonts;

    // fontconfig is already initialized by GTK
    FcPattern   *pat = FcPatternCreate();
    FcObjectSet *os  = FcObjectSetBuild(FC_FILE, (char *)0);
    FcFontSet   *fs  = FcFontList(0, pat, os);

    for(int i = 0; i < fs->nfont; i++) {
        FcChar8 *filenameFC = FcPatternFormat(fs->fonts[i], (const FcChar8*) "%{file}");
        fonts.push_back(Platform::Path::From((const char *)filenameFC));
        FcStrFree(filenameFC);
    }

    FcFontSetDestroy(fs);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);

    return fonts;
}

void OpenInBrowser(const std::string &url) {
    Gio::AppInfo::launch_default_for_uri(url);
}

static Glib::RefPtr<Gtk::Application> gtkApp;

std::vector<std::string> InitGui(int argc, char **argv) {
    // It would in principle be possible to judiciously use Glib::filename_{from,to}_utf8,
    // but it's not really worth the effort.
    // The setlocale() call is necessary for Glib::get_charset() to detect the system
    // character set; otherwise it thinks it is always ANSI_X3.4-1968.
    // We set it back to C after all so that printf() and friends behave in a consistent way.
    setlocale(LC_ALL, "");
    if(!Glib::get_charset()) {
        dbp("Sorry, only UTF-8 locales are supported.");
        exit(1);
    }
    setlocale(LC_ALL, "C");

    gtkApp = Gtk::Application::create("org.solvespace.SolveSpace");
    
    gtkApp->set_resource_base_path("/org/solvespace/SolveSpace");
    
    Glib::set_application_name("SolveSpace");
    Glib::set_prgname("solvespace");
    
    std::vector<std::string> args;
    gtkApp->signal_command_line().connect(
        [&args, argc, argv](const Glib::RefPtr<Gio::ApplicationCommandLine>& command_line) -> int {
            int app_argc;
            char **app_argv = command_line->get_arguments(app_argc);
            
            args = InitCli(app_argc, app_argv);
            
            gtkApp->activate();
            return 0;
        });

    Glib::RefPtr<Gtk::CssProvider> style_provider = Gtk::CssProvider::create();
    style_provider->load_from_data(R"(
    entry {
        background: white;
        color: black;
        border-radius: 4px;
        padding: 2px;
    }
    
    .solvespace-editor-entry {
        background: transparent;
        padding: 0;
        margin: 0;
    }
    
    button.flat {
        padding: 6px;
        margin: 1px;
    }
    )");
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        style_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Set locale from user preferences.
    // This apparently only consults the LANGUAGE environment variable.
    const char* const* langNames = g_get_language_names();
    while(*langNames) {
        if(SetLocale(*langNames++)) break;
    }
    if(!*langNames) {
        SetLocale("en_US");
    }

    return args;
}

void RunGui() {
    const char* display = getenv("DISPLAY");
    if (display && (strncmp(display, ":", 1) == 0)) {
        setenv("GTK_A11Y", "none", 0);
    } else {
        unsetenv("GTK_A11Y");
    }
    
    if (!gtkApp->is_registered()) {
        gtkApp->register_application();
        gtkApp->hold(); // Prevent application from exiting when last window closes
        gtkApp->run();
    }
}

void ExitGui() {
    if(gtkApp) {
        gtkApp->quit();
    }
}

void ClearGui() {
    gtkApp.reset();
}

}
}
