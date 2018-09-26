set(solvespace_util_SOURCES
        ss_util.cpp)
set(solvespace_util_HEADERS
    ss_util.h
    )
add_library(util SHARED
    ${solvespace_util_SOURCES}
    ${solvespace_util_HEADERS}
    )
target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})