cmake_minimum_required(VERSION 3.10)
project(Demo)

set(CMAKE_CXX_STANDARD 14)

add_executable(Demo main.cpp RefBase.cpp RefBase.h Looper.cpp Looper.h)