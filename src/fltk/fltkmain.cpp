//-----------------------------------------------------------------------------
// Our main() function, and FLTK-specific stuff to set up our windows and
// otherwise handle our interface to the operating system. Everything
// outside fltk/... should be standard C++ and OpenGL.
//
// Copyright 2008-2013 Jonathan Westhues.
// Copyright 2013 Daniel Richard G. <skunk@iSKUNK.ORG>
//-----------------------------------------------------------------------------
#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef HAVE_FONTCONFIG_FONTCONFIG_H
#   include <fontconfig/fontconfig.h>
#endif

#ifdef HAVE_LIBSPNAV
#   include <spnav.h>
#   ifndef SI_APP_FIT_BUTTON
#       define SI_APP_FIT_BUTTON 31
#   endif
#endif

#include <FL/Fl_Gl_Window.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/forms.H> // for fl_gettime()
#include <FL/gl.h>
#include <FL/names.h>

#include "solvespace.h"

#define fl_snprintf snprintf

static Fl_Preferences *Preferences = NULL;

class Graphics_Gl_Window;
class Text_Gl_Window;

static Fl_Window          *GraphicsWnd = NULL;
static Graphics_Gl_Window *GraphicsGlWnd = NULL;
static Fl_Window          *GraphicsEditControlWindow = NULL;
static Fl_Input           *GraphicsEditControl = NULL;
static Fl_Sys_Menu_Bar    *MenuBar = NULL;
static Fl_Menu_Item        MenuBarItems[120];
static bool                MenuBarVisible = true;

// Static window object to hold the non-fullscreen size of GraphicsWnd
static Fl_Window GraphicsWndOldSize(100, 100);

static Fl_Window      *TextWnd = NULL;
static Text_Gl_Window *TextGlWnd = NULL;
static Fl_Scrollbar   *TextWndScrollBar = NULL;
static Fl_Input       *TextEditControl = NULL;

static struct {
    int x, y;
} LastMousePos = { 0, 0 };

char RecentFile[MAX_RECENT][MAX_PATH];
static Fl_Menu_Item RecentOpenMenu[MAX_RECENT+1], RecentImportMenu[MAX_RECENT+1];

static Fl_Menu_Item ContextMenu[100];
static int          ContextMenuCount = -1;
static Fl_Menu_Item ContextSubmenu[100];
static int          ContextSubmenuCount = -1;

static long StartTimeSeconds = 0;
static const Fl_Font SS_FONT_MONOSPACE = FL_FREE_FONT + 8;

#define GL_CHECK() \
    do { \
        int err = (int)glGetError(); \
        if(err) dbp("%s:%d: glGetError() == 0x%X\n", __FILE__, __LINE__, err); \
    } while (0)

void DoMessageBox(const char *str, int rows, int cols, bool error)
{
    fl_message_title(error ? "SolveSpace - Error" : "SolveSpace - Message");
    if(error)
        fl_alert("%s", str);
    else
        fl_message("%s", str);
}

void AddContextMenuItem(const char *label, int id)
{
    if(ContextMenuCount < 0) {
        ZERO(ContextMenu);
        ContextMenuCount = 0;
    }

    // ContextMenu and ContextSubmenu are fixed-size arrays, because
    // dynamic Fl_Menu_Item arrays are a PITA to work with
    if(ContextMenuCount + 2 > (int)arraylen(ContextMenu)) oops();
    if(ContextSubmenuCount > 0) {
        if(ContextSubmenuCount + 2 > (int)arraylen(ContextSubmenu)) oops();
        if(ContextMenuCount + ContextSubmenuCount + 3 > (int)arraylen(ContextMenu)) oops();
    }

    if(id == CONTEXT_SUBMENU) {
        if(ContextSubmenuCount <= 0) oops();

        Fl_Menu_Item *mi = ContextMenu + ContextMenuCount;
        mi->label(label);
        mi->flags = FL_SUBMENU;
        ContextMenuCount++;

        memcpy(ContextMenu + ContextMenuCount,
               ContextSubmenu,
               ContextSubmenuCount * sizeof(Fl_Menu_Item));
        ContextMenuCount += ContextSubmenuCount + 1;
        // (the +1 is for the null item that ends the submenu)
        ContextSubmenuCount = -1;
    } else {
        Fl_Menu_Item *mi = ContextSubmenuCount >= 0 ?
            ContextSubmenu + ContextSubmenuCount :
            ContextMenu    + ContextMenuCount;

        int *cnt = ContextSubmenuCount >= 0 ?
            &ContextSubmenuCount : &ContextMenuCount;

        if(id == CONTEXT_SEPARATOR) {
            if(*cnt < 1) oops();
            (mi - 1)->flags |= FL_MENU_DIVIDER;
        } else {
            mi->label(label);
            mi->argument(id);
            ++*cnt;
        }
    }
}

void CreateContextSubmenu(void)
{
    if(ContextSubmenuCount >= 0) oops();
    ZERO(ContextSubmenu);
    ContextSubmenuCount = 0;
}

