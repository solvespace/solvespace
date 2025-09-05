add_executable(CDemo
    CDemo.c)

target_link_libraries(CDemo PRIVATE
    slvs)

if(EMSCRIPTEN)
    target_link_options(CDemo PRIVATE
        "SHELL:-s TOTAL_MEMORY=134217728")
    set_target_properties(CDemo PROPERTIES
        SUFFIX ".html")
endif()
