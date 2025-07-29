# Keyboard Group Navigation Test

## New Feature: PageUp/PageDown Group Navigation

This implements Issue #1498 - keyboard shortcuts for navigating through the group stack.

### How to Test:

1. **Create a test file with multiple groups:**
   - Start SolveSpace
   - Create a new sketch (Sketch → In 3D)
   - Add some geometry (lines, circles, etc.)
   - Create another group (e.g., New Group → Extrude)
   - Create a third group (e.g., New Group → Sketch in New Workplane)

2. **Test the keyboard shortcuts:**
   - Press **PageUp** to navigate to the previous group
   - Press **PageDown** to navigate to the next group
   - The active group should change in both the graphics window and text window
   - The menu items "Previous Group" and "Next Group" should appear in the Edit menu

### Expected Behavior:

- **PageUp**: Moves to the previous group in the group stack
- **PageDown**: Moves to the next group in the group stack
- Navigation wraps at boundaries (stops at first/last group)
- References group is automatically skipped (cannot be activated)
- UI updates both graphics and text windows consistently

### Implementation Details:

- Added `GROUP_PREVIOUS` and `GROUP_NEXT` commands to `ui.h`
- Added menu entries with PageUp/PageDown shortcuts in `graphicswin.cpp`
- Added command handlers in `MenuEdit()` function
- Extended GTK keyboard handling to recognize PageUp/PageDown keys
- Updated accelerator descriptions to show "PageUp"/"PageDown" instead of "F33"/"F34"

### Files Modified:

- `src/ui.h` - Added new command enums
- `src/graphicswin.cpp` - Added menu entries and command handlers
- `src/platform/guigtk.cpp` - Added PageUp/PageDown key recognition and accelerator handling
- `src/platform/gui.cpp` - Updated accelerator descriptions

### Technical Notes:

- Uses existing group navigation patterns from the text window
- Maintains backward compatibility
- No file format changes
- Follows SolveSpace's existing command/menu architecture
