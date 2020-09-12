cmake_minimum_required(VERSION 3.16)

project(project)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(-Wall -Werror -O0)

add_link_options(-pthread)

add_executable(project
        Benchmark.cpp)
