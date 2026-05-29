cmake_minimum_required(VERSION 3.16)

project(summc_fis25 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

BISON_TARGET(
    SummarizeParser
    src/parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.h
)

FLEX_TARGET(
    SummarizeLexer
    src/lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
)

ADD_FLEX_BISON_DEPENDENCY(SummarizeLexer SummarizeParser)

add_executable(summc
    ${BISON_SummarizeParser_OUTPUTS}
    ${FLEX_SummarizeLexer_OUTPUTS}
    src/ast.c
    src/symtab.c
    src/codegen.c
)

target_include_directories(summc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(summc PRIVATE _POSIX_C_SOURCE=200809L)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(summc PRIVATE -Wall -Wextra -Wpedantic)
endif()
