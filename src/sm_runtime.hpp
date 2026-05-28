#pragma once

#include "bytecode.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <stack>

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

using DArray = std::map<int, DValue*>; // Array dvalues should have Ref type

struct DTuple {
    const TupleScheme* scheme;
    DValue* elements[0];
};

struct DFunc {
    const FunctionScheme* scheme;
    DValue* capture[0];
};

DValue* make_none();
DValue* make_int(int value);
DValue* make_real(float value);
DValue* make_bool(bool value);
DValue* make_string(const std::string& string);
DValue* make_array(const DArray& elements);
DValue* make_tuple(const TupleScheme* scheme, const std::vector<DValue*>& elements);
DValue* make_function(const FunctionScheme* scheme, const std::vector<DValue*>& captured);
DValue* make_ref(DValue* ref);

DValue* resolve_ref(DValue* val);
void change_ref(DValue* ref, DValue* val);

float get_float(const DValue& value);

struct Frame;

struct Runtime {
    std::stack<Frame> stack;
    DValue* none_obj = new DValue();

    void print(DValue* value, std::ostream& out);
};

struct Frame {
    const FunctionScheme* scheme;
    size_t return_index;
    std::vector<DValue*> args;
    std::vector<DValue*> locals;
    std::vector<DValue*> captured;
    std::vector<DValue*> stack;

    void push(DValue* value);
    DValue* pop();

    Frame(const Runtime* runtime, const FunctionScheme* scheme, size_t return_index,
          const std::vector<DValue*>& arg_objs = {}, const std::vector<DValue*> capture = {})
        : scheme(scheme),
          return_index(return_index),
          args(scheme->args_number),
          locals(scheme->locals_number),
          captured(capture) {

        for (size_t loc_id = 0; loc_id < scheme->locals_number; ++loc_id) {
            locals[loc_id] = make_ref(runtime->none_obj);
        }

        for (size_t arg_id = 0; arg_id < scheme->args_number; ++arg_id) {
            args[arg_id] = make_ref(arg_objs[arg_id]);
        }
    };

    // DValue*& operator[](Location loc);
    DValue* operator[](Location loc);
};

} // namespace sm
