#include "ast.hpp"
#include "bytecode.hpp"
#include "bytecode_compiler.hpp"
#include "bytecode_io.hpp"
#include "lexer.hpp"
#include "parser.tab.hpp"
#include "semantic_analyzer.hpp"
#include "vm.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <print>
#include <string_view>

// ── Usage ──────────────────────────────────────────────────────────────────────
// dvm [options] [file]
//
//   dvm foo.dl              compile + run foo.dl
//   dvm foo.dbc             load foo.dbc and run it
//   dvm --compile foo.dl    compile foo.dl → foo.dbc (written next to the source)
//   dvm --compile foo.dl -o out.dbc
//                           compile foo.dl → out.dbc
//   dvm --disas  foo.dbc    print annotated disassembly, then exit

// ── Helpers ────────────────────────────────────────────────────────────────────

// Compile a source file (or stdin when path is empty) to a Module.
static std::unique_ptr<Module> compile_source(const std::string& path) {
    std::ifstream file_in;
    if (!path.empty()) {
        file_in = std::ifstream(path);
        if (!file_in) {
            std::println(stderr, "Error: cannot open '{}'", path);
            return nullptr;
        }
    }

    std::unique_ptr<ASTNode> root;
    Lexer      lexer{path.empty() ? static_cast<std::istream&>(std::cin) : file_in};
    yy::parser parser{root, lexer};
    if (parser.parse() != 0 || !root) {
        std::println(stderr, "Parsing failed.");
        return nullptr;
    }

    SemanticAnalyzer sema;
    sema.analyze(*root);
    if (!sema.ok()) {
        for (const auto& e : sema.errors())
            std::println(stderr, "Semantic error at {}:{}: {}",
                         e.loc.line, e.loc.col, e.message);
        return nullptr;
    }

    try {
        return compile_bytecode(*root);
    } catch (const std::exception& ex) {
        std::println(stderr, "Compilation error: {}", ex.what());
        return nullptr;
    }
}

// Load a Module from a .dbc file.
static std::unique_ptr<Module> load_dbc(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::println(stderr, "Error: cannot open '{}'", path);
        return nullptr;
    }
    try {
        auto m = std::make_unique<Module>(read_module(in));
        return m;
    } catch (const std::exception& ex) {
        std::println(stderr, "Error reading '{}': {}", path, ex.what());
        return nullptr;
    }
}

// ── Entry point ────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // ── Parse CLI ──────────────────────────────────────────────────────────────
    enum class Mode { Run, Compile, Disas };
    Mode        mode     = Mode::Run;
    std::string src_path;    // .dl source or .dbc file
    std::string out_path;    // output .dbc path (--compile only)

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--compile") {
            mode = Mode::Compile;
        } else if (arg == "--disas") {
            mode = Mode::Disas;
        } else if (arg == "-o") {
            if (i + 1 >= argc) {
                std::println(stderr, "Error: '-o' requires an argument");
                return 1;
            }
            out_path = argv[++i];
        } else if (arg.starts_with("-")) {
            std::println(stderr, "Error: unknown option '{}'", arg);
            return 1;
        } else {
            src_path = std::string(arg);
        }
    }

    // ── Mode: Disas ────────────────────────────────────────────────────────────
    if (mode == Mode::Disas) {
        if (src_path.empty()) {
            std::println(stderr, "Error: --disas requires a .dbc file argument");
            return 1;
        }
        auto m = load_dbc(src_path);
        if (!m) return 1;
        disassemble(*m, std::cout);
        return 0;
    }

    // ── Mode: Compile ──────────────────────────────────────────────────────────
    if (mode == Mode::Compile) {
        if (src_path.empty()) {
            std::println(stderr, "Error: --compile requires a source file argument");
            return 1;
        }
        auto m = compile_source(src_path);
        if (!m) return 1;

        // Default output path: replace/append .dbc extension next to source.
        if (out_path.empty()) {
            namespace fs = std::filesystem;
            out_path = fs::path(src_path).replace_extension(".dbc").string();
        }

        std::ofstream out(out_path, std::ios::binary);
        if (!out) {
            std::println(stderr, "Error: cannot write '{}'", out_path);
            return 1;
        }
        write_module(*m, out);
        std::println("Written: {}", out_path);
        return 0;
    }

    // ── Mode: Run ──────────────────────────────────────────────────────────────
    std::unique_ptr<Module> m;

    // Detect whether the input is a pre-compiled .dbc file.
    bool is_dbc = !src_path.empty() &&
                  std::filesystem::path(src_path).extension() == ".dbc";

    if (is_dbc) {
        m = load_dbc(src_path);
    } else {
        m = compile_source(src_path);
    }
    if (!m) return 1;

    try {
        VM vm{std::cout};
        vm.run(*m);
    } catch (const std::exception& ex) {
        std::println(stderr, "Runtime error: {}", ex.what());
        return 4;
    }
    return 0;
}
