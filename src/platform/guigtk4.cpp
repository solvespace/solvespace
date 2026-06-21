//-----------------------------------------------------------------------------
// The GTK 4-based implementation of platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
// Copyright 2025 (GTK 4 port)
//-----------------------------------------------------------------------------
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <json-c/json_object.h>
#include <json-c/json_util.h>
#include <glibmm/convert.h>
#include <glibmm/main.h>
#include <giomm/menu.h>
#include <giomm/menuitem.h>
#include <giomm/simpleaction.h>
#include <giomm/simpleactiongroup.h>
#include <gtkmm/application.h>
#include <gtkmm/box.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/entry.h>
#include <gtkmm/eventcontrollerkey.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/fixed.h>
#include <gtkmm/overlay.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/glarea.h>
#include <gtkmm/popovermenu.h>
#include <gtkmm/popovermenubar.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/scrollbar.h>
#include <gtkmm/settings.h>
#include <gtkmm/tooltip.h>
#include <gtkmm/urilauncher.h>
#include <gtkmm/window.h>
#include <gtkmm/alertdialog.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/filefilter.h>
#include <gdkmm/monitor.h>
#include <giomm/liststore.h>

#if defined(HAVE_SPACEWARE)
#   include <spnav.h>
#   include <gdk/gdk.h>
#   if defined(GDK_WINDOWING_X11)
#       include <gdk/x11/gdkx.h>
#   endif
#   if defined(GDK_WINDOWING_WAYLAND)
#       include <gdk/wayland/gdkwayland.h>
#   endif
#   include <gdkmm/seat.h>
#endif

#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

static Glib::RefPtr<Gtk::Application> gtkApp;
static bool gtkAppStarted;
static int gtkArgc;
static char **gtkArgv;

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

static int actionCounter = 0;

static std::string MakeActionName() {
    return ssprintf("mi_%d", actionCounter++);
}

static guint KeyboardEventToGdkKey(const KeyboardEvent &accel) {
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
    return accelKey;
}

static GdkModifierType GetModifiersFromController(Gtk::EventController &ctrl) {
    auto event = ctrl.get_current_event();
    if(event) {
        return (GdkModifierType)event->get_modifier_state();
    }
    return (GdkModifierType)0;
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

        auto handler = [this]() {
            if(this->onTimeout) {
                this->onTimeout();
            }
            return false;
        };
        _connection = Glib::signal_timeout().connect(handler, milliseconds);
    }
};

TimerRef CreateTimer() {
    return std::make_shared<TimerImplGtk>();
}

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

class MenuImplGtk4;

class MenuItemImplGtk4 final : public MenuItem {
public:
    Glib::RefPtr<Gio::SimpleAction>  action;
    std::string                      actionName;
    Indicator                        indicator = Indicator::NONE;

    MenuItemImplGtk4() {
        actionName = MakeActionName();
        action = Gio::SimpleAction::create(actionName);
        action->signal_activate().connect([this](const Glib::VariantBase &) {
            if(indicator != Indicator::NONE) {
                bool current = false;
                action->get_state(current);
                action->change_state(Glib::Variant<bool>::create(!current));
            }
            if(onTrigger) {
                onTrigger();
            }
        });
    }

    void SetAccelerator(KeyboardEvent accel) override {
        guint accelKey = KeyboardEventToGdkKey(accel);
        if(accelKey == 0) return;

        GdkModifierType accelMods = (GdkModifierType)0;
        if(accel.shiftDown) {
            accelMods = (GdkModifierType)(accelMods | GDK_SHIFT_MASK);
        }
        if(accel.controlDown) {
            accelMods = (GdkModifierType)(accelMods | GDK_CONTROL_MASK);
        }

        std::string accelStr = gtk_accelerator_name(accelKey, accelMods);
        if(gtkApp) {
            gtkApp->set_accels_for_action(
                "ss." + actionName, { accelStr });
        }
    }

    void SetIndicator(Indicator type) override {
        indicator = type;
        if(type != Indicator::NONE) {
            action = Gio::SimpleAction::create_bool(actionName, false);
            action->signal_activate().connect([this](const Glib::VariantBase &) {
                if(indicator != Indicator::NONE) {
                    bool current = false;
                    action->get_state(current);
                    action->change_state(Glib::Variant<bool>::create(!current));
                }
                if(onTrigger) {
                    onTrigger();
                }
            });
        }
    }

