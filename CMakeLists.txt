cmake_minimum_required(VERSION 3.30)
project(b_compiler C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_executable(b_compiler
        include/codegen_arm.h
        include/lexer.h
        include/parser.h
        include/register_allocator.h
        include/token.h
        include/compile.h
        src/codegen_arm.c
        src/lexer.c
        src/main.c
        src/parser.c
        src/register_allocator.c
        src/token.c
        src/compile.c)
