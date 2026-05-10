#include "vm.hpp"

#include <format>
#include <stdexcept>

// ── Entry point ────────────────────────────────────────────────────────────────
void VM::run(const Module& mod) {
    exec(*mod.main, {}, {});
}

// ── ConstVal → VMValue ─────────────────────────────────────────────────────────
VMValue VM::from_const(const ConstVal& cv) {
    return std::visit(
        []<class T>(const T& v) -> VMValue {
            if constexpr (std::is_same_v<T, std::monostate>)       return VMValue::make_none();
            else if constexpr (std::is_same_v<T, long long>)        return VMValue::make_int(v);
            else if constexpr (std::is_same_v<T, double>)           return VMValue::make_real(v);
            else /* std::string */                                   return VMValue::make_str(v);
        },
        cv);
}

// ── Arithmetic helpers ─────────────────────────────────────────────────────────
static void require_numeric(const VMValue& a, const VMValue& b, const char* op) {
    bool ok = (a.type == VMValue::Type::Int  || a.type == VMValue::Type::Real) &&
              (b.type == VMValue::Type::Int  || b.type == VMValue::Type::Real);
    if (!ok)
        throw std::runtime_error(std::format("operator '{}' requires numeric operands", op));
}
static bool is_int(const VMValue& v) { return v.type == VMValue::Type::Int; }
static bool is_real(const VMValue& v) { return v.type == VMValue::Type::Real; }
static double to_real(const VMValue& v) {
    return is_int(v) ? static_cast<double>(v.ival) : v.rval;
}

VMValue VM::vm_add(const VMValue& a, const VMValue& b) {
    if (a.type == VMValue::Type::Int && b.type == VMValue::Type::Int)
        return VMValue::make_int(a.ival + b.ival);
    if ((is_int(a) || is_real(a)) && (is_int(b) || is_real(b)))
        return VMValue::make_real(to_real(a) + to_real(b));
    if (a.type == VMValue::Type::String && b.type == VMValue::Type::String)
        return VMValue::make_str(a.sval + b.sval);
    if (a.type == VMValue::Type::Array && b.type == VMValue::Type::Array) {
        std::map<long long, VMValue> result = *a.aval;
        long long next = result.empty() ? 1 : result.rbegin()->first + 1;
        for (auto& [k, v] : *b.aval)
            result[next++] = v;
        return VMValue::make_array(std::move(result));
    }
    if (a.type == VMValue::Type::Tuple && b.type == VMValue::Type::Tuple) {
        std::vector<VMTupleElem> elems = *a.tval;
        for (auto& e : *b.tval)
            elems.push_back(e);
        return VMValue::make_tuple(std::move(elems));
    }
    throw std::runtime_error("invalid operands for +");
}
VMValue VM::vm_sub(const VMValue& a, const VMValue& b) {
    require_numeric(a, b, "-");
    if (is_int(a) && is_int(b)) return VMValue::make_int(a.ival - b.ival);
    return VMValue::make_real(to_real(a) - to_real(b));
}
VMValue VM::vm_mul(const VMValue& a, const VMValue& b) {
    require_numeric(a, b, "*");
    if (is_int(a) && is_int(b)) return VMValue::make_int(a.ival * b.ival);
    return VMValue::make_real(to_real(a) * to_real(b));
}
VMValue VM::vm_div(const VMValue& a, const VMValue& b) {
    require_numeric(a, b, "/");
    if (is_int(a) && is_int(b)) {
        if (b.ival == 0)
            throw std::runtime_error("division by zero");
        // Floor division (rounds toward negative infinity)
        long long q = a.ival / b.ival;
        if ((a.ival ^ b.ival) < 0 && q * b.ival != a.ival)
            --q;
        return VMValue::make_int(q);
    }
    double db = to_real(b);
    if (db == 0.0)
        throw std::runtime_error("division by zero");
    return VMValue::make_real(to_real(a) / db);
}

