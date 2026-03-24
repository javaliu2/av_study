cmake_minimum_required(VERSION 3.10.0)
project(2_rtsp_server)

set(STUDY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/my_way)
add_executable(virtual_study ${STUDY_SRC}/virtual_test.cpp)
add_executable(map_study ${STUDY_SRC}/map_test.cpp)