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
#include <gtkmm/eventcontrollerfocus.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/gesturerotate.h>
#include <gtkmm/gesturezoom.h>
#include <gtkmm/dragsource.h>
#include <gtkmm/droptarget.h>
#include <gtkmm/colordialog.h>
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
                    set_can_focus(false);
                    _receiver->onTrigger();
                    set_can_focus(true);
                }
                return true;
            });
        add_controller(_click_controller);

        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set("Menu Item");
        update_property(Gtk::Accessible::Property::LABEL, label_value);
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

        auto escape_controller = Gtk::ShortcutController::create();
        escape_controller->set_scope(Gtk::ShortcutScope::LOCAL);

        auto escape_trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Escape);
        auto escape_action = Gtk::CallbackAction::create(
            [this, &loop](Gtk::Widget&, const Glib::VariantBase&) -> bool {
                gtkMenu.set_visible(false);
                loop->quit();
                return true;
            });

        auto escape_shortcut = Gtk::Shortcut::create(escape_trigger, escape_action);
        escape_controller->add_shortcut(escape_shortcut);

        gtkMenu.add_controller(escape_controller);

        auto motion_controller = Gtk::EventControllerMotion::create();
        motion_controller->signal_leave().connect(
            [&loop]() {
                loop->quit();
            });
        gtkMenu.add_controller(motion_controller);

        gtkMenu.property_visible().signal_changed().connect(
            [&loop, this]() {
                if (!gtkMenu.get_visible()) {
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

        button->set_tooltip_text(label + " " + C_("tooltip", "Menu"));

        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set(label + " " + C_("accessibility", "Menu"));
        button->update_property(Gtk::Accessible::Property::LABEL, label_value);

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
    Glib::RefPtr<Gtk::DropTarget> _drop_target;
    Glib::RefPtr<Gtk::DragSource> _drag_source;
    std::vector<std::string> _accepted_mime_types;
    std::vector<std::string> _export_mime_types;
    Glib::RefPtr<Gtk::GestureZoom> _zoom_gesture;
    Glib::RefPtr<Gtk::GestureRotate> _rotate_gesture;

public:
    GtkGLWidget(Platform::Window *receiver) : _receiver(receiver) {
        set_has_depth_buffer(true);
        set_can_focus(true);

        add_css_class("solvespace-gl-area");
        add_css_class("drawing-area");

        set_tooltip_text(C_("tooltip", "SolveSpace Drawing Area - 3D modeling canvas"));

        Glib::Value<Glib::ustring> canvas_desc;
        canvas_desc.init(Glib::Value<Glib::ustring>::value_type());
        canvas_desc.set("Canvas element");
        update_property(Gtk::Accessible::Property::DESCRIPTION, canvas_desc);

        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set(C_("accessibility", "SolveSpace Drawing Area"));
        update_property(Gtk::Accessible::Property::LABEL, label_value);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("accessibility", "3D modeling canvas for creating and editing models"));
        update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);

        setup_event_controllers();
        setup_drop_target();
        setup_drag_source();
        setup_touch_gestures();
    }

    void announce_operation_mode(const std::string& mode) {
        Glib::Value<Glib::ustring> mode_label;
        mode_label.init(Glib::Value<Glib::ustring>::value_type());
        mode_label.set(Glib::ustring::compose(C_("accessibility", "SolveSpace Drawing Area - %1 Mode"), mode));
        update_property(Gtk::Accessible::Property::LABEL, mode_label);

        auto live_region = Gtk::Accessible::LiveRegion::POLITE;
        Glib::Value<Gtk::Accessible::LiveRegion> live_value;
        live_value.init(Glib::Value<Gtk::Accessible::LiveRegion>::value_type());
        live_value.set(live_region);
        update_property(Gtk::Accessible::Property::LIVE_REGION, live_value);
        
        Glib::Value<Glib::ustring> mode_desc;
        mode_desc.init(Glib::Value<Glib::ustring>::value_type());
        mode_desc.set(Glib::ustring::compose(C_("accessibility", "Operation mode changed to: %1"), mode));
        update_property(Gtk::Accessible::Property::DESCRIPTION, mode_desc);

        set_can_focus(false);
        set_can_focus(true);
        grab_focus();
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
        motion_controller->set_name("gl-widget-motion-controller");

        motion_controller->signal_motion().connect(
            [this, motion_controller](double x, double y) {
                auto state = motion_controller->get_current_event_state();

                Glib::Value<Glib::ustring> coord_value;
                coord_value.init(Glib::Value<Glib::ustring>::value_type());
                coord_value.set(Glib::ustring::compose(C_("accessibility", "Pointer at coordinates: %1, %2"),
                                          static_cast<int>(x), static_cast<int>(y)));
                update_property(Gtk::Accessible::Property::DESCRIPTION, coord_value);

                process_pointer_event(MouseEvent::Type::MOTION,
                                     x, y,
                                     static_cast<GdkModifierType>(state));
                return true;
            });

        motion_controller->signal_enter().connect(
            [this](double x, double y) {
                Glib::Value<Glib::ustring> focus_value;
                focus_value.init(Glib::Value<Glib::ustring>::value_type());
                focus_value.set("Element has focus");
                update_property(Gtk::Accessible::Property::DESCRIPTION, focus_value);
                process_pointer_event(MouseEvent::Type::MOTION, x, y, GdkModifierType(0));
                return true;
            });

        motion_controller->signal_leave().connect(
            [this]() {
                Glib::Value<Glib::ustring> lost_focus_value;
                lost_focus_value.init(Glib::Value<Glib::ustring>::value_type());
                lost_focus_value.set("Element lost focus");
                update_property(Gtk::Accessible::Property::DESCRIPTION, lost_focus_value);
                double x, y;
                get_pointer_position(x, y);
                process_pointer_event(MouseEvent::Type::LEAVE, x, y, GdkModifierType(0));
                return true;
            });

        add_controller(motion_controller);

        auto gesture_click = Gtk::GestureClick::create();
        gesture_click->set_name("gl-widget-click-controller");
        gesture_click->set_button(0); // Listen for any button
        gesture_click->signal_pressed().connect(
            [this, gesture_click](int n_press, double x, double y) {
                auto state = gesture_click->get_current_event_state();
                guint button = gesture_click->get_current_button();

                Glib::Value<Glib::ustring> active_desc;
                active_desc.init(Glib::Value<Glib::ustring>::value_type());
                active_desc.set("Element is active");
                update_property(Gtk::Accessible::Property::DESCRIPTION, active_desc);

                Glib::Value<Glib::ustring> value;
                value.init(Glib::Value<Glib::ustring>::value_type());
                value.set(Glib::ustring::compose(C_("accessibility", "Mouse button %1 clicked at %2, %3"),
                                               button, static_cast<int>(x), static_cast<int>(y)));
                update_property(Gtk::Accessible::Property::DESCRIPTION, value);

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

                Glib::Value<Glib::ustring> released_desc;
                released_desc.init(Glib::Value<Glib::ustring>::value_type());
                released_desc.set("Element released");
                update_property(Gtk::Accessible::Property::DESCRIPTION, released_desc);

                process_pointer_event(MouseEvent::Type::RELEASE, x, y, static_cast<GdkModifierType>(state), button);
                return true;
            });
        add_controller(gesture_click);

        auto scroll_controller = Gtk::EventControllerScroll::create();
        scroll_controller->set_name("gl-widget-scroll-controller");
        scroll_controller->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
        scroll_controller->signal_scroll().connect(
            [this, scroll_controller](double dx, double dy) {
                double x, y;
                get_pointer_position(x, y);
                auto state = scroll_controller->get_current_event_state();

                Glib::Value<Glib::ustring> value;
                value.init(Glib::Value<Glib::ustring>::value_type());
                value.set(Glib::ustring::compose(C_("accessibility", "Scrolling %1 units vertically"),
                                       static_cast<int>(dy)));
                update_property(Gtk::Accessible::Property::DESCRIPTION, value);

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
                bool handled = process_key_event(KeyboardEvent::Type::PRESS, keyval, gdk_state);

                if (handled) {
                    if (keyval == GDK_KEY_Delete) {
                        Glib::Value<Glib::ustring> label_value;
                        label_value.init(Glib::Value<Glib::ustring>::value_type());
                        label_value.set(C_("accessibility", "SolveSpace 3D View - Delete Mode"));
                        update_property(Gtk::Accessible::Property::LABEL, label_value);
                        Glib::Value<Glib::ustring> busy_value;
                        busy_value.init(Glib::Value<Glib::ustring>::value_type());
                        busy_value.set("Processing operation");
                        update_property(Gtk::Accessible::Property::DESCRIPTION, busy_value);
                    } else if (keyval == GDK_KEY_Escape) {
                        Glib::Value<Glib::ustring> view_label;
                        view_label.init(Glib::Value<Glib::ustring>::value_type());
                        view_label.set(C_("accessibility", "SolveSpace 3D View"));
                        update_property(Gtk::Accessible::Property::LABEL, view_label);
                        Glib::Value<Glib::ustring> not_busy_value;
                        not_busy_value.init(Glib::Value<Glib::ustring>::value_type());
                        not_busy_value.set("Operation completed");
                        update_property(Gtk::Accessible::Property::DESCRIPTION, not_busy_value);
                    } else if (keyval == GDK_KEY_l || keyval == GDK_KEY_L) {
                        Glib::Value<Glib::ustring> line_label;
                        line_label.init(Glib::Value<Glib::ustring>::value_type());
                        line_label.set(C_("accessibility", "SolveSpace 3D View - Line Tool"));
                        update_property(Gtk::Accessible::Property::LABEL, line_label);
                    } else if (keyval == GDK_KEY_c || keyval == GDK_KEY_C) {
                        Glib::Value<Glib::ustring> circle_label;
                        circle_label.init(Glib::Value<Glib::ustring>::value_type());
                        circle_label.set(C_("accessibility", "SolveSpace 3D View - Circle Tool"));
                        update_property(Gtk::Accessible::Property::LABEL, circle_label);
                    } else if (keyval == GDK_KEY_a || keyval == GDK_KEY_A) {
                        Glib::Value<Glib::ustring> arc_label;
                        arc_label.init(Glib::Value<Glib::ustring>::value_type());
                        arc_label.set(C_("accessibility", "SolveSpace 3D View - Arc Tool"));
                        update_property(Gtk::Accessible::Property::LABEL, arc_label);
                    } else if (keyval == GDK_KEY_r || keyval == GDK_KEY_R) {
                        Glib::Value<Glib::ustring> rect_label;
                        rect_label.init(Glib::Value<Glib::ustring>::value_type());
                        rect_label.set(C_("accessibility", "SolveSpace 3D View - Rectangle Tool"));
                        update_property(Gtk::Accessible::Property::LABEL, rect_label);
                    } else if (keyval == GDK_KEY_d || keyval == GDK_KEY_D) {
                        Glib::Value<Glib::ustring> dim_label;
                        dim_label.init(Glib::Value<Glib::ustring>::value_type());
                        dim_label.set(C_("accessibility", "SolveSpace 3D View - Dimension Tool"));
                        update_property(Gtk::Accessible::Property::LABEL, dim_label);
                    }
                }

                return handled;
            }, false);

        key_controller->signal_key_released().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                GdkModifierType gdk_state = static_cast<GdkModifierType>(state);
                return process_key_event(KeyboardEvent::Type::RELEASE, keyval, gdk_state);
            }, false);

        add_controller(key_controller);
        add_controller(shortcut_controller);

        add_css_class("solvespace-gl-widget");
        Glib::Value<Glib::ustring> canvas_desc;
        canvas_desc.init(Glib::Value<Glib::ustring>::value_type());
        canvas_desc.set("Canvas element");
        update_property(Gtk::Accessible::Property::DESCRIPTION, canvas_desc);
        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set(C_("accessibility", "SolveSpace 3D View"));
        update_property(Gtk::Accessible::Property::LABEL, label_value);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("accessibility", "Interactive 3D modeling canvas for creating and editing models"));
        update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
        set_can_focus(true);

        auto focus_controller = Gtk::EventControllerFocus::create();
        add_controller(focus_controller);
        focus_controller->signal_enter().connect(
            [this]() {
                grab_focus();
                Glib::Value<Glib::ustring> focus_value;
                focus_value.init(Glib::Value<Glib::ustring>::value_type());
                focus_value.set("Element has focus");
                update_property(Gtk::Accessible::Property::DESCRIPTION, focus_value);
            });
        focus_controller->signal_leave().connect(
            [this]() {
                Glib::Value<Glib::ustring> lost_focus_value;
                lost_focus_value.init(Glib::Value<Glib::ustring>::value_type());
                lost_focus_value.set("Element lost focus");
                update_property(Gtk::Accessible::Property::DESCRIPTION, lost_focus_value);
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

    void setup_drop_target() {
        _accepted_mime_types = {
            "text/uri-list",                    // Standard URI list (most common)
            "application/x-solvespace",         // SolveSpace files (.slvs)
            "application/dxf",                  // DXF files
            "application/acad",                 // AutoCAD files (DWG)
            "application/vnd.ms-pki.stl",       // STL files
            "application/octet-stream",         // Generic binary data
            "model/stl",                        // STL files (alternate MIME)
            "application/step",                 // STEP files
            "application/iges",                 // IGES files
            "application/idf",                  // IDF/EMN files
            "image/svg+xml",                    // SVG files
            "application/postscript",           // EPS/PS files
            "application/pdf",                  // PDF files
            "text/plain",                       // Text files (including G-code)
            "application/hpgl"                  // HPGL/PLT files
        };

        _drop_target = Gtk::DropTarget::create(G_TYPE_STRING, Gdk::DragAction::COPY);
        _drop_target->set_gtypes(_accepted_mime_types);

        _drop_target->signal_drop().connect(
            [this](const Glib::ValueBase& value, double x, double y) -> bool {
                auto mime_type = _drop_target->get_current_drop()->get_formats().get_mime_types()[0];

                Glib::Value<Glib::ustring> drop_desc;
                drop_desc.init(Glib::Value<Glib::ustring>::value_type());
                drop_desc.set(Glib::ustring::compose(C_("accessibility", "File dropped at %1, %2"),
                                                   static_cast<int>(x), static_cast<int>(y)));
                update_property(Gtk::Accessible::Property::DESCRIPTION, drop_desc);

                if (mime_type == "text/uri-list") {
                    Glib::ustring uri_list = Glib::Value<Glib::ustring>(value).get();
                    std::vector<Glib::ustring> uris = Glib::Regex::split_simple("\\s+", uri_list);

                    for (const auto& uri : uris) {
                        if (uri.empty() || uri[0] == '#') continue; // Skip empty lines and comments

                        Glib::ustring file_path;
                        try {
                            file_path = Glib::filename_from_uri(uri);
                        } catch (const Glib::Error& e) {
                            continue; // Skip invalid URIs
                        }

                        if (_receiver->onFileDrop) {
                            _receiver->onFileDrop(file_path.c_str());
                            return true;
                        }
                    }
                }

                return false;
            });

        add_controller(_drop_target);
    }

    void setup_drag_source() {
        _export_mime_types = {
            "application/x-solvespace",         // SolveSpace files (.slvs)
            "model/stl",                        // STL files
            "application/step",                 // STEP files
            "application/iges",                 // IGES files
            "image/svg+xml",                    // SVG files
            "application/pdf",                  // PDF files
            "text/uri-list"                     // URI list for file references
        };

        _drag_source = Gtk::DragSource::create();
        _drag_source->set_actions(Gdk::DragAction::COPY);

        _drag_source->set_touch_only(false);  // Allow both mouse and touch

        static bool test_drag_touch = false;
        for (int i = 1; i < Glib::get_argc(); i++) {
            if (Glib::get_argv_utf8()[i] == "--test-gtk4-drag-touch") {
                test_drag_touch = true;
                break;
            }
        }

        if (test_drag_touch) {
            Glib::signal_timeout().connect_once(
                [this]() {
                    double x = 100, y = 100;

                    Glib::Value<Glib::ustring> test_value;
                    test_value.init(Glib::Value<Glib::ustring>::value_type());
                    test_value.set(C_("accessibility", "Testing drag source with touch input"));
                    update_property(Gtk::Accessible::Property::DESCRIPTION, test_value);

                    _drag_source->set_touch_only(true);  // Force touch mode for test
                    _drag_source->drag_begin(x, y);

                    Glib::signal_timeout().connect_once(
                        []() {
                            exit(0);
                        },
                        2000);  // Exit after 2 seconds
                },
                1000);  // Start test after 1 second
        }

        _drag_source->signal_prepare().connect(
            [this](double x, double y) -> Glib::RefPtr<Gdk::ContentProvider> {
                Glib::Value<Glib::ustring> drag_desc;
                drag_desc.init(Glib::Value<Glib::ustring>::value_type());
                drag_desc.set(C_("accessibility", "Started dragging model for export"));
                update_property(Gtk::Accessible::Property::DESCRIPTION, drag_desc);

                Glib::Value<Glib::ustring> announce_value;
                announce_value.init(Glib::Value<Glib::ustring>::value_type());
                announce_value.set(C_("accessibility", "Dragging model. Release to export."));
                update_property(Gtk::Accessible::Property::DESCRIPTION, announce_value);

                if (!_receiver->onDragExport) {
                    return Glib::RefPtr<Gdk::ContentProvider>();
                }

                std::string temp_file_path = _receiver->onDragExport();
                if (temp_file_path.empty()) {
                    return Glib::RefPtr<Gdk::ContentProvider>();
                }

                Glib::ustring uri = Glib::filename_to_uri(temp_file_path);

                Glib::Value<Glib::ustring> value;
                value.init(Glib::Value<Glib::ustring>::value_type());
                value.set(uri);

                return Gdk::ContentProvider::create_for_value(value);
            });

        _drag_source->signal_drag_begin().connect(
            [this](const Glib::RefPtr<Gdk::Drag>& drag) {
                auto surface = get_native()->get_surface();
                if (surface) {
                    auto snapshot = Gtk::Snapshot::create();
                    snapshot->append_color(Gdk::RGBA("rgba(0,120,215,0.5)"),
                                          Graphene::Rect::create(0, 0, 64, 64));
                    auto paintable = snapshot->to_paintable(nullptr, Graphene::Size::create(64, 64));
                    if (paintable) {
                        drag->set_icon(paintable, 32, 32);
                    }
                }

                Glib::Value<Glib::ustring> state_value;
                state_value.init(Glib::Value<Glib::ustring>::value_type());
                state_value.set("Element is being dragged");
                update_property(Gtk::Accessible::Property::DESCRIPTION, state_value);
            });

        _drag_source->signal_drag_end().connect(
            [this](const Glib::RefPtr<Gdk::Drag>& drag, bool) {
                Glib::Value<Glib::ustring> end_desc;
                end_desc.init(Glib::Value<Glib::ustring>::value_type());
                end_desc.set(C_("accessibility", "Finished dragging model"));
                update_property(Gtk::Accessible::Property::DESCRIPTION, end_desc);

                Glib::Value<Glib::ustring> announce_value;
                announce_value.init(Glib::Value<Glib::ustring>::value_type());
                announce_value.set(C_("accessibility", "Model export " +
                                    (drag->get_selected_action() == Gdk::DragAction::COPY ?
                                     "completed" : "cancelled")));
                update_property(Gtk::Accessible::Property::DESCRIPTION, announce_value);

                if (_receiver->onDragExportCleanup) {
                    _receiver->onDragExportCleanup();
                }
            });

        add_controller(_drag_source);
    }

    void setup_touch_gestures() {
        _zoom_gesture = Gtk::GestureZoom::create();
        _zoom_gesture->set_name("gl-widget-zoom-gesture");
        _zoom_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);

        _zoom_gesture->signal_begin().connect(
            [this](Gdk::EventSequence* sequence) {
                Glib::Value<Glib::ustring> active_desc;
                active_desc.init(Glib::Value<Glib::ustring>::value_type());
                active_desc.set(C_("accessibility", "Zoom gesture started"));
                update_property(Gtk::Accessible::Property::DESCRIPTION, active_desc);
            });

        _zoom_gesture->signal_scale_changed().connect(
            [this](double scale) {
                double scroll_delta = (scale > 1.0) ? -1.0 : 1.0;
                double x, y;
                get_pointer_position(x, y);

                Glib::Value<Glib::ustring> zoom_desc;
                zoom_desc.init(Glib::Value<Glib::ustring>::value_type());
                zoom_desc.set(Glib::ustring::compose(C_("accessibility", "Zooming with scale factor %1"),
                                                   static_cast<int>(scale * 100) / 100.0));
                update_property(Gtk::Accessible::Property::DESCRIPTION, zoom_desc);

                process_pointer_event(MouseEvent::Type::SCROLL_VERT, x, y,
                                    GdkModifierType(0), 0, scroll_delta);

                if (_receiver->onTouchGesture) {
                    TouchGestureEvent event = {};
                    event.type = TouchGestureEvent::Type::ZOOM;
                    event.x = x;
                    event.y = y;
                    event.scale = scale;
                    _receiver->onTouchGesture(event);
                }
            });

        _zoom_gesture->signal_end().connect(
            [this](Gdk::EventSequence* sequence) {
                Glib::Value<Glib::ustring> end_desc;
                end_desc.init(Glib::Value<Glib::ustring>::value_type());
                end_desc.set(C_("accessibility", "Zoom gesture ended"));
                update_property(Gtk::Accessible::Property::DESCRIPTION, end_desc);
            });

        add_controller(_zoom_gesture);

        _rotate_gesture = Gtk::GestureRotate::create();
        _rotate_gesture->set_name("gl-widget-rotate-gesture");
        _rotate_gesture->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);

        _rotate_gesture->signal_begin().connect(
            [this](Gdk::EventSequence* sequence) {
                Glib::Value<Glib::ustring> active_desc;
                active_desc.init(Glib::Value<Glib::ustring>::value_type());
                active_desc.set(C_("accessibility", "Rotation gesture started"));
                update_property(Gtk::Accessible::Property::DESCRIPTION, active_desc);
            });

        _rotate_gesture->signal_angle_changed().connect(
            [this](double angle, double angle_delta) {
                double x, y;
                get_pointer_position(x, y);

                Glib::Value<Glib::ustring> rotate_desc;
                rotate_desc.init(Glib::Value<Glib::ustring>::value_type());
                rotate_desc.set(Glib::ustring::compose(C_("accessibility", "Rotating by %1 degrees"),
                                                     static_cast<int>(angle_delta * 180 / M_PI)));
                update_property(Gtk::Accessible::Property::DESCRIPTION, rotate_desc);

                if (_receiver->onTouchGesture) {
                    TouchGestureEvent event = {};
                    event.type = TouchGestureEvent::Type::ROTATE;
                    event.x = x;
                    event.y = y;
                    event.rotation = angle_delta;
                    _receiver->onTouchGesture(event);
                }
            });

        _rotate_gesture->signal_end().connect(
            [this](Gdk::EventSequence* sequence) {
                Glib::Value<Glib::ustring> end_desc;
                end_desc.init(Glib::Value<Glib::ustring>::value_type());
                end_desc.set(C_("accessibility", "Rotation gesture ended"));
                update_property(Gtk::Accessible::Property::DESCRIPTION, end_desc);
            });

        add_controller(_rotate_gesture);
    }
    
    void AnnounceOperationMode(const std::string& mode) {
        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set("SolveSpace 3D View - " + mode);
        update_property(Gtk::Accessible::Property::LABEL, label_value);
        
        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("accessibility", "Current operation mode: ") + mode);
        update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
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
        
        try {
            auto file = Gio::File::create_for_path(Platform::PathFromResource("platform/css/editor_overlay.css"));
            css_provider->load_from_file(file);
        } catch (const Glib::Error& e) {
            static const char* editor_css = 
                "grid.editor-overlay { "
                "    background-color: transparent; "
                "}"
                "entry.editor-text { "
                "    background-color: white; "
                "    color: black; "
                "    border-radius: 3px; "
                "    padding: 2px; "
                "    caret-color: #0066cc; "
                "    selection-background-color: rgba(0, 102, 204, 0.3); "
                "    selection-color: black; "
                "}";
            css_provider->load_from_data(editor_css);
        }

        set_name("editor-overlay");
        add_css_class("editor-overlay");
        set_row_spacing(4);
        set_column_spacing(4);
        set_row_homogeneous(false);
        
        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set(C_("accessibility", "SolveSpace Text Editor"));
        update_property(Gtk::Accessible::Property::LABEL, label_value);
        
        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("accessibility", "Text input overlay for editing values"));
        update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);

        set_layout_manager(_constraint_layout);
        set_column_homogeneous(false);

        set_tooltip_text(C_("tooltip", "SolveSpace editor overlay with drawing area and text input"));

        set_property("accessible-role", Gtk::Accessible::Role::GROUP);
        Glib::Value<Glib::ustring> editor_label;
        editor_label.init(Glib::Value<Glib::ustring>::value_type());
        editor_label.set(C_("accessibility", "SolveSpace Editor"));
        update_property(Gtk::Accessible::Property::LABEL, editor_label);
        Glib::Value<Glib::ustring> editor_desc;
        editor_desc.init(Glib::Value<Glib::ustring>::value_type());
        editor_desc.set(C_("accessibility", "Drawing area with text input for SolveSpace parametric CAD"));
        update_property(Gtk::Accessible::Property::DESCRIPTION, editor_desc);

        Glib::Value<bool> has_popup;
        has_popup.init(Glib::Value<bool>::value_type());
        has_popup.set(false);
        update_property(Gtk::Accessible::Property::HAS_POPUP, has_popup);

        Glib::Value<Glib::ustring> key_shortcuts;
        key_shortcuts.init(Glib::Value<Glib::ustring>::value_type());
        key_shortcuts.set(C_("accessibility", "Escape: Cancel, Enter: Confirm"));
        update_property(Gtk::Accessible::Property::KEY_SHORTCUTS, key_shortcuts);

        setup_event_controllers();

        auto gl_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_gl_widget));
        auto this_source = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            gl_target, Gtk::Constraint::Attribute::TOP,
            Gtk::Constraint::Relation::EQ,
            this_source, Gtk::Constraint::Attribute::TOP,
            1.0, 0.0, 800));

        Glib::RefPtr<Gtk::ConstraintTarget> gl_left_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_gl_widget));
        Glib::RefPtr<Gtk::ConstraintTarget> this_left_source = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            gl_left_target, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            this_left_source, Gtk::Constraint::Attribute::LEFT,
            1.0, 0.0, 800));

        Glib::RefPtr<Gtk::ConstraintTarget> gl_right_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_gl_widget));
        Glib::RefPtr<Gtk::ConstraintTarget> this_right_source = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            gl_right_target, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            this_right_source, Gtk::Constraint::Attribute::RIGHT,
            1.0, 0.0, 800));

        Glib::RefPtr<Gtk::ConstraintTarget> gl_bottom_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_gl_widget));
        Glib::RefPtr<Gtk::ConstraintTarget> this_bottom_source = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            gl_bottom_target, Gtk::Constraint::Attribute::BOTTOM,
            Gtk::Constraint::Relation::EQ,
            this_bottom_source, Gtk::Constraint::Attribute::BOTTOM,
            1.0, -30, 800)); // Leave space for text entry

        auto entry_bottom_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_entry));
        auto this_entry_bottom_source = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            entry_bottom_target, Gtk::Constraint::Attribute::BOTTOM,
            Gtk::Constraint::Relation::EQ,
            this_entry_bottom_source, Gtk::Constraint::Attribute::BOTTOM,
            1.0, 0.0, 800));

        auto entry_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_entry));
        auto this_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));

        _constraint_layout->add_constraint(Gtk::Constraint::create(
            entry_target, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            this_target, Gtk::Constraint::Attribute::LEFT,
            1.0, 10, 1000)); // Left margin

        _constraint_layout->add_constraint(Gtk::Constraint::create(
            entry_target, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            this_target, Gtk::Constraint::Attribute::RIGHT,
            1.0, -10, 1000)); // Right margin

        _constraint_layout->add_constraint(Gtk::Constraint::create(
            entry_target, Gtk::Constraint::Attribute::HEIGHT,
            Gtk::Constraint::Relation::EQ,
            Glib::RefPtr<Gtk::ConstraintTarget>(), Gtk::Constraint::Attribute::NONE,
            0.0, 24, 1000)); // Fixed height

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

        _entry.property_visible().signal_changed().connect([this]() {
            if (_entry.get_visible()) {
                _entry.grab_focus();
                Glib::Value<Glib::ustring> focus_desc;
                focus_desc.init(Glib::Value<Glib::ustring>::value_type());
                focus_desc.set("Element has focus");
                _entry.update_property(Gtk::Accessible::Property::DESCRIPTION, focus_desc);
            } else {
                _gl_widget.grab_focus();
            }
        });

        _entry.set_tooltip_text(C_("tooltip", "Text Input"));

        _entry.set_property("accessible-role", Gtk::Accessible::Role::TEXT_BOX);
        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set(C_("accessibility", "SolveSpace Text Input"));
        _entry.update_property(Gtk::Accessible::Property::LABEL, label_value);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("accessibility", "Text input field for entering commands and values"));
        _entry.update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);

        Glib::Value<bool> multiline;
        multiline.init(Glib::Value<bool>::value_type());
        multiline.set(false);
        _entry.update_property(Gtk::Accessible::Property::MULTILINE, multiline);

        Glib::Value<bool> required;
        required.init(Glib::Value<bool>::value_type());
        required.set(false);
        _entry.update_property(Gtk::Accessible::Property::REQUIRED, required);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("accessibility", "Text entry for editing SolveSpace parameters and values"));
        _entry.update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);

        attach(_gl_widget, 0, 0);
        attach(_entry, 0, 1);

        set_layout_manager(_constraint_layout);

        Glib::RefPtr<Gtk::ConstraintGuide> guide = Gtk::ConstraintGuide::create();
        guide->set_min_size(100, 100);
        _constraint_layout->add_guide(guide);

        Glib::RefPtr<Gtk::ConstraintTarget> gl_left_target2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_gl_widget));
        Glib::RefPtr<Gtk::ConstraintTarget> this_left_source2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            gl_left_target2, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            this_left_source2, Gtk::Constraint::Attribute::LEFT,
            1.0, 0.0, 800));

        Glib::RefPtr<Gtk::ConstraintTarget> gl_right_target2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_gl_widget));
        Glib::RefPtr<Gtk::ConstraintTarget> this_right_source2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            gl_right_target2, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            this_right_source2, Gtk::Constraint::Attribute::RIGHT,
            1.0, 0.0, 800));

        Glib::RefPtr<Gtk::ConstraintTarget> gl_target2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_gl_widget));
        Glib::RefPtr<Gtk::ConstraintTarget> this_source2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            gl_target2, Gtk::Constraint::Attribute::TOP,
            Gtk::Constraint::Relation::EQ,
            this_source2, Gtk::Constraint::Attribute::TOP,
            1.0, 0.0, 800));

        Glib::RefPtr<Gtk::ConstraintTarget> entry_left_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_entry));
        Glib::RefPtr<Gtk::ConstraintTarget> this_entry_left_source = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            entry_left_target, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            this_entry_left_source, Gtk::Constraint::Attribute::LEFT,
            1.0, 0.0, 800));

        auto entry_right_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_entry));
        auto this_entry_right_source = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            entry_right_target, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            this_entry_right_source, Gtk::Constraint::Attribute::RIGHT,
            1.0, 0.0, 800));

        auto entry_top_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_entry));
        auto gl_bottom_target2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_gl_widget));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            entry_top_target, Gtk::Constraint::Attribute::TOP,
            Gtk::Constraint::Relation::EQ,
            gl_bottom_target2, Gtk::Constraint::Attribute::BOTTOM,
            1.0, 0.0, 800));

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
        _shortcut_controller->add_shortcut(escape_shortcut);

        _entry.add_controller(_shortcut_controller);

        Glib::Value<Glib::ustring> entry_desc_value;
        entry_desc_value.init(Glib::Value<Glib::ustring>::value_type());
        entry_desc_value.set("Entry description (Shortcuts: Enter to activate, Escape to cancel)");
        _entry.update_property(Gtk::Accessible::Property::DESCRIPTION, entry_desc_value);

        _key_controller = Gtk::EventControllerKey::create();
        _key_controller->set_name("editor-key-controller");
        _key_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);

        _key_controller->signal_key_pressed().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                bool handled = false;
                if (_receiver && keyval) {
                    handled = true;
                }

                if (handled) {
                    if (keyval == GDK_KEY_Delete ||
                        keyval == GDK_KEY_BackSpace ||
                        keyval == GDK_KEY_Tab) {
                        Glib::Value<Glib::ustring> busy_desc;
                        busy_desc.init(Glib::Value<Glib::ustring>::value_type());
                        busy_desc.set("Processing input");
                        _gl_widget.update_property(Gtk::Accessible::Property::DESCRIPTION, busy_desc);

                        Glib::Value<Glib::ustring> enabled_desc;
                        enabled_desc.init(Glib::Value<Glib::ustring>::value_type());
                        enabled_desc.set("Input enabled");
                        _gl_widget.update_property(Gtk::Accessible::Property::DESCRIPTION, enabled_desc);
                    }

                    Glib::Value<Glib::ustring> label_value;
                    label_value.init(Glib::Value<Glib::ustring>::value_type());

                    if (keyval == GDK_KEY_Delete) {
                        label_value.set("SolveSpace 3D View - Delete Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_Escape) {
                        label_value.set("SolveSpace 3D View - Normal Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_l || keyval == GDK_KEY_L) {
                        label_value.set("SolveSpace 3D View - Line Creation Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_c || keyval == GDK_KEY_C) {
                        label_value.set("SolveSpace 3D View - Circle Creation Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_a || keyval == GDK_KEY_A) {
                        label_value.set("SolveSpace 3D View - Arc Creation Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_r || keyval == GDK_KEY_R) {
                        label_value.set("SolveSpace 3D View - Rectangle Creation Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_d || keyval == GDK_KEY_D) {
                        label_value.set("SolveSpace 3D View - Dimension Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_q || keyval == GDK_KEY_Q) {
                        label_value.set("SolveSpace 3D View - Construction Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_w || keyval == GDK_KEY_W) {
                        label_value.set("SolveSpace 3D View - Workplane Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_m || keyval == GDK_KEY_M) {
                        label_value.set("SolveSpace 3D View - Measurement Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_g || keyval == GDK_KEY_G) {
                        label_value.set("SolveSpace 3D View - Group Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_s || keyval == GDK_KEY_S) {
                        label_value.set("SolveSpace 3D View - Step Dimension Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_t || keyval == GDK_KEY_T) {
                        label_value.set("SolveSpace 3D View - Text Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, label_value);
                    } else if (keyval == GDK_KEY_d || keyval == GDK_KEY_D) {
                        Glib::Value<Glib::ustring> mode_value;
                        mode_value.init(Glib::Value<Glib::ustring>::value_type());
                        mode_value.set("SolveSpace 3D View - Dimension Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, mode_value);
                    } else if (keyval == GDK_KEY_w || keyval == GDK_KEY_W) {
                        Glib::Value<Glib::ustring> workplane_value;
                        workplane_value.init(Glib::Value<Glib::ustring>::value_type());
                        workplane_value.set("SolveSpace 3D View - Workplane Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, workplane_value);
                    } else if (keyval == GDK_KEY_s || keyval == GDK_KEY_S) {
                        Glib::Value<Glib::ustring> selection_value;
                        selection_value.init(Glib::Value<Glib::ustring>::value_type());
                        selection_value.set("SolveSpace 3D View - Selection Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, selection_value);
                    } else if (keyval == GDK_KEY_g || keyval == GDK_KEY_G) {
                        Glib::Value<Glib::ustring> group_value;
                        group_value.init(Glib::Value<Glib::ustring>::value_type());
                        group_value.set("SolveSpace 3D View - Group Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, group_value);
                    } else if (keyval == GDK_KEY_m || keyval == GDK_KEY_M) {
                        Glib::Value<Glib::ustring> measure_value;
                        measure_value.init(Glib::Value<Glib::ustring>::value_type());
                        measure_value.set("SolveSpace 3D View - Measure Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, measure_value);
                    } else if (keyval == GDK_KEY_t || keyval == GDK_KEY_T) {
                        Glib::Value<Glib::ustring> text_value;
                        text_value.init(Glib::Value<Glib::ustring>::value_type());
                        text_value.set("SolveSpace 3D View - Text Mode");
                        _gl_widget.update_property(Gtk::Accessible::Property::LABEL, text_value);
                    }
                }

                return handled;
            }, false);

        _key_controller->signal_key_released().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                bool handled = false;
                if (_receiver && keyval) {
                    handled = true;
                }
                return handled;
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

        auto entry_target_x = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_entry));
        auto entry_constraint_x = Gtk::Constraint::create(
            entry_target_x,                         // target widget
            Gtk::Constraint::Attribute::LEFT,         // target attribute
            Gtk::Constraint::Relation::EQ,            // relation
            Glib::RefPtr<Gtk::ConstraintTarget>(),  // source widget (nullptr = parent)
            Gtk::Constraint::Attribute::LEFT,         // source attribute
            1.0,                                    // multiplier
            adjusted_x,                             // constant
            1000                                    // strength
        );

        auto entry_target_y = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_entry));
        auto entry_constraint_y = Gtk::Constraint::create(
            entry_target_y,                         // target widget
            Gtk::Constraint::Attribute::TOP,          // target attribute
            Gtk::Constraint::Relation::EQ,            // relation
            Glib::RefPtr<Gtk::ConstraintTarget>(),  // source widget (nullptr = parent)
            Gtk::Constraint::Attribute::TOP,          // source attribute
            1.0,                                    // multiplier
            adjusted_y,                             // constant
            1000                                    // strength
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

            Glib::RefPtr<Gtk::ConstraintTarget> entry_target1 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_entry));
            _constraint_layout->add_constraint(Gtk::Constraint::create(
                entry_target1,
                Gtk::Constraint::Attribute::WIDTH,
                Gtk::Constraint::Relation::GE,
                100.0,  // constant
                1000)); // strength

            Glib::RefPtr<Gtk::ConstraintTarget> entry_target2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_entry));
            _constraint_layout->add_constraint(Gtk::Constraint::create(
                entry_target2,
                Gtk::Constraint::Attribute::HEIGHT,
                Gtk::Constraint::Relation::EQ,
                static_cast<double>(entry_height),  // constant
                1000)); // strength

            set_layout_manager(_constraint_layout);
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

    void setup_layout_constraints() {
        auto vbox_target1 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_vbox));
        auto this_target1 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            vbox_target1, Gtk::Constraint::Attribute::TOP,
            Gtk::Constraint::Relation::EQ,
            this_target1, Gtk::Constraint::Attribute::TOP,
            1.0, 0.0, 1000));

        auto vbox_target2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_vbox));
        auto this_target2 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            vbox_target2, Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            this_target2, Gtk::Constraint::Attribute::LEFT,
            1.0, 0.0, 1000));

        auto vbox_target3 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_vbox));
        auto this_target3 = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            vbox_target3, Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            this_target3, Gtk::Constraint::Attribute::RIGHT,
            1.0, 0.0, 1000));

        auto vbox_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_vbox));
        auto this_target = Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(this));
        _constraint_layout->add_constraint(Gtk::Constraint::create(
            vbox_target, Gtk::Constraint::Attribute::BOTTOM,
            Gtk::Constraint::Relation::EQ,
            this_target, Gtk::Constraint::Attribute::BOTTOM,
            1.0, 0.0, 1000));
    }

    void setup_fullscreen_binding() {
        property_maximized().signal_changed().connect([this]() {
            bool is_fullscreen = property_maximized().get_value();

            if (_is_fullscreen != is_fullscreen) {
                _is_fullscreen = is_fullscreen;

                set_property("accessible-state",
                    std::string(is_fullscreen ? "expanded" : "collapsed"));

                if(_receiver->onFullScreen) {
                    _receiver->onFullScreen(is_fullscreen);
                }
            }
        });

        set_property("accessible-role", Gtk::Accessible::Role::APPLICATION);
        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set(C_("app-name", "SolveSpace"));
        update_property(Gtk::Accessible::Property::LABEL, label_value);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("accessibility", "Parametric 2D/3D CAD application"));
        update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);

        Glib::Value<Gtk::Orientation> orientation;
        orientation.init(Glib::Value<Gtk::Orientation>::value_type());
        orientation.set(Gtk::Orientation::VERTICAL);
        update_property(Gtk::Accessible::Property::ORIENTATION, orientation);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("app-description", "Parametric 2D/3D CAD application"));
        update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
    }

    void setup_event_controllers() {
        _motion_controller = Gtk::EventControllerMotion::create();
        _motion_controller->set_name("window-motion-controller");
        _motion_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);

        _motion_controller->signal_enter().connect(
            [this](double x, double y) {
                _is_under_cursor = true;

                Glib::Value<Glib::ustring> app_role_value;
                app_role_value.init(Glib::Value<Glib::ustring>::value_type());
                app_role_value.set("application");
                update_property(Gtk::Accessible::Property::DESCRIPTION, app_role_value);
                Glib::Value<Glib::ustring> focus_value;
                focus_value.init(Glib::Value<Glib::ustring>::value_type());
                focus_value.set("Element has focus");
                update_property(Gtk::Accessible::Property::DESCRIPTION, focus_value);

                return true;
            });

        _motion_controller->signal_leave().connect(
            [this]() {
                _is_under_cursor = false;

                Glib::Value<Glib::ustring> lost_focus_value;
                lost_focus_value.init(Glib::Value<Glib::ustring>::value_type());
                lost_focus_value.set("Element lost focus");
                update_property(Gtk::Accessible::Property::DESCRIPTION, lost_focus_value);

                return true;
            });

        add_controller(_motion_controller);

        auto close_controller = Gtk::ShortcutController::create();
        close_controller->set_name("window-close-controller");
        close_controller->set_scope(Gtk::ShortcutScope::LOCAL);

        auto close_action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&) {
            if(_receiver->onClose) {
                _receiver->onClose();
            }
            return true;
        });

        auto close_trigger = Gtk::AlternativeTrigger::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_w, Gdk::ModifierType::CONTROL_MASK),
            Gtk::KeyvalTrigger::create(GDK_KEY_q, Gdk::ModifierType::CONTROL_MASK)
        );

        auto close_named_action = Gtk::NamedAction::create("close-window");
        auto close_shortcut = Gtk::Shortcut::create(close_trigger, close_named_action);
        close_controller->add_shortcut(close_shortcut);
        add_controller(close_controller);

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
                if(_receiver) {
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

                    event.shiftDown = (state & static_cast<Gdk::ModifierType>(GDK_SHIFT_MASK)) != Gdk::ModifierType(0);
                    event.controlDown = (state & static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK)) != Gdk::ModifierType(0);

                    if (keyval == GDK_KEY_Escape || keyval == GDK_KEY_Delete ||
                        keyval == GDK_KEY_Tab || (keyval >= GDK_KEY_F1 && keyval <= GDK_KEY_F12)) {
                        Glib::Value<Glib::ustring> busy_value;
                        busy_value.init(Glib::Value<Glib::ustring>::value_type());
                        busy_value.set("Processing key event");
                        update_property(Gtk::Accessible::Property::DESCRIPTION, busy_value);
                    }

                    if(_receiver) {
                        _receiver->onKeyboardEvent(event);
                    }
                    return true;
                }
                return false;
            }, false);

        key_controller->signal_key_released().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                if(_receiver) {
                    Platform::KeyboardEvent event = {};
                    event.key = Platform::KeyboardEvent::Key::CHARACTER;
                    event.chr = keyval;
                    event.shiftDown = (state & static_cast<Gdk::ModifierType>(GDK_SHIFT_MASK)) != Gdk::ModifierType(0);
                    event.controlDown = (state & static_cast<Gdk::ModifierType>(GDK_CONTROL_MASK)) != Gdk::ModifierType(0);
                    if(_receiver) {
                        _receiver->onKeyboardEvent(event);
                    }
                    return true;
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
                Glib::Value<Glib::ustring> active_value;
                active_value.init(Glib::Value<Glib::ustring>::value_type());
                active_value.set("Mouse button pressed");
                update_property(Gtk::Accessible::Property::DESCRIPTION, active_value);
            });

        add_controller(gesture_controller);
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

            Glib::Value<Glib::ustring> desc_value;
            desc_value.init(Glib::Value<Glib::ustring>::value_type());
            desc_value.set(std::string("Parametric 2D/3D CAD application") +
                (dark_theme ? " (Dark theme)" : " (Light theme)"));
            update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
        });
    }

