#include "sm_runtime.hpp"

#include "bytecode.hpp"

#include <iostream>
#include <stdexcept>

namespace sm {

using namespace gc;

float get_float(HeapObject* value) {
    switch (value->type) {
    case Type::Int: {
        return static_cast<float>(reinterpret_cast<DInt*>(value)->value);
    }
    case Type::Real: {
        return reinterpret_cast<DReal*>(value)->value;
    }
    default: {
        throw std::runtime_error("expected numeric value");
    }
    }
}

void Frame::push(HeapObject* value) {
    stack.push_back(value);
}

HeapObject* Frame::pop() {
    HeapObject* value = stack.back();
    stack.pop_back();
    return value;
}

HeapObject* resolve_ref(HeapObject* val) {
    while (val->type == Type::Ref) {
        val = reinterpret_cast<DRef*>(val)->ref;
    }
    return reinterpret_cast<HeapObject*>(val);
}

void change_ref(HeapObject* ref, HeapObject* val) {
    assert(ref->type == Type::Ref);
    reinterpret_cast<DRef*>(ref)->ref = val;
}

HeapObject* Frame::operator[](Location loc) {
    switch (loc.type) {
    case LOCAL: {
        return locals[loc.index];
    }
    case ARGUMENT: {
        return args[loc.index];
    }
    case CAPTURED: {
        return captured[loc.index];
    }
        __builtin_unreachable();
    }
}

void Runtime::print(HeapObject* value, std::ostream& os) {
    switch (value->type) {
    case Type::None: {
        os << "none";
        break;
    }

    case Type::Int: {
        os << static_cast<int>(reinterpret_cast<DInt*>(value)->value);
        break;
    }

    case Type::Real: {
        os << reinterpret_cast<DReal*>(value)->value;
        break;
    }

    case Type::Bool: {
        if (reinterpret_cast<DBool*>(value)->value) {
            os << "true";
        } else {
            os << "false";
        }
        break;
    }

    case Type::String: {
        auto string = reinterpret_cast<DString*>(value)->data;
        os << string;
        break;
    }

    case Type::Array: {
        auto darray = reinterpret_cast<DArray*>(value);
        os << "[";
        for (size_t i = 0; i < darray->data->size; i++) {
            auto [key, value] = darray->data->elements[i];
            if (i > 0) {
                os << ", ";
            }
            // os << key << ":";
            print(value, os);
        }
        os << "]";
        break;
    }

    case Type::Tuple: {
        auto tuple = reinterpret_cast<DTuple*>(value);
        os << "{";
        for (size_t i = 0; i < tuple->length; i++) {
            auto [name_index, value] = tuple->elements[i];
            if (i > 0) {
                os << ", ";
            }

            if (name_index != static_cast<size_t>(-1)) {
                os << bc_file->strings[name_index];
                os << " := ";
            }
            print(value, os);
        }
        os << "}";
        break;
    }

    case Type::Func: {
        os << "__func__";
        break;
    }

    case Type::Ref: {
        os << "__ref__";
        break;
    }
    }
}

HeapObject* Runtime::concat(HeapObject* first, HeapObject* second) {
    if (first->type != second->type) {
        throw std::runtime_error("cannot concat different types");
    }

    switch (first->type) {
    case Type::Ref:
    case Type::None:
    case Type::Int:
    case Type::Real:
    case Type::Bool:
    case Type::Func:
        throw std::runtime_error("unsupported type for concatenation");

    case Type::String: {
        DString* first_str  = reinterpret_cast<DString*>(first);
        DString* second_str = reinterpret_cast<DString*>(second);

        std::string result = first_str->data;
        result += second_str->data;

        return heap.make_string(result);
    }

    case Type::Array: {
        DArray* first_arr  = reinterpret_cast<DArray*>(first);
        DArray* second_arr = reinterpret_cast<DArray*>(second);

        std::vector<std::pair<size_t, DRef*>> array;

        size_t first_arr_max_key = std::numeric_limits<size_t>::min();
        for (const auto& [key, value] :
             std::span(first_arr->data->elements, first_arr->data->size)) {
            array.emplace_back(key, value);
            first_arr_max_key = std::max(first_arr_max_key, key);
        }
        for (const auto& [key, value] :
             std::span(second_arr->data->elements, second_arr->data->size)) {
            array.emplace_back(key + first_arr_max_key, value);
        }

        return heap.make_array(array);
    }

    case Type::Tuple: {
        DTuple* first_tuple  = reinterpret_cast<DTuple*>(first);
        DTuple* second_tuple = reinterpret_cast<DTuple*>(second);

        for (const auto& [name2_index, _] :
             std::span(second_tuple->elements, second_tuple->length)) {
            for (const auto& [name1_index, _] :
                 std::span(first_tuple->elements, first_tuple->length)) {
                if (name1_index == name2_index && name1_index != static_cast<size_t>(-1)) {
                    throw std::runtime_error("duplicated tuple fields during concatenation");
                }
            }
        }

        std::vector<std::pair<size_t, DRef*>> result_elements(
            first_tuple->elements, first_tuple->elements + first_tuple->length);
        result_elements.insert(result_elements.end(), second_tuple->elements,
                               second_tuple->elements + second_tuple->length);

        return heap.make_tuple(result_elements);
    }
    }
    __builtin_unreachable();
}

} // namespace sm
