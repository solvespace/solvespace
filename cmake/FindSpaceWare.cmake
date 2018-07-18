# Find the libspnav library and header.
#
# Sets the usual variables expected for find_package scripts:
#
# SPACEWARE_INCLUDE_DIR - header location
# SPACEWARE_LIBRARIES - library to link against
# SPACEWARE_FOUND - true if libspnav was found.

if(UNIX)

    find_path(SPACEWARE_INCLUDE_DIR
        spnav.h)

    find_library(SPACEWARE_LIBRARY
        NAMES spnav libspnav)

    # Support the REQUIRED and QUIET arguments, and set SPACEWARE_FOUND if found.
    include(FindPackageHandleStandardArgs)
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(SPACEWARE DEFAULT_MSG
        SPACEWARE_LIBRARY SPACEWARE_INCLUDE_DIR)

    if(SPACEWARE_FOUND)
        set(SPACEWARE_LIBRARIES ${SPACEWARE_LIBRARY})
    endif()

    mark_as_advanced(SPACEWARE_LIBRARY SPACEWARE_INCLUDE_DIR)

endif()
