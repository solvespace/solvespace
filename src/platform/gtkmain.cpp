//-----------------------------------------------------------------------------
// Our main() function, and GTK2/3-specific stuff to set up our windows and
// otherwise handle our interface to the operating system. Everything
// outside platform/... should be standard C++ and OpenGL.
//
// Copyright 2015 <whitequark@whitequark.org>
//-----------------------------------------------------------------------------
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include <json-c/json_object.h>
#include <json-c/json_util.h>

#include <glibmm/main.h>
#include <glibmm/convert.h>
#include <giomm/file.h>
#include <gdkmm/cursor.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/glarea.h>
#include <gtkmm/scrollbar.h>
#include <gtkmm/entry.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/hvbox.h>
#include <gtkmm/fixed.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/radiobuttongroup.h>
#include <gtkmm/menu.h>
#include <gtkmm/menubar.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/main.h>

#include <cairomm/xlib_surface.h>
#include <pangomm/fontdescription.h>
#include <gdk/gdkx.h>
#include <fontconfig/fontconfig.h>

#include <GL/glx.h>

#include "solvespace.h"
#include "config.h"

#ifdef HAVE_SPACEWARE
#include <spnav.h>
#endif

namespace SolveSpace {
/* Utility functions */
std::string Title(const std::string &s) {
    return "SolveSpace - " + s;
}

/* Save/load */

static std::string ConvertFilters(std::string active, const FileFilter ssFilters[],
                                  Gtk::FileChooser *chooser) {
    for(const FileFilter *ssFilter = ssFilters; ssFilter->name; ssFilter++) {
        Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
        filter->set_name(Translate(ssFilter->name));

        bool is_active = false;
        std::string desc = "";
        for(const char *const *ssPattern = ssFilter->patterns; *ssPattern; ssPattern++) {
            std::string pattern = "*." + std::string(*ssPattern);
            filter->add_pattern(pattern);
            filter->add_pattern(Glib::ustring(pattern).uppercase());
            if(active == "")
                active = pattern.substr(2);
            if("*." + active == pattern)
                is_active = true;
            if(desc == "")
                desc = pattern;
            else
                desc += ", " + pattern;
        }
        filter->set_name(filter->get_name() + " (" + desc + ")");

        chooser->add_filter(filter);
        if(is_active)
            chooser->set_filter(filter);
    }

    return active;
}

bool GetOpenFile(Platform::Path *filename, const std::string &activeOrEmpty,
                 const FileFilter filters[]) {
    Gtk::FileChooserDialog chooser(*(Gtk::Window *)SS.GW.window->NativePtr(),
                                   Title(C_("title", "Open File")));
    chooser.set_filename(filename->raw);
    chooser.add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
    chooser.add_button(_("_Open"), Gtk::RESPONSE_OK);
    chooser.set_current_folder(Platform::GetSettings()->ThawString("FileChooserPath"));

    ConvertFilters(activeOrEmpty, filters, &chooser);

    if(chooser.run() == Gtk::RESPONSE_OK) {
        Platform::GetSettings()->FreezeString("FileChooserPath", chooser.get_current_folder());
        *filename = Platform::Path::From(chooser.get_filename());
        return true;
    } else {
        return false;
    }
}

static void ChooserFilterChanged(Gtk::FileChooserDialog *chooser)
{
    /* Extract the pattern from the filter. GtkFileFilter doesn't provide
       any way to list the patterns, so we extract it from the filter name.
       Gross. */
    std::string filter_name = chooser->get_filter()->get_name();
    int lparen = filter_name.rfind('(') + 1;
    int rdelim = filter_name.find(',', lparen);
    if(rdelim < 0)
        rdelim = filter_name.find(')', lparen);
    ssassert(lparen > 0 && rdelim > 0, "Expected to find a parenthesized extension");

    std::string extension = filter_name.substr(lparen, rdelim - lparen);
    if(extension == "*")
        return;

    if(extension.length() > 2 && extension.substr(0, 2) == "*.")
        extension = extension.substr(2, extension.length() - 2);

    Platform::Path path = Platform::Path::From(chooser->get_filename());
    chooser->set_current_name(path.WithExtension(extension).FileName());
}

bool GetSaveFile(Platform::Path *filename, const std::string &defExtension,
                 const FileFilter filters[]) {
    Gtk::FileChooserDialog chooser(*(Gtk::Window *)SS.GW.window->NativePtr(),
                                   Title(C_("title", "Save File")),
                                   Gtk::FILE_CHOOSER_ACTION_SAVE);
    chooser.set_do_overwrite_confirmation(true);
    chooser.add_button(C_("button", "_Cancel"), Gtk::RESPONSE_CANCEL);
    chooser.add_button(C_("button", "_Save"), Gtk::RESPONSE_OK);

    std::string activeExtension = ConvertFilters(defExtension, filters, &chooser);

    if(filename->IsEmpty()) {
        chooser.set_current_folder(Platform::GetSettings()->ThawString("FileChooserPath"));
        chooser.set_current_name(std::string(_("untitled")) + "." + activeExtension);
    } else {
        chooser.set_current_folder(filename->Parent().raw);
        chooser.set_current_name(filename->WithExtension(activeExtension).FileName());
    }

    /* Gtk's dialog doesn't change the extension when you change the filter,
       and makes it extremely hard to do so. Gtk is garbage. */
    chooser.property_filter().signal_changed().
       connect(sigc::bind(sigc::ptr_fun(&ChooserFilterChanged), &chooser));

    if(chooser.run() == Gtk::RESPONSE_OK) {
        Platform::GetSettings()->FreezeString("FileChooserPath", chooser.get_current_folder());
        *filename = Platform::Path::From(chooser.get_filename());
        return true;
    } else {
        return false;
    }
}

DialogChoice SaveFileYesNoCancel(void) {
    Glib::ustring message =
        _("The file has changed since it was last saved.\n\n"
          "Do you want to save the changes?");
    Gtk::MessageDialog dialog(*(Gtk::Window *)SS.GW.window->NativePtr(),
                              message, /*use_markup*/ true, Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_NONE, /*is_modal*/ true);
    dialog.set_title(Title(C_("title", "Modified File")));
    dialog.add_button(C_("button", "_Save"), Gtk::RESPONSE_YES);
    dialog.add_button(C_("button", "Do_n't Save"), Gtk::RESPONSE_NO);
    dialog.add_button(C_("button", "_Cancel"), Gtk::RESPONSE_CANCEL);

    switch(dialog.run()) {
        case Gtk::RESPONSE_YES:
        return DIALOG_YES;

        case Gtk::RESPONSE_NO:
        return DIALOG_NO;

        case Gtk::RESPONSE_CANCEL:
        default:
        return DIALOG_CANCEL;
    }
}

DialogChoice LoadAutosaveYesNo(void) {
    Glib::ustring message =
        _("An autosave file is available for this project.\n\n"
          "Do you want to load the autosave file instead?");
    Gtk::MessageDialog dialog(*(Gtk::Window *)SS.GW.window->NativePtr(),
                              message, /*use_markup*/ true, Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_NONE, /*is_modal*/ true);
    dialog.set_title(Title(C_("title", "Autosave Available")));
    dialog.add_button(C_("button", "_Load autosave"), Gtk::RESPONSE_YES);
    dialog.add_button(C_("button", "Do_n't Load"), Gtk::RESPONSE_NO);

    switch(dialog.run()) {
        case Gtk::RESPONSE_YES:
        return DIALOG_YES;

        case Gtk::RESPONSE_NO:
        default:
        return DIALOG_NO;
    }
}

DialogChoice LocateImportedFileYesNoCancel(const Platform::Path &filename,
                                           bool canCancel) {
    Glib::ustring message =
        "The linked file " + filename.raw + " is not present.\n\n"
        "Do you want to locate it manually?\n\n"
        "If you select \"No\", any geometry that depends on "
        "the missing file will be removed.";
    Gtk::MessageDialog dialog(*(Gtk::Window *)SS.GW.window->NativePtr(),
                              message, /*use_markup*/ true, Gtk::MESSAGE_QUESTION,
                              Gtk::BUTTONS_NONE, /*is_modal*/ true);
    dialog.set_title(Title(C_("title", "Missing File")));
    dialog.add_button(C_("button", "_Yes"), Gtk::RESPONSE_YES);
    dialog.add_button(C_("button", "_No"), Gtk::RESPONSE_NO);
    if(canCancel)
        dialog.add_button(C_("button", "_Cancel"), Gtk::RESPONSE_CANCEL);

    switch(dialog.run()) {
        case Gtk::RESPONSE_YES:
        return DIALOG_YES;

        case Gtk::RESPONSE_NO:
        return DIALOG_NO;

        case Gtk::RESPONSE_CANCEL:
        default:
        return DIALOG_CANCEL;
    }
}

/* Miscellanea */

void DoMessageBox(const char *message, int rows, int cols, bool error) {
    Gtk::MessageDialog dialog(*(Gtk::Window *)SS.GW.window->NativePtr(),
                              message, /*use_markup*/ true,
                              error ? Gtk::MESSAGE_ERROR : Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK,
                              /*is_modal*/ true);
    dialog.set_title(error ?
        Title(C_("title", "Error")) : Title(C_("title", "Message")));
    dialog.run();
}

void OpenWebsite(const char *url) {
    gtk_show_uri(Gdk::Screen::get_default()->gobj(), url, GDK_CURRENT_TIME, NULL);
}

/* fontconfig is already initialized by GTK */
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


/* Space Navigator support */

#ifdef HAVE_SPACEWARE
static GdkFilterReturn GdkSpnavFilter(GdkXEvent *gxevent, GdkEvent *, gpointer) {
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
            if(!sev.button.press && sev.button.bnum == 0) {
                SS.GW.SpaceNavigatorButtonUp();
            }
            break;
    }