// ── Comparison helpers ─────────────────────────────────────────────────────────
VMValue VM::vm_eq(const VMValue& a, const VMValue& b) {
    if (a.type != b.type) {
        // Int/Real cross-comparison
        if ((is_int(a) || is_real(a)) && (is_int(b) || is_real(b)))
            return VMValue::make_bool(to_real(a) == to_real(b));
        return VMValue::make_bool(false);
    }
    switch (a.type) {
    case VMValue::Type::None:   return VMValue::make_bool(true);
    case VMValue::Type::Int:    return VMValue::make_bool(a.ival == b.ival);
    case VMValue::Type::Real:   return VMValue::make_bool(a.rval == b.rval);
    case VMValue::Type::Bool:   return VMValue::make_bool(a.bval == b.bval);
    case VMValue::Type::String: return VMValue::make_bool(a.sval == b.sval);
    default:                    return VMValue::make_bool(a.fval == b.fval);
    }
}
VMValue VM::vm_neq(const VMValue& a, const VMValue& b) {
    return VMValue::make_bool(!vm_eq(a, b).bval);
}
VMValue VM::vm_lt(const VMValue& a, const VMValue& b) {
    require_numeric(a, b, "<");
    if (is_int(a) && is_int(b)) return VMValue::make_bool(a.ival < b.ival);
    return VMValue::make_bool(to_real(a) < to_real(b));
}
VMValue VM::vm_le(const VMValue& a, const VMValue& b) {
    require_numeric(a, b, "<=");
    if (is_int(a) && is_int(b)) return VMValue::make_bool(a.ival <= b.ival);
    return VMValue::make_bool(to_real(a) <= to_real(b));
}
VMValue VM::vm_gt(const VMValue& a, const VMValue& b) {
    require_numeric(a, b, ">");
    if (is_int(a) && is_int(b)) return VMValue::make_bool(a.ival > b.ival);
    return VMValue::make_bool(to_real(a) > to_real(b));
}
VMValue VM::vm_ge(const VMValue& a, const VMValue& b) {
    require_numeric(a, b, ">=");
    if (is_int(a) && is_int(b)) return VMValue::make_bool(a.ival >= b.ival);
    return VMValue::make_bool(to_real(a) >= to_real(b));
}

// ── Logical helpers ────────────────────────────────────────────────────────────
static void require_bool(const VMValue& a, const char* op) {
    if (a.type != VMValue::Type::Bool)
        throw std::runtime_error(
            std::format("operator '{}' requires boolean operands", op));
}
VMValue VM::vm_and(const VMValue& a, const VMValue& b) {
    require_bool(a, "and"); require_bool(b, "and");
    return VMValue::make_bool(a.bval && b.bval);
}
VMValue VM::vm_or(const VMValue& a, const VMValue& b) {
    require_bool(a, "or"); require_bool(b, "or");
    return VMValue::make_bool(a.bval || b.bval);
}
VMValue VM::vm_xor(const VMValue& a, const VMValue& b) {
    require_bool(a, "xor"); require_bool(b, "xor");
    return VMValue::make_bool(a.bval != b.bval);
}

// ── Unary helpers ──────────────────────────────────────────────────────────────
VMValue VM::vm_uminus(const VMValue& a) {
    if (is_int(a))  return VMValue::make_int(-a.ival);
    if (is_real(a)) return VMValue::make_real(-a.rval);
    throw std::runtime_error("unary '-' requires numeric operand");
}
VMValue VM::vm_uplus(const VMValue& a) {
    if (is_int(a) || is_real(a)) return a;
    throw std::runtime_error("unary '+' requires numeric operand");
}
VMValue VM::vm_not(const VMValue& a) {
    if (a.type == VMValue::Type::Bool) return VMValue::make_bool(!a.bval);
    throw std::runtime_error("'not' requires boolean operand");
}

