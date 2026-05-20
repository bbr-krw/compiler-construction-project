#pragma once

#include "sm_runtime.hpp"
#include "bytecode.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace sm {

template <typename T>
Segment<T>* make_segment(size_t n) {
    size_t segment_size = sizeof(size_t) + n * sizeof(T);
    auto ptr = new uint8_t[segment_size];
    return reinterpret_cast<Segment<T>*>(ptr);
}

DValue make_int(int value) {
    return DValue{.type = Type::Int, .mark = false, .value = static_cast<uint64_t>(value)};
}

DValue make_real(double value) {
    return DValue{.type = Type::Real, .mark = false, .value = static_cast<uint64_t>(value)};
}

DValue make_bool(bool value) {
    return DValue{.type = Type::Bool, .mark = false, .value = static_cast<uint64_t>(value)};
}

DValue make_string(const std::string& string) {
    auto segment = make_segment<char>(1 + string.size());
    std::strcpy(segment->data, string.c_str());
    return DValue{.type = Type::String, .mark = false, .value = reinterpret_cast<uint64_t>(segment)};
}

DValue make_array(size_t n) {
    auto array = new DArray{};
    return DValue{.type = Type::Array, .mark = false, .value = reinterpret_cast<uint64_t>(array)};
}

DValue make_tuple(const TupleScheme* scheme, const std::vector<DValue>& elements) {
    size_t tuple_size = sizeof(DTuple) + elements.size() * sizeof(DValue);
    auto tuple = reinterpret_cast<DTuple*>(new uint8_t[tuple_size]);
    tuple->scheme = scheme;
    for (size_t i = 0; i < elements.size(); i++) {
        tuple->elements[i] = elements[i];
    }
    return DValue{.type = Type::Tuple, .mark = false, .value = reinterpret_cast<uint64_t>(tuple)};
}

DValue make_function(const FunctionScheme* scheme, const std::vector<DValue>& captured) {
    size_t func_size = sizeof(DFunc) + captured.size() * sizeof(DValue);
    auto func = reinterpret_cast<DFunc*>(new uint8_t[func_size]);
    func->scheme = scheme;
    for (size_t i = 0; i < captured.size(); i++) {
        func->capture[i] = captured[i];
    }
    return DValue{.type = Type::Func, .mark = false, .value = reinterpret_cast<uint64_t>(func)};
}

void Runtime::call(DFunc func, void* return_addr, std::vector<DValue> args, size_t locals) {
    Frame frame;

    frame.return_addr = return_addr;
    frame.args = args;
    frame.locals.resize(locals);
    frame.captured.assign(func.capture, func.capture + func.scheme.capture.size());

    stack.push_back(frame);
}

void Runtime::print(DValue value) {
    switch (value.type) {
    case Type::None: {
        std::cout << "none";
        break;
    }

    case Type::Int: {
        std::cout << static_cast<int>(value.value);
        break;
    }

    case Type::Real: {
        std::cout << static_cast<double>(value.value);
        break;
    }

    case Type::Bool: {
        if (value.value) {
            std::cout << "true";
        } else {
            std::cout << "false";
        }
        break;
    }

    case Type::String: {
        auto string = reinterpret_cast<DString*>(value.value);
        std::cout << string->data;
        break;
    }

    case Type::Array: {
        auto array = reinterpret_cast<DArray*>(value.value);
        std::cout << "[";
        size_t i = 0;
        for (const auto& [key, value] : *array) {
            if (i > 0) {
                std::cout << ", ";
            }
            i++;
            std::cout << key << ":";
            print(value);
        }
        std::cout << "]";
        break;
    }

    case Type::Tuple: {
        auto tuple = reinterpret_cast<DTuple*>(value.value);
        std::cout << "{";
        for (size_t i = 0; i < tuple->capacity; i++) {
            if (i > 0) {
                std::cout << ", ";
            }
            print(tuple->data[i].key);
            std::cout << ":";
            print(tuple->data[i].value);
        }
        std::cout << "}";
        break;
    }

    case Type::Func: {
        std::cout << "__func__";
    }

    }
}

} // namespace sm
