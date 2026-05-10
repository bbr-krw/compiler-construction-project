#pragma once

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

// ── Opcode ─────────────────────────────────────────────────────────────────────
// Each instruction has fields a, b, c, d (int32_t, -1 = unused).
// Register indices are per-function virtual registers (unlimited).
// Jump targets are absolute PC indices (instruction indices).
enum class Opc : uint8_t {
    // ── Loads ──────────────────────────────────────────────────────────────────
    LOADK,      // a=dst  b=kidx           regs[a] = consts[b]
    LOADBOOL,   // a=dst  b=0or1           regs[a] = bool(b != 0)
    LOADNONE,   // a=dst                   regs[a] = None
    MOVE,       // a=dst  b=src            regs[a] = regs[b]

    // ── Upvalue access (for closure-captured variables from outer functions) ──
    GETUPVAL,   // a=dst  b=uvidx          regs[a] = *upvals[b]
    SETUPVAL,   // a=uvidx  b=src          *upvals[a] = regs[b]

    // ── Cell-local access (variables captured by *inner* functions) ───────────
    // Cells are shared_ptr<VMValue> slots that outlive the stack frame.
    LOADCELL,   // a=dst  b=cellidx        regs[a] = *cells[b]
    STORECELL,  // a=cellidx  b=src        *cells[a] = regs[b]

    // ── Binary arithmetic  (a=dst  b=s1  c=s2) ────────────────────────────────
    ADD, SUB, MUL, DIV,

    // ── Comparison → Bool result  (a=dst  b=s1  c=s2) ─────────────────────────
    EQ, NEQ, LT, LE, GT, GE,

    // ── Logical (Bool operands only;  a=dst  b=s1  c=s2) ─────────────────────
    AND, OR, XOR,

    // ── Unary  (a=dst  b=src) ─────────────────────────────────────────────────
    UMINUS, UPLUS, NOT,

    // ── Type predicate ─────────────────────────────────────────────────────────
    ISTYPE,     // a=dst  b=src  c=TypeTag    regs[a] = (type_of(regs[b]) == c)

    // ── Control flow  (absolute PC targets) ───────────────────────────────────
    JMP,        // b=target
    JMPT,       // a=cond  b=target          if regs[a]: pc = b
    JMPF,       // a=cond  b=target          if !regs[a]: pc = b

    // ── Collection construction ────────────────────────────────────────────────
    NEWARRAY,   // a=dst  b=base  c=count    regs[a] = [regs[b..b+c)]  (1-based keys)
    // d=nkidx: names are consts[nkidx], consts[nkidx+1] ... consts[nkidx+count-1]
    NEWTUPLE,   // a=dst  b=base  c=count  d=nkidx
    GETINDEX,   // a=dst  b=base  c=key    regs[a] = regs[b][regs[c]]
    SETINDEX,   // a=base  b=key  c=val    regs[a][regs[b]] = regs[c]
    // c=kidx: field name (or 1-based int index for positional tuple access)
    GETFIELD,   // a=dst  b=base  c=kidx   regs[a] = regs[b].consts[c]
    SETFIELD,   // a=base  b=kidx  c=val   regs[a].consts[b] = regs[c]

    // ── Function / closure ─────────────────────────────────────────────────────
    CLOSURE,    // a=dst  b=proto_idx       create closure from protos[b]
    // a=dst  b=func  c=args_base  d=argc
    CALL,       // regs[a] = regs[b](regs[c], ..., regs[c+d-1])
    RETURN,     // a=src                   return regs[a]
    RETURNNONE, //                         return None

    // ── Iterator (for-in loops) ────────────────────────────────────────────────
    ITERINIT,   // a=dst  b=src            regs[a] = make_iterator(regs[b])
    // a=elem_dst  b=iter  c=exit_target
    ITERNEXT,   // advance; if exhausted: pc=c; else regs[a] = next element

    // ── I/O ────────────────────────────────────────────────────────────────────
    PRINT,      // a=base  b=count         print regs[a], ..., regs[a+b-1]

    NOP,
};

// ── Type tags for ISTYPE ───────────────────────────────────────────────────────
enum class TypeTag : int32_t {
    Int   = 0,
    Real  = 1,
    Bool  = 2,
    Str   = 3,
    None  = 4,
    Array = 5,
    Tuple = 6,
    Func  = 7,
};

// ── Instruction ────────────────────────────────────────────────────────────────
struct Instr {
    Opc     op = Opc::NOP;
    int32_t a  = -1;
    int32_t b  = -1;
    int32_t c  = -1;
    int32_t d  = -1;  // used by NEWTUPLE and CALL
};

// ── Constant pool element ──────────────────────────────────────────────────────
using ConstVal = std::variant<std::monostate, long long, double, std::string>;

// ── Upvalue descriptor ─────────────────────────────────────────────────────────
struct UpvalDesc {
    bool        is_local = false;  // true → capture cell[idx] from enclosing frame
    int32_t     idx      = -1;     // cell index (is_local) or parent upval index
    std::string name;              // debug
};

// ── Function prototype ─────────────────────────────────────────────────────────
// Immutable after compilation; shared by closures at runtime.
struct Proto {
    std::string name;      // debug identifier, e.g. "<main>", "<func@3:5>"
    int params  = 0;       // number of formal parameters
    int regs    = 0;       // total virtual registers allocated
    int cells   = 0;       // number of cell-local slots (shared_ptr<VMValue>)

    std::vector<Instr>                   code;
    std::vector<ConstVal>                consts;
    std::vector<std::shared_ptr<Proto>>  protos;  // nested function prototypes
    std::vector<UpvalDesc>               upvals;  // upvalue descriptors

    // Append a constant to the pool; returns its index.
    int add_const(ConstVal v);
};

// ── Module ─────────────────────────────────────────────────────────────────────
struct Module {
    std::shared_ptr<Proto> main;
};

// ── Disassembler ───────────────────────────────────────────────────────────────
void disassemble(const Proto& p, std::ostream& os, int depth = 0);
void disassemble(const Module& m, std::ostream& os);
