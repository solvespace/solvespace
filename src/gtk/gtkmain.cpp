//-----------------------------------------------------------------------------
// Our main() function, and GTK3-specific stuff to set up our windows and
// otherwise handle our interface to the operating system. Everything
// outside gtk/... should be standard C++ and OpenGL.
//
// Copyright 2015 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <iostream>

#include <json-c/json_object.h>
#include <json-c/json_util.h>

#include <glibmm/main.h>
#include <giomm/file.h>
#include <gdkmm/cursor.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/scrollbar.h>
#include <gtkmm/entry.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/fixed.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/radiobuttongroup.h>
#include <gtkmm/menu.h>
#include <gtkmm/menubar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/main.h>

#if HAVE_GTK3
#include <gtkmm/hvbox.h>
#else
#include <gtkmm/box.h>
#endif

#include <cairomm/xlib_surface.h>
#include <pangomm/fontdescription.h>
#include <gdk/gdkx.h>
#include <fontconfig/fontconfig.h>

#undef HAVE_STDINT_H /* no thanks, we have our own config.h */

#include <GL/glx.h>

#include <config.h>
#include "solvespace.h"
#include "../unix/gloffscreen.h"

#ifdef HAVE_SPACEWARE
#   include <spnav.h>
#   ifndef SI_APP_FIT_BUTTON
#       define SI_APP_FIT_BUTTON 31
#   endif
#endif

namespace SolveSpace {
char RecentFile[MAX_RECENT][MAX_PATH];

#define GL_CHECK() \
    do { \
        int err = (int)glGetError(); \
        if(err) dbp("%s:%d: glGetError() == 0x%X %s", \
                    __FILE__, __LINE__, err, gluErrorString(err)); \
    } while (0)

/* Settings */

/* Why not just use GSettings? Two reasons. It doesn't allow to easily see
   whether the setting had the default value, and it requires to install
   a schema globally. */
static json_object *settings = NULL;

static std::string CnfPrepare() {
    // Refer to http://standards.freedesktop.org/basedir-spec/latest/

    std::string dir;
    if(getenv("XDG_CONFIG_HOME")) {
        dir = std::string(getenv("XDG_CONFIG_HOME")) + "/solvespace";
    } else if(getenv("HOME")) {
        dir = std::string(getenv("HOME")) + "/.config/solvespace";
    } else {
        dbp("neither XDG_CONFIG_HOME nor HOME are set");
        return "";
    }

    struct stat st;
    if(stat(dir.c_str(), &st)) {
        if(errno == ENOENT) {
            if(mkdir(dir.c_str(), 0777)) {
                dbp("cannot mkdir %s: %s", dir.c_str(), strerror(errno));
                return "";
            }
        } else {
            dbp("cannot stat %s: %s", dir.c_str(), strerror(errno));
            return "";
        }
    } else if(!S_ISDIR(st.st_mode)) {
        dbp("%s is not a directory", dir.c_str());
        return "";
    }

    return dir + "/settings.json";
}

static void CnfLoad() {
    std::string path = CnfPrepare();
    if(path.empty())
        return;

    if(settings)
        json_object_put(settings); // deallocate

    settings = json_object_from_file(path.c_str());
    if(!settings) {
        if(errno != ENOENT)
            dbp("cannot load settings: %s", strerror(errno));

        settings = json_object_new_object();
    }
}

static void CnfSave() {
    std::string path = CnfPrepare();
    if(path.empty())
        return;

    /* json-c <0.12 has the first argument non-const here */
    if(json_object_to_file_ext((char*) path.c_str(), settings, JSON_C_TO_STRING_PRETTY))
        dbp("cannot save settings: %s", strerror(errno));
}

void CnfFreezeInt(uint32_t val, const std::string &key) {
    struct json_object *jval = json_object_new_int(val);
    json_object_object_add(settings, key.c_str(), jval);
    CnfSave();
}

uint32_t CnfThawInt(uint32_t val, const std::string &key) {
    struct json_object *jval;
    if(json_object_object_get_ex(settings, key.c_str(), &jval))
        return json_object_get_int(jval);
    else return val;
}

void CnfFreezeFloat(float val, const std::string &key) {
    struct json_object *jval = json_object_new_double(val);
    json_object_object_add(settings, key.c_str(), jval);
    CnfSave();
}

float CnfThawFloat(float val, const std::string &key) {
    struct json_object *jval;
    if(json_object_object_get_ex(settings, key.c_str(), &jval))
        return json_object_get_double(jval);
    else return val;
}

void CnfFreezeString(const std::string &val, const std::string &key) {
    struct json_object *jval = json_object_new_string(val.c_str());
    json_object_object_add(settings, key.c_str(), jval);
    CnfSave();
}

std::string CnfThawString(const std::string &val, const std::string &key) {
    struct json_object *jval;
    if(json_object_object_get_ex(settings, key.c_str(), &jval))
        return json_object_get_string(jval);
    return val;
}

static void CnfFreezeWindowPos(Gtk::Window *win, const std::string &key) {
    int x, y, w, h;
    win->get_position(x, y);
    win->get_size(w, h);

    CnfFreezeInt(x, key + "_left");
    CnfFreezeInt(y, key + "_top");
    CnfFreezeInt(w, key + "_width");
    CnfFreezeInt(h, key + "_height");
}

static void CnfThawWindowPos(Gtk::Window *win, const std::string &key) {
    int x, y, w, h;
    win->get_position(x, y);
    win->get_size(w, h);

    x = CnfThawInt(x, key + "_left");
    y = CnfThawInt(y, key + "_top");
    w = CnfThawInt(w, key + "_width");
    h = CnfThawInt(h, key + "_height");

    win->move(x, y);
    win->resize(w, h);
}

/* Timers */

int64_t GetMilliseconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000 * (uint64_t) ts.tv_sec + ts.tv_nsec / 1000000;
}

