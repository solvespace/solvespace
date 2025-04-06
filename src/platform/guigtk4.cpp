//-----------------------------------------------------------------------------
//
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
#include <gtkmm/constraintlayout.h>
#include <gtkmm/constraint.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/entry.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/fixed.h>
#include <gtkmm/glarea.h>
#include <gtkmm/grid.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/overlay.h>
#include <gtkmm/popover.h>
#include <gtkmm/scrollbar.h>
#include <gtkmm/separator.h>
#include <gtkmm/settings.h>
#include <gtkmm/stylecontext.h>
#include <gtkmm/tooltip.h>
#include <gtkmm/window.h>

#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/label.h>
#include <gtkmm/shortcutcontroller.h>
#include <gtkmm/shortcut.h>
#include <gtkmm/shortcuttrigger.h>
#include <gtkmm/shortcutaction.h>
#include <gtkmm/accessible.h>
#include <gtkmm/expression.h> // PropertyExpression is included in expression.h in GTKmm 4.10.0
#include <gtkmm/signallistitemfactory.h>
#include <gtkmm/constraintguide.h>

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
        _connection = Glib::MainContext::get_default()->signal_timeout().connect(handler, milliseconds);
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
    Glib::RefPtr<Gtk::GestureClick> _click_controller;

public:
    GtkMenuItem(Platform::MenuItem *receiver) :
        _receiver(receiver), _has_indicator(false), _synthetic_event(false) {

        _click_controller = Gtk::GestureClick::create();
        _click_controller->set_button(GDK_BUTTON_PRIMARY);
        _click_controller->signal_released().connect(
            [this](int n_press, double x, double y) {
                if(!_synthetic_event && _receiver->onTrigger) {
                    _receiver->onTrigger();
                }
                return true;
            });
        add_controller(_click_controller);
        
        set_property("accessible-role", Gtk::Accessible::Role::MENU_ITEM);
        set_property("accessible-name", _receiver->name);
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
    std::string shortcutText; // Store shortcut text for GTK4 compatibility
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

        std::string modText;
        if(accel.controlDown) modText += "Ctrl+";
        if(accel.shiftDown) modText += "Shift+";

        std::string keyText;
        if(accel.key == KeyboardEvent::Key::CHARACTER) {
            if(accel.chr == '\t') {
                keyText = "Tab";
            } else if(accel.chr == '\x1b') {
                keyText = "Esc";
            } else if(accel.chr == '\x7f') {
                keyText = "Del";
            } else if(accel.chr >= ' ' && accel.chr <= '~') {
                keyText = std::string(1, toupper(accel.chr));
            }
        } else if(accel.key == KeyboardEvent::Key::FUNCTION) {
            keyText = "F" + std::to_string(accel.num);
        }

        shortcutText = modText + keyText;
    }

    void SetIndicator(Indicator type) override {
        switch(type) {
            case Indicator::NONE:
                gtkMenuItem.set_has_indicator(false);
                gtkMenuItem.remove_css_class("check-menu-item");
                gtkMenuItem.remove_css_class("radio-menu-item");
                break;

            case Indicator::CHECK_MARK:
                gtkMenuItem.set_has_indicator(true);
                gtkMenuItem.add_css_class("check-menu-item");
                gtkMenuItem.remove_css_class("radio-menu-item");
                break;

            case Indicator::RADIO_MARK:
                gtkMenuItem.set_has_indicator(true);
                gtkMenuItem.remove_css_class("check-menu-item");
                gtkMenuItem.add_css_class("radio-menu-item");
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
        Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();

        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(
            [this, &loop](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                if (keyval == GDK_KEY_Escape) {
                    gtkMenu.set_visible(false);
                    loop->quit();
                    return true;
                }
                return false;
            }, false);
        gtkMenu.add_controller(key_controller);

        auto motion_controller = Gtk::EventControllerMotion::create();
        motion_controller->signal_leave().connect(
            [this, &loop]() {
                loop->quit();
            });
        gtkMenu.add_controller(motion_controller);

        auto visibility_binding = Gtk::PropertyExpression<bool>::create(
            Gtk::Popover::get_type(), &gtkMenu, "visible");
        visibility_binding->connect([&loop, this](bool visible) {
            if (!visible) {
                loop->quit();
            }
        });

        gtkMenu.set_visible(true);

        loop->run();

        gtkMenu.set_visible(false);
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
    std::vector<std::shared_ptr<MenuImplGtk>> subMenus;
    std::vector<Gtk::MenuButton*> menuButtons;

    MenuBarImplGtk() {
        gioMenuBar = Gio::Menu::create();
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplGtk>();
        subMenus.push_back(subMenu);

        std::string itemLabel = PrepareMnemonics(label);

        gioMenuBar->append_submenu(itemLabel, subMenu->gioMenu);

        return subMenu;
    }

    Gtk::MenuButton* CreateMenuButton(const std::string &label, const std::shared_ptr<MenuImplGtk> &menu) {
        auto button = Gtk::make_managed<Gtk::MenuButton>();
        button->set_label(PrepareMnemonics(label));
        button->set_menu_model(menu->gioMenu);
        button->add_css_class("menu-button");

        button->set_tooltip_text(label + " Menu");
        
        button->set_property("accessible-role", std::string("menu-button"));
        button->set_property("accessible-name", label + " Menu");

        menuButtons.push_back(button);
        return button;
    }

    void Clear() override {
        while (gioMenuBar->get_n_items() > 0) {
            gioMenuBar->remove(0);
        }

        menuButtons.clear();
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

        add_css_class("solvespace-gl-area");
        add_css_class("drawing-area");

        set_tooltip_text("SolveSpace Drawing Area - 3D modeling canvas");
        
        update_property(Gtk::Accessible::Property::ROLE, Gtk::Accessible::Role::CANVAS);
        update_property(Gtk::Accessible::Property::LABEL, "SolveSpace Drawing Area");
        update_property(Gtk::Accessible::Property::DESCRIPTION, "3D modeling canvas for creating and editing models");

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
                process_pointer_event(MouseEvent::Type::MOTION, 
                                     x, y, 
                                     static_cast<GdkModifierType>(state));
                return true;
            });
        
        motion_controller->signal_enter().connect(
            [this](double x, double y) {
                process_pointer_event(MouseEvent::Type::ENTER, x, y, GdkModifierType(0));
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
            }, false);
        add_controller(scroll_controller);

        auto shortcut_controller = Gtk::ShortcutController::create();
        shortcut_controller->set_scope(Gtk::ShortcutScope::LOCAL);
        
        auto key_controller = Gtk::EventControllerKey::create();
        
        key_controller->signal_key_pressed().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                GdkModifierType gdk_state = static_cast<GdkModifierType>(state);
                return process_key_event(KeyboardEvent::Type::PRESS, keyval, gdk_state);
            }, false);
            
        key_controller->signal_key_released().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                GdkModifierType gdk_state = static_cast<GdkModifierType>(state);
                return process_key_event(KeyboardEvent::Type::RELEASE, keyval, gdk_state);
            }, false);
            
        add_controller(key_controller);
        add_controller(shortcut_controller);

        add_css_class("solvespace-gl-widget");
        update_property(Gtk::Accessible::Property::ROLE, Gtk::Accessible::Role::CANVAS);
        update_property(Gtk::Accessible::Property::LABEL, "SolveSpace 3D View");
        update_property(Gtk::Accessible::Property::DESCRIPTION, "Interactive 3D modeling canvas for creating and editing models");
        set_can_focus(true);

        auto focus_controller = Gtk::EventControllerFocus::create();
        focus_controller->signal_enter().connect(
            [this]() {
                grab_focus();
                update_property(Gtk::Accessible::Property::STATE, Gtk::Accessible::State::FOCUSED);
            });
        focus_controller->signal_leave().connect(
            [this]() {
                update_property(Gtk::Accessible::Property::STATE, Gtk::Accessible::State::NONE);
            });
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

