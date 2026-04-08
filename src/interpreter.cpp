#include "interpreter.hpp"

#include "ast.hpp"

#include <cmath>
#include <format>
#include <stdexcept>

// ── DValue helpers ─────────────────────────────────────────────────────────────

std::string DValue::to_string() const {
    switch (type) {
    case Type::None:
        return "none";
    case Type::Int:
        return std::to_string(ival);
    case Type::Bool:
        return bval ? "true" : "false";
    case Type::String:
        return sval;
    case Type::Real:
        if (std::isfinite(rval) && rval == std::floor(rval))
            return std::to_string(static_cast<long long>(rval));
        return std::format("{:g}", rval);
    case Type::Array: {
        std::string s = "[";
        bool first    = true;
        for (auto& [k, v] : *aval) {
            if (!first)
                s += ", ";
            s += v.to_string();
            first = false;
        }
        return s + "]";
    }
    case Type::Tuple: {
        std::string s = "{";
        bool first    = true;
        for (auto& e : *tval) {
            if (!first)
                s += ", ";
            s += e.name.empty() ? e.value.to_string() : e.name + " := " + e.value.to_string();
            first = false;
        }
        return s + "}";
    }
    case Type::Func:
        return "<func>";
    }
    return "";
}

bool DValue::is_truthy() const {
    if (type == Type::Bool)
        return bval;
    throw std::runtime_error("non-boolean value used in boolean context");
}

// ── Interpreter ────────────────────────────────────────────────────────────────

Interpreter::Interpreter(std::ostream& out) : out_{out} {}

void Interpreter::run(const ASTNode& root) {
    env_.clear();
    push_frame();
    root.accept(*this);
    // Break shared_ptr reference cycles: closures capture env frames by
    // shared_ptr, and those frames may store the same closures as variables.
    // Clear values first (dropping closure→frame refs), then release frames.
    val_ = {};
    for (auto& frame : env_)
        frame->clear();
    env_.clear();
}

void Interpreter::push_frame() {
    env_.push_back(std::make_shared<Frame>());
}
void Interpreter::pop_frame() {
    env_.pop_back();
}

void Interpreter::declare(const std::string& name, DValue v) {
    (*env_.back())[name] = std::move(v);
}

DValue& Interpreter::lookup_ref(const std::string& name) {
    for (auto it = env_.rbegin(); it != env_.rend(); ++it)
        if (auto jt = (*it)->find(name); jt != (*it)->end())
            return jt->second;
    throw std::runtime_error(std::format("undefined variable '{}'", name));
}

DValue Interpreter::eval(const ASTNode& node) {
    node.accept(*this);
    return std::move(val_);
}

// ── Statements ─────────────────────────────────────────────────────────────────

void Interpreter::visit(const ProgramNode& n) {
    for (const auto& s : n.stmts)
        s->accept(*this);
}

void Interpreter::visit(const BodyNode& n) {
    push_frame();
    // RAII: always pop frame even when exception propagates
    struct Guard {
        Interpreter& i;
        ~Guard() { i.pop_frame(); }
    } g{*this};
    for (const auto& s : n.stmts)
        s->accept(*this);
}

void Interpreter::visit(const VarDeclNode& n) {
    for (const auto& d : n.defs)
        d->accept(*this);
}

void Interpreter::visit(const VarDefNode& n) {
    // Mirror semantic analyzer: pre-declare func-literal initialisers so the
    // closure can capture a frame that already contains the variable slot.
    const bool is_func_init = n.init && dynamic_cast<const FuncLitNode*>(n.init.get()) != nullptr;
    if (is_func_init)
        declare(n.varname); // initially none
    DValue v = n.init ? eval(*n.init) : DValue{};
    if (is_func_init)
        lookup_ref(n.varname) = std::move(v);
    else
        declare(n.varname, std::move(v));
}

void Interpreter::visit(const AssignNode& n) {
    assign_lvalue(*n.lhs, eval(*n.rhs));
}

