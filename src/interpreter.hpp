#pragma once

#include "ast.hpp"
#include "ast_visitor.hpp"

#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// ── Runtime value ─────────────────────────────────────────────────────────────
//
// Forward declarations to break circular dependencies:
//   DValue  ← shared_ptr<vector<TupleElem>>
//   TupleElem ← DValue (by value)
//
// Solution: DValue holds a shared_ptr to an opaque vector; TupleElem is
// defined after DValue is complete; make_tuple/make_func are out-of-line.

struct TupleElem;   // forward – complete definition follows DValue
struct FuncClosure; // forward – complete definition follows TupleElem

struct DValue {
    enum class Type { None, Int, Real, Bool, String, Array, Tuple, Func };
    Type type{Type::None};

    long long ival{};
    double rval{};
    bool bval{};
    std::string sval;
    std::shared_ptr<std::map<long long, DValue>> aval; // Array: key → value
    std::shared_ptr<std::vector<TupleElem>> tval;      // Tuple elements (heap)
    std::shared_ptr<FuncClosure> fval;                 // Function closure

    static DValue make_none() { return {}; }
    static DValue make_int(long long v) {
        DValue d;
        d.type = Type::Int;
        d.ival = v;
        return d;
    }
    static DValue make_real(double v) {
        DValue d;
        d.type = Type::Real;
        d.rval = v;
        return d;
    }
    static DValue make_bool(bool v) {
        DValue d;
        d.type = Type::Bool;
        d.bval = v;
        return d;
    }
    static DValue make_str(std::string v) {
        DValue d;
        d.type = Type::String;
        d.sval = std::move(v);
        return d;
    }
    static DValue make_array(std::map<long long, DValue> m) {
        DValue d;
        d.type = Type::Array;
        d.aval = std::make_shared<std::map<long long, DValue>>(std::move(m));
        return d;
    }

    // Declared here, defined after TupleElem / FuncClosure are complete:
    static DValue make_tuple(std::vector<TupleElem> e);
    static DValue make_func(
        const FuncLitNode* n,
        std::vector<std::shared_ptr<std::unordered_map<std::string, DValue>>> env);

    std::string to_string() const;
    bool is_truthy() const; // throws if not Bool
};

// ── TupleElem (complete after DValue) ────────────────────────────────────────

struct TupleElem {
    std::string name; // empty for unnamed elements
    DValue value;
};

// ── Environment types ─────────────────────────────────────────────────────────

using Frame    = std::unordered_map<std::string, DValue>;
using FramePtr = std::shared_ptr<Frame>;
using Env      = std::vector<FramePtr>;

// ── FuncClosure (complete after Env) ─────────────────────────────────────────

struct FuncClosure {
    const FuncLitNode* node; // non-owning; AST owns the node
    Env captured_env;        // lexical environment at definition time
};

// ── Out-of-line factory definitions (all dependencies now complete) ────────────

inline DValue DValue::make_tuple(std::vector<TupleElem> e) {
    DValue d;
    d.type = Type::Tuple;
    d.tval = std::make_shared<std::vector<TupleElem>>(std::move(e));
    return d;
}

inline DValue DValue::make_func(const FuncLitNode* n, Env env) {
    DValue d;
    d.type = Type::Func;
    d.fval = std::make_shared<FuncClosure>(FuncClosure{n, std::move(env)});
    return d;
}

// ── Control-flow signals (thrown as exceptions) ────────────────────────────────

struct ExitSignal {};
struct ReturnSignal {
    DValue value;
};

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