public:
    GtkWindow(Platform::Window *receiver) :
        Gtk::Window(),
        _receiver(receiver),
        _vbox(Gtk::Orientation::VERTICAL),
        _hbox(Gtk::Orientation::HORIZONTAL),
        _editor_overlay(receiver),
        _scrollbar(Gtk::Adjustment::create(0, 0, 100, 1, 10, 10), Gtk::Orientation::VERTICAL),
        _is_under_cursor(false),
        _is_fullscreen(false)
    {
        _constraint_layout = Gtk::ConstraintLayout::create();
        set_layout_manager(_constraint_layout);
        setup_layout_constraints();

        auto css_provider = Gtk::CssProvider::create();
        
        try {
            auto theme_file = Gio::File::create_for_path(Platform::PathFromResource("platform/css/theme_colors.css"));
            css_provider->load_from_file(theme_file);
            
            auto window_file = Gio::File::create_for_path(Platform::PathFromResource("platform/css/window.css"));
            css_provider->load_from_file(window_file);
        } catch (const Glib::Error& e) {
            static const char* theme_colors = 
                "@define-color bg_color #f5f5f5;"
                "@define-color fg_color #333333;"
                "@define-color header_bg #e0e0e0;"
                "@define-color header_border #c0c0c0;"
                "@define-color button_hover rgba(128, 128, 128, 0.1);"
                "@define-color accent_color #0066cc;"
                "@define-color accent_fg white;"
                "@define-color entry_bg white;"
                "@define-color entry_fg black;"
                "@define-color border_color #e0e0e0;"
                
                "@define-color dark_bg_color #2d2d2d;"
                "@define-color dark_fg_color #e0e0e0;"
                "@define-color dark_header_bg #1e1e1e;"
                "@define-color dark_header_border #3d3d3d;"
                "@define-color dark_button_hover rgba(255, 255, 255, 0.1);"
                "@define-color dark_accent_color #3584e4;"
                "@define-color dark_accent_fg white;"
                "@define-color dark_entry_bg #3d3d3d;"
                "@define-color dark_entry_fg #e0e0e0;"
                "@define-color dark_border_color #3d3d3d;";
            
            static const char* window_css = 
                "window.solvespace-window {"
                "    background-color: @theme_bg_color;"
                "    color: @theme_fg_color;"
                "}"
                "window.solvespace-window.dark {"
                "    background-color: #303030;"
                "    color: #e0e0e0;"
                "}"
                "window.solvespace-window.light {"
                "    background-color: #f0f0f0;"
                "    color: #303030;"
                "}"
                "window.solvespace-window[text-direction=\"rtl\"] {"
                "    direction: rtl;"
                "}"
                "window.solvespace-window[text-direction=\"rtl\"] * {"
                "    text-align: right;"
                "}"
                "window.solvespace-window[text-direction=\"rtl\"] button,"
                "window.solvespace-window[text-direction=\"rtl\"] label,"
                "window.solvespace-window[text-direction=\"rtl\"] menuitem {"
                "    margin-left: 8px;"
                "    margin-right: 0;"
                "}"
                "window.solvespace-window[text-direction=\"rtl\"] .solvespace-header {"
                "    flex-direction: row-reverse;"
                "}"
                "window.solvespace-window[text-direction=\"rtl\"] menubar > menuitem {"
                "    margin-right: 4px;"
                "    margin-left: 0;"
                "}"
                "scrollbar {"
                "    background-color: alpha(@theme_fg_color, 0.1);"
                "    border-radius: 0;"
                "}"
                "scrollbar slider {"
                "    min-width: 16px;"
                "    border-radius: 8px;"
                "    background-color: alpha(@theme_fg_color, 0.3);"
                "}"
                "scrollbar slider:hover {"
                "    background-color: alpha(@theme_fg_color, 0.5);"
                "}"
                "scrollbar slider:active {"
                "    background-color: alpha(@theme_fg_color, 0.7);"
                "}"
                ".solvespace-gl-area {"
                "    background-color: @theme_base_color;"
                "    border-radius: 2px;"
                "    border: 1px solid @borders;"
                "}"
                "button.menu-button {"
                "    padding: 4px 8px;"
                "    border-radius: 3px;"
                "    background-color: alpha(@theme_fg_color, 0.05);"
                "    color: @theme_fg_color;"
                "}"
                "button.menu-button:hover {"
                "    background-color: alpha(@theme_fg_color, 0.1);"
                "}"
                "button.menu-button:active {"
                "    background-color: alpha(@theme_fg_color, 0.15);"
                "}"
                ".solvespace-header {"
                "    padding: 4px;"
                "    background-color: @theme_bg_color;"
                "    border-bottom: 1px solid @borders;"
                "}"
                ".solvespace-editor-text {"
                "    background-color: @theme_base_color;"
                "    color: @theme_text_color;"
                "    border-radius: 3px;"
                "    padding: 4px;"
                "    caret-color: @link_color;"
                "}";
            
            std::string combined_css = std::string(theme_colors) + "\n" + std::string(window_css);
            css_provider->load_from_data(combined_css);
        }

        set_name("solvespace-window");
        add_css_class("solvespace-window");

        Gtk::StyleContext::add_provider_for_display(
            get_display(),
            css_provider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            
        auto settings = Gtk::Settings::get_default();
        auto theme_binding = Gtk::PropertyExpression<bool>::create(
            settings->property_gtk_application_prefer_dark_theme());
        theme_binding->connect([this, settings]() {
            bool dark_theme = settings->property_gtk_application_prefer_dark_theme();
            if(dark_theme) {
                add_css_class("dark");
                remove_css_class("light");
            } else {
                remove_css_class("dark");
                add_css_class("light");
            }
            
            Glib::Value<Glib::ustring> desc_value;
            desc_value.init(Glib::Value<Glib::ustring>::value_type());
            desc_value.set(C_("accessibility", "SolveSpace CAD - ") + 
                (dark_theme ? C_("theme", "Dark theme") : C_("theme", "Light theme")));
            update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
        });
        
        auto settings_impl = dynamic_cast<SettingsImplGtk*>(Platform::GetSettings().get());
        if(settings_impl) {
            std::string locale = settings_impl->ThawString("locale", "");
            bool is_rtl = false;
            
            if(!locale.empty()) {
                std::vector<std::string> rtl_languages = {"ar", "he", "fa", "ur", "ps", "sd", "yi", "dv"};
                std::string lang_code = locale.substr(0, 2);
                
                for(const auto& rtl_lang : rtl_languages) {
                    if(lang_code == rtl_lang) {
                        is_rtl = true;
                        break;
                    }
                }
            }
            
            if(is_rtl) {
                std::string direction = "rtl";
                set_property("text-direction", direction);
                
                Glib::Value<Glib::ustring> rtl_value;
                rtl_value.init(Glib::Value<Glib::ustring>::value_type());
                rtl_value.set(C_("accessibility", "Right-to-left text direction"));
                update_property(Gtk::Accessible::Property::ORIENTATION, rtl_value);
            } else {
                std::string direction = "ltr";
                set_property("text-direction", direction);
            }
        }

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
        setup_fullscreen_binding();
        setup_property_bindings();

        _constraint_layout->add_constraint(Gtk::Constraint::create(
            Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_hbox)),
            Gtk::Constraint::Attribute::LEFT,
            Gtk::Constraint::Relation::EQ,
            Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_vbox)),
            Gtk::Constraint::Attribute::LEFT,
            1.0, 0.0, 1000));

        _constraint_layout->add_constraint(Gtk::Constraint::create(
            Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_hbox)),
            Gtk::Constraint::Attribute::RIGHT,
            Gtk::Constraint::Relation::EQ,
            Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_vbox)),
            Gtk::Constraint::Attribute::RIGHT,
            1.0, 0.0, 1000));

        _constraint_layout->add_constraint(Gtk::Constraint::create(
            Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_editor_overlay)),
            Gtk::Constraint::Attribute::WIDTH,
            Gtk::Constraint::Relation::EQ,
            Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_hbox)),
            Gtk::Constraint::Attribute::WIDTH,
            1.0, -20, 1000)); // Subtract scrollbar width

        _constraint_layout->add_constraint(Gtk::Constraint::create(
            Glib::RefPtr<Gtk::ConstraintTarget>(dynamic_cast<Gtk::ConstraintTarget*>(&_scrollbar)),
            Gtk::Constraint::Attribute::WIDTH,
            Gtk::Constraint::Relation::EQ,
            Glib::RefPtr<Gtk::ConstraintTarget>(nullptr),
            Gtk::Constraint::Attribute::NONE,
            0.0, 20, 1000)); // Fixed widthfor scrollbar

        _vbox.set_visible(true);
        _hbox.set_visible(true);
        _editor_overlay.set_visible(true);
        get_gl_widget().set_visible(true);

        auto adjustment = Gtk::Adjustment::create(0.0, 0.0, 100.0, 1.0, 10.0, 10.0);
        _scrollbar.set_adjustment(adjustment);

        adjustment->property_value().signal_changed().connect([this, adjustment]() {
            double value = adjustment->get_value();
            if(_receiver->onScrollbarAdjusted) {
                _receiver->onScrollbarAdjusted(value / adjustment->get_upper());
            }
        });

        get_gl_widget().set_has_tooltip(true);

        auto tooltip_controller = Gtk::EventControllerMotion::create();
        tooltip_controller->set_name("gl-widget-tooltip-controller");

        property_tooltip_text().signal_changed().connect([this]() {
            get_gl_widget().set_tooltip_text(get_tooltip_text());
        });

        get_gl_widget().add_controller(tooltip_controller);

        setup_event_controllers();
        setup_fullscreen_binding();
        setup_property_bindings();

        set_property("accessible-role", std::string("application"));

        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set("SolveSpace");
        update_property(Gtk::Accessible::Property::LABEL, label_value);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set("Parametric 2D/3D CAD application");
        update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
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
    
    void announce_operation_mode(const std::string &mode) {
        get_gl_widget().announce_operation_mode(mode);
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

            settings->property_gtk_application_prefer_dark_theme().signal_changed().connect(
                [this, settings]() {
                    bool dark_theme = false;
                    settings->get_property("gtk-application-prefer-dark-theme", dark_theme);
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

        gtkWindow.set_tooltip_text(C_("tooltip", "SolveSpace - Parametric 2D/3D CAD tool"));

        gtkWindow.set_property("accessible-role",
            kind == Kind::TOOL ? "dialog" : "application");
        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set(C_("accessibility", "SolveSpace"));
        gtkWindow.update_property(Gtk::Accessible::Property::LABEL, label_value);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("accessibility", "Parametric 2D/3D CAD tool"));
        gtkWindow.update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
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

        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set("SolveSpace" + (title.empty() ? "" : ": " + title));
        gtkWindow.update_property(Gtk::Accessible::Property::LABEL, label_value);
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
                    if (!menuImpl->menuItems.empty() && !menuImpl->menuItems[0]->actionName.empty()) {
                        menuLabel = menuImpl->menuItems[0]->actionName;
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
                    item->set_label(menuItem->actionName);
                    item->set_has_frame(false);
                    item->add_css_class("flat");
                    item->add_css_class("menu-item");
                    item->set_halign(Gtk::Align::FILL);
                    item->set_hexpand(true);
                    item->set_tooltip_text(menuItem->actionName);

                    item->set_property("accessible-role", std::string("menu_item"));
                    Glib::Value<Glib::ustring> label_value;
                    label_value.init(Glib::Value<Glib::ustring>::value_type());
                    label_value.set(menuItem->actionName);
                    item->update_property(Gtk::Accessible::Property::LABEL, label_value);

                    Glib::Value<Glib::ustring> desc_value;
                    desc_value.init(Glib::Value<Glib::ustring>::value_type());
                    desc_value.set("Menu item: " + menuItem->actionName);
                    item->update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);

                    if (menuItem->onTrigger) {
                        auto active_binding = Gtk::PropertyExpression<bool>::create(
                            Gtk::Button::get_type(), // Use Button instead of CheckMenuItem
                            "active" // Property name
                        );

                        item->signal_clicked().connect([item]() {
                            Glib::Value<Glib::ustring> pressed_value;
                            pressed_value.init(Glib::Value<Glib::ustring>::value_type());
                            pressed_value.set("Element is pressed");
                            item->update_property(Gtk::Accessible::Property::DESCRIPTION, pressed_value);

                            auto timer = CreateTimer();
                            timer->onTimeout = [item]() {
                                Glib::Value<Glib::ustring> inactive_value;
                                inactive_value.init(Glib::Value<Glib::ustring>::value_type());
                                inactive_value.set("Element is inactive");
                                item->update_property(Gtk::Accessible::Property::DESCRIPTION, inactive_value);
                            };
                            timer->RunAfter(200); // 200ms delay
                        });

                        auto action = Gtk::CallbackAction::create([popover, onTrigger = menuItem->onTrigger](Gtk::Widget&, const Glib::VariantBase&) {
                            popover->popdown();
                            onTrigger();
                            return true;
                        });

                        auto trigger = Gtk::KeyvalTrigger::create(GDK_KEY_Return);
                        auto shortcut = Gtk::Shortcut::create(trigger, action);

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

                        shortcutLabel->set_property("accessible-role", std::string("label"));
                        Glib::Value<Glib::ustring> shortcut_label;
                        shortcut_label.init(Glib::Value<Glib::ustring>::value_type());
                        shortcut_label.set("Shortcut: " + menuItemImpl->shortcutText);
                        shortcutLabel->update_property(Gtk::Accessible::Property::LABEL, shortcut_label);

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

        adjustment->property_value().signal_changed().connect([this, adjustment]() {
            double value = adjustment->get_value();
            if(onScrollbarAdjusted) {
                onScrollbarAdjusted(value / adjustment->get_upper());
            }
        });

        gtkWindow.get_scrollbar().set_adjustment(adjustment);

        Glib::Value<double> max_value;
        max_value.init(Glib::Value<double>::value_type());
        max_value.set(max);
        gtkWindow.get_scrollbar().update_property(Gtk::Accessible::Property::VALUE_MAX, max_value);

        Glib::Value<double> min_value;
        min_value.init(Glib::Value<double>::value_type());
        min_value.set(min);
        gtkWindow.get_scrollbar().update_property(Gtk::Accessible::Property::VALUE_MIN, min_value);

        Glib::Value<double> now_value;
        now_value.init(Glib::Value<double>::value_type());
        now_value.set(adjustment->get_value());
        gtkWindow.get_scrollbar().update_property(Gtk::Accessible::Property::VALUE_NOW, now_value);
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

void AnnounceOperationMode(const std::string& mode, Gtk::Window* window) {
    if (!window) return;
    
    Glib::Value<Glib::ustring> label_value;
    label_value.init(Glib::Value<Glib::ustring>::value_type());
    label_value.set(C_("accessibility", "SolveSpace 3D View - ") + C_("operation-mode", mode));
    window->update_property(Gtk::Accessible::Property::DESCRIPTION, label_value);
    
    Glib::Value<Glib::ustring> live_value;
    live_value.init(Glib::Value<Glib::ustring>::value_type());
    live_value.set("polite");
    window->update_property(Gtk::Accessible::Property::LIVE, live_value);
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
            auto message_type = gtkDialog.property_message_type().get_value();
            if (message_type == Gtk::MessageType::QUESTION) {
                dialogType = "Question";
            } else if (message_type == Gtk::MessageType::WARNING) {
                dialogType = "Warning";
            } else if (message_type == Gtk::MessageType::ERROR) {
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
                Glib::Value<Glib::ustring> desc_value;
                desc_value.init(Glib::Value<Glib::ustring>::value_type());
                desc_value.set(description);
                button->update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
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

        gtkDialog.property_visible().signal_changed().connect([this]() {
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
        shortcut_controller->add_shortcut(escape_shortcut);

        auto enter_action = Gtk::CallbackAction::create([this, &response, &loop](Gtk::Widget&, const Glib::VariantBase&) {
            response = Gtk::ResponseType::OK;
            loop->quit();
            return true;
        });
        auto enter_shortcut = Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_Return, Gdk::ModifierType(0)),
            enter_action);
        shortcut_controller->add_shortcut(enter_shortcut);

        gtkDialog.add_controller(shortcut_controller);

        auto key_controller = Gtk::EventControllerKey::create();
        key_controller->set_name("dialog-key-controller");
        key_controller->signal_key_pressed().connect(
            [this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                auto default_widget = gtkDialog.get_default_widget();
                if (default_widget) {
                    default_widget->grab_focus();

                    Glib::Value<Glib::ustring> desc_value;
                    desc_value.init(Glib::Value<Glib::ustring>::value_type());
                    desc_value.set("Element has focus");
                    default_widget->update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
                }
                return false; // Allow event propagation
            }, false);
        gtkDialog.add_controller(key_controller);

        auto response_controller = Gtk::EventControllerKey::create();
        response_controller->set_name("dialog-response-controller");
        response_controller->signal_key_released().connect(
            [&response, &loop, this](guint keyval, guint keycode, Gdk::ModifierType state) -> bool {
                if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
                    response = Gtk::ResponseType::OK;
                    loop->quit();
                    return true;
                }
                return false;
            });
        gtkDialog.add_controller(response_controller);

        gtkDialog.property_visible().signal_changed().connect(
            [&loop, &response, this]() {
                if (!gtkDialog.get_visible()) {
                    loop->quit();
                }
            });

        gtkDialog.set_tooltip_text("Message Dialog");

        gtkDialog.set_property("accessible-role", std::string("dialog"));

        Glib::Value<Glib::ustring> dialog_label;
        dialog_label.init(Glib::Value<Glib::ustring>::value_type());
        dialog_label.set("Message Dialog");
        gtkDialog.update_property(Gtk::Accessible::Property::LABEL, dialog_label);

        Glib::Value<Glib::ustring> dialog_desc;
        dialog_desc.init(Glib::Value<Glib::ustring>::value_type());
        dialog_desc.set("SolveSpace notification dialog");
        gtkDialog.update_property(Gtk::Accessible::Property::DESCRIPTION, dialog_desc);

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
            widget->set_property("accessible-role", std::string("file_chooser"));
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
                        this->FilterChanged();
                        return true;
                    }
                    return false;
                });
            dialog->add_controller(response_controller);

            dialog->property_filter().signal_changed().connect(
                [this]() {
                    this->FilterChanged();
                });

            auto shortcut_controller = Gtk::ShortcutController::create();
            shortcut_controller->set_scope(Gtk::ShortcutScope::LOCAL);
            shortcut_controller->set_name("file-dialog-shortcuts");

            auto home_action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&) {
                gtkChooser->set_current_folder(Gio::File::create_for_path(g_get_home_dir()));
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

        Glib::Value<Glib::ustring> dialog_label;
        dialog_label.init(Glib::Value<Glib::ustring>::value_type());
        dialog_label.set(isSave ? C_("dialog-title", "Save File") : C_("dialog-title", "Open File"));
        gtkDialog.update_property(Gtk::Accessible::Property::LABEL, dialog_label);

        Glib::Value<Glib::ustring> dialog_desc;
        dialog_desc.init(Glib::Value<Glib::ustring>::value_type());
        dialog_desc.set(isSave ? C_("dialog-description", "Dialog for saving SolveSpace files")
                   : C_("dialog-description", "Dialog for opening SolveSpace files"));
        gtkDialog.update_property(Gtk::Accessible::Property::DESCRIPTION, dialog_desc);

        auto cancel_button = gtkDialog.add_button(C_("button", "_Cancel"), Gtk::ResponseType::CANCEL);
        cancel_button->add_css_class("destructive-action");
        cancel_button->add_css_class("cancel-action");
        cancel_button->set_name("cancel-button");
        cancel_button->set_tooltip_text(C_("tooltip", "Cancel"));

        cancel_button->set_property("accessible-role", std::string("button"));

        Glib::Value<Glib::ustring> cancel_label;
        cancel_label.init(Glib::Value<Glib::ustring>::value_type());
        cancel_label.set(C_("button", "Cancel"));
        cancel_button->update_property(Gtk::Accessible::Property::LABEL, cancel_label);

        Glib::Value<Glib::ustring> cancel_desc;
        cancel_desc.init(Glib::Value<Glib::ustring>::value_type());
        cancel_desc.set("Cancel the file operation");
        cancel_button->update_property(Gtk::Accessible::Property::DESCRIPTION, cancel_desc);

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

        gtkDialog.signal_response().connect(
            [&loop, &response_id, this](int response) {
                if (response != Gtk::ResponseType::NONE) {
                    response_id = static_cast<Gtk::ResponseType>(response);
                    loop->quit();
                }
            });

        gtkDialog.property_visible().signal_changed().connect(
            [&loop, this]() {
                if (!gtkDialog.get_visible()) {
                    loop->quit();
                }
            });

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
        escape_shortcut = Gtk::Shortcut::create(escape_shortcut->get_trigger(), escape_action);
        shortcut_controller->add_shortcut(escape_shortcut);

        auto enter_action = Gtk::CallbackAction::create([&response_id, &loop, this](Gtk::Widget&, const Glib::VariantBase&) {
            response_id = Gtk::ResponseType::OK;
            loop->quit();
            return true;
        });
        auto enter_shortcut = Gtk::Shortcut::create(
            Gtk::KeyvalTrigger::create(GDK_KEY_Return, Gdk::ModifierType(0)),
            enter_action);
        enter_shortcut = Gtk::Shortcut::create(enter_shortcut->get_trigger(), enter_action);
        shortcut_controller->add_shortcut(enter_shortcut);

        gtkDialog.add_controller(shortcut_controller);

        gtkDialog.property_visible().signal_changed().connect(
            [&loop, this]() {
                if (!gtkDialog.get_visible()) {
                    loop->quit();
                }
            });

        gtkDialog.set_modal(true);

        gtkDialog.show();
        loop->run();

        return response_id == Gtk::ResponseType::OK;
    }
};