void Interpreter::assign_lvalue(const ASTNode& lhs, DValue rhs) {
    if (auto* id = dynamic_cast<const IdentNode*>(&lhs)) {
        (*env_[env_.size() - 1 - id->resolved_depth])[id->ident_name] = std::move(rhs);
    } else if (auto* idx = dynamic_cast<const IndexNode*>(&lhs)) {
        DValue base = eval(*idx->base);
        DValue key  = eval(*idx->index_expr);
        if (base.type != DValue::Type::Array)
            throw std::runtime_error("index assignment on non-array");
        if (key.type != DValue::Type::Int)
            throw std::runtime_error("array index must be integer");
        (*base.aval)[key.ival] = std::move(rhs);
    } else if (auto* dot = dynamic_cast<const DotFieldNode*>(&lhs)) {
        DValue base = eval(*dot->base);
        if (base.type != DValue::Type::Tuple)
            throw std::runtime_error("field assignment on non-tuple");
        for (auto& e : *base.tval) {
            if (e.name == dot->field) {
                e.value = std::move(rhs);
                return;
            }
        }
        throw std::runtime_error(std::format("tuple has no field '{}'", dot->field));
    } else if (auto* di = dynamic_cast<const DotIntNode*>(&lhs)) {
        DValue base = eval(*di->base);
        if (base.type != DValue::Type::Tuple)
            throw std::runtime_error("dot-int assignment on non-tuple");
        if (di->index < 1 || di->index > static_cast<long long>(base.tval->size()))
            throw std::runtime_error("tuple index out of range");
        (*base.tval)[di->index - 1].value = std::move(rhs);
    } else {
        throw std::runtime_error("invalid lvalue");
    }
}

void Interpreter::visit(const IfNode& n) {
    if (eval(*n.cond).is_truthy())
        n.then_body->accept(*this);
    else if (n.else_body)
        n.else_body->accept(*this);
}

void Interpreter::visit(const IfShortNode& n) {
    if (eval(*n.cond).is_truthy())
        n.stmt->accept(*this);
}

void Interpreter::visit(const WhileNode& n) {
    while (eval(*n.cond).is_truthy()) {
        try {
            n.body->accept(*this);
        } catch (ExitSignal&) {
            return;
        }
    }
}

void Interpreter::visit(const ForRangeNode& n) {
    const long long from = eval(*n.from).ival;
    const long long to   = eval(*n.to).ival;

    push_frame(); // scope for iterator variable
    struct Guard {
        Interpreter& i;
        ~Guard() { i.pop_frame(); }
    } g{*this};

    for (long long v = from; v <= to; ++v) {
        if (!n.iter.empty())
            (*env_.back())[n.iter] = DValue::make_int(v);
        try {
            n.body->accept(*this);
        } catch (ExitSignal&) {
            return;
        }
        // ReturnSignal propagates through; Guard ensures iter frame is popped
    }
}

void Interpreter::visit(const ForIterNode& n) {
    DValue iterable = eval(*n.iterable);

    push_frame(); // scope for iterator variable
    struct Guard {
        Interpreter& i;
        ~Guard() { i.pop_frame(); }
    } g{*this};

    auto run_body = [&](DValue elem) {
        if (!n.iter.empty())
            (*env_.back())[n.iter] = std::move(elem);
        try {
            n.body->accept(*this);
            return true;
        } catch (ExitSignal&) {
            return false;
        }
    };

    if (iterable.type == DValue::Type::Array) {
        for (auto& [k, v] : *iterable.aval)
            if (!run_body(v))
                return;
    } else if (iterable.type == DValue::Type::Tuple) {
        for (auto& e : *iterable.tval)
            if (!run_body(e.value))
                return;
    } else {
        throw std::runtime_error("cannot iterate over non-array/tuple");
    }
}

void Interpreter::visit(const LoopInfNode& n) {
    while (true) {
        try {
            n.body->accept(*this);
        } catch (ExitSignal&) {
            return;
        }
    }
}

void Interpreter::visit(const ExitNode&) {
    throw ExitSignal{};
}
void Interpreter::visit(const ReturnNode& n) {
    throw ReturnSignal{n.value ? eval(*n.value) : DValue{}};
}

