# Find the given library in the system locations, or build in-tree if not found.
#
# Arguments:
#   PKG_NAME - name of the package as passed to find_package
#   PKG_PATH - name of the source tree relative to extlib/
#
# The rest of the arguments are VARIABLE VALUE pairs. If the library is not found,
# every VARIABLE will be set to VALUE and find_package will be rerun with the REQUIRED flag.
# Regardless of where the library was found, only the specified VARIABLEs that start with
# ${PKG_NAME} will be set in the parent scope.
#
# All warnings in the in-tree package are disabled.

include(DisableWarnings)

function(find_vendored_package PKG_NAME PKG_PATH)
    if(NOT FORCE_VENDORED_${PKG_NAME})
        find_package(${PKG_NAME})
    endif()

    set(cfg_name)
    foreach(item ${ARGN})
        if(NOT cfg_name)
            set(cfg_name ${item})
        else()
            set(${cfg_name} ${item} CACHE INTERNAL "")
            set(cfg_name)
        endif()
    endforeach()

    disable_warnings()

    string(TOUPPER ${PKG_NAME} VAR_NAME)
    if(NOT ${VAR_NAME}_FOUND)
        message(STATUS "Using in-tree ${PKG_PATH}")
        set(${VAR_NAME}_IN_TREE YES CACHE INTERNAL "")

        add_subdirectory(extlib/${PKG_PATH} EXCLUDE_FROM_ALL)
        find_package(${PKG_NAME} REQUIRED)
    elseif(${VAR_NAME}_IN_TREE)
        add_subdirectory(extlib/${PKG_PATH} EXCLUDE_FROM_ALL)
    endif()

    # Now put everything we just discovered into the cache.
    set(cfg_name)
    foreach(item ${ARGN} ${VAR_NAME}_FOUND)
        if(NOT cfg_name)
            set(cfg_name ${item})
        else()
            if(cfg_name MATCHES "^${VAR_NAME}")
                set(${cfg_name} "${${cfg_name}}" CACHE INTERNAL "")
            endif()
            set(cfg_name)
        endif()
    endforeach()
endfunction()
