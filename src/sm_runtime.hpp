#pragma once

#include "bytecode.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace sm {

struct DValue {
    Type type{Type::None};
    bool mark;

    uint64_t value;
};

template <typename T> struct __attribute__((packed)) Segment {
    size_t capacity;
    T data[0];
};

using DString = Segment<char>;

using DArray = std::map<int, DValue*>;

struct DTuple {
    const TupleScheme* scheme;
    DValue* elements[0];
};

struct DFunc {
    const FunctionScheme* scheme;
    DValue** capture[0];
};

DValue* make_none();
DValue* make_int(int value);
DValue* make_real(float value);
DValue* make_bool(bool value);
DValue* make_string(const std::string& string);
DValue* make_array(const DArray& elements);
DValue* make_tuple(const TupleScheme* scheme, const std::vector<DValue*>& elements);
DValue* make_function(const FunctionScheme* scheme, const std::vector<DValue**>& captured);

float get_float(const DValue& value);

struct Frame {
    const FunctionScheme* scheme;
    size_t return_index;
    std::vector<DValue**> captured;
    std::vector<DValue*> args;
    std::vector<DValue*> locals;
    std::vector<DValue*> stack;

    void push(DValue* value);
    DValue* pop();

    DValue*& operator[](Location loc);
};

struct Runtime {
    std::vector<Frame> stack;

    void print(DValue* value, std::ostream& out);
};

} // namespace sm
