# global

include_directories(
    ${OPENGL_INCLUDE_DIR}
    ${PNG_INCLUDE_DIRS})

link_directories(
    ${PNG_LIBRARY_DIRS})

add_definitions(
    ${PNG_CFLAGS_OTHER})

link_directories(
    ${PNG_LIBRARY_DIRS})

add_definitions(
    ${PNG_CFLAGS_OTHER})

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/built"
    "${CMAKE_CURRENT_BINARY_DIR}")

if(SPACEWARE_FOUND)
    include_directories(
        "${SPACEWARE_INCLUDE_DIR}")
endif()

set(HAVE_SPACEWARE ${SPACEWARE_FOUND})
set(HAVE_GTK ${GTKMM_FOUND})
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/config.h")

# platform utilities

if(WIN32)
    set(util_SOURCES
        win32/w32util.cpp)
else()
    set(util_SOURCES
        unix/unixutil.cpp)
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
    PUBLIC "${CMAKE_SOURCE_DIR}/include")

set_target_properties(slvs PROPERTIES
    PUBLIC_HEADER "${CMAKE_SOURCE_DIR}/include/slvs.h"
    VERSION ${solvespace_VERSION_MAJOR}.${solvespace_VERSION_MINOR}
    SOVERSION 1)

if(NOT WIN32)
    install(TARGETS slvs
        LIBRARY DESTINATION lib/${CMAKE_LIBRARY_PATH} # multiarch
        PUBLIC_HEADER DESTINATION include)
endif()

# generated files

# `$<TARGET_FILE:tool>` allows us to use binfmt support on Linux
# without special-casing anything; running `tool.exe` would succeed
# but unlike Windows, Linux does not have the machinery to map
# an invocation of `tool` to an executable `tool.exe` in $PATH.

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")

file(GLOB icons "${CMAKE_CURRENT_SOURCE_DIR}/icons/*.png")
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/icons.cpp"
           "${CMAKE_CURRENT_BINARY_DIR}/generated/icons.h"
    COMMAND $<TARGET_FILE:png2c>
            "${CMAKE_CURRENT_BINARY_DIR}/generated/icons.cpp"
            "${CMAKE_CURRENT_BINARY_DIR}/generated/icons.h"
            ${icons}
    DEPENDS png2c ${icons})

file(GLOB chars "${CMAKE_CURRENT_SOURCE_DIR}/fonts/private/*.png")
list(SORT chars)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/bitmapfont.table.h"
    COMMAND $<TARGET_FILE:unifont2c>
            "${CMAKE_CURRENT_BINARY_DIR}/generated/bitmapfont.table.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts/unifont-8.0.01.hex.gz"
            ${chars}
    DEPENDS unifont2c ${chars})

set(generated_HEADERS
    ${CMAKE_CURRENT_BINARY_DIR}/generated/bitmapfont.table.h
    ${CMAKE_CURRENT_BINARY_DIR}/generated/icons.h)

set(generated_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/generated/icons.cpp)

# platform dependencies

if(WIN32)
    set(platform_HEADERS
        win32/freeze.h)

    set(platform_SOURCES
        win32/freeze.cpp
        win32/w32main.cpp
        win32/resource.rc)

    set(platform_LIBRARIES
        comctl32)
elseif(APPLE)
    add_definitions(
        -fobjc-arc)

    set(platform_SOURCES
        cocoa/cocoamain.mm
        unix/gloffscreen.cpp)

    set(platform_XIBS
        cocoa/MainMenu.xib
        cocoa/SaveFormatAccessory.xib)

    set(platform_ICONS
        cocoa/AppIcon.iconset)

    set(platform_RESOURCES
        unix/solvespace-48x48.png)

    set(platform_BUNDLED_LIBS
        ${PNG_LIBRARIES})

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
        gtk/gtkmain.cpp
        unix/gloffscreen.cpp)

    set(platform_LIBRARIES
        ${GTKMM_LIBRARIES}
        ${JSONC_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        ${GLEW_LIBRARIES})
endif()

set(platform_BUNDLED_RESOURCES)

foreach(xib ${platform_XIBS})
    get_filename_component(nib ${xib} NAME_WE)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${xib})
    set(target ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/Resources/${nib}.nib)
    list(APPEND platform_BUNDLED_RESOURCES ${target})

    add_custom_command(
        OUTPUT ${target}
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/Resources
        COMMAND ibtool --errors --warnings --notices
            --output-format human-readable-text --compile
            ${target} ${source}
        COMMENT "Building Interface Builder file ${xib}"
        DEPENDS ${xib})
