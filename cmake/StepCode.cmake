# STEPcode is deliberately not part of slvs_deps. Only the experimental STEP
# reader and its generated schema libraries consume these private targets.
include(ExternalProject)

set(STEP_IMPORT_SCHEMAS "AP203;AP214;AP242" CACHE STRING
    "STEP proof bindings to generate (AP203, AP214, AP242; pinned editions)")

function(slvs_add_stepcode)
    if(CMAKE_CONFIGURATION_TYPES AND CMAKE_VERSION VERSION_LESS 3.20)
        message(FATAL_ERROR "The STEP proof needs CMake 3.20+ for multi-config generated outputs")
    endif()
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "The STEP dependency proof currently requires a native build. "
            "Disable ENABLE_STEP_IMPORT_PROOF for cross builds; see doc/STEP_IMPORT.md.")
    endif()
    if(NOT STEP_IMPORT_SCHEMAS)
        message(FATAL_ERROR "STEP_IMPORT_SCHEMAS must contain at least one binding")
    endif()
    set(schemas ${STEP_IMPORT_SCHEMAS})
    list(REMOVE_DUPLICATES schemas)
    foreach(schema IN LISTS schemas)
        if(NOT schema MATCHES "^(AP203|AP214|AP242)$")
            message(FATAL_ERROR "Unknown STEP binding '${schema}'; use AP203, AP214 or AP242")
        endif()
    endforeach()

    set(source "${PROJECT_SOURCE_DIR}/extlib/stepcode")
    set(binary "${CMAKE_CURRENT_BINARY_DIR}/runtime")
    if(NOT EXISTS "${source}/CMakeLists.txt")
        message(FATAL_ERROR "Initialize STEPcode with: git submodule update --init extlib/stepcode")
    endif()

    # A separate CMake invocation contains STEPcode's cache, flags, generated
    # config.h and top-level-project assumptions. No tools are downloaded, and
    # the cached lexer/parser sources avoid a re2c/Lemon/Perplex requirement.
    set(args
        "-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON"
        "-DCMAKE_MODULE_PATH:PATH=${PROJECT_SOURCE_DIR}/cmake/stepcode"
        "-DBUILD_SHARED_LIBS:BOOL=OFF"
        "-DBUILD_STATIC_LIBS:BOOL=ON"
        "-DSC_BUILD_SCHEMAS:STRING="
        "-DSC_PYTHON_GENERATOR:BOOL=OFF"
        "-DSC_ENABLE_TESTING:BOOL=OFF"
        "-DSC_GENERATE_LEXER_PARSER:STRING=OFF"
        "-DSC_IS_SUBBUILD:BOOL=ON")
    foreach(var CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS
                CMAKE_USER_MAKE_RULES_OVERRIDE CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
                CMAKE_MSVC_RUNTIME_LIBRARY CMAKE_MAKE_PROGRAM
                CMAKE_CONFIGURATION_TYPES
                CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_OSX_SYSROOT)
        if(DEFINED ${var})
            string(REPLACE ";" "|" value "${${var}}")
            list(APPEND args "-D${var}:STRING=${value}")
        endif()
    endforeach()
    foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        foreach(lang C CXX)
            list(APPEND args "-DCMAKE_${lang}_FLAGS_${config}:STRING=${CMAKE_${lang}_FLAGS_${config}}")
        endforeach()
    endforeach()

    # Match upstream's single-/multi-config output layout, including MSVC's
    # architecture and /MT[d] settings inherited above from SolveSpace.
    if(CMAKE_CONFIGURATION_TYPES)
        set(output "${binary}/$<CONFIG>")
    else()
        set(output "${binary}")
    endif()
    set(generator "${output}/bin/exp2cxx${CMAKE_EXECUTABLE_SUFFIX}")
    set(libraries)
    foreach(lib stepeditor stepcore stepdai steputils)
        set(location "${output}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${lib}-static${CMAKE_STATIC_LIBRARY_SUFFIX}")
        list(APPEND libraries "${location}")
        add_library(slvs-stepcode-${lib} STATIC IMPORTED)
        set_target_properties(slvs-stepcode-${lib} PROPERTIES IMPORTED_LOCATION "${location}")
        if(CMAKE_CONFIGURATION_TYPES)
            foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
                string(TOUPPER "${config}" upper)
                set_target_properties(slvs-stepcode-${lib} PROPERTIES
                    IMPORTED_LOCATION_${upper}
                    "${binary}/${config}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${lib}-static${CMAKE_STATIC_LIBRARY_SUFFIX}")
            endforeach()
        endif()
    endforeach()
    set_property(TARGET slvs-stepcode-stepeditor PROPERTY INTERFACE_LINK_LIBRARIES
        slvs-stepcode-stepcore slvs-stepcode-stepdai slvs-stepcode-steputils)
    set_property(TARGET slvs-stepcode-stepcore PROPERTY INTERFACE_LINK_LIBRARIES
        slvs-stepcode-stepdai slvs-stepcode-steputils)
    set_property(TARGET slvs-stepcode-stepdai PROPERTY INTERFACE_LINK_LIBRARIES
        slvs-stepcode-steputils)
    if(WIN32)
        set_property(TARGET slvs-stepcode-steputils PROPERTY INTERFACE_LINK_LIBRARIES shlwapi)
    endif()

    ExternalProject_Add(slvs-stepcode-build
        SOURCE_DIR "${source}"
        BINARY_DIR "${binary}"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND ""
        LIST_SEPARATOR "|"
        CMAKE_ARGS ${args}
        BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config $<CONFIG>
            --target exp2cxx stepeditor-static stepcore-static stepdai-static steputils-static
        BUILD_ALWAYS TRUE
        BUILD_BYPRODUCTS "${generator}" ${libraries}
        INSTALL_COMMAND "")
    foreach(lib stepeditor stepcore stepdai steputils)
        add_dependencies(slvs-stepcode-${lib} slvs-stepcode-build)
    endforeach()

    # Only consumers that explicitly link this target see vendor headers/macros.
    file(MAKE_DIRECTORY "${binary}/include")
    add_library(slvs-stepcode-runtime INTERFACE)
    target_include_directories(slvs-stepcode-runtime SYSTEM INTERFACE
        "${source}/include" "${binary}/include")
    target_compile_definitions(slvs-stepcode-runtime INTERFACE SC_STATIC)
    target_link_libraries(slvs-stepcode-runtime INTERFACE slvs-stepcode-stepeditor)

    set(schema_libraries)
    foreach(schema IN LISTS schemas)
        if(schema STREQUAL "AP203")
            set(express "ap203/ap203.exp")
            set(name "CONFIG_CONTROL_DESIGN")
        elseif(schema STREQUAL "AP214")
            set(express "ap214e3/AP214E3_2010.exp")
            set(name "AUTOMOTIVE_DESIGN")
        elseif(schema STREQUAL "AP242")
            set(express "ap242/242_mim_lf.exp")
            set(name "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF")
        endif()
        if(CMAKE_CONFIGURATION_TYPES)
            set(directory "${CMAKE_CURRENT_BINARY_DIR}/schemas/$<CONFIG>/${schema}")
        else()
            set(directory "${CMAKE_CURRENT_BINARY_DIR}/schemas/${schema}")
        endif()
        set(prefix "Sdai${name}")
        set(sources schema.cc SdaiAll.cc compstructs.cc "${prefix}.cc" "${prefix}.init.cc")
        set(outputs)
        foreach(file IN LISTS sources)
            list(APPEND outputs "${directory}/${file}")
        endforeach()
        add_custom_command(OUTPUT "${directory}/generated.stamp"
            BYPRODUCTS ${outputs}
                "${directory}/schema.h" "${directory}/Sdaiclasses.h"
                "${directory}/${prefix}.h" "${directory}/${prefix}Names.h"
                "${directory}/${prefix}.sources.cmake" "${directory}/.exp2cxx-manifest"
            COMMAND "${CMAKE_COMMAND}"
                "-DEXP2CXX=${generator}" "-DEXPRESS=${source}/data/${express}"
                "-DOUTPUT_DIR=${directory}" "-DPREFIX=${prefix}"
                -P "${PROJECT_SOURCE_DIR}/cmake/stepcode/GenerateSchema.cmake"
            COMMAND "${CMAKE_COMMAND}" -E touch "${directory}/generated.stamp"
            DEPENDS "${generator}" "${source}/data/${express}"
                "${PROJECT_SOURCE_DIR}/cmake/stepcode/GenerateSchema.cmake"
            COMMENT "Generating compact STEP ${schema} binding"
            VERBATIM)
        add_custom_target(slvs-stepcode-generate-${schema} DEPENDS "${directory}/generated.stamp")
        add_dependencies(slvs-stepcode-generate-${schema} slvs-stepcode-build)
        add_library(slvs-stepcode-${schema} STATIC ${outputs})
        add_dependencies(slvs-stepcode-${schema} slvs-stepcode-generate-${schema})
        target_include_directories(slvs-stepcode-${schema} PRIVATE "${directory}")
        target_link_libraries(slvs-stepcode-${schema} PRIVATE slvs-stepcode-runtime)
        # Four generic entry points remain global even in the compact backend.
        # Rename them per target, never in shared headers or the runtime itself.
        target_compile_definitions(slvs-stepcode-${schema} PRIVATE
            "SchemaInit=SlvsStep${schema}SchemaInit"
            "InitSchemasAndEnts=SlvsStep${schema}InitSchemasAndEnts"
            "gencomplex=SlvsStep${schema}GenComplex"
            "GetModelContents=SlvsStep${schema}GetModelContents")
        if(MSVC)
            target_compile_options(slvs-stepcode-${schema} PRIVATE /bigobj /w)
        else()
            target_compile_options(slvs-stepcode-${schema} PRIVATE -w)
        endif()
        list(APPEND schema_libraries slvs-stepcode-${schema})
    endforeach()
    set(STEP_IMPORT_SCHEMA_LIBRARIES ${schema_libraries} PARENT_SCOPE)
    set(STEP_IMPORT_ENABLED_SCHEMAS ${schemas} PARENT_SCOPE)
endfunction()