    return GDK_FILTER_REMOVE;
}
#endif

};

int main(int argc, char** argv) {
    /* It would in principle be possible to judiciously use
       Glib::filename_{from,to}_utf8, but it's not really worth
       the effort.
       The setlocale() call is necessary for Glib::get_charset()
       to detect the system character set; otherwise it thinks
       it is always ANSI_X3.4-1968.
       We set it back to C after all.  */
    setlocale(LC_ALL, "");
    if(!Glib::get_charset()) {
        dbp("Sorry, only UTF-8 locales are supported.");
        return 1;
    }
    setlocale(LC_ALL, "C");

    /* If we don't do this, gtk_init will set the C standard library
       locale, and printf will format floats using ",". We will then
       fail to parse these. Also, many text window lines will become
       ambiguous. */
    gtk_disable_setlocale();

    Gtk::Main main(argc, argv);

    // Add our application-specific styles, to override GTK defaults.
    Glib::RefPtr<Gtk::CssProvider> style_provider = Gtk::CssProvider::create();
    style_provider->load_from_data(R"(
    entry {
        background: white;
        color: black;
    }
    )");
    Gtk::StyleContext::add_provider_for_screen(Gdk::Screen::get_default(),
                                               style_provider,
                                               600 /*Gtk::STYLE_PROVIDER_PRIORITY_APPLICATION*/);

#ifdef HAVE_SPACEWARE
    gdk_window_add_filter(NULL, GdkSpnavFilter, NULL);
#endif

    const char* const* langNames = g_get_language_names();
    while(*langNames) {
        if(SetLocale(*langNames++)) break;
    }
    if(!*langNames) {
        SetLocale("en_US");
    }

    SS.Init();

#if defined(HAVE_SPACEWARE) && defined(GDK_WINDOWING_X11)
    if(GDK_IS_X11_DISPLAY(Gdk::Display::get_default()->gobj())) {
        // We don't care if it can't be opened; just continue without.
        spnav_x11_open(gdk_x11_get_default_xdisplay(),
                       gdk_x11_window_get_xid(((Gtk::Window *)SS.GW.window->NativePtr())
                                              ->get_window()->gobj()));
    }
#endif

    if(argc >= 2) {
        if(argc > 2) {
            dbp("Only the first file passed on command line will be opened.");
        }

        /* Make sure the argument is valid UTF-8. */
        Glib::ustring arg(argv[1]);
        SS.Load(Platform::Path::From(arg).Expand(/*fromCurrentDirectory=*/true));
    }

    main.run();

    SK.Clear();
    SS.Clear();

    return 0;
}
