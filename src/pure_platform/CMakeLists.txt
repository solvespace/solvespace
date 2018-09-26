
# platform utilities

if(WIN32)
    set(util_SOURCES
        utilwin.cpp)
else()
    set(util_SOURCES
        utilunix.cpp)
endif()

if(APPLE)
    set(util_LIBRARIES
        ${APPKIT_LIBRARY})
endif()

set(pure_platform_SOURCES
    platform.cpp)

set(pure_platform_HEADERS
    platform.h)

add_library(pure_platform SHARED
    ${pure_platform_SOURCES}
    ${pure_platform_HEADERS}
    ${util_SOURCES}
    )

target_link_libraries(pure_platform
    util
    ${util_LIBRARIES}
    )

target_include_directories(pure_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