bool VM::vm_istype(const VMValue& a, TypeTag tag) {
    switch (tag) {
    case TypeTag::Int:   return a.type == VMValue::Type::Int;
    case TypeTag::Real:  return a.type == VMValue::Type::Real;
    case TypeTag::Bool:  return a.type == VMValue::Type::Bool;
    case TypeTag::Str:   return a.type == VMValue::Type::String;
    case TypeTag::None:  return a.type == VMValue::Type::None;
    case TypeTag::Array: return a.type == VMValue::Type::Array;
    case TypeTag::Tuple: return a.type == VMValue::Type::Tuple;
    case TypeTag::Func:  return a.type == VMValue::Type::Func;
    }
    return false;
}

// ── Field access helpers ───────────────────────────────────────────────────────
VMValue VM::tuple_get_field(const VMValue& base, const ConstVal& key) {
    if (base.type != VMValue::Type::Tuple)
        throw std::runtime_error("field access on non-tuple");
    return std::visit(
        [&](const auto& k) -> VMValue {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, std::string>) {
                // Named field access
                for (const auto& e : *base.tval)
                    if (e.name == k)
                        return e.value;
                throw std::runtime_error(
                    std::format("tuple has no field '{}'", k));
            } else if constexpr (std::is_same_v<T, long long>) {
                // Positional (1-based) access
                if (k < 1 || k > static_cast<long long>(base.tval->size()))
                    throw std::runtime_error(
                        std::format("tuple index {} out of range", k));
                return (*base.tval)[static_cast<size_t>(k - 1)].value;
            } else {
                throw std::runtime_error("invalid field key type");
            }
        },
        key);
}

void VM::tuple_set_field(VMValue& base, const ConstVal& key, const VMValue& val) {
    if (base.type != VMValue::Type::Tuple)
        throw std::runtime_error("field assignment on non-tuple");
    std::visit(
        [&](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, std::string>) {
                for (auto& e : *base.tval)
                    if (e.name == k) { e.value = val; return; }
                throw std::runtime_error(
                    std::format("tuple has no field '{}'", k));
            } else if constexpr (std::is_same_v<T, long long>) {
                if (k < 1 || k > static_cast<long long>(base.tval->size()))
                    throw std::runtime_error(
                        std::format("tuple index {} out of range", k));
                (*base.tval)[static_cast<size_t>(k - 1)].value = val;
            } else {
                throw std::runtime_error("invalid field key type");
            }
        },
        key);
}