class GtkEditorOverlay : public Gtk::Grid {
    Window      *_receiver;
    GtkGLWidget _gl_widget;
    Gtk::Entry  _entry;
    Glib::RefPtr<Gtk::EventControllerKey> _key_controller;
    Glib::RefPtr<Gtk::ShortcutController> _shortcut_controller;
    Glib::RefPtr<Gtk::ConstraintLayout> _constraint_layout;

public:
    GtkEditorOverlay(Platform::Window *receiver) :
        Gtk::Grid(),
        _receiver(receiver),
        _gl_widget(receiver),
        _constraint_layout(Gtk::ConstraintLayout::create()) {

        auto css_provider = Gtk::CssProvider::create();
        css_provider->load_from_data(
            "grid.editor-overlay { "
            "   background-color: transparent; "
            "}"
            "entry.editor-text { "
            "   background-color: white; "
            "   color: black; "
            "   border-radius: 3px; "
            "   padding: 2px; "
            "   caret-color: #0066cc; "
            "   selection-background-color: rgba(0, 102, 204, 0.3); "
            "   selection-color: black; "
            "}"
        );

        set_name("editor-overlay");
        add_css_class("editor-overlay");
        set_row_spacing(4);
        set_column_spacing(4);
        set_row_homogeneous(false);
        
        set_layout_manager(_constraint_layout);
        set_column_homogeneous(false);

        set_tooltip_text("SolveSpace editor overlay with drawing area and text input");
        
        update_property(Gtk::Accessible::Property::ROLE, Gtk::Accessible::Role::PANEL);
        update_property(Gtk::Accessible::Property::LABEL, "SolveSpace Editor");
        update_property(Gtk::Accessible::Property::DESCRIPTION, "Drawing area with text input for SolveSpace parametric CAD");
        
        setup_event_controllers();
        
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_gl_widget, Gtk::Constraint::Attribute::TOP,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::TOP));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_gl_widget, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::LEFT));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_gl_widget, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::RIGHT));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_gl_widget, Gtk::Constraint::Attribute::BOTTOM,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::BOTTOM,
            1.0, -30)); // Leave space for text entry
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_entry, Gtk::Constraint::Attribute::BOTTOM,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::BOTTOM));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_entry, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::LEFT,
            1.0, 10)); // Left margin
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_entry, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::RIGHT,
            1.0, -10)); // Right margin
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_entry, Gtk::Constraint::Attribute::HEIGHT,
            Gtk::Constraint::Relation::EQ,
            nullptr, Gtk::Constraint::Attribute::NONE,
            0.0, 24)); // Fixed height

        Gtk::StyleContext::add_provider_for_display(
            get_display(),
            css_provider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        _gl_widget.set_hexpand(true);
        _gl_widget.set_vexpand(true);

        _entry.set_name("editor-text");
        _entry.add_css_class("editor-text");
        _entry.set_visible(false);
        _entry.set_has_frame(false);
        _entry.set_hexpand(true);
        _entry.set_vexpand(false);

        auto entry_visible_binding = Gtk::PropertyExpression<bool>::create(_entry.property_visible());
        entry_visible_binding->connect([this]() {
            if (_entry.get_visible()) {
                _entry.grab_focus();
                _entry.update_property(Gtk::Accessible::Property::STATE, Gtk::Accessible::State::FOCUSED);
            } else {
                _gl_widget.grab_focus();
            }
        });

        _entry.set_tooltip_text("Text Input");
        
        _entry.update_property(Gtk::Accessible::Property::ROLE, Gtk::Accessible::Role::TEXT_BOX);
        _entry.update_property(Gtk::Accessible::Property::LABEL, "SolveSpace Text Input");
        _entry.update_property(Gtk::Accessible::Property::DESCRIPTION, "Text entry for editing SolveSpace parameters and values");

        attach(_gl_widget, 0, 0);
        attach(_entry, 0, 1);
        
        set_layout_manager(_constraint_layout);
        
        auto gl_guide = _constraint_layout->add_guide(Gtk::ConstraintGuide::create());
        gl_guide->set_min_size(100, 100);
        
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_gl_widget, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::LEFT));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_gl_widget, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::RIGHT));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_gl_widget, Gtk::Constraint::Attribute::TOP,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::TOP));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_entry, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::LEFT));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_entry, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            this, Gtk::Constraint::Attribute::RIGHT));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_entry, Gtk::Constraint::Attribute::TOP,
            Gtk::Constraint::Relation::EQ,
            &_gl_widget, Gtk::Constraint::Attribute::BOTTOM));
        
        _entry.set_margin_start(10);
        _entry.set_margin_end(10);
        _entry.set_margin_bottom(10);
        
        set_valign(Gtk::Align::FILL);
        set_halign(Gtk::Align::FILL);
        
        
        _shortcut_controller = Gtk::ShortcutController::create();
        _shortcut_controller->set_name("editor-shortcuts");
        _shortcut_controller->set_scope(Gtk::ShortcutScope::LOCAL);

        auto enter_action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&) {
            on_activate();
            return true;
        });
        auto enter_trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Return, Gdk::ModifierType(0));
        auto enter_shortcut = Gtk::Shortcut::create(enter_trigger, enter_action);
        enter_shortcut->set_action_name("activate-editor");
        _shortcut_controller->add_shortcut(enter_shortcut);

        auto escape_action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&) {
            if (is_editing()) {
                stop_editing();
                return true;
            }
            return false;
        });
        auto escape_trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Escape, Gdk::ModifierType(0));
        auto escape_shortcut = Gtk::Shortcut::create(escape_trigger, escape_action);
        escape_shortcut->set_action_name("stop-editing");
        _shortcut_controller->add_shortcut(escape_shortcut);

        _entry.add_controller(_shortcut_controller);
        
        _entry.update_property(Gtk::Accessible::Property::DESCRIPTION, 
            _entry.get_property<Glib::ustring>(Gtk::Accessible::Property::DESCRIPTION) + 
            " (Shortcuts: Enter to activate, Escape to cancel)");

        _key_controller = Gtk::EventControllerKey::create();
        _key_controller->set_name("editor-key-controller");
        _key_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
        
        _key_controller->signal_key_pressed().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                GdkModifierType gdk_state = static_cast<GdkModifierType>(state);
                bool handled = on_key_pressed(keyval, keycode, gdk_state);
                
                if (handled && (keyval == GDK_KEY_Delete || 
                               keyval == GDK_KEY_BackSpace || 
                               keyval == GDK_KEY_Tab)) {
                    _gl_widget.update_property(Gtk::Accessible::Property::BUSY, true);
                    _gl_widget.update_property(Gtk::Accessible::Property::ENABLED, true);
                    
                    if (keyval == GDK_KEY_Delete) {
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, "SolveSpace 3D View - Delete Mode");
                    }
                }
                
                return handled;
            }, false);
            
        _key_controller->signal_key_released().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                GdkModifierType gdk_state = static_cast<GdkModifierType>(state);
                return on_key_released(keyval, keycode, gdk_state);
            }, false);

        _gl_widget.add_controller(_key_controller);

        auto size_controller = Gtk::EventControllerMotion::create();
        _gl_widget.add_controller(size_controller);

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

        _constraint_layout->remove_all_constraints();
        
        int adjusted_x = x - margin.get_left() - border.get_left() - padding.get_left();
        int adjusted_y = y - margin.get_top() - border.get_top() - padding.get_top();
        
        int fitWidth = width / Pango::SCALE + padding.get_left() + padding.get_right();
        _entry.set_size_request(max(fitWidth, min_width), -1);
        
        auto entry_constraint_x = Gtk::Constraint::create(
            &_entry,                                // target widget
            Gtk::Constraint::Attribute::LEFT,         // target attribute
            Gtk::Constraint::Relation::EQ,            // relation
            nullptr,                                // source widget (nullptr = parent)
            Gtk::Constraint::Attribute::LEFT,         // source attribute
            1.0,                                    // multiplier
            adjusted_x                              // constant
        );
        
        auto entry_constraint_y = Gtk::Constraint::create(
            &_entry,                                // target widget
            Gtk::Constraint::Attribute::TOP,          // target attribute
            Gtk::Constraint::Relation::EQ,            // relation
            nullptr,                                // source widget (nullptr = parent)
            Gtk::Constraint::Attribute::TOP,          // source attribute
            1.0,                                    // multiplier
            adjusted_y                              // constant
        );
        
        _constraint_layout->add_constraint(entry_constraint_x);
        _constraint_layout->add_constraint(entry_constraint_y);
        
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
    void setup_event_controllers() {
        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->signal_key_pressed().connect(
            [this](unsigned int keyval, unsigned int keycode, Gdk::ModifierType state) -> bool {
                if(is_editing()) {
                    if(keyval == GDK_KEY_Escape) {
                        stop_editing();
                        return true;
                    }
                    return false; // Let the entry handle it
                }
                return false;
            }, false);
            
        key_controller->signal_key_released().connect(
            [this](unsigned int keyval, unsigned int keycode, Gdk::ModifierType state) -> bool {
                if(is_editing()) {
                    return false; // Let the entry handle it
                }
                return false;
            }, false);
            
        add_controller(key_controller);
    }

    void on_size_allocate() {
        int width = get_width();
        int height = get_height();

        _gl_widget.set_size_request(width, height);

        if(_entry.get_visible()) {
            int min_height = 0, natural_height = 0;
            int min_width = 0, natural_width = 0;

            _entry.measure(Gtk::Orientation::VERTICAL, -1,
                          min_height, natural_height,
                          min_width, natural_width);

            int entry_width = _entry.get_width();
            int entry_height = natural_height;

            _entry.set_size_request(entry_width > 0 ? entry_width : 100, entry_height);
            
            _constraint_layout->set_layout_requested();
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
    std::string         _tooltip_text;
    Gdk::Rectangle      _tooltip_area;
    Glib::RefPtr<Gtk::EventControllerMotion> _motion_controller;

    bool _is_under_cursor;
    bool _is_fullscreen;
    Glib::RefPtr<Gtk::ConstraintLayout> _constraint_layout;
    
    void setup_state_binding() {
        auto state_binding = Gtk::PropertyExpression<Gdk::ToplevelState>::create(property_state());
        state_binding->connect([this](Gdk::ToplevelState state) {
            bool is_fullscreen = (state & Gdk::ToplevelState::FULLSCREEN) != 0;
            
            if (_is_fullscreen != is_fullscreen) {
                _is_fullscreen = is_fullscreen;
                
                set_property("accessible-state", 
                    std::string(is_fullscreen ? "expanded" : "collapsed"));
                
                if(_receiver->onFullScreen) {
                    _receiver->onFullScreen(is_fullscreen);
                }
            }
        });
        
        set_property("accessible-role", std::string("application"));
    //     update_property(Gtk::Accessible::Property::LABEL, "SolveSpace");
    //     update_property(Gtk::Accessible::Property::DESCRIPTION, "Parametric 2D/3D CAD application");
    // }
    
    void setup_event_controllers() {
        _motion_controller = Gtk::EventControllerMotion::create();
        _motion_controller->set_name("window-motion-controller");
        _motion_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
        
        _motion_controller->signal_enter().connect(
            [this](double x, double y) {
                _is_under_cursor = true;
                
                update_property(Gtk::Accessible::Property::ROLE, Gtk::Accessible::Role::APPLICATION);
                update_property(Gtk::Accessible::Property::STATE, Gtk::Accessible::State::FOCUSED);
                
                return true;
            });
            
        _motion_controller->signal_leave().connect(
            [this]() {
                _is_under_cursor = false;
                
                update_property(Gtk::Accessible::Property::STATE, Gtk::Accessible::State::NONE);
                
                return true;
            });
            
        add_controller(_motion_controller);
        
        signal_close_request().connect(
            [this]() -> bool {
                if(_receiver->onClose) {
                    _receiver->onClose();
                }
                return true;
            }, false);

        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->set_name("window-key-controller");
        key_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
        
        key_controller->signal_key_pressed().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) {
                if(_receiver->onKeyDown) {
                    Platform::KeyboardEvent event = {};
                    if(keyval == GDK_KEY_Escape) {
                        event.key = Platform::KeyboardEvent::Key::CHARACTER;
                        event.chr = '\x1b';
                    } else if(keyval == GDK_KEY_Delete) {
                        event.key = Platform::KeyboardEvent::Key::CHARACTER;
                        event.chr = '\x7f';
                    } else if(keyval == GDK_KEY_Tab) {
                        event.key = Platform::KeyboardEvent::Key::CHARACTER;
                        event.chr = '\t';
                    } else if(keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F12) {
                        event.key = Platform::KeyboardEvent::Key::FUNCTION;
                        event.num = keyval - GDK_KEY_F1 + 1;
                    } else if(keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9) {
                        event.key = Platform::KeyboardEvent::Key::CHARACTER;
                        event.chr = '0' + (keyval - GDK_KEY_0);
                    } else if(keyval >= GDK_KEY_a && keyval <= GDK_KEY_z) {
                        event.key = Platform::KeyboardEvent::Key::CHARACTER;
                        event.chr = 'a' + (keyval - GDK_KEY_a);
                    } else if(keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z) {
                        event.key = Platform::KeyboardEvent::Key::CHARACTER;
                        event.chr = 'A' + (keyval - GDK_KEY_A);
                    } else {
                        guint32 unicode = gdk_keyval_to_unicode(keyval);
                        if(unicode) {
                            event.key = Platform::KeyboardEvent::Key::CHARACTER;
                            event.chr = unicode;
                        }
                    }
                    
                    event.shiftDown = (state & Gdk::ModifierType::SHIFT_MASK) != 0;
                    event.controlDown = (state & Gdk::ModifierType::CONTROL_MASK) != 0;
                    
                    if (keyval == GDK_KEY_Escape || keyval == GDK_KEY_Delete || 
                        keyval == GDK_KEY_Tab || (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F12)) {
                        set_accessible_state(Gtk::AccessibleState::BUSY);
                        set_accessible_state(Gtk::AccessibleState::ENABLED);
                    }
                    
                    _receiver->onKeyDown(event);
                    return true;
                }
                return false;
            }, false);
            
        key_controller->signal_key_released().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                if(_receiver->onKeyUp) {
                    return _receiver->onKeyUp(keyval, state);
                }
                return false;
            }, false);
            
        add_controller(key_controller);
        
        auto gesture_controller = Gtk::GestureClick::create();
        gesture_controller->set_name("window-click-controller");
        gesture_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
        gesture_controller->set_button(0); // Any button
        
        gesture_controller->signal_pressed().connect(
            [this](int n_press, double x, double y) {
                set_accessible_state(Gtk::AccessibleState::ACTIVE);
            });
            
        add_controller(gesture_controller);
    }
    
    void setup_state_binding() {
        auto state_binding = Gtk::PropertyExpression<Gdk::ToplevelState>::create(property_state());
        state_binding->connect([this]() {
            auto state = get_state();
            bool is_fullscreen = (state & Gdk::ToplevelState::FULLSCREEN) != 0;
            
            if (_is_fullscreen != is_fullscreen) {
                _is_fullscreen = is_fullscreen;
                
                update_property(Gtk::Accessible::Property::STATE_EXPANDED, is_fullscreen);
                
                if(_receiver->onFullScreen) {
                    _receiver->onFullScreen(is_fullscreen);
                }
            }
        });
        
        update_property(Gtk::Accessible::Property::ROLE, Gtk::Accessible::Role::APPLICATION);
        update_property(Gtk::Accessible::Property::LABEL, "SolveSpace");
        update_property(Gtk::Accessible::Property::DESCRIPTION, "Parametric 2D/3D CAD application");
    }

    void setup_property_bindings() {
        auto settings = Gtk::Settings::get_default();
        auto theme_binding = Gtk::PropertyExpression<bool>::create(
            settings->property_gtk_application_prefer_dark_theme());
        theme_binding->connect([this, settings]() {
            bool dark_theme = settings->property_gtk_application_prefer_dark_theme();
            if (dark_theme) {
                add_css_class("dark");
                remove_css_class("light");
            } else {
                add_css_class("light");
                remove_css_class("dark");
            }
            
            set_property("accessible-description", 
                std::string("Parametric 2D/3D CAD application") + 
                (dark_theme ? " (Dark theme)" : " (Light theme)"));
        });
    }

