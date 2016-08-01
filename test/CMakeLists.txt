# test suite

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CAIRO_INCLUDE_DIRS})

set(testsuite_SOURCES
    harness.cpp
    request/line_segment/test.cpp)

add_executable(solvespace_testsuite
    ${testsuite_SOURCES})

target_link_libraries(solvespace_testsuite
    solvespace_headless
    ${COVERAGE_LIBRARY})

add_custom_target(solvespace-test ALL
    COMMAND $<TARGET_FILE:solvespace_testsuite>
    COMMENT "Testing SolveSpace"
    VERBATIM)

# coverage reports

if(ENABLE_COVERAGE)
    set(LCOV_FLAGS   -q --rc lcov_branch_coverage=1 --rc lcov_excl_line=ssassert)
    set(LCOV_FLAGS   ${LCOV_FLAGS} --gcov-tool ${GCOV})
    set(LCOV_COLLECT -c -b ${CMAKE_SOURCE_DIR}/src -d ${CMAKE_BINARY_DIR}/src --no-external)

    add_custom_command(
        OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/coverage_base.info
        COMMAND ${LCOV} ${LCOV_FLAGS} ${LCOV_COLLECT}
            -o ${CMAKE_BINARY_DIR}/coverage_base.info -i
        DEPENDS solvespace_testsuite
        COMMENT "Importing baseline coverage data"
        VERBATIM)

    add_custom_target(solvespace-coverage ALL
        COMMAND ${LCOV} ${LCOV_FLAGS} ${LCOV_COLLECT}
            -o ${CMAKE_BINARY_DIR}/coverage_test.info
        COMMAND ${LCOV} ${LCOV_FLAGS}
            -o ${CMAKE_BINARY_DIR}/coverage_full.info
            -a ${CMAKE_BINARY_DIR}/coverage_base.info
            -a ${CMAKE_BINARY_DIR}/coverage_test.info
        COMMAND ${LCOV} ${LCOV_FLAGS} --summary
            ${CMAKE_BINARY_DIR}/coverage_full.info
        COMMAND ${GENHTML} -q --branch-coverage --demangle-cpp --legend
            ${CMAKE_BINARY_DIR}/coverage_full.info
            -o ${CMAKE_BINARY_DIR}/coverage/
            -t "SolveSpace testbench"
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/coverage_base.info
        DEPENDS solvespace-test
        COMMENT "Generating coverage report"
        VERBATIM)
endif()
