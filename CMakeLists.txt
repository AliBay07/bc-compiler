cmake_minimum_required(VERSION 3.22)
project(B)

set(CMAKE_CXX_STANDARD 20)

add_executable(bcc src/b-compiler.cpp src/lexer/lexer.cpp src/parser/parser.cpp src/allocator/allocator.cpp src/generator/generator.cpp)

