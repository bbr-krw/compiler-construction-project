#pragma once

#include <cinttypes>
#include <cstdint>
#include <vector>
#include <optional>
#include <string>
#include <assert.h>

enum LocTypes : uint16_t {
  LOCAL, // #args + 1 ... inf
  ARGUMENT, // 0 ... #args
  CAPTURED, // #-1 ... -inf? // TODO[atrubnikov] Not supported
};
struct Location {
  LocTypes type;
  int16_t index;
};

static_assert(sizeof(Location) == 4, "Location is a valid argument");

/**
 *  Bytecode description is under the bytecode declaration
 *  All stack operands are popped from the stack
 */
enum BytecodeSignatures : uint8_t {
  BC_BINOP    = 0,
  // ioperands: op, stack_operand: loperand, roperand
  // push result onto stack

  BC_LD       = 2,
  // ioperands: loc, stack_operand: none
  // loads value onto stack

  BC_LDA      = 3,
  // ioperands: loc, stack_operands: index
  // loads array element by index to stack

  BC_LDT      = 4,
  // ioperands: loc, index
  // if index is non-negative, it should be processed as index of name in strings table
  // if index is negative, it should be processed as index in tuple scheme
  // loads tuple element by ondex onto stack

  BC_ST       = 5,
  // ioperands: loc, stack operand: value
  // stores value from stack

  BC_STA      = 6,
  // ioperands: loc, stack_operands: index, element
  // stores array element by index to array

  BC_STT      = 7,
  // ioperands: loc, index
  // if index is non-negative, it should be processed as index of name in strings table
  // if index is negative, it should be processed as index in tuple scheme
  // stores tuple element by index to tuple
  
  BC_STOP     = 15,
  // ends execution

  BC_CONST    = 16,
  // ioperands: const
  // loads const onto stack

  BC_ARRAY    = 17,
  // ioperands: -
  // loads empty array onto stack

  BC_STRING   = 18,
  // ioperands: index
  // loads string constant onto stack

  BC_TUPLE    = 19,
  // ioperands: tuple_scheme_index
  // stack operands: value1, value2, ...
  // loads tuple onto stack

  BC_JMP      = 22,
  // ioperands: bytecode_index
  // jumps
  
  BC_RET      = 24,
  // stack operands: rv
  // return

  BC_DROP     = 25,
  // stack operands: top
  // drops from stack

  BC_DUP      = 26,
  // stack operands: top
  // pushes top to stack

  BC_CJMP     = 80,
  // ioperands: bytecode_index
  // stack operands: condition
  // jumps if condition truthy

  BC_CLOSURE  = 84,
  // ioperands: function_scheme_index
  // captures values and loads function object onto stack

  BC_CALLC    = 85,
  // ioperands: args_count
  // stack operands: function, args... (args_count)
  // calls closure

  BC_PRINT   = 113,
  // ioperands: -
  // stack operands: value
  // prints value
};

using Bytecode = std::uint64_t;

struct FunctionScheme {
  std::vector<Bytecode> code;
  std::vector<Location> capture;
  // std::vector<LOC>?
  // TODO[atrubnikov]
};

struct TupleScheme {
  std::vector<std::optional<std::string>> field_names;
};

static Bytecode with_bc_signature(BytecodeSignatures s, Bytecode ops) {
  assert((ops & 0xFF00'0000'0000'0000) == 0);
  assert(s < 256);
  return (ops << 8) | s;
}

static Bytecode bc_2op(BytecodeSignatures s, uint16_t op1, uint16_t op2) {
  uint64_t op = op1;
  op = (op << 16) | op2;
  return with_bc_signature(s, with_bc_signature(s, op));
}

static Bytecode bc_1op(BytecodeSignatures s, uint32_t op) {
  return with_bc_signature(s, with_bc_signature(s, op));
}

static Bytecode bc_0op(BytecodeSignatures s) {
  return with_bc_signature(s, with_bc_signature(s, 0));
}

class BcFile {
  std::vector<std::string> strings;
  std::vector<TupleScheme> tuples;
  std::vector<FunctionScheme> functions;

  size_t addString(const std::string& s) {
    strings.push_back(s);
    return strings.size() - 1;
  }
};

#define BINOPS(DEF) \
  DEF( 0, +)\
  DEF( 1, -)\
  DEF( 2, *)\
  DEF( 3, /)\
  DEF( 4, %)\
  DEF( 5, <)\
  DEF( 6, <=)\
  DEF( 7, >)\
  DEF( 8, >=)\
  DEF( 9, ==)\
  DEF(10, !=)\
  DEF(11, &&)\
  DEF(12, ||)
