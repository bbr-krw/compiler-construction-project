#pragma once

#include <cinttypes>
#include <cstdint>
#include <vector>
#include <string>
#include <assert.h>

enum LocTypes {
    LOCAL, // #args + 1 ... inf
    ARGUMENT, // 0 ... #args
    CAPTURED, // #-1 ... -inf? // TODO[atrubnikov] Not supported
};

enum BytecodeSignatures {
  BC_BINOP    = 0,
  // ioperands: op, stack_operand: loperand, roperand
  // push result onto stack

  BC_LD       = 2,
  // ioperands: loc, stack_operand: none
  // loads value onto stack

  BC_LDA      = 3,

  BC_ST       = 4,
  BC_STOP     = 15,

  /* 00-15 is BC_BINOP */

  BC_CONST    = 16,
  BC_ARRAY    = 17,
  BC_STRING   = 18,
  BC_TUPLE    = 19,

  BC_STI      = 20,
  BC_STA      = 21,
  BC_JMP      = 22,
  BC_END      = 23,
  BC_RET      = 24,
  BC_DROP     = 25,
  BC_DUP      = 26,
  BC_SWAP     = 27,
  BC_ELEM     = 28,

  /* 32-47 is BC_LD */
  /* 48-63 is BC_LDA */
  /* 64-79 is BC_ST */

  BC_CJMPZ    = 80,
  BC_CJMPNZ   = 81,

  BC_BEGIN    = 82,
  BC_CBEGIN   = 83,
  BC_CLOSURE  = 84,
  BC_CALLC    = 85,
  BC_CALL     = 86,

  BC_LWRITE   = 113,
};


class ClosureConstants {
  // std::vector<LOC>?
  // TODO[atrubnikov]
};

using Bytecode = std::uint64_t;

template <BytecodeSignatures sig, typename ...Args>
Bytecode encode(Args... args);

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
  std::vector<ClosureConstants> closures;
  std::vector<Bytecode> code;

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
