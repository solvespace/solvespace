# GTK4 Migration Improvements

This PR enhances the GTK4 migration with the following improvements:

## Internationalization Support
- Added language selection in preferences with proper locale handling
- Marked UI strings for translation using C_() macro with context
- Implemented fallback mechanism for locale selection
- Added support for all available locales in the configuration screen

## Accessibility Enhancements
- Improved screen reader support with `update_property` for accessibility properties
- Added descriptive labels and descriptions for the 3D view canvas
- Enhanced keyboard navigation with mode announcements for screen readers
- Added comprehensive operation mode announcements for keyboard actions:
  - Delete and Escape keys
  - Drawing tools (Line, Circle, Arc, Rectangle)
  - Dimension tools

## Dark Mode Styling
- Enhanced CSS styling with proper theme detection
- Added support for both light and dark themes
- Ensured 3D canvas colors remain consistent regardless of theme
- Improved UI element styling for better theme consistency

## Event Controller Improvements
- Replaced legacy signal handlers with GTK4's event controller system
- Implemented PropertyExpression for dialog visibility and theme binding
- Enhanced focus management with EventControllerFocus
- Improved keyboard and mouse event handling with GTK4's controller-based approach

## Code Quality Improvements
- Replaced legacy property access with GTK4's idiomatic accessibility API
- Enhanced menu button accessibility with proper role and label properties
- Improved dialog accessibility with GTK4's idiomatic accessibility API
- Ensured cross-platform compatibility with no Linux-specific code
- Enhanced layout management using ConstraintLayout for responsive UI

These changes follow GTK4 best practices and maintain compatibility with future GTK5 migrations.

Link to Devin run: https://app.devin.ai/sessions/149f398947fd4b5ca08e94aa478f4786
Requested by: Erkin Alp Güney
