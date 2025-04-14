# cmake configuration
cmake_minimum_required(VERSION 3.18...3.25)

cmake_policy(SET CMP0094 NEW) # for Python*_FIND_STRATEGY=LOCATION

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
            "In-tree builds are not supported; please perform an out-of-tree build:\n"
            "    rm -rf CMakeCache.txt CMakeFiles/\n"
            "    mkdir build && cd build && cmake ..")
endif()

list(APPEND CMAKE_MODULE_PATH
    "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# for /MT on MSVC
set(CMAKE_USER_MAKE_RULES_OVERRIDE
   "${CMAKE_SOURCE_DIR}/cmake/c_flag_overrides.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
   "${CMAKE_SOURCE_DIR}/cmake/cxx_flag_overrides.cmake")

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if (APPLE)
    # Docs say this must be set before the first project() call
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12" CACHE STRING "macOS minimum supported version")
endif()

# project

# NOTE TO PACKAGERS: The embedded git commit hash is critical for rapid bug triage when the builds
# can come from a variety of sources. If you are mirroring the sources or otherwise build when
# the .git directory is not present, please comment the following line:
include(GetGitCommitHash)
# and instead uncomment the following, adding the complete git hash of the checkout you are using:
# set(GIT_COMMIT_HASH 0000000000000000000000000000000000000000)

string(SUBSTRING "${GIT_COMMIT_HASH}" 0 8 solvespace_GIT_HASH)
project(solvespace
    VERSION 3.1
    LANGUAGES C CXX ASM)

if(EMSCRIPTEN)
    # Enable optimization. Workaround for "too many locals" error when runs in the browser.
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O1)
    endif()

    set(INCLUDE_NATIVE_COMPONENTS OFF)
else()
    set(INCLUDE_NATIVE_COMPONENTS ON)
endif()

set(ENABLE_GUI        ON CACHE BOOL
    "Whether the graphical interface is enabled")
set(ENABLE_CLI        ${INCLUDE_NATIVE_COMPONENTS} CACHE BOOL
    "Whether the command line interface is enabled")
set(ENABLE_TESTS      ${INCLUDE_NATIVE_COMPONENTS} CACHE BOOL
    "Whether the test suite will be built and run")
set(ENABLE_COVERAGE   OFF CACHE BOOL
    "Whether code coverage information will be collected")
set(ENABLE_SANITIZERS OFF CACHE BOOL
    "Whether to enable Clang's AddressSanitizer and UndefinedBehaviorSanitizer")
set(ENABLE_OPENMP     OFF CACHE BOOL
    "Whether geometric operations will be parallelized using OpenMP")
set(ENABLE_LTO        OFF CACHE BOOL
    "Whether interprocedural (global) optimizations are enabled")
option(FORCE_VENDORED_Eigen3
    "Whether we should use our bundled Eigen even in the presence of a system copy"
    OFF)

set(OPENGL 3 CACHE STRING "OpenGL version to use (one of: 1 3)")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR}/bin)
if("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
        # GCC 4.8/4.9 ship with broken but present <regex>. meh.
        message(FATAL_ERROR "GCC 5.0+ is required")
    endif()
endif()

# common compiler flags
include(CheckCXXCompilerFlag)

if (NOT APPLE)
    set(FILE_PREFIX_MAP "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
    check_cxx_compiler_flag("${FILE_PREFIX_MAP}" HAS_FILE_PREFIX_MAP)
    if(HAS_FILE_PREFIX_MAP)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FILE_PREFIX_MAP}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FILE_PREFIX_MAP}")
    endif()
endif()

if(MINGW)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -static-libgcc")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    # Link 32 bit SolveSpace with --large-address-aware which allows it to access
    # up to 3GB on a properly configured 32 bit Windows and up to 4GB on 64 bit.
    # See https://msdn.microsoft.com/en-us/library/aa366778
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--large-address-aware")
endif()

# Ensure that all platforms use 64-bit IEEE floating point operations for consistency;
# this is most important for the testsuite, which compares savefiles directly
# and depends on consistent rounding of intermediate results.
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "i686" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "X86")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(FLOAT_FLAGS "-mfpmath=sse -msse2")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(FLOAT_FLAGS "-msse2")
    endif()

    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${FLOAT_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLOAT_FLAGS}")
endif()

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(ENABLE_OPENMP)
    find_package( OpenMP REQUIRED )
    if(OPENMP_FOUND)
        add_library(slvs_openmp INTERFACE)
        target_compile_options(slvs_openmp INTERFACE ${OpenMP_CXX_FLAGS})
        target_link_libraries(slvs_openmp INTERFACE
            ${OpenMP_CXX_LIBRARIES})
        target_include_directories(slvs_openmp SYSTEM INTERFACE
            ${OpenMP_CXX_INCLUDE_DIRS})
        message(STATUS "found OpenMP, compiling with flags: " ${OpenMP_CXX_FLAGS} )
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed ${CMAKE_EXE_LINKER_FLAGS}")
endif()