static bool TimerCallback() {
    SS.GW.TimerCallback();
    SS.TW.TimerCallback();
    return false;
}

void SetTimerFor(int milliseconds) {
    Glib::signal_timeout().connect(&TimerCallback, milliseconds);
}

static bool AutosaveTimerCallback() {
    SS.Autosave();
    return false;
}

void SetAutosaveTimerFor(int minutes) {
    Glib::signal_timeout().connect(&AutosaveTimerCallback, minutes * 60 * 1000);
}

static bool LaterCallback() {
    SS.DoLater();
    return false;
}

void ScheduleLater() {
    Glib::signal_idle().connect(&LaterCallback);
}

/* GL wrapper */

class GlWidget : public Gtk::DrawingArea {
public:
    GlWidget() : _offscreen(NULL) {
        _xdisplay = gdk_x11_get_default_xdisplay();

        int glxmajor, glxminor;
        if(!glXQueryVersion(_xdisplay, &glxmajor, &glxminor)) {
            dbp("OpenGL is not supported");
            oops();
        }

        if(glxmajor < 1 || (glxmajor == 1 && glxminor < 3)) {
            dbp("GLX version %d.%d is too old; 1.3 required", glxmajor, glxminor);
            oops();
        }

        static int fbconfig_attrs[] = {
            GLX_RENDER_TYPE, GLX_RGBA_BIT,
            GLX_RED_SIZE, 8,
            GLX_GREEN_SIZE, 8,
            GLX_BLUE_SIZE, 8,
            GLX_DEPTH_SIZE, 24,
            None
        };
        int fbconfig_num = 0;
        GLXFBConfig *fbconfigs = glXChooseFBConfig(_xdisplay, DefaultScreen(_xdisplay),
                fbconfig_attrs, &fbconfig_num);
        if(!fbconfigs || fbconfig_num == 0)
            oops();

        /* prefer FBConfigs with depth of 32;
            * Mesa software rasterizer explodes with a BadMatch without this;
            * without this, Intel on Mesa flickers horribly for some reason.
           this does not seem to affect other rasterizers (ie NVidia).

           see this Mesa bug:
           http://lists.freedesktop.org/archives/mesa-dev/2015-January/074693.html */
        GLXFBConfig fbconfig = fbconfigs[0];
        for(int i = 0; i < fbconfig_num; i++) {
            XVisualInfo *visual_info = glXGetVisualFromFBConfig(_xdisplay, fbconfigs[i]);
            /* some GL visuals, notably on Chromium GL, do not have an associated
               X visual; this is not an obstacle as we always render offscreen. */
            if(!visual_info) continue;
            int depth = visual_info->depth;
            XFree(visual_info);

            if(depth == 32) {
                fbconfig = fbconfigs[i];
                break;
            }
        }

        _glcontext = glXCreateNewContext(_xdisplay,
                fbconfig, GLX_RGBA_TYPE, 0, True);
        if(!_glcontext) {
            dbp("cannot create OpenGL context");
            oops();
        }

        XFree(fbconfigs);

        /* create a dummy X window to create a rendering context against.
           we could use a Pbuffer, but some implementations (Chromium GL)
           don't support these. we could use an existing window, but
           some implementations (Chromium GL... do you see a pattern?)
           do really strange things, i.e. draw a black rectangle on
           the very front of the desktop if you do this. */
        _xwindow = XCreateSimpleWindow(_xdisplay,
                XRootWindow(_xdisplay, gdk_x11_get_default_screen()),
                /*x*/ 0, /*y*/ 0, /*width*/ 1, /*height*/ 1,
                /*border_width*/ 0, /*border*/ 0, /*background*/ 0);
    }

    ~GlWidget() {
        glXMakeCurrent(_xdisplay, None, NULL);

        XDestroyWindow(_xdisplay, _xwindow);

        delete _offscreen;

        glXDestroyContext(_xdisplay, _glcontext);
    }

protected:
    /* Draw on a GLX framebuffer object, then read pixels out and draw them on
       the Cairo context. Slower, but you get to overlay nice widgets. */
    virtual bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) {
        if(!glXMakeCurrent(_xdisplay, _xwindow, _glcontext))
            oops();

        if(!_offscreen)
            _offscreen = new GLOffscreen;

        Gdk::Rectangle allocation = get_allocation();
        if(!_offscreen->begin(allocation.get_width(), allocation.get_height()))
            oops();

        on_gl_draw();
        glFlush();
        GL_CHECK();

        Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create(
                _offscreen->end(), Cairo::FORMAT_RGB24,
                allocation.get_width(), allocation.get_height(), allocation.get_width() * 4);
        cr->set_source(surface, 0, 0);
        cr->paint();
        surface->finish();

        return true;
    }

#ifdef HAVE_GTK2
    virtual bool on_expose_event(GdkEventExpose *event) {
        return on_draw(get_window()->create_cairo_context());
    }
#endif

    virtual void on_gl_draw() = 0;

private:
    Display *_xdisplay;
    GLXContext _glcontext;
    GLOffscreen *_offscreen;
    ::Window _xwindow;
};

/* Editor overlay */

