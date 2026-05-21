#pragma once

#include <cinttypes>
#include <cstdint>
#include <vector>
#include <optional>
#include <string>
#include <ostream>
#include <iomanip>
#include <cassert>

namespace sm {

enum LocTypes : uint16_t {
  LOCAL, // #args + 1 ... inf
  ARGUMENT, // 0 ... #args
  CAPTURED, // #-1 ... -inf? // TODO[atrubnikov] Not supported
};

struct Location {
  LocTypes type;
  uint16_t index;
};

uint32_t packLock(Location loc);

Location unpackLock(uint32_t data);

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
  uint16_t args_number;
  uint16_t locals_number;
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

struct BcFile {
  std::vector<std::string> strings;
  std::vector<TupleScheme> tuples;
  std::vector<FunctionScheme> functions;
  size_t main_function_index;

  size_t addString(const std::string& s) {
    strings.push_back(s);
    return strings.size() - 1;
  }

  // ai-slop method
  void print(std::ostream& os) {
    os << "Strings (" << strings.size() << "):\n";
    for (size_t i = 0; i < strings.size(); ++i) {
      os << "  [" << i << "] " << std::quoted(strings[i]) << "\n";
    }

    os << "Tuples (" << tuples.size() << "):\n";
    for (size_t ti = 0; ti < tuples.size(); ++ti) {
      const auto& ts = tuples[ti];
      os << "  [" << ti << "] (";
      for (size_t fi = 0; fi < ts.field_names.size(); ++fi) {
        if (fi) os << ", ";
        const auto& name = ts.field_names[fi];
        if (name) os << *name; else os << "_";
      }
      os << ")\n";
    }

    os << "Functions (" << functions.size() << "):\n";
    for (size_t fi = 0; fi < functions.size(); ++fi) {
      const auto& fn = functions[fi];
      os << "  [" << fi << "] capture: [";
      for (size_t ci = 0; ci < fn.capture.size(); ++ci) {
        if (ci) os << ", ";
        const auto& loc = fn.capture[ci];
        const char* tname = (loc.type == LOCAL) ? "LOCAL" : (loc.type == ARGUMENT) ? "ARG" : "CAP";
        os << tname << "(" << loc.index << ")";
      }
      os << "]\n";

      for (size_t bi = 0; bi < fn.code.size(); ++bi) {
        Bytecode bc = fn.code[bi];
        uint8_t sig = bc & 0xFF;
        uint64_t ops = bc >> 8;
        os << "    " << std::setw(4) << bi << ": ";

        switch (static_cast<BytecodeSignatures>(sig)) {
          case BC_BINOP: {
            uint16_t op = static_cast<uint16_t>(ops & 0xFFFF);
            const char* opname = "?";
            switch (op) {
#define CASE(opc, sym) case opc: opname = #sym; break;
              BINOPS(CASE)
#undef CASE
            }
            os << "BINOP " << opname << "\n";
            break;
          }
          case BC_LD:      os << "LD loc=" << static_cast<uint16_t>(ops) << "\n"; break;
          case BC_LDA: {
            uint16_t loc  = static_cast<uint16_t>(ops >> 16);
            uint16_t idx  = static_cast<uint16_t>(ops & 0xFFFF);
            os << "LDA loc=" << loc << " idx=" << idx << "\n"; break;
          }
          case BC_LDT: {
            uint16_t loc  = static_cast<uint16_t>(ops >> 16);
            int16_t  idx  = static_cast<int16_t>(ops & 0xFFFF);
            os << "LDT loc=" << loc << " idx=" << idx << "\n"; break;
          }
          case BC_ST:      os << "ST loc=" << static_cast<uint16_t>(ops) << "\n"; break;
          case BC_STA: {
            uint16_t loc  = static_cast<uint16_t>(ops >> 32);
            uint16_t idx  = static_cast<uint16_t>((ops >> 16) & 0xFFFF);
            uint16_t val  = static_cast<uint16_t>(ops & 0xFFFF);
            os << "STA loc=" << loc << " idx=" << idx << " val=" << val << "\n"; break;
          }
          case BC_STT: {
            uint16_t loc  = static_cast<uint16_t>(ops >> 16);
            int16_t  idx  = static_cast<int16_t>(ops & 0xFFFF);
            os << "STT loc=" << loc << " idx=" << idx << "\n"; break;
          }
          case BC_STOP:    os << "STOP\n"; break;
          case BC_CONST:   os << "CONST " << static_cast<int32_t>(ops) << "\n"; break;
          case BC_ARRAY:   os << "ARRAY\n"; break;
          case BC_STRING:  os << "STRING #" << static_cast<uint16_t>(ops) << "\n"; break;
          case BC_TUPLE:   os << "TUPLE #" << static_cast<uint16_t>(ops) << "\n"; break;
          case BC_JMP:     os << "JMP -> " << static_cast<uint32_t>(ops) << "\n"; break;
          case BC_RET:     os << "RET\n"; break;
          case BC_DROP:    os << "DROP\n"; break;
          case BC_DUP:     os << "DUP\n"; break;
          case BC_CJMP:    os << "CJMP -> " << static_cast<uint32_t>(ops) << "\n"; break;
          case BC_CLOSURE: os << "CLOSURE #" << static_cast<uint16_t>(ops) << "\n"; break;
          case BC_CALLC:   os << "CALLC args=" << static_cast<uint16_t>(ops) << "\n"; break;
          case BC_PRINT:   os << "PRINT\n"; break;
          default:         os << "UNKNOWN(" << static_cast<int>(sig) << ")\n"; break;
        }
      }
    }

    os << "Main: " << main_function_index << "\n";
  }
};

} // namespace sm
