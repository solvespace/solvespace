include(GNUInstallDirs)

# libslvs
add_library(slvs-interface INTERFACE)
target_sources(slvs-interface INTERFACE lib.cpp)
target_compile_definitions(slvs-interface INTERFACE -DLIBRARY)
target_include_directories(slvs-interface INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(slvs-interface INTERFACE slvs-solver mimalloc-static)

if(ENABLE_PYTHON_LIB)
    add_custom_command(
        OUTPUT cylib.c
        COMMAND cython lib.pyx -o ${CMAKE_CURRENT_BINARY_DIR}/cylib.c --include-dir ${CMAKE_SOURCE_DIR}/include --module-name solvespace
        DEPENDS lib.pyx
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
    Python_add_library(slvs-py MODULE cylib.c)
    set_target_properties(slvs-py PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(slvs-py PRIVATE -DSTATIC_LIB)
    target_link_libraries(
        slvs-py
    PRIVATE
        slvs-interface
        Python::Module
    )
    set_target_properties(slvs-py PROPERTIES LIBRARY_OUTPUT_NAME solvespace)
    if(SKBUILD)
        install(TARGETS slvs-py
            DESTINATION ${SKBUILD_PROJECT_NAME}
            COMPONENT   python
        )
    else()
        install(TARGETS slvs-py
            DESTINATION "slvs"
            COMPONENT   python
        )
        install(
            DIRECTORY ${CMAKE_SOURCE_DIR}/python/slvs/
            DESTINATION "slvs"
            COMPONENT   python
        )
    endif()
endif()

if(EMSCRIPTEN)
    add_executable(slvs-wasm jslib.cpp)
    target_link_libraries(slvs-wasm PRIVATE slvs-interface embind)
    target_link_options(slvs-wasm PRIVATE
        "SHELL:-s MODULARIZE=1"
        "SHELL:-s EXPORT_NAME=solvespace"
        "SHELL:-s SINGLE_FILE=1"
        "SHELL:-s INITIAL_MEMORY=512MB"
        "SHELL:-s ALLOW_MEMORY_GROWTH"
        -O3
        -flto
        --closure 1)
    set_target_properties(slvs-wasm PROPERTIES OUTPUT_NAME "slvs" SUFFIX ".js")
endif()

add_library(slvs SHARED)
target_compile_definitions(slvs PRIVATE -DEXPORT_DLL)
target_link_libraries(slvs PRIVATE slvs-interface)
set_target_properties(slvs PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(slvs PUBLIC ${CMAKE_SOURCE_DIR}/include)

set_target_properties(slvs PROPERTIES
    PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/include/slvs.h
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# if(NOT WIN32)
install(TARGETS slvs
    COMPONENT     slvs
    LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# endif()
