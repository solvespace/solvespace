# Deliberately separate from solvespace-testsuite: this verifies that clients
# can inspect STEP data without depending on the solver, GUI, or vendor headers.
find_package(Threads REQUIRED)
add_executable(solvespace-step-testsuite test.cpp)
target_link_libraries(solvespace-step-testsuite PRIVATE solvespace-step-reader Threads::Threads)
target_compile_definitions(solvespace-step-testsuite PRIVATE
    "STEP_TEST_SOURCE_DIR=\"${PROJECT_SOURCE_DIR}\"")
add_custom_target(test_step_import
    COMMAND $<TARGET_FILE:solvespace-step-testsuite>
    DEPENDS solvespace-step-testsuite
    COMMENT "Testing the dependency-only STEP reader"
    VERBATIM)

# Optional, explicit network/corpus test, never run by the normal build/tests.
# An existing archive can be supplied to avoid the download entirely.
set(STEP_IMPORT_NIST_ARCHIVE "" CACHE FILEPATH "Optional local NIST PMI STEP test archive")
if("AP242" IN_LIST STEP_IMPORT_SCHEMAS)
    add_custom_target(test_step_import_nist
        COMMAND "${CMAKE_COMMAND}"
            "-DTEST_EXE=$<TARGET_FILE:solvespace-step-testsuite>"
            "-DCORPUS_DIR=${CMAKE_CURRENT_BINARY_DIR}/nist"
            "-DARCHIVE=${STEP_IMPORT_NIST_ARCHIVE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/RunNist.cmake"
        DEPENDS solvespace-step-testsuite
        COMMENT "Testing real NIST AP242 files (optional corpus download)"
        VERBATIM)
endif()

# Native layer tests use the existing headless geometry/runtime support, but
# the original SolveSpace suite and dependency-only reader suite stay separate.
if(TARGET solvespace-step-geometry AND TARGET solvespace-headless)
    add_executable(solvespace-step-geometry-testsuite geometry.cpp
        $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)
    target_link_libraries(solvespace-step-geometry-testsuite PRIVATE
        solvespace-step-reader solvespace-headless)
    target_include_directories(solvespace-step-geometry-testsuite PRIVATE
        "${PROJECT_SOURCE_DIR}/src/step")
    target_compile_definitions(solvespace-step-geometry-testsuite PRIVATE
        "STEP_LINK_TEST_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\"")
    if(NOT ENABLE_STEP_IMPORT)
        target_link_libraries(solvespace-step-geometry-testsuite PRIVATE solvespace-step-geometry)
    endif()
    add_dependencies(solvespace-step-geometry-testsuite resources)
    add_custom_target(test_step_geometry
        COMMAND $<TARGET_FILE:solvespace-step-geometry-testsuite>
        DEPENDS solvespace-step-geometry-testsuite
        COMMENT "Testing exact native STEP B-rep conversion"
        VERBATIM)
    if(ENABLE_STEP_IMPORT)
        target_compile_definitions(solvespace-step-geometry-testsuite PRIVATE HAVE_STEP_IMPORT)
        add_custom_target(test_step_link
            COMMAND "${CMAKE_COMMAND}" -E env "SLVS_STEP_GEOMETRY_TEST_FILTER=link/"
                $<TARGET_FILE:solvespace-step-geometry-testsuite>
            DEPENDS solvespace-step-geometry-testsuite
            COMMENT "Testing native STEP linked groups and constraint references"
            VERBATIM)
    endif()
endif()
