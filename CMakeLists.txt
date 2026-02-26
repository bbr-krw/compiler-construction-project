cmake_minimum_required(VERSION 3.15)
project(DLanguageParser CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Tool discovery ─────────────────────────────────────────────────────────────
find_package(FLEX  REQUIRED)
find_package(BISON REQUIRED)

# ── Generated sources ──────────────────────────────────────────────────────────
BISON_TARGET(parser
    src/parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hpp
    COMPILE_FLAGS "-Wall"
)

# Generate lexer with Flex
FLEX_TARGET(lexer src/lexer.l 
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.cpp
    COMPILE_FLAGS "--header-file=${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.hpp"
)

ADD_FLEX_BISON_DEPENDENCY(lexer parser)

# Create lexer library
add_library(lexer_lib
    ${FLEX_lexer_OUTPUTS}
    ${BISON_parser_OUTPUTS}
    src/ast.cpp
    src/token_dump.cpp
)

target_include_directories(lexer_lib
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── Executable ─────────────────────────────────────────────────────────────────
add_executable(dparser
    src/dparser.cpp
    ${BISON_Parser_OUTPUTS}
)

target_link_libraries(dparser PRIVATE lexer_lib)

target_include_directories(dparser PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(dparser PRIVATE
    -Wall -Wextra
    -Wno-unused-function   # suppress flex internal helpers
)

enable_testing()
find_package(GTest REQUIRED)

# Test executable
add_executable(parser_suite_tests
    test/parser_suite_test.cpp
)

target_link_libraries(parser_suite_tests PRIVATE lexer_lib)

target_include_directories(parser_suite_tests
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(parser_suite_tests
    PRIVATE
    GTest::gtest
    GTest::gtest_main
    lexer_lib
)

target_compile_definitions(parser_suite_tests PRIVATE
    TEST_SUITE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/suite"
)

add_test(NAME ParserSuiteTests COMMAND parser_suite_tests)

# ── dlexer: standalone token dumper ───────────────────────────────────────────
add_executable(dlexer
    src/dlexer.cpp
)

target_link_libraries(dlexer PRIVATE lexer_lib)

target_include_directories(dlexer PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(dlexer PRIVATE
    -Wall -Wextra
    -Wno-unused-function
)

# ── Lexer suite tests ──────────────────────────────────────────────────────────
add_executable(lexer_suite_tests
    test/lexer_suite_test.cpp
)

target_link_libraries(lexer_suite_tests
    PRIVATE
    GTest::gtest
    GTest::gtest_main
    lexer_lib
)

target_include_directories(lexer_suite_tests
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(lexer_suite_tests PRIVATE
    TEST_SUITE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/suite"
)

add_test(NAME LexerSuiteTests COMMAND lexer_suite_tests)

# ── clang-format targets ───────────────────────────────────────────────────────
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE FORMAT_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${FORMAT_SOURCES}
        COMMENT "Running clang-format (in-place)"
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${FORMAT_SOURCES}
        COMMENT "Checking clang-format compliance"
    )
else()
    message(STATUS "clang-format not found; 'format' and 'format-check' targets unavailable")
endif()
