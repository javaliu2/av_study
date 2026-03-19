cmake_minimum_required(VERSION 3.10.0)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_executable(main_h264 ${SRC_DIR}/main_h264.cpp ${SRC_DIR}/rtp.cpp ${SRC_DIR}/rtsp_base.cpp)
target_link_libraries(main_h264 PRIVATE ws2_32)  # 链接ws2_32库

add_executable(main_aac ${SRC_DIR}/main_aac.cpp ${SRC_DIR}/rtp.cpp ${SRC_DIR}/rtsp_base.cpp)
target_link_libraries(main_aac PRIVATE ws2_32)