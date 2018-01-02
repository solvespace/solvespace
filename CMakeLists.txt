# cmake configuration

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
            "In-tree builds are not supported; please perform an out-of-tree build:\n"
            "    rm -rf CMakeCache.txt CMakeFiles/\n"
            "    mkdir build && cd build && cmake ..")
endif()

cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)
cmake_policy(VERSION 3.2.0)
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

set(ENABLE_GUI        ON CACHE BOOL
    "Whether the graphical interface is enabled (command line interface always is)")
set(ENABLE_TESTS      ON  CACHE BOOL
    "Whether the test suite will be built and run")
set(ENABLE_COVERAGE   OFF CACHE BOOL
    "Whether code coverage information will be collected")
set(ENABLE_SANITIZERS OFF CACHE BOOL
    "Whether to enable Clang's AddressSanitizer and UndefinedBehaviorSanitizer")

set(OPENGL 3 CACHE STRING "OpenGL version to use (one of: 1 3)")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_C_COMPILER_ID STREQUAL CMAKE_CXX_COMPILER_ID)
    message(FATAL_ERROR "C and C++ compilers should be supplied by the same vendor")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
        # GCC 4.8/4.9 ship with broken but present <regex>. meh.
        message(FATAL_ERROR "GCC 5.0+ is required")
    endif()
endif()

# common compiler flags

if(MINGW)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -static-libgcc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")

    if(TRIPLE STREQUAL "i686-w64-mingw32")
        set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -msse2")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
    endif()
endif()

if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed ${CMAKE_EXE_LINKER_FLAGS}")
endif()