class EditorOverlay : public Gtk::Fixed {
public:
    EditorOverlay(Gtk::Widget &underlay) : _underlay(underlay) {
        add(_underlay);

        Pango::FontDescription desc;
        desc.set_family("monospace");
        desc.set_size(7000);
#ifdef HAVE_GTK3
        _entry.override_font(desc);
#else
        _entry.modify_font(desc);
#endif
        _entry.set_width_chars(30);
        _entry.set_no_show_all(true);
        add(_entry);

        _entry.signal_activate().
            connect(sigc::mem_fun(this, &EditorOverlay::on_activate));
    }

    void start_editing(int x, int y, const char *val) {
        move(_entry, x, y - 4);
        _entry.set_text(val);
        if(!_entry.is_visible()) {
            _entry.show();
            _entry.grab_focus();
            _entry.add_modal_grab();
        }
    }

    void stop_editing() {
        if(_entry.is_visible())
            _entry.remove_modal_grab();
        _entry.hide();
    }

    bool is_editing() const {
        return _entry.is_visible();
    }

    sigc::signal<void, Glib::ustring> signal_editing_done() {
        return _signal_editing_done;
    }

    Gtk::Entry &get_entry() {
        return _entry;
    }

protected:
    virtual bool on_key_press_event(GdkEventKey *event) {
        if(event->keyval == GDK_KEY_Escape) {
            stop_editing();
            return true;
        }

        return false;
    }

    virtual void on_size_allocate(Gtk::Allocation& allocation) {
        Gtk::Fixed::on_size_allocate(allocation);

        _underlay.size_allocate(allocation);
    }

    virtual void on_activate() {
        _signal_editing_done(_entry.get_text());
    }

private:
    Gtk::Widget &_underlay;
    Gtk::Entry _entry;
    sigc::signal<void, Glib::ustring> _signal_editing_done;
};

/* Graphics window */

int DeltaYOfScrollEvent(GdkEventScroll *event) {
#ifdef HAVE_GTK3
    int delta_y = event->delta_y;
#else
    int delta_y = 0;
#endif
    if(delta_y == 0) {
        switch(event->direction) {
            case GDK_SCROLL_UP:
            delta_y = -1;
            break;

            case GDK_SCROLL_DOWN:
            delta_y = 1;
            break;

            default:
            /* do nothing */
            return false;
        }
    }

    return delta_y;
}

class GraphicsWidget : public GlWidget {
public:
    GraphicsWidget() {
        set_events(Gdk::POINTER_MOTION_MASK |
                   Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::BUTTON_MOTION_MASK |
                   Gdk::SCROLL_MASK |
                   Gdk::LEAVE_NOTIFY_MASK);
        set_double_buffered(true);
    }

    void emulate_key_press(GdkEventKey *event) {
        on_key_press_event(event);
    }

protected:
    virtual bool on_configure_event(GdkEventConfigure *event) {
        _w = event->width;
        _h = event->height;

        return GlWidget::on_configure_event(event);;
    }

    virtual void on_gl_draw() {
        SS.GW.Paint();
    }

    virtual bool on_motion_notify_event(GdkEventMotion *event) {
        int x, y;
        ij_to_xy(event->x, event->y, x, y);

        SS.GW.MouseMoved(x, y,
            event->state & GDK_BUTTON1_MASK,
            event->state & GDK_BUTTON2_MASK,
            event->state & GDK_BUTTON3_MASK,
            event->state & GDK_SHIFT_MASK,
            event->state & GDK_CONTROL_MASK);

        return true;
    }

    virtual bool on_button_press_event(GdkEventButton *event) {
        int x, y;
        ij_to_xy(event->x, event->y, x, y);

        switch(event->button) {
            case 1:
            if(event->type == GDK_BUTTON_PRESS)
                SS.GW.MouseLeftDown(x, y);
            else if(event->type == GDK_2BUTTON_PRESS)
                SS.GW.MouseLeftDoubleClick(x, y);
            break;

            case 2:
            case 3:
            SS.GW.MouseMiddleOrRightDown(x, y);
            break;
        }

        return true;
    }

    virtual bool on_button_release_event(GdkEventButton *event) {
        int x, y;
        ij_to_xy(event->x, event->y, x, y);

        switch(event->button) {
            case 1:
            SS.GW.MouseLeftUp(x, y);
            break;

            case 3:
            SS.GW.MouseRightUp(x, y);
            break;
        }

        return true;
    }

    virtual bool on_scroll_event(GdkEventScroll *event) {
        int x, y;
        ij_to_xy(event->x, event->y, x, y);

        SS.GW.MouseScroll(x, y, -DeltaYOfScrollEvent(event));

        return true;
    }

    virtual bool on_leave_notify_event (GdkEventCrossing*event) {
        SS.GW.MouseLeave();

        return true;
    }

    virtual bool on_key_press_event(GdkEventKey *event) {
        int chr;

        switch(event->keyval) {
            case GDK_KEY_Escape:
            chr = GraphicsWindow::ESCAPE_KEY;
            break;

            case GDK_KEY_Delete:
            chr = GraphicsWindow::DELETE_KEY;
            break;

            case GDK_KEY_Tab:
            chr = '\t';
            break;

            case GDK_KEY_BackSpace:
            case GDK_KEY_Back:
            chr = '\b';
            break;

            default:
            if(event->keyval >= GDK_KEY_F1 && event->keyval <= GDK_KEY_F12)
                chr = GraphicsWindow::FUNCTION_KEY_BASE + (event->keyval - GDK_KEY_F1);
            else
                chr = gdk_keyval_to_unicode(event->keyval);
        }

        if(event->state & GDK_SHIFT_MASK)
            chr |= GraphicsWindow::SHIFT_MASK;
        if(event->state & GDK_CONTROL_MASK)
            chr |= GraphicsWindow::CTRL_MASK;

        if(chr && SS.GW.KeyDown(chr))
            return true;

        return false;
    }

private:
    int _w, _h;
    void ij_to_xy(int i, int j, int &x, int &y) {
        // Convert to xy (vs. ij) style coordinates,
        // with (0, 0) at center
        x = i - _w / 2;
        y = _h / 2 - j;
    }
};

