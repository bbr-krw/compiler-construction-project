#pragma once

#include "ast_visitor.hpp"

#include <format>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// ── Base node ─────────────────────────────────────────────────────────────────
struct ASTNode {
    int line{0};
    int col{0};

    explicit ASTNode(int ln = 0, int c = 0) : line{ln}, col{c} {}
    virtual ~ASTNode()                 = default;
    ASTNode(const ASTNode&)            = delete;
    ASTNode& operator=(const ASTNode&) = delete;
    ASTNode(ASTNode&&)                 = default;
    ASTNode& operator=(ASTNode&&)      = default;

    virtual std::string_view kind_name() const noexcept = 0;
    virtual void accept(IASTVisitor& v) const = 0;

    void print(int indent, std::ostream& os) const;
    void print(int indent = 0) const;

    // Factory helpers
    static std::unique_ptr<ASTNode> make_int(long long v, int ln = 0, int col = 0);
    static std::unique_ptr<ASTNode> make_real(double v, int ln = 0, int col = 0);
    static std::unique_ptr<ASTNode> make_str(std::string s, int ln = 0, int col = 0);
    static std::unique_ptr<ASTNode> make_ident(std::string s, int ln = 0, int col = 0);
    static std::unique_ptr<ASTNode> make_bool(bool v, int ln = 0, int col = 0);
    static std::unique_ptr<ASTNode> make_none(int ln = 0, int col = 0);
};

// ── Statements / structure ────────────────────────────────────────────────────

