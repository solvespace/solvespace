include(GNUInstallDirs)

# configuration

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

set(HAVE_SPACEWARE ${SPACEWARE_FOUND})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# platform utilities

if(WIN32)
    set(util_SOURCES
        platform/w32util.cpp)
else()
    set(util_SOURCES
        platform/unixutil.cpp)
endif()

# libslvs

set(libslvs_SOURCES
    util.cpp
    entity.cpp
    expr.cpp
    constraint.cpp
    constrainteq.cpp
    system.cpp)

set(libslvs_HEADERS
    solvespace.h)

add_library(slvs SHARED
    ${libslvs_SOURCES}
    ${libslvs_HEADERS}
    ${util_SOURCES}
    lib.cpp)

target_compile_definitions(slvs
    PRIVATE -DLIBRARY)

target_include_directories(slvs
    PUBLIC ${CMAKE_SOURCE_DIR}/include)

set_target_properties(slvs PROPERTIES
    PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/include/slvs.h
    VERSION ${solvespace_VERSION_MAJOR}.${solvespace_VERSION_MINOR}
    SOVERSION 1)

if(NOT WIN32)
    install(TARGETS slvs
        LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

# solvespace dependencies

include_directories(
    ${OPENGL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
    ${PNG_PNG_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS})

if(SPACEWARE_FOUND)
    include_directories(
        ${SPACEWARE_INCLUDE_DIR})
endif()

if(WIN32)
    set(gl_SOURCES
        render/rendergl1.cpp)
else()
    set(gl_SOURCES
        render/gl2shader.cpp
        render/rendergl2.cpp)
endif()

if(WIN32)
    set(platform_SOURCES
        platform/w32main.cpp
        ${gl_SOURCES})

    set(platform_LIBRARIES
        comctl32
        ${SPACEWARE_LIBRARIES})
elseif(APPLE)
    add_compile_options(
        -fobjc-arc)

    set(platform_SOURCES
        platform/cocoamain.mm
        render/rendergl.cpp
        ${gl_SOURCES})

    set(platform_BUNDLED_LIBS
        ${PNG_LIBRARIES}
        ${FREETYPE_LIBRARIES})

    set(platform_LIBRARIES
        ${APPKIT_LIBRARY})
elseif(HAVE_GTK)
    set(platform_SOURCES
        platform/gtkmain.cpp
        render/rendergl.cpp
        ${gl_SOURCES})

    set(platform_LIBRARIES
        ${SPACEWARE_LIBRARIES})

    foreach(pkg_config_lib GTKMM JSONC FONTCONFIG)
        include_directories(${${pkg_config_lib}_INCLUDE_DIRS})
        link_directories(${${pkg_config_lib}_LIBRARY_DIRS})
        list(APPEND platform_LIBRARIES ${${pkg_config_lib}_LIBRARIES})
    endforeach()
endif()

# solvespace library

set(solvespace_cad_HEADERS
    config.h
    dsc.h
    expr.h
    polygon.h
    sketch.h
    solvespace.h
    ui.h
    render/render.h
    render/gl2shader.h
    srf/surface.h)

set(solvespace_cad_SOURCES
    bsp.cpp
    clipboard.cpp
    confscreen.cpp
    constraint.cpp
    constrainteq.cpp
    describescreen.cpp
    draw.cpp
    drawconstraint.cpp
    drawentity.cpp
    entity.cpp
    exportstep.cpp
    exportvector.cpp
    expr.cpp
    file.cpp
    generate.cpp
    graphicswin.cpp
    group.cpp
    groupmesh.cpp
    importdxf.cpp
    mesh.cpp
    modify.cpp
    mouse.cpp
    polyline.cpp
    polygon.cpp
    resource.cpp
    request.cpp
    style.cpp
    system.cpp
    textscreens.cpp
    textwin.cpp
    toolbar.cpp
    ttf.cpp
    undoredo.cpp
    util.cpp
    view.cpp
    render/render.cpp
    render/render2d.cpp
    srf/boolean.cpp
    srf/curve.cpp
    srf/merge.cpp
    srf/ratpoly.cpp
    srf/raycast.cpp
    srf/surface.cpp
    srf/surfinter.cpp
    srf/triangulate.cpp)

set(solvespace_cad_gl_SOURCES
    export.cpp
    solvespace.cpp)

add_library(solvespace_cad STATIC
    ${util_SOURCES}
    ${solvespace_cad_HEADERS}
    ${solvespace_cad_SOURCES})

target_link_libraries(solvespace_cad
    dxfrw
    ${ZLIB_LIBRARY}
    ${PNG_LIBRARY}
    ${FREETYPE_LIBRARY}
    ${Backtrace_LIBRARIES})

target_compile_options(solvespace_cad
    PRIVATE ${COVERAGE_FLAGS})

# solvespace gui executable

add_executable(solvespace WIN32 MACOSX_BUNDLE
    ${solvespace_cad_gl_SOURCES}
    ${platform_SOURCES}
    $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

add_dependencies(solvespace
    resources)

target_link_libraries(solvespace
    solvespace_cad
    ${OPENGL_LIBRARIES}
    ${platform_LIBRARIES}
    ${COVERAGE_LIBRARY})

if(WIN32 AND NOT MINGW)
    set_target_properties(solvespace PROPERTIES
        LINK_FLAGS "/MANIFEST:NO /SAFESEH:NO")
endif()

if(APPLE)
    foreach(lib ${platform_BUNDLED_LIBS})
        get_filename_component(name ${lib} NAME)
        set(target ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/MacOS/${name})

        execute_process(COMMAND otool -D ${lib}
            OUTPUT_VARIABLE canonical_lib OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX REPLACE "^.+:\n" "" canonical_lib ${canonical_lib})
        add_custom_command(TARGET solvespace POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${lib} ${target}
            COMMAND install_name_tool -change ${canonical_lib} @executable_path/${name}
                    $<TARGET_FILE:solvespace>
            COMMENT "Bundling shared library ${lib}"
            VERBATIM)
    endforeach()

    set(bundle solvespace)
    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/${bundle}.dmg
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${bundle}.dmg
        COMMAND hdiutil create -srcfolder ${CMAKE_CURRENT_BINARY_DIR}/${bundle}.app
                ${CMAKE_BINARY_DIR}/${bundle}.dmg
        DEPENDS $<TARGET_FILE:${bundle}>
        COMMENT "Building ${bundle}.dmg"
        VERBATIM)
    add_custom_target(${bundle}-dmg ALL
        DEPENDS ${CMAKE_BINARY_DIR}/${bundle}.dmg)
endif()

if(NOT WIN32)
    install(TARGETS solvespace
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE  DESTINATION .)
endif()

# solvespace headless library

set(headless_SOURCES
    platform/headless.cpp
    render/rendercairo.cpp)

add_library(solvespace_headless STATIC EXCLUDE_FROM_ALL
    ${solvespace_cad_gl_SOURCES}
    ${headless_SOURCES})

target_compile_definitions(solvespace_headless
    PRIVATE -DHEADLESS)

target_include_directories(solvespace_headless
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(solvespace_headless
    solvespace_cad
    ${CAIRO_LIBRARIES})

target_compile_options(solvespace_headless
    PRIVATE ${COVERAGE_FLAGS})