class GraphicsWindowGtk : public Gtk::Window {
public:
    GraphicsWindowGtk() : _overlay(_widget) {
        set_default_size(900, 600);

        _box.pack_start(_menubar, false, true);
        _box.pack_start(_overlay, true, true);

        add(_box);

        _overlay.signal_editing_done().
            connect(sigc::mem_fun(this, &GraphicsWindowGtk::on_editing_done));
    }

    GraphicsWidget &get_widget() {
        return _widget;
    }

    EditorOverlay &get_overlay() {
        return _overlay;
    }

    Gtk::MenuBar &get_menubar() {
        return _menubar;
    }

    bool is_fullscreen() const {
        return _is_fullscreen;
    }

protected:
    virtual void on_show() {
        Gtk::Window::on_show();

        CnfThawWindowPos(this, "GraphicsWindow");
    }

    virtual void on_hide() {
        CnfFreezeWindowPos(this, "GraphicsWindow");

        Gtk::Window::on_hide();
    }

    virtual bool on_delete_event(GdkEventAny *event) {
        SS.Exit();

        return true;
    }

    virtual bool on_window_state_event(GdkEventWindowState *event) {
        _is_fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

        /* The event arrives too late for the caller of ToggleFullScreen
           to notice state change; and it's possible that the WM will
           refuse our request, so we can't just toggle the saved state */
        SS.GW.EnsureValidActives();

        return Gtk::Window::on_window_state_event(event);
    }

    virtual void on_editing_done(Glib::ustring value) {
        SS.GW.EditControlDone(value.c_str());
    }

private:
    GraphicsWidget _widget;
    EditorOverlay _overlay;
    Gtk::MenuBar _menubar;
    Gtk::VBox _box;

    bool _is_fullscreen;
};

GraphicsWindowGtk *GW = NULL;

void GetGraphicsWindowSize(int *w, int *h) {
    Gdk::Rectangle allocation = GW->get_widget().get_allocation();
    *w = allocation.get_width();
    *h = allocation.get_height();
}

void InvalidateGraphics(void) {
    GW->get_widget().queue_draw();
}

void PaintGraphics(void) {
    GW->get_widget().queue_draw();
    /* Process animation */
    Glib::MainContext::get_default()->iteration(false);
}

void SetCurrentFilename(const char *filename) {
    if(filename) {
        GW->set_title(std::string("SolveSpace - ") + filename);
    } else {
        GW->set_title("SolveSpace - (not yet saved)");
    }
}

void ToggleFullScreen(void) {
    if(GW->is_fullscreen())
        GW->unfullscreen();
    else
        GW->fullscreen();
}

bool FullScreenIsActive(void) {
    return GW->is_fullscreen();
}

void ShowGraphicsEditControl(int x, int y, char *val) {
    Gdk::Rectangle rect = GW->get_widget().get_allocation();

    // Convert to ij (vs. xy) style coordinates,
    // and compensate for the input widget height due to inverse coord
    int i, j;
    i = x + rect.get_width() / 2;
    j = -y + rect.get_height() / 2 - 24;

    GW->get_overlay().start_editing(i, j, val);
}

void HideGraphicsEditControl(void) {
    GW->get_overlay().stop_editing();
}

bool GraphicsEditControlIsVisible(void) {
    return GW->get_overlay().is_editing();
}

/* TODO: removing menubar breaks accelerators. */
void ToggleMenuBar(void) {
    GW->get_menubar().set_visible(!GW->get_menubar().is_visible());
}

bool MenuBarIsVisible(void) {
    return GW->get_menubar().is_visible();
}

/* Context menus */

class ContextMenuItem : public Gtk::MenuItem {
public:
    static int choice;

    ContextMenuItem(const Glib::ustring &label, int id, bool mnemonic=false) :
            Gtk::MenuItem(label, mnemonic), _id(id) {
    }

protected:
    virtual void on_activate() {
        Gtk::MenuItem::on_activate();

        if(has_submenu())
            return;

        choice = _id;
    }

    /* Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=695488.
       This is used in addition to on_activate() to catch mouse events.
       Without on_activate(), it would be impossible to select a menu item
       via keyboard.
       This selects the item twice in some cases, but we are idempotent.
     */
    virtual bool on_button_press_event(GdkEventButton *event) {
        if(event->button == 1 && event->type == GDK_BUTTON_PRESS) {
            on_activate();
            return true;
        }

        return Gtk::MenuItem::on_button_press_event(event);
    }

private:
    int _id;
};

int ContextMenuItem::choice = 0;

static Gtk::Menu *context_menu = NULL, *context_submenu = NULL;

void AddContextMenuItem(const char *label, int id) {
    Gtk::MenuItem *menu_item;
    if(label)
        menu_item = new ContextMenuItem(label, id);
    else
        menu_item = new Gtk::SeparatorMenuItem();

    if(id == CONTEXT_SUBMENU) {
        menu_item->set_submenu(*context_submenu);
        context_submenu = NULL;
    }

    if(context_submenu) {
        context_submenu->append(*menu_item);
    } else {
        if(!context_menu)
            context_menu = new Gtk::Menu;

        context_menu->append(*menu_item);
    }
}