public:
    GtkWindow(Platform::Window *receiver) :
        _receiver(receiver),
        _vbox(Gtk::Orientation::VERTICAL),
        _hbox(Gtk::Orientation::HORIZONTAL),
        _editor_overlay(receiver),
        _scrollbar(),
        _is_under_cursor(false),
        _is_fullscreen(false),
        _constraint_layout(Gtk::ConstraintLayout::create()) {
        _scrollbar.set_orientation(Gtk::Orientation::VERTICAL);

        auto css_provider = Gtk::CssProvider::create();
        css_provider->load_from_data(R"css(
            window.solvespace-window { 
                background-color: @theme_bg_color; 
            }
            window.solvespace-window.dark { 
                background-color: #303030; 
            }
            window.solvespace-window.light { 
                background-color: #f0f0f0; 
            }
            scrollbar { 
                background-color: alpha(@theme_fg_color, 0.1); 
                border-radius: 0; 
            }
            scrollbar slider {
                min-width: 16px;
                border-radius: 8px;
                background-color: alpha(@theme_fg_color, 0.3);
            }
            .solvespace-gl-area {
                background-color: @theme_base_color;
                border-radius: 2px;
                border: 1px solid @borders;
            }
            .solvespace-header {
                padding: 4px;
            }
            .solvespace-editor-text {
                background-color: @theme_base_color;
                color: @theme_text_color;
                border-radius: 3px;
                padding: 4px;
                caret-color: @link_color;
            }
            button {
                padding: 6px 10px;
                border-radius: 4px;
            }
            button:focus {
                outline: 2px solid alpha(@accent_color, 0.8);
                outline-offset: 1px;
            }
            button.suggested-action:focus {
                outline-color: alpha(@accent_color, 0.9);
            }
            button.destructive-action:focus {
                outline-color: alpha(@destructive_color, 0.8);
            }
        )css");

        set_name("solvespace-window");
        add_css_class("solvespace-window");
        
        Gtk::StyleContext::add_provider_for_display(
            get_display(),
            css_provider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        _hbox.set_hexpand(true);
        _hbox.set_vexpand(true);
        _editor_overlay.set_hexpand(true);
        _editor_overlay.set_vexpand(true);

        _hbox.append(_editor_overlay);
        _hbox.append(_scrollbar);
        _vbox.append(_hbox);
        set_child(_vbox);

        _vbox.set_layout_manager(_constraint_layout);
        
        setup_event_controllers();
        setup_state_binding();
        setup_property_bindings();
        
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_hbox, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            &_vbox, Gtk::Constraint::Attribute::LEFT));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_hbox, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            &_vbox, Gtk::Constraint::Attribute::RIGHT));
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_editor_overlay, Gtk::Constraint::Attribute::WIDTH,
            Gtk::Constraint::Relation::EQ,
            &_hbox, Gtk::Constraint::Attribute::WIDTH,
            1.0, -20)); // Subtract scrollbar width
            
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            &_scrollbar, Gtk::Constraint::Attribute::WIDTH,
            Gtk::Constraint::Relation::EQ,
            nullptr, Gtk::Constraint::Attribute::NONE,
            0.0, 20)); // Fixed width for scrollbar
        
        _vbox.set_visible(true);
        _hbox.set_visible(true);
        _editor_overlay.set_visible(true);
        get_gl_widget().set_visible(true);

        auto adjustment = Gtk::Adjustment::create(0.0, 0.0, 100.0, 1.0, 10.0, 10.0);
        _scrollbar.set_adjustment(adjustment);

        auto value_binding = Gtk::PropertyExpression<double>::create(adjustment->property_value());
        value_binding->connect([this, adjustment]() {
            double value = adjustment->get_value();
            if(_receiver->onScrollbarAdjusted) {
                _receiver->onScrollbarAdjusted(value / adjustment->get_upper());
            }
        });

        get_gl_widget().set_has_tooltip(true);
        get_gl_widget().signal_query_tooltip().connect(
            [this](int x, int y, bool keyboard_tooltip,
                  const Glib::RefPtr<Gtk::Tooltip> &tooltip) -> bool {
                tooltip->set_text(_tooltip_text);
                tooltip->set_tip_area(_tooltip_area);
                return !_tooltip_text.empty() && (keyboard_tooltip || _is_under_cursor);
            });

        setup_event_controllers();
        setup_state_binding();
        setup_property_bindings();
        
        update_property(Gtk::Accessible::Property::ROLE, Gtk::Accessible::Role::APPLICATION);
        update_property(Gtk::Accessible::Property::LABEL, "SolveSpace");
        update_property(Gtk::Accessible::Property::DESCRIPTION, "Parametric 2D/3D CAD application");
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

    bool on_query_tooltip(int x, int y, bool keyboard_tooltip,
                          const Glib::RefPtr<Gtk::Tooltip> &tooltip) {
        tooltip->set_text(_tooltip_text);
        tooltip->set_tip_area(_tooltip_area);
        return !_tooltip_text.empty() && (keyboard_tooltip || _is_under_cursor);
    }
};

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

