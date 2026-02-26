# Copilot Instructions for D Language Parser

## Project Overview

This is a **compiler frontend** (lexer + parser) for the D dynamic language, a custom educational language. The system generates an Abstract Syntax Tree (AST) from source code. C++23, modern Bison (3.2+), Flex, and CMake.

## Architecture

### Component Flow
```
Source Code (.d file)
    ↓
Flex Lexer (lexer.l) → Tokens
    ↓                     ↓
Bison Parser (parser.y)  dlexer (standalone token dump)
    ↓
ASTNode tree (ast.hpp/cpp)
    ↓
dparser prints AST or exits with error
```

### Core Components

- **[lexer.l](src/lexer.l)**: Flex scanner that tokenizes D source code
  - Returns `yy::parser::symbol_type` objects (Bison's modern C++ token constructor)
  - Handles keywords, operators, literals, strings with escape sequences
  - Uses `yy::parser::make_*()` factory methods (not manual token construction)

- **[parser.y](src/parser.y)**: Bison LALR(1) grammar
  - Modern C++ skeleton: `lalr1.cc` with `api.value.type variant`
  - Passes AST root by reference: `%parse-param { ASTNode*& parse_result }`
  - Error messages include line numbers via `yylineno`

- **[ast.hpp/cpp](src/ast.hpp)**: Node-based AST structure
  - `NodeKind` enum — full list:
    - Root: `PROGRAM`
    - Declarations: `VAR_DECL`, `VAR_DEF`
    - Statements: `ASSIGN`, `IF`, `IF_SHORT`, `WHILE`, `FOR_RANGE`, `FOR_ITER`, `LOOP_INF`, `EXIT`, `RETURN`, `PRINT`, `BODY`
    - Binary ops: `OR`, `AND`, `XOR`, `LT`, `LE`, `GT`, `GE`, `EQ`, `NEQ`, `ADD`, `SUB`, `MUL`, `DIV`
    - Unary ops: `UPLUS`, `UMINUS`, `NOT`, `IS`
    - Postfix/ref: `IDENT`, `INDEX`, `CALL`, `DOT_FIELD`, `DOT_INT`
    - Literals: `INT_LIT`, `REAL_LIT`, `STR_LIT`, `BOOL_LIT`, `NONE_LIT`, `ARRAY_LIT`, `TUPLE_LIT`, `TUPLE_ELEM`, `FUNC_LIT`, `PARAM_LIST`
    - Type indicators: `TYPE_INT`, `TYPE_REAL`, `TYPE_BOOL`, `TYPE_STRING`, `TYPE_NONE`, `TYPE_ARRAY`, `TYPE_TUPLE`, `TYPE_FUNC`
  - Payload variant: `std::variant<std::monostate, long long, double, std::string>`
  - `.name` field for `VAR_DEF` name, `FOR_RANGE`/`FOR_ITER` iterator, `TUPLE_ELEM` element name, `DOT_FIELD` field name
  - `.children` vector owns child nodes (recursively deleted in destructor)
  - Factory methods: `make()`, `make_int()`, `make_real()`, `make_str()`, `make_ident()`, `make_bool()`, `make_none()`
  - Helper methods: `add_child()`, `prepend_child()`
  - Payload accessors: `get_ival()`, `get_rval()`, `get_sval()`

- **[dparser.cpp](src/dparser.cpp)**: Entry point for the parser binary
  - Accepts optional file argument; reads stdin if none
  - Returns exit code 1 on parse failure, 0 on success
  - Prints AST via `root->print(0)` then deletes tree

- **[dlexer.cpp](src/dlexer.cpp)**: Entry point for the standalone lexer/token-dump binary

- **[token_dump.hpp/cpp](src/token_dump.hpp)**: `dump_tokens(input)` — tokenizes a string and returns one token per line
  - Format: `TOK_NAME` for keyword/operator tokens, `TOK_NAME(value)` for valued tokens, `YYEOF` for end-of-input

## Build & Test Workflow

### Development Setup
- Use `flake.nix` for reproducible dev environment (includes gcc, cmake, bison, flex, gtest)
- Command: `nix flake show && nix develop` (or `nix-shell` in older setups)

### Build Commands
```bash
mkdir -p build && cd build
cmake ..
make
```

Produces three binaries in `build/`: `dparser`, `dlexer`, `lexer_suite_tests`, `parser_suite_tests`.

### Running Parser / Lexer
```bash
./dparser                         # parse stdin, print AST
./dparser ../test/suite/test1.d   # parse file, print AST
./dlexer   ../test/suite/test1.d  # dump tokens for a file
```

### Running Tests
```bash
ctest                  # runs LexerSuiteTests + ParserSuiteTests
./lexer_suite_tests    # lexer golden tests directly
./parser_suite_tests   # parser golden tests directly
```

Tests are parameterised over `test/suite/test{1..150}.d`:
- Lexer: compares `dump_tokens()` output against `.lgold` files
- Parser: compares `root->print()` output against `.pgold` files
- Skip gracefully if a `.d` / gold file is absent

### Regenerating Golden Files
```bash
bash test/generate_gold_lexer.sh    # regenerates .lgold files
bash test/generate_gold_parser.sh   # regenerates .pgold files
```

### Adding Grammar Rules
When extending `parser.y`:
1. Add token declarations if needed: `%token <type> TOK_NAME`
2. Define production rules with action code in `{ ... }`
3. Use `ASTNode::make*(...)` factory methods to construct nodes
4. Set `.name` for identifiers/structural nodes
5. Rebuild: `make` (CMake detects .y/.l changes and regenerates)
6. GCC warnings about flex internal helpers are suppressed (`-Wno-unused-function`)

## Key Conventions & Patterns

### Bison Modern C++ (Critical Differences from Classic)
- **No `yylval` global**: values live in `symbol_type` constructors
- Lexer returns: `yy::parser::make_TOK_NAME(value)` for valued tokens, `yy::parser::make_TOK_NAME()` for void
- Parser semantic values are union-free `std::variant<long long, double, std::string, ASTNode*>`
- Error handling: override `yy::parser::error(const std::string&)` (declared in `%code`)

### AST Construction Patterns
```cpp
// Prefer factory methods
auto* node = ASTNode::make(NodeKind::IF, yylineno);
node->add_child(cond_node);    // children[0] = condition
node->add_child(then_body);    // children[1] = then branch
node->add_child(else_body);    // children[2] = optional else
parse_result = node;           // assign to out-param
```

### Lexer String Handling
- `strip_quotes()` helper removes surrounding quotes and processes escapes: `\n`, `\t`, `\"`, `\'`, `\\`
- Called for both double and single-quoted strings

### Testing Patterns
- **Lexer suite** ([test/lexer_suite_test.cpp](test/lexer_suite_test.cpp)): calls `dump_tokens()` on `.d` files and diffs against `.lgold`
- **Parser suite** ([test/parser_suite_test.cpp](test/parser_suite_test.cpp)): runs parser via `yy_scan_string()` and diffs `root->print()` against `.pgold`
- Both are GTest parameterised fixtures over test IDs 1–150; missing files are skipped

## Common Tasks

### Adding a New Statement Type
1. Add `NodeKind` enum value in [ast.hpp](src/ast.hpp)
2. Add token in [parser.y](src/parser.y) if new keyword needed
3. Add lexer pattern in [lexer.l](src/lexer.l)
4. Add parser production rule in [parser.y](src/parser.y)
5. Rebuild and test: `./dparser test_input.d`
6. Regenerate golden files if the new construct appears in suite tests

### Debugging Parse Errors
- Parser prints: `Parse error at line N: ...` (check grammar for ambiguity)
- Enable Bison debugging: add `%debug` and `%define parse.trace` in parser.y, recompile
- Use `gdb ./dparser` for post-mortem on crashes

### Memory Safety
- AST destructor recursively deletes children → never manually delete child nodes
- Parser owns `parse_result` tree; caller must `delete root` after use

## D Language Syntax (Simplified)

```
var x := 42
print x

if x > 10 then
  print "big"
else
  print "small"
end

for i in 1..10 do
  print i
end

func add(a, b) => a + b
print add(3, 4)
```

See [docs/Project_D_FULL.md](docs/Project_D_FULL.md) for full spec.

## File Organization

- `src/` – lexer.l, parser.y, ast.hpp/cpp, dparser.cpp, dlexer.cpp, token_dump.hpp/cpp
- `build/` – generated files (lexer.yy.*, parser.tab.*) and build artifacts
- `test/suite/` – integration tests: `.d` input, `.lgold` lexer golden, `.pgold` parser golden
- `test/lexer_suite_test.cpp` – GTest lexer suite (parameterised, tests 1–150)
- `test/parser_suite_test.cpp` – GTest parser suite (parameterised, tests 1–150)
- `test/generate_gold_lexer.sh` / `test/generate_gold_parser.sh` – golden file generators
- `docs/Project_D_FULL.md` – complete language spec
