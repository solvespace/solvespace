include_directories(
    ${CMAKE_SOURCE_DIR}/include)

add_executable(CDemo
    CDemo.c)

target_link_libraries(CDemo
    slvs)
