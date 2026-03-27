# Plan: Implement `guigtk4.cpp` — GTK 4 Backend for SolveSpace

## Overview

Port the existing `src/platform/guigtk.cpp` (GTK 3 / gtkmm-3.0) to a new
`src/platform/guigtk4.cpp` targeting GTK 4 (gtkmm-4.0). The file implements
the `Platform::*` abstract interfaces declared in `src/platform/gui.h`.

The GTK 3→4 migration involves **breaking API changes** in nearly every
subsystem used by SolveSpace. This plan is ordered by component and
covers every class/function in the existing 1 679-line file.

---

## 1. Build System (`CMakeLists.txt`)

| Task | Details |
|------|---------|
| Add a `USE_GTK4` option | `option(USE_GTK4 "Build with GTK 4 instead of GTK 3" OFF)` |
| Detect gtkmm-4.0 | `pkg_check_modules(GTKMM4 REQUIRED gtkmm-4.0>=4.10)` |
| Conditionally compile | When `USE_GTK4` is ON, compile `guigtk4.cpp` instead of `guigtk.cpp` |
| Remove `HAVE_GTK_FILECHOOSERNATIVE` | GTK 4's `Gtk::FileDialog` replaces both `FileChooserDialog` and `FileChooserNative` |

---

## 2. Application Lifecycle (`InitGui`, `RunGui`, `ExitGui`, `ClearGui`)

### GTK 3 (current)
- Uses `Gtk::Main` (deprecated even in late GTK 3).
- `Gtk::Main::run()` / `Gtk::Main::quit()`.

### GTK 4 migration
| Item | Replacement |
|------|-------------|
| `Gtk::Main` | `Gtk::Application` (`Gio::Application`) |
| `Gtk::Main::run()` | `Gtk::Application::run()` or manual `GMainContext` iteration |
| `Gtk::Main::quit()` | `g_application_quit()` or close all windows |
| `Gdk::Screen::get_default()` | Removed — use `Gdk::Display::get_default()` |
| `Gtk::StyleContext::add_provider_for_screen` | `Gtk::StyleContext::add_provider_for_display` |

### Implementation notes
- Create a `Gtk::Application` with an appropriate app-id
  (e.g. `com.solvespace.SolveSpace`).
- The `activate` signal replaces the old argc/argv flow.
- CSS provider registration moves to `add_provider_for_display()`.

---

## 3. Settings (`SettingsImplGtk`)

**No changes required.** This class uses `json-c` directly and does not
depend on any GTK API. It can be reused as-is (or shared via a common
base file).

---

## 4. Timers (`TimerImplGtk`)

### GTK 3
- `Glib::signal_timeout().connect(handler, ms)` — unchanged in GLib.

### GTK 4 migration
- GLib timeout API is **unchanged**; `sigc::connection` still works with
  gtkmm-4.0. No code changes needed beyond header updates.

---

## 5. Menus (`MenuItemImplGtk`, `MenuImplGtk`, `MenuBarImplGtk`)

### GTK 3
- `Gtk::Menu`, `Gtk::MenuBar`, `Gtk::MenuItem`, `Gtk::CheckMenuItem`,
  `Gtk::SeparatorMenuItem` — **all removed in GTK 4**.

### GTK 4 migration
GTK 4 replaces widget-based menus with the **GMenu model + GAction** pattern.

| Old widget | Replacement |
|------------|-------------|
| `Gtk::Menu` | `Gio::Menu` + `Gtk::PopoverMenu` (for popups) |
| `Gtk::MenuBar` | `Gio::Menu` + `Gtk::PopoverMenuBar` |
| `Gtk::MenuItem` / `Gtk::CheckMenuItem` | `Gio::MenuItem` + `Gio::SimpleAction` |
| `Gtk::SeparatorMenuItem` | `Gio::Menu::append_section()` (section boundary = separator) |
| `set_accel_key()` | `Gtk::Application::set_accels_for_action()` |

### Design approach

