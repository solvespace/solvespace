set(solvespace_util_SOURCES
        ss_util.h
        ss_util.cpp)
add_library(util ${solvespace_util_SOURCES})
target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})