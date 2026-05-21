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

std::ostream& operator<< (std::ostream& os, const Location& loc);

enum class Type { None, Int, Real, Bool, String, Array, Tuple, Func };
std::ostream& operator<< (std::ostream&, const Type&);

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

  BC_REAL     = 20,
  // ioperands: float ieee-754 const
  // loads float value onto stack

  BC_BOOL     = 21,
  // ioperands:  0/1
  // loads boolean value onto stack

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

  BC_ISTYPE   = 27,
  // ioperands: type_id to check
  // stack operands: checked dvalue
  // pushes boolean if popped value is type_id

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

static int16_t imm16_1(Bytecode bc) {
    return static_cast<int16_t>((bc >> 16) & 0xffff);
}

static int32_t imm32(Bytecode bc) {
    return static_cast<int32_t>((bc >> 32) & 0xffffffff);
}

static Location loc(Bytecode bc) {
    return unpackLock(imm32(bc));
}

static BytecodeSignatures sig(Bytecode bc) {
    return static_cast<BytecodeSignatures>(bc & 0xff);
}

static float raw_to_float(uint32_t val) {
    return *reinterpret_cast<float*>(&val);
}

static uint32_t float_to_raw(float f) {
    return *reinterpret_cast<uint32_t*>(&f);
}

static Bytecode bc_2op(BytecodeSignatures s, uint16_t op1, uint32_t op2) {
  return (static_cast<int64_t>(op2) << 32) | (static_cast<int32_t>(op1) << 16) | s;
}

static Bytecode bc_1op(BytecodeSignatures s, uint32_t op) {
  return (static_cast<int64_t>(op) << 32) | s;
}

static Bytecode bc_0op(BytecodeSignatures s) {
  return s;
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
  DEF(12, ||)\
  DEF(13, ^)

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
        const Location& loc = fn.capture[ci];
        os << loc;
      }
      os << "]\n";

      for (size_t bi = 0; bi < fn.code.size(); ++bi) {
        Bytecode bc = fn.code[bi];
        os << "    " << std::setw(4) << bi << ": ";

        switch (sig(bc)) {
          case BC_BINOP: {
            uint16_t op = imm16_1(bc);
            const char* opname = "?";
            switch (op) {
#define CASE(opc, sym) case opc: opname = #sym; break;
              BINOPS(CASE)
#undef CASE
            }
            os << "BINOP " << opname << "\n";
            break;
          }
          case BC_ISTYPE: {
            os << "ISTYPE type=" << static_cast<sm::Type> (imm32(bc)) << "\n"; break;
          }
          case BC_LD:      os << "LD loc=" << loc(bc) << "\n"; break;
          case BC_LDA: {
            os << "LDA loc=" << loc(bc) << "\n"; break;
          }
          case BC_LDT: {
            os << "LDT loc=" << loc(bc) << " idx=" << imm16_1(bc) << "\n"; break;
          }
          case BC_ST:      os << "ST loc=" << loc(bc) << "\n"; break;
          case BC_STA: {
            os << "STA loc=" << loc(bc) << "\n"; break;
          }
          case BC_STT: {
            os << "STT loc=" << loc(bc) << " idx=" << imm16_1(bc) << "\n"; break;
          }
          case BC_STOP:    os << "STOP\n"; break;
          case BC_CONST:   os << "CONST " << imm32(bc) << "\n"; break;
          case BC_BOOL:    os << "BOOL " << imm32(bc) << "\n"; break;
          case BC_REAL:    os << "REAL " << raw_to_float(imm32(bc)) << "\n"; break;
          case BC_ARRAY:   os << "ARRAY\n"; break;
          case BC_STRING:  os << "STRING #" << imm32(bc) << "\n"; break;
          case BC_TUPLE:   os << "TUPLE #" << imm32(bc) << "\n"; break;
          case BC_JMP:     os << "JMP -> " << imm32(bc) << "\n"; break;
          case BC_RET:     os << "RET\n"; break;
          case BC_DROP:    os << "DROP\n"; break;
          case BC_DUP:     os << "DUP\n"; break;
          case BC_CJMP:    os << "CJMP -> " << imm32(bc) << "\n"; break;
          case BC_CLOSURE: os << "CLOSURE #" << imm32(bc) << "\n"; break;
          case BC_CALLC:   os << "CALLC args=" << imm32(bc) << "\n"; break;
          case BC_PRINT:   os << "PRINT\n"; break;
          default:         os << "UNKNOWN(" << static_cast<int>(sig(bc)) << ")\n"; break;
        }
      }
    }

    os << "Main: " << main_function_index << "\n";
  }
};

} // namespace sm
