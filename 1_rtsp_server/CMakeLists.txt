cmake_minimum_required(VERSION 3.10.0)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_executable(main ${SRC_DIR}/main.cpp ${SRC_DIR}/rtp.cpp)
target_link_libraries(main PRIVATE ws2_32)  # 链接ws2_32库