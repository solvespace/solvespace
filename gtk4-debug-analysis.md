# GTK4 Migration Debug Analysis

## Critical Issues

1. **Accessibility API Incompatibility**
   - Current code uses `get_accessible()` method which doesn't exist in GTK4 4.14.2
   - Correct API: Use `Gtk::Accessible` interface directly with `update_property()` method
   - Correct enum: `Gtk::Accessible::Role` instead of `Gtk::AccessibleRole`

2. **Property Expression API Issues**
   - Current implementation uses incorrect syntax for property expressions
   - Correct API: Include `<gtkmm/expression.h>` and use proper template syntax

3. **Constraint Layout API Issues**
   - Current implementation uses incorrect methods for constraint layout
   - Need to update constraint creation and attribute usage

4. **Event Controller API Issues**
   - Some event controllers like `EventControllerFocus` and `EventControllerLegacy` don't exist
   - Need to use correct event controllers available in GTK4 4.14.2

## Recommendations

1. Update accessibility implementation to use `Gtk::Accessible` interface directly:
   ```cpp
   // Instead of:
   widget->get_accessible()->set_property("accessible-role", "button");
   
   // Use:
   widget->update_property(Gtk::Accessible::Property::ROLE, Gtk::Accessible::Role::BUTTON);
   ```

2. Update property expression usage:
   ```cpp
   // Include the correct header
   #include <gtkmm/expression.h>
   
   // Use correct syntax for property expressions
   auto expr = Gtk::PropertyExpression<bool>::create(widget->property_visible());
   ```

3. Update constraint layout implementation:
   ```cpp
   // Use correct constraint layout API
   auto layout = Gtk::ConstraintLayout::create();
   auto constraint = Gtk::Constraint::create(
       widget1, Gtk::Constraint::Attribute::LEFT,
       Gtk::Constraint::Relation::EQ,
       widget2, Gtk::Constraint::Attribute::LEFT);
   layout->add_constraint(constraint);
   ```

4. Replace unsupported event controllers:
   ```cpp
   // Instead of EventControllerFocus
   auto focus_controller = Gtk::EventControllerKey::create();
   focus_controller->signal_focus_in().connect([this]() { /* ... */ });
   
   // Instead of EventControllerLegacy
   auto click_controller = Gtk::GestureClick::create();
   click_controller->signal_pressed().connect([this]() { /* ... */ });
   ```

## Next Steps

1. Update `GtkMenuItem` class to use correct accessibility API
2. Fix `GtkGLWidget` implementation to use proper event controllers
3. Update `GtkEditorOverlay` to use correct constraint layout API
4. Fix property expression usage throughout the codebase
5. Test changes in Docker container with GTK4 4.14.2