if(ENABLE_SANITIZERS)
    if(NOT SANITIZERS)
        set(SANITIZERS "address;undefined")
    endif()

    if("thread" IN_LIST SANITIZERS)
        list(REMOVE_ITEM SANITIZERS "thread")
        list(APPEND SANITIZE_OPTIONS thread)
    endif()
    if("address" IN_LIST SANITIZERS)
        list(REMOVE_ITEM SANITIZERS "address")
        list(APPEND SANITIZE_OPTIONS address)
    endif()
    if("undefined" IN_LIST SANITIZERS)
        list(REMOVE_ITEM SANITIZERS "undefined")
        list(APPEND SANITIZE_OPTIONS alignment bounds)
        list(APPEND SANITIZE_OPTIONS shift signed-integer-overflow integer-divide-by-zero)
        list(APPEND SANITIZE_OPTIONS null bool enum)
        list(APPEND SANITIZE_OPTIONS return)
    endif()
    if(SANITIZERS)
        message(FATAL_ERROR "Unknown sanitizer(s) ${SANITIZERS}")
    else()
        message(STATUS "Using sanitizer options ${SANITIZE_OPTIONS}")
    endif()

    string(REPLACE ";" "," SANITIZE_OPTIONS "${SANITIZE_OPTIONS}")

    if (NOT APPLE)
        set(SANITIZE_FLAGS "-O1 -fsanitize=${SANITIZE_OPTIONS} -fno-sanitize-recover=address,undefined")
    else()
        set(SANITIZE_FLAGS "-O1 -fsanitize=${SANITIZE_OPTIONS}")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SANITIZE_FLAGS "${SANITIZE_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(SANITIZE_FLAGS "${SANITIZE_FLAGS} -fuse-ld=gold")
    else()
        message(FATAL_ERROR "Sanitizers are only available when using GCC or Clang")
    endif()

    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${SANITIZE_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZE_FLAGS}")
endif()

# common dependencies

if(APPLE)
    set(CMAKE_FIND_FRAMEWORK FIRST)
endif()

if(ENABLE_GUI OR ENABLE_CLI)
    message(STATUS "Using in-tree libdxfrw")
    add_subdirectory(extlib/libdxfrw)
endif()