#if defined(HAVE_GTK_FILECHOOSERNATIVE)

class FileDialogNativeImplGtk final : public FileDialogImplGtk {
public:
    Glib::RefPtr<Gtk::FileChooserNative> gtkNative;
    bool isSave;

    FileDialogNativeImplGtk(Gtk::Window &gtkParent, bool isSave) : isSave(isSave) {
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

        gtkNative->set_title(isSave ? C_("dialog-title", "Save SolveSpace File")
                                    : C_("dialog-title", "Open SolveSpace File"));

        std::string role = "dialog";
        gtkNative->set_property("accessible-role", role);

        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set(isSave ? C_("dialog-title", "Save File") : C_("dialog-title", "Open File"));
        gtkNative->update_property(Gtk::Accessible::Property::LABEL, label_value);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(isSave ? C_("dialog-description", "Dialog to save SolveSpace files")
                             : C_("dialog-description", "Dialog to open SolveSpace files"));
        gtkNative->update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);

        if(!isSave) {
            gtkNative->set_select_multiple(true);
        } else {
            gtkNative->set_current_name("untitled");
        }

        auto home_dir = Glib::get_home_dir();
        if(!home_dir.empty()) {
            gtkNative->set_current_folder(Gio::File::create_for_path(home_dir));
        }