int ShowContextMenu(void)
{
    int r = 0;
    if(ContextMenuCount > 0) {
        const Fl_Menu_Item *mi =
            ContextMenu->popup(Fl::event_x(), Fl::event_y());
        if(mi) r = (int)mi->argument();
        ContextMenuCount = -1;
    }
    return r;
}

static void TimerCallback(void *)
{
    SS.GW.TimerCallback();
    SS.TW.TimerCallback();
}

void SetTimerFor(int milliseconds)
{
    Fl::add_timeout((double)milliseconds / 1000.0, TimerCallback);
}

void OpenWebsite(const char *url)
{
    fl_open_uri(url, NULL, 0);
}

void ExitNow(void)
{
    // This will make Fl::wait() return zero
    GraphicsWnd->hide();
    TextWnd->hide();
}

//-----------------------------------------------------------------------------
// Helpers so that we can read/write preference keys from the platform-
// independent code.
//-----------------------------------------------------------------------------
void CnfFreezeString(const char *str, const char *name)
{
    if(Preferences) Preferences->set(name, str);
}

void CnfFreezeInt(uint32_t v, const char *name)
{
    if(Preferences) Preferences->set(name, (int)v);
}

void CnfFreezeFloat(float v, const char *name)
{
    if(Preferences) Preferences->set(name, v);
}

static void CnfFreezeWindowPos(Fl_Window *wnd, const char *name)
{
    char buf[100];
    fl_snprintf(buf, sizeof(buf), "%s_left", name);
    CnfFreezeInt(wnd->x(), buf);
    fl_snprintf(buf, sizeof(buf), "%s_top", name);
    CnfFreezeInt(wnd->y(), buf);
    fl_snprintf(buf, sizeof(buf), "%s_width", name);
    CnfFreezeInt(wnd->w(), buf);
    fl_snprintf(buf, sizeof(buf), "%s_height", name);
    CnfFreezeInt(wnd->h(), buf);
}

void CnfThawString(char *str, int maxLen, const char *name)
{
    char *def = strdup(str);
    if(Preferences) Preferences->get(name, str, def, maxLen - 1);
    free(def);
}

uint32_t CnfThawInt(uint32_t v, const char *name)
{
    int r = 0;
    if(Preferences) Preferences->get(name, r, (int)v);
    return (uint32_t)r;
}

float CnfThawFloat(float v, const char *name)
{
    float r = 0.0;
    if(Preferences) Preferences->get(name, r, v);
    return r;
}

static void CnfThawWindowPos(Fl_Window *wnd, const char *name)
{
    char buf[100];
    fl_snprintf(buf, sizeof(buf), "%s_left", name);
    int x = CnfThawInt(wnd->x(), buf);
    fl_snprintf(buf, sizeof(buf), "%s_top", name);
    int y = CnfThawInt(wnd->y(), buf);
    fl_snprintf(buf, sizeof(buf), "%s_width", name);
    int w = CnfThawInt(wnd->w(), buf);
    fl_snprintf(buf, sizeof(buf), "%s_height", name);
    int h = CnfThawInt(wnd->h(), buf);

#define MARGIN 32

    if(x < -MARGIN || y < -MARGIN) return;
    if(x > Fl::w() - MARGIN || y > Fl::h() - MARGIN) return;
    if(w < 100 || h < 100) return;

#undef MARGIN

    wnd->resize(x, y, w, h);
}

static void LoadPreferences(void)
{
    const char *xchome, *home;
    char dir[MAX_PATH];
    int r = 0;

    // Refer to http://standards.freedesktop.org/basedir-spec/latest/

    xchome = fl_getenv("XDG_CONFIG_HOME");
    home   = fl_getenv("HOME");

    if(xchome)
        r = fl_snprintf(dir, sizeof(dir), "%s/solvespace", xchome);
    else if(home)
        r = fl_snprintf(dir, sizeof(dir), "%s/.config/solvespace", home);
    else
        return;

    if(r >= (int)sizeof(dir))
        return;

    if(!fl_filename_isdir(dir) && mkdir(dir, 0777) != 0) {
        r = fl_snprintf(dir, sizeof(dir), "%s/.solvespace", home);
        if(r >= (int)sizeof(dir))
            return;
        if(!fl_filename_isdir(dir))
            if(mkdir(dir, 0777) != 0)
                return;
    }

    Preferences = new Fl_Preferences(dir, "solvespace.org", "solvespace");
}

void SetWindowTitle(const char *str) {
    GraphicsWnd->label(str);
}

void SetMousePointerToHand(bool yes) {
    Fl_Cursor cur = yes ? FL_CURSOR_HAND : FL_CURSOR_ARROW;
    GraphicsWnd->cursor(cur);
    TextWnd->cursor(cur);
}

void MoveTextScrollbarTo(int pos, int maxPos, int page)
{
    TextWndScrollBar->value(pos, page, 0, maxPos);
}

static void HandleTextWindowScrollBar(Fl_Widget *w)
{
    if(w != TextWndScrollBar) oops();
    SS.TW.ScrollbarEvent(TextWndScrollBar->value());
}

