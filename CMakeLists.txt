# cmake configuration

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
cmake_policy(VERSION 3.1.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

include(CheckIncludeFile)

# for /MT on MSVC
set(CMAKE_USER_MAKE_RULES_OVERRIDE
   "${CMAKE_SOURCE_DIR}/cmake/c_flag_overrides.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
   "${CMAKE_SOURCE_DIR}/cmake/cxx_flag_overrides.cmake")

# project

project(solvespace)
set(solvespace_VERSION_MAJOR 2)
set(solvespace_VERSION_MINOR 1)

if(NOT WIN32 AND NOT APPLE)
    set(GUI gtk2 CACHE STRING "GUI toolkit to use (one of: gtk2 gtk3)")
endif()

# compiler

if(WIN32)
    add_definitions(
        -D_CRT_SECURE_NO_DEPRECATE=1
        -D_CRT_SECURE_NO_WARNINGS=1
        -D_WIN32_WINNT=0x500
        -D_WIN32_IE=_WIN32_WINNT
        -DISOLATION_AWARE_ENABLED=1
        -DWIN32=1
        -DWIN32_LEAN_AND_MEAN=1)
endif()

if(MINGW)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
endif()

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed ${CMAKE_EXE_LINKER_FLAGS}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed ${CMAKE_EXE_LINKER_FLAGS}")
endif()

# dependencies

CHECK_INCLUDE_FILE("stdint.h" HAVE_STDINT_H)

find_package(OpenGL REQUIRED)

if(WIN32)
    find_package(ZLIB)

    if(NOT ZLIB_FOUND)
        message(STATUS "Using in-tree zlib")

        add_subdirectory(extlib/zlib)

        set(ZLIB_LIBRARY
            zlibstatic)
        set(ZLIB_INCLUDE_DIR
            "${CMAKE_SOURCE_DIR}/extlib/zlib"
            "${CMAKE_BINARY_DIR}/extlib/zlib")
        find_package(ZLIB REQUIRED)
    endif()

    find_package(PNG)

    if(NOT PNG_FOUND)
        message(STATUS "Using in-tree libpng")

        set(SKIP_INSTALL_ALL
            ON)
        add_subdirectory(extlib/libpng)

        set(PNG_LIBRARY
            png16_static)
        set(PNG_PNG_INCLUDE_DIR
            "${CMAKE_SOURCE_DIR}/extlib/libpng"
            "${CMAKE_BINARY_DIR}/extlib/libpng")
        find_package(PNG REQUIRED)
    endif()

    if(NOT MINGW)
        message(STATUS "Using prebuilt SpaceWare")
        set(SPACEWARE_FOUND TRUE)
        set(SPACEWARE_INCLUDE_DIR
            "${CMAKE_SOURCE_DIR}/extlib/si")
        set(SPACEWARE_LIBRARIES
            "${CMAKE_SOURCE_DIR}/extlib/si/siapp.lib")
    endif()
elseif(APPLE)
    find_package(PNG REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
else() # Linux and compatible systems
    find_package(SpaceWare)

    # Use freedesktop's pkg-config to locate everything.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ZLIB REQUIRED zlib)
    pkg_check_modules(PNG REQUIRED libpng)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    pkg_check_modules(JSONC REQUIRED json-c)
    pkg_check_modules(GLEW REQUIRED glew)

    set(HAVE_GTK TRUE)
    if(GUI STREQUAL "gtk3")
        set(HAVE_GTK3 TRUE)
        pkg_check_modules(GTKMM REQUIRED gtkmm-3.0 pangomm-1.4 x11)
    elseif(GUI STREQUAL "gtk2")
        set(HAVE_GTK2 TRUE)
        pkg_check_modules(GTKMM REQUIRED gtkmm-2.4 pangomm-1.4 x11)
    else()
        message(FATAL_ERROR "GUI unrecognized: ${GUI}")
    endif()
endif()

# components

add_subdirectory(tools)
add_subdirectory(src)
add_subdirectory(exposed)
