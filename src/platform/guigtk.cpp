//-----------------------------------------------------------------------------
// The GTK-based implementation of platform-dependent GUI functionality.
//
// Copyright 2018 whitequark
//-----------------------------------------------------------------------------
#include <glibmm/main.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/menu.h>
#include <gtkmm/menubar.h>
#include "solvespace.h"

namespace SolveSpace {
namespace Platform {

//-----------------------------------------------------------------------------
// Timers
//-----------------------------------------------------------------------------

class TimerImplGtk : public Timer {
public:
    sigc::connection    _connection;

    void WindUp(unsigned milliseconds) override {
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
    return std::unique_ptr<TimerImplGtk>(new TimerImplGtk);
}

//-----------------------------------------------------------------------------
// GTK menu extensions
//-----------------------------------------------------------------------------

class GtkMenuItem : public Gtk::CheckMenuItem {
    Platform::MenuItem *_receiver;
    bool                _has_indicator;
    bool                _synthetic_event;

public:
    GtkMenuItem(Platform::MenuItem *receiver) :
        _receiver(receiver), _has_indicator(false), _synthetic_event(false) {
    }

    void set_accel_key(const Gtk::AccelKey &accel_key) {
        Gtk::CheckMenuItem::set_accel_key(accel_key);
    }

    bool has_indicator() const {
        return _has_indicator;
    }

    void set_has_indicator(bool has_indicator) {
        _has_indicator = has_indicator;
    }

    void set_active(bool active) {
        if(Gtk::CheckMenuItem::get_active() == active)
            return;

        _synthetic_event = true;
        Gtk::CheckMenuItem::set_active(active);
        _synthetic_event = false;
    }

protected:
    void on_activate() override {
        Gtk::CheckMenuItem::on_activate();

        if(!_synthetic_event && _receiver->onTrigger) {
            _receiver->onTrigger();
        }
    }

    void draw_indicator_vfunc(const Cairo::RefPtr<Cairo::Context> &cr) override {
        if(_has_indicator) {
            Gtk::CheckMenuItem::draw_indicator_vfunc(cr);
        }
    }
};

//-----------------------------------------------------------------------------
// Menus
//-----------------------------------------------------------------------------

static std::string PrepareMenuLabel(std::string label) {
    std::replace(label.begin(), label.end(), '&', '_');
    return label;
}

class MenuItemImplGtk : public MenuItem {
public:
    GtkMenuItem gtkMenuItem;

    MenuItemImplGtk() : gtkMenuItem(this) {}

    void SetAccelerator(KeyboardEvent accel) override {
        guint accelKey;
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
            accelMods |= Gdk::SHIFT_MASK;
        }
        if(accel.controlDown) {
            accelMods |= Gdk::CONTROL_MASK;
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
                gtkMenuItem.set_draw_as_radio(false);
                break;

            case Indicator::RADIO_MARK:
                gtkMenuItem.set_has_indicator(true);
                gtkMenuItem.set_draw_as_radio(true);
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

class MenuImplGtk : public Menu {
public:
    Gtk::Menu   gtkMenu;
    std::vector<std::shared_ptr<MenuItemImplGtk>>   menuItems;
    std::vector<std::shared_ptr<MenuImplGtk>>       subMenus;

    MenuItemRef AddItem(const std::string &label,
                        std::function<void()> onTrigger = NULL) override {
        auto menuItem = std::make_shared<MenuItemImplGtk>();
        menuItems.push_back(menuItem);

        menuItem->gtkMenuItem.set_label(PrepareMenuLabel(label));
        menuItem->gtkMenuItem.set_use_underline(true);
        menuItem->gtkMenuItem.show();
        menuItem->onTrigger = onTrigger;
        gtkMenu.append(menuItem->gtkMenuItem);

        return menuItem;
    }

    MenuRef AddSubMenu(const std::string &label) override {
        auto menuItem = std::make_shared<MenuItemImplGtk>();
        menuItems.push_back(menuItem);

        auto subMenu = std::make_shared<MenuImplGtk>();
        subMenus.push_back(subMenu);

        menuItem->gtkMenuItem.set_label(PrepareMenuLabel(label));
        menuItem->gtkMenuItem.set_use_underline(true);
        menuItem->gtkMenuItem.set_submenu(subMenu->gtkMenu);
        menuItem->gtkMenuItem.show_all();
        gtkMenu.append(menuItem->gtkMenuItem);

        return subMenu;
    }

    void AddSeparator() override {
        Gtk::SeparatorMenuItem *gtkMenuItem = Gtk::manage(new Gtk::SeparatorMenuItem());
        gtkMenuItem->show();
        gtkMenu.append(*Gtk::manage(gtkMenuItem));
    }

    void PopUp() override {
        Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
        auto signal = gtkMenu.signal_deactivate().connect([&]() { loop->quit(); });

        gtkMenu.show_all();
        gtkMenu.popup(0, GDK_CURRENT_TIME);
        loop->run();
        signal.disconnect();
    }

    void Clear() override {
        gtkMenu.foreach([&](Gtk::Widget &w) { gtkMenu.remove(w); });
        menuItems.clear();
        subMenus.clear();
    }
};

MenuRef CreateMenu() {
    return std::make_shared<MenuImplGtk>();
}

class MenuBarImplGtk : public MenuBar {
public:
    Gtk::MenuBar    gtkMenuBar;
    std::vector<std::shared_ptr<MenuImplGtk>>       subMenus;

    MenuRef AddSubMenu(const std::string &label) override {
        auto subMenu = std::make_shared<MenuImplGtk>();
        subMenus.push_back(subMenu);

        Gtk::MenuItem *gtkMenuItem = Gtk::manage(new Gtk::MenuItem);
        gtkMenuItem->set_label(PrepareMenuLabel(label));
        gtkMenuItem->set_use_underline(true);
        gtkMenuItem->set_submenu(subMenu->gtkMenu);
        gtkMenuItem->show_all();
        gtkMenuBar.append(*gtkMenuItem);

        return subMenu;
    }

    void Clear() override {
        gtkMenuBar.foreach([&](Gtk::Widget &w) { gtkMenuBar.remove(w); });
        subMenus.clear();
    }

    void *NativePtr() override {
        return &gtkMenuBar;
    }
};

MenuBarRef GetOrCreateMainMenu(bool *unique) {
    *unique = false;
    return std::make_shared<MenuBarImplGtk>();
}

}
}