void Interpreter::visit(const PrintNode& n) {
    for (size_t i = 0; i < n.exprs.size(); ++i) {
        if (i > 0)
            out_ << ' ';
        out_ << eval(*n.exprs[i]).to_string();
    }
    out_ << '\n';
}

// ── Expressions ─────────────────────────────────────────────────────────────────

void Interpreter::visit(const IntLitNode& n) {
    val_ = DValue::make_int(n.value);
}
void Interpreter::visit(const RealLitNode& n) {
    val_ = DValue::make_real(n.value);
}
void Interpreter::visit(const StrLitNode& n) {
    val_ = DValue::make_str(n.value);
}
void Interpreter::visit(const BoolLitNode& n) {
    val_ = DValue::make_bool(n.value);
}
void Interpreter::visit(const NoneLitNode&) {
    val_ = {};
}

void Interpreter::visit(const IdentNode& n) {
    val_ = (*env_[env_.size() - 1 - n.resolved_depth])[n.ident_name];
}

void Interpreter::visit(const FuncLitNode& n) {
    val_ = DValue::make_func(&n, capture_env());
}

void Interpreter::visit(const TypeNode&) {
    val_ = {};
}
void Interpreter::visit(const TupleElemNode& n) {
    val_ = eval(*n.expr);
}

void Interpreter::visit(const ArrayLitNode& n) {
    std::map<long long, DValue> m;
    long long key = 1;
    for (const auto& e : n.elems)
        m[key++] = eval(*e);
    val_ = DValue::make_array(std::move(m));
}

void Interpreter::visit(const TupleLitNode& n) {
    std::vector<TupleElem> elems;
    for (const auto& e : n.elems) {
        const auto& te = static_cast<const TupleElemNode&>(*e);
        elems.push_back(TupleElem{te.elem_name, eval(*te.expr)});
    }
    val_ = DValue::make_tuple(std::move(elems));
}

void Interpreter::visit(const IndexNode& n) {
    DValue base = eval(*n.base);
    DValue key  = eval(*n.index_expr);
    if (base.type != DValue::Type::Array)
        throw std::runtime_error("index on non-array");
    if (key.type != DValue::Type::Int)
        throw std::runtime_error("array index must be integer");
    auto it = base.aval->find(key.ival);
    if (it == base.aval->end())
        throw std::runtime_error(std::format("array key {} not found", key.ival));
    val_ = it->second;
}

void Interpreter::visit(const CallNode& n) {
    DValue callee = eval(*n.callee);
    std::vector<DValue> args;
    for (const auto& a : n.args)
        args.push_back(eval(*a));
    val_ = call_func(callee, std::move(args));
}

DValue Interpreter::call_func(const DValue& fv, std::vector<DValue> args) {
    if (fv.type != DValue::Type::Func)
        throw std::runtime_error("call on non-function");
    const FuncClosure& closure = *fv.fval;
    const FuncLitNode& fn      = *closure.node;

    Env saved = std::move(env_);
    env_      = closure.captured_env;
    push_frame(); // frame for parameters

    if (fn.params) {
        const auto& pl = static_cast<const ParamListNode&>(*fn.params);
        for (size_t i = 0; i < pl.params.size(); ++i) {
            const auto& ident                = static_cast<const IdentNode&>(*pl.params[i]);
            (*env_.back())[ident.ident_name] = i < args.size() ? std::move(args[i]) : DValue{};
        }
    }

    DValue result;
    try {
        fn.body->accept(*this); // BodyNode pushes/pops its own frame
    } catch (ReturnSignal& r) {
        result = std::move(r.value);
    }
    // env_ may have had frames popped by BodyNode guards on exception path,
    // but we unconditionally replace it anyway:
    env_ = std::move(saved);
    return result;
}

void Interpreter::visit(const DotFieldNode& n) {
    DValue base = eval(*n.base);
    if (base.type != DValue::Type::Tuple)
        throw std::runtime_error("dot field access on non-tuple");
    for (const auto& e : *base.tval)
        if (e.name == n.field) {
            val_ = e.value;
            return;
        }
    throw std::runtime_error(std::format("tuple has no field '{}'", n.field));
}

