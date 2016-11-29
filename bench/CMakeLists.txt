# benchmark runner

foreach(pkg_config_lib CAIRO)
    include_directories(${${pkg_config_lib}_INCLUDE_DIRS})
    link_directories(${${pkg_config_lib}_LIBRARY_DIRS})
endforeach()

add_executable(solvespace-benchmark
    harness.cpp
    $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

target_link_libraries(solvespace-benchmark
    solvespace-core
    solvespace-headless)

add_dependencies(solvespace-benchmark
    resources)
