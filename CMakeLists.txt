cmake_minimum_required(VERSION 3.15)
project(circular_buffer)

set(CMAKE_CXX_STANDARD 17)

set(BUFF_SRC
        src/basic_circular_buffer.h
        src/circular_buffer.h
        src/compressed_pair.h
        src/utility.h)

set(TEST_SRC
        test/fault_injection.cpp
        test/fault_injection.h
        test/counted.cpp
        test/counted.h)

set(GTEST_SRC
        gtest/gtest-all.cc
        gtest/gtest_main.cc
        gtest/gtest.h)

add_executable(buffer_test main.cpp
        ${BUFF_SRC}
        ${TEST_SRC}
        ${GTEST_SRC})

if (UNIX AND NOT APPLE)
target_link_libraries(circular_buffer pthread)
endif(UNIX AND NOT APPLE)