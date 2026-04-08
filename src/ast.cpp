#include "ast.hpp"

#include "print_visitor.hpp"

#include <iostream>
#include <ostream>

// ── Base ──────────────────────────────────────────────────────────────────────

void ASTNode::print(int indent, std::ostream& os) const {
    PrintVisitor v{os, indent};
    accept(v);
}

void ASTNode::print(int indent) const {
    print(indent, std::cout);
}

// ── Factory helpers ───────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ASTNode::make_int(long long v, Location loc) {
    return std::make_unique<IntLitNode>(v, loc);
}
std::unique_ptr<ASTNode> ASTNode::make_real(double v, Location loc) {
    return std::make_unique<RealLitNode>(v, loc);
}
std::unique_ptr<ASTNode> ASTNode::make_str(std::string s, Location loc) {
    return std::make_unique<StrLitNode>(std::move(s), loc);
}
std::unique_ptr<ASTNode> ASTNode::make_ident(std::string s, Location loc) {
    return std::make_unique<IdentNode>(std::move(s), loc);
}
std::unique_ptr<ASTNode> ASTNode::make_bool(bool v, Location loc) {
    return std::make_unique<BoolLitNode>(v, loc);
}
std::unique_ptr<ASTNode> ASTNode::make_none(Location loc) {
    return std::make_unique<NoneLitNode>(loc);
}

// ── BinOpNode::kind_name ──────────────────────────────────────────────────────

std::string_view BinOpNode::kind_name() const noexcept {
    switch (op) {
    case Op::OR:
        return "Or";
    case Op::AND:
        return "And";
    case Op::XOR:
        return "Xor";
    case Op::LT:
        return "Lt";
    case Op::LE:
        return "Le";
    case Op::GT:
        return "Gt";
    case Op::GE:
        return "Ge";
    case Op::EQ:
        return "Eq";
    case Op::NEQ:
        return "Neq";
    case Op::ADD:
        return "Add";
    case Op::SUB:
        return "Sub";
    case Op::MUL:
        return "Mul";
    case Op::DIV:
        return "Div";
    }
    return "???";
}

// ── UnaryOpNode::kind_name ────────────────────────────────────────────────────

std::string_view UnaryOpNode::kind_name() const noexcept {
    switch (op) {
    case Op::UPLUS:
        return "UPlus";
    case Op::UMINUS:
        return "UMinus";
    case Op::NOT:
        return "Not";
    }
    return "???";
}

// ── TypeNode::kind_name ───────────────────────────────────────────────────────

std::string_view TypeNode::kind_name() const noexcept {
    switch (type) {
    case Type::INT:
        return "TypeInt";
    case Type::REAL:
        return "TypeReal";
    case Type::BOOL:
        return "TypeBool";
    case Type::STRING:
        return "TypeString";
    case Type::NONE:
        return "TypeNone";
    case Type::ARRAY:
        return "TypeArray";
    case Type::TUPLE:
        return "TypeTuple";
    case Type::FUNC:
        return "TypeFunc";
    }
    return "???";
}
