set(pure_platform_SOURCES
    platform.cpp)

set(pure_platform_HEADERS
    platform.h)

add_library(pure_platform #STATIC
    ${pure_platform_SOURCES}
    ${pure_platform_HEADERS}
    )

target_link_libraries(pure_platform
    util)

target_include_directories(pure_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

