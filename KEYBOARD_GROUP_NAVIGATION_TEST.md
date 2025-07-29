# Keyboard Group Navigation Test

## What was implemented

Added PageUp and PageDown keyboard shortcuts to navigate between groups in SolveSpace.

## Files modified

1. `src/ui.h` - Added `GROUP_PREVIOUS` and `GROUP_NEXT` command enums
2. `src/graphicswin.cpp` - Added menu entries and command handlers
3. `src/platform/guigtk.cpp` - Added PageUp/PageDown key recognition
4. `src/platform/gui.cpp` - Added proper accelerator descriptions

## How to test

1. Open SolveSpace
2. Create multiple groups:
   - Start with a sketch (default group 0)
   - Add a workplane: Sketch → In New Workplane (group 1)
   - Add an extrude: New Group → Extrude (group 2)
   - Add another sketch in the new workplane (group 3)

3. Test the navigation:
   - Press **PageDown** to go to the next group
   - Press **PageUp** to go to the previous group
   - Check that the active group changes in both the graphics window and text window
   - Verify you can navigate through all groups

## Expected behavior

- PageUp navigates to the previous group (if not already at the first group)
- PageDown navigates to the next group (if not already at the last group)
- The active group should change and be highlighted in the text window
- The graphics window should update to show the active group's entities
- Group navigation should skip the references group (group #references) when going backwards

## Menu entries

The shortcuts are also available in the Edit menu:
- Edit → Previous Group (PageUp)
- Edit → Next Group (PageDown)

## Known limitations

- Translation strings for menu items are not yet added (shows "Missing translation")
- Only works on GTK-based builds (Linux)
