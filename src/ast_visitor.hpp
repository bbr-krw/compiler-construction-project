#pragma once

// Forward declarations of all concrete node types
struct ProgramNode;
struct BodyNode;
struct VarDeclNode;
struct VarDefNode;
struct AssignNode;
struct IfNode;
struct IfShortNode;
struct WhileNode;
struct ForRangeNode;
struct ForIterNode;
struct LoopInfNode;
struct ExitNode;
struct ReturnNode;
struct PrintNode;
struct BinOpNode;
struct UnaryOpNode;
struct IsNode;
struct IdentNode;
struct IndexNode;
struct CallNode;
struct DotFieldNode;
struct DotIntNode;
struct IntLitNode;
struct RealLitNode;
struct StrLitNode;
struct BoolLitNode;
struct NoneLitNode;
struct ArrayLitNode;
struct TupleLitNode;
struct TupleElemNode;
struct ParamListNode;
struct FuncLitNode;
struct TypeNode;

// ── Abstract visitor interface ────────────────────────────────────────────────

struct IASTVisitor {
    virtual ~IASTVisitor() = default;

    virtual void visit(const ProgramNode&)   = 0;
    virtual void visit(const BodyNode&)      = 0;
    virtual void visit(const VarDeclNode&)   = 0;
    virtual void visit(const VarDefNode&)    = 0;
    virtual void visit(const AssignNode&)    = 0;
    virtual void visit(const IfNode&)        = 0;
    virtual void visit(const IfShortNode&)   = 0;
    virtual void visit(const WhileNode&)     = 0;
    virtual void visit(const ForRangeNode&)  = 0;
    virtual void visit(const ForIterNode&)   = 0;
    virtual void visit(const LoopInfNode&)   = 0;
    virtual void visit(const ExitNode&)      = 0;
    virtual void visit(const ReturnNode&)    = 0;
    virtual void visit(const PrintNode&)     = 0;
    virtual void visit(const BinOpNode&)     = 0;
    virtual void visit(const UnaryOpNode&)   = 0;
    virtual void visit(const IsNode&)        = 0;
    virtual void visit(const IdentNode&)     = 0;
    virtual void visit(const IndexNode&)     = 0;
    virtual void visit(const CallNode&)      = 0;
    virtual void visit(const DotFieldNode&)  = 0;
    virtual void visit(const DotIntNode&)    = 0;
    virtual void visit(const IntLitNode&)    = 0;
    virtual void visit(const RealLitNode&)   = 0;
    virtual void visit(const StrLitNode&)    = 0;
    virtual void visit(const BoolLitNode&)   = 0;
    virtual void visit(const NoneLitNode&)   = 0;
    virtual void visit(const ArrayLitNode&)  = 0;
    virtual void visit(const TupleLitNode&)  = 0;
    virtual void visit(const TupleElemNode&) = 0;
    virtual void visit(const ParamListNode&) = 0;
    virtual void visit(const FuncLitNode&)   = 0;
    virtual void visit(const TypeNode&)      = 0;
};

// ── CRTP base with default no-op implementations ──────────────────────────────
//
// Concrete visitors inherit from ASTVisitorBase<Derived> and override only the
// visit() overloads they care about.  The template parameter Derived enables
// CRTP dispatch in subclasses that need it (e.g. delegating traversal).

template <typename Derived>
struct ASTVisitorBase : IASTVisitor {
    void visit(const ProgramNode&) override {}
    void visit(const BodyNode&) override {}
    void visit(const VarDeclNode&) override {}
    void visit(const VarDefNode&) override {}
    void visit(const AssignNode&) override {}
    void visit(const IfNode&) override {}
    void visit(const IfShortNode&) override {}
    void visit(const WhileNode&) override {}
    void visit(const ForRangeNode&) override {}
    void visit(const ForIterNode&) override {}
    void visit(const LoopInfNode&) override {}
    void visit(const ExitNode&) override {}
    void visit(const ReturnNode&) override {}
    void visit(const PrintNode&) override {}
    void visit(const BinOpNode&) override {}
    void visit(const UnaryOpNode&) override {}
    void visit(const IsNode&) override {}
    void visit(const IdentNode&) override {}
    void visit(const IndexNode&) override {}
    void visit(const CallNode&) override {}
    void visit(const DotFieldNode&) override {}
    void visit(const DotIntNode&) override {}
    void visit(const IntLitNode&) override {}
    void visit(const RealLitNode&) override {}
    void visit(const StrLitNode&) override {}
    void visit(const BoolLitNode&) override {}
    void visit(const NoneLitNode&) override {}
    void visit(const ArrayLitNode&) override {}
    void visit(const TupleLitNode&) override {}
    void visit(const TupleElemNode&) override {}
    void visit(const ParamListNode&) override {}
    void visit(const FuncLitNode&) override {}
    void visit(const TypeNode&) override {}
};