class WindowImplGtk final : public Window {
public:
    GtkWindow       gtkWindow;
    MenuBarRef      menuBar;

    bool _visible;
    bool _fullscreen;

    WindowImplGtk(Window::Kind kind) :
        gtkWindow(this),
        _visible(false),
        _fullscreen(false)
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

        auto css_provider = Gtk::CssProvider::create();
        css_provider->load_from_data(R"css(
            window.tool-window { 
                background-color: @theme_bg_color; 
            }
            window.tool-window.dark { 
                background-color: #303030; 
            }
            window.tool-window.light { 
                background-color: #f5f5f5; 
            }
            .tool-window .menu-button {
                margin: 2px;
                padding: 4px 8px;
            }
            .tool-window .menu-item {
                padding: 6px 8px;
            }
        )css");

        if (kind == Kind::TOOL) {
            gtkWindow.set_name("tool-window");
            gtkWindow.add_css_class("tool-window");
            
            auto settings = Gtk::Settings::get_default();
            if (settings->property_gtk_application_prefer_dark_theme()) {
                gtkWindow.add_css_class("dark");
            } else {
                gtkWindow.add_css_class("light");
            }
            
            auto theme_binding = Gtk::PropertyExpression<bool>::create(settings->property_gtk_application_prefer_dark_theme());
            theme_binding->connect([this, settings]() {
                bool dark_theme = settings->property_gtk_application_prefer_dark_theme();
                if (dark_theme) {
                    gtkWindow.add_css_class("dark");
                    gtkWindow.remove_css_class("light");
                } else {
                    gtkWindow.add_css_class("light");
                    gtkWindow.remove_css_class("dark");
                }
            });
            
            Gtk::StyleContext::add_provider_for_display(
                gtkWindow.get_display(),
                css_provider,
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }

        gtkWindow.add_css_class("window");
        
        gtkWindow.set_tooltip_text("SolveSpace - Parametric 2D/3D CAD tool");
        
        gtkWindow.update_property(Gtk::Accessible::Property::ROLE, 
            kind == Kind::TOOL ? Gtk::Accessible::Role::DIALOG : Gtk::Accessible::Role::APPLICATION);
        gtkWindow.update_property(Gtk::Accessible::Property::LABEL, "SolveSpace");
        gtkWindow.update_property(Gtk::Accessible::Property::DESCRIPTION, 
            "Parametric 2D/3D CAD tool");
    }

    double GetPixelDensity() override {
        return gtkWindow.get_scale_factor();
    }

    double GetDevicePixelRatio() override {
        return gtkWindow.get_scale_factor();
    }

    bool IsVisible() override {
        return _visible;
    }

    void SetVisible(bool visible) override {
        _visible = visible;
        if (_visible) {
            gtkWindow.show();
        } else {
            gtkWindow.hide();
        }
    }

    void Focus() override {
        gtkWindow.present();
    }

    bool IsFullScreen() override {
        return _fullscreen;
    }

    void SetFullScreen(bool fullScreen) override {
        _fullscreen = fullScreen;
        if (_fullscreen) {
            gtkWindow.fullscreen();
        } else {
            gtkWindow.unfullscreen();
        }
    }

    void SetTitle(const std::string &title) override {
        std::string prepared_title = PrepareTitle(title);
        gtkWindow.set_title(prepared_title);
        
        gtkWindow.update_property(Gtk::Accessible::Property::LABEL, 
            "SolveSpace" + (title.empty() ? "" : ": " + title));
    }

