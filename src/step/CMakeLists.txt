include(StepCode)
slvs_add_stepcode()

add_library(solvespace-step-reader STATIC stepreader.cpp)
target_include_directories(solvespace-step-reader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(solvespace-step-reader PRIVATE
    ${STEP_IMPORT_SCHEMA_LIBRARIES} slvs-stepcode-runtime)
foreach(schema IN LISTS STEP_IMPORT_ENABLED_SCHEMAS)
    target_compile_definitions(solvespace-step-reader PRIVATE "SLVS_STEP_${schema}")
endforeach()

add_executable(solvespace-step-probe probe.cpp)
target_link_libraries(solvespace-step-probe PRIVATE solvespace-step-reader)
