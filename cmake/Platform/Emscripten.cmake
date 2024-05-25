set(EMSCRIPTEN 1)

set(CMAKE_C_OUTPUT_EXTENSION   ".o")
set(CMAKE_CXX_OUTPUT_EXTENSION ".o")
set(CMAKE_EXECUTABLE_SUFFIX    ".html")

set(CMAKE_SIZEOF_VOID_P 4)

set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS FALSE)

# FIXME(emscripten): Suppress non-c-typedef-for-linkage warnings in solvespace.h
add_compile_options(-Wno-non-c-typedef-for-linkage)
add_link_options(-s EXPORTED_RUNTIME_METHODS=[allocate])

# Enable optimization. Workaround for "too many locals" error when runs on browser.
if(CMAKE_BUILD_TYPE STREQUAL Release)
    add_compile_options(-O2)
else()
    add_compile_options(-O1)
endif()