        gtkNative->set_create_folders(true);

        gtkNative->set_search_mode(true);

        gtkNative->set_show_hidden(false);

        gtkNative->set_default_size(800, 600);

        if (IsRTL()) {
            Glib::Value<Glib::ustring> rtl_value;
            rtl_value.init(Glib::Value<Glib::ustring>::value_type());
            std::string rtl_direction = "rtl";
            rtl_value.set(rtl_direction);
            gtkNative->update_property(Gtk::Accessible::Property::ORIENTATION, rtl_value);
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
        
        auto visibility_binding = Gtk::PropertyExpression<bool>::create(gtkNative->property_visible());
        visibility_binding->connect([&loop, this]() {
            if (!gtkNative->get_visible()) {
                loop->quit();
            }
        });

        if (auto widget = gtkNative->get_widget()) {
            widget->add_css_class("solvespace-file-dialog");
            widget->add_css_class("dialog");
            widget->add_css_class(isSave ? "save-dialog" : "open-dialog");

            widget->set_property("accessible-role", std::string("dialog"));

            Glib::Value<Glib::ustring> label_value;
            label_value.init(Glib::Value<Glib::ustring>::value_type());
            label_value.set(isSave ? C_("dialog-title", "Save SolveSpace File")
                                   : C_("dialog-title", "Open SolveSpace File"));
            widget->update_property(Gtk::Accessible::Property::LABEL, label_value);

            Glib::Value<Glib::ustring> desc_value;
            desc_value.init(Glib::Value<Glib::ustring>::value_type());
            desc_value.set(isSave ? C_("dialog-description", "Dialog for saving SolveSpace files")
                                  : C_("dialog-description", "Dialog for opening SolveSpace files"));
            widget->update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);

            Glib::Value<Glib::ustring> modal_value;
            modal_value.init(Glib::Value<Glib::ustring>::value_type());
            modal_value.set("Dialog is modal");
            widget->update_property(Gtk::Accessible::Property::DESCRIPTION, modal_value);

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
            shortcut_controller->add_shortcut(escape_shortcut);

            auto enter_action = Gtk::CallbackAction::create([this](Gtk::Widget&, const Glib::VariantBase&) {
                gtkNative->response(Gtk::ResponseType::ACCEPT);
                return true;
            });
            auto enter_shortcut = Gtk::Shortcut::create(
                Gtk::KeyvalTrigger::create(GDK_KEY_Return, Gdk::ModifierType(0)),
                enter_action);
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

                                Glib::Value<Glib::ustring> focus_desc;
                                focus_desc.init(Glib::Value<Glib::ustring>::value_type());
                                focus_desc.set("Element has focus");
                                button->update_property(Gtk::Accessible::Property::DESCRIPTION, focus_desc);
                                break;
                            }
                        }
                    }
                    return false; // Allow event propagation
                },
                false); // Connect before default handler

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

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class ClipboardImplGtk {
public:
    Glib::RefPtr<Gdk::Clipboard> clipboard;

    ClipboardImplGtk() {
        auto display = Gdk::Display::get_default();
        if (display) {
            clipboard = display->get_clipboard();
        }
    }

    void SetText(const std::string &text) {
        if (clipboard) {
            clipboard->set_text(text);
        }
    }

    std::string GetText() {
        std::string result;
        if (clipboard) {
            auto future = clipboard->read_text_async();

            try {
                result = future.get();
            } catch (const Glib::Error &e) {
                dbp("Clipboard error: %s", e.what().c_str());
            }
        }
        return result;
    }

    void SetImage(const Glib::RefPtr<Gdk::Texture> &texture) {
        if (clipboard && texture) {
            clipboard->set_texture(texture);
        }
    }

    bool HasText() {
        if (clipboard) {
            auto formats = clipboard->get_formats();
            return formats.contain_mime_type("text/plain");
        }
        return false;
    }

    bool HasImage() {
        if (clipboard) {
            auto formats = clipboard->get_formats();
            return formats.contain_mime_type("image/png") ||
                   formats.contain_mime_type("image/jpeg") ||
                   formats.contain_mime_type("image/svg+xml");
        }
        return false;
    }

    void Clear() {
        if (clipboard) {
            clipboard->set_text("");
        }
    }

    void SetData(const std::string &mime_type, const std::vector<uint8_t> &data) {
        if (clipboard) {
            auto bytes = Glib::Bytes::create(data.data(), data.size());
            clipboard->set_content(Gdk::ContentProvider::create_for_bytes(mime_type, bytes));
        }
    }

    std::vector<uint8_t> GetData(const std::string &mime_type) {
        std::vector<uint8_t> result;
        if (clipboard) {
            try {
                auto future = clipboard->read_async(mime_type);

                auto value = future.get();

                if (value.gobj() && G_VALUE_TYPE(value.gobj()) == G_TYPE_BYTES) {
                    auto bytes = Glib::Value<Glib::Bytes>::cast_dynamic(value).get();
                    gsize size = 0;
                    auto data = static_cast<const uint8_t*>(bytes->get_data(size));
                    result.assign(data, data + size);
                }
            } catch (const Glib::Error &e) {
                dbp("Clipboard error: %s", e.what().c_str());
            }
        }
        return result;
    }
};