// ── Main execution loop ────────────────────────────────────────────────────────
VMValue VM::exec(const Proto&                                proto,
                 std::vector<VMValue>                        args,
                 const std::vector<std::shared_ptr<VMValue>>& upvals_in) {
    // ── Frame setup ────────────────────────────────────────────────────────────
    std::vector<VMValue>                   regs(static_cast<size_t>(proto.regs));
    std::vector<std::shared_ptr<VMValue>>  cells(static_cast<size_t>(proto.cells));
    for (auto& c : cells)
        c = std::make_shared<VMValue>();

    // Copy arguments into parameter registers
    for (int i = 0; i < proto.params && i < static_cast<int>(args.size()); ++i)
        regs[static_cast<size_t>(i)] = std::move(args[static_cast<size_t>(i)]);

    // Upvalues come from the closure
    const std::vector<std::shared_ptr<VMValue>>& upvals = upvals_in;

    // ── Dispatch loop ──────────────────────────────────────────────────────────
    int pc = 0;
    const int code_size = static_cast<int>(proto.code.size());

    while (pc < code_size) {
        const Instr& ins = proto.code[static_cast<size_t>(pc++)];

        switch (ins.op) {

        // ── Loads ──────────────────────────────────────────────────────────────
        case Opc::LOADK:
            regs[ins.a] = from_const(proto.consts[ins.b]);
            break;
        case Opc::LOADBOOL:
            regs[ins.a] = VMValue::make_bool(ins.b != 0);
            break;
        case Opc::LOADNONE:
            regs[ins.a] = VMValue::make_none();
            break;
        case Opc::MOVE:
            regs[ins.a] = regs[ins.b];
            break;

        // ── Upvalue access ─────────────────────────────────────────────────────
        case Opc::GETUPVAL:
            regs[ins.a] = *upvals[ins.b];
            break;
        case Opc::SETUPVAL:
            *upvals[ins.a] = regs[ins.b];
            break;

        // ── Cell-local access ──────────────────────────────────────────────────
        case Opc::LOADCELL:
            regs[ins.a] = *cells[ins.b];
            break;
        case Opc::STORECELL:
            *cells[ins.a] = regs[ins.b];
            break;

        // ── Binary arithmetic ──────────────────────────────────────────────────
        case Opc::ADD:  regs[ins.a] = vm_add(regs[ins.b], regs[ins.c]); break;
        case Opc::SUB:  regs[ins.a] = vm_sub(regs[ins.b], regs[ins.c]); break;
        case Opc::MUL:  regs[ins.a] = vm_mul(regs[ins.b], regs[ins.c]); break;
        case Opc::DIV:  regs[ins.a] = vm_div(regs[ins.b], regs[ins.c]); break;

        // ── Comparison ─────────────────────────────────────────────────────────
        case Opc::EQ:   regs[ins.a] = vm_eq (regs[ins.b], regs[ins.c]); break;
        case Opc::NEQ:  regs[ins.a] = vm_neq(regs[ins.b], regs[ins.c]); break;
        case Opc::LT:   regs[ins.a] = vm_lt (regs[ins.b], regs[ins.c]); break;
        case Opc::LE:   regs[ins.a] = vm_le (regs[ins.b], regs[ins.c]); break;
        case Opc::GT:   regs[ins.a] = vm_gt (regs[ins.b], regs[ins.c]); break;
        case Opc::GE:   regs[ins.a] = vm_ge (regs[ins.b], regs[ins.c]); break;

        // ── Logical ────────────────────────────────────────────────────────────
        case Opc::AND:  regs[ins.a] = vm_and(regs[ins.b], regs[ins.c]); break;
        case Opc::OR:   regs[ins.a] = vm_or (regs[ins.b], regs[ins.c]); break;
        case Opc::XOR:  regs[ins.a] = vm_xor(regs[ins.b], regs[ins.c]); break;

        // ── Unary ──────────────────────────────────────────────────────────────
        case Opc::UMINUS: regs[ins.a] = vm_uminus(regs[ins.b]); break;
        case Opc::UPLUS:  regs[ins.a] = vm_uplus (regs[ins.b]); break;
        case Opc::NOT:    regs[ins.a] = vm_not   (regs[ins.b]); break;

        // ── Type predicate ─────────────────────────────────────────────────────
        case Opc::ISTYPE:
            regs[ins.a] = VMValue::make_bool(
                vm_istype(regs[ins.b], static_cast<TypeTag>(ins.c)));
            break;

        // ── Control flow ───────────────────────────────────────────────────────
        case Opc::JMP:
            pc = ins.b;
            break;
        case Opc::JMPT:
            if (regs[ins.a].is_truthy())
                pc = ins.b;
            break;
        case Opc::JMPF:
            if (!regs[ins.a].is_truthy())
                pc = ins.b;
            break;

        // ── Collection construction ────────────────────────────────────────────
        case Opc::NEWARRAY: {
            std::map<long long, VMValue> m;
            for (int i = 0; i < ins.c; ++i)
                m[i + 1] = regs[ins.b + i];
            regs[ins.a] = VMValue::make_array(std::move(m));
            break;
        }
        case Opc::NEWTUPLE: {
            std::vector<VMTupleElem> elems;
            elems.reserve(static_cast<size_t>(ins.c));
            for (int i = 0; i < ins.c; ++i) {
                std::string name;
                const ConstVal& cv = proto.consts[ins.d + i];
                if (auto* s = std::get_if<std::string>(&cv))
                    name = *s;
                elems.push_back({std::move(name), regs[ins.b + i]});
            }
            regs[ins.a] = VMValue::make_tuple(std::move(elems));
            break;
        }
        case Opc::GETINDEX: {
            const VMValue& base = regs[ins.b];
            const VMValue& key  = regs[ins.c];
            if (base.type != VMValue::Type::Array)
                throw std::runtime_error("index on non-array");
            if (key.type != VMValue::Type::Int)
                throw std::runtime_error("array index must be integer");
            auto it = base.aval->find(key.ival);
            if (it == base.aval->end())
                throw std::runtime_error(
                    std::format("array key {} not found", key.ival));
            regs[ins.a] = it->second;
            break;
        }
        case Opc::SETINDEX: {
            VMValue& base      = regs[ins.a];
            const VMValue& key = regs[ins.b];
            const VMValue& val = regs[ins.c];
            if (base.type != VMValue::Type::Array)
                throw std::runtime_error("index assignment on non-array");
            if (key.type != VMValue::Type::Int)
                throw std::runtime_error("array index must be integer");
            (*base.aval)[key.ival] = val;
            break;
        }
        case Opc::GETFIELD: {
            regs[ins.a] = tuple_get_field(regs[ins.b], proto.consts[ins.c]);
            break;
        }
        case Opc::SETFIELD: {
            tuple_set_field(regs[ins.a], proto.consts[ins.b], regs[ins.c]);
            break;
        }

        // ── Closure creation ───────────────────────────────────────────────────
        case Opc::CLOSURE: {
            const auto& sub_proto = proto.protos[ins.b];
            auto        cl        = std::make_shared<VMClosure>();
            cl->proto             = sub_proto;
            cl->upvals.reserve(sub_proto->upvals.size());
            for (const auto& uv : sub_proto->upvals) {
                if (uv.is_local) {
                    // Capture a cell from the current frame
                    cl->upvals.push_back(cells[uv.idx]);
                } else {
                    // Re-capture an upvalue from the current closure
                    cl->upvals.push_back(upvals[uv.idx]);
                }
            }
            regs[ins.a] = VMValue::make_func(std::move(cl));
            break;
        }

        // ── Function call ──────────────────────────────────────────────────────
        case Opc::CALL: {
            const VMValue& fv = regs[ins.b];
            if (fv.type != VMValue::Type::Func)
                throw std::runtime_error("call on non-function");
            const VMClosure& cl = *fv.fval;
            std::vector<VMValue> call_args;
            call_args.reserve(static_cast<size_t>(ins.d));
            for (int i = 0; i < ins.d; ++i)
                call_args.push_back(regs[ins.c + i]);
            regs[ins.a] = exec(*cl.proto, std::move(call_args), cl.upvals);
            break;
        }

        // ── Return ─────────────────────────────────────────────────────────────
        case Opc::RETURN:
            return regs[ins.a];
        case Opc::RETURNNONE:
            return VMValue{};

        // ── Iterator ───────────────────────────────────────────────────────────
        case Opc::ITERINIT: {
            const VMValue& src = regs[ins.b];
            auto state = std::make_shared<VMIterState>();
            if (src.type == VMValue::Type::Array) {
                for (auto& [k, v] : *src.aval)
                    state->items.push_back(v);
            } else if (src.type == VMValue::Type::Tuple) {
                for (auto& e : *src.tval)
                    state->items.push_back(e.value);
            } else {
                throw std::runtime_error(
                    "cannot iterate over non-array/tuple value");
            }
            regs[ins.a] = VMValue::make_iter(std::move(state));
            break;
        }
        case Opc::ITERNEXT: {
            VMIterState& state = *regs[ins.b].iter;
            if (!state.has_next()) {
                pc = ins.c;
            } else {
                regs[ins.a] = state.next();
            }
            break;
        }

        // ── I/O ────────────────────────────────────────────────────────────────
        case Opc::PRINT: {
            for (int i = 0; i < ins.b; ++i) {
                if (i > 0) out_ << ' ';
                out_ << regs[ins.a + i].to_string();
            }
            out_ << '\n';
            break;
        }

        case Opc::NOP:
            break;
        } // switch
    } // while

    return VMValue{};
}
