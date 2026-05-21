#include "ast.hpp"
#include "bc_compiler.hpp"
#include "bytecode.hpp"
#include "lexer.hpp"
#include "parser.tab.hpp"
#include "semantic_analyzer.hpp"
#include "sm_interpreter.hpp"
#include "sm_runtime.hpp"

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
            std::println(stderr, "Semantic error at {}:{}: {}", e.loc.line, e.loc.col, e.message);
        return 2;
    }

    sm::BcFile bc_file;
    try {
        sm::BcCompiler bc_comp;
        bc_file = bc_comp.compile(*root);

        // debug output
        std::println(stderr, "Bytecode generated");
        bc_file.print(std::cerr);
    } catch (const std::exception& ex) {
        std::println(stderr, "Bytecode compilation error: {}", ex.what());
        return 3;
    }

    try {
        std::println(stderr, "Running SM interpreter");
        sm::Runtime runtime;
        sm::interprete(&runtime, bc_file);
    } catch (const std::exception& ex) {
        std::println(stderr, "Runtime error: {}", ex.what());
        return 3;
    }

    return 0;
}
