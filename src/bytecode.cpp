#include "bytecode.hpp"

namespace sm {

std::ostream& operator<<(std::ostream& os, const Type& type) {
    switch (type) {
    case Type::None:
        os << "NONE";
        break;
    case Type::Int:
        os << "INT";
        break;
    case Type::Real:
        os << "REAL";
        break;
    case Type::Bool:
        os << "BOOL";
        break;
    case Type::String:
        os << "STRING";
        break;
    case Type::Array:
        os << "ARRAY";
        break;
    case Type::Tuple:
        os << "TUPLE";
        break;
    case Type::Func:
        os << "FUNC";
        break;
    case Type::Ref:
        os << "REF";
        break;
    }
    return os;
}

uint32_t packLock(Location loc) {
    return std::bit_cast<uint32_t>(loc);
}

Location unpackLock(uint32_t data) {
    return std::bit_cast<Location>(data);
}

std::ostream& operator<<(std::ostream& os, const Location& loc) {
    switch (loc.type) {
    case LOCAL:
        os << "Loc(" << loc.index << ")";
        break;
    case ARGUMENT:
        os << "Arg(" << loc.index << ")";
        break;
    case CAPTURED:
        os << "Capture(" << loc.index << ")";
        break;
    }
    return os;
}

} // namespace sm
