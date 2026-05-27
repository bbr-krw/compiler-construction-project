#include "runtime.hpp"

#include <string>

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
