cmake_minimum_required(VERSION 3.16)
project(hello_ffmpeg)

find_package(PkgConfig REQUIRED)

pkg_check_modules(FFMPEG REQUIRED
    libavformat libavcodec libavutil libswscale libswresample)

pkg_check_modules(SDL2 REQUIRED sdl2)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC)  # 将src目录下的所有cpp文件置于SRC变量里
set(SRC_SIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
# for test.cpp
# list(REMOVE_ITEM SRC
#     ${CMAKE_CURRENT_SOURCE_DIR}/src/vPlayer_sdl2.cpp
# )
# end
# add_executable(player ${SRC})
add_library(ffmpeg_common INTERFACE)
target_include_directories(ffmpeg_common INTERFACE
    ${FFMPEG_INCLUDE_DIRS}
)
target_link_libraries(ffmpeg_common INTERFACE
    ${FFMPEG_LIBRARIES}
)
add_executable(decode_video_test 
    ${SRC_SIR}/player_entry.cpp 
    ${SRC_SIR}/vPlayer_sdl2.cpp 
    ${SRC_SIR}/output_log.cpp
)

add_executable(test ${SRC_SIR}/test.cpp)

add_executable(encode_video_test 
    ${SRC_SIR}/encode_video_test.cpp 
    ${SRC_SIR}/encode_yuv_to_h264.cpp 
    ${SRC_SIR}/output_log.cpp
)

target_include_directories(decode_video_test PRIVATE
    # ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(decode_video_test
    ffmpeg_common
    ${SDL2_LIBRARIES}
)
target_link_libraries(encode_video_test 
    ffmpeg_common
)

message(STATUS "SDL2_LIBRARIES = ${SDL2_LIBRARIES}")
