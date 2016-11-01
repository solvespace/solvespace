Changelog
=========

3.0
---

New sketch features:
  * Extrude, lathe, translate and rotate groups can now use the "assembly"
    boolean operation, to increase performance.
  * Translate and rotate groups can create n-dimensional arrays using
    the "difference" and "assembly" boolean operations.
  * A new sketch in workplane group can be created based on existing workplane.
  * TTF text request has two additional points on the right side, which allow
    constraining the width of text.
  * Irrelevant points (e.g. arc center point) are not counted when estimating
    the bounding box used to compute chord tolerance.

New export/import features:
  * Three.js: allow configuring projection for exported model, and initially
    use the current viewport projection.
  * Wavefront OBJ: a material file is exported alongside the model, containing
    mesh color information.
  * DXF/DWG: 3D DXF files are imported as construction entities, in 3d.

New rendering features:
  * The "Show/hide hidden lines" button is now a tri-state button that allows
    showing all lines (on top of shaded mesh), stippling occluded lines
    or not drawing them at all.
  * The "Show/hide outlines" button is now independent from "Show/hide edges".

Other new features:
  * New command for measuring total length of selected entities,
    "Analyze → Measure Perimeter".
  * New link to match the on-screen size of the sketch with its actual size,
    "view → set to full scale".
  * When zooming to fit, constraints are also considered.
  * When selecting a point and a line, projected distance to to current
    workplane is displayed.
  * The "=" key is bound to "Zoom In", like "+" key.

Bugs fixed:
  * A point in 3d constrained to any line whose length is free no longer
    causes the line length to collapse.
  * Lines in 3d constrained parallel are solved in a more robust way.

2.3
---

Bug fixes:
  * Do not crash when applying a symmetry constraint to two points.
  * Fix TTF font metrics again (properly this time).
  * Fix the "draw back faces in red" option.
  * Fix export of wireframe as 3D DXF.
  * Various minor crashes.

2.2
---

Other new features:
  * OS X: support 3Dconnexion devices (SpaceMouse, SpaceNavigator, etc).
  * GTK: files with uppercase extensions can be opened.

Bug fixes:
  * Do not remove autosaves after successfully opening a file, preventing
    data loss in case of two abnormal terminations in a row.
  * Do not crash when changing autosave interval.
  * Unbreak the "Show degrees of freedom" command.
  * Three.js: correctly respond to controls when browser zoom is used.
  * OS X: do not completely hide main window when defocused.
  * GTK: unbreak 3Dconnexion support.
  * When pasting transformed entities, multiply constraint values by scale.
  * Fix TTF font metrics (restore the behavior from version 2.0).
  * Forcibly show the current group once we start a drawing operation.
  * DXF export: always declare layers before using them.
  * Do not truncate operations on selections to first 32 selected entities.
  * Translate and rotate groups inherit the "suppress solid model" setting.
  * DXF: files with paths containing non-ASCII or spaces can be exported
    or imported.
  * Significantly improved performance when dragging an entity.
  * Various crashes and minor glitches.

2.1
---

New sketch features:
  * Lathe groups create circle and face entities.
  * New toolbar button for creating lathe groups.
  * Chord tolerance is separated into two: display chord tolerance (specified
    in percents, relative to model bounding box), and export chord tolerance
    (specified in millimeters as absolute value).
  * Bezier spline points can be added and removed after the spline is created.
  * When an unconstrained extrusion is switched between "union" and
    "difference", its normal is flipped.
  * Groups can be added in the middle of the stack. Note that this results
    in files incompatible with version 2.0.
  * Active group can be removed.
  * Removing an imported group does not cause all subsequent groups to also
    be removed.
  * When a new group with a solid is created, the color is taken from
    a previous group with a solid, if any.
  * Entities in a newly active group do not become visible.
  * When entities are selected, "Zoom to fit" zooms to fit only these
    entities and not the entire sketch.
  * Zero-length edges are reported with a "zero-length error", not
    "points not all coplanar".

New constraint features:
  * Height of the font used for drawing constraint labels can be changed.
  * New constraint, length difference, placed with J.
    (Patch by Peter Ruevski)
  * Horizontal/vertical constraints are automatically added if a line segment
    is close enough to being horizontal/vertical. This can be disabled by
    holding Ctrl.
  * Reference dimensions and angles can be placed with Shift+D and Shift+N.
  * Copying and pasting entities duplicates any constraints that only involve
    entities in the clipboard, as well as selected comments.
  * Diameter constraints can be shown as radius.
  * The "pi" identifier can be used in expressions.
  * Constraint labels can be snapped to grid.
  * Integer angles are displayed without trailing zeroes.
  * Angle constraints have proper reference lines and arrowheads.
  * Extension lines are drawn for point-line distance constraints.

New solver features:
  * Sketches with redundant and unsolvable constraints are distinguished.
  * New group setting, "allow redundant constraints". Note that it makes
    the solver less stable.

New rendering and styling features:
  * New line style parameter: stippling, based on ISO 128.
  * Outlines of solids can be drawn in a particular style (by default, thick
    lines) controlled by the "Show outline of solid model" button.
  * Occluded edges can be drawn in a particular style (by default, stippled
    with short dashes) controlled by the "Show hidden lines" button.
  * Solids can be made transparent.

New export/import features:
  * The old "import" command (for .slvs files) is renamed to "link".
  * If a linked .slvs file is not found, first the relative path recorded
    in the .slvs file is checked and then the absolute path; this is
    an inversion of the previously used order. If it is still not found,
    a dialog appears offering to locate it.
  * DXF and DWG files can be imported, with point-coincident, horizontal and
    vertical constraints automatically inferred from geometry, and distance
    and angle constraints created when a dimension placed against geometry
    exists.
  * Triangle mesh can be exported for viewing in the browser through WebGL.
  * Export dialogs remember the last file format used, and preselect it.
  * Exported DXF files have exact circles, arcs and splines instead of
    a piecewise linear approximation (unless hidden line removal was needed).
  * Exported DXF files preserve color and line thickness.
  * In exported DXF files, constraints are represented as DXF dimensions,
    instead of piecewise linear geometry.
  * When exporting 2d views, overlapping lines are removed.

Other new features:
  * Native Linux (GTK 2 and GTK 3) and Mac OS X ports.
  * Automatically save and then restore sketches if SolveSpace crashes.
    (Patch by Marc Britten)
  * Unicode is supported everywhere (filenames, group names, TTF text,
    comments), although RTL scripts and scripts making heavy use of ligatures
    are not rendered correctly.
  * The vector font is grid-fitted when rendered on screen to make it easier
    to read regardless of its size.

2.0
---

Initial public release.
