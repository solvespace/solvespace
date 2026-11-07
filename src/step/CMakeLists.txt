include(StepCode)
slvs_add_stepcode()

add_library(solvespace-step-reader STATIC stepreader.cpp)
target_include_directories(solvespace-step-reader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(solvespace-step-reader PRIVATE
    ${STEP_IMPORT_SCHEMA_LIBRARIES} slvs-stepcode-runtime)
foreach(schema IN LISTS STEP_IMPORT_ENABLED_SCHEMAS)
    target_compile_definitions(solvespace-step-reader PRIVATE "SLVS_STEP_${schema}")
endforeach()

add_executable(solvespace-step-probe probe.cpp)
target_link_libraries(solvespace-step-probe PRIVATE solvespace-step-reader)

# Separate layer and target: never add the reader or vendor dependencies to
# solvespace-core/slvs_deps/the solver. Reader-only builds still work without
# application geometry targets (ENABLE_GUI=OFF and ENABLE_CLI=OFF).
if(TARGET solvespace-core)
    add_library(solvespace-step-geometry STATIC stepgeometry.cpp)
    target_include_directories(solvespace-step-geometry PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(solvespace-step-geometry
        PUBLIC solvespace-core
        PRIVATE solvespace-step-reader)
    if(TARGET solvespace-headless)
        add_executable(solvespace-step-geometry-probe geometryprobe.cpp
            $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)
        target_link_libraries(solvespace-step-geometry-probe PRIVATE
            solvespace-step-geometry solvespace-step-reader solvespace-headless)
        add_dependencies(solvespace-step-geometry-probe resources)
    endif()
endif()
