# GTK4 Migration Improvements

This PR completes the GTK4 migration for SolveSpace with the following enhancements:

## Internationalization Support
- Improved language selection in preferences with proper locale handling
- Added RTL language support for Arabic, Hebrew, Persian, and other RTL languages
- Enhanced text direction handling in CSS for RTL languages

## Accessibility Enhancements
- Implemented operation mode announcements for screen readers using GTK4's update_property API
- Added proper accessibility labels and descriptions for UI elements
- Enhanced keyboard navigation support

## CSS Styling Improvements
- Extracted CSS from raw strings into separate files for better maintainability
- Implemented file-based CSS loading with fallback to embedded strings
- Added dark mode styling with CSS variables
- Ensured 3D canvas colors remain consistent across theme changes

## Event Controller Replacements
- Replaced signal handlers with GTK4 event controllers
- Implemented PropertyExpression for reactive UI binding
- Enhanced touch screen support with gesture controllers

## Cross-Platform Compatibility
- Ensured no Linux-specific code is used
- Maintained compatibility with Windows and macOS builds
- Updated Flatpak manifest with correct GTK4 dependencies

## Testing
- Tested in Ubuntu 24.04 Docker environment
- Verified language selection and RTL support
- Tested accessibility features with screen readers
- Verified dark mode appearance with both light and dark system themes

This PR addresses the requirements in issue #1560 for the GTK4 migration.

Link to Devin run: https://app.devin.ai/sessions/80839d35747c407fa31aa0e59c2d85b5
Requested by: Erkin Alp Güney
