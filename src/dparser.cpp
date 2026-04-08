/*
 * main.cpp – entry point for the D language parser (C++23)
 *
 * Usage:
 *   dparser [file]
 *
 * Without arguments reads from stdin.
 * Prints the AST on success; exits with 1 on parse error.
 */
#include "ast.hpp"
#include "lexer.hpp"
#include "parser.tab.hpp"
#include "semantic_analyzer.hpp"

#include <cstdio>
#include <fstream>
#include <memory>
#include <print>

int main(int argc, char* argv[]) {

    std::ifstream yyin;
    if (argc > 1) {
        yyin = std::ifstream(argv[1]);
        if (!yyin) {
            std::println(stderr, "Error: cannot open '{}'", argv[1]);
            return 1;
        }
    }

    std::unique_ptr<ASTNode> root;
    Lexer lexer{yyin};
    yy::parser parser{root, lexer};

    const int rc = parser.parse();

    if (argc > 1)
        yyin.close();

    if (rc != 0 || !root) {
        std::println(stderr, "Parsing failed.");
        return 1;
    }

    root->print(0);

    SemanticAnalyzer sema;
    sema.analyze(*root);
    if (!sema.ok()) {
        for (const auto& e : sema.errors())
            std::println(stderr, "Semantic error at {}:{}: {}", e.loc.line, e.loc.col, e.message);
        return 2;
    }

    return 0;
}
