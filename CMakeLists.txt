cmake_minimum_required(VERSION 3.15)
project(DLanguageParser CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Tool discovery ─────────────────────────────────────────────────────────────
find_package(FLEX  REQUIRED)
find_package(BISON REQUIRED)

# ── Generated sources ──────────────────────────────────────────────────────────
BISON_TARGET(Parser
    src/parser.y
    ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.cpp
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.tab.hpp
    COMPILE_FLAGS "-Wall"
)

FLEX_TARGET(Lexer
    src/lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp
)

ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# ── Executable ─────────────────────────────────────────────────────────────────
add_executable(dparser
    src/main.cpp
    src/ast.cpp
    ${BISON_Parser_OUTPUTS}
    ${FLEX_Lexer_OUTPUTS}
)

target_include_directories(dparser PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(dparser PRIVATE
    -Wall -Wextra
    -Wno-unused-function   # suppress flex internal helpers
)
