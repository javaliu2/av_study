cmake_minimum_required(VERSION 3.16)
project(hello_ffmpeg)

find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED
    libavformat libavcodec libavutil libswscale libswresample)

pkg_check_modules(SDL2 REQUIRED sdl2)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include())
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)
# for test.cpp
# list(REMOVE_ITEM SRC
#     ${CMAKE_CURRENT_SOURCE_DIR}/src/vPlayer_sdl2.cpp
# )
# end
add_executable(player ${SRC})

target_include_directories(player PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(player
    ${FFMPEG_LIBRARIES}
    ${SDL2_LIBRARIES}
)
