#include "sm_runtime.hpp"

#include "bytecode.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace sm {

template <typename T> Segment<T>* make_segment(size_t n) {
    size_t segment_size = sizeof(size_t) + n * sizeof(T);
    auto ptr            = new uint8_t[segment_size];
    return reinterpret_cast<Segment<T>*>(ptr);
}

DValue* make_none() {
    return new DValue{.type = Type::None, .mark = false, .value = 0};
}

DValue* make_int(int value) {
    return new DValue{.type = Type::Int, .mark = false, .value = static_cast<uint64_t>(value)};
}

DValue* make_real(float value) {
    return new DValue{.type = Type::Real, .mark = false, .value = float_to_raw(value)};
}

DValue* make_bool(bool value) {
    return new DValue{.type = Type::Bool, .mark = false, .value = static_cast<uint64_t>(value)};
}

DValue* make_string(const std::string& string) {
    auto segment = make_segment<char>(1 + string.size());
    std::strcpy(segment->data, string.c_str());
    return new DValue{
        .type = Type::String, .mark = false, .value = reinterpret_cast<uint64_t>(segment)};
}

DValue* make_array(const DArray& elements) {
    auto array = new DArray{elements};
    return new DValue{
        .type = Type::Array, .mark = false, .value = reinterpret_cast<uint64_t>(array)};
}

DValue* make_tuple(const TupleScheme* scheme, const std::vector<DValue*>& elements) {
    size_t tuple_size = sizeof(DTuple) + elements.size() * sizeof(DValue);
    auto tuple        = reinterpret_cast<DTuple*>(new uint8_t[tuple_size]);
    tuple->scheme     = scheme;
    for (size_t i = 0; i < elements.size(); i++) {
        tuple->elements[i] = elements[i];
    }
    return new DValue{
        .type = Type::Tuple, .mark = false, .value = reinterpret_cast<uint64_t>(tuple)};
}

DValue* make_function(const FunctionScheme* scheme, const std::vector<DValue**>& captured) {
    size_t func_size = sizeof(DFunc) + captured.size() * sizeof(DValue**);
    auto func        = reinterpret_cast<DFunc*>(new uint8_t[func_size]);
    func->scheme     = scheme;
    for (size_t i = 0; i < captured.size(); i++) {
        func->capture[i] = captured[i];
    }
    return new DValue{.type = Type::Func, .mark = false, .value = reinterpret_cast<uint64_t>(func)};
}

float get_float(const DValue& value) {
    switch (value.type) {
    case Type::Int:
        return static_cast<int>(value.value);
    case Type::Real:
        return raw_to_float(value.value);
    default:
        throw std::runtime_error("can't case value to float");
    }
}

void Frame::push(DValue* value) {
    stack.push_back(value);
}

DValue* Frame::pop() {
    DValue* value = stack.back();
    stack.pop_back();
    return value;
}

DValue*& Frame::operator[](Location loc) {
    switch (loc.type) {
    case LOCAL: {
        return locals[loc.index];
    }
    case ARGUMENT: {
        return args[loc.index];
    }
    case CAPTURED: {
        return *captured[loc.index];
    }
    }
    __builtin_unreachable();
}

void Runtime::print(DValue* value, std::ostream& os) {
    switch (value->type) {
    case Type::None: {
        os << "none";
        break;
    }

    case Type::Int: {
        os << static_cast<int>(value->value);
        break;
    }

    case Type::Real: {
        os << raw_to_float(value->value);
        break;
    }

    case Type::Bool: {
        if (value->value) {
            os << "true";
        } else {
            os << "false";
        }
        break;
    }

    case Type::String: {
        auto string = reinterpret_cast<DString*>(value->value);
        os << string->data;
        break;
    }

    case Type::Array: {
        auto array = reinterpret_cast<DArray*>(value->value);
        os << "[";
        size_t i = 0;
        for (const auto& [key, value] : *array) {
            if (i > 0) {
                os << ", ";
            }
            i++;
            // os << key << ":";
            print(value, os);
        }
        os << "]";
        break;
    }

    case Type::Tuple: {
        auto tuple = reinterpret_cast<DTuple*>(value->value);
        os << "{";
        for (size_t i = 0; i < tuple->scheme->field_names.size(); i++) {
            if (i > 0) {
                os << ", ";
            }

            if (tuple->scheme->field_names[i].has_value()) {
                os << tuple->scheme->field_names[i].value();
                os << " := ";
            }
            print(tuple->elements[i], os);
        }
        os << "}";
        break;
    }

    case Type::Func: {
        os << "__func__";
    }
    }
}

} // namespace sm
