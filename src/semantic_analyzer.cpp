#include "semantic_analyzer.hpp"

#include "ast.hpp"

#include <format>

void SemanticAnalyzer::analyze(const ASTNode& root) {
    errors_.clear();
    scopes_.clear();
    loop_depth_ = 0;
    func_depth_ = 0;
    push_scope();
    root.accept(*this);
    pop_scope();
}

void SemanticAnalyzer::push_scope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::pop_scope() {
    scopes_.pop_back();
}

void SemanticAnalyzer::declare(const std::string& name, Location loc) {
    auto& scope = scopes_.back();
    auto it     = scope.find(name);
    if (it != scope.end()) {
        error(loc, std::format("'{}' already declared in this scope (previously at line {})", name,
                               it->second));
    } else {
        scope[name] = loc.line;
    }
}

int SemanticAnalyzer::resolve(const std::string& name, Location loc) {
    for (int d = static_cast<int>(scopes_.size()) - 1; d >= 0; --d) {
        if (scopes_[d].count(name))
            return static_cast<int>(scopes_.size()) - 1 - d;
    }
    error(loc, std::format("use of undeclared variable '{}'", name));
    return -1;
}

void SemanticAnalyzer::error(Location loc, std::string msg) {
    errors_.push_back({loc, std::move(msg)});
}

void SemanticAnalyzer::accept(const ASTNode* n) {
    if (n)
        n->accept(*this);
}

void SemanticAnalyzer::visit(const ProgramNode& n) {
    for (const auto& s : n.stmts)
        accept(s.get());
}

void SemanticAnalyzer::visit(const BodyNode& n) {
    push_scope();
    for (const auto& s : n.stmts)
        accept(s.get());
    pop_scope();
}

void SemanticAnalyzer::visit(const VarDeclNode& n) {
    for (const auto& d : n.defs)
        accept(d.get());
}

void SemanticAnalyzer::visit(const VarDefNode& n) {

    const bool is_func_init = n.init && dynamic_cast<const FuncLitNode*>(n.init.get()) != nullptr;
    if (is_func_init)
        declare(n.varname, n.loc);
    if (n.init)
        accept(n.init.get());
    if (!is_func_init)
        declare(n.varname, n.loc);
}

void SemanticAnalyzer::visit(const AssignNode& n) {
    accept(n.lhs.get());
    accept(n.rhs.get());
}

void SemanticAnalyzer::visit(const IfNode& n) {
    accept(n.cond.get());
    accept(n.then_body.get());
    if (n.else_body)
        accept(n.else_body.get());
}

void SemanticAnalyzer::visit(const IfShortNode& n) {
    accept(n.cond.get());
    accept(n.stmt.get());
}

void SemanticAnalyzer::visit(const WhileNode& n) {
    accept(n.cond.get());
    ++loop_depth_;
    accept(n.body.get());
    --loop_depth_;
}

void SemanticAnalyzer::visit(const ForRangeNode& n) {
    accept(n.from.get());
    accept(n.to.get());
    ++loop_depth_;
    push_scope();
    if (!n.iter.empty())
        declare(n.iter, n.loc);
    accept(n.body.get());
    pop_scope();
    --loop_depth_;
}

void SemanticAnalyzer::visit(const ForIterNode& n) {
    accept(n.iterable.get());
    ++loop_depth_;
    push_scope();
    if (!n.iter.empty())
        declare(n.iter, n.loc);
    accept(n.body.get());
    pop_scope();
    --loop_depth_;
}

void SemanticAnalyzer::visit(const LoopInfNode& n) {
    ++loop_depth_;
    accept(n.body.get());
    --loop_depth_;
}

void SemanticAnalyzer::visit(const ExitNode& n) {
    if (!in_loop())
        error(n.loc, "'exit' used outside of a loop");
}

void SemanticAnalyzer::visit(const ReturnNode& n) {
    if (!in_func())
        error(n.loc, "'return' used outside of a function");
    if (n.value)
        accept(n.value.get());
}

void SemanticAnalyzer::visit(const PrintNode& n) {
    for (const auto& e : n.exprs)
        accept(e.get());
}

void SemanticAnalyzer::visit(const BinOpNode& n) {
    accept(n.left.get());
    accept(n.right.get());
}

void SemanticAnalyzer::visit(const UnaryOpNode& n) {
    accept(n.operand.get());
}

void SemanticAnalyzer::visit(const IsNode& n) {
    accept(n.operand.get());
}

void SemanticAnalyzer::visit(const IdentNode& n) {
    n.resolved_depth = resolve(n.ident_name, n.loc);
}

void SemanticAnalyzer::visit(const IndexNode& n) {
    accept(n.base.get());
    accept(n.index_expr.get());
}

void SemanticAnalyzer::visit(const CallNode& n) {
    accept(n.callee.get());
    for (const auto& a : n.args)
        accept(a.get());
}

void SemanticAnalyzer::visit(const DotFieldNode& n) {
    accept(n.base.get());
}

void SemanticAnalyzer::visit(const DotIntNode& n) {
    accept(n.base.get());
}

void SemanticAnalyzer::visit(const ArrayLitNode& n) {
    for (const auto& e : n.elems)
        accept(e.get());
}

void SemanticAnalyzer::visit(const TupleLitNode& n) {
    for (const auto& e : n.elems)
        accept(e.get());
}

void SemanticAnalyzer::visit(const TupleElemNode& n) {
    accept(n.expr.get());
}

void SemanticAnalyzer::visit(const ParamListNode& n) {
    for (const auto& p : n.params) {
        const auto* ident = static_cast<const IdentNode*>(p.get());
        declare(ident->ident_name, ident->loc);
    }
}

void SemanticAnalyzer::visit(const FuncLitNode& n) {
    ++func_depth_;
    push_scope();
    if (n.params)
        visit(static_cast<const ParamListNode&>(*n.params));
    accept(n.body.get());
    pop_scope();
    --func_depth_;
}
