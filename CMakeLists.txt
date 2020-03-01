cmake_minimum_required(VERSION 3.15)
project(circular_buffer)

set(CMAKE_CXX_STANDARD 17)

add_executable(circular_buffer main.cpp
        basic_circular_buffer.h
        circular_buffer.h
        compressed_pair.h
        utility.h
        fault_injection.cpp
        fault_injection.h
        counted.cpp
        counted.h
        gtest/gtest-all.cc
        gtest/gtest_main.cc
        gtest/gtest.h)

target_link_libraries(circular_buffer pthread)