void ShowTextWindow(bool visible)
{
    if(visible) {
        TextWnd->show();

#ifdef InputHint
        {
            // Prevent the text window from gaining window manager focus by
            // setting the appropriate WM hint via direct X calls

            XWMHints *hints = XAllocWMHints();
            hints->input = False;
            hints->flags = InputHint;
            XSetWMHints(fl_display, fl_xid(TextWnd), hints);
            XFree(hints);

            // In case the text window got the focus before we could set
            // the hint, switch the focus back to the graphics window

            Window xid = 0;
            int revert_to = 0;
            XGetInputFocus(fl_display, &xid, &revert_to);
            if(xid == fl_xid(TextWnd)) {
                XSetInputFocus(
                    fl_display,
                    fl_xid(GraphicsWnd),
                    RevertToParent,
                    CurrentTime);
            }
        }
#endif
    } else {
        TextWnd->hide();
    }
}

int64_t GetMilliseconds(void)
{
    long sec = StartTimeSeconds, usec = 0;
    fl_gettime(&sec, &usec);
    if(!StartTimeSeconds) StartTimeSeconds = sec;
    sec -= StartTimeSeconds;
    return 1000 * (int64_t)sec + (int64_t)usec / 1000;
}

int64_t GetUnixTime(void)
{
    time_t ret;
    time(&ret);
    return (int64_t)ret;
}

class Graphics_Gl_Window : public Fl_Gl_Window
{
public:

    Graphics_Gl_Window(int x, int y, int w, int h)
    : Fl_Gl_Window(x, y, w, h)
    {
        mode(FL_RGB | FL_DOUBLE);
    }

    virtual int handle(int event) {
        if(handle_gl(event) == 0) {
            return Fl_Gl_Window::handle(event);
        } else {
            return 1;
        }
    }

    int handle_gl(int event)
    {
        switch(event)
        {
#ifdef HAVE_LIBSPNAV
            case FL_NO_EVENT: {
                spnav_event sev;
                if(!spnav_x11_event(fl_xevent, &sev)) break;
                switch(sev.type) {
                    case SPNAV_EVENT_MOTION:
                        SS.GW.SpaceNavigatorMoved(
                            (double)sev.motion.x,
                            (double)sev.motion.y,
                            (double)sev.motion.z  * -1.0,
                            (double)sev.motion.rx *  0.001,
                            (double)sev.motion.ry *  0.001,
                            (double)sev.motion.rz * -0.001,
                            Fl::event_shift());
                        break;

                    case SPNAV_EVENT_BUTTON:
                        if(!sev.button.press && sev.button.bnum == SI_APP_FIT_BUTTON) {
                            SS.GW.SpaceNavigatorButtonUp();
                        }
                        break;
                }
                return 1;
            }
#endif // HAVE_LIBSPNAV

            case FL_PUSH:    // mouse button click...
            case FL_RELEASE: // ...and release
            case FL_DRAG:
            case FL_MOVE: {
                int x = Fl::event_x();
                int y = Fl::event_y();

                // Convert to xy (vs. ij) style coordinates,
                // with (0, 0) at center
                x = x - w() / 2;
                y = h() / 2 - y;

                LastMousePos.x = x;
                LastMousePos.y = y;

                // Don't go any further if the OpenGL context hasn't been
                // initialized/updated by a draw()
                if(!valid()) return 1;

                if(event == FL_DRAG || event == FL_MOVE) {
                    int state = Fl::event_state();
                    SS.GW.MouseMoved(x, y,
                        state & FL_BUTTON1,
                        state & FL_BUTTON2,
                        state & FL_BUTTON3,
                        state & FL_SHIFT,
                        state & FL_CTRL);
                    return 1;
                }

#if FL_RIGHT_MOUSE != 3
#  error "MOUSE() macro may need revising"
#endif
#define MOUSE(btn,ev) (16 * ev + btn)

                switch(MOUSE(Fl::event_button(), event))
                {
                    case MOUSE(FL_LEFT_MOUSE, FL_PUSH):
                        if(Fl::event_clicks()) {
                            SS.GW.MouseLeftDoubleClick(x, y);
                        } else {
                            SS.GW.MouseLeftDown(x, y);
                        }
                        break;

                    case MOUSE(FL_LEFT_MOUSE, FL_RELEASE):
                        SS.GW.MouseLeftUp(x, y); break;

                    case MOUSE(FL_MIDDLE_MOUSE, FL_PUSH):
                    case MOUSE(FL_RIGHT_MOUSE,  FL_PUSH):
                        SS.GW.MouseMiddleOrRightDown(x, y); break;

                    case MOUSE(FL_MIDDLE_MOUSE, FL_RELEASE):
                        /* Not used */ break;

                    case MOUSE(FL_RIGHT_MOUSE, FL_RELEASE):
                        SS.GW.MouseRightUp(x, y); break;

                    default: break;
                }
#undef MOUSE
                return 1;
            }

            case FL_ENTER:
                return 1;

            case FL_LEAVE:
                SS.GW.MouseLeave();
                return 1;

            case FL_FOCUS:
                return 1;

            case FL_UNFOCUS:
                return 1;

            case FL_KEYDOWN: {
                int key = Fl::event_key();
                int c = key;
                switch(key) {
                    case FL_Escape:
                        c = GraphicsWindow::ESCAPE_KEY;
                        break;
                    case FL_Delete:
                        c = GraphicsWindow::DELETE_KEY;
                        break;
                    case FL_Tab:
                        c = '\t';
                        break;

                    case FL_Back:
                    case FL_BackSpace:
                        c = '\b';
                        break;
                }
                if(key >= (FL_F+1) && key <= (FL_F+12)) {
                    c = GraphicsWindow::FUNCTION_KEY_BASE + (key - FL_F);
                }
                if(Fl::event_shift()) c |= GraphicsWindow::SHIFT_MASK;
                if(Fl::event_ctrl())  c |= GraphicsWindow::CTRL_MASK;

                if(SS.GW.KeyDown(c)) return 1;

                // No accelerator; process the key as normal.
                break;
            }

            case FL_KEYUP:
                return 1;

            case FL_CLOSE:
                // GraphicsGlWnd does not receive this event; we intercept
                // close events in WindowCloseHandler()
                oops();
                return 0;

            case FL_MOUSEWHEEL:
                SS.GW.MouseScroll(LastMousePos.x, LastMousePos.y, Fl::event_dy());
                return 1;
        }

        return 0;
    }

