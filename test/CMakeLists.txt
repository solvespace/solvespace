include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

set(testsuite_SOURCES
    harness.cpp
    request/line_segment/test.cpp)

add_executable(solvespace_testsuite
    ${testsuite_SOURCES})

target_link_libraries(solvespace_testsuite
    solvespace_headless)

add_custom_target(solvespace-test ALL
    COMMAND $<TARGET_FILE:solvespace_testsuite>
    COMMENT "Testing SolveSpace"
    VERBATIM)
