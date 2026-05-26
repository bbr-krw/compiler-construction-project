#pragma once

#include "ast.hpp"
#include "ast_visitor.hpp"
#include "runtime.hpp"

#include <ostream>
#include <string>
#include <vector>

// ── Interpreter ───────────────────────────────────────────────────────────────

class Interpreter : public ASTVisitorBase<Interpreter> {
public:
    explicit Interpreter(std::ostream& out);
    void run(const ASTNode& root);

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
    void visit(const FuncLitNode&) override;
    void visit(const TypeNode&) override;

private:
    std::ostream& out_;
    Env env_;
    DValue val_; // expression result register

    void push_frame();
    void pop_frame();
    Env capture_env() const { return env_; }
    void declare(const std::string& name, DValue v = {});
    DValue& lookup_ref(const std::string& name);

    DValue eval(const ASTNode& node);
    void assign_lvalue(const ASTNode& lhs, DValue rhs);
    DValue call_func(const DValue& fv, std::vector<DValue> args);
};
