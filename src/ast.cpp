#include "ast.hpp"

#include <format>
#include <iostream>
#include <ostream>

// ── Base ──────────────────────────────────────────────────────────────────────

void ASTNode::print_suffix(std::ostream& os) const {
    os << "  (loc " << line << ":" << col << ")\n";
}

void ASTNode::print(int indent, std::ostream& os) const {
    for (int i = 0; i < indent; ++i)
        os << "  ";
    os << '[' << kind_name() << ']';
    print_inline(os);
    print_suffix(os);
    print_children(indent + 1, os);
}

void ASTNode::print(int indent) const {
    print(indent, std::cout);
}

// ── Factory helpers ───────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ASTNode::make_int(long long v, int ln, int col) {
    return std::make_unique<IntLitNode>(v, ln, col);
}
std::unique_ptr<ASTNode> ASTNode::make_real(double v, int ln, int col) {
    return std::make_unique<RealLitNode>(v, ln, col);
}
std::unique_ptr<ASTNode> ASTNode::make_str(std::string s, int ln, int col) {
    return std::make_unique<StrLitNode>(std::move(s), ln, col);
}
std::unique_ptr<ASTNode> ASTNode::make_ident(std::string s, int ln, int col) {
    return std::make_unique<IdentNode>(std::move(s), ln, col);
}
std::unique_ptr<ASTNode> ASTNode::make_bool(bool v, int ln, int col) {
    return std::make_unique<BoolLitNode>(v, ln, col);
}
std::unique_ptr<ASTNode> ASTNode::make_none(int ln, int col) {
    return std::make_unique<NoneLitNode>(ln, col);
}

// ── print helpers (file-local) ────────────────────────────────────────────────

static void pnode(const std::unique_ptr<ASTNode>& n, int ind, std::ostream& os) {
    if (n)
        n->print(ind, os);
}
static void pnodes(const std::vector<std::unique_ptr<ASTNode>>& v, int ind, std::ostream& os) {
    for (const auto& n : v)
        if (n)
            n->print(ind, os);
}

// ── print_children implementations ───────────────────────────────────────────

void ProgramNode::print_children(int ind, std::ostream& os) const {
    pnodes(stmts, ind, os);
}
void BodyNode::print_children(int ind, std::ostream& os) const {
    pnodes(stmts, ind, os);
}
void VarDeclNode::print_children(int ind, std::ostream& os) const {
    pnodes(defs, ind, os);
}
void VarDefNode::print_children(int ind, std::ostream& os) const {
    pnode(init, ind, os);
}
void AssignNode::print_children(int ind, std::ostream& os) const {
    pnode(lhs, ind, os);
    pnode(rhs, ind, os);
}
void IfNode::print_children(int ind, std::ostream& os) const {
    pnode(cond, ind, os);
    pnode(then_body, ind, os);
    pnode(else_body, ind, os);
}
void IfShortNode::print_children(int ind, std::ostream& os) const {
    pnode(cond, ind, os);
    pnode(stmt, ind, os);
}
void WhileNode::print_children(int ind, std::ostream& os) const {
    pnode(cond, ind, os);
    pnode(body, ind, os);
}
void ForRangeNode::print_children(int ind, std::ostream& os) const {
    pnode(from, ind, os);
    pnode(to, ind, os);
    pnode(body, ind, os);
}
void ForIterNode::print_children(int ind, std::ostream& os) const {
    pnode(iterable, ind, os);
    pnode(body, ind, os);
}
void LoopInfNode::print_children(int ind, std::ostream& os) const {
    pnode(body, ind, os);
}
void ReturnNode::print_children(int ind, std::ostream& os) const {
    pnode(value, ind, os);
}
void PrintNode::print_children(int ind, std::ostream& os) const {
    pnodes(exprs, ind, os);
}
void BinOpNode::print_children(int ind, std::ostream& os) const {
    pnode(left, ind, os);
    pnode(right, ind, os);
}
void UnaryOpNode::print_children(int ind, std::ostream& os) const {
    pnode(operand, ind, os);
}
void IsNode::print_children(int ind, std::ostream& os) const {
    pnode(operand, ind, os);
    pnode(type_node, ind, os);
}
void IndexNode::print_children(int ind, std::ostream& os) const {
    pnode(base, ind, os);
    pnode(index_expr, ind, os);
}
void CallNode::print_children(int ind, std::ostream& os) const {
    pnode(callee, ind, os);
    pnodes(args, ind, os);
}
void DotFieldNode::print_children(int ind, std::ostream& os) const {
    pnode(base, ind, os);
}
void DotIntNode::print_children(int ind, std::ostream& os) const {
    pnode(base, ind, os);
}
void ArrayLitNode::print_children(int ind, std::ostream& os) const {
    pnodes(elems, ind, os);
}
void TupleLitNode::print_children(int ind, std::ostream& os) const {
    pnodes(elems, ind, os);
}
void TupleElemNode::print_children(int ind, std::ostream& os) const {
    pnode(expr, ind, os);
}
void ParamListNode::print_children(int ind, std::ostream& os) const {
    pnodes(params, ind, os);
}
void FuncLitNode::print_children(int ind, std::ostream& os) const {
    pnode(params, ind, os);
    pnode(body, ind, os);
}

// ── print_inline implementations ─────────────────────────────────────────────

void VarDefNode::print_inline(std::ostream& os) const {
    os << " name=" << varname;
}
void ForRangeNode::print_inline(std::ostream& os) const {
    if (!iter.empty())
        os << " name=" << iter;
}
void ForIterNode::print_inline(std::ostream& os) const {
    if (!iter.empty())
        os << " name=" << iter;
}
void IdentNode::print_inline(std::ostream& os) const {
    os << ' ' << ident_name;
}
void DotFieldNode::print_inline(std::ostream& os) const {
    os << " name=" << field;
}
void DotIntNode::print_inline(std::ostream& os) const {
    os << ' ' << index;
}
void IntLitNode::print_inline(std::ostream& os) const {
    os << ' ' << value;
}
void RealLitNode::print_inline(std::ostream& os) const {
    os << ' ' << std::format("{:g}", value);
}
void StrLitNode::print_inline(std::ostream& os) const {
    os << ' ' << value;
}
void BoolLitNode::print_inline(std::ostream& os) const {
    os << ' ' << (value ? 1LL : 0LL);
}
void TupleElemNode::print_inline(std::ostream& os) const {
    if (!elem_name.empty())
        os << " name=" << elem_name;
}

// ── print_suffix overrides ────────────────────────────────────────────────────

void BoolLitNode::print_suffix(std::ostream& os) const {
    os << "  (" << (value ? "true" : "false") << ") (loc " << line << ":" << col << ")\n";
}
void DotIntNode::print_suffix(std::ostream& os) const {
    os << "  (." << index << ") (loc " << line << ":" << col << ")\n";
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