    void SetActive(bool active) override {
        ssassert(indicator != Indicator::NONE,
                 "Cannot change state of a menu item without indicator");
        action->change_state(Glib::Variant<bool>::create(active));
    }

    void SetEnabled(bool enabled) override {
        action->set_enabled(enabled);
    }
};

class MenuImplGtk4 final : public Menu {
public:
    Glib::RefPtr<Gio::Menu>                            gioMenu;
    Glib::RefPtr<Gio::SimpleActionGroup>               actionGroup;
    std::vector<std::shared_ptr<MenuItemImplGtk4>>     menuItems;
    std::vector<std::shared_ptr<MenuImplGtk4>>         subMenus;
    Glib::RefPtr<Gio::Menu>                            currentSection;

    MenuImplGtk4() {
        gioMenu = Gio::Menu::create();
        actionGroup = Gio::SimpleActionGroup::create();
        currentSection = Gio::Menu::create();
        gioMenu->append_section("", currentSection);
    }

    MenuItemRef AddItem(const std::string &label,
                        std::function<void()> onTrigger = std::function<void()>(),
                        bool mnemonics = true) override {
        auto menuItem = std::make_shared<MenuItemImplGtk4>();
        menuItems.push_back(menuItem);

        menuItem->onTrigger = onTrigger;
        actionGroup->add_action(menuItem->action);

        std::string menuLabel = mnemonics ? PrepareMnemonics(label) : label;
        currentSection->append(menuLabel, "ss." + menuItem->actionName);

        return menuItem;
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplGtk4>();
        subMenus.push_back(subMenu);

        currentSection->append_submenu(PrepareMnemonics(label), subMenu->gioMenu);

        return subMenu;
    }

    void AddSeparator() override {
        currentSection = Gio::Menu::create();
        gioMenu->append_section("", currentSection);
    }

    void PopUp() override {
        auto windows = gtkApp->get_windows();
        Gtk::Window *parent = nullptr;
        for(auto *w : windows) {
            if(w->is_active()) {
                parent = w;
                break;
            }
        }
        if(!parent && !windows.empty()) {
            parent = windows[0];
        }
        if(!parent) return;

        InsertActionsInto(parent);

        auto popover = Gtk::make_managed<Gtk::PopoverMenu>();
        popover->set_menu_model(gioMenu);
        popover->set_parent(*parent);
        popover->set_has_arrow(false);

        Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
        popover->signal_closed().connect([&]() { loop->quit(); });

        popover->popup();
        loop->run();

        popover->unparent();
    }

    void InsertActionsInto(Gtk::Widget *widget) {
        widget->insert_action_group("ss", actionGroup);
        for(auto &sub : subMenus) {
            sub->InsertActionsInto(widget);
        }
    }

    void Clear() override {
        gioMenu->remove_all();
        currentSection = Gio::Menu::create();
        gioMenu->append_section("", currentSection);
        menuItems.clear();
        subMenus.clear();
        actionGroup = Gio::SimpleActionGroup::create();
    }
};

MenuRef CreateMenu() {
    return std::make_shared<MenuImplGtk4>();
}

class MenuBarImplGtk4 final : public MenuBar {
public:
    Glib::RefPtr<Gio::Menu>                        gioMenu;
    Glib::RefPtr<Gio::SimpleActionGroup>           actionGroup;
    std::vector<std::shared_ptr<MenuImplGtk4>>     subMenus;

    MenuBarImplGtk4() {
        gioMenu = Gio::Menu::create();
        actionGroup = Gio::SimpleActionGroup::create();
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplGtk4>();
        subMenus.push_back(subMenu);

        gioMenu->append_submenu(PrepareMnemonics(label), subMenu->gioMenu);

        return subMenu;
    }

    void Clear() override {
        gioMenu->remove_all();
        subMenus.clear();
        actionGroup = Gio::SimpleActionGroup::create();
    }

    void CollectActions(const std::shared_ptr<MenuImplGtk4> &menu) {
        for(auto &item : menu->menuItems) {
            actionGroup->add_action(item->action);
        }
        for(auto &sub : menu->subMenus) {
            CollectActions(sub);
        }
    }

    void InsertActionsInto(Gtk::Widget *widget) {
        for(auto &sub : subMenus) {
            CollectActions(sub);
        }
        widget->insert_action_group("ss", actionGroup);
    }
};

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    *unique = false;
    return std::make_shared<MenuBarImplGtk4>();
}

