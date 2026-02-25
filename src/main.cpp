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
#include "parser.tab.hpp"

#include <cstdio>
#include <print>

extern FILE* yyin;   // flex input stream

int main(int argc, char* argv[]) {
    if (argc > 1) {
        yyin = std::fopen(argv[1], "r");
        if (!yyin) {
            std::println(stderr, "Error: cannot open '{}'", argv[1]);
            return 1;
        }
    }

    ASTNode* root{nullptr};
    yy::parser parser{root};

    const int rc = parser.parse();

    if (argc > 1) std::fclose(yyin);

    if (rc != 0 || !root) {
        std::println(stderr, "Parsing failed.");
        delete root;
        return 1;
    }

    root->print(0);
    delete root;
    return 0;
}