endforeach()

foreach(icon ${platform_ICONS})
    get_filename_component(name ${icon} NAME_WE)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${icon})
    set(target ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/Resources/${name}.icns)
    list(APPEND platform_BUNDLED_RESOURCES ${target})

    add_custom_command(
        OUTPUT ${target}
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/Resources
        COMMAND iconutil -c icns -o ${target} ${source}
        COMMENT "Building icon set ${icon}"
        DEPENDS ${source})
endforeach()

foreach(res ${platform_RESOURCES})
    get_filename_component(name ${res} NAME)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${res})
    set(target ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/Resources/${name})
    list(APPEND platform_BUNDLED_RESOURCES ${target})

    add_custom_command(
        OUTPUT ${target}
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/Resources
        COMMAND ${CMAKE_COMMAND} -E copy ${source} ${target}
        COMMENT "Copying resource file ${res}"
        DEPENDS ${res})
endforeach()

foreach(lib ${platform_BUNDLED_LIBS})
    get_filename_component(name ${lib} NAME)
    set(target ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/MacOS/${name})
    list(APPEND platform_BUNDLED_RESOURCES ${target})

    add_custom_command(
        OUTPUT ${target}
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/solvespace.app/Contents/Resources
        COMMAND ${CMAKE_COMMAND} -E copy ${lib} ${target}
        COMMENT "Bundling shared library ${lib}"
        DEPENDS ${lib})
endforeach()

# solvespace executable

set(solvespace_HEADERS
    config.h
    dsc.h
    expr.h
    font.table.h
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
    mesh.cpp
    modify.cpp
    mouse.cpp
    polygon.cpp
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
    ${platform_HEADERS}
    ${platform_SOURCES}
    ${platform_BUNDLED_RESOURCES}
    ${generated_SOURCES}
    ${generated_HEADERS}
    ${solvespace_HEADERS}
    ${solvespace_SOURCES})

target_link_libraries(solvespace
    "${OPENGL_LIBRARIES}"
    "${PNG_LIBRARIES}"
    "${platform_LIBRARIES}")

if(WIN32 AND NOT MINGW)
    set_target_properties(solvespace PROPERTIES
        LINK_FLAGS "/MANIFEST:NO /SAFESEH:NO")
endif()

if(SPACEWARE_FOUND)
    target_link_libraries(solvespace
        "${SPACEWARE_LIBRARIES}")
endif()

if(APPLE)
    set(fixups)
    foreach(lib ${platform_BUNDLED_LIBS})
        execute_process(COMMAND otool -XD ${lib}
            OUTPUT_VARIABLE canonical_lib OUTPUT_STRIP_TRAILING_WHITESPACE)
        add_custom_command(TARGET solvespace POST_BUILD
            COMMAND install_name_tool -change "${canonical_lib}" "@executable_path/${name}"
                    $<TARGET_FILE:solvespace>
            COMMENT "Fixing up rpath for dylib ${name}")
    endforeach()

    set(bundle solvespace)
    add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/${bundle}.dmg
        COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${bundle}.dmg
        COMMAND hdiutil create -srcfolder ${CMAKE_CURRENT_BINARY_DIR}/${bundle}.app
                ${CMAKE_BINARY_DIR}/${bundle}.dmg
        DEPENDS $<TARGET_FILE:${bundle}>
        COMMENT "Building ${bundle}.dmg")

    add_custom_target(${bundle}-dmg ALL
        DEPENDS ${CMAKE_BINARY_DIR}/${bundle}.dmg)
endif()

install(TARGETS solvespace
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .)

install(FILES unix/solvespace.desktop
        DESTINATION share/applications)
foreach(SIZE 16x16 24x24 32x32 48x48)
    install(FILES unix/solvespace-${SIZE}.png
            DESTINATION share/icons/hicolor/${SIZE}/apps
            RENAME solvespace.png)
    install(FILES unix/solvespace-${SIZE}.png
            DESTINATION share/icons/hicolor/${SIZE}/mimetypes
            RENAME application.x-solvespace.png)
endforeach()

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
