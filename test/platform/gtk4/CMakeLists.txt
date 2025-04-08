# GTK4 UI tests
if(USE_GTK4)
    add_executable(test_gtk4_ui test.cpp)
    target_link_libraries(test_gtk4_ui
        solvespace-core
        ${GTKMM_LIBRARIES}
    )
    target_include_directories(test_gtk4_ui PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
        ${GTKMM_INCLUDE_DIRS}
    )
    target_compile_definitions(test_gtk4_ui PRIVATE
        USE_GTK4
    )
    
    add_test(NAME gtk4_ui COMMAND test_gtk4_ui)
endif()
