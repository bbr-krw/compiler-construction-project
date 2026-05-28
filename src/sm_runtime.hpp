#pragma once

#include "bytecode.hpp"
#include "gc_heap.hpp"

#include <cstddef>
#include <cstring>
#include <stack>
#include <vector>

namespace sm {

using namespace gc;

HeapObject* resolve_ref(HeapObject* val);
void change_ref(HeapObject* ref, HeapObject* val);

float get_float(HeapObject* value);

struct Frame;

struct Runtime {
    const BcFile* bc_file;
    Heap heap{1024 * 1024 * 100}; // 100 MB heap
    std::stack<Frame> stack;
    HeapObject* none_obj = new HeapObject();

    void print(HeapObject* value, std::ostream& out);

    HeapObject* concat(HeapObject* first, HeapObject* second);
};

struct Frame {
    const FunctionScheme* scheme;
    size_t return_index;
    std::vector<DRef*> args;
    std::vector<DRef*> locals;
    std::vector<DRef*> captured;
    std::vector<HeapObject*> stack;

    void push(HeapObject* value);
    HeapObject* pop();

    Frame(Runtime* runtime, const FunctionScheme* scheme, size_t return_index,
          const std::vector<HeapObject*>& arg_objs = {}, const std::vector<DRef*>& capture = {})
        : scheme(scheme),
          return_index(return_index),
          args(scheme->args_number),
          locals(scheme->locals_number),
          captured(capture) {

        for (size_t loc_id = 0; loc_id < scheme->locals_number; ++loc_id) {
            locals[loc_id] = runtime->heap.make_ref(runtime->heap.make_none());
        }

        for (size_t arg_id = 0; arg_id < scheme->args_number; ++arg_id) {
            args[arg_id] = runtime->heap.make_ref(arg_objs[arg_id]);
        }
    };

    // DValue*& operator[](Location loc);
    HeapObject* operator[](Location loc);
};

} // namespace sm
