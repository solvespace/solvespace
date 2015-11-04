include_directories(
    ${PNG_INCLUDE_DIRS})

link_directories(
    ${PNG_LIBRARY_DIRS})

add_executable(png2c
    png2c.cpp)

target_link_libraries(png2c
    ${PNG_LIBRARIES})

add_executable(unifont2c
    unifont2c.cpp)

target_link_libraries(unifont2c
    ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
