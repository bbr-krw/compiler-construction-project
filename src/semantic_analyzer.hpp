#pragma once

#include "ast.hpp"
#include "ast_visitor.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct SemanticError {
    Location loc{};
    std::string message;
};

struct SemanticAnalyzer : ASTVisitorBase<SemanticAnalyzer> {

    void analyze(const ASTNode& root);

    const std::vector<SemanticError>& errors() const noexcept { return errors_; }
    bool ok() const noexcept { return errors_.empty(); }

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
    using Scope = std::unordered_map<std::string, int /*decl line*/>;

    std::vector<Scope> scopes_;
    int loop_depth_{0};
    int func_depth_{0};

    std::vector<SemanticError> errors_;

    void push_scope();
    void pop_scope();

    void declare(const std::string& name, Location loc);

    int resolve(const std::string& name, Location loc);

    bool in_loop() const noexcept { return loop_depth_ > 0; }
    bool in_func() const noexcept { return func_depth_ > 0; }

    void error(Location loc, std::string msg);

    void accept(const ASTNode* n);
};
