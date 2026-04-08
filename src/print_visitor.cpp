#include "print_visitor.hpp"

#include "ast.hpp"

#include <format>
#include <ostream>
#include <string_view>

// ── Helpers ───────────────────────────────────────────────────────────────────

void PrintVisitor::put_indent() const {
    for (int i = 0; i < indent_; ++i)
        os_ << "  ";
}

void PrintVisitor::put_suffix(const ASTNode& n) {
    os_ << "  (loc " << n.line << ':' << n.col << ")\n";
}

void PrintVisitor::recurse(const ASTNode* n) {
    if (!n)
        return;
    ++indent_;
    n->accept(*this);
    --indent_;
}

// ── Statements / structure ────────────────────────────────────────────────────

void PrintVisitor::visit(const ProgramNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    for (const auto& s : n.stmts)
        recurse(s.get());
}

void PrintVisitor::visit(const BodyNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    for (const auto& s : n.stmts)
        recurse(s.get());
}

void PrintVisitor::visit(const VarDeclNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    for (const auto& d : n.defs)
        recurse(d.get());
}

void PrintVisitor::visit(const VarDefNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    os_ << " name=" << n.varname;
    put_suffix(n);
    recurse(n.init.get());
}

void PrintVisitor::visit(const AssignNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.lhs.get());
    recurse(n.rhs.get());
}

void PrintVisitor::visit(const IfNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.cond.get());
    recurse(n.then_body.get());
    recurse(n.else_body.get());
}

void PrintVisitor::visit(const IfShortNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.cond.get());
    recurse(n.stmt.get());
}

void PrintVisitor::visit(const WhileNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.cond.get());
    recurse(n.body.get());
}

void PrintVisitor::visit(const ForRangeNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    if (!n.iter.empty())
        os_ << " name=" << n.iter;
    put_suffix(n);
    recurse(n.from.get());
    recurse(n.to.get());
    recurse(n.body.get());
}

void PrintVisitor::visit(const ForIterNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    if (!n.iter.empty())
        os_ << " name=" << n.iter;
    put_suffix(n);
    recurse(n.iterable.get());
    recurse(n.body.get());
}

void PrintVisitor::visit(const LoopInfNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.body.get());
}

void PrintVisitor::visit(const ExitNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
}

void PrintVisitor::visit(const ReturnNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.value.get());
}

void PrintVisitor::visit(const PrintNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    for (const auto& e : n.exprs)
        recurse(e.get());
}

// ── Binary operators ──────────────────────────────────────────────────────────

void PrintVisitor::visit(const BinOpNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.left.get());
    recurse(n.right.get());
}

// ── Unary operators ───────────────────────────────────────────────────────────

void PrintVisitor::visit(const UnaryOpNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.operand.get());
}

void PrintVisitor::visit(const IsNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.operand.get());
    recurse(n.type_node.get());
}

// ── Postfix / access ──────────────────────────────────────────────────────────

void PrintVisitor::visit(const IdentNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    os_ << ' ' << n.ident_name;
    put_suffix(n);
}

void PrintVisitor::visit(const IndexNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.base.get());
    recurse(n.index_expr.get());
}

void PrintVisitor::visit(const CallNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.callee.get());
    for (const auto& a : n.args)
        recurse(a.get());
}

void PrintVisitor::visit(const DotFieldNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    os_ << " name=" << n.field;
    put_suffix(n);
    recurse(n.base.get());
}

void PrintVisitor::visit(const DotIntNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    os_ << ' ' << n.index;
    os_ << "  (." << n.index << ") (loc " << n.line << ':' << n.col << ")\n";
    recurse(n.base.get());
}

// ── Literals ──────────────────────────────────────────────────────────────────

void PrintVisitor::visit(const IntLitNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    os_ << ' ' << n.value;
    put_suffix(n);
}

void PrintVisitor::visit(const RealLitNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    os_ << ' ' << std::format("{:g}", n.value);
    put_suffix(n);
}

void PrintVisitor::visit(const StrLitNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    os_ << ' ' << n.value;
    put_suffix(n);
}

void PrintVisitor::visit(const BoolLitNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    os_ << ' ' << (n.value ? 1LL : 0LL);
    os_ << "  (" << (n.value ? "true" : "false") << ") (loc " << n.line << ':' << n.col << ")\n";
}

void PrintVisitor::visit(const NoneLitNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
}

void PrintVisitor::visit(const ArrayLitNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    for (const auto& e : n.elems)
        recurse(e.get());
}

void PrintVisitor::visit(const TupleLitNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    for (const auto& e : n.elems)
        recurse(e.get());
}

void PrintVisitor::visit(const TupleElemNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    if (!n.elem_name.empty())
        os_ << " name=" << n.elem_name;
    put_suffix(n);
    recurse(n.expr.get());
}

void PrintVisitor::visit(const ParamListNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    for (const auto& p : n.params)
        recurse(p.get());
}

void PrintVisitor::visit(const FuncLitNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
    recurse(n.params.get());
    recurse(n.body.get());
}

// ── Type indicators ───────────────────────────────────────────────────────────

void PrintVisitor::visit(const TypeNode& n) {
    put_indent();
    os_ << '[' << n.kind_name() << ']';
    put_suffix(n);
}
