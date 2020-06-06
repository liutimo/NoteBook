cmake_minimum_required(VERSION 3.10)
project(Vold)
set(CMAKE_CXX_STANDARD 17)

add_executable(Vold main.cpp main.cpp NetlinkManager.cpp NetlinkManager.h)