static std::unique_ptr<ClipboardImplGtk> g_clipboard;

void InitClipboard() {
    g_clipboard = std::make_unique<ClipboardImplGtk>();
}

void SetClipboardText(const std::string &text) {
    if (g_clipboard) {
        g_clipboard->SetText(text);
    }
}

std::string GetClipboardText() {
    if (g_clipboard) {
        return g_clipboard->GetText();
    }
    return "";
}

void SetClipboardImage(const Glib::RefPtr<Gdk::Texture> &texture) {
    if (g_clipboard) {
        g_clipboard->SetImage(texture);
    }
}

bool ClipboardHasText() {
    if (g_clipboard) {
        return g_clipboard->HasText();
    }
    return false;
}

bool ClipboardHasImage() {
    if (g_clipboard) {
        return g_clipboard->HasImage();
    }
    return false;
}

void ClearClipboard() {
    if (g_clipboard) {
        g_clipboard->Clear();
    }
}

void SetClipboardData(const std::string &mime_type, const std::vector<uint8_t> &data) {
    if (g_clipboard) {
        g_clipboard->SetData(mime_type, data);
    }
}

std::vector<uint8_t> GetClipboardData(const std::string &mime_type) {
    if (g_clipboard) {
        return g_clipboard->GetData(mime_type);
    }
    return {};
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class ColorPickerImplGtk {
public:
    Glib::RefPtr<Gtk::ColorDialog> colorDialog;
    std::function<void(const RgbaColor&)> callback;

    ColorPickerImplGtk() {
        colorDialog = Gtk::ColorDialog::create();
        colorDialog->set_title(C_("dialog-title", "Choose a Color"));
        colorDialog->set_modal(true);

        std::string role = "color-chooser";
        colorDialog->set_property("accessible-role", role);

        Glib::Value<Glib::ustring> label_value;
        label_value.init(Glib::Value<Glib::ustring>::value_type());
        label_value.set(C_("dialog-title", "Color Picker"));
        colorDialog->update_property(Gtk::Accessible::Property::LABEL, label_value);

        Glib::Value<Glib::ustring> desc_value;
        desc_value.init(Glib::Value<Glib::ustring>::value_type());
        desc_value.set(C_("dialog-description", "Dialog for selecting colors"));
        colorDialog->update_property(Gtk::Accessible::Property::DESCRIPTION, desc_value);
    }

    void Show(Gtk::Window& parent, const RgbaColor& initialColor,
              std::function<void(const RgbaColor&)> onColorSelected) {
        Gdk::RGBA gdkColor;
        gdkColor.set_rgba(initialColor.redF(), initialColor.greenF(),
                         initialColor.blueF(), initialColor.alphaF());

        callback = onColorSelected;

        colorDialog->choose_rgba(parent, gdkColor,
            sigc::mem_fun(*this, &ColorPickerImplGtk::OnColorSelected));
    }

private:
    void OnColorSelected(const Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            Gdk::RGBA gdkColor = colorDialog->choose_rgba_finish(result);

            RgbaColor color = RGBf(gdkColor.get_red(), gdkColor.get_green(),
                                  gdkColor.get_blue(), gdkColor.get_alpha());

            if (callback) {
                callback(color);
            }
        } catch (const Glib::Error& e) {
            dbp("Color picker error: %s", e.what().c_str());
        }
    }
};