//-----------------------------------------------------------------------------
// GTK 4 GL and window extensions
//-----------------------------------------------------------------------------

class Gtk4GLWidget : public Gtk::GLArea {
    Window *_receiver;
    Glib::RefPtr<Gtk::EventControllerMotion> _motionCtrl;
    Glib::RefPtr<Gtk::GestureClick>          _clickGesture;
    Glib::RefPtr<Gtk::EventControllerScroll> _scrollCtrl;
    Glib::RefPtr<Gtk::EventControllerKey>    _keyCtrl;

public:
    Gtk4GLWidget(Platform::Window *receiver) : _receiver(receiver) {
        set_has_depth_buffer(true);
        set_focusable(true);
        set_can_focus(true);

        _motionCtrl = Gtk::EventControllerMotion::create();
        _motionCtrl->signal_motion().connect(
            [this](double x, double y) { OnMotion(x, y); });
        _motionCtrl->signal_leave().connect(
            [this]() { OnLeave(); });
        add_controller(_motionCtrl);

        _clickGesture = Gtk::GestureClick::create();
        _clickGesture->set_button(0);
        _clickGesture->signal_pressed().connect(
            [this](int n, double x, double y) { OnPressed(n, x, y); });
        _clickGesture->signal_released().connect(
            [this](int n, double x, double y) { OnReleased(n, x, y); });
        add_controller(_clickGesture);

        _scrollCtrl = Gtk::EventControllerScroll::create();
        _scrollCtrl->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL |
                               Gtk::EventControllerScroll::Flags::DISCRETE);
        _scrollCtrl->signal_scroll().connect(
            [this](double dx, double dy) -> bool { return OnScroll(dx, dy); }, false);
        add_controller(_scrollCtrl);

        _keyCtrl = Gtk::EventControllerKey::create();
        _keyCtrl->signal_key_pressed().connect(
            [this](guint kv, guint kc, Gdk::ModifierType s) -> bool {
                return OnKeyPressed(kv, kc, s);
            }, false);
        _keyCtrl->signal_key_released().connect(
            [this](guint kv, guint kc, Gdk::ModifierType s) {
                OnKeyReleased(kv, kc, s);
            });
        add_controller(_keyCtrl);
    }