```
MenuItemImplGtk4
  - Owns a Gio::SimpleAction (stateless or stateful for check/radio).
  - Stores the action name.
  - SetAccelerator() → app->set_accels_for_action(action_name, {accel}).
  - SetIndicator(CHECK/RADIO) → make the action stateful (bool).
  - SetActive(bool) → action->change_state(bool).
  - SetEnabled(bool) → action->set_enabled(bool).

MenuImplGtk4
  - Owns a Gio::Menu model.
  - AddItem()   → create SimpleAction, add to action group, append to menu.
  - AddSubMenu() → append_submenu().
  - AddSeparator() → start new section.
  - PopUp()     → create a Gtk::PopoverMenu from the model, popup().
  - Clear()     → remove_all() from the menu model, remove actions.

MenuBarImplGtk4
  - Owns a Gio::Menu model.
  - Rendered via Gtk::PopoverMenuBar in the window.
  - AddSubMenu() → append_submenu().
```

### Unique action naming
Each menu item needs a unique action name. Use a global counter
(e.g. `"ss.mi_<N>"`) prefixed with an action-map namespace.

---

## 6. OpenGL Widget (`GtkGLWidget` → `Gtk4GLWidget`)

### GTK 3
- Inherits `Gtk::GLArea`.
- Uses `set_events()` with event masks.
- Overrides `on_motion_notify_event`, `on_button_press_event`, etc.
- Uses `GdkEvent*` struct fields directly (via accessors like
  `gdk_event_get_coords`).

### GTK 4 migration

| GTK 3 | GTK 4 |
|-------|-------|
| `Gtk::GLArea` | `Gtk::GLArea` (still exists) |
| `set_events(mask)` | Removed — all widgets receive all events |
| `on_create_context()` override | Still available but signature may differ |
| `on_render()` override | Still available |
| Event signal overrides | **Replaced by event controllers** |

#### Event controllers to add

| Old signal | Controller |
|------------|------------|
| `on_motion_notify_event` | `Gtk::EventControllerMotion` |
| `on_button_press_event` / `on_button_release_event` | `Gtk::GestureClick` |
| `on_scroll_event` | `Gtk::EventControllerScroll` |
| `on_leave_notify_event` | `Gtk::EventControllerMotion::signal_leave()` |
| `on_key_press_event` / `on_key_release_event` | `Gtk::EventControllerKey` |

#### Double-click handling
- `GDK_2BUTTON_PRESS` is gone.
- `Gtk::GestureClick` already counts clicks: use `get_current_button()`
  and the `n_press` parameter of `signal_pressed()` to detect double-click.

#### Modifier state
- `GdkModifierType` masks like `GDK_BUTTON1_MASK` still exist but are
  queried from the event controller's current event
  (`gtk_event_controller_get_current_event_state()`).

---

## 7. Editor Overlay (`GtkEditorOverlay`)

### GTK 3
- Inherits `Gtk::Fixed`, contains a `Gtk::Entry` and the GL widget.
- Uses `add()`, `move()`, `add_modal_grab()`, `remove_modal_grab()`.
- Overrides `on_size_allocate()`.

### GTK 4 migration

| GTK 3 | GTK 4 |
|-------|-------|
| `Gtk::Fixed` | `Gtk::Fixed` (still exists) |
| `add(widget)` / `move(widget, x, y)` | `put(widget, x, y)` / `move(widget, x, y)` |
| `add_modal_grab()` / `remove_modal_grab()` | Grabs are removed; use `Gtk::Popover` or manual focus management |
| `on_size_allocate(Allocation&)` | `Gtk::Widget::on_size_allocate(int width, int height, int baseline)` — new signature |
| `_entry.override_font()` | Use CSS classes / `Gtk::CssProvider` per-widget |
| `_entry.get_style_context()->get_margin()` | CSS node inspection or `Gtk::Widget::get_margin_*()` |
| `event()` forwarding | Use event controller propagation phases |

### Design notes
- The overlay entry for in-place editing can be placed using `Gtk::Fixed`.
- Font override should use a per-widget CSS provider
  (`gtk_widget_add_css_class` / dynamic CSS).
- Modal grab replacement: set the entry as the focus widget and handle
  Escape via `Gtk::EventControllerKey`.

---

## 8. Window (`GtkWindow` → `Gtk4Window`, `WindowImplGtk` → `WindowImplGtk4`)

