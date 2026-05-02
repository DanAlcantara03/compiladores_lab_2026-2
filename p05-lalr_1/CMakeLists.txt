cmake_minimum_required(VERSION 3.16)
project(first_and_follow LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(FLEX REQUIRED)
flex_target(generate_scanner
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scanner.l
    ${CMAKE_CURRENT_BINARY_DIR}/scanner.c
)

add_executable(first_and_follow
    ./src/main.c
    ./src/grammar.c
    ./src/analyzer.c
    ./src/automaton.c
    ./src/parser.c
    ${FLEX_generate_scanner_OUTPUTS}
)

target_include_directories(first_and_follow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(lalr_trace_driver
    ./tools/lalr_trace_driver.cpp
)

target_link_libraries(lalr_trace_driver PRIVATE
    nlohmann_json::nlohmann_json
)
