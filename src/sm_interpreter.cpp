#include "sm_interpreter.hpp"

#include "bytecode.hpp"
#include "sm_runtime.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <print>
#include <stdexcept>
#include <vector>

namespace sm {

using namespace gc;

void interprete(Runtime* runtime, const BcFile& bc_file, std::ostream& out) {
    Heap& heap = runtime->heap;
    HeapObject* return_value;

    const auto* main_scheme = &bc_file.functions[bc_file.main_function_index];

    runtime->stack.emplace_back(runtime, main_scheme, std::numeric_limits<size_t>::max());

    size_t bc_index = 0;

    while (true) {
        if (runtime->stack.empty()) {
            break;
        }

        Frame& frame = runtime->stack.back();

        Bytecode bc = frame.scheme->code[bc_index];
        bool jumped = false;
        // std::cerr << "HERE: " << bc_index << ' ' << frame.stack.size() << '\n';

        switch (sig(bc)) {
        case BC_BINOP: {
            HeapObject* second = resolve_ref(frame.pop());
            HeapObject* first  = resolve_ref(frame.pop());

            int32_t op = imm32(bc);

            switch (first->type) {
            case Type::Ref: {
                throw std::runtime_error("binop with ref");
            }
            case Type::None: {
                throw std::runtime_error("binop with none");
            }

            case Type::Func: {
                throw std::runtime_error("binop with func");
            }

#define CASE_BINOP(id, op, cons)                                                                   \
    case id:                                                                                       \
        frame.push(cons(first_arg op second_arg));                                                 \
        break;

            case Type::Int:
            case Type::Real: {
                if (not(second->type == Type::Int || second->type == Type::Real)) {
                    throw std::runtime_error("unsupported binop operand type");
                }

                if (first->type == Type::Real || second->type == Type::Real) {
                    const auto first_arg  = get_float(first);
                    const auto second_arg = get_float(second);
                    std::cerr << first_arg << ' ' << second_arg << '\n';

                    switch (op) {
                        CASE_BINOP(0, +, heap.make_real)
                        CASE_BINOP(1, -, heap.make_real)
                        CASE_BINOP(2, *, heap.make_real)
                        CASE_BINOP(3, /, heap.make_real)
                        CASE_BINOP(5, <, heap.make_bool)
                        CASE_BINOP(6, <=, heap.make_bool)
                        CASE_BINOP(7, >, heap.make_bool)
                        CASE_BINOP(8, >=, heap.make_bool)
                        CASE_BINOP(9, ==, heap.make_bool)
                        CASE_BINOP(10, !=, heap.make_bool)

                    default: {
                        throw std::runtime_error("unsupported binop operand type");
                    }
                    }
                } else {
                    const auto first_arg  = reinterpret_cast<DInt*>(first)->value;
                    const auto second_arg = reinterpret_cast<DInt*>(second)->value;

                    switch (op) {
                        CASE_BINOP(0, +, heap.make_int)
                        CASE_BINOP(1, -, heap.make_int)
                        CASE_BINOP(2, *, heap.make_int)
                        CASE_BINOP(3, /, heap.make_int)
                        CASE_BINOP(4, %, heap.make_int)
                        CASE_BINOP(5, <, heap.make_bool)
                        CASE_BINOP(6, <=, heap.make_bool)
                        CASE_BINOP(7, >, heap.make_bool)
                        CASE_BINOP(8, >=, heap.make_bool)
                        CASE_BINOP(9, ==, heap.make_bool)
                        CASE_BINOP(10, !=, heap.make_bool)

                    default: {
                        throw std::runtime_error("unsupported binop operand type");
                    }
                    }
                }

                break;
            }

            case Type::Bool: {
                if (not(second->type == Type::Bool)) {
                    throw std::runtime_error("unsupported binop operand type");
                }

                const auto first_arg  = reinterpret_cast<DBool*>(first)->value;
                const auto second_arg = reinterpret_cast<DBool*>(second)->value;

                switch (op) {
                    CASE_BINOP(11, &&, heap.make_bool)
                    CASE_BINOP(12, ||, heap.make_bool)
                    CASE_BINOP(13, ^, heap.make_bool)

                default: {
                    throw std::runtime_error("unsupported binop operand type");
                }
                }

                break;
            }

            case Type::String:
            case Type::Array:
            case Type::Tuple: {
                switch (op) {
                case 0:
                    frame.push(runtime->concat(first, second));
                    break;
                case 9: {
                    if (first->type != second->type) {
                        frame.push(heap.make_bool(false));
                        break;
                    }

                    if (first->type == Type::String) {
                        auto first_string  = reinterpret_cast<DString*>(first);
                        auto second_string = reinterpret_cast<DString*>(second);
                        frame.push(
                            heap.make_bool(strcmp(first_string->data, second_string->data) == 0));
                        break;
                    }

                    frame.push(heap.make_bool(first == second));
                    break;
                }
                case 10: {
                    if (first->type != second->type) {
                        frame.push(heap.make_bool(true));
                        break;
                    }

                    if (first->type == Type::String) {
                        auto first_string  = reinterpret_cast<DString*>(first);
                        auto second_string = reinterpret_cast<DString*>(second);
                        frame.push(
                            heap.make_bool(strcmp(first_string->data, second_string->data) != 0));
                        break;
                    }

                    frame.push(heap.make_bool(first != second));
                    break;
                }
                }
                if (op != 0 && op != 9 && op != 10) {
                    throw std::runtime_error("unsupported binop operand type");
                }
            }
            case Type::ArrayData:
                throw std::runtime_error("binop with array data");
            }

            break;
        }

        case BC_ISTYPE: {
            HeapObject* val    = resolve_ref(frame.pop());
            Type expected_type = static_cast<Type>(imm32(bc));
            frame.push(runtime->heap.make_bool(val->type == expected_type));
            break;
        }

        case BC_LD: {
            const auto value = frame[loc(bc)];
            frame.push(value);
            break;
        }

        case BC_LDA: {
            const auto index = resolve_ref(frame.pop());
            if (index->type != Type::Int) {
                throw std::runtime_error("Index should be an integer value");
            }

            const auto array = resolve_ref(frame.pop());
            if (array->type != Type::Array) {
                throw std::runtime_error("Array should be of array type");
            }

            auto* darray     = reinterpret_cast<DArray*>(array);
            auto raw_array   = darray->data->elements;
            size_t raw_index = reinterpret_cast<DInt*>(index)->value;

            for (size_t i = 0; i < darray->data->size; i++) {
                if (raw_array[i].first == raw_index) {
                    frame.push(raw_array[i].second);
                    goto found;
                }
            }

            heap.replace_array_data(darray,
                                    std::pair<size_t, DRef*>{raw_index, heap.make_ref(heap.none)});
            raw_array = darray->data->elements;
            for (size_t i = 0; i < darray->data->size; i++) {
                if (raw_array[i].first == raw_index) {
                    frame.push(raw_array[i].second);
                    goto found;
                }
            }
            __builtin_unreachable();
        found:
            break;
        }

        case BC_LDT: {
            const auto tuple = resolve_ref(frame.pop());
            if (tuple->type != Type::Tuple) {
                throw std::runtime_error("Tuple should be of tuple type");
            }
            DTuple* raw_tuple = reinterpret_cast<DTuple*>(tuple);

            const auto index = imm32(bc);
            if (index < 0) {
                if (static_cast<size_t>(-1 - index) >= raw_tuple->length) {
                    throw std::runtime_error("invalid tuple index");
                }

                frame.push(raw_tuple->elements[-1 - index].second);
            } else {
                size_t tuple_index = -1;
                for (size_t i = 0; i < raw_tuple->length; i++) {
                    if (raw_tuple->elements[i].first == static_cast<size_t>(index)) {
                        tuple_index = i;
                        break;
                    }
                }
                if (tuple_index == static_cast<size_t>(-1)) {
                    throw std::runtime_error("invalid tuple get");
                }
                frame.push(raw_tuple->elements[tuple_index].second);
            }
            break;
        }

        case BC_ST: {
            change_ref(frame[loc(bc)], resolve_ref(frame.pop()));
            break;
        }

        case BC_STA: {
            const auto element = resolve_ref(frame.pop());

            const auto index = resolve_ref(frame.pop());
            if (index->type != Type::Int) {
                throw std::runtime_error("Index should be an integer value");
            }

            const auto array = resolve_ref(frame.pop());
            if (array->type != Type::Array) {
                throw std::runtime_error("Array should be of array type");
            }

            DArray* darray   = reinterpret_cast<DArray*>(array);
            auto raw_array   = darray->data->elements;
            size_t raw_index = reinterpret_cast<DInt*>(index)->value;

            for (size_t i = 0; i < darray->data->size; i++) {
                if (raw_array[i].first == raw_index) {
                    raw_array[i].second = heap.make_ref(element);
                    goto found2;
                }
            }
            heap.replace_array_data(darray,
                                    std::pair<size_t, DRef*>{raw_index, heap.make_ref(element)});
        found2:
            break;
        }

        case BC_STT: {
            const auto element = frame.pop();

            const auto tuple = resolve_ref(frame.pop());
            if (tuple->type != Type::Tuple) {
                throw std::runtime_error("Tuple should be of tuple type");
            }
            DTuple* raw_tuple = reinterpret_cast<DTuple*>(tuple);

            const auto index = imm16_1(bc);
            if (index >= 0) {
                if (static_cast<size_t>(index) >= raw_tuple->length) {
                    throw std::runtime_error("invalid tuple index");
                }
                raw_tuple->elements[index].second = heap.make_ref(element);
            } else {
                size_t tuple_index = -1;
                for (size_t i = 0; i < raw_tuple->length; i++) {
                    if (raw_tuple->elements[i].first == static_cast<size_t>(-1 - index)) {
                        tuple_index = i;
                        break;
                    }
                }
                if (tuple_index == static_cast<size_t>(-1)) {
                    throw std::runtime_error("invalid tuple get");
                }

                raw_tuple->elements[tuple_index].second = heap.make_ref(element);
            }

            break;
        }

        case BC_STD: {
            HeapObject* src  = resolve_ref(frame.pop());
            HeapObject* dest = frame.pop();
            if (dest->type != Type::Ref) {
                throw std::runtime_error("assignment to non-ref object at bytecode index " +
                                         std::to_string(bc_index));
            }
            change_ref(dest, src);
            break;
        }

        case BC_STOP: {
            bc_index = frame.scheme->code.size();
            jumped   = true;
            break;
        }

        case BC_NONE: {
            frame.push(heap.make_none());
            break;
        }

        case BC_CONST: {
            HeapObject* value = heap.make_int(imm32(bc));
            frame.push(value);
            break;
        }

        case BC_BOOL: {
            HeapObject* value = heap.make_bool(imm32(bc));
            frame.push(value);
            break;
        }

        case BC_REAL: {
            HeapObject* value = heap.make_real(raw_to_float(imm32(bc)));
            frame.push(value);
            break;
        }

        case BC_ARRAY: {
            frame.push(heap.make_array({}));
            break;
        }

        case BC_STRING: {
            const size_t index = imm32(bc);
            HeapObject* string = heap.make_string(bc_file.strings[index]);
            frame.push(string);
            break;
        }

        case BC_TUPLE: {
            const TupleScheme* scheme = &bc_file.tuples[imm32(bc)];

            std::vector<std::pair<size_t, DRef*>> elements;
            for (size_t i = 0; i < scheme->field_names.size(); i++) {
                elements.emplace_back(scheme->field_names[i],
                                      heap.make_ref(resolve_ref(frame.pop())));
            }
            std::reverse(elements.begin(), elements.end());

            frame.push(heap.make_tuple(elements));
            break;
        }

        case BC_JMP: {
            bc_index = imm32(bc);
            jumped   = true;
            break;
        }

        case BC_RET: {
            return_value = frame.pop();

            bc_index = frame.return_index;
            jumped   = true;

            runtime->stack.pop_back();
            if (not runtime->stack.empty()) {
                runtime->stack.back().push(return_value);
            }

            break;
        }

        case BC_DROP: {
            frame.pop();
            break;
        }

        case BC_DUP: {
            HeapObject* value = frame.pop();
            frame.push(value);
            frame.push(value);
            break;
        }

        case BC_RNGSPC: {
            HeapObject* iterator = resolve_ref(frame.pop());
            HeapObject* target   = resolve_ref(frame.pop());
            if (target->type != Type::Int || iterator->type != Type::Int) {
                throw std::runtime_error("unexpected type for range limits");
            }
            int iterable_v = reinterpret_cast<DInt*>(iterator)->value;
            int target_v   = reinterpret_cast<DInt*>(target)->value;
            frame.push(heap.make_int(iterable_v <= target_v ? iterable_v + 1 : iterable_v - 1));
            break;
        }

        case BC_LENGTH: {
            HeapObject* iterable = resolve_ref(frame.pop());
            switch (iterable->type) {
            case Type::Array: {
                DArray* arr = reinterpret_cast<DArray*>(iterable);
                frame.push(heap.make_int(arr->data->size));
                break;
            }
            case Type::Tuple: {
                DTuple* tup = reinterpret_cast<DTuple*>(iterable);
                frame.push(heap.make_int(tup->length));
                break;
            }
            case Type::String: {
                DString* str = reinterpret_cast<DString*>(iterable);
                frame.push(heap.make_int(str->length));
                break;
            }
            default:
                throw std::runtime_error("non iterable object doen't have length");
            }
            break;
        }

        case BC_CJMPZ: {
            HeapObject* condition = resolve_ref(frame.pop());
            if (condition->type != Type::Bool) {
                throw std::runtime_error("unexpected condition type");
            }

            if (!static_cast<bool>(reinterpret_cast<DBool*>(condition)->value)) {
                bc_index = imm32(bc);
                jumped   = true;
            }
            break;
        }

        case BC_CLOSURE: {
            const FunctionScheme* scheme = &bc_file.functions[imm32(bc)];

            std::vector<DRef*> captured;
            for (auto& loc : scheme->capture) {
                captured.push_back(heap.make_ref(frame[loc]));
            }

            HeapObject* func = heap.make_function(scheme, captured);
            frame.push(func);
            break;
        }

        case BC_CALLC: {
            const size_t args_count = imm32(bc);

            HeapObject* func = resolve_ref(frame.pop());
            DFunc* raw_func  = reinterpret_cast<DFunc*>(func);
            if (func->type != Type::Func) {
                throw std::runtime_error("Function should be of function type");
            }
            if (args_count != raw_func->scheme->args_number) {
                throw std::runtime_error("Function called with invalid args number");
            }

            std::vector<HeapObject*> args(args_count);
            for (int arg_id = args_count - 1; arg_id >= 0; --arg_id) {
                args[arg_id] = resolve_ref(frame.pop());
            }
            std::vector<DRef*> capture(raw_func->capture,
                                       raw_func->capture + raw_func->scheme->capture.size());
            runtime->stack.emplace_back(runtime, raw_func->scheme, bc_index + 1, args, capture);

            bc_index = 0;
            jumped   = true;
            break;
        }

        case BC_PRINT: {
            const size_t args_count = imm32(bc);
            for (size_t arg = 0; arg < args_count; ++arg) {
                runtime->print(resolve_ref(frame.pop()), out);
                out << (arg + 1 < args_count ? " " : "\n");
            }
            break;
        }

        case BC_LABEL:
            throw std::runtime_error("BC_LABEL encountered at runtime (compiler bug)");
        }

        if (not jumped) {
            bc_index++;
        }

        while (bc_index == runtime->stack.back().scheme->code.size()) {
            bc_index = runtime->stack.back().return_index;
            runtime->stack.pop_back();
            if (not runtime->stack.empty()) {
                runtime->stack.back().push(heap.none);
            } else {
                return;
            }
        }
    }
}

} // namespace sm
