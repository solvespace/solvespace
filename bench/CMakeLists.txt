# benchmark runner

foreach(pkg_config_lib CAIRO)
    include_directories(${${pkg_config_lib}_INCLUDE_DIRS})
    link_directories(${${pkg_config_lib}_LIBRARY_DIRS})
endforeach()

add_executable(solvespace_benchmark
    harness.cpp
    $<TARGET_PROPERTY:resources,EXTRA_SOURCES>)

target_link_libraries(solvespace_benchmark
    solvespace_headless)

add_dependencies(solvespace_benchmark
    resources)
