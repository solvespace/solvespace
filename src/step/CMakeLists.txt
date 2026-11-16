include(StepCode)
slvs_add_stepcode()
find_package(Threads REQUIRED)

add_library(solvespace-step-reader STATIC stepreader.cpp)
target_include_directories(solvespace-step-reader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(solvespace-step-reader PRIVATE
    ${STEP_IMPORT_SCHEMA_LIBRARIES} slvs-stepcode-runtime Threads::Threads)
foreach(schema IN LISTS STEP_IMPORT_ENABLED_SCHEMAS)
    target_compile_definitions(solvespace-step-reader PRIVATE "SLVS_STEP_${schema}")
endforeach()

add_executable(solvespace-step-probe probe.cpp)
target_link_libraries(solvespace-step-probe PRIVATE solvespace-step-reader)

# Native objects can be consumed by a proof executable OR included in the
# application core, without a core <-> geometry static-library dependency cycle.
# STEPcode remains private to the reader, never slvs_deps or the solver.
if(TARGET solvespace-core)
    add_library(solvespace-step-geometry OBJECT stepgeometry.cpp stepnurbs.cpp stepcolor.cpp)
    target_include_directories(solvespace-step-geometry PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(solvespace-step-geometry PRIVATE
        "${PROJECT_SOURCE_DIR}/src" "${PROJECT_BINARY_DIR}/src")
    target_link_libraries(solvespace-step-geometry PRIVATE slvs_deps)
    if(ENABLE_STEP_IMPORT)
        target_sources(solvespace-core PRIVATE $<TARGET_OBJECTS:solvespace-step-geometry>)
        target_link_libraries(solvespace-core PRIVATE solvespace-step-reader)
        target_compile_definitions(solvespace-core PUBLIC HAVE_STEP_IMPORT)
    endif()
    if(TARGET solvespace-headless)
        add_executable(solvespace-step-geometry-probe geometryprobe.cpp
            $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)
        target_link_libraries(solvespace-step-geometry-probe PRIVATE
            solvespace-step-reader solvespace-headless)
        if(NOT ENABLE_STEP_IMPORT)
            target_link_libraries(solvespace-step-geometry-probe PRIVATE solvespace-step-geometry)
        endif()
        add_dependencies(solvespace-step-geometry-probe resources)
    endif()
endif()