### GTK 3
- `GtkWindow` inherits `Gtk::Window`.
- Uses `Gtk::VBox`, `Gtk::HBox` for layout.
- Contains `Gtk::MenuBar*`, `GtkEditorOverlay`, `Gtk::VScrollbar`.
- Uses `on_delete_event`, `on_window_state_event`, `on_enter_notify_event`,
  `on_leave_notify_event`.

### GTK 4 migration

| GTK 3 | GTK 4 |
|-------|-------|
| `Gtk::VBox` / `Gtk::HBox` | `Gtk::Box(Gtk::Orientation::VERTICAL)` / `HORIZONTAL` |
| `pack_start` / `pack_end` | `append()` / `prepend()` — use `hexpand`/`vexpand` properties |
| `Gtk::VScrollbar` | `Gtk::Scrollbar(Gtk::Orientation::VERTICAL)` |
| `on_delete_event` | `signal_close_request()` |
| `on_window_state_event` | Property notifications on `"fullscreened"`, `"maximized"` |
| `on_enter/leave_notify_event` | `Gtk::EventControllerMotion` on the window |
| `set_type_hint(UTILITY)` | No direct equivalent; consider `set_transient_for()` only |
| `set_skip_taskbar_hint` | Removed (Wayland doesn't support this) |
| `set_skip_pager_hint` | Removed |
| `get_position()` / `move()` | Removed (no server-side window positioning on Wayland) |
| `resize()` | `set_default_size()` |
| `Gdk::Pixbuf` for icon | `Gdk::Texture` / `Gtk::IconTheme` |
| `get_screen()->get_resolution()` | `Gdk::Display` / `Gdk::Monitor` API |
| `get_scale_factor()` | Still available |
| `show_all()` | Widgets are visible by default in GTK 4 |
| `Gtk::Tooltip` API | Mostly unchanged; `set_has_tooltip` still works |

### Window position save/restore
- GTK 4 **cannot** get/set window position on Wayland.
- `FreezePosition` / `ThawPosition` should save only width, height,
  and maximized state. Position saving can be retained for X11 only
  (check `GDK_IS_X11_DISPLAY`) or dropped entirely.

---

## 9. Message Dialogs (`MessageDialogImplGtk`)

### GTK 3
- Uses `Gtk::MessageDialog` with `run()` (blocking).

### GTK 4 migration
- `Gtk::MessageDialog` still exists but `run()` is **removed**.
- Use `Gtk::AlertDialog` (gtkmm ≥ 4.10) for simple message dialogs.
- Alternatively, use `Gtk::MessageDialog` + `signal_response()` in
  async mode.
- `RunModal()` must be restructured to use an inner `GMainContext`
  iteration loop or switch to fully async flow. Prefer async +
  callback wrapping with a local `Glib::MainLoop`.

### `set_image()` removal
- `Gtk::MessageDialog::set_image()` is gone. The dialog chooses its own
  icon from the message type. Remove explicit icon setting.

---

## 10. File Dialogs (`FileDialogImplGtk`)

### GTK 3
- Uses `Gtk::FileChooserDialog` and `Gtk::FileChooserNative`.
- `run()` blocks.

### GTK 4 migration
- `Gtk::FileDialog` (gtkmm ≥ 4.10) is the replacement.
- It is **fully async**: `open()` / `save()` return via
  `Gio::AsyncResult` callback.
- File filters use `Gtk::FileFilter` (still available) added to
  `Gtk::FileDialog::set_filters()` (a `Gio::ListModel`).
- `set_do_overwrite_confirmation()` is gone (always confirmed).

### Blocking wrapper
The `Platform::FileDialog::RunModal()` interface is synchronous. Wrap
the async GTK 4 API with a `Glib::MainLoop` to block:

```cpp
bool RunModal() override {
    bool accepted = false;
    auto loop = Glib::MainLoop::create();
    gtkFileDialog->open(gtkParent,
        [&](Glib::RefPtr<Gio::AsyncResult>& result) {
            try {
                auto file = gtkFileDialog->open_finish(result);
                selectedPath = file->get_path();
                accepted = true;
            } catch (...) {}
            loop->quit();
        });
    loop->run();
    return accepted;
}
```

---

## 11. Miscellaneous APIs

| Function | GTK 3 | GTK 4 |
|----------|-------|-------|
| `OpenInBrowser()` | `gtk_show_uri_on_window()` | `gtk_uri_launcher_launch()` or `Gtk::UriLauncher` (4.10+) |
| `GetFontFiles()` | Uses fontconfig directly — **no change** |
| `AcceleratorDescription()` | Uses `Gtk::AccelGroup::name()` | Use `gtk_accelerator_get_label()` (still available) |

---

## 12. 3DConnexion / SpaceNav Support

### GTK 3
- X11: `gdk_window_add_filter()` + `spnav_x11_event()`.
- Wayland: `g_io_add_watch()` on `spnav_fd()`.
- Uses `GdkDeviceManager` (removed in GTK 4).

### GTK 4
- `gdk_window_add_filter()` is **removed**.
- On X11, use `g_io_add_watch()` on the spnav fd (same as Wayland path).
  Unify both backends to poll via fd rather than X11 event filter.
- Replace `GdkDeviceManager` with `Gdk::Seat` (already conditional on
  GTK ≥ 3.20 in the existing code).

---

## 13. Header Changes

### Removed headers (GTK 3 only)
```
gtkmm/checkmenuitem.h
gtkmm/filechooserdialog.h
gtkmm/filechoosernative.h
gtkmm/main.h
gtkmm/menu.h
gtkmm/menubar.h
gtkmm/messagedialog.h  (if using AlertDialog)
gtkmm/separatormenuitem.h
gdkmm/devicemanager.h
```

### New headers (GTK 4)
```
gtkmm/application.h
gtkmm/popovermenu.h
gtkmm/popovermenubar.h
gtkmm/eventcontrollerkey.h
gtkmm/eventcontrollermotion.h
gtkmm/eventcontrollerscroll.h
gtkmm/gestureclick.h
gtkmm/alertdialog.h      (4.10+)
gtkmm/filedialog.h       (4.10+)
giomm/menu.h
giomm/simpleaction.h
giomm/simpleactiongroup.h
```

---

## 14. Implementation Order

Recommended phased approach:

### Phase 1 — Skeleton & lifecycle
1. Create `guigtk4.cpp` with includes and namespace.
2. Port `InitGui` / `RunGui` / `ExitGui` / `ClearGui` using
   `Gtk::Application`.
3. Copy `SettingsImplGtk` and `TimerImplGtk` unchanged.
4. Build system: add `USE_GTK4` option, detect gtkmm-4.0.
5. **Verify**: application starts and exits cleanly.

### Phase 2 — Window & GL rendering
1. Port `GtkGLWidget` / `Gtk4GLWidget` with event controllers.
2. Port `GtkEditorOverlay` / `Gtk4EditorOverlay`.
3. Port `GtkWindow` / `Gtk4Window` and `WindowImplGtk4`.
4. **Verify**: window opens, GL renders, mouse/keyboard input works.

### Phase 3 — Menus
1. Implement `MenuItemImplGtk4` with `Gio::SimpleAction`.
2. Implement `MenuImplGtk4` with `Gio::Menu` + `Gtk::PopoverMenu`.
3. Implement `MenuBarImplGtk4` with `Gtk::PopoverMenuBar`.
4. **Verify**: menus display, accelerators work, check marks toggle.

### Phase 4 — Dialogs
1. Port `MessageDialogImplGtk4`.
2. Port `FileDialogImplGtk4`.
3. **Verify**: open/save dialogs work, message boxes appear.

### Phase 5 — Ancillary
1. Port `OpenInBrowser()`.
2. Port 3DConnexion/SpaceNav support.
3. Port cursor, tooltip, scrollbar APIs.
4. **Verify**: full application workflow end-to-end.

---

## 15. Risks & Open Questions

| Risk | Mitigation |
|------|------------|
| Blocking `RunModal()` via nested main loop may cause re-entrancy | Test carefully; consider refactoring `Platform::FileDialog` to async in a follow-up |
| Window positioning removed on Wayland | Accept loss of position restore; keep size + maximized state |
| Menu model approach is fundamentally different | May require significant refactoring of action naming and lifecycle |
| gtkmm-4.0 minimum version | Target gtkmm ≥ 4.10 for `Gtk::FileDialog` and `Gtk::AlertDialog` |
| SpaceNav X11 event filter gone | Unify to fd-polling approach (already used for Wayland) |
| `Gtk::Fixed` size allocation API change | Test overlay positioning thoroughly |