    virtual void resize(int x, int y,int w, int h) {
        Fl_Window::resize(x, y, w, h);
    }

protected:
    virtual void draw() {
        if(damage()) {
            draw_gl();
        }
    }

    void draw_gl(void)
    {
        // Actually paint the window, with gl.
        SS.GW.Paint();
        GL_CHECK();
    }

    virtual void dummy(void);
};

void Graphics_Gl_Window::dummy(void)
{
    // sop to Clang++'s -Wweak-vtables warning
}

void PaintGraphics(void)
{
    GraphicsGlWnd->redraw();
}

void InvalidateGraphics(void)
{
    GraphicsGlWnd->redraw();
}

void ToggleFullScreen(void)
{
#ifdef HAVE_FLTK_FULLSCREEN
    if(GraphicsWnd->fullscreen_active()) {
        GraphicsWnd->fullscreen_off();
    } else {
        GraphicsWndOldSize.resize(
            GraphicsWnd->x(),
            GraphicsWnd->y(),
            GraphicsWnd->w(),
            GraphicsWnd->h());

        GraphicsWnd->fullscreen();
    }
#endif
}

bool FullScreenIsActive(void)
{
#ifdef HAVE_FLTK_FULLSCREEN
    return GraphicsWnd->fullscreen_active();
#else
    return false;
#endif
}

void GetGraphicsWindowSize(int *w, int *h)
{
    *w = GraphicsGlWnd->w();
    *h = GraphicsGlWnd->h();
}

void ToggleMenuBar(void)
{
    int y = 0;

    MenuBarVisible = !MenuBarVisible;

    // We hide the menu bar by expanding the GL area over it, instead of
    // calling hide(). This way, F10/Alt+F/etc. remain usable.

    if(MenuBarVisible) y = MenuBar->h();

    GraphicsGlWnd->resize(
        0, y,
        GraphicsWnd->w(), GraphicsWnd->h() - y);

    // Make GraphicsWnd forget about the previous sizes of its children, or
    // else the menu bar will {dis,re}appear when the window is resized
    GraphicsWnd->init_sizes();
}

bool MenuBarIsVisible(void)
{
    return MenuBarVisible;
}

class Text_Gl_Window : public Fl_Gl_Window
{
public:

    Text_Gl_Window(int x, int y, int w, int h)
    : Fl_Gl_Window(x, y, w, h)
    {
        mode(FL_RGB | FL_DOUBLE);
    }

    virtual int handle(int event) {
        if(handle_gl(event) == 0) {
            return Fl_Gl_Window::handle(event);
        } else {
            return 1;
        }
    }

    int handle_gl(int event)
    {
        switch(event)
        {
            case FL_PUSH:  // mouse button click
            case FL_MOVE:
                if(valid()) {
                    SS.TW.MouseEvent(
                        event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE,
                        Fl::event_button1(),
                        Fl::event_x(), Fl::event_y());
                }
                return 1;

            case FL_ENTER:
            case FL_FOCUS:
                return 1;

            case FL_LEAVE:
                SS.TW.MouseLeave();
                return 1;

            case FL_KEYDOWN:
            case FL_KEYUP:
                return GraphicsGlWnd->handle(event);

            case FL_CLOSE:
                // TextGlWnd does not receive this event; we intercept
                // close events in WindowCloseHandler()
                oops();
                return 0;

            case FL_MOUSEWHEEL:
                return TextWndScrollBar->handle(event);
        }

        return 0;
    }

    virtual void resize(int x, int y,int w, int h) {
        Fl_Window::resize(x, y, w, h);
    }

protected:
    virtual void draw() {
        if(damage()) {
            draw_gl();
        }
    }

    void draw_gl(void)
    {
        // Actually paint the text window, with gl.
        SS.TW.Paint();
        GL_CHECK();
    }

