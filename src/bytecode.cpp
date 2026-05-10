#include "bytecode.hpp"

#include <format>

int Proto::add_const(ConstVal v) {
    int idx = static_cast<int>(consts.size());
    consts.push_back(std::move(v));
    return idx;
}

// ── Helpers ────────────────────────────────────────────────────────────────────

static std::string const_to_str(const ConstVal& v) {
    return std::visit(
        []<class T>(const T& a) -> std::string {
            if constexpr (std::is_same_v<T, std::monostate>)
                return "none";
            else if constexpr (std::is_same_v<T, long long>)
                return std::to_string(a);
            else if constexpr (std::is_same_v<T, double>)
                return std::format("{:g}", a);
            else
                return "\"" + a + "\"";
        },
        v);
}

static const char* opc_name(Opc op) {
    switch (op) {
    case Opc::LOADK:      return "LOADK";
    case Opc::LOADBOOL:   return "LOADBOOL";
    case Opc::LOADNONE:   return "LOADNONE";
    case Opc::MOVE:       return "MOVE";
    case Opc::GETUPVAL:   return "GETUPVAL";
    case Opc::SETUPVAL:   return "SETUPVAL";
    case Opc::LOADCELL:   return "LOADCELL";
    case Opc::STORECELL:  return "STORECELL";
    case Opc::ADD:        return "ADD";
    case Opc::SUB:        return "SUB";
    case Opc::MUL:        return "MUL";
    case Opc::DIV:        return "DIV";
    case Opc::EQ:         return "EQ";
    case Opc::NEQ:        return "NEQ";
    case Opc::LT:         return "LT";
    case Opc::LE:         return "LE";
    case Opc::GT:         return "GT";
    case Opc::GE:         return "GE";
    case Opc::AND:        return "AND";
    case Opc::OR:         return "OR";
    case Opc::XOR:        return "XOR";
    case Opc::UMINUS:     return "UMINUS";
    case Opc::UPLUS:      return "UPLUS";
    case Opc::NOT:        return "NOT";
    case Opc::ISTYPE:     return "ISTYPE";
    case Opc::JMP:        return "JMP";
    case Opc::JMPT:       return "JMPT";
    case Opc::JMPF:       return "JMPF";
    case Opc::NEWARRAY:   return "NEWARRAY";
    case Opc::NEWTUPLE:   return "NEWTUPLE";
    case Opc::GETINDEX:   return "GETINDEX";
    case Opc::SETINDEX:   return "SETINDEX";
    case Opc::GETFIELD:   return "GETFIELD";
    case Opc::SETFIELD:   return "SETFIELD";
    case Opc::CLOSURE:    return "CLOSURE";
    case Opc::CALL:       return "CALL";
    case Opc::RETURN:     return "RETURN";
    case Opc::RETURNNONE: return "RETURNNONE";
    case Opc::ITERINIT:   return "ITERINIT";
    case Opc::ITERNEXT:   return "ITERNEXT";
    case Opc::PRINT:      return "PRINT";
    case Opc::NOP:        return "NOP";
    }
    return "???";
}

// ── Disassembler ───────────────────────────────────────────────────────────────

void disassemble(const Proto& p, std::ostream& os, int depth) {
    const std::string pad(static_cast<size_t>(depth * 2), ' ');

    os << pad << "=== '" << p.name << "'"
       << "  params=" << p.params
       << "  regs=" << p.regs
       << "  cells=" << p.cells << " ===\n";

    if (!p.consts.empty()) {
        os << pad << "  consts:\n";
        for (int i = 0; i < static_cast<int>(p.consts.size()); ++i)
            os << pad << "    [" << i << "] " << const_to_str(p.consts[i]) << "\n";
    }

    if (!p.upvals.empty()) {
        os << pad << "  upvals:\n";
        for (int i = 0; i < static_cast<int>(p.upvals.size()); ++i) {
            const auto& u = p.upvals[i];
            os << pad << "    [" << i << "] " << u.name
               << (u.is_local ? "  <- local cell " : "  <- upval ") << u.idx << "\n";
        }
    }

    os << pad << "  code:\n";
    for (int pc = 0; pc < static_cast<int>(p.code.size()); ++pc) {
        const Instr& ins = p.code[pc];
        os << pad << "    " << std::format("{:4d}  {:<12}", pc, opc_name(ins.op));
        if (ins.a != -1) os << "  a=" << ins.a;
        if (ins.b != -1) os << "  b=" << ins.b;
        if (ins.c != -1) os << "  c=" << ins.c;
        if (ins.d != -1) os << "  d=" << ins.d;
        // Inline annotation for constants
        if (ins.op == Opc::LOADK && ins.b >= 0 && ins.b < static_cast<int>(p.consts.size()))
            os << "  ; " << const_to_str(p.consts[ins.b]);
        if ((ins.op == Opc::GETFIELD || ins.op == Opc::SETFIELD) && ins.c >= 0 &&
            ins.c < static_cast<int>(p.consts.size()))
            os << "  ; ." << const_to_str(p.consts[ins.c]);
        os << "\n";
    }

    for (const auto& sub : p.protos)
        disassemble(*sub, os, depth + 1);
}

void disassemble(const Module& m, std::ostream& os) {
    disassemble(*m.main, os);
}
