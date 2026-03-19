cmake_minimum_required(VERSION 3.16)
project(first_and_follow C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(first_and_follow
    ./src/main.c
    ./src/grammar.c
    ./src/analyzer.c
)