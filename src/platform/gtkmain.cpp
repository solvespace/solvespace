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

namespace SolveSpace {

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

    const char* const* langNames = g_get_language_names();
    while(*langNames) {
        if(SetLocale(*langNames++)) break;
    }
    if(!*langNames) {
        SetLocale("en_US");
    }

    SS.Init();

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
