cmake_minimum_required(VERSION 3.10.0)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
set(COMMON_SRC
    ${SRC_DIR}/rtp.cpp
    ${SRC_DIR}/connection.cpp
)
add_library(common STATIC ${COMMON_SRC})

add_executable(main_h264 ${SRC_DIR}/main_h264.cpp ${SRC_DIR}/rtsp_h264.cpp ${SRC_DIR}/send_h264.cpp)
target_link_libraries(main_h264 PRIVATE common ws2_32)  # 链接ws2_32库

add_executable(main_aac ${SRC_DIR}/main_aac.cpp ${SRC_DIR}/rtsp_aac.cpp ${SRC_DIR}/send_aac.cpp)
target_link_libraries(main_aac PRIVATE common ws2_32)

add_executable(main_av ${SRC_DIR}/main_av.cpp ${SRC_DIR}/rtsp_av.cpp)
target_link_libraries(main_av PRIVATE common ws2_32)