void CreateContextSubmenu(void) {
    if(context_submenu) oops();

    context_submenu = new Gtk::Menu;
}

int ShowContextMenu(void) {
    if(!context_menu)
        return -1;

    Glib::RefPtr<Glib::MainLoop> loop = Glib::MainLoop::create();
    context_menu->signal_deactivate().
        connect(sigc::mem_fun(loop.operator->(), &Glib::MainLoop::quit));

    ContextMenuItem::choice = -1;

    context_menu->show_all();
    context_menu->popup(3, GDK_CURRENT_TIME);

    loop->run();

    delete context_menu;
    context_menu = NULL;

    return ContextMenuItem::choice;
}

/* Main menu */

template<class MenuItem> class MainMenuItem : public MenuItem {
public:
    MainMenuItem(const GraphicsWindow::MenuEntry &entry) :
            MenuItem(), _entry(entry), _synthetic(false) {
        Glib::ustring label(_entry.label);
        for(int i = 0; i < label.length(); i++) {
            if(label[i] == '&')
                label.replace(i, 1, "_");
        }

        guint accel_key = 0;
        Gdk::ModifierType accel_mods = Gdk::ModifierType();
        switch(_entry.accel) {
            case GraphicsWindow::DELETE_KEY:
            accel_key = GDK_KEY_Delete;
            break;

            case GraphicsWindow::ESCAPE_KEY:
            accel_key = GDK_KEY_Escape;
            break;

            default:
            accel_key = _entry.accel & ~(GraphicsWindow::SHIFT_MASK | GraphicsWindow::CTRL_MASK);
            if(accel_key > GraphicsWindow::FUNCTION_KEY_BASE &&
                    accel_key <= GraphicsWindow::FUNCTION_KEY_BASE + 12)
                accel_key = GDK_KEY_F1 + (accel_key - GraphicsWindow::FUNCTION_KEY_BASE - 1);
            else
                accel_key = gdk_unicode_to_keyval(accel_key);

            if(_entry.accel & GraphicsWindow::SHIFT_MASK)
                accel_mods |= Gdk::SHIFT_MASK;
            if(_entry.accel & GraphicsWindow::CTRL_MASK)
                accel_mods |= Gdk::CONTROL_MASK;
        }

        MenuItem::set_label(label);
        MenuItem::set_use_underline(true);
        if(!(accel_key & 0x01000000))
            MenuItem::set_accel_key(Gtk::AccelKey(accel_key, accel_mods));
    }

    void set_active(bool checked) {
        if(MenuItem::get_active() == checked)
            return;

       _synthetic = true;
        MenuItem::set_active(checked);
    }

protected:
    virtual void on_activate() {
        MenuItem::on_activate();

        if(_synthetic)
            _synthetic = false;
        else if(!MenuItem::has_submenu() && _entry.fn)
            _entry.fn(_entry.id);
    }

private:
    const GraphicsWindow::MenuEntry &_entry;
    bool _synthetic;
};

static std::map<int, Gtk::MenuItem *> main_menu_items;

static void InitMainMenu(Gtk::MenuShell *menu_shell) {
    Gtk::MenuItem *menu_item = NULL;
    Gtk::MenuShell *levels[5] = {menu_shell, 0};

    const GraphicsWindow::MenuEntry *entry = &GraphicsWindow::menu[0];
    int current_level = 0;
    while(entry->level >= 0) {
        if(entry->level > current_level) {
            Gtk::Menu *menu = new Gtk::Menu;
            menu_item->set_submenu(*menu);

            if(entry->level >= sizeof(levels) / sizeof(levels[0]))
                oops();

            levels[entry->level] = menu;
        }

        current_level = entry->level;

        if(entry->label) {
            switch(entry->kind) {
                case GraphicsWindow::MENU_ITEM_NORMAL:
                menu_item = new MainMenuItem<Gtk::MenuItem>(*entry);
                break;

                case GraphicsWindow::MENU_ITEM_CHECK:
                menu_item = new MainMenuItem<Gtk::CheckMenuItem>(*entry);
                break;

                case GraphicsWindow::MENU_ITEM_RADIO:
                MainMenuItem<Gtk::CheckMenuItem> *radio_item =
                        new MainMenuItem<Gtk::CheckMenuItem>(*entry);
                radio_item->set_draw_as_radio(true);
                menu_item = radio_item;
                break;
            }
        } else {
            menu_item = new Gtk::SeparatorMenuItem();
        }

        levels[entry->level]->append(*menu_item);

        main_menu_items[entry->id] = menu_item;

        ++entry;
    }
}

void EnableMenuById(int id, bool enabled) {
    main_menu_items[id]->set_sensitive(enabled);
}

static void ActivateMenuById(int id) {
    main_menu_items[id]->activate();
}

void CheckMenuById(int id, bool checked) {
    ((MainMenuItem<Gtk::CheckMenuItem>*)main_menu_items[id])->set_active(checked);
}

void RadioMenuById(int id, bool selected) {
    SolveSpace::CheckMenuById(id, selected);
}

class RecentMenuItem : public Gtk::MenuItem {
public:
    RecentMenuItem(const Glib::ustring& label, int id) :
            MenuItem(label), _id(id) {
    }

protected:
    virtual void on_activate() {
        if(_id >= RECENT_OPEN && _id < (RECENT_OPEN + MAX_RECENT))
            SolveSpaceUI::MenuFile(_id);
        else if(_id >= RECENT_IMPORT && _id < (RECENT_IMPORT + MAX_RECENT))
            Group::MenuGroup(_id);
    }

private:
    int _id;
};

