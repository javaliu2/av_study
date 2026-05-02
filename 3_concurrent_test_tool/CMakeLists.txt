cmake_minimum_required(VERSION 3.10.0)

project(media_server_concurrent_test_tool)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale libswresample)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_executable(learn_common ${SRC_DIR}/learn_common.cpp)

include_directories(${FFMPEG_INCLUDE_DIRS})
add_executable(main ${SRC_DIR}/main.cpp ${SRC_DIR}/FFmpegDecoder.cpp)
target_link_libraries(main ${FFMPEG_LIBRARIES})
