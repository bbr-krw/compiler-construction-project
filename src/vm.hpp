#pragma once

#include "bytecode.hpp"

#include <cmath>
#include <format>
#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

// ── Runtime value ──────────────────────────────────────────────────────────────
// Separate from DValue so the VM is independent of the tree-walking interpreter.
// Forward declarations to break circular dependencies (same pattern as DValue).

struct VMTupleElem;  // defined after VMValue
struct VMClosure;    // defined after VMTupleElem
struct VMIterState;  // lightweight iterator state

struct VMValue {
    enum class Type { None, Int, Real, Bool, String, Array, Tuple, Func, Iter };
    Type type{Type::None};

    long long ival{};
    double    rval{};
    bool      bval{};
    std::string sval;
    std::shared_ptr<std::map<long long, VMValue>>  aval; // Array: int key → value
    std::shared_ptr<std::vector<VMTupleElem>>      tval; // Tuple elements
    std::shared_ptr<VMClosure>                     fval; // Function closure
    std::shared_ptr<VMIterState>                   iter; // Iterator state

    // ── Factory helpers ──────────────────────────────────────────────────────
    static VMValue make_none()                 { return {}; }
    static VMValue make_int(long long v)       { VMValue d; d.type=Type::Int;    d.ival=v; return d; }
    static VMValue make_real(double v)         { VMValue d; d.type=Type::Real;   d.rval=v; return d; }
    static VMValue make_bool(bool v)           { VMValue d; d.type=Type::Bool;   d.bval=v; return d; }
    static VMValue make_str(std::string v)     { VMValue d; d.type=Type::String; d.sval=std::move(v); return d; }

    static VMValue make_array(std::map<long long, VMValue> m) {
        VMValue d;
        d.type = Type::Array;
        d.aval = std::make_shared<std::map<long long, VMValue>>(std::move(m));
        return d;
    }

    // Defined out-of-line after VMTupleElem / VMClosure / VMIterState are complete:
    static VMValue make_tuple(std::vector<VMTupleElem> e);
    static VMValue make_func(std::shared_ptr<VMClosure> c);
    static VMValue make_iter(std::shared_ptr<VMIterState> s);

    std::string to_string() const;
    bool        is_truthy() const; // throws if not Bool
};

// ── VMTupleElem ────────────────────────────────────────────────────────────────
struct VMTupleElem {
    std::string name;  // empty for unnamed elements
    VMValue     value;
};

// ── VMClosure ──────────────────────────────────────────────────────────────────
// Immutable after creation; shared by all activations.
struct VMClosure {
    std::shared_ptr<Proto>                    proto;
    std::vector<std::shared_ptr<VMValue>>     upvals; // captured shared cells
};

// ── VMIterState ────────────────────────────────────────────────────────────────
struct VMIterState {
    std::vector<VMValue> items;
    int                  pos = 0;

    bool     has_next() const { return pos < static_cast<int>(items.size()); }
    VMValue  next()           { return items[pos++]; }
};

// ── Out-of-line factory definitions ───────────────────────────────────────────
inline VMValue VMValue::make_tuple(std::vector<VMTupleElem> e) {
    VMValue d;
    d.type = Type::Tuple;
    d.tval = std::make_shared<std::vector<VMTupleElem>>(std::move(e));
    return d;
}
inline VMValue VMValue::make_func(std::shared_ptr<VMClosure> c) {
    VMValue d;
    d.type = Type::Func;
    d.fval = std::move(c);
    return d;
}
inline VMValue VMValue::make_iter(std::shared_ptr<VMIterState> s) {
    VMValue d;
    d.type = Type::Iter;
    d.iter = std::move(s);
    return d;
}

// ── VMValue methods ────────────────────────────────────────────────────────────
inline std::string VMValue::to_string() const {
    switch (type) {
    case Type::None:   return "none";
    case Type::Int:    return std::to_string(ival);
    case Type::Bool:   return bval ? "true" : "false";
    case Type::String: return sval;
    case Type::Real:
        if (std::isfinite(rval) && rval == std::floor(rval))
            return std::to_string(static_cast<long long>(rval));
        return std::format("{:g}", rval);
    case Type::Array: {
        std::string s = "[";
        bool first = true;
        for (auto& [k, v] : *aval) {
            if (!first) s += ", ";
            s += v.to_string();
            first = false;
        }
        return s + "]";
    }
    case Type::Tuple: {
        std::string s = "{";
        bool first = true;
        for (auto& e : *tval) {
            if (!first) s += ", ";
            s += e.name.empty() ? e.value.to_string() : e.name + " := " + e.value.to_string();
            first = false;
        }
        return s + "}";
    }
    case Type::Func: return "<func>";
    case Type::Iter: return "<iter>";
    }
    return "";
}

inline bool VMValue::is_truthy() const {
    if (type == Type::Bool)
        return bval;
    throw std::runtime_error("non-boolean value used in boolean context");
}

// ── VM ─────────────────────────────────────────────────────────────────────────
// Executes a compiled Module.  The execution is recursive (each CALL creates
// a new C++ stack frame via VM::exec); this keeps the implementation simple.
class VM {
public:
    explicit VM(std::ostream& out) : out_(out) {}

    void run(const Module& mod);

private:
    std::ostream& out_;

    // Execute a proto with the given argument values and upvalues.
    // Returns the return value of the function.
    VMValue exec(const Proto& proto,
                 std::vector<VMValue>                      args,
                 const std::vector<std::shared_ptr<VMValue>>& upvals);

    // Convert a ConstVal to a VMValue.
    static VMValue from_const(const ConstVal& cv);

    // Arithmetic / comparison / logical helpers (throw on type mismatch).
    static VMValue vm_add(const VMValue& a, const VMValue& b);
    static VMValue vm_sub(const VMValue& a, const VMValue& b);
    static VMValue vm_mul(const VMValue& a, const VMValue& b);
    static VMValue vm_div(const VMValue& a, const VMValue& b);
    static VMValue vm_eq (const VMValue& a, const VMValue& b);
    static VMValue vm_neq(const VMValue& a, const VMValue& b);
    static VMValue vm_lt (const VMValue& a, const VMValue& b);
    static VMValue vm_le (const VMValue& a, const VMValue& b);
    static VMValue vm_gt (const VMValue& a, const VMValue& b);
    static VMValue vm_ge (const VMValue& a, const VMValue& b);
    static VMValue vm_and(const VMValue& a, const VMValue& b);
    static VMValue vm_or (const VMValue& a, const VMValue& b);
    static VMValue vm_xor(const VMValue& a, const VMValue& b);
    static VMValue vm_uminus(const VMValue& a);
    static VMValue vm_uplus (const VMValue& a);
    static VMValue vm_not   (const VMValue& a);
    static bool    vm_istype(const VMValue& a, TypeTag tag);

    // Field access helpers (string name or 1-based integer index for tuples).
    static VMValue tuple_get_field(const VMValue& base, const ConstVal& key);
    static void    tuple_set_field(VMValue& base, const ConstVal& key, const VMValue& val);
};