static void RefreshRecentMenu(int id, int base) {
    Gtk::MenuItem *recent = static_cast<Gtk::MenuItem*>(main_menu_items[id]);
    recent->unset_submenu();

    Gtk::Menu *menu = new Gtk::Menu;
    recent->set_submenu(*menu);

    if(std::string(RecentFile[0]).empty()) {
        Gtk::MenuItem *placeholder = new Gtk::MenuItem("(no recent files)");
        placeholder->set_sensitive(false);
        menu->append(*placeholder);
    } else {
        for(int i = 0; i < MAX_RECENT; i++) {
            if(std::string(RecentFile[i]).empty())
                break;

            RecentMenuItem *item = new RecentMenuItem(RecentFile[i], base + i);
            menu->append(*item);
        }
    }

    menu->show_all();
}

void RefreshRecentMenus(void) {
    RefreshRecentMenu(GraphicsWindow::MNU_OPEN_RECENT, RECENT_OPEN);
    RefreshRecentMenu(GraphicsWindow::MNU_GROUP_RECENT, RECENT_IMPORT);
}

/* Save/load */

static void FiltersFromPattern(const char *active, const char *patterns,
                               Gtk::FileChooser &chooser) {
    Glib::ustring uactive = "*." + Glib::ustring(active);
    Glib::ustring upatterns = patterns;

#ifdef HAVE_GTK3
    Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
#else
    Gtk::FileFilter *filter = new Gtk::FileFilter;
#endif
    Glib::ustring desc = "";
    bool has_name = false, is_active = false;
    int last = 0;
    for(int i = 0; i <= upatterns.length(); i++) {
        if(upatterns[i] == '\t' || upatterns[i] == '\n' || upatterns[i] == '\0') {
            Glib::ustring frag = upatterns.substr(last, i - last);
            if(!has_name) {
                filter->set_name(frag);
                has_name = true;
            } else {
                filter->add_pattern(frag);
                if(uactive == frag)
                    is_active = true;
                if(desc == "")
                    desc = frag;
                else
                    desc += ", " + frag;
            }
        } else continue;

        if(upatterns[i] == '\n' || upatterns[i] == '\0') {
            filter->set_name(filter->get_name() + " (" + desc + ")");
#ifdef HAVE_GTK3
            chooser.add_filter(filter);
            if(is_active)
                chooser.set_filter(filter);

            filter = Gtk::FileFilter::create();
#else
            chooser.add_filter(*filter);
            if(is_active)
                chooser.set_filter(*filter);

            filter = new Gtk::FileFilter();
#endif
            has_name = false;
            is_active = false;
            desc = "";
        }

        last = i + 1;
    }
}

bool GetOpenFile(char *file, const char *active, const char *patterns) {
    Gtk::FileChooserDialog chooser(*GW, "SolveSpace - Open File");
    chooser.set_filename(file);
    chooser.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    chooser.add_button("_Open", Gtk::RESPONSE_OK);
    chooser.set_current_folder(CnfThawString("", "FileChooserPath"));

    FiltersFromPattern(active, patterns, chooser);

    if(chooser.run() == Gtk::RESPONSE_OK) {
        CnfFreezeString(chooser.get_current_folder().c_str(), "FileChooserPath");
        strcpy(file, chooser.get_filename().c_str());
        return true;
    } else {
        return false;
    }
}

/* Glib::path_get_basename got /removed/ in 3.0?! Come on */
static std::string Basename(std::string filename) {
    int slash = filename.rfind('/');
    if(slash >= 0)
        return filename.substr(slash + 1, filename.length());
    return "";
}

static void ChooserFilterChanged(Gtk::FileChooserDialog *chooser)
{
    /* Extract the pattern from the filter. GtkFileFilter doesn't provide
       any way to list the patterns, so we extract it from the filter name.
       Gross. */
    std::string filter_name = chooser->get_filter()->get_name();
    int lparen = filter_name.find('(') + 1;
    int rdelim = filter_name.find(',', lparen);
    if(rdelim < 0)
        rdelim = filter_name.find(')', lparen);
    if(lparen < 0 || rdelim < 0)
        oops();

    std::string extension = filter_name.substr(lparen, rdelim - lparen);
    if(extension == "*")
        return;

    if(extension.length() > 2 && extension.substr(0, 2) == "*.")
        extension = extension.substr(2, extension.length() - 2);

    std::string basename = Basename(chooser->get_filename());
    int dot = basename.rfind('.');
    if(dot >= 0) {
        basename.replace(dot + 1, basename.length() - dot - 1, extension);
        chooser->set_current_name(basename);
    } else {
        chooser->set_current_name(basename + "." + extension);
    }
}

bool GetSaveFile(char *file, const char *active, const char *patterns) {
    Gtk::FileChooserDialog chooser(*GW, "SolveSpace - Save File",
                                   Gtk::FILE_CHOOSER_ACTION_SAVE);
    chooser.set_do_overwrite_confirmation(true);
    chooser.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    chooser.add_button("_Save", Gtk::RESPONSE_OK);

    FiltersFromPattern(active, patterns, chooser);

    chooser.set_current_folder(CnfThawString("", "FileChooserPath"));
    chooser.set_current_name(std::string("untitled.") + active);

    /* Gtk's dialog doesn't change the extension when you change the filter,
       and makes it extremely hard to do so. Gtk is garbage. */
    chooser.property_filter().signal_changed().
       connect(sigc::bind(sigc::ptr_fun(&ChooserFilterChanged), &chooser));

    if(chooser.run() == Gtk::RESPONSE_OK) {
        CnfFreezeString(chooser.get_current_folder(), "FileChooserPath");
        strcpy(file, chooser.get_filename().c_str());
        return true;
    } else {
        return false;
    }
}

