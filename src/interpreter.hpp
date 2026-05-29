#pragma once

#include "ast.hpp"
#include "ast_visitor.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using GcHandle                    = uint32_t;
inline constexpr GcHandle GC_NULL = UINT32_MAX;

struct TupleElem;
struct FuncClosure;

struct DValue {
    enum class Type { None, Int, Real, Bool, String, Array, Tuple, Func };
    Type type{Type::None};

    long long ival{};
    double rval{};
    bool bval{};
    std::string sval;
    GcHandle href{GC_NULL};
    std::shared_ptr<FuncClosure> fval;

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

    static DValue make_array(std::map<long long, DValue> m);
    static DValue make_tuple(std::vector<TupleElem> e);

    static DValue make_func(
        const FuncLitNode* n,
        std::vector<std::shared_ptr<std::unordered_map<std::string, DValue>>> env);

    std::string to_string() const;
    bool is_truthy() const;
};

struct TupleElem {
    std::string name;
    DValue value;
};

using Frame    = std::unordered_map<std::string, DValue>;
using FramePtr = std::shared_ptr<Frame>;
using Env      = std::vector<FramePtr>;

struct FuncClosure {
    const FuncLitNode* node;
    Env captured_env;
};

inline DValue DValue::make_func(const FuncLitNode* n, Env env) {
    DValue d;
    d.type = Type::Func;
    d.fval = std::make_shared<FuncClosure>(FuncClosure{n, std::move(env)});
    return d;
}

enum class ObjKind : uint8_t { Array, Tuple };

struct HeapObj {
    ObjKind kind;
    GcHandle fwd{GC_NULL};
    std::map<long long, DValue> arr;
    std::vector<TupleElem> tup;
};

class Heap {
public:
    static constexpr size_t kDefaultThreshold = 512;
    explicit Heap(size_t threshold = kDefaultThreshold);

    GcHandle alloc_array(std::map<long long, DValue> m);
    GcHandle alloc_tuple(std::vector<TupleElem> t);

    HeapObj& get(GcHandle h) { return from_[h]; }
    const HeapObj& get(GcHandle h) const { return from_[h]; }

    size_t size() const { return from_.size(); }
    size_t gc_count() const { return gc_count_; }
    bool needs_gc() const { return from_.size() >= threshold_; }

    void collect(std::vector<DValue*> roots, std::vector<Frame*> env_frames);

private:
    std::vector<HeapObj> from_, to_;
    std::unordered_set<Frame*> gc_visited_frames_;
    size_t threshold_;
    size_t gc_count_{0};

    GcHandle evacuate(GcHandle h);
    void scan_value(DValue& v);
    void scan_obj(GcHandle idx);
};

extern thread_local Heap* g_heap;

struct ExitSignal {};
struct ReturnSignal {
    DValue value;
};

class Interpreter : public ASTVisitorBase<Interpreter> {
public:
    explicit Interpreter(std::ostream& out);
    void run(const ASTNode& root);

    void visit(const ProgramNode&) override;
    void visit(const BodyNode&) override;
    void visit(const VarDeclNode&) override;
    void visit(const VarDefNode&) override;
    void visit(const AssignNode&) override;
    void visit(const ExprStmtNode&) override;
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
    DValue val_;

    void push_frame();
    void pop_frame();
    Env capture_env() const { return env_; }
    void declare(const std::string& name, DValue v = {});
    DValue& lookup_ref(const std::string& name);

    DValue eval(const ASTNode& node);
    void assign_lvalue(const ASTNode& lhs, DValue rhs);
    DValue call_func(const DValue& fv, std::vector<DValue> args);

    Heap heap_;
    void maybe_gc();
};
