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
    Heap heap{1024 * 1024 * 10}; // 10 MB heap
    std::deque<Frame> stack;

    void print(HeapObject* value, std::ostream& out);

    HeapObject* concat(HeapObject* first, HeapObject* second);
};

struct Frame {
    Runtime* runtime;
    const FunctionScheme* scheme;
    size_t return_index;
    std::vector<DRef*> args;
    std::vector<DRef*> locals;
    std::vector<DRef*> captured;
    std::deque<HeapObject*> stack;

    void push(HeapObject* value);
    HeapObject* pop();

    Frame(Runtime* runtime, const FunctionScheme* scheme, size_t return_index,
          const std::vector<HeapObject*>& arg_objs = {}, const std::vector<DRef*>& capture = {})
        : runtime(runtime),
          scheme(scheme),
          return_index(return_index),
          args(scheme->args_number),
          locals(scheme->locals_number),
          captured(capture) {

        for (size_t loc_id = 0; loc_id < scheme->locals_number; ++loc_id) {
            locals[loc_id] = runtime->heap.make_ref(runtime->heap.make_none());
            DRef** ref     = &locals[loc_id];
            runtime->heap.roots.push_back((HeapObject**)(ref));
        }

        for (size_t arg_id = 0; arg_id < scheme->args_number; ++arg_id) {
            args[arg_id] = runtime->heap.make_ref(arg_objs[arg_id]);
            runtime->heap.roots.push_back((HeapObject**)&args[arg_id]);
        }

        for (size_t capture_id = 0; capture_id < capture.size(); ++capture_id) {
            DRef** ref = &captured[capture_id];
            runtime->heap.roots.push_back((HeapObject**)ref);
        }
    };

    ~Frame() {
        for (DRef* _ : captured) {
            runtime->heap.roots.pop_back();
        }
        for (DRef* _ : locals) {
            runtime->heap.roots.pop_back();
        }
        for (DRef* _ : args) {
            runtime->heap.roots.pop_back();
        }
    }

    // DValue*& operator[](Location loc);
    HeapObject* operator[](Location loc);
};

} // namespace sm
