cmake_minimum_required(VERSION 3.30)
project(b_compiler C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

file(GLOB HEADER_FILES include/*.h)
file(GLOB SOURCE_FILES src/*.c)

add_executable(b_compiler ${HEADER_FILES} ${SOURCE_FILES})