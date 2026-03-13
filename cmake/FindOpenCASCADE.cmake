# Try to find OpenCASCADE Technology (OCCT).
#
# Once done, this will define:
#   OpenCASCADE_FOUND        - System has OpenCASCADE
#   OpenCASCADE_INCLUDE_DIRS - The OpenCASCADE include directories
#   OpenCASCADE_LIBRARIES    - The OpenCASCADE libraries required for the integration
#
# Search order:
#   1. CMake config file shipped with OCCT (OpenCASCADEConfig.cmake)
#   2. pkg-config
#   3. Manual header/library search

# OCCT components used by SolveSpace
set(_OpenCASCADE_REQUIRED_COMPONENTS
    TKernel
    TKMath
    TKBRep
    TKGeomBase
    TKGeomAlgo
    TKG3d
    TKPrim
    TKBO
    TKBool
    TKSTEP
    TKSTEPBase
    TKSTEPAttr
    TKSTEP209
    TKIGES
    TKXSBase
    TKTopAlgo
    TKShHealing
)

# ---------------------------------------------------------------------------
# 1. Try the OCCT-supplied CMake config file first.
# ---------------------------------------------------------------------------
find_package(OpenCASCADE CONFIG QUIET)

if(OpenCASCADE_FOUND)
    message(STATUS "Found OpenCASCADE (config): ${OpenCASCADE_INSTALL_PREFIX}")

    # Collect import targets for the required components.
    set(OpenCASCADE_LIBRARIES "")
    foreach(_comp ${_OpenCASCADE_REQUIRED_COMPONENTS})
        if(TARGET ${_comp})
            list(APPEND OpenCASCADE_LIBRARIES ${_comp})
        endif()
    endforeach()

    # Fill include dirs if the config file did not set them.
    if(NOT OpenCASCADE_INCLUDE_DIRS)
        if(OpenCASCADE_INSTALL_PREFIX)
            set(OpenCASCADE_INCLUDE_DIRS
                "${OpenCASCADE_INSTALL_PREFIX}/include/opencascade")
        endif()
    endif()

    return()
endif()

# ---------------------------------------------------------------------------
# 2. pkg-config fallback.
# ---------------------------------------------------------------------------
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_OpenCASCADE QUIET opencascade)
endif()

# ---------------------------------------------------------------------------
# 3. Manual header search.
# ---------------------------------------------------------------------------
find_path(OpenCASCADE_INCLUDE_DIRS
    NAMES Standard_Version.hxx
    HINTS
        ${PC_OpenCASCADE_INCLUDEDIR}
        ${PC_OpenCASCADE_INCLUDE_DIRS}
        ENV CASROOT
    PATH_SUFFIXES
        opencascade
        include/opencascade
        OpenCASCADE
    PATHS
        /usr/include
        /usr/local/include
        /opt/opencascade/include
        /opt/local/include
)

# ---------------------------------------------------------------------------
# 3. Manual library search.
# ---------------------------------------------------------------------------
find_library(OpenCASCADE_TKernel_LIBRARY
    NAMES TKernel
    HINTS
        ${PC_OpenCASCADE_LIBDIR}
        ${PC_OpenCASCADE_LIBRARY_DIRS}
        ENV CASROOT
    PATH_SUFFIXES lib
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/opencascade/lib
        /opt/local/lib
)

if(OpenCASCADE_TKernel_LIBRARY)
    get_filename_component(_occt_lib_dir "${OpenCASCADE_TKernel_LIBRARY}" DIRECTORY)

    set(OpenCASCADE_LIBRARIES "")
    foreach(_lib ${_OpenCASCADE_REQUIRED_COMPONENTS})
        find_library(_occt_lib_${_lib}
            NAMES ${_lib}
            HINTS ${_occt_lib_dir}
            NO_DEFAULT_PATH)
        if(_occt_lib_${_lib})
            list(APPEND OpenCASCADE_LIBRARIES "${_occt_lib_${_lib}}")
        endif()
        mark_as_advanced(_occt_lib_${_lib})
    endforeach()
endif()

# ---------------------------------------------------------------------------
# Standard result handling.
# ---------------------------------------------------------------------------
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenCASCADE
    REQUIRED_VARS
        OpenCASCADE_INCLUDE_DIRS
        OpenCASCADE_TKernel_LIBRARY
    FAIL_MESSAGE
        "Could not find OpenCASCADE (OCCT). Install OCCT 7.x or set OpenCASCADE_DIR to the directory containing OpenCASCADEConfig.cmake."
)

mark_as_advanced(
    OpenCASCADE_INCLUDE_DIRS
    OpenCASCADE_TKernel_LIBRARY
)