static std::unique_ptr<ColorPickerImplGtk> g_colorPicker;

void InitColorPicker() {
    g_colorPicker = std::make_unique<ColorPickerImplGtk>();
}

static void ShowColorPickerImpl(const RgbaColor& initialColor,
                    std::function<void(const RgbaColor&)> onColorSelected) {
    if (g_colorPicker && gtkApp) {
        auto window = gtkApp->get_active_window();
        if (window) {
            g_colorPicker->Show(*window, initialColor, onColorSelected);
        }
    }
}

namespace Platform {
void ShowColorPicker(const RgbaColor& initialColor,
                    std::function<void(const RgbaColor&)> onColorSelected) {
    ShowColorPickerImpl(initialColor, onColorSelected);
}
}

static Glib::RefPtr<Gtk::Application> gtkApp;

std::vector<std::string> InitGui(int argc, char **argv) {
    std::vector<std::string> args;
    for(int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }
    
    InitColorPicker();
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

    gtkApp->set_resource_base_path("/org/solvespace/SolveSpace");


    auto css_provider = Gtk::CssProvider::create();
    
    bool theme_css_loaded = false;
    
    try {
        auto executable_path = Glib::file_get_contents("/proc/self/exe");
        auto executable_dir = Glib::path_get_dirname(executable_path);
        auto css_path = Glib::build_filename(executable_dir, "..", "share", "solvespace", "css", "theme_colors.css");
        
        if (Glib::file_test(css_path, Glib::FileTest::EXISTS)) {
            css_provider->load_from_path(css_path);
            theme_css_loaded = true;
            dbp("Loaded theme CSS from file: %s", css_path.c_str());
        }
    } catch (const Glib::Error& e) {
        dbp("Error loading theme CSS from file: %s", e.what().c_str());
    }
    
    if (!theme_css_loaded) {
        dbp("Using embedded theme CSS fallback");
        
        #include "css/theme_colors.css.h"
        css_provider->load_from_data(theme_colors_css);
    }
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    auto window_css_provider = Gtk::CssProvider::create();
    bool window_css_loaded = false;
    
    try {
        auto executable_path = Glib::file_get_contents("/proc/self/exe");
        auto executable_dir = Glib::path_get_dirname(executable_path);
        auto css_path = Glib::build_filename(executable_dir, "..", "share", "solvespace", "css", "window.css");
        
        if (Glib::file_test(css_path, Glib::FileTest::EXISTS)) {
            window_css_provider->load_from_path(css_path);
            window_css_loaded = true;
            dbp("Loaded window CSS from file: %s", css_path.c_str());
        }
    } catch (const Glib::Error& e) {
        dbp("Error loading window CSS from file: %s", e.what().c_str());
    }
    
    if (!window_css_loaded) {
        dbp("Using embedded window CSS fallback");
        
        #include "css/window.css.h"
        window_css_provider->load_from_data(window_css);
    }
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        window_css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
    auto editor_css_provider = Gtk::CssProvider::create();
    bool editor_css_loaded = false;
    
    try {
        auto executable_path = Glib::file_get_contents("/proc/self/exe");
        auto executable_dir = Glib::path_get_dirname(executable_path);
        auto css_path = Glib::build_filename(executable_dir, "..", "share", "solvespace", "css", "editor_overlay.css");
        
        if (Glib::file_test(css_path, Glib::FileTest::EXISTS)) {
            editor_css_provider->load_from_path(css_path);
            editor_css_loaded = true;
            dbp("Loaded editor CSS from file: %s", css_path.c_str());
        }
    } catch (const Glib::Error& e) {
        dbp("Error loading editor CSS from file: %s", e.what().c_str());
    }
    
    if (!editor_css_loaded) {
        dbp("Using embedded editor CSS fallback");
        
        editor_css_provider->load_from_data(
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
    }
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        editor_css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
    auto gl_css_provider = Gtk::CssProvider::create();
    bool gl_css_loaded = false;
    
    try {
        auto css_path = Glib::build_filename(executable_dir, "..", "share", "solvespace", "css", "gl_area.css");
        
        if (Glib::file_test(css_path, Glib::FileTest::EXISTS)) {
            gl_css_provider->load_from_path(css_path);
            gl_css_loaded = true;
            dbp("Loaded GL area CSS from file: %s", css_path.c_str());
        }
    } catch (const Glib::Error& e) {
        dbp("Error loading GL area CSS from file: %s", e.what().c_str());
    }
    
    if (!gl_css_loaded) {
        dbp("Using embedded GL area CSS fallback");
        gl_css_provider->load_from_data(
            ".solvespace-gl-area { "
            "   background-color: #ffffff; "
            "   border-radius: 2px; "
            "   border: 1px solid @border_color; "
            "}"
        );
    }
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        gl_css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
    auto header_css_provider = Gtk::CssProvider::create();
    bool header_css_loaded = false;
    
    try {
        auto css_path = Glib::build_filename(executable_dir, "..", "share", "solvespace", "css", "header.css");
        
        if (Glib::file_test(css_path, Glib::FileTest::EXISTS)) {
            header_css_provider->load_from_path(css_path);
            header_css_loaded = true;
            dbp("Loaded header CSS from file: %s", css_path.c_str());
        }
    } catch (const Glib::Error& e) {
        dbp("Error loading header CSS from file: %s", e.what().c_str());
    }
    
    if (!header_css_loaded) {
        dbp("Using embedded header CSS fallback");
        header_css_provider->load_from_data(
            "headerbar { "
            "   padding: 4px; "
            "   background-image: none; "
            "   background-color: @header_bg; "
            "   border-bottom: 1px solid @header_border; "
            "}"
            ".dark headerbar { "
            "   background-color: @dark_header_bg; "
            "   border-bottom: 1px solid @dark_header_border; "
            "   color: @dark_fg_color; "
            "}"
        );
    }
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        header_css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
    auto button_css_provider = Gtk::CssProvider::create();
    bool button_css_loaded = false;
    
    try {
        auto css_path = Glib::build_filename(executable_dir, "..", "share", "solvespace", "css", "button.css");
        
        if (Glib::file_test(css_path, Glib::FileTest::EXISTS)) {
            button_css_provider->load_from_path(css_path);
            button_css_loaded = true;
            dbp("Loaded button CSS from file: %s", css_path.c_str());
        }
    } catch (const Glib::Error& e) {
        dbp("Error loading button CSS from file: %s", e.what().c_str());
    }
    
    if (!button_css_loaded) {
        dbp("Using embedded button CSS fallback");
        button_css_provider->load_from_data(
            "button.menu-button { "
            "   margin: 2px; "
            "   padding: 4px 8px; "
            "   border-radius: 3px; "
            "   transition: background-color 200ms ease; "
            "}"
            "button.menu-button:hover { "
            "   background-color: @button_hover; "
            "}"
            ".dark button.menu-button { "
            "   color: @dark_fg_color; "
            "}"
            ".dark button.menu-button:hover { "
            "   background-color: @dark_button_hover; "
            "}"
        );
    }
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        button_css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
    auto dialog_css_provider = Gtk::CssProvider::create();
    bool dialog_css_loaded = false;
    
    try {
        auto css_path = Glib::build_filename(executable_dir, "..", "share", "solvespace", "css", "dialog.css");
        
        if (Glib::file_test(css_path, Glib::FileTest::EXISTS)) {
            dialog_css_provider->load_from_path(css_path);
            dialog_css_loaded = true;
            dbp("Loaded dialog CSS from file: %s", css_path.c_str());
        }
    } catch (const Glib::Error& e) {
        dbp("Error loading dialog CSS from file: %s", e.what().c_str());
    }
    
    if (!dialog_css_loaded) {
        dbp("Using embedded dialog CSS fallback");
        dialog_css_provider->load_from_data(
        "dialog.solvespace-file-dialog { "
        "   border-radius: 4px; "
        "   padding: 8px; "
        "   background-color: @bg_color; "
        "   color: @fg_color; "
        "}"
        ".dark dialog.solvespace-file-dialog { "
        "   background-color: @dark_bg_color; "
        "   color: @dark_fg_color; "
        "   border: 1px solid @dark_border_color; "
        "}"
        "dialog.solvespace-file-dialog button { "
        "   border-radius: 3px; "
        "   padding: 6px 12px; "
        "}"
        "dialog.solvespace-file-dialog button.suggested-action { "
        "   background-color: @accent_color; "
        "   color: @accent_fg; "
        "}"
    );
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        dialog_css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
    auto button_action_css_provider = Gtk::CssProvider::create();
    button_action_css_provider->load_from_data(
        "dialog.solvespace-file-dialog button.destructive-action { "
        "   background-color: @bg_color; "
        "   color: @fg_color; "
        "}"
    );
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        button_action_css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
    auto editor_css_provider = Gtk::CssProvider::create();
    
    #include "css/editor_overlay.css.h"
    editor_css_provider->load_from_data(editor_overlay_css);
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        editor_css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
    auto media_css_provider = Gtk::CssProvider::create();
    media_css_provider->load_from_data(
        "@media (prefers-dark-theme) {"
        "   window.solvespace-window { "
        "       background-color: @dark_bg_color; "
        "       color: @dark_fg_color; "
        "   }"
        "   headerbar { "
        "       background-color: @dark_header_bg; "
        "       border-bottom: 1px solid @dark_header_border; "
        "   }"
        "   button.menu-button:hover { "
        "       background-color: @dark_button_hover; "
        "   }"
        "   dialog.solvespace-file-dialog { "
        "       background-color: @dark_bg_color; "
        "       color: @dark_fg_color; "
        "   }"
        "   dialog.solvespace-file-dialog button.suggested-action { "
        "       background-color: @dark_accent_color; "
        "       color: @dark_accent_fg; "
        "   }"
        "   dialog.solvespace-file-dialog button.destructive-action { "
        "       background-color: @dark_bg_color; "
        "       color: @dark_fg_color; "
        "   }"
        "   entry.editor-text { "
        "       background-color: @dark_entry_bg; "
        "       color: @dark_entry_fg; "
        "       caret-color: @dark_accent_color; "
        "       selection-background-color: alpha(@dark_accent_color, 0.3); "
        "       selection-color: @dark_entry_fg; "
        "   }"
        "}"
    );
    
    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        media_css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        css_provider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

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


    auto settings = Gtk::Settings::get_default();
    
    auto theme_binding = Gtk::PropertyExpression<bool>::create(
        settings->get_type(), 
        nullptr, 
        "gtk-application-prefer-dark-theme");
    
    Gtk::Window* window_ptr = &window;
    theme_binding->connect([window_ptr, settings]() {
        bool dark_theme = settings->property_gtk_application_prefer_dark_theme();
        if (dark_theme) {
            window_ptr->add_css_class("dark");
        } else {
            window_ptr->remove_css_class("dark");
        }
    });
    
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

    if (settings) {
        settings->property_gtk_application_prefer_dark_theme().signal_changed().connect(
            [](){
                SS.GenerateAll(SolveSpaceUI::Generate::ALL);
                SS.GW.Invalidate();
            });
    }

    std::set<std::string> rtl_languages = {"ar", "he", "fa", "ur", "dv", "ha", "khw", "ks", "ku", "ps", "sd", "ug", "yi"};

    std::string lang = Glib::get_locale();
    if (lang.length() >= 2) {
        std::string lang_code = lang.substr(0, 2);
        bool is_rtl = rtl_languages.find(lang_code) != rtl_languages.end();

        if (is_rtl) {
            Gtk::Window *window = dynamic_cast<Gtk::Window*>(gtkApp->get_active_window());
            if (window) {
                window->set_property("text-direction", "rtl");
            }
        }
    }

    std::vector<std::string> args;
    for (int i = 0; i < argc; i++) {
        args.push_back(argv[i]);
    }

    auto help_shortcut_controller = Gtk::ShortcutController::create();
    help_shortcut_controller->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);

    auto help_action = Gtk::CallbackAction::create([](Gtk::Widget&, const Glib::VariantBase&) {
        dbp("Help requested");
        return true;
    });

    auto help_shortcut = Gtk::Shortcut::create(
        Gtk::KeyvalTrigger::create(GDK_KEY_F1),
        help_action
    );
    help_shortcut = Gtk::Shortcut::create(help_shortcut->get_trigger(), help_action);
    help_shortcut_controller->add_shortcut(help_shortcut);


    style_provider->load_from_data(R"(
    /* CSS Variables for theming */
    :root {
        --bg-color: #f5f5f5;
        --header-bg-color: #e0e0e0;
        --header-border-color: #d0d0d0;
        --text-color: #333333;
        --hover-bg-color: rgba(0, 0, 0, 0.1);
        --entry-bg-color: #ffffff;
        --entry-text-color: #000000;
        --button-bg-color: #e0e0e0;
        --button-hover-bg-color: #d0d0d0;
        --button-active-bg-color: #c0c0c0;
        --link-color: #0066cc;
    }

    /* Dark mode variables */
    .dark {
        --bg-color: #2d2d2d;
        --header-bg-color: #1d1d1d;
        --header-border-color: #3d3d3d;
        --text-color: #e0e0e0;
        --hover-bg-color: rgba(255, 255, 255, 0.1);
        --entry-bg-color: #3d3d3d;
        --entry-text-color: #e0e0e0;
        --button-bg-color: #3d3d3d;
        --button-hover-bg-color: #4d4d4d;
        --button-active-bg-color: #5d5d5d;
        --link-color: #5599ff;
    }

    /* Base window styling */
    window {
        background-color: var(--bg-color);
        color: var(--text-color);
    }

    headerbar {
        background-color: var(--header-bg-color);
        border-bottom: 1px solid var(--header-border-color);
        padding: 4px;
        color: var(--text-color);
    }

    /* Menu styling */
    .menu-button {
        padding: 4px 8px;
        margin: 2px;
        border-radius: 4px;
        background-color: var(--button-bg-color);
        color: var(--text-color);
    }

    .menu-button:hover {
        background-color: var(--button-hover-bg-color);
    }

    .menu-button:active {
        background-color: var(--button-active-bg-color);
    }

    .menu-item {
        padding: 6px 8px;
        margin: 1px;
        color: var(--text-color);
    }

    .menu-item:hover {
        background-color: var(--hover-bg-color);
    }

    /* GL area styling - not affected by dark mode */
    .solvespace-gl-area {
        background-color: #ffffff;
    }

    /* Editor overlay styling */
    .editor-overlay {
        background-color: transparent;
    }

    /* Base entry styling */
    entry {
        background-color: var(--entry-bg-color);
        color: var(--entry-text-color);
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

    auto platformSettings = GetSettings();
    std::string savedLocale = platformSettings->ThawString("locale", "");

    if(!savedLocale.empty()) {
        if(!SetLocale(savedLocale)) {
            dbp("Failed to set saved locale: %s", savedLocale.c_str());
            const char* const* langNames = g_get_language_names();
            while(*langNames) {
                if(SetLocale(*langNames++)) break;
            }
            if(!*langNames) {
                SetLocale("en_US");
            }
        } else {
            dbp("Using saved locale: %s", savedLocale.c_str());
        }
    } else {
        const char* const* langNames = g_get_language_names();
        while(*langNames) {
            if(SetLocale(*langNames++)) break;
        }
        if(!*langNames) {
            SetLocale("en_US");
        }
    }

    return args;
}

static void RunGui();
static void ExitGui();
static void ClearGui();

static void RunGui() {
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

    InitClipboard();
    InitColorPicker();

    if (!gtkApp->is_registered()) {
        gtkApp->register_application();

        gtkApp->hold();

        if (settings) {
            bool dark_theme = false;
            settings->get_property("gtk-application-prefer-dark-theme", dark_theme);
            dbp("Initial theme: %s", dark_theme ? "dark" : "light");

            settings->property_gtk_application_prefer_dark_theme().signal_changed().connect(
                [settings]() {
                    bool dark_theme = false;
                    settings->get_property("gtk-application-prefer-dark-theme", dark_theme);
                    dbp("Theme changed: %s", dark_theme ? "dark" : "light");

                    auto windows = Gtk::Window::list_toplevels();
                    for (auto window : windows) {
                        if (dark_theme) {
                            window->add_css_class("dark");
                        } else {
                            window->remove_css_class("dark");
                        }
                    }

                    SS.GenerateAll(SolveSpaceUI::Generate::ALL);
                    SS.GW.Invalidate();
                });
        }

        gtkApp->run();
    }
}

static void ExitGui() {
    if(gtkApp) {
        gtkApp->quit();
    }
}

static void ClearGui() {
    gtkApp.reset();
}

    return args;
}

} // namespace Platform
} // namespace SolveSpace
