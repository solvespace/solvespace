# global

include(GNUInstallDirs)

include_directories(
    ${OPENGL_INCLUDE_DIR}
    ${PNG_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS})

link_directories(
    ${PNG_LIBRARY_DIRS}
    ${FREETYPE_LIBRARY_DIRS})

add_definitions(
    ${PNG_CFLAGS_OTHER})

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/built
    ${CMAKE_CURRENT_BINARY_DIR})

if(SPACEWARE_FOUND)
    include_directories(
        ${SPACEWARE_INCLUDE_DIR})
endif()

set(HAVE_SPACEWARE ${SPACEWARE_FOUND})
set(HAVE_GTK ${GTKMM_FOUND})
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

# platform dependencies

if(WIN32)
    set(platform_SOURCES
        platform/w32main.cpp)

    set(platform_LIBRARIES
        comctl32)
elseif(APPLE)
    add_definitions(
        -fobjc-arc)

    set(platform_SOURCES
        platform/cocoamain.mm
        platform/gloffscreen.cpp)

    set(platform_BUNDLED_LIBS
        ${PNG_LIBRARIES}
        ${FREETYPE_LIBRARIES})

    if(SPACEWARE_FOUND)
        set(platform_BUNDLED_LIBS ${platform_BUNDLED_LIBS}
            ${SPACEWARE}/Versions/A/3DconnexionClient)
    endif()

    set(platform_LIBRARIES
        ${APPKIT_LIBRARY})
elseif(HAVE_GTK)
    include_directories(
        ${GTKMM_INCLUDE_DIRS}
        ${JSONC_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIRS})

    link_directories(
        ${GTKMM_LIBRARY_DIRS}
        ${JSONC_LIBRARY_DIRS}
        ${FONTCONFIG_LIBRARY_DIRS}
        ${GLEW_LIBRARY_DIRS})

    add_definitions(
        ${GTKMM_CFLAGS_OTHER}
        ${JSONC_CFLAGS_OTHER}
        ${FONTCONFIG_CFLAGS_OTHER}
        ${GLEW_CFLAGS_OTHER})

    set(platform_SOURCES
        platform/gtkmain.cpp
        platform/gloffscreen.cpp)

    set(platform_LIBRARIES
        ${GTKMM_LIBRARIES}
        ${JSONC_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        ${GLEW_LIBRARIES})
endif()

# solvespace executable

set(solvespace_HEADERS
    config.h
    dsc.h
    expr.h
    polygon.h
    sketch.h
    solvespace.h
    ui.h
    srf/surface.h)

set(solvespace_SOURCES
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
    export.cpp
    exportstep.cpp
    exportvector.cpp
    expr.cpp
    file.cpp
    generate.cpp
    glhelper.cpp
    graphicswin.cpp
    group.cpp
    groupmesh.cpp
    importdxf.cpp
    mesh.cpp
    modify.cpp
    mouse.cpp
    polygon.cpp
    resource.cpp
    request.cpp
    solvespace.cpp
    style.cpp
    system.cpp
    textscreens.cpp
    textwin.cpp
    toolbar.cpp
    ttf.cpp
    undoredo.cpp
    util.cpp
    view.cpp
    srf/boolean.cpp
    srf/curve.cpp
    srf/merge.cpp
    srf/ratpoly.cpp
    srf/raycast.cpp
    srf/surface.cpp
    srf/surfinter.cpp
    srf/triangulate.cpp)

add_executable(solvespace WIN32 MACOSX_BUNDLE
    ${libslvs_HEADERS}
    ${libslvs_SOURCES}
    ${util_SOURCES}
    ${platform_SOURCES}
    ${solvespace_HEADERS}
    ${solvespace_SOURCES}
    $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

add_dependencies(solvespace
    resources)

target_link_libraries(solvespace
    dxfrw
    ${OPENGL_LIBRARIES}
    ${PNG_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${platform_LIBRARIES})

if(WIN32 AND NOT MINGW)
    set_target_properties(solvespace PROPERTIES
        LINK_FLAGS "/MANIFEST:NO /SAFESEH:NO")
endif()

if(SPACEWARE_FOUND)
    if(APPLE)
        target_link_libraries(solvespace
            ${SPACEWARE}/Versions/A/3DconnexionClient)
    else()
        target_link_libraries(solvespace
            ${SPACEWARE_LIBRARIES})
    endif()
endif()

if(APPLE)
    foreach(lib ${platform_BUNDLED_LIBS})
        get_filename_component(name ${lib} NAME)
        set(target ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/MacOS/${name})

        execute_process(COMMAND otool -XD ${lib}
            OUTPUT_VARIABLE canonical_lib OUTPUT_STRIP_TRAILING_WHITESPACE)
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

# valgrind

add_custom_target(solvespace-valgrind
    valgrind
        --tool=memcheck
        --verbose
        --track-fds=yes
        --log-file=vg.%p.out
        --num-callers=50
        --error-limit=no
        --read-var-info=yes
        --leak-check=full
        --leak-resolution=high
        --show-reachable=yes
        --track-origins=yes
        --malloc-fill=0xac
        --free-fill=0xde
        $<TARGET_FILE:solvespace>)
