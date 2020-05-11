cmake_minimum_required(VERSION 3.5)
project(Demo)

set(CMAKE_CXX_STANDARD 14)

add_executable(Demo main.cpp RefBase.cpp RefBase.h Looper.cpp Looper.h)

target_link_libraries(Demo pthread)