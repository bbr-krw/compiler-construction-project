cmake_minimum_required(VERSION 3.15)
project(DLanguageParser CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

set(SUPPORTED_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel Sanitizers)
if(NOT CMAKE_CONFIGURATION_TYPES)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${SUPPORTED_BUILD_TYPES})
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Sanitizers")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(
            -O1
            -g
            -fno-omit-frame-pointer
            -fsanitize=address,undefined
        )
        add_link_options(
            -fsanitize=address,undefined
        )
    else()
        message(FATAL_ERROR "Sanitizers build type is only supported with GCC or Clang")
    endif()
endif()

# ── Tool discovery ─────────────────────────────────────────────────────────────
find_package(BISON REQUIRED)

# ── Generated sources ──────────────────────────────────────────────────────────
BISON_TARGET(parser
    src/parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hpp
    COMPILE_FLAGS "-Wall"
)

# ── Core: lexer + parser + AST ────────────────────────────────────────────────
add_library(parse_lib STATIC
    src/lexer.cpp
    ${BISON_parser_OUTPUTS}
    src/ast.cpp
    src/print_visitor.cpp
    src/token_dump.cpp
)
target_include_directories(parse_lib PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── Semantic analysis ─────────────────────────────────────────────────────────
add_library(sema_lib STATIC
    src/semantic_analyzer.cpp
)
target_link_libraries(sema_lib PUBLIC parse_lib)

# ── Tree-walking interpreter ──────────────────────────────────────────────────
add_library(interp_lib STATIC
    src/interpreter.cpp
)
target_link_libraries(interp_lib PUBLIC parse_lib)

# ── Bytecode compiler ─────────────────────────────────────────────────────────
add_library(bc_lib STATIC
    src/bytecode.cpp
    src/bc_compiler.cpp
)
target_link_libraries(bc_lib PUBLIC parse_lib)

# ── Stack-machine interpreter ─────────────────────────────────────────────────
add_library(sm_lib STATIC
    src/sm_interpreter.cpp
    src/sm_runtime.cpp
)
target_link_libraries(sm_lib PUBLIC bc_lib)

# ── dparser ────────────────────────────────────────────────────────────────────
add_executable(dparser src/dparser.cpp)
target_link_libraries(dparser PRIVATE sema_lib)

# ── dlexer: standalone token dumper ───────────────────────────────────────────
add_executable(dlexer src/dlexer.cpp)
target_link_libraries(dlexer PRIVATE parse_lib)
target_compile_options(dlexer PRIVATE -Wno-unused-function)

# ── dinterp: tree-walking interpreter ─────────────────────────────────────────
add_executable(dinterp src/dinterp.cpp)
target_link_libraries(dinterp PRIVATE interp_lib sema_lib)

# ── dbc: bytecode compiler ────────────────────────────────────────────────────
add_executable(dbc src/dbc.cpp)
target_link_libraries(dbc PRIVATE bc_lib sema_lib)

# ── dbci: stack-machine interpreter ───────────────────────────────────────────
add_executable(dbci src/dbci.cpp)
target_link_libraries(dbci PRIVATE sm_lib sema_lib)

enable_testing()
find_package(GTest REQUIRED)

# Maximum test number in the suite (exclusive upper bound for ::testing::Range)
set(SUITE_MAX_TEST 168)

# ── Lexer suite tests ──────────────────────────────────────────────────────────
add_executable(lexer_suite_tests test/lexer_suite_test.cpp)
target_link_libraries(lexer_suite_tests PRIVATE GTest::gtest GTest::gtest_main parse_lib)
target_compile_definitions(lexer_suite_tests PRIVATE TEST_SUITE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/suite" SUITE_MAX_TEST=${SUITE_MAX_TEST})
add_test(NAME LexerSuiteTests COMMAND lexer_suite_tests)

# ── Parser suite tests ─────────────────────────────────────────────────────────
add_executable(parser_suite_tests test/parser_suite_test.cpp)
target_link_libraries(parser_suite_tests PRIVATE GTest::gtest GTest::gtest_main parse_lib)
target_compile_definitions(parser_suite_tests PRIVATE TEST_SUITE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/suite" SUITE_MAX_TEST=${SUITE_MAX_TEST})
add_test(NAME ParserSuiteTests COMMAND parser_suite_tests)

# ── Semantic analyzer unit tests ───────────────────────────────────────────────
add_executable(sema_tests test/sema_test.cpp)
target_link_libraries(sema_tests PRIVATE GTest::gtest GTest::gtest_main sema_lib)
add_test(NAME SemaTests COMMAND sema_tests)

# ── Interpreter suite tests ────────────────────────────────────────────────────
add_executable(interp_suite_tests test/interp_suite_test.cpp)
target_link_libraries(interp_suite_tests PRIVATE GTest::gtest GTest::gtest_main interp_lib sema_lib)
target_compile_definitions(interp_suite_tests PRIVATE TEST_SUITE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/suite" SUITE_MAX_TEST=${SUITE_MAX_TEST})
add_test(NAME InterpSuiteTests COMMAND interp_suite_tests)

# ── SM interpreter suite tests ────────────────────────────────────────────────────
add_executable(dbci_suite_tests test/dbci_suite_test.cpp)
target_link_libraries(dbci_suite_tests PRIVATE GTest::gtest GTest::gtest_main sm_lib sema_lib)
target_compile_definitions(dbci_suite_tests PRIVATE TEST_SUITE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/suite" SUITE_MAX_TEST=${SUITE_MAX_TEST})
add_test(NAME dbciSuiteTests COMMAND dbci_suite_tests)

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