void Interpreter::visit(const DotIntNode& n) {
    DValue base = eval(*n.base);
    if (base.type != DValue::Type::Tuple)
        throw std::runtime_error("dot-int access on non-tuple");
    if (n.index < 1 || n.index > static_cast<long long>(base.tval->size()))
        throw std::runtime_error(std::format("tuple index {} out of range", n.index));
    val_ = (*base.tval)[n.index - 1].value;
}

void Interpreter::visit(const UnaryOpNode& n) {
    DValue v = eval(*n.operand);
    switch (n.op) {
    case UnaryOpNode::Op::UPLUS:
        val_ = (v.type == DValue::Type::Int) ? DValue::make_int(v.ival)
               : (v.type == DValue::Type::Real)
                   ? DValue::make_real(v.rval)
                   : (throw std::runtime_error("unary + on non-numeric"), DValue{});
        break;
    case UnaryOpNode::Op::UMINUS:
        val_ = (v.type == DValue::Type::Int) ? DValue::make_int(-v.ival)
               : (v.type == DValue::Type::Real)
                   ? DValue::make_real(-v.rval)
                   : (throw std::runtime_error("unary - on non-numeric"), DValue{});
        break;
    case UnaryOpNode::Op::NOT:
        val_ = DValue::make_bool(!v.is_truthy());
        break;
    }
}

void Interpreter::visit(const IsNode& n) {
    DValue v       = eval(*n.operand);
    const auto& tn = static_cast<const TypeNode&>(*n.type_node);
    bool ok        = false;
    switch (tn.type) {
    case TypeNode::Type::INT:
        ok = v.type == DValue::Type::Int;
        break;
    case TypeNode::Type::REAL:
        ok = v.type == DValue::Type::Real;
        break;
    case TypeNode::Type::BOOL:
        ok = v.type == DValue::Type::Bool;
        break;
    case TypeNode::Type::STRING:
        ok = v.type == DValue::Type::String;
        break;
    case TypeNode::Type::NONE:
        ok = v.type == DValue::Type::None;
        break;
    case TypeNode::Type::ARRAY:
        ok = v.type == DValue::Type::Array;
        break;
    case TypeNode::Type::TUPLE:
        ok = v.type == DValue::Type::Tuple;
        break;
    case TypeNode::Type::FUNC:
        ok = v.type == DValue::Type::Func;
        break;
    }
    val_ = DValue::make_bool(ok);
}

// ── Helper: floor division (spec: integer/integer rounds down) ─────────────────
static long long floor_div(long long a, long long b) {
    long long q = a / b;
    if (a % b != 0 && (a ^ b) < 0)
        --q; // adjust when signs differ
    return q;
}

// ── Numeric coercion helpers ───────────────────────────────────────────────────
static double to_real(const DValue& v) {
    if (v.type == DValue::Type::Int)
        return static_cast<double>(v.ival);
    if (v.type == DValue::Type::Real)
        return v.rval;
    throw std::runtime_error("expected numeric value");
}
static bool is_numeric(const DValue& v) {
    return v.type == DValue::Type::Int || v.type == DValue::Type::Real;
}
static bool is_mixed_real(const DValue& a, const DValue& b) {
    return is_numeric(a) && is_numeric(b) &&
           (a.type == DValue::Type::Real || b.type == DValue::Type::Real);
}

