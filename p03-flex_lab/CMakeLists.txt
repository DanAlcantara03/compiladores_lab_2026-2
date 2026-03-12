cmake_minimum_required(VERSION 3.16)

project(flex_lab LANGUAGES C CXX)

find_package(FLEX REQUIRED)

set(SCANNER_L_FILE ${CMAKE_CURRENT_SOURCE_DIR}/src/scanner.l)
set(SCANNER_API_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/scanner.h)
set(SCANNER_GEN_C_FILE ${CMAKE_CURRENT_BINARY_DIR}/scanner_flex.c)
set(SCANNER_GEN_HEADER ${CMAKE_CURRENT_BINARY_DIR}/scanner_flex.h)

add_custom_command(
	OUTPUT ${SCANNER_GEN_C_FILE} ${SCANNER_GEN_HEADER}
	COMMAND ${FLEX_EXECUTABLE} --header-file=${SCANNER_GEN_HEADER} -o ${SCANNER_GEN_C_FILE} ${SCANNER_L_FILE}
	DEPENDS ${SCANNER_L_FILE} ${SCANNER_API_HEADER}
	VERBATIM
)

add_library(scanner STATIC ${SCANNER_GEN_C_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/src/symtab.c)

add_executable(scanner_cli ${CMAKE_CURRENT_SOURCE_DIR}/src/scanner.c)

target_link_libraries(scanner_cli PRIVATE scanner)

target_include_directories(scanner
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(scanner_cli
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_features(scanner PUBLIC c_std_11)
target_compile_features(scanner_cli PRIVATE c_std_11)

add_custom_target(generate_scanner DEPENDS ${SCANNER_GEN_C_FILE} ${SCANNER_GEN_HEADER})
