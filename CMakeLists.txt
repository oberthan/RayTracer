cmake_minimum_required(VERSION 3.29)
project(RayTracer CUDA CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(CUDA REQUIRED)
find_package(SDL3 REQUIRED)

add_executable(RayTracer 
    main.cpp
    renderer.cu
)

target_link_libraries(RayTracer PRIVATE SDL3::SDL3 ${CUDA_LIBRARIES})
target_include_directories(RayTracer PRIVATE ${CUDA_INCLUDE_DIRS})

set_target_properties(RayTracer PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
set_target_properties(RayTracer PROPERTIES CUDA_STANDARD 17)