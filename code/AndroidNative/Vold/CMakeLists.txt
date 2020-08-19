cmake_minimum_required(VERSION 3.10)
project(Vold)
set(CMAKE_CXX_STANDARD 17)

link_directories(../cmake-build-debug/Libraries)
include_directories(../Libraries/)
file(GLOB all_source  *.cpp)
add_executable(Vold ${all_source})
target_link_libraries(Vold my_lib)