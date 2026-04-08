#include "ast.hpp"
#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.tab.hpp"
#include "semantic_analyzer.hpp"

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
    Lexer lexer{argc > 1 ? static_cast<std::istream&>(yyin) : std::cin};
    yy::parser parser{root, lexer};
    if (parser.parse() != 0 || !root) {
        std::println(stderr, "Parsing failed.");
        return 1;
    }

    SemanticAnalyzer sema;
    sema.analyze(*root);
    if (!sema.ok()) {
        for (const auto& e : sema.errors())
            std::println(stderr, "Semantic error at {}:{}: {}", e.line, e.col, e.message);
        return 2;
    }

    try {
        Interpreter interp{std::cout};
        interp.run(*root);
    } catch (const std::exception& ex) {
        std::println(stderr, "Runtime error: {}", ex.what());
        return 3;
    }
    return 0;
}