protected:
    bool on_render(const Glib::RefPtr<Gdk::GLContext> &context) override {
        if(_receiver->onRender) {
            _receiver->onRender();
        }
        return true;
    }

    bool ProcessPointerEvent(MouseEvent::Type type, double x, double y,
                             GdkModifierType state, guint button = 0,
                             double scrollDelta = 0) {
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
        if(scrollDelta != 0) {
            event.scrollDelta = scrollDelta;
        }

        if(_receiver->onMouseEvent) {
            return _receiver->onMouseEvent(event);
        }

        return false;
    }

    void OnMotion(double x, double y) {
        GdkModifierType state = GetModifiersFromController(*_motionCtrl);
        ProcessPointerEvent(MouseEvent::Type::MOTION, x, y, state);
    }

    void OnLeave() {
        GdkModifierType state = GetModifiersFromController(*_motionCtrl);
        ProcessPointerEvent(MouseEvent::Type::LEAVE, 0, 0, state);
    }

    void OnPressed(int nPress, double x, double y) {
        GdkModifierType state = GetModifiersFromController(*_clickGesture);
        guint button = _clickGesture->get_current_button();

        MouseEvent::Type type = (nPress >= 2) ? MouseEvent::Type::DBL_PRESS
                                              : MouseEvent::Type::PRESS;
        ProcessPointerEvent(type, x, y, state, button);
    }

    void OnReleased(int nPress, double x, double y) {
        GdkModifierType state = GetModifiersFromController(*_clickGesture);
        guint button = _clickGesture->get_current_button();
        ProcessPointerEvent(MouseEvent::Type::RELEASE, x, y, state, button);
    }

    bool OnScroll(double dx, double dy) {
        GdkModifierType state = GetModifiersFromController(*_scrollCtrl);
        double delta = -dy;
        if(delta == 0) return false;
        return ProcessPointerEvent(MouseEvent::Type::SCROLL_VERT, 0, 0, state, 0, delta);
    }

    bool ProcessKeyEvent(KeyboardEvent::Type type, guint keyval, GdkModifierType state) {
        KeyboardEvent event = {};
        event.type = type;

        GdkModifierType modMask = (GdkModifierType)(GDK_SHIFT_MASK | GDK_CONTROL_MASK |
                                                     GDK_ALT_MASK | GDK_SUPER_MASK);
        if((state & modMask) & ~(GDK_SHIFT_MASK | GDK_CONTROL_MASK)) {
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

    bool OnKeyPressed(guint keyval, guint keycode, Gdk::ModifierType state) {
        return ProcessKeyEvent(KeyboardEvent::Type::PRESS, keyval, (GdkModifierType)state);
    }

    void OnKeyReleased(guint keyval, guint keycode, Gdk::ModifierType state) {
        ProcessKeyEvent(KeyboardEvent::Type::RELEASE, keyval, (GdkModifierType)state);
    }
};

//-----------------------------------------------------------------------------
// Editor overlay
//-----------------------------------------------------------------------------

class Gtk4EditorOverlay : public Gtk::Overlay {
    Window       *_receiver;
    Gtk4GLWidget  _gl_widget;
    Gtk::Fixed    _fixed;
    Gtk::Entry    _entry;

public:
    Gtk4EditorOverlay(Platform::Window *receiver)
        : _receiver(receiver), _gl_widget(receiver) {
        _gl_widget.set_hexpand(true);
        _gl_widget.set_vexpand(true);
        set_child(_gl_widget);

        _entry.set_visible(false);
        _entry.set_has_frame(false);
        _entry.set_halign(Gtk::Align::START);
        _entry.set_valign(Gtk::Align::START);
        _fixed.put(_entry, 0, 0);
        _fixed.set_can_target(false);
        add_overlay(_fixed);

        _entry.signal_activate().connect([this]() { OnActivate(); });

        auto keyCtrl = Gtk::EventControllerKey::create();
        keyCtrl->signal_key_pressed().connect(
            [this](guint kv, guint kc, Gdk::ModifierType s) -> bool {
                return OnEntryKeyPressed(kv, kc, s);
            }, false);
        _entry.add_controller(keyCtrl);
    }

    bool is_editing() const {
        return _entry.is_visible();
    }

    void start_editing(int x, int y, int font_height, int min_width, bool is_monospace,
                       const std::string &val) {
        std::string fontFamily = is_monospace ? "monospace" : "sans-serif";
        std::string css = ssprintf("entry { font-family: %s; font-size: %dpx; }",
                                   fontFamily.c_str(), font_height);
        auto cssProvider = Gtk::CssProvider::create();
        cssProvider->load_from_data(css);
        auto display = Gdk::Display::get_default();
        Gtk::StyleContext::add_provider_for_display(
            display, cssProvider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);

        Pango::FontDescription font_desc;
        font_desc.set_family(fontFamily);
        font_desc.set_absolute_size(font_height * Pango::SCALE);
        Pango::FontMetrics font_metrics = get_pango_context()->get_metrics(font_desc);
        y -= font_metrics.get_ascent() / Pango::SCALE;

        Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create(get_pango_context());
        layout->set_font_description(font_desc);
        layout->set_text(val + " ");
        int width = layout->get_logical_extents().get_width();

        _fixed.move(_entry, x, y);

        int fitWidth = width / Pango::SCALE;
        _entry.set_size_request(max(fitWidth, min_width), -1);

        _entry.set_text(val);

        if(!_entry.is_visible()) {
            _entry.set_visible(true);
            _entry.grab_focus();
        }
    }

    void stop_editing() {
        if(_entry.is_visible()) {
            _entry.set_visible(false);
            _gl_widget.grab_focus();
        }
    }

    Gtk4GLWidget &get_gl_widget() {
        return _gl_widget;
    }

protected:
    bool OnEntryKeyPressed(guint keyval, guint keycode, Gdk::ModifierType state) {
        if(keyval == GDK_KEY_Escape) {
            stop_editing();
            return true;
        }
        return false;
    }

    void OnActivate() {
        if(_receiver->onEditingDone) {
            _receiver->onEditingDone(_entry.get_text());
        }
    }
};

//-----------------------------------------------------------------------------
// Window
//-----------------------------------------------------------------------------

class Gtk4Window : public Gtk::Window {
    Platform::Window    *_receiver;
    Gtk::Box             _vbox;
    Gtk::PopoverMenuBar *_menu_bar = nullptr;
    Gtk::Box             _hbox;
    Gtk4EditorOverlay    _editor_overlay;
    Gtk::Scrollbar       _scrollbar;
    bool                 _is_under_cursor = false;
    bool                 _is_fullscreen = false;
    std::string          _tooltip_text;
    Gdk::Rectangle       _tooltip_area;

public:
    Gtk4Window(Platform::Window *receiver)
        : _receiver(receiver),
          _vbox(Gtk::Orientation::VERTICAL),
          _hbox(Gtk::Orientation::HORIZONTAL),
          _editor_overlay(receiver),
          _scrollbar(Gtk::Adjustment::create(0, 0, 0), Gtk::Orientation::VERTICAL) {
        _editor_overlay.set_hexpand(true);
        _editor_overlay.set_vexpand(true);
        _hbox.append(_editor_overlay);
        _hbox.append(_scrollbar);
        _scrollbar.set_visible(false);

        _hbox.set_hexpand(true);
        _hbox.set_vexpand(true);
        _vbox.append(_hbox);
        set_child(_vbox);

        _scrollbar.get_adjustment()->signal_value_changed().connect(
            [this]() { OnScrollbarValueChanged(); });

        get_gl_widget().set_has_tooltip(true);
        get_gl_widget().signal_query_tooltip().connect(
            [this](int x, int y, bool kb,
                   const Glib::RefPtr<Gtk::Tooltip> &tip) -> bool {
                return OnQueryTooltip(x, y, kb, tip);
            }, false);

        auto motionCtrl = Gtk::EventControllerMotion::create();
        motionCtrl->signal_enter().connect(
            [this](double x, double y) { _is_under_cursor = true; });
        motionCtrl->signal_leave().connect(
            [this]() { _is_under_cursor = false; });
        add_controller(motionCtrl);

        signal_close_request().connect(
            [this]() -> bool { return OnCloseRequest(); }, false);

        property_fullscreened().signal_changed().connect(
            [this]() { OnFullscreenChanged(); });
    }

    bool is_full_screen() const {
        return _is_fullscreen;
    }

    Gtk::PopoverMenuBar *get_menu_bar() const {
        return _menu_bar;
    }

    void set_menu_bar(Gtk::PopoverMenuBar *menu_bar) {
        if(_menu_bar) {
            _vbox.remove(*_menu_bar);
        }
        _menu_bar = menu_bar;
        if(_menu_bar) {
            _vbox.prepend(*_menu_bar);
        }
    }

    Gtk4EditorOverlay &get_editor_overlay() {
        return _editor_overlay;
    }

    Gtk4GLWidget &get_gl_widget() {
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
    bool OnQueryTooltip(int x, int y, bool keyboard_tooltip,
                        const Glib::RefPtr<Gtk::Tooltip> &tooltip) {
        tooltip->set_text(_tooltip_text);
        tooltip->set_tip_area(_tooltip_area);
        return !_tooltip_text.empty() && (keyboard_tooltip || _is_under_cursor);
    }

    bool OnCloseRequest() {
        if(_receiver->onClose) {
            _receiver->onClose();
            return true;
        }
        return false;
    }

    void OnFullscreenChanged() {
        _is_fullscreen = is_fullscreen();
        if(_receiver->onFullScreen) {
            _receiver->onFullScreen(_is_fullscreen);
        }
    }

    void OnScrollbarValueChanged() {
        if(_receiver->onScrollbarAdjusted) {
            _receiver->onScrollbarAdjusted(_scrollbar.get_adjustment()->get_value());
        }
    }
};

//-----------------------------------------------------------------------------
// Windows
//-----------------------------------------------------------------------------

class WindowImplGtk4 final : public Window {
public:
    Gtk4Window      gtkWindow;
    MenuBarRef      menuBar;

    WindowImplGtk4(Window::Kind kind) : gtkWindow(this) {
        if(kind == Kind::TOOL) {
            gtkWindow.set_decorated(true);
        }

        gtk_window_set_default_icon_name("solvespace");
    }

    double GetPixelDensity() override {
        auto display = gtkWindow.get_display();
        auto surface = gtkWindow.get_surface();
        if(surface) {
            auto monitor = display->get_monitor_at_surface(surface);
            if(monitor) {
                Gdk::Rectangle geom;
                monitor->get_geometry(geom);
                int widthMm = monitor->get_width_mm();
                if(widthMm > 0) {
                    return (double)geom.get_width() / ((double)widthMm / 25.4);
                }
            }
        }
        return 96.0;
    }

    double GetDevicePixelRatio() override {
        return gtkWindow.get_scale_factor();
    }

    bool IsVisible() override {
        return gtkWindow.is_visible();
    }

    void SetVisible(bool visible) override {
        if(gtkAppStarted) {
            if(visible) {
                gtkApp->add_window(gtkWindow);
            } else {
                gtkApp->remove_window(gtkWindow);
            }
        }
        gtkWindow.set_visible(visible);
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
            auto *menuBarImpl = (MenuBarImplGtk4 *)&*newMenuBar;
            auto *popoverMenuBar = Gtk::make_managed<Gtk::PopoverMenuBar>(
                menuBarImpl->gioMenu);
            gtkWindow.set_menu_bar(popoverMenuBar);
            menuBarImpl->InsertActionsInto(&gtkWindow);
        } else {
            gtkWindow.set_menu_bar(nullptr);
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

        int width = gtkWindow.get_width();
        int height = gtkWindow.get_height();
        bool isMaximized = gtkWindow.is_maximized();

        settings->FreezeInt(key + "_Width",      width);
        settings->FreezeInt(key + "_Height",     height);
        settings->FreezeBool(key + "_Maximized", isMaximized);
    }

    void ThawPosition(SettingsRef settings, const std::string &key) override {
        int width  = settings->ThawInt(key + "_Width",  720);
        int height = settings->ThawInt(key + "_Height", 540);

        gtkWindow.set_default_size(width, height);

        if(settings->ThawBool(key + "_Maximized", false)) {
            gtkWindow.maximize();
        }
    }

    void SetCursor(Cursor cursor) override {
        std::string cursor_name;
        switch(cursor) {
            case Cursor::POINTER: cursor_name = "default"; break;
            case Cursor::HAND:    cursor_name = "pointer"; break;
            default: ssassert(false, "Unexpected cursor");
        }
        gtkWindow.get_gl_widget().set_cursor(
            Gdk::Cursor::create(cursor_name));
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
        gtkWindow.get_scrollbar().set_visible(visible);
    }

    void ConfigureScrollbar(double min, double max, double pageSize) override {
        auto adjustment = gtkWindow.get_scrollbar().get_adjustment();
        adjustment->configure(adjustment->get_value(), min, max, 1, 4, pageSize);
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
    auto window = std::make_shared<WindowImplGtk4>(kind);
    if(parentWindow) {
        window->gtkWindow.set_transient_for(
            std::static_pointer_cast<WindowImplGtk4>(parentWindow)->gtkWindow);
    }
    return window;
}

//-----------------------------------------------------------------------------
// 3DConnexion support
//-----------------------------------------------------------------------------

void Open3DConnexion() {}
void Close3DConnexion() {}

#if defined(HAVE_SPACEWARE) && (defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND))
static void ProcessSpnavEvent(WindowImplGtk4 *window, const spnav_event &spnavEvent,
                              bool shiftDown, bool controlDown) {
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

static gboolean ConsumeSpnavQueue(GIOChannel *, GIOCondition, gpointer data) {
    WindowImplGtk4 *window = (WindowImplGtk4 *)data;

    bool shiftDown = false;
    bool controlDown = false;

    spnav_event spnavEvent;
    while(spnav_poll_event(&spnavEvent)) {
        ProcessSpnavEvent(window, spnavEvent, shiftDown, controlDown);
    }
    return TRUE;
}

void Request3DConnexionEventsForWindow(WindowRef window) {
    std::shared_ptr<WindowImplGtk4> windowImpl =
        std::static_pointer_cast<WindowImplGtk4>(window);

    if(spnav_open() != -1) {
        g_io_add_watch(g_io_channel_unix_new(spnav_fd()), G_IO_IN,
                       ConsumeSpnavQueue, windowImpl.get());
    }
}
#else
void Request3DConnexionEventsForWindow(WindowRef window) {}
#endif

//-----------------------------------------------------------------------------
// Message dialogs
//-----------------------------------------------------------------------------

class MessageDialogImplGtk4 final : public MessageDialog {
public:
    Gtk::Window        &gtkParent;
    Type                type = Type::INFORMATION;
    std::string         title;
    std::string         message;
    std::string         description;
    std::vector<std::pair<std::string, Response>> buttons;
    Response            defaultResponse = Response::NONE;

    MessageDialogImplGtk4(Gtk::Window &parent)
        : gtkParent(parent) {
        title = "Message";
    }

    void SetType(Type type) override {
        this->type = type;
    }

    void SetTitle(std::string title) override {
        this->title = PrepareTitle(title);
    }

    void SetMessage(std::string message) override {
        this->message = message;
    }

    void SetDescription(std::string description) override {
        this->description = description;
    }

    void AddButton(std::string label, Response response, bool isDefault) override {
        buttons.push_back({ label, response });
        if(isDefault) {
            defaultResponse = response;
        }
    }

    Response RunModal() override {
        auto dialog = Gtk::AlertDialog::create();
        dialog->set_message(message);
        dialog->set_detail(description);

        std::vector<Glib::ustring> labels;
        for(auto &btn : buttons) {
            labels.push_back(PrepareMnemonics(btn.first));
        }
        dialog->set_buttons(labels);

        for(size_t i = 0; i < buttons.size(); i++) {
            if(buttons[i].second == defaultResponse) {
                dialog->set_default_button(i);
                break;
            }
        }

        for(size_t i = 0; i < buttons.size(); i++) {
            if(buttons[i].second == Response::CANCEL) {
                dialog->set_cancel_button(i);
                break;
            }
        }

        Response result = Response::NONE;
        bool done = false;
        auto loop = Glib::MainLoop::create();

        dialog->choose(gtkParent,
            [&](Glib::RefPtr<Gio::AsyncResult> &asyncResult) {
                try {
                    int idx = dialog->choose_finish(asyncResult);
                    if(idx >= 0 && idx < (int)buttons.size()) {
                        result = buttons[idx].second;
                    }
                } catch(...) {
                    result = Response::NONE;
                }
                done = true;
                loop->quit();
            });

        if(!done) {
            loop->run();
        }

        return result;
    }

    void ShowModal() override {
        Response response = RunModal();
        if(onResponse) {
            onResponse(response);
        }
    }
};

MessageDialogRef CreateMessageDialog(WindowRef parentWindow) {
    return std::make_shared<MessageDialogImplGtk4>(
                std::static_pointer_cast<WindowImplGtk4>(parentWindow)->gtkWindow);
}

//-----------------------------------------------------------------------------
// File dialogs
//-----------------------------------------------------------------------------

class FileDialogImplGtk4 : public FileDialog {
public:
    Gtk::Window                    &gtkParent;
    Glib::RefPtr<Gtk::FileDialog>   gtkFileDialog;
    bool                            isSave;
    std::vector<std::string>        extensions;
    Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>> filterList;
    Platform::Path                  currentPath;
    std::string                     currentName;

    FileDialogImplGtk4(Gtk::Window &parent, bool isSave)
        : gtkParent(parent), isSave(isSave) {
        gtkFileDialog = Gtk::FileDialog::create();
        gtkFileDialog->set_title(PrepareTitle(
            isSave ? C_("title", "Save File") : C_("title", "Open File")));
        gtkFileDialog->set_modal(true);
        filterList = Gio::ListStore<Gtk::FileFilter>::create();
        gtkFileDialog->set_filters(filterList);
    }

    void SetTitle(std::string title) override {
        gtkFileDialog->set_title(PrepareTitle(title));
    }

    void SetCurrentName(std::string name) override {
        currentName = name;
        gtkFileDialog->set_initial_name(name);
    }

    Platform::Path GetFilename() override {
        return currentPath;
    }

    void SetFilename(Platform::Path path) override {
        currentPath = path;
        auto file = Gio::File::create_for_path(path.raw);
        gtkFileDialog->set_initial_file(file);
    }

    void SuggestFilename(Platform::Path path) override {
        std::string ext = GetExtension();
        std::string name = path.FileStem() + "." + ext;
        gtkFileDialog->set_initial_name(name);
    }

    void AddFilter(std::string name, std::vector<std::string> extensions) override {
        auto gtkFilter = Gtk::FileFilter::create();
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
        filterList->append(gtkFilter);
    }

    std::string GetExtension() {
        if(extensions.empty()) return "";
        return extensions.front();
    }

    void FreezeChoices(SettingsRef settings, const std::string &key) override {
        if(!currentPath.IsEmpty()) {
            auto parent = Gio::File::create_for_path(currentPath.raw)->get_parent();
            if(parent) {
                settings->FreezeString("Dialog_" + key + "_Folder", parent->get_path());
            }
        }
        settings->FreezeString("Dialog_" + key + "_Filter", GetExtension());
    }

    void ThawChoices(SettingsRef settings, const std::string &key) override {
        std::string folder = settings->ThawString("Dialog_" + key + "_Folder");
        if(!folder.empty()) {
            auto folderFile = Gio::File::create_for_path(folder);
            gtkFileDialog->set_initial_folder(folderFile);
        }
        std::string ext = settings->ThawString("Dialog_" + key + "_Filter");
        if(!ext.empty()) {
            for(guint i = 0; i < filterList->get_n_items(); i++) {
                if(i < extensions.size() && extensions[i] == ext) {
                    gtkFileDialog->set_default_filter(filterList->get_item(i));
                    break;
                }
            }
        }
    }

    bool RunModal() override {
        bool accepted = false;
        auto loop = Glib::MainLoop::create();

        if(isSave) {
            if(currentName.empty() || Platform::Path::From(currentName).FileStem().empty()) {
                std::string ext = GetExtension();
                if(!ext.empty()) {
                    gtkFileDialog->set_initial_name(
                        std::string(_("untitled")) + "." + ext);
                }
            }
            gtkFileDialog->save(gtkParent,
                [&](Glib::RefPtr<Gio::AsyncResult> &result) {
                    try {
                        auto file = gtkFileDialog->save_finish(result);
                        if(file) {
                            currentPath = Platform::Path::From(file->get_path());
                            accepted = true;
                        }
                    } catch(...) {}
                    loop->quit();
                });
        } else {
            gtkFileDialog->open(gtkParent,
                [&](Glib::RefPtr<Gio::AsyncResult> &result) {
                    try {
                        auto file = gtkFileDialog->open_finish(result);
                        if(file) {
                            currentPath = Platform::Path::From(file->get_path());
                            accepted = true;
                        }
                    } catch(...) {}
                    loop->quit();
                });
        }

        loop->run();
        return accepted;
    }
};

FileDialogRef CreateOpenFileDialog(WindowRef parentWindow) {
    Gtk::Window &gtkParent = std::static_pointer_cast<WindowImplGtk4>(parentWindow)->gtkWindow;
    return std::make_shared<FileDialogImplGtk4>(gtkParent, /*isSave=*/false);
}

FileDialogRef CreateSaveFileDialog(WindowRef parentWindow) {
    Gtk::Window &gtkParent = std::static_pointer_cast<WindowImplGtk4>(parentWindow)->gtkWindow;
    return std::make_shared<FileDialogImplGtk4>(gtkParent, /*isSave=*/true);
}

//-----------------------------------------------------------------------------
// Application-wide APIs
//-----------------------------------------------------------------------------

std::vector<Platform::Path> GetFontFiles() {
    std::vector<Platform::Path> fonts;

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
    auto launcher = Gtk::UriLauncher::create(url);
    launcher->launch([](Glib::RefPtr<Gio::AsyncResult> &) {});
}

std::vector<std::string> InitGui(int argc, char **argv) {
    setlocale(LC_ALL, "");
    if(!Glib::get_charset()) {
        dbp("Sorry, only UTF-8 locales are supported.");
        exit(1);
    }
    setlocale(LC_ALL, "C");

    gtkApp = Gtk::Application::create("com.solvespace.SolveSpace",
                                      Gio::Application::Flags::NON_UNIQUE);
    gtkApp->hold();

    gtkArgc = argc;
    gtkArgv = argv;

    std::vector<std::string> args = InitCli(argc, argv);

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
    gtkApp->signal_activate().connect([]() {
        gtkAppStarted = true;

        Glib::RefPtr<Gtk::CssProvider> style_provider = Gtk::CssProvider::create();
        style_provider->load_from_data(R"(
        entry {
            background: white;
            color: black;
        }
        )");
        Gtk::StyleContext::add_provider_for_display(
            Gdk::Display::get_default(), style_provider,
            600 /*GTK_STYLE_PROVIDER_PRIORITY_APPLICATION*/);

        // Windows created before run() (by SS.Init) need to be registered
        // with the application now that startup has completed.
        auto toplevels = Gtk::Window::list_toplevels();
        for(auto *w : toplevels) {
            gtkApp->add_window(*w);
        }
    });

    gtkApp->run(gtkArgc, gtkArgv);
}

void ExitGui() {
    gtkApp->release();
    gtkApp->quit();
}

void ClearGui() {
    gtkApp.reset();
}

}
}
