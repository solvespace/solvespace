# Compatibility overlay for STEPcode 24783b448511b8f6bce2829286a3f00015430236.
# All adjusted files live in the build tree; never modify the pinned submodule.

# ReadHeader's temporary instance manager is non-owning. Its early EOF/error
# returns therefore leak every header instance read so far (and their nodes).
# Own those instances until the normal merge transfers them to STEPfile.
set(reader_source "${SC_SOURCE_DIR}/src/cleditor/STEPfile.cc")
file(READ "${reader_source}" reader)
set(allocation "InstMgr * im = new InstMgr;")
set(transfer "HeaderMergeInstances( im ); // handles delete for im")
foreach(expected IN ITEMS "${allocation}" "${transfer}")
    string(FIND "${reader}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Review the SolveSpace STEPcode reader overlay after updating STEPcode")
    endif()
endforeach()
string(REPLACE "${allocation}" "InstMgr * im = new InstMgr(1);" reader "${reader}")
string(REPLACE "${transfer}"
    "im->OwnsInstances(0);\n    ${transfer}" reader "${reader}")
# Missing mandatory headers are rejected, but upstream first constructs default
# header objects. Their aggregate setters copy, rather than take ownership of,
# temporary aggregates. Balance those allocations on this failure path too.
foreach(copied IN ITEMS "fn->authorization_( \"\" );" "fs->schema_identifiers_( tmp );")
    string(FIND "${reader}" "${copied}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Review the SolveSpace STEPcode header-default overlay after updating STEPcode")
    endif()
    string(REPLACE "${copied}" "${copied}\n    delete tmp;" reader "${reader}")
endforeach()
file(CONFIGURE OUTPUT "${SC_BINARY_DIR}/STEPfile.compat.cc" CONTENT "${reader}" @ONLY)

# SC_ADDLIB already links every dependency with PUBLIC, then repeats the last
# dependency using the incompatible unscoped signature. Remove that call.
set(original "${SC_SOURCE_DIR}/cmake/SC_Targets.cmake")
file(READ "${original}" contents)
set(redundant [=[    target_link_libraries(${_addlib_target} ${_lib})]=])
string(FIND "${contents}" "${redundant}" found)
if(found EQUAL -1)
    message(FATAL_ERROR "Review the SolveSpace STEPcode CMake overlay after updating STEPcode")
endif()
string(REPLACE "${redundant}" "" contents "${contents}")
set(add_library [=[    add_library(${_addlib_target} STATIC ${${_arg_prefix}_SOURCES})]=])
string(FIND "${contents}" "${add_library}" found)
if(found EQUAL -1)
    message(FATAL_ERROR "Review the SolveSpace STEPcode source overlay after updating STEPcode")
endif()
set(select_reader [=[    if("${_addlib_target}" STREQUAL "stepeditor-static")
      list(FIND ${_arg_prefix}_SOURCES STEPfile.cc reader_index)
      if(reader_index EQUAL -1)
        message(FATAL_ERROR "STEPcode reader source list changed; review the overlay")
      endif()
      list(REMOVE_ITEM ${_arg_prefix}_SOURCES STEPfile.cc)
      list(APPEND ${_arg_prefix}_SOURCES "${SC_BINARY_DIR}/STEPfile.compat.cc")
    endif()
]=])
string(REPLACE "${add_library}" "${select_reader}${add_library}" contents "${contents}")
file(CONFIGURE OUTPUT "${SC_BINARY_DIR}/SC_Targets.compat.cmake" CONTENT "${contents}" @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}" "${reader_source}")
include("${SC_BINARY_DIR}/SC_Targets.compat.cmake")
