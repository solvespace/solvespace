# solvespace-core: standalone computation library without GUI dependencies.
#
# This library contains the constraint solver, entity/group generation,
# mesh/surface operations, file I/O, import/export, and utilities.
# It can be linked into any application without requiring GUI frameworks.

add_library(solvespace-core-lib STATIC
    # Pure math/geometry
    bsp.cpp
    constrainteq.cpp
    entity.cpp
    expr.cpp
    polygon.cpp
    polyline.cpp
    system.cpp
    mesh.cpp

    # Surface operations
    srf/boolean.cpp
    srf/curve.cpp
    srf/merge.cpp
    srf/ratpoly.cpp
    srf/raycast.cpp
    srf/shell.cpp
    srf/surface.cpp
    srf/surfinter.cpp
    srf/triangulate.cpp

    # Data model
    request.cpp
    group.cpp
    constraint.cpp
    style.cpp

    # Generation and solving
    generate.cpp
    groupmesh.cpp

    # File I/O
    file.cpp
    undoredo.cpp

    # Import
    importdxf.cpp
    importidf.cpp
    importmesh.cpp

    # Export
    export.cpp
    exportstep.cpp
    exportvector.cpp

    # Utilities
    util.cpp
    ttf.cpp
    resource.cpp

    # Platform (Path, file operations - no GUI dependency)
    ../platform/platform.cpp

    # Core engine implementation
    solvespace_core.cpp

    # Public C++ API
    api.cpp
)

# Set output name to avoid conflict with the old solvespace-core target
set_target_properties(solvespace-core-lib PROPERTIES
    OUTPUT_NAME solvespace-core)

# Add the SOLVESPACE_CORE_ONLY define to strip UI dependencies
target_compile_definitions(solvespace-core-lib
    PUBLIC SOLVESPACE_CORE_ONLY)

# Include paths: src/ for headers, build dir for config.h
target_include_directories(solvespace-core-lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_BINARY_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# Link against the solver interface and dependencies
target_link_libraries(solvespace-core-lib
    PRIVATE slvs-solver dxfrw
    PUBLIC slvs_deps)

# Export as a named target
add_library(SolveSpace::Core ALIAS solvespace-core-lib)