    void SetMenuBar(MenuBarRef newMenuBar) override {
        if(newMenuBar) {
            auto headerBar = Gtk::make_managed<Gtk::HeaderBar>();
            headerBar->set_show_title_buttons(true);

            auto menuBarImpl = std::static_pointer_cast<MenuBarImplGtk>(newMenuBar);

            for (size_t menuIndex = 0; menuIndex < menuBarImpl->subMenus.size(); menuIndex++) {
                const auto& subMenu = menuBarImpl->subMenus[menuIndex];
                auto menuButton = Gtk::make_managed<Gtk::MenuButton>();
                menuButton->add_css_class("menu-button");

                Glib::ustring menuLabel;
                if (subMenu->gioMenu->get_n_items() > 0) {
                    auto menuImpl = std::static_pointer_cast<MenuImplGtk>(subMenu);
                    if (!menuImpl->name.empty()) {
                        menuLabel = menuImpl->name;
                    } else {
                        menuLabel = "Menu " + std::to_string(menuIndex+1);
                    }
                } else {
                    menuLabel = "Menu " + std::to_string(menuIndex+1);
                }
                menuButton->set_label(menuLabel);

                menuButton->set_tooltip_text(menuButton->get_label());
                menuButton->add_css_class("menu-button");
                menuButton->set_tooltip_text(menuButton->get_label() + " Menu");

                auto popover = Gtk::make_managed<Gtk::Popover>();
                menuButton->set_popover(*popover);

                auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
                box->set_spacing(2);
                box->set_margin(8);
                
                auto constraint_layout = Gtk::ConstraintLayout::create();
                box->set_layout_manager(constraint_layout);

                for (size_t i = 0; i < subMenu->menuItems.size(); i++) {
                    auto menuItem = subMenu->menuItems[i];

                    auto item_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
                    item_box->set_spacing(8);
                    
                    auto item = Gtk::make_managed<Gtk::Button>();
                    item->set_label(menuItem->label);
                    item->set_has_frame(false);
                    item->add_css_class("flat");
                    item->add_css_class("menu-item");
                    item->set_halign(Gtk::Align::FILL);
                    item->set_hexpand(true);
                    item->set_tooltip_text(menuItem->name);
                    
                    item->update_property(Gtk::Accessible::Property::ROLE, Gtk::Accessible::Role::MENU_ITEM);
                    item->update_property(Gtk::Accessible::Property::LABEL, menuItem->name);
                    item->update_property(Gtk::Accessible::Property::DESCRIPTION, 
                        "Menu item: " + menuItem->name);

                    if (menuItem->onTrigger) {
                        auto active_binding = Gtk::PropertyExpression<bool>::create(item->property_active());
                        active_binding->connect([item]() {
                            bool active = item->get_active();
                            if (active) {
                                item->update_property(Gtk::Accessible::Property::STATE, 
                                    Gtk::Accessible::State::PRESSED);
                            } else {
                                item->update_property(Gtk::Accessible::Property::STATE, 
                                    Gtk::Accessible::State::NONE);
                            }
                        });
                        
                        auto action = Gtk::CallbackAction::create([popover, onTrigger = menuItem->onTrigger](Gtk::Widget&, const Glib::VariantBase&) {
                            popover->popdown();
                            onTrigger();
                            return true;
                        });
                        
                        auto shortcut = Gtk::Shortcut::create(
                            Gtk::ShortcutTrigger::parse("pressed"), action);
                            
                        auto controller = Gtk::ShortcutController::create();
                        controller->add_shortcut(shortcut);
                        
                        auto click_controller = Gtk::GestureClick::create();
                        click_controller->set_button(GDK_BUTTON_PRIMARY);
                        click_controller->signal_released().connect(
                            [popover, onTrigger = menuItem->onTrigger](int n_press, double x, double y) {
                                popover->popdown();
                                onTrigger();
                            });
                            
                        item->add_controller(controller);
                        item->add_controller(click_controller);
                    }

                    item_box->append(*item);
                    
                    auto menuItemImpl = std::dynamic_pointer_cast<MenuItemImplGtk>(menuItem);
                    if (menuItemImpl && !menuItemImpl->shortcutText.empty()) {
                        auto shortcutLabel = Gtk::make_managed<Gtk::Label>();
                        shortcutLabel->set_label(menuItemImpl->shortcutText);
                        shortcutLabel->add_css_class("dim-label");
                        shortcutLabel->set_halign(Gtk::Align::END);
                        shortcutLabel->set_hexpand(true);
                        shortcutLabel->set_margin_start(16);
                        
                        shortcutLabel->update_property(Gtk::Accessible::Property::ROLE, 
                            Gtk::Accessible::Role::LABEL);
                        shortcutLabel->update_property(Gtk::Accessible::Property::LABEL, 
                            "Shortcut: " + menuItemImpl->shortcutText);
                        
                        item_box->append(*shortcutLabel);
                    }
                    
                    box->append(*item_box);
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
        
        auto value_binding = Gtk::PropertyExpression<double>::create(adjustment->property_value());
        value_binding->connect([this, adjustment]() {
            double value = adjustment->get_value();
            if(onScrollbarAdjusted) {
                onScrollbarAdjusted(value / adjustment->get_upper());
            }
        });
        
        gtkWindow.get_scrollbar().set_adjustment(adjustment);
        
        gtkWindow.get_scrollbar().update_property(Gtk::Accessible::Property::VALUE_MAX, max);
        gtkWindow.get_scrollbar().update_property(Gtk::Accessible::Property::VALUE_MIN, min);
        gtkWindow.get_scrollbar().update_property(Gtk::Accessible::Property::VALUE_NOW, adjustment->get_value());
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

        auto content_area = gtkDialog.get_content_area();
        if (content_area) {
            content_area->add_css_class("dialog-content-area");
        }

        gtkDialog.set_property("accessible-role", Gtk::Accessible::Role::DIALOG);
        gtkDialog.set_property("accessible-name", std::string("SolveSpace Message"));
        gtkDialog.set_property("accessible-description", std::string("Dialog displaying a message from SolveSpace"));
        
        gtkDialog.add_css_class("solvespace-dialog");
        gtkDialog.add_css_class("message-dialog");
    }

    void SetType(Type type) override {
        const char* icon_name = "dialog-information";

        switch(type) {
            case Type::INFORMATION:
                icon_name = "dialog-information";
                gtkDialog.set_modal(true);
                break;

            case Type::QUESTION:
                icon_name = "dialog-question";
                gtkDialog.set_modal(true);
                gtkDialog.set_destroy_with_parent(true);
                break;

            case Type::WARNING:
                icon_name = "dialog-warning";
                gtkDialog.set_modal(true);
                gtkDialog.set_destroy_with_parent(true);
                break;

            case Type::ERROR:
                icon_name = "dialog-error";
                gtkDialog.set_modal(true);
                gtkDialog.set_destroy_with_parent(true);
                break;

            default:
                icon_name = "dialog-error";
                gtkDialog.set_modal(true);
                gtkDialog.set_destroy_with_parent(true);
                break;
        }

        gtkImage.set_from_icon_name(icon_name);
        gtkImage.set_icon_size(Gtk::IconSize::LARGE);
        gtkImage.add_css_class("dialog-icon");

        auto content_area = gtkDialog.get_content_area();
        content_area->add_css_class("dialog-content");
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
        
        if (!message.empty()) {
            std::string dialogType = "Message";
            if (gtkDialog.get_message_type() == Gtk::MessageType::QUESTION) {
                dialogType = "Question";
            } else if (gtkDialog.get_message_type() == Gtk::MessageType::WARNING) {
                dialogType = "Warning";
            } else if (gtkDialog.get_message_type() == Gtk::MessageType::ERROR) {
                dialogType = "Error";
            }
            
            gtkDialog.set_property("accessible-name", "SolveSpace " + dialogType + ": " + message);
        }
    }

    void SetDescription(std::string description) override {
        gtkDialog.set_secondary_text(description);
        
        if (!description.empty()) {
            gtkDialog.set_property("accessible-description", description);
        }
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
        
        auto button = gtkDialog.add_button(PrepareMnemonics(label), responseId);
        
        if(isDefault) {
            gtkDialog.set_default_response(responseId);
            button->add_css_class("suggested-action");
        }
        
        button->set_property("accessible-role", Gtk::Accessible::Role::BUTTON);
        button->set_property("accessible-name", label);
            
        std::string description;
            switch(response) {
                case Response::OK:     description = "Confirm the action"; break;
                case Response::YES:    description = "Agree with the question"; break;
                case Response::NO:     description = "Disagree with the question"; break;
                case Response::CANCEL: description = "Cancel the operation"; break;
                default: break;
            }
            
            if (!description.empty()) {
                button->set_property("accessible-description", description);
            }
        
        switch(response) {
            case Response::OK:
            case Response::YES:
                button->add_css_class("affirmative-action");
                break;
            case Response::CANCEL:
                button->add_css_class("cancel-action");
                break;
            case Response::NO:
                button->add_css_class("negative-action");
                break;
            default:
                break;
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
        shownMessageDialogs.push_back(shared_from_this());

        auto dialog_visible_binding = Gtk::PropertyExpression<bool>::create(gtkDialog.property_visible());
        dialog_visible_binding->connect([this]() {
            bool visible = gtkDialog.get_visible();
            if (!visible) {
                auto it = std::remove(shownMessageDialogs.begin(), shownMessageDialogs.end(),
                                     shared_from_this());
                shownMessageDialogs.erase(it);
            }
        });

        auto response_controller = Gtk::EventControllerKey::create();
        response_controller->signal_key_released().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                if (keyval == GDK_KEY_Escape) {
                    int gtkResponse = Gtk::ResponseType::CANCEL;
                    ProcessResponse(gtkResponse);
                    gtkDialog.hide();
                    return true;
                }
                return false;
            });
        gtkDialog.add_controller(response_controller);

        gtkDialog.show();
    }

    Response RunModal() override {
        gtkDialog.set_modal(true);

        int response = Gtk::ResponseType::NONE;
        auto loop = Glib::MainLoop::create();

        auto shortcut_controller = Gtk::ShortcutController::create();
        shortcut_controller->set_scope(Gtk::ShortcutScope::LOCAL);
        shortcut_controller->set_name("dialog-shortcuts");

        auto escape_action = Gtk::CallbackAction::create([&loop](Gtk::Widget&, const Glib::VariantBase&) {
            loop->quit();
            return true;
        });
        auto escape_shortcut = Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_Escape, Gdk::ModifierType(0)),
            escape_action);
        escape_shortcut->set_action_name("escape");
        shortcut_controller->add_shortcut(escape_shortcut);

        auto enter_action = Gtk::CallbackAction::create([this, &response, &loop](Gtk::Widget&, const Glib::VariantBase&) {
            auto default_response = gtkDialog.get_default_response();
            if (default_response != Gtk::ResponseType::NONE) {
                response = default_response;
                loop->quit();
                return true;
            }
            return false;
        });
        auto enter_shortcut = Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_Return, Gdk::ModifierType(0)),
            enter_action);
        enter_shortcut->set_action_name("activate-default");
        shortcut_controller->add_shortcut(enter_shortcut);

        gtkDialog.add_controller(shortcut_controller);

        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->set_name("dialog-key-controller");
        key_controller->signal_key_pressed().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                auto default_widget = gtkDialog.get_default_widget();
                if (default_widget) {
                    default_widget->grab_focus();
                    
                    auto accessible = default_widget->get_accessible();
                    if (accessible) {
                        std::string name;
                        accessible->get_property("accessible-name", name);
                        if (!name.empty()) {
                            accessible->set_property("accessible-state", std::string("focused"));
                        }
                    }
                }
                return false; // Allow event propagation
            });
        gtkDialog.add_controller(key_controller);

        auto response_controller = Gtk::EventControllerKey::create();
        response_controller->set_name("dialog-response-controller");
        response_controller->signal_key_released().connect(
            [&response, &loop, this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
                    response = gtkDialog.get_default_response();
                    if (response != Gtk::ResponseType::NONE) {
                        loop->quit();
                        return true;
                    }
                }
                return false;
            });
        gtkDialog.add_controller(response_controller);

        auto dialog_visible_binding = Gtk::PropertyExpression<bool>::create(gtkDialog.property_visible());
        dialog_visible_binding->connect([&loop, &response, this, &gtkDialog]() {
            if (!gtkDialog.get_visible()) {
                loop->quit();
            }
        });

        gtkDialog.set_tooltip_text("Message Dialog");
        
        auto accessible = gtkDialog.get_accessible();
        if (accessible) {
            accessible->set_property("accessible-state", std::string("modal"));
            accessible->set_property("accessible-role", Gtk::Accessible::Role::DIALOG);
        }

        gtkDialog.show();
        loop->run();

        gtkDialog.hide();

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
        
        if (auto widget = dynamic_cast<Gtk::Widget*>(gtkChooser)) {
            widget->set_property("accessible-role", Gtk::Accessible::Role::FILE_CHOOSER);
            widget->set_property("accessible-name", std::string("SolveSpace File Chooser"));
            widget->set_property("accessible-description", std::string("Dialog for selecting files in SolveSpace"));
            
            widget->add_css_class("solvespace-file-dialog");
        }
        
        if (auto dialog = dynamic_cast<Gtk::FileChooserDialog*>(gtkChooser)) {
            auto response_controller = Gtk::EventControllerKey::create();
            response_controller->set_name("file-dialog-response-controller");
            response_controller->signal_key_released().connect(
                [this, dialog](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
                        int response = dialog->get_default_response();
                        if (response == Gtk::ResponseType::OK) {
                            this->FilterChanged();
                        }
                        return true;
                    }
                    return false;
                });
            dialog->add_controller(response_controller);
            
            auto filter_binding = Gtk::PropertyExpression<Glib::RefPtr<Gtk::FileFilter>>::create(gtkChooser->property_filter());
            filter_binding->connect([this]() {
                this->FilterChanged();
            });
            
            auto shortcut_controller = Gtk::ShortcutController::create();
            shortcut_controller->set_scope(Gtk::ShortcutScope::LOCAL);
            shortcut_controller->set_name("file-dialog-shortcuts");
            
            auto home_action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&) {
                gtkChooser->set_current_folder(Gio::File::create_for_path(Glib::get_home_dir()));
                return true;
            });
            auto home_shortcut = Gtk::Shortcut::create(
                Gtk::KeyvalTrigger::create(GDK_KEY_h, Gdk::ModifierType::CONTROL_MASK | Gdk::ModifierType::ALT_MASK),
                home_action);
            shortcut_controller->add_shortcut(home_shortcut);
            
            dialog->add_controller(shortcut_controller);
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
                           : Gtk::FileChooser::Action::OPEN)
    {
        gtkDialog.set_transient_for(gtkParent);
        gtkDialog.set_modal(true);

        gtkDialog.add_css_class("dialog");
        gtkDialog.add_css_class("solvespace-file-dialog");
        gtkDialog.add_css_class(isSave ? "save-dialog" : "open-dialog");

        gtkDialog.set_name(isSave ? "save-file-dialog" : "open-file-dialog");
        gtkDialog.set_title(isSave ? "Save File" : "Open File");

        gtkDialog.set_property("accessible-role", std::string("dialog"));
        gtkDialog.set_property("accessible-name", std::string(isSave ? "Save File" : "Open File"));
        gtkDialog.set_property("accessible-description", 
            std::string(isSave ? "Dialog for saving SolveSpace files" : "Dialog for opening SolveSpace files"));

        auto cancel_button = gtkDialog.add_button(C_("button", "_Cancel"), Gtk::ResponseType::CANCEL);
        cancel_button->add_css_class("destructive-action");
        cancel_button->add_css_class("cancel-action");
        cancel_button->set_name("cancel-button");
        cancel_button->set_tooltip_text("Cancel");
        
        cancel_button->set_property("accessible-role", std::string("button"));
        cancel_button->set_property("accessible-name", std::string("Cancel"));
        cancel_button->set_property("accessible-description", std::string("Cancel the file operation"));

        auto action_button = gtkDialog.add_button(
            isSave ? C_("button", "_Save") : C_("button", "_Open"),
            Gtk::ResponseType::OK);
        action_button->add_css_class("suggested-action");
        action_button->add_css_class(isSave ? "save-action" : "open-action");
        action_button->set_name(isSave ? "save-button" : "open-button");
        action_button->set_tooltip_text(isSave ? "Save" : "Open");
        
        action_button->set_property("accessible-role", std::string("button"));
        action_button->set_property("accessible-name", std::string(isSave ? "Save" : "Open"));
        action_button->set_property("accessible-description", 
            std::string(isSave ? "Save the current file" : "Open the selected file"));

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

        auto loop = Glib::MainLoop::create();
        auto response_id = Gtk::ResponseType::CANCEL;

        auto response_controller = Gtk::EventControllerLegacy::create();
        response_controller->set_name("file-dialog-response-controller");
        response_controller->signal_event().connect(
            [&loop, &response_id, this](const GdkEvent* event) -> bool {
                if (gdk_event_get_event_type(event) == GDK_RESPONSE) {
                    response_id = static_cast<Gtk::ResponseType>(gtkDialog.get_response());
                    loop->quit();
                    return true;
                }
                return false;
            });
        gtkDialog.add_controller(response_controller);

        auto shortcut_controller = Gtk::ShortcutController::create();
        shortcut_controller->set_scope(Gtk::ShortcutScope::LOCAL);
        shortcut_controller->set_name("file-dialog-shortcuts");
        
        auto escape_action = Gtk::CallbackAction::create([&loop](Gtk::Widget&, const Glib::VariantBase&) {
            loop->quit();
            return true;
        });
        auto escape_shortcut = Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_Escape, Gdk::ModifierType(0)),
            escape_action);
        escape_shortcut->set_action_name("escape");
        shortcut_controller->add_shortcut(escape_shortcut);
        
        auto enter_action = Gtk::CallbackAction::create([&response_id, &loop, this](Gtk::Widget&, const Glib::VariantBase&) {
            response_id = Gtk::ResponseType::OK;
            loop->quit();
            return true;
        });
        auto enter_shortcut = Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_Return, Gdk::ModifierType(0)),
            enter_action);
        enter_shortcut->set_action_name("activate-default");
        shortcut_controller->add_shortcut(enter_shortcut);
        
        gtkDialog.add_controller(shortcut_controller);

        auto visibility_binding = Gtk::PropertyExpression<bool>::create(
            Gtk::Dialog::get_type(), &gtkDialog, "visible");
        visibility_binding->connect([&loop, this](bool visible) {
            if (!visible) {
                loop->quit();
            }
        });

        auto accessible = gtkDialog.get_accessible();
        if (accessible) {
            accessible->set_property("accessible-state", std::string("modal"));
        }

        gtkDialog.show();
        loop->run();

        return response_id == Gtk::ResponseType::OK;
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

        gtkNative->set_modal(true);

        gtkNative->add_css_class("dialog");
        gtkNative->add_css_class("solvespace-file-dialog");
        gtkNative->add_css_class(isSave ? "save-dialog" : "open-dialog");
        
        gtkNative->set_title(isSave ? "Save SolveSpace File" : "Open SolveSpace File");
        
        gtkNative->set_property("accessible-role", Gtk::Accessible::Role::DIALOG);
        gtkNative->set_property("accessible-name", isSave ? "Save File" : "Open File");
        gtkNative->set_property("accessible-description", 
            isSave ? "Dialog to save SolveSpace files" : "Dialog to open SolveSpace files");

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

        int response_id = Gtk::ResponseType::CANCEL;
        auto loop = Glib::MainLoop::create();

        auto response_binding = Gtk::PropertyExpression<int>::create(gtkNative->property_response());
        response_binding->connect([&response_id, &loop, this]() {
            int response = gtkNative->get_response();
            if (response != Gtk::ResponseType::NONE) {
                response_id = response;
                loop->quit();
            }
        });

        auto visible_binding = Gtk::PropertyExpression<bool>::create(gtkNative->property_visible());
        visible_binding->connect([&loop, this]() {
            if (!gtkNative->get_visible()) {
                loop->quit();
            }
        });

        if (auto widget = gtkNative->get_widget()) {
            widget->add_css_class("solvespace-file-dialog");
            widget->add_css_class("dialog");
            widget->add_css_class(isSave ? "save-dialog" : "open-dialog");
            
            auto accessible = widget->get_accessible();
            if (accessible) {
                accessible->set_property("accessible-role", "file-chooser");
                accessible->set_property("accessible-name", isSave ? "Save SolveSpace File" : "Open SolveSpace File");
                accessible->set_property("accessible-description", 
                    isSave ? "Dialog for saving SolveSpace files" : "Dialog for opening SolveSpace files");
                accessible->set_property("accessible-state", std::string("modal"));
            }

            auto shortcut_controller = Gtk::ShortcutController::create();
            shortcut_controller->set_scope(Gtk::ShortcutScope::LOCAL);
            shortcut_controller->set_name("native-file-dialog-shortcuts");
            
            auto escape_action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&) {
                gtkNative->response(Gtk::ResponseType::CANCEL);
                return true;
            });
            auto escape_shortcut = Gtk::Shortcut::create(
                Gtk::KeyvalTrigger::create(GDK_KEY_Escape, Gdk::ModifierType(0)),
                escape_action);
            escape_shortcut->set_action_name("escape");
            shortcut_controller->add_shortcut(escape_shortcut);

            auto enter_action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&) {
                gtkNative->response(Gtk::ResponseType::ACCEPT);
                return true;
            });
            auto enter_shortcut = Gtk::Shortcut::create(
                Gtk::KeyvalTrigger::create(GDK_KEY_Return, Gdk::ModifierType(0)),
                enter_action);
            enter_shortcut->set_action_name("activate-default");
            shortcut_controller->add_shortcut(enter_shortcut);

            widget->add_controller(shortcut_controller);

            auto key_controller = Gtk::EventControllerKey::create();
            key_controller->set_name("native-file-dialog-key-controller");
            key_controller->signal_key_pressed().connect(
                [widget](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                    auto buttons = widget->observe_children();
                    for (auto child : buttons) {
                        if (auto button = dynamic_cast<Gtk::Button*>(child)) {
                            if (button->get_receives_default()) {
                                button->grab_focus();
                                
                                auto accessible = button->get_accessible();
                                if (accessible) {
                                    accessible->set_property("accessible-state", std::string("focused"));
                                }
                                break;
                            }
                        }
                    }
                    return false; // Allow event propagation
                });
            widget->add_controller(key_controller);

            widget->set_tooltip_text(
                gtkNative->get_title() + " (Modal Dialog)");
        }

        gtkNative->show();
        loop->run();

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

    gtkApp->property_application_id() = "org.solvespace.SolveSpace";
    gtkApp->set_property("accessible-role", std::string("application"));
    gtkApp->set_property("accessible-name", std::string("SolveSpace"));
    gtkApp->set_property("accessible-description", std::string("Parametric 2D/3D CAD tool"));

    gtkApp->set_resource_base_path("/org/solvespace/SolveSpace");

    auto shortcut_controller = Gtk::ShortcutController::create();
    shortcut_controller->set_scope(Gtk::ShortcutScope::GLOBAL);

    auto escape_action = Gtk::NamedAction::create("app.escape");
    auto escape_trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Escape, Gdk::ModifierType());
    auto escape_shortcut = Gtk::Shortcut::create(escape_trigger, escape_action);
    shortcut_controller->add_shortcut(escape_shortcut);

    auto save_action = Gtk::NamedAction::create("app.save");
    auto save_trigger = Gtk::KeyvalTrigger::create(GDK_KEY_s, Gdk::ModifierType::CONTROL_MASK);
    auto save_shortcut = Gtk::Shortcut::create(save_trigger, save_action);
    shortcut_controller->add_shortcut(save_shortcut);

    auto open_action = Gtk::NamedAction::create("app.open");
    auto open_trigger = Gtk::KeyvalTrigger::create(GDK_KEY_o, Gdk::ModifierType::CONTROL_MASK);
    auto open_shortcut = Gtk::Shortcut::create(open_trigger, open_action);
    shortcut_controller->add_shortcut(open_shortcut);


    auto style_provider = Gtk::CssProvider::create();
    style_provider->load_from_data(R"(
        /* Application-wide styles with improved accessibility */
        .solvespace-app {
            background-color: #f8f8f8;
            color: #333333;
            font-family: 'Cantarell', sans-serif;
        }
        
        /* Improved header bar styling */
        headerbar {
            background-color: #e0e0e0;
            border-bottom: 1px solid #d0d0d0;
            padding: 6px;
            min-height: 46px;
        }
        
        /* Button styling with focus indicators for accessibility */
        button {
            padding: 6px 10px;
            border-radius: 4px;
            transition: background-color 200ms ease;
        }
        
        button:hover {
            background-color: alpha(#000000, 0.05);
        }
        
        button:focus {
            outline: 2px solid #3584e4;
            outline-offset: -1px;
        }
        
        /* Menu styling with improved contrast */
        menubutton {
            padding: 4px;
        }
        
        menubutton:hover {
            background-color: alpha(#000000, 0.05);
        }
        
        menubutton > button {
            padding: 4px 8px;
        }
        
        /* GL area styling */
        .solvespace-gl-area {
            background-color: #ffffff;
            border: 1px solid #d0d0d0;
        }
        
        /* Editor overlay styling with improved accessibility */
        .editor-overlay {
            background-color: transparent;
        }
        
        .editor-text {
            background-color: white;
            color: black;
            border-radius: 3px;
            padding: 4px;
            margin: 2px;
            caret-color: #0066cc;
            selection-background-color: rgba(0, 102, 204, 0.3);
            selection-color: black;
        }
        
        /* High contrast mode support */
        @define-color accent_bg_color #3584e4;
        @define-color accent_fg_color #ffffff;
        
        /* Dialog styling with improved accessibility */
        dialog {
            background-color: #f8f8f8;
            border-radius: 6px;
            border: 1px solid #d0d0d0;
            padding: 12px;
        }
        
        dialog .dialog-title {
            font-weight: bold;
            font-size: 1.2em;
            margin-bottom: 12px;
        }
        
        dialog .dialog-content {
            margin: 8px 0;
        }
        
        dialog .dialog-buttons {
            margin-top: 12px;
        }
        
        /* High contrast mode support */
        @media (prefers-contrast: high) {
            .editor-text {
                background-color: white;
                color: black;
                border: 2px solid black;
            }
            
            button:focus {
                outline: 3px solid black;
                outline-offset: -2px;
            }
            
            dialog {
                border: 2px solid black;
                background-color: white;
            }
        }
        
        /* Menu button styling */
        .menu-button {
            padding: 4px 8px;
            margin: 2px;
            border-radius: 4px;
            transition: background-color 200ms ease;
        }
        
        .menu-button:hover {
            background-color: rgba(0, 0, 0, 0.05);
        }
        
        .menu-button:focus {
            outline: 2px solid rgba(61, 174, 233, 0.5);
        }
        
        /* Dialog styling with improved accessibility */
        dialog {
            background-color: #f8f8f8;
            border: 1px solid #d0d0d0;
            border-radius: 6px;
            padding: 12px;
        }
        
        dialog headerbar {
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
        }
        
        /* Text input styling with focus indicators */
        entry {
            background-color: #ffffff;
            color: #333333;
            border: 1px solid #d0d0d0;
            border-radius: 4px;
            padding: 6px;
            caret-color: #3584e4;
        }
        
        entry:focus {
            border-color: #3584e4;
            outline: 2px solid alpha(#3584e4, 0.3);
            outline-offset: -1px;
        }
        
        /* Scrollbar styling for better visibility */
        scrollbar {
            background-color: transparent;
            border-radius: 8px;
            min-width: 14px;
            min-height: 14px;
        }
        
        scrollbar slider {
            background-color: #b0b0b0;
            border-radius: 8px;
            min-width: 8px;
            min-height: 8px;
        }
        
        scrollbar slider:hover {
            background-color: #909090;
        }
        
        /* Editor overlay styling */
        .editor-overlay {
            background-color: transparent;
        }
        
        /* Text entry styling */
        .editor-text {
            background-color: white;
            color: black;
            border-radius: 3px;
            padding: 4px;
            margin: 2px;
            caret-color: #0066cc;
            selection-background-color: rgba(0, 102, 204, 0.3);
            selection-color: black;
        }
        
        /* Accessibility focus styling */
        *:focus {
            outline: 2px solid rgba(61, 174, 233, 0.8);
            outline-offset: 1px;
        }
        
        /* Dialog styling */
        dialog {
            background-color: #f8f8f8;
            border-radius: 6px;
            border: 1px solid #d0d0d0;
            padding: 12px;
        }
        
        /* Scrollbar styling */
        scrollbar {
            background-color: transparent;
            border-radius: 8px;
            margin: 2px;
        }
        
        scrollbar slider {
            background-color: rgba(0, 0, 0, 0.3);
            border-radius: 8px;
            min-width: 8px;
            min-height: 8px;
        }
        
        scrollbar slider:hover {
            background-color: rgba(0, 0, 0, 0.5);
        }
        
        .menu-item {
            padding: 6px 8px;
            margin: 1px;
            border-radius: 4px;
            transition: background-color 200ms ease;
        }
        
        .menu-item:hover {
            background-color: rgba(0, 0, 0, 0.05);
        }
        
        .menu-item:focus {
            outline: 2px solid rgba(0, 102, 204, 0.5);
            outline-offset: 1px;
        }
        
        .solvespace-gl-area {
            background-color: #ffffff;
        }
        
        .editor-overlay {
            background-color: transparent;
        }
        
        .editor-text {
            background-color: white;
            color: black;
            border-radius: 3px;
            padding: 2px;
            caret-color: #0066cc;
            selection-background-color: rgba(0, 102, 204, 0.3);
            selection-color: black;
        }

        /* Window styles */
        window.solvespace-window {
            background-color: #f0f0f0;
        }

        headerbar.titlebar {
            background-color: #e0e0e0;
            border-bottom: 1px solid rgba(0, 0, 0, 0.1);
            padding: 6px;
        }

        /* Menu styles */
        .menu-button {
            padding: 4px 8px;
            border-radius: 4px;
            background-color: transparent;
            transition: background-color 200ms ease;
        }

        .menu-button:hover {
            background-color: rgba(0, 0, 0, 0.05);
        }

        .menu-button:active {
            background-color: rgba(0, 0, 0, 0.1);
        }

        .menu-item {
            padding: 6px 8px;
            border-radius: 4px;
            transition: background-color 200ms ease;
        }

        .menu-item:hover {
            background-color: alpha(currentColor, 0.1);
        }

        .menu-item:active {
            background-color: alpha(currentColor, 0.15);
        }

        .check-menu-item, .radio-menu-item {
            margin-left: 4px;
        }

        /* Drawing area styles */
        .solvespace-gl-area {
            background-color: #ffffff;
            border-radius: 2px;
            border: 1px solid rgba(0, 0, 0, 0.1);
        }

        .drawing-area {
            min-width: 300px;
            min-height: 300px;
        }

        /* Text entry styles */
        .text-entry {
            font-family: monospace;
            padding: 4px;
            border-radius: 3px;
            background-color: white;
            color: #333333;
            caret-color: #0066cc;
            selection-background-color: rgba(0, 102, 204, 0.3);
            selection-color: black;
        }

        /* Dialog styles */
        .dialog {
            background-color: #f5f5f5;
            border-radius: 3px;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
        }

        .solvespace-dialog {
            padding: 12px;
        }

        .solvespace-file-dialog {
            min-width: 600px;
            min-height: 450px;
            padding: 8px;
        }

        .dialog-content {
            margin: 12px;
        }

        .dialog-button-box {
            margin-top: 12px;
            padding: 8px;
            border-top: 1px solid rgba(0, 0, 0, 0.1);
        }

        .dialog-icon {
            margin-right: 12px;
        }

        /* Button styles */
        button.suggested-action {
            background-color: #3584e4;
            color: white;
            border-radius: 4px;
            padding: 6px 12px;
            transition: background-color 200ms ease;
        }

        button.suggested-action:hover {
            background-color: #3a8cf0;
        }

        button.destructive-action {
            background-color: #e01b24;
            color: white;
            border-radius: 4px;
            padding: 6px 12px;
            transition: background-color 200ms ease;
        }

        button.destructive-action:hover {
            background-color: #f02b34;
        }
    )");

    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        style_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    auto settings = Gtk::Settings::get_for_display(Gdk::Display::get_default());

    if (settings) {
        auto theme_binding = Gtk::PropertyExpression<bool>::create(settings->property_gtk_application_prefer_dark_theme());
        theme_binding->connect(
            []() {
                SS.GenerateAll(SolveSpaceUI::Generate::ALL);
                SS.GW.Invalidate();
            });
    }

    std::vector<std::string> args;
    for (int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }

    auto shortcut_controller = Gtk::ShortcutController::create();
    shortcut_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    
    auto help_shortcut = Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(GDK_KEY_F1),
        Gtk::CallbackAction::create([](Gtk::Widget& widget, const Glib::VariantBase& args) {
            SS.ShowHelp();
            return true;
        })
    );
    shortcut_controller->add_shortcut(help_shortcut);
    

    style_provider->load_from_data(R"(
    /* Base window styling */
    window {
        background-color: #f5f5f5;
    }
    
    headerbar {
        background-color: #e0e0e0;
        border-bottom: 1px solid #d0d0d0;
        padding: 4px;
    }
    
    /* Menu styling */
    .menu-button {
        padding: 4px 8px;
        margin: 2px;
        border-radius: 4px;
    }
    
    .menu-button:hover {
        background-color: rgba(0, 0, 0, 0.1);
    }
    
    .menu-item {
        padding: 6px 8px;
        margin: 1px;
    }
    
    .menu-item:hover {
        background-color: rgba(0, 0, 0, 0.1);
    }
    
    /* GL area styling */
    .solvespace-gl-area {
        background-color: #ffffff;
    }
    
    /* Editor overlay styling */
    .editor-overlay {
        background-color: transparent;
    }
    
    /* Base entry styling */
    entry {
        background: white;
        color: black;
        border-radius: 4px;
        padding: 2px;
        min-height: 24px;
        caret-color: #0066cc;
        selection-background-color: rgba(0, 102, 204, 0.3);
        selection-color: black;
    }
    
    entry:focus {
        border-color: #0066cc;
        box-shadow: 0 0 0 1px rgba(0, 102, 204, 0.5);
    }

    /* Custom editor entry styling */
    .solvespace-editor-entry {
        background: transparent;
        padding: 0;
        margin: 0;
        caret-color: #0066cc;
    }

    /* Button styling */
    button.flat {
        padding: 6px;
        margin: 1px;
        border-radius: 4px;
    }

    /* Dialog styling */
    .dialog-icon {
        min-width: 32px;
        min-height: 32px;
        margin: 8px;
    }

    /* Message dialog styling */
    .message-dialog-content {
        margin: 12px;
        padding: 8px;
    }

    /* Header bar styling */
    headerbar {
        padding: 4px;
        min-height: 38px;
    }

    /* Scrollbar styling */
    scrollbar {
        background-color: transparent;
    }

    scrollbar slider {
        background-color: rgba(128, 128, 128, 0.7);
        border-radius: 6px;
        min-width: 8px;
        min-height: 8px;
    }

    scrollbar slider:hover {
        background-color: rgba(128, 128, 128, 0.9);
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
        const char* ci = getenv("CI");
        if (ci && (strcmp(ci, "true") == 0)) {
            setenv("GTK_A11Y", "none", 0);
        } else {
            const char* a11y_bus = getenv("AT_SPI_BUS_ADDRESS");
            if (!a11y_bus) {
                setenv("GTK_A11Y", "none", 0);
            }
        }
    } else {
        unsetenv("GTK_A11Y");
    }

    if (!gtkApp->is_registered()) {
        gtkApp->register_application();

        gtkApp->hold();

        auto settings = Gtk::Settings::get_for_display(Gdk::Display::get_default());
        if (settings) {
            auto theme_binding = Gtk::PropertyExpression<bool>::create(
                Gtk::Settings::get_type(), settings.get(), "gtk-application-prefer-dark-theme");
            theme_binding->connect([](bool dark_theme) {
                SS.GenerateAll(SolveSpaceUI::Generate::ALL);
                SS.GW.Invalidate();
            });
        }

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
