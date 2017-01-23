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

if(APPLE)
    set(util_LIBRARIES
        ${APPKIT_LIBRARY})
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

target_link_libraries(slvs
    ${util_LIBRARIES})

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

if(OPENGL STREQUAL 2)
    set(gl_SOURCES
        render/gl2shader.cpp
        render/rendergl2.cpp)
elseif(OPENGL STREQUAL 1)
    set(gl_SOURCES
        render/rendergl1.cpp)
else()
    message(FATAL_ERROR "Unsupported OpenGL version ${OPENGL}")
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
else()
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

set(every_platform_SOURCES
    platform/w32main.cpp
    platform/gtkmain.cpp
    platform/cocoamain.mm)

# solvespace library

set(solvespace_core_HEADERS
    dsc.h
    expr.h
    polygon.h
    sketch.h
    solvespace.h
    ui.h
    render/render.h
    render/gl2shader.h
    srf/surface.h)

set(solvespace_core_SOURCES
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

set(solvespace_core_gl_SOURCES
    solvespace.cpp)

add_library(solvespace-core STATIC
    ${util_SOURCES}
    ${solvespace_core_HEADERS}
    ${solvespace_core_SOURCES})

target_link_libraries(solvespace-core
    dxfrw
    ${util_LIBRARIES}
    ${ZLIB_LIBRARY}
    ${PNG_LIBRARY}
    ${FREETYPE_LIBRARY}
    ${Backtrace_LIBRARIES})

target_compile_options(solvespace-core
    PRIVATE ${COVERAGE_FLAGS})

# solvespace translations

if(HAVE_GETTEXT)
    set(inputs
        ${solvespace_core_SOURCES}
        ${solvespace_core_HEADERS}
        ${every_platform_SOURCES})
    set(output_pot ${CMAKE_CURRENT_SOURCE_DIR}/../res/messages.pot)
    set(templ_po   ${CMAKE_CURRENT_BINARY_DIR}/messages.po)
    set(output_po  ${CMAKE_CURRENT_SOURCE_DIR}/../res/locales/en_US.po)

    file(GLOB locale_pos ${CMAKE_CURRENT_SOURCE_DIR}/../res/locales/*.po)

    add_custom_command(
        OUTPUT  ${output_pot}
        COMMAND ${XGETTEXT}
                --language=C++
                --keyword --keyword=_ --keyword=N_ --keyword=C_:2,1c --keyword=CN_:2,1c
                --force-po --width=100 --sort-by-file
                --package-name=SolveSpace
                --package-version=${solvespace_VERSION_MAJOR}.${solvespace_VERSION_MINOR}
                "--copyright-holder=the PACKAGE authors"
                --msgid-bugs-address=whitequark@whitequark.org
                --from-code=utf-8 --output=${output_pot} ${inputs}
        DEPENDS ${inputs}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Extracting translations"
        VERBATIM)

    # en_US is a bit special; we pre-fill the msgstrs from msgids, instead of (as would normally
    # happen) leaving them empty.
    add_custom_command(
        OUTPUT  ${output_po}
        COMMAND ${MSGINIT}
                --locale=en_US --no-translator
                --output=${templ_po} --input=${output_pot}
        COMMAND ${MSGMERGE}
                --force-po --no-fuzzy-matching
                --output=${output_po} ${output_po} ${templ_po}
        DEPENDS ${output_pot}
        COMMENT "Updating en_US translations"
        VERBATIM)

    foreach(locale_po ${locale_pos})
        get_filename_component(locale_name ${locale_po} NAME_WE)
        if(locale_name STREQUAL "en_US")
            continue()
        endif()

        add_custom_command(
            OUTPUT  ${locale_po}
            COMMAND ${MSGMERGE}
                    --update --no-fuzzy-matching
                    ${locale_po} ${output_pot}
            DEPENDS ${output_pot}
            COMMENT "Updating ${locale_name} translations"
            VERBATIM)
    endforeach()

    add_custom_target(solvespace-translations
        DEPENDS ${output_pot} ${output_po} ${locale_pos})
endif()

# solvespace graphical executable

if(ENABLE_GUI)
    add_executable(solvespace WIN32 MACOSX_BUNDLE
        ${solvespace_core_gl_SOURCES}
        ${platform_SOURCES}
        $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

    add_dependencies(solvespace
        resources)

    target_link_libraries(solvespace
        solvespace-core
        ${OPENGL_LIBRARIES}
        ${platform_LIBRARIES}
        ${COVERAGE_LIBRARY})

    if(MSVC)
        set_target_properties(solvespace PROPERTIES
            LINK_FLAGS "/MANIFEST:NO /SAFESEH:NO /INCREMENTAL:NO /OPT:REF")
    endif()
endif()

# solvespace headless library

set(headless_SOURCES
    platform/headless.cpp
    render/rendercairo.cpp)

add_library(solvespace-headless STATIC EXCLUDE_FROM_ALL
    ${solvespace_core_gl_SOURCES}
    ${headless_SOURCES})

target_compile_definitions(solvespace-headless
    PRIVATE -DHEADLESS)

target_include_directories(solvespace-headless
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(solvespace-headless
    solvespace-core
    ${CAIRO_LIBRARIES})

target_compile_options(solvespace-headless
    PRIVATE ${COVERAGE_FLAGS})

# solvespace command-line executable

add_executable(solvespace-cli
    platform/climain.cpp
    $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

target_link_libraries(solvespace-cli
    solvespace-core
    solvespace-headless)

add_dependencies(solvespace-cli
    resources)

if(MSVC)
    set_target_properties(solvespace-cli PROPERTIES
        LINK_FLAGS "/INCREMENTAL:NO /OPT:REF")
endif()

# solvespace unix package

if(NOT (WIN32 OR APPLE))
    if(ENABLE_GUI)
        install(TARGETS solvespace
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(TARGETS solvespace-cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# solvespace macOS package

if(APPLE)
    set(bundle     solvespace)
    set(bundle_bin ${EXECUTABLE_OUTPUT_PATH}/${bundle}.app/Contents/MacOS)

    add_custom_command(TARGET solvespace POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:solvespace-cli> ${bundle_bin}
        COMMENT "Bundling executable solvespace-cli"
        VERBATIM)

    foreach(lib ${platform_BUNDLED_LIBS})
        get_filename_component(name ${lib} NAME)

        execute_process(COMMAND otool -D ${lib}
            OUTPUT_VARIABLE canonical_lib OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX REPLACE "^.+:\n" "" canonical_lib ${canonical_lib})
        add_custom_command(TARGET ${bundle} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${lib} ${bundle_bin}/${name}
            COMMAND install_name_tool -change ${canonical_lib} @executable_path/${name}
                    $<TARGET_FILE:${bundle}>
            COMMAND install_name_tool -change ${canonical_lib} @executable_path/${name}
                    $<TARGET_FILE:solvespace-cli>
            COMMENT "Bundling shared library ${lib}"
            VERBATIM)
    endforeach()

    add_custom_command(OUTPUT ${EXECUTABLE_OUTPUT_PATH}/${bundle}.dmg
        COMMAND ${CMAKE_COMMAND} -E remove ${EXECUTABLE_OUTPUT_PATH}/${bundle}.dmg
        COMMAND hdiutil create -srcfolder ${EXECUTABLE_OUTPUT_PATH}/${bundle}.app
                ${EXECUTABLE_OUTPUT_PATH}/${bundle}.dmg
        DEPENDS $<TARGET_FILE:${bundle}>
        COMMENT "Building ${bundle}.dmg"
        VERBATIM)
    add_custom_target(${bundle}-dmg ALL
        DEPENDS ${EXECUTABLE_OUTPUT_PATH}/${bundle}.dmg)
endif()
