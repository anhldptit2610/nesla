cmake_minimum_required(VERSION 3.28.1)

project(nesla)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

add_subdirectory(core)
add_subdirectory(desktop)
add_subdirectory(3rdparty)
add_subdirectory(test)