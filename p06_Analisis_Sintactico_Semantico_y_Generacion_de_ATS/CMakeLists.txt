cmake_minimum_required(VERSION 3.16)

project(p06_compact_language C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

BISON_TARGET(
    CompactParser
    src/parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.h
)

FLEX_TARGET(
    CompactLexer
    src/lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
)

ADD_FLEX_BISON_DEPENDENCY(CompactLexer CompactParser)

add_executable(p06_validator
    ${BISON_CompactParser_OUTPUTS}
    ${FLEX_CompactLexer_OUTPUTS}
    src/ast.c
    src/symtab.c
)

target_include_directories(p06_validator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(p06_validator PRIVATE _POSIX_C_SOURCE=200809L)

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(p06_validator PRIVATE -Wall -Wextra -Wpedantic)
endif()
