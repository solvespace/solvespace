# cmake configuration

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
cmake_policy(VERSION 3.1.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# for /MT on MSVC
set(CMAKE_USER_MAKE_RULES_OVERRIDE
   "${CMAKE_SOURCE_DIR}/cmake/c_flag_overrides.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
   "${CMAKE_SOURCE_DIR}/cmake/cxx_flag_overrides.cmake")

# project

# NOTE TO PACKAGERS: The embedded git commit hash is critical for rapid bug triage when the builds
# can come from a variety of sources. If you are mirroring the sources or otherwise build when
# the .git directory is not present, please comment the following line:
include(GetGitCommitHash)
# and instead uncomment the following, adding the complete git hash of the checkout you are using:
# set(GIT_COMMIT_HASH 0000000000000000000000000000000000000000)

project(solvespace)
set(solvespace_VERSION_MAJOR 3)
set(solvespace_VERSION_MINOR 0)
string(SUBSTRING "${GIT_COMMIT_HASH}" 0 8 solvespace_GIT_HASH)

if(NOT WIN32 AND NOT APPLE)
    set(GUI gtk2 CACHE STRING "GUI toolkit to use (one of: gtk2 gtk3)")
endif()

# compiler

if(WIN32)
    add_definitions(
        -D_CRT_SECURE_NO_DEPRECATE
        -D_CRT_SECURE_NO_WARNINGS
        -D_SCL_SECURE_NO_WARNINGS
        -D_WIN32_WINNT=0x500
        -D_WIN32_IE=_WIN32_WINNT
        -DISOLATION_AWARE_ENABLED
        -DWIN32
        -DWIN32_LEAN_AND_MEAN
        -DUNICODE
        -D_UNICODE
        -DNOMINMAX
        -D_USE_MATH_DEFINES)
endif()

if(MSVC)
    # Many versions of MSVC do not have the (C99) inline keyword, instead
    # they have their own __inline; this breaks `static inline` functions.
    # We do not want to care and so we fix this with a definition.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Dinline=__inline")
    # Same for the (C99) __func__ special variable; we use it only in C++ code.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D__func__=__FUNCTION__")

    # We rely on these /we flags. They correspond to the GNU-style flags below as
    # follows: /w4062=-Wswitch
    set(WARNING_FLAGS   "${WARNING_FLAGS} /we4062")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
        # GCC 4.8/4.9 ship with broken but present <regex>. meh.
        message(FATAL_ERROR "GCC 5.0+ is required")
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL GNU OR CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    set(WARNING_FLAGS   "-Wall -Wextra -Wno-unused-parameter")
    if(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        set(WARNING_FLAGS "${WARNING_FLAGS} -Wfloat-conversion")
    endif()
    # We rely on these -Werror flags.
    set(WARNING_FLAGS   "${WARNING_FLAGS} -Werror=switch")
endif()

if(WIN32)
    set(CMAKE_RC_FLAGS  "${CMAKE_RC_FLAGS} -l0")
endif()

if(MINGW)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -static-libgcc")
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

if(SANITIZE)
    if(NOT (CMAKE_C_COMPILER_ID STREQUAL Clang AND CMAKE_CXX_COMPILER_ID STREQUAL Clang))
        message(ERROR "Sanitizers are only available in Clang/Clang++")
    endif()
    set(SANITIZE_FLAGS  "-O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    set(SANITIZE_FLAGS  "${SANITIZE_FLAGS} -fsanitize=address,undefined,integer")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
endif()

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS}")

# dependencies

find_package(OpenGL REQUIRED)

message(STATUS "Using in-tree libdxfrw")
add_subdirectory(extlib/libdxfrw)

if(WIN32)
    # Configure Freetype first. If done later, it will notice that
    # zlib is available, try to use it and promptly break on MSVC
    # in a very obscure way. Given that the only use of zlib, bzip2
    # and png support is in support for extremely obsolete Unix fonts,
    # we don't care.
    find_package(Freetype)

    if(NOT FREETYPE_FOUND)
        message(STATUS "Using in-tree libfreetype")

        add_subdirectory(extlib/libfreetype EXCLUDE_FROM_ALL)

        set(FREETYPE_LIBRARY
            freetype)
        set(FREETYPE_INCLUDE_DIRS
            "${CMAKE_SOURCE_DIR}/extlib/libfreetype/include")
        find_package(Freetype REQUIRED)
    endif()

    find_package(ZLIB)

    if(NOT ZLIB_FOUND)
        message(STATUS "Using in-tree zlib")
        add_subdirectory(extlib/zlib EXCLUDE_FROM_ALL)

        message(STATUS "Using in-tree libpng")
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
        add_subdirectory(extlib/libpng EXCLUDE_FROM_ALL)

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
    set(CMAKE_FIND_FRAMEWORK LAST)

    find_package(PNG REQUIRED)
    find_package(Freetype REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
else() # Linux and compatible systems
    find_package(Backtrace)
    find_package(SpaceWare)

    # Use freedesktop's pkg-config to locate everything.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ZLIB REQUIRED zlib)
    pkg_check_modules(PNG REQUIRED libpng)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    pkg_check_modules(JSONC REQUIRED json-c)
    pkg_check_modules(GLEW REQUIRED glew)
    pkg_check_modules(FREETYPE REQUIRED freetype2)

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

add_subdirectory(res)
add_subdirectory(src)
add_subdirectory(exposed)
