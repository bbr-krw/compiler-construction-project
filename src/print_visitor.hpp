#pragma once

#include "ast_visitor.hpp"

#include <ostream>

// ── PrintVisitor ──────────────────────────────────────────────────────────────
//
// Walks the AST and prints an indented textual representation to an ostream.
// Each visit() method writes the current node's header line and recurses into
// children at indent+1 via the helper recurse().

struct ASTNode;

struct PrintVisitor : ASTVisitorBase<PrintVisitor> {
    std::ostream& os_;
    int indent_{0};

    explicit PrintVisitor(std::ostream& os, int indent = 0) : os_{os}, indent_{indent} {}

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
    void visit(const IntLitNode&) override;
    void visit(const RealLitNode&) override;
    void visit(const StrLitNode&) override;
    void visit(const BoolLitNode&) override;
    void visit(const NoneLitNode&) override;
    void visit(const ArrayLitNode&) override;
    void visit(const TupleLitNode&) override;
    void visit(const TupleElemNode&) override;
    void visit(const ParamListNode&) override;
    void visit(const FuncLitNode&) override;
    void visit(const TypeNode&) override;

private:
    void put_indent() const;
    void put_suffix(const ASTNode& n);
    void recurse(const ASTNode* n);
};