if(ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Sanitizers are only available when using Clang/Clang++")
    endif()
    set(SANITIZE_FLAGS  "-O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    set(SANITIZE_FLAGS  "${SANITIZE_FLAGS} -fsanitize=address,integer")
    set(SANITIZE_FLAGS  "${SANITIZE_FLAGS} -fno-sanitize-recover=integer")
    # We assume IEEE floats, which means DIV/0 is defined; but ubsan doesn't do so by default.
    set(SANITIZE_FLAGS  "${SANITIZE_FLAGS} -fno-sanitize=float-divide-by-zero")
    set(SANITIZE_FLAGS  "${SANITIZE_FLAGS} -fno-sanitize=unsigned-integer-overflow")

    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
endif()

# dependencies

message(STATUS "Using in-tree libdxfrw")
add_subdirectory(extlib/libdxfrw)

if(WIN32)
    include(FindVendoredPackage)
    include(AddVendoredSubdirectory)

    find_vendored_package(Freetype freetype
        WITH_ZLIB               OFF
        WITH_BZip2              OFF
        WITH_PNG                OFF
        WITH_HarfBuzz           OFF
        FREETYPE_LIBRARY        freetype
        FREETYPE_INCLUDE_DIRS   ${CMAKE_SOURCE_DIR}/extlib/freetype/include)

    find_vendored_package(ZLIB zlib
        ZLIB_LIBRARY            zlibstatic
        ZLIB_INCLUDE_DIR        ${CMAKE_SOURCE_DIR}/extlib/zlib)
    list(APPEND ZLIB_INCLUDE_DIR ${CMAKE_BINARY_DIR}/extlib/zlib)

    find_vendored_package(PNG libpng
        SKIP_INSTALL_ALL        ON
        PNG_LIBRARY             png_static
        PNG_PNG_INCLUDE_DIR     ${CMAKE_SOURCE_DIR}/extlib/libpng)
    list(APPEND PNG_PNG_INCLUDE_DIR ${CMAKE_BINARY_DIR}/extlib/libpng)

    message(STATUS "Using in-tree pixman")
    add_vendored_subdirectory(extlib/pixman)
    set(PIXMAN_FOUND            YES)
    set(PIXMAN_LIBRARY          pixman)
    set(PIXMAN_INCLUDE_DIRS     ${CMAKE_SOURCE_DIR}/extlib/pixman/pixman)
    list(APPEND PIXMAN_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/extlib/pixman/pixman)

    message(STATUS "Using in-tree cairo")
    add_vendored_subdirectory(extlib/cairo)
    set(CAIRO_FOUND             YES)
    set(CAIRO_LIBRARIES         cairo)
    set(CAIRO_INCLUDE_DIRS      ${CMAKE_SOURCE_DIR}/extlib/cairo/src)
    list(APPEND CAIRO_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/extlib/cairo/src)

    if(ENABLE_GUI)
        if(OPENGL STREQUAL "3")
            message(STATUS "Using in-tree ANGLE")
            set(ANGLE_STATIC            ON  CACHE INTERNAL "")
            set(ANGLE_ENABLE_D3D9       ON  CACHE INTERNAL "")
            set(ANGLE_ENABLE_D3D11      ON  CACHE INTERNAL "")
            set(ANGLE_ENABLE_OPENGL     OFF CACHE INTERNAL "")
            set(ANGLE_ENABLE_ESSL       OFF CACHE INTERNAL "")
            set(ANGLE_ENABLE_GLSL       OFF CACHE INTERNAL "")
            set(ANGLE_ENABLE_HLSL       ON  CACHE INTERNAL "")
            add_vendored_subdirectory(extlib/angle)
            set(OPENGL_LIBRARIES        EGL GLESv2)
            set(OPENGL_INCLUDE_DIR      ${CMAKE_SOURCE_DIR}/extlib/angle/include)
        else()
            find_package(OpenGL REQUIRED)
        endif()

        if(MSVC AND ${CMAKE_SIZEOF_VOID_P} EQUAL 4)
            message(STATUS "Using prebuilt SpaceWare")
            set(SPACEWARE_FOUND TRUE)
            set(SPACEWARE_INCLUDE_DIR
                "${CMAKE_SOURCE_DIR}/extlib/si")
            set(SPACEWARE_LIBRARIES
                "${CMAKE_SOURCE_DIR}/extlib/si/siapp.lib")
        endif()
    endif()
elseif(APPLE)
    set(CMAKE_FIND_FRAMEWORK LAST)

    find_package(ZLIB REQUIRED)
    find_package(PNG REQUIRED)
    find_package(Freetype REQUIRED)

    find_library(CAIRO_LIBRARIES cairo REQUIRED)
    find_path(CAIRO_INCLUDE_DIRS cairo.h PATH_SUFFIXES cairo)

    if(ENABLE_GUI)
        find_package(OpenGL REQUIRED)
        find_library(APPKIT_LIBRARY AppKit REQUIRED)
    endif()
else() # Linux and compatible systems
    find_package(PkgConfig REQUIRED)

    find_package(Backtrace)
    find_package(SpaceWare)

    find_package(ZLIB REQUIRED)
    find_package(PNG REQUIRED)
    find_package(Freetype REQUIRED)
    pkg_check_modules(CAIRO REQUIRED cairo)

    if(ENABLE_GUI)
        find_package(OpenGL REQUIRED)
        pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
        pkg_check_modules(JSONC REQUIRED json-c)
        pkg_check_modules(GTKMM REQUIRED gtkmm-3.0>=3.16 pangomm-1.4 x11)
    endif()
endif()

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
        find_program(GCOV gcov)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES Clang)
        find_program(LLVM_COV llvm-cov)

        if(LLVM_COV)
            set(GCOV ${CMAKE_CURRENT_BINARY_DIR}/llvm-gcov.sh)
            file(WRITE ${GCOV} "#!/bin/sh -e\n${LLVM_COV} gcov $*")
            execute_process(COMMAND chmod +x ${GCOV})
        endif()
    endif()

    find_program(LCOV lcov)
    find_program(GENHTML genhtml)
    if(NOT GCOV OR NOT LCOV OR NOT GENHTML)
        message(FATAL_ERROR "gcov/llvm-cov and lcov are required for producing coverage reports")
    endif()
endif()

find_program(XGETTEXT xgettext)
find_program(MSGINIT  msginit)
find_program(MSGMERGE msgmerge)
if(XGETTEXT AND MSGINIT AND MSGMERGE)
    set(HAVE_GETTEXT TRUE)
else()
    message(WARNING "Gettext not found, translations will not be updated")
    set(HAVE_GETTEXT FALSE)
endif()

# solvespace-only compiler flags

if(WIN32)
    add_definitions(
        -D_CRT_SECURE_NO_DEPRECATE
        -D_CRT_SECURE_NO_WARNINGS
        -D_SCL_SECURE_NO_WARNINGS
        -DWINVER=0x0601
        -D_WIN32_WINNT=0x0601
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

    # Needed to avoid crash on start with VS2017, etc.
    set(CMAKE_EXE_LINKER_FLAGS "/STACK:33554432 ${CMAKE_EXE_LINKER_FLAGS}")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(WARNING_FLAGS   "-Wall -Wextra -Wno-unused-parameter")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(WARNING_FLAGS "${WARNING_FLAGS} -Wfloat-conversion")
    endif()
    # We rely on these -Werror flags.
    set(WARNING_FLAGS   "${WARNING_FLAGS} -Werror=switch")
endif()

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS}")

if(WIN32)
    set(CMAKE_RC_FLAGS  "${CMAKE_RC_FLAGS} -l0")
endif()

if(ENABLE_COVERAGE)
    if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
            CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
        message(FATAL_ERROR "Code coverage is only available on GCC and Clang")
    endif()

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(FATAL_ERROR "Code coverage produces reliable results only on Debug builds")
    endif()

    # With -fexceptions, every call becomes a branch. While technically accurate,
    # this is not useful for us.
    set(COVERAGE_FLAGS -fno-exceptions --coverage)
    set(COVERAGE_LIBRARY --coverage)
endif()

# components

add_subdirectory(res)
add_subdirectory(src)
add_subdirectory(exposed)
if(ENABLE_TESTS)
    add_subdirectory(test)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    add_subdirectory(bench)
else()
    message(STATUS "Benchmarking disabled in debug builds.")
endif()