    virtual void dummy(void);
};

void Text_Gl_Window::dummy(void)
{
    // sop to Clang++'s -Wweak-vtables warning
}

void ShowTextEditControl(int x, int y, char *s)
{
    if(GraphicsEditControlIsVisible()) return;

    // Note: TextEditControl->position() does NOT set (x,y) position!
    // It changes the cursor postion in the input widget
    // Use the :: operator to avoid confusion
    TextEditControl->Fl_Widget::position(x, y);
    if(s) TextEditControl->value(s);
    TextEditControl->show();

    // Grab focus and select all to ease editing
    TextEditControl->take_focus();
    TextEditControl->Fl_Input::position(TextEditControl->size());
    TextEditControl->mark(0);

}

void HideTextEditControl(void)
{
    // return focus to the GL window before hiding the widget
    // this avoid the disapparition of the mouse pointer if it was over the widget
    TextGlWnd->take_focus();

    TextEditControl->hide();
}

bool TextEditControlIsVisible(void)
{
    return TextEditControl->visible();
}

void ShowGraphicsEditControl(int x, int y, char *s)
{
    if(GraphicsEditControlIsVisible()) return;

    // Convert to ij (vs. xy) style coordinates,
    // and compensate for the input widget height due to inverse coord
    x = x + GraphicsGlWnd->w()/2 + GraphicsWnd->x();
    y = -y + GraphicsGlWnd->h()/2 - GraphicsEditControlWindow->h() + GraphicsWnd->y();

    GraphicsEditControlWindow->position(x, y);
    GraphicsEditControl->value(s);
    GraphicsEditControlWindow->show();

    // Grab focus and select all to ease editing
    GraphicsEditControl->take_focus();
    GraphicsEditControl->Fl_Input::position(GraphicsEditControl->size());
    GraphicsEditControl->mark(0);

}

void HideGraphicsEditControl(void)
{
    // return focus to the GL window before hiding the widget
    // this avoid the disapparition of the mouse pointer if it was over the widget
    GraphicsGlWnd->take_focus();

    GraphicsEditControlWindow->hide();
}

bool GraphicsEditControlIsVisible(void)
{
    return GraphicsEditControlWindow->visible();
}

void InvalidateText(void)
{
    TextGlWnd->redraw();
}

void GetTextWindowSize(int *w, int *h)
{
    *w = TextGlWnd->w();
    *h = TextGlWnd->h();
}

static void EditControlCallback(Fl_Widget *w)
{
    if(w == GraphicsEditControl) {
        SS.GW.EditControlDone(GraphicsEditControl->value());
    } else if(w == TextEditControl) {
        SS.TW.EditControlDone(TextEditControl->value());
    } else {
        oops();
    }
}

static void WindowCloseHandler(Fl_Window *wnd, void *data)
{
    if(wnd == GraphicsWnd) {
        SolveSpace::MenuFile(GraphicsWindow::MNU_EXIT);
    }
    else if(wnd == GraphicsEditControlWindow) {
        HideGraphicsEditControl();
    }
    else if(wnd == TextWnd) {
        if(SS.GW.showTextWindow) {
            GraphicsWindow::MenuView(GraphicsWindow::MNU_SHOW_TEXT_WND);
        }
    } else {
        oops();
    }
}

//-----------------------------------------------------------------------------
// Common dialog routines, to open or save a file.
//-----------------------------------------------------------------------------
bool GetOpenFile(char *file, const char *defExtension, const char *selPattern)
{
#ifdef USE_FLTK_FILE_CHOOSER
    char *f = fl_file_chooser(
        "Open File",
        selPattern,
        file[0] ? file : NULL,
        0);
    if(strlen(f)+1 > MAX_PATH) return false;
    strcpy(file, f);
    return true;
#else
    Fl_Native_File_Chooser fc;
    fc.title("Open File");
    fc.type(Fl_Native_File_Chooser::BROWSE_FILE);
    if(file[0]) fc.preset_file(file);
    fc.filter(selPattern);
    fc.options(Fl_Native_File_Chooser::PREVIEW);
    if(fc.show() != 0) return false;
    if(strlen(fc.filename())+1 > MAX_PATH) return false;
    strcpy(file, fc.filename());
    return true;
#endif
}

