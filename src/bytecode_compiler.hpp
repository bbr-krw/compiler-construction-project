#pragma once

#include "bytecode.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

struct ASTNode;
struct FuncLitNode;

// Maps each FuncLitNode* (or nullptr for the global/main scope) to the set of
// variable names declared in that scope that are captured by at least one inner
// closure.  Used by the compiler to decide REG vs Cell allocation.
using CaptureMap =
    std::unordered_map<const FuncLitNode*, std::unordered_set<std::string>>;

// First pass: walk the fully-annotated AST and collect capture information.
// Must be called after SemanticAnalyzer::analyze().
CaptureMap analyze_captures(const ASTNode& root);

// Compile the AST to a Module (call after semantic analysis).
// The compilation is split into two steps internally:
//   1. analyze_captures()  – identifies closure-captured variables
//   2. BytecodeCompiler     – translates AST nodes to Proto instructions
std::unique_ptr<Module> compile_bytecode(const ASTNode& root);
