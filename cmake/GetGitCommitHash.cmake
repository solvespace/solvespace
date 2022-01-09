function(get_git_commit_hash)
    execute_process(COMMAND git rev-parse --git-dir OUTPUT_VARIABLE GIT_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND git rev-parse HEAD OUTPUT_VARIABLE HEAD_REF OUTPUT_STRIP_TRAILING_WHITESPACE)

    # Add a CMake configure dependency to the currently checked out revision.
    set(GIT_DEPENDS ${GIT_DIR}/HEAD)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GIT_DEPENDS})

    if(HEAD_REF STREQUAL "")
        message(WARNING "Cannot determine git HEAD")
    else()
        set(GIT_COMMIT_HASH ${HEAD_REF} PARENT_SCOPE)
    endif()
endfunction()
get_git_commit_hash()
