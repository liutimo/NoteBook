cmake_minimum_required(VERSION 3.10)
project(AndroidNative)


add_subdirectory(Libraries)
add_subdirectory(Vold)

set(CMAKE_CXX_STANDARD 17)

add_executable(AndroidNative main.cpp)