int SaveFileYesNoCancel(void) {
    Glib::ustring message =
        "The file has changed since it was last saved.\n"
        "Do you want to save the changes?";
    Gtk::MessageDialog dialog(*GW, message, /*use_markup*/ true, Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_NONE, /*is_modal*/ true);
    dialog.set_title("SolveSpace - Modified File");
    dialog.add_button("_Save", Gtk::RESPONSE_YES);
    dialog.add_button("Do_n't Save", Gtk::RESPONSE_NO);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);

    switch(dialog.run()) {
        case Gtk::RESPONSE_YES:
        return SAVE_YES;

        case Gtk::RESPONSE_NO:
        return SAVE_NO;

        case Gtk::RESPONSE_CANCEL:
        default:
        return SAVE_CANCEL;
    }
}

int LoadAutosaveYesNo(void) {
    Glib::ustring message =
        "An autosave file is availible for this project.\n"
        "Do you want to load the autosave file instead?";
    Gtk::MessageDialog dialog(*GW, message, /*use_markup*/ true, Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_NONE, /*is_modal*/ true);
    dialog.set_title("SolveSpace - Autosave Available");
    dialog.add_button("_Load autosave", Gtk::RESPONSE_YES);
    dialog.add_button("Do_n't Load", Gtk::RESPONSE_NO);

    switch(dialog.run()) {
        case Gtk::RESPONSE_YES:
        return SAVE_YES;

        case Gtk::RESPONSE_NO:
        default:
        return SAVE_NO;
    }
}

/* Text window */

class TextWidget : public GlWidget {
public:
#ifdef HAVE_GTK3
    TextWidget(Glib::RefPtr<Gtk::Adjustment> adjustment) : _adjustment(adjustment) {
#else
    TextWidget(Gtk::Adjustment* adjustment) : _adjustment(adjustment) {
#endif
        set_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::SCROLL_MASK |
                   Gdk::LEAVE_NOTIFY_MASK);
    }

    void set_cursor_hand(bool is_hand) {
        Glib::RefPtr<Gdk::Window> gdkwin = get_window();
        if(gdkwin) { // returns NULL if not realized
            Gdk::CursorType type = is_hand ? Gdk::HAND1 : Gdk::ARROW;
#ifdef HAVE_GTK3
            gdkwin->set_cursor(Gdk::Cursor::create(type));
#else
            gdkwin->set_cursor(Gdk::Cursor(type));
#endif
        }
    }

protected:
    virtual void on_gl_draw() {
        SS.TW.Paint();
    }

    virtual bool on_motion_notify_event(GdkEventMotion *event) {
        SS.TW.MouseEvent(/*leftClick*/ false,
                         /*leftDown*/ event->state & GDK_BUTTON1_MASK,
                         event->x, event->y);

        return true;
    }

    virtual bool on_button_press_event(GdkEventButton *event) {
        SS.TW.MouseEvent(/*leftClick*/ event->type == GDK_BUTTON_PRESS,
                         /*leftDown*/ event->state & GDK_BUTTON1_MASK,
                         event->x, event->y);

        return true;
    }

    virtual bool on_scroll_event(GdkEventScroll *event) {
        _adjustment->set_value(_adjustment->get_value() +
                DeltaYOfScrollEvent(event) * _adjustment->get_page_increment());

        return true;
    }

    virtual bool on_leave_notify_event (GdkEventCrossing*event) {
        SS.TW.MouseLeave();

        return true;
    }

private:
#ifdef HAVE_GTK3
    Glib::RefPtr<Gtk::Adjustment> _adjustment;
#else
    Gtk::Adjustment *_adjustment;
#endif
};

class TextWindowGtk : public Gtk::Window {
public:
    TextWindowGtk() : _scrollbar(), _widget(_scrollbar.get_adjustment()),
                      _overlay(_widget), _box() {
        set_keep_above(true);
        set_type_hint(Gdk::WINDOW_TYPE_HINT_UTILITY);
        set_skip_taskbar_hint(true);
        set_skip_pager_hint(true);
        set_title("SolveSpace - Browser");
        set_default_size(420, 300);

        _box.pack_start(_overlay, true, true);
        _box.pack_start(_scrollbar, false, true);
        add(_box);

        _scrollbar.get_adjustment()->signal_value_changed().
            connect(sigc::mem_fun(this, &TextWindowGtk::on_scrollbar_value_changed));

        _overlay.signal_editing_done().
            connect(sigc::mem_fun(this, &TextWindowGtk::on_editing_done));

        _overlay.get_entry().signal_motion_notify_event().
            connect(sigc::mem_fun(this, &TextWindowGtk::on_editor_motion_notify_event));
        _overlay.get_entry().signal_button_press_event().
            connect(sigc::mem_fun(this, &TextWindowGtk::on_editor_button_press_event));
    }

    Gtk::VScrollbar &get_scrollbar() {
        return _scrollbar;
    }

    TextWidget &get_widget() {
        return _widget;
    }

    EditorOverlay &get_overlay() {
        return _overlay;
    }

protected:
    virtual void on_show() {
        Gtk::Window::on_show();

        CnfThawWindowPos(this, "TextWindow");
    }

    virtual void on_hide() {
        CnfFreezeWindowPos(this, "TextWindow");

        Gtk::Window::on_hide();
    }

