//-----------------------------------------------------------------------------
// Test that SVG export includes "Z" for closed paths (issue #1368).
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "harness.h"

TEST_CASE(closed_path_has_z) {
    // Create a simple closed rectangle sketch programmatically.
    // We'll use a fixture that has a closed contour.
    CHECK_LOAD("normal.slvs");

    // Set up export parameters
    SS.exportMode = true;
    SS.GW.projRight = Vector::From(1, 0, 0);
    SS.GW.projUp    = Vector::From(0, 1, 0);

    // Export to SVG in a temp file
    Platform::Path svgPath = helper->GetAssetPath(__FILE__, "test.svg", "out");
    SS.ExportViewOrWireframeTo(svgPath, /*exportWireframe=*/false);

    // Read the SVG file
    std::string svgData;
    bool ok = ReadFile(svgPath, &svgData);
    CHECK_TRUE(ok);

    // Check that the SVG contains "Z" for closed paths
    // The path data should end with "Z" before the closing quote
    CHECK_TRUE(svgData.find(" Z") != std::string::npos ||
               svgData.find("Z'") != std::string::npos);

    // Clean up
    RemoveFile(svgPath);
}