message(STATUS "Using in-tree mimalloc")
set(MI_OVERRIDE OFF CACHE BOOL "")
set(MI_BUILD_SHARED OFF CACHE BOOL "")
set(MI_BUILD_OBJECT OFF CACHE BOOL "")
set(MI_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(extlib/mimalloc EXCLUDE_FROM_ALL)
set(MIMALLOC_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/extlib/mimalloc/include)

if(NOT FORCE_VENDORED_Eigen3)
    find_package(Eigen3 CONFIG)
endif()
if(FORCE_VENDORED_Eigen3 OR NOT EIGEN3_INCLUDE_DIRS)
    message(STATUS "Using in-tree Eigen")
    set(EIGEN3_FOUND             YES)
    set(EIGEN3_INCLUDE_DIRS      ${CMAKE_SOURCE_DIR}/extlib/eigen)
else()
    message(STATUS "Using system Eigen: ${EIGEN3_INCLUDE_DIRS}")
endif()
if(NOT EXISTS "${EIGEN3_INCLUDE_DIRS}")
    message(FATAL_ERROR "Eigen 3 not found on system or in-tree")
endif()


if(ENABLE_GUI OR ENABLE_CLI)
    if(WIN32 OR APPLE OR EMSCRIPTEN)
        # On Win32 and macOS we use vendored packages, since there is little to no benefit
        # to trying to find system versions. In particular, trying to link to libraries from
        # Homebrew or macOS system libraries into the .app file is highly likely to result
        # in incompatibilities after upgrades.

        include(FindVendoredPackage)
        include(AddVendoredSubdirectory)

        set(FORCE_VENDORED_ZLIB     ON)
        set(FORCE_VENDORED_PNG      ON)
        set(FORCE_VENDORED_Freetype ON)

        # There's an issue with the zlib configuration, which doesn't provide a way to
        # disable the shared or static library builds, and on UNIX sets the base name of
        # the output for both the static and shared targets to `libz`. As Emscripten doesn't
        # support shared libraries, CMake converts the shared library target to a static
        # library and both targets end up with the name `libz.a`, causing Ninja to complain
        # about different rules generating the same target. As a workaround, temporarily
        # unset UNIX for Emscripten while the zlib configuration is being processed, and
        # reset it back afterwards.
        # NOTE: The unreleased develop branch of zlib after version 1.3.1 allows disabling
        # the shared library build, which is the proper fix for this issue, so when the
        # next version of zlib is released and updated as a dependency, `ZLIB_BUILD_SHARED`
        # should be set to `OFF` instead of this hack.
        if(EMSCRIPTEN AND UNIX)
            set(zlib_workaround_unix_was_unset 1)
            unset(UNIX)
        endif()

        find_vendored_package(ZLIB zlib
            ZLIB_LIBRARY            zlibstatic
            ZLIB_INCLUDE_DIR        ${CMAKE_SOURCE_DIR}/extlib/zlib)
        list(APPEND ZLIB_INCLUDE_DIR ${CMAKE_BINARY_DIR}/extlib/zlib)

        if(zlib_workaround_unix_was_unset)
            set(UNIX 1)
            unset(zlib_workaround_unix_was_unset)
        endif()

        find_vendored_package(PNG libpng
            SKIP_INSTALL_ALL        ON
            PNG_LIBRARY             png_static
            PNG_ARM_NEON            "off"
            PNG_SHARED              OFF
            PNG_STATIC              ON
            PNG_EXECUTABLES         OFF
            PNG_TESTS               OFF
            PNG_FRAMEWORK           OFF
            PNG_PNG_INCLUDE_DIR     ${CMAKE_SOURCE_DIR}/extlib/libpng)
        list(APPEND PNG_PNG_INCLUDE_DIR ${CMAKE_BINARY_DIR}/extlib/libpng)

        find_vendored_package(Freetype freetype
            FT_DISABLE_BZIP2        ON
            FT_DISABLE_HARFBUZZ     ON
            FT_DISABLE_BROTLI       ON
            FT_DISABLE_PNG          ON
            FREETYPE_LIBRARY        freetype
            FREETYPE_INCLUDE_DIRS   ${CMAKE_SOURCE_DIR}/extlib/freetype/include)

        message(STATUS "Using in-tree pixman")
        set(PIXMAN_FOUND            YES)
        set(PIXMAN_LIBRARY          pixman)
        set(PIXMAN_BUILD_TESTS      OFF CACHE BOOL "")
        set(PIXMAN_BUILD_DEMOS      OFF CACHE BOOL "")

        set(PIXMAN_INCLUDE_DIRS     ${CMAKE_SOURCE_DIR}/extlib/pixman/pixman)
        list(APPEND PIXMAN_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/extlib/pixman/pixman)
        add_vendored_subdirectory(extlib/pixman)

        message(STATUS "Using in-tree cairo")
        add_vendored_subdirectory(extlib/cairo)
        set(CAIRO_FOUND             YES)
        set(CAIRO_LIBRARIES         cairo)
        set(CAIRO_INCLUDE_DIRS      ${CMAKE_SOURCE_DIR}/extlib/cairo/src)
        list(APPEND CAIRO_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/extlib/cairo/src)
    else()
        # On Linux and BSDs we're a good citizen and link to system libraries.
        find_package(Backtrace)
        find_package(PkgConfig REQUIRED)
        find_package(ZLIB REQUIRED)
        find_package(PNG REQUIRED)
        find_package(Freetype REQUIRED)
        find_package(Cairo REQUIRED)
    endif()
endif()

# GUI dependencies

if(ENABLE_GUI)
    if(WIN32)
        if(OPENGL STREQUAL "3")
            message(STATUS "Using in-tree ANGLE")
            set(ANGLE_STATIC            ON  CACHE INTERNAL "")
            set(ANGLE_ENABLE_D3D9       ON  CACHE INTERNAL "")
            set(ANGLE_ENABLE_D3D11      ON  CACHE INTERNAL "")
            set(ANGLE_ENABLE_OPENGL     ON  CACHE INTERNAL "")
            set(ANGLE_ENABLE_ESSL       ON  CACHE INTERNAL "")
            set(ANGLE_ENABLE_GLSL       ON  CACHE INTERNAL "")
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
    elseif(APPLE)
        find_package(OpenGL REQUIRED)
        find_library(APPKIT_LIBRARY AppKit REQUIRED)
    elseif(EMSCRIPTEN)
        # Everything is built in
    else()
        find_package(OpenGL REQUIRED)
        find_package(SpaceWare)
        pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
        pkg_check_modules(JSONC REQUIRED json-c)
        pkg_check_modules(GTKMM REQUIRED gtkmm-3.0>=3.18 pangomm-1.4 x11)
    endif()
endif()

# code coverage

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

# translations

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
        -DWINVER=0x0501
        -D_WIN32_WINNT=0x0501
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

    # Multi-processor Compilation
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

    # We rely on these /we flags. They correspond to the GNU-style flags below as
    # follows: /w4062=-Wswitch
    set(WARNING_FLAGS   "${WARNING_FLAGS} /we4062")

    # Link 32 bit SolveSpace with /LARGEADDRESSAWARE which allows it to access
    # up to 3GB on a properly configured 32 bit Windows and up to 4GB on 64 bit.
    # See https://msdn.microsoft.com/en-us/library/aa366778
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /LARGEADDRESSAWARE")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(WARNING_FLAGS   "-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
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

# application components
if(ENABLE_GUI OR ENABLE_CLI)
    add_subdirectory(res)
    add_subdirectory(exposed)
endif()
add_subdirectory(src)
if(ENABLE_TESTS)
    add_subdirectory(test)
endif()
if((ENABLE_CLI OR ENABLE_GUI) AND (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    add_subdirectory(bench)
else()
    message(STATUS "Benchmarking disabled in debug and library builds.")
endif()
