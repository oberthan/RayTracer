cmake_minimum_required(VERSION 3.29)
project(RayTracer)

set(CMAKE_CXX_STANDARD 20)

find_package(SDL3 CONFIG REQUIRED)

add_executable(RayTracer main.cpp classes.h renderer.h BMP.h)

target_link_libraries(RayTracer PRIVATE SDL3::SDL3)