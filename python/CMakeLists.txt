# cmake_minimum_required(VERSION 3.15...3.25)

# project(${SKBUILD_PROJECT_NAME} LANGUAGES CXX VERSION ${SKBUILD_PROJECT_VERSION})

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/pybind11)

find_package(Eigen3 REQUIRED)
if(Eigen3_FOUND)
  include_directories(${EIGEN_INCLUDE_DIR})
endif()

find_package(mimalloc CONFIG REQUIRED)
if(mimalloc_FOUND)
  include_directories(${MIMALLOC_INCLUDE_DIR})
endif()

# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

pybind11_add_module(
    slvs
MODULE
    ${CMAKE_SOURCE_DIR}/src/util.cpp
    ${CMAKE_SOURCE_DIR}/src/entity.cpp
    ${CMAKE_SOURCE_DIR}/src/expr.cpp
    ${CMAKE_SOURCE_DIR}/src/constrainteq.cpp
    ${CMAKE_SOURCE_DIR}/src/system.cpp
    ${CMAKE_SOURCE_DIR}/src/groupbase.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/platform.cpp
    ${CMAKE_SOURCE_DIR}/src/solversystem.cpp
    ${CMAKE_SOURCE_DIR}/src/sketch.cpp
    ${CMAKE_SOURCE_DIR}/python/bindings.cpp
)

configure_file(${CMAKE_SOURCE_DIR}/src/config.h.in
               ${CMAKE_BINARY_DIR}/src/config.h)

target_include_directories(slvs PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src ${EIGEN3_INCLUDE_DIRS} ${MIMALLOC_INCLUDE_DIR})

target_compile_definitions(slvs PRIVATE VERSION_INFO=${PROJECT_VERSION} LIBRARY)

target_link_libraries(slvs PRIVATE mimalloc)

install(TARGETS slvs DESTINATION ${SKBUILD_PROJECT_NAME})
