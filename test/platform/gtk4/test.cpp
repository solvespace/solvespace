//
#include "harness.h"
#include "solvespace.h"
#include <gtkmm.h>

#ifdef USE_GTK4

class GtkTestFixture {
public:
    GtkTestFixture() {
        app = Gtk::Application::create("com.solvespace.test");
        
        window = Gtk::make_managed<Gtk::Window>();
        window->set_title("SolveSpace GTK4 Test");
        window->set_default_size(400, 300);
        
        grid = Gtk::make_managed<Gtk::Grid>();
        grid->set_row_homogeneous(false);
        grid->set_column_homogeneous(false);
        window->set_child(*grid);
        
        css_provider = Gtk::CssProvider::create();
        css_provider->load_from_data(
            "window.test-window { background-color: #f0f0f0; }"
            ".test-button { background-color: #e0e0e0; }"
            ".test-label { color: #000000; }"
        );
        
        Gtk::StyleContext::add_provider_for_display(
            window->get_display(),
            css_provider,
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        
        window->add_css_class("test-window");
    }
    
    ~GtkTestFixture() {
        window = nullptr;
    }
    
    Glib::RefPtr<Gtk::Application> app;
    Gtk::Window* window;
    Gtk::Grid* grid;
    Glib::RefPtr<Gtk::CssProvider> css_provider;
    bool event_triggered = false;
};

TEST_CASE(event_controllers) {
    GtkTestFixture fixture;
    
    auto button = Gtk::make_managed<Gtk::Button>("Test Button");
    button->add_css_class("test-button");
    fixture.grid->attach(*button, 0, 0, 1, 1);
    
    auto click_controller = Gtk::GestureClick::create();
    click_controller->signal_released().connect(
        [&fixture](int n_press, double x, double y) {
            fixture.event_triggered = true;
        }
    );
    button->add_controller(click_controller);
    
    fixture.event_triggered = false;
    click_controller->signal_released().emit(1, 10.0, 10.0);
    
    CHECK(fixture.event_triggered == true);
}

TEST_CASE(layout_managers) {
    GtkTestFixture fixture;
    
    auto button1 = Gtk::make_managed<Gtk::Button>("Button 1");
    auto button2 = Gtk::make_managed<Gtk::Button>("Button 2");
    auto button3 = Gtk::make_managed<Gtk::Button>("Button 3");
    
    fixture.grid->attach(*button1, 0, 0, 1, 1);
    fixture.grid->attach(*button2, 1, 0, 1, 1);
    fixture.grid->attach(*button3, 0, 1, 2, 1);
    
    CHECK(fixture.grid->get_child_at(0, 0) == button1);
    CHECK(fixture.grid->get_child_at(1, 0) == button2);
    CHECK(fixture.grid->get_child_at(0, 1) == button3);
}

TEST_CASE(css_styling) {
    GtkTestFixture fixture;
    
    auto button = Gtk::make_managed<Gtk::Button>("Styled Button");
    button->add_css_class("test-button");
    fixture.grid->attach(*button, 0, 0, 1, 1);
    
    bool has_class = button->has_css_class("test-button");
    
    CHECK(has_class == true);
}

TEST_CASE(property_bindings) {
    GtkTestFixture fixture;
    
    auto toggle = Gtk::make_managed<Gtk::ToggleButton>("Toggle");
    auto label = Gtk::make_managed<Gtk::Label>("Hidden");
    
    fixture.grid->attach(*toggle, 0, 0, 1, 1);
    fixture.grid->attach(*label, 0, 1, 1, 1);
    
    label->set_visible(false);
    
    toggle->property_active().signal_changed().connect([toggle, label]() {
        label->set_visible(toggle->get_active());
    });
    
    CHECK(label->get_visible() == false);
    
    toggle->set_active(true);
    
    CHECK(label->get_visible() == true);
}

TEST_CASE(accessibility) {
    GtkTestFixture fixture;
    
    auto button = Gtk::make_managed<Gtk::Button>("Accessible Button");
    fixture.grid->attach(*button, 0, 0, 1, 1);
    
    Glib::Value<Glib::ustring> name_value;
    name_value.init(Glib::Value<Glib::ustring>::value_type());
    name_value.set("Test Button");
    button->update_property(Gtk::Accessible::Property::LABEL, name_value);
    
    CHECK(button->get_label() == "Accessible Button");
}

#endif // USE_GTK4