void Interpreter::visit(const BinOpNode& n) {
    using T  = DValue::Type;
    using Op = BinOpNode::Op;

    // Short-circuit logical operators
    if (n.op == Op::AND) {
        val_ = DValue::make_bool(eval(*n.left).is_truthy() && eval(*n.right).is_truthy());
        return;
    }
    if (n.op == Op::OR) {
        val_ = DValue::make_bool(eval(*n.left).is_truthy() || eval(*n.right).is_truthy());
        return;
    }
    if (n.op == Op::XOR) {
        val_ = DValue::make_bool(eval(*n.left).is_truthy() != eval(*n.right).is_truthy());
        return;
    }

    DValue L = eval(*n.left);
    DValue R = eval(*n.right);

    switch (n.op) {
    case Op::ADD:
        if (L.type == T::Int && R.type == T::Int)
            val_ = DValue::make_int(L.ival + R.ival);
        else if (is_mixed_real(L, R))
            val_ = DValue::make_real(to_real(L) + to_real(R));
        else if (L.type == T::String && R.type == T::String)
            val_ = DValue::make_str(L.sval + R.sval);
        else if (L.type == T::Array && R.type == T::Array) {
            std::map<long long, DValue> result = *L.aval;
            long long next                     = result.empty() ? 1 : result.rbegin()->first + 1;
            for (auto& [k, v] : *R.aval)
                result[next++] = v;
            val_ = DValue::make_array(std::move(result));
        } else if (L.type == T::Tuple && R.type == T::Tuple) {
            std::vector<TupleElem> elems = *L.tval;
            for (auto& e : *R.tval)
                elems.push_back(e);
            val_ = DValue::make_tuple(std::move(elems));
        } else
            throw std::runtime_error("invalid operands for +");
        break;

    case Op::SUB:
        if (L.type == T::Int && R.type == T::Int)
            val_ = DValue::make_int(L.ival - R.ival);
        else if (is_mixed_real(L, R))
            val_ = DValue::make_real(to_real(L) - to_real(R));
        else
            throw std::runtime_error("invalid operands for -");
        break;

    case Op::MUL:
        if (L.type == T::Int && R.type == T::Int)
            val_ = DValue::make_int(L.ival * R.ival);
        else if (is_mixed_real(L, R))
            val_ = DValue::make_real(to_real(L) * to_real(R));
        else
            throw std::runtime_error("invalid operands for *");
        break;

    case Op::DIV:
        if (L.type == T::Int && R.type == T::Int)
            val_ = DValue::make_int(floor_div(L.ival, R.ival));
        else if (is_mixed_real(L, R))
            val_ = DValue::make_real(to_real(L) / to_real(R));
        else
            throw std::runtime_error("invalid operands for /");
        break;

    // Comparisons
    case Op::LT:
        if (is_numeric(L) && is_numeric(R))
            val_ = DValue::make_bool(to_real(L) < to_real(R));
        else
            throw std::runtime_error("< requires numeric operands");
        break;
    case Op::LE:
        if (is_numeric(L) && is_numeric(R))
            val_ = DValue::make_bool(to_real(L) <= to_real(R));
        else
            throw std::runtime_error("<= requires numeric operands");
        break;
    case Op::GT:
        if (is_numeric(L) && is_numeric(R))
            val_ = DValue::make_bool(to_real(L) > to_real(R));
        else
            throw std::runtime_error("> requires numeric operands");
        break;
    case Op::GE:
        if (is_numeric(L) && is_numeric(R))
            val_ = DValue::make_bool(to_real(L) >= to_real(R));
        else
            throw std::runtime_error(">= requires numeric operands");
        break;

    case Op::EQ: {
        bool eq = false;
        if (L.type == T::None && R.type == T::None)
            eq = true;
        else if (L.type == T::None || R.type == T::None)
            eq = false;
        else if (L.type == T::Bool && R.type == T::Bool)
            eq = L.bval == R.bval;
        else if (L.type == T::String && R.type == T::String)
            eq = L.sval == R.sval;
        else if (is_numeric(L) && is_numeric(R)) {
            if (L.type == T::Int && R.type == T::Int)
                eq = L.ival == R.ival;
            else
                eq = to_real(L) == to_real(R);
        }
        val_ = DValue::make_bool(eq);
        break;
    }
    case Op::NEQ: {
        bool eq = false;
        if (L.type == T::None && R.type == T::None)
            eq = true;
        else if (L.type == T::None || R.type == T::None)
            eq = false;
        else if (L.type == T::Bool && R.type == T::Bool)
            eq = L.bval == R.bval;
        else if (L.type == T::String && R.type == T::String)
            eq = L.sval == R.sval;
        else if (is_numeric(L) && is_numeric(R)) {
            if (L.type == T::Int && R.type == T::Int)
                eq = L.ival == R.ival;
            else
                eq = to_real(L) == to_real(R);
        }
        val_ = DValue::make_bool(!eq);
        break;
    }
    default:
        break;
    }
}