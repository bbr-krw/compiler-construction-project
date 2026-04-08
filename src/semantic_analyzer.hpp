#pragma once

#include "ast_visitor.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct ASTNode;

struct SemanticError {
    int line{0};
    int col{0};
    std::string message;
};

// ── SemanticAnalyzer ──────────────────────────────────────────────────────────
//
// Walks the AST once and reports:
//   - Use of undeclared variables
//   - Duplicate variable declarations in the same scope
//   - `exit` used outside a loop
//   - `return` used outside a function body
//
// Scoping rules follow D language spec:
//   - Each Body / FuncLit opens a new scope
//   - For-loop iterator variables are scoped to the loop body
//   - Function parameters are scoped to the function body
//
// After analyze() returns, errors() contains all collected diagnostics.
// ok() is true when errors() is empty.

struct SemanticAnalyzer : ASTVisitorBase<SemanticAnalyzer> {
    // ── Public API ─────────────────────────────────────────────────────────────

    /// Run analysis on the given AST root.
    void analyze(const ASTNode& root);

    const std::vector<SemanticError>& errors() const noexcept { return errors_; }
    bool ok() const noexcept { return errors_.empty(); }

    // ── Visitor overrides ──────────────────────────────────────────────────────

    void visit(const ProgramNode&) override;
    void visit(const BodyNode&) override;
    void visit(const VarDeclNode&) override;
    void visit(const VarDefNode&) override;
    void visit(const AssignNode&) override;
    void visit(const IfNode&) override;
    void visit(const IfShortNode&) override;
    void visit(const WhileNode&) override;
    void visit(const ForRangeNode&) override;
    void visit(const ForIterNode&) override;
    void visit(const LoopInfNode&) override;
    void visit(const ExitNode&) override;
    void visit(const ReturnNode&) override;
    void visit(const PrintNode&) override;
    void visit(const BinOpNode&) override;
    void visit(const UnaryOpNode&) override;
    void visit(const IsNode&) override;
    void visit(const IdentNode&) override;
    void visit(const IndexNode&) override;
    void visit(const CallNode&) override;
    void visit(const DotFieldNode&) override;
    void visit(const DotIntNode&) override;
    void visit(const ArrayLitNode&) override;
    void visit(const TupleLitNode&) override;
    void visit(const TupleElemNode&) override;
    void visit(const ParamListNode&) override;
    void visit(const FuncLitNode&) override;

private:
    // ── Scope stack ────────────────────────────────────────────────────────────

    using Scope = std::unordered_map<std::string, int /*decl line*/>;

    std::vector<Scope> scopes_; // back() = innermost scope
    int loop_depth_{0};
    int func_depth_{0};

    std::vector<SemanticError> errors_;

    // ── Helpers ────────────────────────────────────────────────────────────────

    void push_scope();
    void pop_scope();

    /// Declare a name in the current (innermost) scope.
    /// Reports an error if the name is already declared in the same scope.
    void declare(const std::string& name, int line, int col);

    /// Resolve a name by searching scopes from innermost outward.
    /// Returns the depth (0 = innermost), or -1 and records an error if not found.
    int resolve(const std::string& name, int line, int col);

    bool in_loop() const noexcept { return loop_depth_ > 0; }
    bool in_func() const noexcept { return func_depth_ > 0; }

    void error(int line, int col, std::string msg);

    void accept(const ASTNode* n);
};
