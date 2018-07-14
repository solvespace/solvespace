# Equivalent to add_subdirectory(... EXCLUDE_FROM_ALL), but also disables
# all warnings.

include(DisableWarnings)

function(add_vendored_subdirectory PATH)
    disable_warnings()

    add_subdirectory(${PATH} EXCLUDE_FROM_ALL)
endfunction()
