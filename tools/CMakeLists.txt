include_directories(
    ${PNG_INCLUDE_DIRS})

link_directories(
    ${PNG_LIBRARY_DIRS})

if(WIN32)
    add_executable(ttf2c
        ttf2c.cpp)

    target_link_libraries(ttf2c
        comctl32)
endif()

add_executable(png2c
    png2c.cpp)

target_link_libraries(png2c
    ${PNG_LIBRARIES})
