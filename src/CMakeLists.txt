include(GNUInstallDirs)

# configuration

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})

set(HAVE_SPACEWARE ${SPACEWARE_FOUND})

if(NOT WIN32 OR APPLE)
    if(GTKMM_gtkmm-3.0_VERSION VERSION_LESS "3.24.0")
        set(HAVE_GTK_FILECHOOSERNATIVE 0)
    else()
        set(HAVE_GTK_FILECHOOSERNATIVE 1)
    endif()
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

if(USE_QT_GUI)
	#Including QT
	set(CMAKE_AUTOMOC ON)
	find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL  REQUIRED)
	include (../extlib/tinyxml2/CMakeLists.txt)

	#including code from PoCAE
	set(GitPath ${CMAKE_CURRENT_LIST_DIR}/../..)
endif()

# solvespace dependencies
add_library(slvs_deps INTERFACE)
target_include_directories(slvs_deps INTERFACE SYSTEM
    ${OPENGL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIR}
    ${PNG_PNG_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${MIMALLOC_INCLUDE_DIR}
    ${EIGEN3_INCLUDE_DIRS})
target_link_libraries(slvs_deps INTERFACE
    dxfrw
    ${ZLIB_LIBRARY}
    ${PNG_LIBRARY}
    ${FREETYPE_LIBRARY}
    #${CAIRO_LIBRARIES}
    mimalloc-static)

if(Backtrace_FOUND)
    target_include_directories(slvs_deps INTERFACE SYSTEM
        ${Backtrace_INCLUDE_DIRS})
    target_link_libraries(slvs_deps INTERFACE
        ${Backtrace_LIBRARY})
endif()

if(SPACEWARE_FOUND)
    target_include_directories(slvs_deps INTERFACE SYSTEM
        ${SPACEWARE_INCLUDE_DIR})
    target_link_libraries(slvs_deps INTERFACE
        ${SPACEWARE_LIBRARIES})
endif()

if(ENABLE_OPENMP)
    target_link_libraries(slvs_deps INTERFACE slvs_openmp)
endif()

target_compile_options(slvs_deps
    INTERFACE ${COVERAGE_FLAGS})

# platform utilities

if(APPLE)
    target_link_libraries(slvs_deps INTERFACE
        ${APPKIT_LIBRARY})
endif()

# libslvs
add_library(slvs SHARED
    solvespace.h
    platform/platform.h
    util.cpp
    entity.cpp
    expr.cpp
    constraint.cpp
    constrainteq.cpp
    system.cpp
    platform/platform.cpp
    lib.cpp)

target_compile_definitions(slvs
    PRIVATE -DLIBRARY)

target_include_directories(slvs
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${EIGEN3_INCLUDE_DIRS})

target_link_libraries(slvs PRIVATE slvs_deps)

set_target_properties(slvs PROPERTIES
    PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/include/slvs.h
    VERSION ${PROJECT_VERSION}
    SOVERSION 1)

if(NOT WIN32)
    install(TARGETS slvs
        LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

set(every_platform_SOURCES
    platform/guiwin.cpp
    platform/guigtk.cpp
    platform/guimac.mm
    platform/guihtml.cpp)

# solvespace library

set(solvespace_core_gl_SOURCES
    solvespace.cpp)

if (NOT USE_QT_GUI)
	add_library(solvespace-core STATIC
		dsc.h
		expr.h
		polygon.h
		sketch.h
		solvespace.h
		ui.h
		platform/platform.h
		render/render.h
	   render/gl3shader.h
	   render/render2d.cpp
		srf/surface.h
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
		importidf.cpp
		importmesh.cpp
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
		platform/platform.cpp
		platform/gui.cpp
		render/render.cpp
		srf/boolean.cpp
		srf/curve.cpp
		srf/merge.cpp
		srf/ratpoly.cpp
		srf/raycast.cpp
		srf/shell.cpp
		srf/surface.cpp
		srf/surfinter.cpp
		srf/triangulate.cpp)
else()
	add_library(solvespace-core STATIC
		dsc.h
		expr.h
		polygon.h
		sketch.h
		solvespace.h
		ui.h
		platform/platform.h
		render/render.h
		srf/surface.h
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
		importidf.cpp
		importmesh.cpp
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
		platform/platform.cpp
		platform/gui.cpp
		render/render.cpp
		srf/boolean.cpp
		srf/curve.cpp
		srf/merge.cpp
		srf/ratpoly.cpp
		srf/raycast.cpp
		srf/shell.cpp
		srf/surface.cpp
		srf/surfinter.cpp
		srf/triangulate.cpp)
endif()

target_link_libraries(solvespace-core PUBLIC slvs_deps)

# solvespace translations

if(HAVE_GETTEXT)
    get_target_property(solvespace_core_SOURCES solvespace-core SOURCES)
    set(inputs
        ${solvespace_core_SOURCES}
        ${every_platform_SOURCES}
        ${solvespace_core_gl_SOURCES})

    set(templ_po   ${CMAKE_CURRENT_BINARY_DIR}/../res/messages.po)

    set(output_pot ${CMAKE_CURRENT_SOURCE_DIR}/../res/messages.pot)
    set(output_po  ${CMAKE_CURRENT_SOURCE_DIR}/../res/locales/en_US.po)
    file(GLOB locale_pos ${CMAKE_CURRENT_SOURCE_DIR}/../res/locales/*.po)

    string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
           gen_output_pot ${output_pot}.gen)
    string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
           gen_output_po ${output_po}.gen)
    foreach(locale_po ${locale_pos})
        string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
               gen_locale_po ${locale_po}.gen)
        list(APPEND gen_locale_pos ${gen_locale_po})
    endforeach()

    add_custom_command(
        OUTPUT  ${gen_output_pot}
        COMMAND ${XGETTEXT}
                --language=C++
                --keyword --keyword=_ --keyword=N_ --keyword=C_:2,1c --keyword=CN_:2,1c
                --force-po --width=100 --sort-by-file
                --package-name=SolveSpace
                --package-version=${PROJECT_VERSION}
                "--copyright-holder=the PACKAGE authors"
                --msgid-bugs-address=phkahler@gmail.com
                --from-code=utf-8 --output=${gen_output_pot} ${inputs}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${gen_output_pot} ${output_pot}
        DEPENDS ${inputs}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Extracting translations"
        VERBATIM)

    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../res/locales)

    # en_US is a bit special; we pre-fill the msgstrs from msgids, instead of (as would normally
    # happen) leaving them empty.
    add_custom_command(
        OUTPUT  ${gen_output_po}
        COMMAND ${MSGINIT}
                --locale=en_US --no-translator
                --output=${templ_po} --input=${gen_output_pot}
        COMMAND ${MSGMERGE}
                --force-po --no-fuzzy-matching
                --output=${gen_output_po} ${output_po} ${templ_po}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${gen_output_po} ${output_po}
        DEPENDS ${gen_output_pot}
        COMMENT "Updating en_US translations"
        VERBATIM)

    foreach(locale_po ${locale_pos})
        string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}
               gen_locale_po ${locale_po}.gen)

        get_filename_component(locale_name ${locale_po} NAME_WE)
        if(locale_name STREQUAL "en_US")
            continue()
        endif()

        add_custom_command(
            OUTPUT  ${gen_locale_po}
            COMMAND ${MSGMERGE}
                    --no-fuzzy-matching
                    --output=${gen_locale_po} ${locale_po} ${gen_output_pot}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${gen_locale_po} ${locale_po}
            DEPENDS ${gen_output_pot}
            COMMENT "Updating ${locale_name} translations"
            VERBATIM)
    endforeach()

    add_custom_target(translate_solvespace
        DEPENDS ${gen_output_pot} ${gen_output_po} ${gen_locale_pos})
endif()

# solvespace graphical executable

if(ENABLE_GUI)
    if(NOT USE_QT_GUI)
	    add_executable(solvespace WIN32 MACOSX_BUNDLE
		${solvespace_core_gl_SOURCES}
		platform/entrygui.cpp
		$<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

	    add_dependencies(solvespace
		resources)

	    target_link_libraries(solvespace
		PRIVATE
		solvespace-core
		${OPENGL_LIBRARIES})

	    # OpenGL version
	    if(OPENGL STREQUAL 3)
		target_sources(solvespace PRIVATE
		    render/gl3shader.cpp
		    render/rendergl3.cpp)
	    elseif(OPENGL STREQUAL 1)
		target_sources(solvespace PRIVATE
		    render/rendergl1.cpp)
	    else()
		message(FATAL_ERROR "Unsupported OpenGL version ${OPENGL}")
	    endif()

	    # Platform-specific
	    if(WIN32)
		target_sources(solvespace PRIVATE
		    platform/guiwin.cpp)

		target_link_libraries(solvespace PRIVATE comctl32)
	    elseif(APPLE)
		target_compile_options(solvespace PRIVATE -fobjc-arc)
		target_compile_definitions(solvespace PRIVATE GL_SILENCE_DEPRECATION)

		target_sources(solvespace PRIVATE
		    platform/guimac.mm)
		set_target_properties(solvespace PROPERTIES
		    OUTPUT_NAME SolveSpace
		    XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME "YES"
		    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.solvespace"
		    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
	    elseif(EMSCRIPTEN)
		set(SHELL ${CMAKE_CURRENT_SOURCE_DIR}/platform/html/emshell.html)
		set(LINK_FLAGS
		    --bind --shell-file ${SHELL}
		    --no-heap-copy -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s ASYNCIFY=1
		    -s DYNCALLS=1 -s ASSERTIONS=1
		    -s TOTAL_STACK=33554432 -s TOTAL_MEMORY=134217728)

		get_target_property(resource_names resources NAMES)
		foreach(resource ${resource_names})
		    list(APPEND LINK_FLAGS --preload-file ${resource})
		endforeach()

		if(CMAKE_BUILD_TYPE STREQUAL Debug)
		    list(APPEND LINK_FLAGS
			--emrun --emit-symbol-map
			-s DEMANGLE_SUPPORT=1
			-s SAFE_HEAP=1)
		endif()

		target_sources(solvespace PRIVATE
		    platform/guihtml.cpp)

		string(REPLACE ";" " " LINK_FLAGS "${LINK_FLAGS}")
		set_target_properties(solvespace PROPERTIES
		    LINK_FLAGS "${LINK_FLAGS}")
		set_source_files_properties(platform/guihtml.cpp PROPERTIES
		    OBJECT_DEPENDS ${SHELL})

		add_custom_command(
		    TARGET solvespace POST_BUILD
		    COMMAND ${CMAKE_COMMAND} -E copy_if_different
			    ${CMAKE_CURRENT_SOURCE_DIR}/platform/html/solvespaceui.css
			    ${EXECUTABLE_OUTPUT_PATH}/solvespaceui.css
		    COMMENT "Copying UI stylesheet"
		    VERBATIM)
		add_custom_command(
		    TARGET solvespace POST_BUILD
		    COMMAND ${CMAKE_COMMAND} -E copy_if_different
			    ${CMAKE_CURRENT_SOURCE_DIR}/platform/html/solvespaceui.js
			    ${EXECUTABLE_OUTPUT_PATH}/solvespaceui.js
		    COMMENT "Copying UI script solvespaceui.js"
		    VERBATIM)
		add_custom_command(
		    TARGET solvespace POST_BUILD
		    COMMAND ${CMAKE_COMMAND} -E copy_if_different
			    ${CMAKE_CURRENT_SOURCE_DIR}/platform/html/filemanagerui.js
			    ${EXECUTABLE_OUTPUT_PATH}/filemanagerui.js
		    COMMENT "Copying UI script filemanagerui.sj"
		    VERBATIM)
	    else()
		target_sources(solvespace PRIVATE
		    platform/guigtk.cpp)

		target_include_directories(solvespace PRIVATE SYSTEM
		    ${GTKMM_INCLUDE_DIRS}
		    ${JSONC_INCLUDE_DIRS}
		    ${FONTCONFIG_INCLUDE_DIRS})
		target_link_directories(solvespace PRIVATE
		    ${GTKMM_LIBRARY_DIRS}
		    ${JSONC_LIBRARY_DIRS}
		    ${FONTCONFIG_LIBRARY_DIRS})
		target_link_libraries(solvespace PRIVATE
		    ${GTKMM_LIBRARIES}
		    ${JSONC_LIBRARIES}
		    ${FONTCONFIG_LIBRARIES})
	    endif()

	    if(MSVC)
		set_target_properties(solvespace PROPERTIES
		    LINK_FLAGS "/MANIFEST:NO /SAFESEH:NO /INCREMENTAL:NO /OPT:REF")
	    endif()
	endif()
	if(USE_QT_GUI)
	    add_executable(solvespaceQt WIN32 MACOSX_BUNDLE
		${solvespace_core_gl_SOURCES}
		platform/entrygui.cpp
		$<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

	    add_dependencies(solvespaceQt
		resources)

	    target_link_libraries(solvespaceQt
		PRIVATE
		solvespace-core
		${OPENGL_LIBRARIES})

	    # OpenGL version
	    if(OPENGL STREQUAL 3)
		target_sources(solvespaceQt PRIVATE
		    render/gl3shader.cpp
		    render/rendergl3.cpp)
	    elseif(OPENGL STREQUAL 1)
		target_sources(solvespaceQt PRIVATE
		    render/rendergl1.cpp)
	    else()
		message(FATAL_ERROR "Unsupported OpenGL version ${OPENGL}")
	    endif()

	    # Platform-specific
		target_sources(solvespaceQt PRIVATE
		    #platform/guiwin.cpp
		    platform/guiqt.cpp
		    platform/qglmainwindow.h
		    ${TinyXmlIncludes}
		    )

		target_link_libraries(solvespaceQt PRIVATE #comctl32
		    Qt5::Core
		    Qt5::Gui
		    Qt5::Widgets
		    Qt5::OpenGL
		    )
		set(QT_DEBUG_ENV "${_qt5_install_prefix}/../../bin\\\;")
		set(QT_RELEASE_ENV "${_qt5_install_prefix}/../../bin\\\;")
		set(QT_PLUGIN_PATH "${_qt5_install_prefix}/../../plugins\\\;")
		GenUserProp(solvespaceQt ${QT_DEBUG_ENV} ${QT_RELEASE_ENV} ${QT_PLUGIN_PATH})
	    if(MSVC)
		set_target_properties(solvespaceQt PROPERTIES
		    LINK_FLAGS "/MANIFEST:NO /SAFESEH:NO /INCREMENTAL:NO /OPT:REF")
	    endif()
	endif()
endif()

# solvespace headless library

if(NOT USE_QT_GUI)
	add_library(solvespace-headless STATIC EXCLUDE_FROM_ALL
		${solvespace_core_gl_SOURCES}
		platform/guinone.cpp
		render/rendercairo.cpp
		)
else()
	add_library(solvespace-headless STATIC EXCLUDE_FROM_ALL
		${solvespace_core_gl_SOURCES}
		)
endif()



target_compile_definitions(solvespace-headless
    PRIVATE HEADLESS)

target_include_directories(solvespace-headless
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${EIGEN3_INCLUDE_DIRS})

target_link_libraries(solvespace-headless
    PRIVATE
    solvespace-core)

# solvespace command-line executable

if(ENABLE_CLI)
    add_executable(solvespace-cli
        platform/entrycli.cpp
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
endif()

# solvespace unix package

if(NOT (WIN32 OR APPLE OR EMSCRIPTEN))
    if(NOT USE_QT_GUI)
        if(ENABLE_GUI)
            install(TARGETS solvespace
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()
    else()
        if(ENABLE_GUI)
            install(TARGETS solvespaceQt
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        endif()
    endif()

    if(ENABLE_CLI)
        install(TARGETS solvespace-cli
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

# solvespace macOS package

if(APPLE)
    set(LIBOMP_LIB_PATH ${OpenMP_CXX_INCLUDE_DIRS}/../lib/libomp.dylib)
    set(LIBOMP_LINK_PATH "@executable_path/../Resources/libomp.dylib")
    set(LIBOMP_LINK_PATH_UTILS "@executable_path/SolveSpace.app/Contents/Resources/libomp.dylib")
    if(ENABLE_GUI)
        add_custom_command(TARGET solvespace POST_BUILD
            COMMAND cp -r ${CMAKE_BINARY_DIR}/Resources $<TARGET_BUNDLE_CONTENT_DIR:solvespace>
        )
        if(ENABLE_OPENMP)
            execute_process(COMMAND install_name_tool -id ${LIBOMP_LINK_PATH} ${LIBOMP_LIB_PATH})
            message("FROM " ${${LIBOMP_LIB_PATH}} "TO" $<TARGET_BUNDLE_CONTENT_DIR:solvespace>/Resources/libomp.dylib)
            add_custom_command(TARGET solvespace POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${LIBOMP_LIB_PATH} $<TARGET_BUNDLE_CONTENT_DIR:solvespace>/Resources/libomp.dylib
                COMMAND install_name_tool -change ${LIBOMP_LINK_PATH} ${LIBOMP_LINK_PATH_UTILS} $<TARGET_FILE:solvespace-debugtool>
            )
        endif()
    endif()
    if(ENABLE_TESTS AND ENABLE_OPENMP)
        add_custom_command(TARGET solvespace POST_BUILD
            COMMAND install_name_tool -change ${LIBOMP_LINK_PATH} ${LIBOMP_LINK_PATH_UTILS} $<TARGET_FILE:solvespace-testsuite>)
    endif()
    if(ENABLE_CLI)
        add_custom_command(TARGET solvespace POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:solvespace-cli> $<TARGET_FILE_DIR:solvespace>
            COMMENT "Bundling executable solvespace-cli"
            VERBATIM)
    endif()
endif()
