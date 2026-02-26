# Copilot Instructions for D Language Parser

## Project Overview

This is a **compiler frontend** (lexer + parser) for the D dynamic language, a custom educational language. The system generates an Abstract Syntax Tree (AST) from source code. C++23, modern Bison (3.2+), Flex, and CMake.

## Architecture

### Component Flow
```
Source Code (.d file)
    ↓
Flex Lexer (lexer.l) → Tokens
    ↓
Bison Parser (parser.y) → AST
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
  - `NodeKind` enum (PROGRAM, VAR_DECL, IF, while, FOR_RANGE, etc.)
  - Payload variant: `long long`, `double`, `string` for literals/identifiers
  - `.name` field for identifiers and structural metadata
  - `.children` vector owns child nodes (deleted in destructor)
  - No virtual parents (parent tracking is consumer's responsibility)

- **[main.cpp](src/main.cpp)**: Entry point
  - Accepts optional file argument; reads stdin if none
  - Returns exit code 1 on parse failure, 0 on success
  - Prints AST via `root->print(0)` then deletes tree

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

### Running Parser
```bash
./dparser                    # reads stdin
./dparser ../test/suite/test1.d   # parses file
```

### Running Tests
```bash
./lexer_tests                # unit tests (GTest framework)
ctest                        # runs all tests (currently just lexer_tests)
```

### Adding Grammar Rules
When extending `parser.y`:
1. Add token declarations if needed: `%token <type> TOK_NAME`
2. Define production rules with action code in `{ ... }`
3. Use `new ASTNode(NodeKind::...)` to construct nodes
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
auto* node = new ASTNode(NodeKind::IF, yylineno);  // copy line number from lexer
node->children.push_back(cond_node);               // condition
node->children.push_back(then_body);               // then branch
node->children.push_back(else_body);               // optional else
parse_result = node;                               // assign to out-param
```

### Lexer String Handling
- `strip_quotes()` helper removes surrounding quotes and processes escapes: `\n`, `\t`, `\"`, `\'`, `\\`
- Called for both double and single-quoted strings

### Testing Patterns
- **Unit tests** ([test/lexer_test.cpp](test/lexer_test.cpp)): Use `yy_scan_string(input.c_str())` to inject test input
- **Golden tests**: [test/suite/](test/suite/) contains `.d` files with expected AST output in `.gold` files (currently manual verification)

## Common Tasks

### Adding a New Statement Type
1. Add `NodeKind` enum value in [ast.hpp](src/ast.hpp)
2. Add token in [parser.y](src/parser.y) if new keyword needed
3. Add lexer pattern in [lexer.l](src/lexer.l)
4. Add parser production rule in [parser.y](src/parser.y)
5. Rebuild and test with `./dparser test_input.d`

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

- `src/` – lexer.l, parser.y, ast.hpp/cpp, main.cpp (core compiler)
- `build/` – generated files (lexer.yy.*, parser.tab.*) and build artifacts
- `test/suite/` – integration tests (input .d files and golden .gold outputs)
- `test/lexer_test.cpp` – GTest unit tests
- `docs/Project_D_FULL.md` – complete language spec
