# Deliberately separate from solvespace-testsuite: this verifies that clients
# can inspect STEP data without depending on the solver, GUI, or vendor headers.
add_executable(solvespace-step-testsuite test.cpp)
target_link_libraries(solvespace-step-testsuite PRIVATE solvespace-step-reader)
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