    virtual bool on_delete_event(GdkEventAny *event) {
        /* trigger the action and ignore the request */
        GraphicsWindow::MenuView(GraphicsWindow::MNU_SHOW_TEXT_WND);

        return false;
    }

    virtual void on_scrollbar_value_changed() {
        SS.TW.ScrollbarEvent(_scrollbar.get_adjustment()->get_value());
    }

    virtual void on_editing_done(Glib::ustring value) {
        SS.TW.EditControlDone(value.c_str());
    }

    virtual bool on_editor_motion_notify_event(GdkEventMotion *event) {
        return _widget.event((GdkEvent*) event);
    }

    virtual bool on_editor_button_press_event(GdkEventButton *event) {
        return _widget.event((GdkEvent*) event);
    }

private:
    Gtk::VScrollbar _scrollbar;
    TextWidget _widget;
    EditorOverlay _overlay;
    Gtk::HBox _box;
};

TextWindowGtk *TW = NULL;

void ShowTextWindow(bool visible) {
    if(visible)
        TW->show();
    else
        TW->hide();
}

void GetTextWindowSize(int *w, int *h) {
    Gdk::Rectangle allocation = TW->get_widget().get_allocation();
    *w = allocation.get_width();
    *h = allocation.get_height();
}

void InvalidateText(void) {
    TW->get_widget().queue_draw();
}

void MoveTextScrollbarTo(int pos, int maxPos, int page) {
    TW->get_scrollbar().get_adjustment()->configure(pos, 0, maxPos, 1, 10, page);
}

void SetMousePointerToHand(bool is_hand) {
    TW->get_widget().set_cursor_hand(is_hand);
}

void ShowTextEditControl(int x, int y, char *val) {
    TW->get_overlay().start_editing(x, y, val);
}

void HideTextEditControl(void) {
    TW->get_overlay().stop_editing();
    GW->raise();
}

bool TextEditControlIsVisible(void) {
    return TW->get_overlay().is_editing();
}

/* Miscellanea */


void DoMessageBox(const char *message, int rows, int cols, bool error) {
    Gtk::MessageDialog dialog(*GW, message, /*use_markup*/ true,
                              error ? Gtk::MESSAGE_ERROR : Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK,
                              /*is_modal*/ true);
    dialog.set_title(error ? "SolveSpace - Error" : "SolveSpace - Message");
    dialog.run();
}

void OpenWebsite(const char *url) {
    gtk_show_uri(Gdk::Screen::get_default()->gobj(), url, GDK_CURRENT_TIME, NULL);
}

/* fontconfig is already initialized by GTK */
void LoadAllFontFiles(void) {
    FcPattern   *pat = FcPatternCreate();
    FcObjectSet *os  = FcObjectSetBuild(FC_FILE, (char *)0);
    FcFontSet   *fs  = FcFontList(0, pat, os);

    for(int i = 0; i < fs->nfont; i++) {
        FcChar8 *filename = FcPatternFormat(fs->fonts[i], (const FcChar8*) "%{file}");
        Glib::ustring ufilename = (char*) filename;
        if(ufilename.length() > 4 &&
           ufilename.substr(ufilename.length() - 4, 4).lowercase() == ".ttf") {
            TtfFont tf = {};
            strcpy(tf.fontFile, (char*) filename);
            SS.fonts.l.Add(&tf);
        }
        FcStrFree(filename);
    }

    FcFontSetDestroy(fs);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);
}

/* Space Navigator support */

#ifdef HAVE_SPACEWARE
static GdkFilterReturn GdkSpnavFilter(GdkXEvent *gxevent, GdkEvent *event, gpointer data) {
    XEvent *xevent = (XEvent*) gxevent;

    spnav_event sev;
    if(!spnav_x11_event(xevent, &sev))
        return GDK_FILTER_CONTINUE;

    switch(sev.type) {
        case SPNAV_EVENT_MOTION:
            SS.GW.SpaceNavigatorMoved(
                (double)sev.motion.x,
                (double)sev.motion.y,
                (double)sev.motion.z  * -1.0,
                (double)sev.motion.rx *  0.001,
                (double)sev.motion.ry *  0.001,
                (double)sev.motion.rz * -0.001,
                xevent->xmotion.state & ShiftMask);
            break;

        case SPNAV_EVENT_BUTTON:
            if(!sev.button.press && sev.button.bnum == SI_APP_FIT_BUTTON) {
                SS.GW.SpaceNavigatorButtonUp();
            }
            break;
    }

    return GDK_FILTER_REMOVE;
}
#endif

/* Application lifecycle */

void ExitNow(void) {
    GW->hide();
    TW->hide();
}
};

int main(int argc, char** argv) {
    /* If we don't call this, gtk_init will set the C standard library
       locale, and printf will format floats using ",". We will then
       fail to parse these. Also, many text window lines will become
       ambiguous. */
    gtk_disable_setlocale();

    Gtk::Main main(argc, argv);

#ifdef HAVE_SPACEWARE
    gdk_window_add_filter(NULL, GdkSpnavFilter, NULL);
#endif

    CnfLoad();

    TW = new TextWindowGtk;
    GW = new GraphicsWindowGtk;
    InitMainMenu(&GW->get_menubar());
    GW->get_menubar().accelerate(*TW);

    TW->show_all();
    GW->show_all();

    SS.Init();

    if(argc >= 2) {
        if(argc > 2) {
            std::cerr << "Only the first file passed on command line will be opened."
                      << std::endl;
        }

        SS.OpenFile(argv[1]);
    }

    main.run(*GW);

    delete GW;
    delete TW;

    SK.Clear();
    SS.Clear();

    return 0;
}
