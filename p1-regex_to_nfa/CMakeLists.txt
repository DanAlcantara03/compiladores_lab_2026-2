cmake_minimum_required(VERSION 3.16)
project(regex_to_nfa C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(regex_to_nfa
    ./src/main.c
    ./src/nfa.c
    ./src/regex.c
    ./src/data_structures.c
)

target_include_directories(regex_to_nfa PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()

# ---- Data structures tests ----
add_executable(data_structures_tests
    ./tests/data_structures_test.c
    ./src/data_structures.c
)

target_include_directories(data_structures_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME data_structures_tests COMMAND data_structures_tests)

# ---- Regex tests ----

add_executable(regex_tests
    ./tests/regex_test.c
    ./src/regex.c
)

target_include_directories(regex_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME regex_tests COMMAND regex_tests)