struct ProgramNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> stmts;
    explicit ProgramNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "Program"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct BodyNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> stmts;
    explicit BodyNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "Body"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct VarDeclNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> defs; // VarDefNode children
    explicit VarDeclNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "VarDecl"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct VarDefNode : ASTNode {
    std::string varname;
    std::unique_ptr<ASTNode> init; // optional initialiser expression
    explicit VarDefNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "VarDef"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct AssignNode : ASTNode {
    std::unique_ptr<ASTNode> lhs;
    std::unique_ptr<ASTNode> rhs;
    explicit AssignNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "Assign"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct IfNode : ASTNode {
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> then_body;
    std::unique_ptr<ASTNode> else_body; // optional
    explicit IfNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "If"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct IfShortNode : ASTNode {
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> stmt;
    explicit IfShortNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "IfShort"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct WhileNode : ASTNode {
    std::unique_ptr<ASTNode> cond;
    std::unique_ptr<ASTNode> body;
    explicit WhileNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "While"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct ForRangeNode : ASTNode {
    std::string iter; // iterator variable name (may be empty)
    std::unique_ptr<ASTNode> from;
    std::unique_ptr<ASTNode> to;
    std::unique_ptr<ASTNode> body;
    explicit ForRangeNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "ForRange"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct ForIterNode : ASTNode {
    std::string iter; // iterator variable name (may be empty)
    std::unique_ptr<ASTNode> iterable;
    std::unique_ptr<ASTNode> body;
    explicit ForIterNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "ForIter"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct LoopInfNode : ASTNode {
    std::unique_ptr<ASTNode> body;
    explicit LoopInfNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "LoopInf"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct ExitNode : ASTNode {
    explicit ExitNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "Exit"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct ReturnNode : ASTNode {
    std::unique_ptr<ASTNode> value; // optional return expression
    explicit ReturnNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "Return"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct PrintNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> exprs;
    explicit PrintNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "Print"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

// ── Binary operators ──────────────────────────────────────────────────────────

struct BinOpNode : ASTNode {
    enum class Op { OR, AND, XOR, LT, LE, GT, GE, EQ, NEQ, ADD, SUB, MUL, DIV };
    Op op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    explicit BinOpNode(Op o, int ln = 0, int c = 0) : ASTNode{ln, c}, op{o} {}
    std::string_view kind_name() const noexcept override;
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

// ── Unary operators ───────────────────────────────────────────────────────────

struct UnaryOpNode : ASTNode {
    enum class Op { UPLUS, UMINUS, NOT };
    Op op;
    std::unique_ptr<ASTNode> operand;
    explicit UnaryOpNode(Op o, int ln = 0, int c = 0) : ASTNode{ln, c}, op{o} {}
    std::string_view kind_name() const noexcept override;
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct IsNode : ASTNode {
    std::unique_ptr<ASTNode> operand;
    std::unique_ptr<ASTNode> type_node;
    explicit IsNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "Is"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

// ── Postfix / access ──────────────────────────────────────────────────────────

struct IdentNode : ASTNode {
    std::string ident_name;
    explicit IdentNode(std::string name, int ln = 0, int c = 0)
        : ASTNode{ln, c},
          ident_name{std::move(name)} {}
    std::string_view kind_name() const noexcept override { return "Ident"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct IndexNode : ASTNode {
    std::unique_ptr<ASTNode> base;
    std::unique_ptr<ASTNode> index_expr;
    explicit IndexNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "Index"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct CallNode : ASTNode {
    std::unique_ptr<ASTNode> callee;
    std::vector<std::unique_ptr<ASTNode>> args;
    explicit CallNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "Call"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct DotFieldNode : ASTNode {
    std::string field;
    std::unique_ptr<ASTNode> base;
    explicit DotFieldNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "DotField"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct DotIntNode : ASTNode {
    long long index;
    std::unique_ptr<ASTNode> base;
    explicit DotIntNode(long long idx, int ln = 0, int c = 0) : ASTNode{ln, c}, index{idx} {}
    std::string_view kind_name() const noexcept override { return "DotInt"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

// ── Literals ──────────────────────────────────────────────────────────────────

struct IntLitNode : ASTNode {
    long long value;
    explicit IntLitNode(long long v, int ln = 0, int c = 0) : ASTNode{ln, c}, value{v} {}
    std::string_view kind_name() const noexcept override { return "IntLit"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct RealLitNode : ASTNode {
    double value;
    explicit RealLitNode(double v, int ln = 0, int c = 0) : ASTNode{ln, c}, value{v} {}
    std::string_view kind_name() const noexcept override { return "RealLit"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct StrLitNode : ASTNode {
    std::string value;
    explicit StrLitNode(std::string v, int ln = 0, int c = 0)
        : ASTNode{ln, c},
          value{std::move(v)} {}
    std::string_view kind_name() const noexcept override { return "StrLit"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct BoolLitNode : ASTNode {
    bool value;
    explicit BoolLitNode(bool v, int ln = 0, int c = 0) : ASTNode{ln, c}, value{v} {}
    std::string_view kind_name() const noexcept override { return "BoolLit"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct NoneLitNode : ASTNode {
    explicit NoneLitNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "NoneLit"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct ArrayLitNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> elems;
    explicit ArrayLitNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "ArrayLit"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct TupleLitNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> elems; // TupleElemNode children
    explicit TupleLitNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "TupleLit"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct TupleElemNode : ASTNode {
    std::string elem_name; // element name (empty for unnamed)
    std::unique_ptr<ASTNode> expr;
    explicit TupleElemNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "TupleElem"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct ParamListNode : ASTNode {
    std::vector<std::unique_ptr<ASTNode>> params; // IdentNode children
    explicit ParamListNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "ParamList"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

struct FuncLitNode : ASTNode {
    std::unique_ptr<ASTNode> params; // ParamListNode
    std::unique_ptr<ASTNode> body;   // BodyNode
    explicit FuncLitNode(int ln = 0, int c = 0) : ASTNode{ln, c} {}
    std::string_view kind_name() const noexcept override { return "FuncLit"; }
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};

// ── Type indicators ───────────────────────────────────────────────────────────

struct TypeNode : ASTNode {
    enum class Type { INT, REAL, BOOL, STRING, NONE, ARRAY, TUPLE, FUNC };
    Type type;
    explicit TypeNode(Type t, int ln = 0, int c = 0) : ASTNode{ln, c}, type{t} {}
    std::string_view kind_name() const noexcept override;
    void accept(IASTVisitor& v) const override { v.visit(*this); }
};
