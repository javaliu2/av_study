cmake_minimum_required(VERSION 3.10.0)

project(media_server_concurrent_test_tool)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB_RECURSE SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(main ${SRC_DIR})