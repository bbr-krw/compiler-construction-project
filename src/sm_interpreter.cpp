#pragma once

#include "sm_interpreter.hpp"
#include "bytecode.hpp"
#include "sm_runtime.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>
#include <iostream>

namespace sm {

DValue* concat(DValue* first, DValue* second) {
    if (first->type != second->type) {
        throw std::runtime_error("cannot concat different types");
    }

    switch (first->type) {
    case Type::None:
    case Type::Int:
    case Type::Real:
    case Type::Bool:
    case Type::Func:
        throw std::runtime_error("unsupported type for concatenation");
    
    case Type::String: {
        DString* first_str = reinterpret_cast<DString*>(first->value);
        DString* second_str = reinterpret_cast<DString*>(second->value);

        std::string result = first_str->data;
        result += second_str->data;

        return make_string(result);
    }

    case Type::Array: {
        DArray* first_arr = reinterpret_cast<DArray*>(first->value);
        DArray* second_arr = reinterpret_cast<DArray*>(second->value);

        DArray array;
        for (const auto& [key, value] : *first_arr) {
            array.emplace(key, value);
        }
        for (const auto& [key, value] : *second_arr) {
            array.emplace(key, value);
        }

        return make_array(array);
    }

    case Type::Tuple: {
        DTuple* first_tuple = reinterpret_cast<DTuple*>(first->value);
        DTuple* second_tuple = reinterpret_cast<DTuple*>(second->value);

        std::vector<std::optional<std::string>> result_names = first_tuple->scheme->field_names;
        std::vector<DValue*> result_elements{first_tuple->elements, first_tuple->elements + first_tuple->scheme->field_names.size()};

        for (size_t i = 0; i < second_tuple->scheme->field_names.size(); i++) {
            const auto& name = second_tuple->scheme->field_names[i];
            if (name.has_value()) {
                if (std::find(result_names.begin(), result_names.end(), name) != result_names.end()) {
                    throw std::runtime_error("duplicated tuple fields during concatenation");
                }
            }

            result_names.push_back(name);
            result_elements.push_back(second_tuple->elements[i]);
        }

        const auto* result_scheme = new TupleScheme{result_names};
        return sm::make_tuple(result_scheme, result_elements);
    }

    }
}

void interprete(Runtime* runtime, const BcFile& bcFile) {
    DValue* return_value;

    Frame initial_frame;
    
    const auto* main_scheme = &bcFile.functions[bcFile.main_function_index];
    initial_frame.scheme = main_scheme;
    initial_frame.return_index = std::numeric_limits<size_t>::max();
    initial_frame.captured = {};
    initial_frame.args = {};
    initial_frame.locals.assign(main_scheme->locals_number, new DValue{});
    initial_frame.stack = {};

    runtime->stack.push_back(initial_frame);

    size_t bc_index = 0;
    
    while (true) {
        if (runtime->stack.empty()) {
            break;
        }

        Frame& frame = runtime->stack.back();
        if (bc_index == frame.scheme->code.size()) {
            break;
        }

        Bytecode bc = frame.scheme->code[bc_index];
        bool jumped = false;

        switch (static_cast<BytecodeSignatures>(bc & 0xff)) {
        case BC_BINOP: {
            DValue* second = frame.pop();
            DValue* first = frame.pop();

            int32_t op = imm32(bc);

            switch (first->type) {
            case Type::None: {
                throw std::runtime_error("binop with none");
            }

            case Type::Func: {
                throw std::runtime_error("binop with func");
            }

            #define CASE_BINOP(id, op, cons)                    \
            case id:                                            \
                frame.push(cons(first_arg op second_arg));      \
                break;                                          \

            case Type::Int:
            case Type::Real: {
                if (not (second->type == Type::Int || second->type == Type::Real)) {
                    throw std::runtime_error("unsupported binop operand type");
                }

                if (first->type == Type::Real || second->type == Type::Real) {
                    const auto first_arg = static_cast<double>(first->value);
                    const auto second_arg = static_cast<double>(second->value);

                    switch (op) {
                    CASE_BINOP(0, +, make_real)
                    CASE_BINOP(1, -, make_real)
                    CASE_BINOP(2, *, make_real)
                    CASE_BINOP(3, /, make_real)
                    CASE_BINOP(5, <, make_bool)
                    CASE_BINOP(6, <=, make_bool)
                    CASE_BINOP(7, >, make_bool)
                    CASE_BINOP(8, >=, make_bool)
                    CASE_BINOP(9, ==, make_bool)
                    CASE_BINOP(10, !=, make_bool)

                    default: {
                        throw std::runtime_error("unsupported binop operand type");
                    }
                    }
                } else {
                    const auto first_arg = static_cast<int>(first->value);
                    const auto second_arg = static_cast<int>(second->value);

                    switch (op) {
                    CASE_BINOP(0, +, make_int)
                    CASE_BINOP(1, -, make_int)
                    CASE_BINOP(2, *, make_int)
                    CASE_BINOP(3, /, make_int)
                    CASE_BINOP(4, %, make_int)
                    CASE_BINOP(5, <, make_bool)
                    CASE_BINOP(6, <=, make_bool)
                    CASE_BINOP(7, >, make_bool)
                    CASE_BINOP(8, >=, make_bool)
                    CASE_BINOP(9, ==, make_bool)
                    CASE_BINOP(10, !=, make_bool)

                    default: {
                        throw std::runtime_error("unsupported binop operand type");
                    }
                    }
                }

                break;
            }

            case Type::Bool: {
                if (not (second->type == Type::Bool)) {
                    throw std::runtime_error("unsupported binop operand type");
                }

                const auto first_arg = static_cast<bool>(first->value);
                const auto second_arg = static_cast<bool>(second->value);

                switch (op) {
                CASE_BINOP(11, &&, make_bool)
                CASE_BINOP(12, ||, make_bool)
                CASE_BINOP(13, ^, make_bool)

                default: {
                    throw std::runtime_error("unsupported binop operand type");
                }
                }

                break;
            }

            case Type::String:
            case Type::Array:
            case Type::Tuple: {
                if (op != 0) {
                    throw std::runtime_error("unsupported binop operand type");
                }

                frame.push(concat(first, second));
                break;
            }

            }

            break;
        }

        case BC_ISTYPE: {
            DValue* val = frame.pop();
            Type expected_type = static_cast<Type>(imm32(bc));
            frame.push(make_bool(val->type == expected_type));
            break;
        }

        case BC_LD: {
            const auto value = frame[loc(bc)];
            frame.push(value);
            break;
        }

        case BC_LDA: {
            const auto index = frame.pop();
            if (index->type != Type::Int) {
                throw std::runtime_error("Index should be an integer value");
            }

            const auto array = frame.pop();
            if (array->type != Type::Array) {
                throw std::runtime_error("Array should be of array type");
            }

            DArray* raw_array = reinterpret_cast<DArray*>(array->value);
            size_t raw_index = static_cast<size_t>(index->value);

            DValue* value = raw_array->contains(raw_index)
                ? raw_array->at(raw_index)
                : ((*raw_array)[raw_index] = make_none());
            frame.push(value);

            break;
        }

        case BC_LDT: {
            const auto tuple = frame.pop();
            if (tuple->type != Type::Tuple) {
                throw std::runtime_error("Tuple should be of tuple type");
            }
            DTuple* raw_tuple = reinterpret_cast<DTuple*>(tuple->value);

            const auto index = imm32(bc);
            if (index < 0) {
                if (-1 - index >= raw_tuple->scheme->field_names.size()) {
                    throw std::runtime_error("invalid tuple index");
                }

                frame.push(raw_tuple->elements[-1 - index]);
            } else {
                const std::string& name = bcFile.strings.at(index);
                const auto& field_names = raw_tuple->scheme->field_names;
                const size_t tuple_index = std::find(field_names.begin(), field_names.end(), name) - field_names.begin();
                if (tuple_index == field_names.size()) {
                    throw std::runtime_error("invalid tuple get");
                }

                frame.push(raw_tuple->elements[tuple_index]);
            }

            break;
        }

        case BC_ST: {
            frame[loc(bc)] = frame.pop();
            break;
        }

        case BC_STA: {
            const auto element = frame.pop();

            const auto index = frame.pop();
            if (index->type != Type::Int) {
                throw std::runtime_error("Index should be an integer value");
            }

            const auto array = frame.pop();
            if (array->type != Type::Array) {
                throw std::runtime_error("Array should be of array type");
            }

            DArray* raw_array = reinterpret_cast<DArray*>(array->value);
            size_t raw_index = static_cast<size_t>(index->value);

            raw_array->emplace(raw_index, element);

            break;
        }

        case BC_STT: {
            const auto element = frame.pop();

            const auto tuple = frame.pop();
            if (tuple->type != Type::Tuple) {
                throw std::runtime_error("Tuple should be of tuple type");
            }
            DTuple* raw_tuple = reinterpret_cast<DTuple*>(tuple->value);

            const auto index = imm16_1(bc);
            if (index >= 0) {
                if (index >= raw_tuple->scheme->field_names.size()) {
                    throw std::runtime_error("invalid tuple index");
                }

                raw_tuple->elements[index] = element;
            } else {
                const std::string& name = bcFile.strings.at(-1 - index);
                const auto& field_names = raw_tuple->scheme->field_names;
                const size_t tuple_index = std::find(field_names.begin(), field_names.end(), name) - field_names.begin();
                if (tuple_index == field_names.size()) {
                    throw std::runtime_error("invalid tuple get");
                }

                raw_tuple->elements[tuple_index] = element;
            }

            break;
        }

        case BC_STD: {
            DValue* src = frame.pop();
            DValue* dest = frame.pop();
            *dest = *src;
            break;
        }

        case BC_STOP: {
            bc_index = frame.scheme->code.size();
            jumped = true;
            break;
        }

        case BC_NONE: {
            frame.push(make_none());
            break;
        }

        case BC_CONST: {
            DValue* value = make_int(imm32(bc));
            frame.push(value);
            break;
        }

        case BC_BOOL: {
            DValue* value = make_bool(imm32(bc));
            frame.push(value);
            break;
        }

        case BC_REAL: {
            DValue* value = make_real(raw_to_float(imm32(bc)));
            frame.push(value);
            break;
        }

        case BC_ARRAY: {
            frame.push(make_array({}));
            break;
        }

        case BC_STRING: {
            const size_t index = imm32(bc);
            DValue* string =  make_string(bcFile.strings[index]);
            frame.push(string);
            break;
        }

        case BC_TUPLE: {
            const TupleScheme* scheme = &bcFile.tuples[imm32(bc)];

            std::vector<DValue*> elements;
            for (size_t i = 0; i < scheme->field_names.size(); i++) {
                elements.push_back(frame.pop());
            }
            std::reverse(elements.begin(), elements.end());

            frame.push(sm::make_tuple(scheme, elements));
            break;
        }

        case BC_JMP: {
            bc_index = imm32(bc);
            jumped = true;
            break;
        }

        case BC_RET: {
            return_value = frame.pop();
            
            bc_index = frame.return_index;
            jumped = true;

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
            DValue* value = frame.pop();
            frame.push(value);
            frame.push(value);
            break;
        }

        case BC_CJMP: {
            DValue* condition = frame.pop();
            if (condition->type != Type::Bool) {
                throw std::runtime_error("unexpected condition type");
            }

            if (static_cast<bool>(condition->value)) {
                bc_index = imm32(bc);
                jumped = true;
                break;
            }
        }

        case BC_CLOSURE: {
            const FunctionScheme* scheme = &bcFile.functions[imm32(bc)];
            
            std::vector<DValue*> captured;
            for (size_t i = 0; i < scheme->capture.size(); i++) {
                captured.push_back(frame[scheme->capture[i]]);
            }
            
            DValue* func = make_function(scheme, captured);
            frame.push(func);
            break;
        }

        case BC_CALLC: {
            const size_t args_count = imm32(bc);

            DValue* func = frame.pop();
            if (func->type != Type::Func) {
                throw std::runtime_error("Function should be of function type");
            }

            DFunc* raw_func = reinterpret_cast<DFunc*>(func->value);

            Frame new_frame;
            new_frame.scheme = raw_func->scheme;
            new_frame.return_index = bc_index + 1;
            new_frame.captured.assign(raw_func->capture, raw_func->capture + raw_func->scheme->capture.size());
            new_frame.locals.resize(raw_func->scheme->locals_number);

            if (args_count != raw_func->scheme->args_number) {
                throw std::runtime_error("Function called with invalid args number");
            }

            for (size_t i = 0; i < raw_func->scheme->args_number; i++) {
                new_frame.args.push_back(frame.pop());
            }
            std::reverse(new_frame.args.begin(), new_frame.args.end());

            runtime->stack.push_back(new_frame);

            bc_index = 0;
            jumped = true;
            break;
        }

        case BC_PRINT: {
            runtime->print(frame.pop());
            std::cout << "\n";
            break;
        }

        }

        if (not jumped) {
            bc_index++;
        }
    }
}

} // namespace sm