bool GetSaveFile(char *file, const char *defExtension, const char *selPattern)
{
#ifdef USE_FLTK_FILE_CHOOSER
    char *f = fl_file_chooser(
        "Save File",
        selPattern,
        file[0] ? file : NULL,
        0);
    if(strlen(f)+1 > MAX_PATH) return false;
    strcpy(file, f);
    return true;
#else
    Fl_Native_File_Chooser fc;
    fc.title("Save File");
    fc.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    if(file[0]) fc.preset_file(file);
    fc.filter(selPattern);
    fc.options(
        Fl_Native_File_Chooser::NEW_FOLDER |
        Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    if(fc.show() != 0) return false;
    if(strlen(fc.filename())+1 > MAX_PATH) return false;
    strcpy(file, fc.filename());
    return true;
#endif
}

int SaveFileYesNoCancel(void)
{
    int ycn[] = { SAVE_YES, SAVE_CANCEL, SAVE_NO };
    int r;

    fl_message_title("SolveSpace");
    r = fl_choice(
        "The program has changed since it was last saved.\n\n"
        "Do you want to save the changes?",
        "Yes",
        "Cancel",  // default
        "No");

    return ycn[r];
}

#ifndef HAVE_FONTCONFIG
static void ScanFontDirectory(const char *dir)
{
    dirent **list = NULL;
    char path[MAX_PATH];

    int n = fl_filename_list(dir, &list, fl_alphasort);
    if(n < 0)
        return;

    for(int i = 0; i < n; i++)
    {
        int len = fl_snprintf(path, sizeof(path), "%s/%s", dir, list[i]->d_name);
        if(len >= MAX_PATH) continue;

        if(fl_filename_isdir(path)) {
            ScanFontDirectory(path);
        }
        else if(fl_filename_match(path, "*.{TTF,ttf}")) {
            TtfFont tf;
            ZERO(&tf);
            strcpy(tf.fontFile, path);
            SS.fonts.l.Add(&tf);
        }
    }

    fl_filename_free_list(&list, n);
}
#endif // ndef HAVE_FONTCONFIG

void LoadAllFontFiles(void)
{
#ifdef HAVE_FONTCONFIG

    if(!FcInit())
        return;

    FcPattern   *pat = FcPatternCreate();
    FcObjectSet *os  = FcObjectSetBuild(FC_FILE, (char *)0);
    FcFontSet   *fs  = FcFontList(0, pat, os);

    for(int i = 0; i < fs->nfont; i++) {
        char *s = (char*)FcPatternFormat(fs->fonts[i], (FcChar8*) "%{file}");
        if(strlen(s)+1 <= MAX_PATH && fl_filename_match(s, "*.{TTF,ttf}")) {
            TtfFont tf;
            ZERO(&tf);
            strcpy(tf.fontFile, s);
            SS.fonts.l.Add(&tf);
        }
        FcStrFree((FcChar8*) s);
    }

    FcFontSetDestroy(fs);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);
    FcFini();

#else

#  ifdef __APPLE__
    ScanFontDirectory("/System/Library/Fonts");
    ScanFontDirectory("/Library/Fonts");
#  else
    ScanFontDirectory("/usr/lib/X11/fonts");
    ScanFontDirectory("/usr/openwin/lib/X11/fonts/TrueType");
    ScanFontDirectory("/usr/share/fonts/truetype");
#  endif

#endif
}

enum {
    CHECK,
    RADIO,
    ACTIVE
};

static void MenuById(int id, bool yes, int what)
{
    const Fl_Menu_Item *menu = MenuBar->menu();
    int size = MenuBar->size();

    for(int i = 0; i < size; i++) {
        const Fl_Menu_Item *m = &menu[i];
        if(m->submenu()) continue;
        if(m->argument() != (long)id) continue;

        int flags = MenuBar->mode(i);
        switch(what) {
            case CHECK:
                if(!m->checkbox()) flags |= FL_MENU_TOGGLE;
                if(yes) flags |=  FL_MENU_VALUE;
                else    flags &= ~FL_MENU_VALUE;
                break;
            case RADIO:
                if(!m->radio()) flags |= FL_MENU_RADIO;
                if(yes) flags |=  FL_MENU_VALUE;
                else    flags &= ~FL_MENU_VALUE;
                break;
            case ACTIVE:
                if(yes) flags &= ~FL_MENU_INACTIVE;
                else    flags |=  FL_MENU_INACTIVE;
                break;
            default: oops(); break;
        }
        MenuBar->mode(i, flags);
        return;
    }
    oops();
}

void CheckMenuById(int id, bool checked)
{
    MenuById(id, checked, CHECK);
}

void RadioMenuById(int id, bool selected)
{
    MenuById(id, selected, RADIO);
}

void EnableMenuById(int id, bool enabled)
{
    MenuById(id, enabled, ACTIVE);
}

static void RecentMenuCallback(Fl_Widget *w, long data)
{
    int id = (int)data;
    if((id >= RECENT_OPEN && id < (RECENT_OPEN + MAX_RECENT))) {
        SolveSpace::MenuFile(id);
    }
    else if((id >= RECENT_IMPORT && id < (RECENT_IMPORT + MAX_RECENT))) {
        Group::MenuGroup(id);
    }
}

static void DoRecent(Fl_Menu_Item *m, int base)
{
    int c = 0;
    for(int i = 0; i < MAX_RECENT; i++) {
        char *s = RecentFile[i];
        if(*s) {
            m[c].label(s);
            m[c].callback(RecentMenuCallback);
            m[c].argument(base + i);
            c++;
        }
    }
    if(c == 0) {
        m[0].label("(no recent files)");
        m[0].deactivate();
    }
}

void RefreshRecentMenus(void)
{
    ZERO(RecentOpenMenu);
    ZERO(RecentImportMenu);
    DoRecent(RecentOpenMenu,   RECENT_OPEN);
    DoRecent(RecentImportMenu, RECENT_IMPORT);
}

