cmake_minimum_required(VERSION 3.10)
project(Libraries)
set(CMAKE_CXX_STANDARD 17)

file(GLOB all_source  *.cpp  sysutils/*.cpp )
add_library(my_lib SHARED ${all_source} sysutils/SocketClient.cpp sysutils/SocketClient.h)