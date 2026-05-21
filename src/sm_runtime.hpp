#pragma once

#include "bytecode.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace sm {

enum class Type { None, Int, Real, Bool, String, Array, Tuple, Func };

struct DValue {
    Type type{Type::None};
    bool mark;

    uint64_t value;
};

template <typename T>
struct __attribute__((packed)) Segment {
    size_t capacity;
    T data[0];
};

using DString = Segment<char>;

using DArray = std::unordered_map<int, DValue>;

struct DTuple {
    const TupleScheme* scheme;
    DValue elements[0];
};

struct DFunc {
    const FunctionScheme* scheme;
    DValue capture[0];
};

struct Memory {
    Segment<DValue>* stack;
    size_t stack_size{};
};

DValue make_int(int value);
DValue make_real(float value);
DValue make_bool(bool value);
DValue make_string(const std::string& string);
DValue make_array(const DArray& elements);
DValue make_tuple(const TupleScheme* scheme, const std::vector<DValue>& elements);
DValue make_function(const FunctionScheme* scheme, const std::vector<DValue>& captured);

struct Frame {
    const FunctionScheme* scheme;
    size_t return_index;
    std::vector<DValue> captured;
    std::vector<DValue> args;
    std::vector<DValue> locals;
    std::vector<DValue> stack;

    void push(DValue value);
    DValue pop();

    DValue& operator[](Location loc);
};

struct Runtime {
    Memory memory;
    std::vector<Frame> stack;

    void call(DFunc func, size_t return_index, std::vector<DValue> args, size_t locals);
    void print(DValue value);
};

} // namespace sm