static void GraphicsWndMenuCallback(Fl_Widget *w, long data)
{
    int id = (int)data;

    for(int i = 0; SS.GW.menu[i].level >= 0; i++)
        if(SS.GW.menu[i].id == id)
            return SS.GW.menu[i].fn(id);
    oops();
}

static void CreateGraphicsWindowMenus(void)
{
    MenuBar = new Fl_Sys_Menu_Bar(0, 0, GraphicsWnd->w(), 100);
    if(!MenuBar) oops();

    RefreshRecentMenus();
    ZERO(MenuBarItems);

    int c = 0;
    for(int i = 0; SS.GW.menu[i].level >= 0; i++) {
        int accel = SS.GW.menu[i].accel;
        int shortcut = accel & 0xff;
        if(shortcut >= 'A' && shortcut <= 'Z') shortcut |= 0x20;
        switch(shortcut) {
            case GraphicsWindow::ESCAPE_KEY:
                shortcut = FL_Escape;
                break;
            case GraphicsWindow::DELETE_KEY:
                shortcut = FL_Delete;
                break;
            default:
                if(accel & GraphicsWindow::SHIFT_MASK)
                    shortcut += FL_SHIFT;
                if(accel & GraphicsWindow::CTRL_MASK)
                    shortcut += FL_COMMAND;
                break;
        }
        if(accel >= (GraphicsWindow::FUNCTION_KEY_BASE + 1) &&
           accel <= (GraphicsWindow::FUNCTION_KEY_BASE + 12)) {
            shortcut = FL_F + (accel - GraphicsWindow::FUNCTION_KEY_BASE);
        }

        Fl_Menu_Item *m = &MenuBarItems[c];
        switch(SS.GW.menu[i].level) {
            case 0:
                m->label(SS.GW.menu[i].label);
                m->shortcut(shortcut);
                m->flags = FL_SUBMENU;
                c++;
                break;

            case 1:
                if(!SS.GW.menu[i].label) break;  // divider
                m->label(SS.GW.menu[i].label);
                m->shortcut(shortcut);
                switch(SS.GW.menu[i].id) {
                    case GraphicsWindow::MNU_OPEN_RECENT:
                        m->user_data(RecentOpenMenu);
                        m->flags = FL_SUBMENU_POINTER;
                        break;
                    case GraphicsWindow::MNU_GROUP_RECENT:
                        m->user_data(RecentImportMenu);
                        m->flags = FL_SUBMENU_POINTER;
                        break;
                    default:
                        m->callback(GraphicsWndMenuCallback);
                        m->argument(SS.GW.menu[i].id);
                        m->flags = SS.GW.menu[i+1].label
                            || SS.GW.menu[i+1].level < 0 ? 0 : FL_MENU_DIVIDER;
                        break;
                }
                c++;
                break;

            default: oops(); break;
        }

        if(!SS.GW.menu[i+1].level) {
            if(!SS.GW.menu[i].label) oops();
            c++;  // leave null item to end current submenu
        }
    }

    // Make F10 bring up the File menu
    MenuBarItems[0].shortcut(FL_F+10);

    MenuBar->menu(MenuBarItems);
    MenuBar->size(MenuBar->w(), 2 * MenuBar->textsize());  // fudge
    MenuBar->global();
}

static void CreateMainWindows(void)
{
    // Graphics window

    GraphicsWnd = new Fl_Window(
        3 * Fl::w() / 4, 3 * Fl::h() / 4,
        "SolveSpace (not yet saved)");
    if(!GraphicsWnd) oops();

    CreateGraphicsWindowMenus();

    // Avoid momentary grey flicker
    GraphicsWnd->color(FL_BLACK);

    GraphicsGlWnd = new Graphics_Gl_Window(
        0, MenuBar->h(),
        GraphicsWnd->w(), GraphicsWnd->h() - MenuBar->h());

    GraphicsWnd->resizable(GraphicsGlWnd);
    GraphicsWnd->size_range(Fl::w() / 4, Fl::h() / 4);

    GraphicsGlWnd->end();
    GraphicsWnd->end();

    int w = 120;
    int h = 30;
    int padding = 5;

    GraphicsEditControlWindow = new Fl_Window(
        w + (padding * 2), h + (padding * 2),
        ""
    );

    GraphicsEditControlWindow->set_tooltip_window();

    GraphicsEditControl = new Fl_Input(
        (GraphicsEditControlWindow->w() / 2) - (w / 2),
        (GraphicsEditControlWindow->h() / 2) - (h / 2),
        w,
        h
    );

    GraphicsEditControl->textfont(SS_FONT_MONOSPACE);
    GraphicsEditControl->callback(EditControlCallback);
    GraphicsEditControl->when(FL_WHEN_ENTER_KEY | FL_WHEN_NOT_CHANGED);

    GraphicsEditControlWindow->end();
    GraphicsEditControlWindow->hide();

    // Text window

    TextWnd = new Fl_Window(480, 320, "SolveSpace - Browser");
    if(!TextWnd) oops();

    TextWnd->color(FL_BLACK);

    TextWndScrollBar = new Fl_Scrollbar(
        TextWnd->w() - Fl::scrollbar_size(), 0,
        Fl::scrollbar_size(), TextWnd->h());

    //TextWndScrollBar->value(0, 1, 0, 1);
    TextWndScrollBar->callback(HandleTextWindowScrollBar);

    TextGlWnd = new Text_Gl_Window(
        0, 0,
        TextWnd->w() - TextWndScrollBar->w(), TextWnd->h());

    TextWnd->resizable(TextGlWnd);
    TextWnd->size_range(Fl::w() / 8, Fl::h() / 8);

    // We get the desired Alt+Tab behaviour by specifying that the text
    // window is "non-modal".

    TextWnd->set_non_modal();

    TextEditControl = new Fl_Input(
        0, 0,
        20 * TextWindow::CHAR_WIDTH, TextWindow::LINE_HEIGHT);
    TextEditControl->textfont(SS_FONT_MONOSPACE);
    TextEditControl->callback(EditControlCallback);
    TextEditControl->when(FL_WHEN_ENTER_KEY | FL_WHEN_NOT_CHANGED);
    TextEditControl->hide();

    TextGlWnd->end();
    TextWnd->end();

    Fl::set_atclose(WindowCloseHandler);
}

