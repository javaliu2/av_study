cmake_minimum_required(VERSION 3.10.0)
project(2_rtsp_server)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(STUDY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/my_way)
add_executable(virtual_study ${STUDY_SRC}/virtual_test.cpp)
add_executable(map_study ${STUDY_SRC}/map_test.cpp)
add_executable(vector_test ${STUDY_SRC}/vector_test.cpp)
add_executable(file_test ${STUDY_SRC}/file_test.cpp)

# 递归搜索所有头文件
file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_SOURCE_DIR}/include/*.h
)

#递归搜索所有源文件
file(GLOB_RECURSE SRC_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# 添加头文件搜索路径
include_directories(${CMAKE_SOURCE_DIR}/include)