static void LoadFixedFont(void)
{
    const char *names[] = {
        "DejaVu Sans Mono",
        "Bitstream Vera Sans Mono",
        "Liberation Mono",
        "monospace",
        NULL
    };
    int i;

    for (i = 0; names[i] != NULL; i++)
    {
        Fl::set_font(SS_FONT_MONOSPACE, names[i]);
        fl_font(SS_FONT_MONOSPACE, 144);
        if (fl_width("abcd1234") >= 1.0)
            return;
    }

    oops();
}

static int ArgHandler(int argc, char **argv, int &i)
{
    fprintf(stderr, "option %d = '%s'\n", i, argv[i]);



    return 0;
}

//-----------------------------------------------------------------------------
// Entry point into the program.
//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
    // Parse command-line options
    int optndx = 0;
    if (!Fl::args(argc, argv, optndx, ArgHandler)) {
        Fl::fatal(Fl::help);
    }

    // Initialize StartTimeSeconds
    GetMilliseconds();

#ifndef USE_FLTK_FILE_CHOOSER
    // The docs for Fl_Native_File_Chooser recommend doing this
    Fl_File_Icon::load_system_icons();
#endif

    // Don't make message dialogs show up under the mouse pointer
    fl_message_hotspot(0);

    LoadPreferences();

    // A monospaced font
    LoadFixedFont();

    // Create the root windows: one for control, with text, and one for
    // the graphics
    CreateMainWindows();

    CnfThawWindowPos(TextWnd, "TextWindow");
    CnfThawWindowPos(GraphicsWnd, "GraphicsWindow");

    GraphicsWnd->show(argc, argv);
    ShowTextWindow(true);

    // Don't use the default (FL_CURSOR_DEFAULT) arrow pointer, as it can't
    // be resized by the user
    SetMousePointerToHand(false);

    // A filename may have been specified on the command line; if so, then
    // make it absolute.
    char file[MAX_PATH] = "";
    if(optndx < argc && strlen(argv[optndx])+1 < MAX_PATH) {
        strcpy(file, argv[optndx]);
    }
    if(*file != '\0') {
        GetAbsoluteFilename(file);
    }

#ifdef HAVE_LIBSPNAV
    bool spacenavd_active =
        spnav_x11_open(fl_display, fl_xid(GraphicsWnd)) == 0;
#endif

    // Call in to the platform-independent code, and let them do their init
    SS.Init(file);

    // And now it's the main event loop. All calls in to the rest of the
    // code will be from the callbacks.
    for(;;) {
        // This call to Fl::first_window() ensures that Fl::flush() draws
        // TextGlWnd before TextWnd, which allows MoveTextScrollbarTo()
        // (normally called while TextGlWnd is being drawn) to update the
        // text-window scrollbar (a child widget of TextWnd) immediately,
        // rather than being delayed until the next redraw. This has to be
        // done at every iteration because FLTK constantly updates the
        // window order, placing those which received events most recently
        // at the beginning of the list.
        Fl::first_window(TextGlWnd);
        if(!Fl::wait()) break;
// TODO: invoke DoLater() at the right time
        SS.DoLater();
    }

#ifdef HAVE_LIBSPNAV
    if(spacenavd_active) {
        spnav_close();
    }
#endif

    // Write everything back into preferences
    CnfFreezeWindowPos(TextWnd, "TextWindow");
    CnfFreezeWindowPos(
#ifdef HAVE_FLTK_FULLSCREEN
        GraphicsWnd->fullscreen_active() ? &GraphicsWndOldSize : GraphicsWnd,
#else
        GraphicsWnd,
#endif
        "GraphicsWindow");

    delete TextWnd;
    delete GraphicsWnd;
    delete Preferences;

    // Free the memory we've used; anything that remains is a leak.
    SK.Clear();
    SS.Clear();

    return 0;
}
