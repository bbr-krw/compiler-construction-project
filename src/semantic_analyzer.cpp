#include "semantic_analyzer.hpp"

#include "ast.hpp"

#include <format>

// ── Public API ────────────────────────────────────────────────────────────────

void SemanticAnalyzer::analyze(const ASTNode& root) {
    errors_.clear();
    scopes_.clear();
    loop_depth_ = 0;
    func_depth_ = 0;
    push_scope(); // global scope
    root.accept(*this);
    pop_scope();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void SemanticAnalyzer::push_scope() {
    scopes_.emplace_back();
}

void SemanticAnalyzer::pop_scope() {
    scopes_.pop_back();
}

void SemanticAnalyzer::declare(const std::string& name, int line, int col) {
    auto& scope = scopes_.back();
    auto  it    = scope.find(name);
    if (it != scope.end()) {
        error(line, col,
              std::format("'{}' already declared in this scope (previously at line {})",
                          name, it->second));
    } else {
        scope[name] = line;
    }
}

void SemanticAnalyzer::resolve(const std::string& name, int line, int col) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        if (it->count(name))
            return;
    }
    error(line, col, std::format("use of undeclared variable '{}'", name));
}

void SemanticAnalyzer::error(int line, int col, std::string msg) {
    errors_.push_back({line, col, std::move(msg)});
}

void SemanticAnalyzer::accept(const ASTNode* n) {
    if (n)
        n->accept(*this);
}

// ── Statements / structure ────────────────────────────────────────────────────

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
    // For function-literal initialisers, declare the name *before* visiting the
    // body so that recursive self-references (e.g. var fact := func(n) is
    // ... fact(n-1) ... end) are valid.  The function body is executed at call
    // time, so the variable is already in scope when the body actually runs.
    //
    // For all other initialisers, evaluate first so that `var x := x` correctly
    // reports x as undeclared.
    const bool is_func_init =
        n.init && dynamic_cast<const FuncLitNode*>(n.init.get()) != nullptr;
    if (is_func_init)
        declare(n.varname, n.line, n.col);
    if (n.init)
        accept(n.init.get());
    if (!is_func_init)
        declare(n.varname, n.line, n.col);
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
    // The single statement shares the enclosing scope (no new Body wrapper).
    accept(n.stmt.get());
}

void SemanticAnalyzer::visit(const WhileNode& n) {
    accept(n.cond.get());
    ++loop_depth_;
    accept(n.body.get());
    --loop_depth_;
}

void SemanticAnalyzer::visit(const ForRangeNode& n) {
    // Evaluate bounds in the outer scope.
    accept(n.from.get());
    accept(n.to.get());
    ++loop_depth_;
    // Iterator variable is scoped to the loop body.
    push_scope();
    if (!n.iter.empty())
        declare(n.iter, n.line, n.col);
    accept(n.body.get());
    pop_scope();
    --loop_depth_;
}

void SemanticAnalyzer::visit(const ForIterNode& n) {
    // Evaluate the iterable in the outer scope.
    accept(n.iterable.get());
    ++loop_depth_;
    push_scope();
    if (!n.iter.empty())
        declare(n.iter, n.line, n.col);
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
        error(n.line, n.col, "'exit' used outside of a loop");
}

void SemanticAnalyzer::visit(const ReturnNode& n) {
    if (!in_func())
        error(n.line, n.col, "'return' used outside of a function");
    if (n.value)
        accept(n.value.get());
}

void SemanticAnalyzer::visit(const PrintNode& n) {
    for (const auto& e : n.exprs)
        accept(e.get());
}

// ── Expressions ───────────────────────────────────────────────────────────────

void SemanticAnalyzer::visit(const BinOpNode& n) {
    accept(n.left.get());
    accept(n.right.get());
}

void SemanticAnalyzer::visit(const UnaryOpNode& n) {
    accept(n.operand.get());
}

void SemanticAnalyzer::visit(const IsNode& n) {
    accept(n.operand.get());
    // n.type_node is a TypeNode — always structurally valid, no check needed.
}

void SemanticAnalyzer::visit(const IdentNode& n) {
    resolve(n.ident_name, n.line, n.col);
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

// ── Literals ──────────────────────────────────────────────────────────────────

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

// ── Function literal ──────────────────────────────────────────────────────────

void SemanticAnalyzer::visit(const ParamListNode& n) {
    // Parameters are declared by visit(FuncLitNode) before we recurse into body.
    // This overload is called from visit(FuncLitNode) to register all param names.
    for (const auto& p : n.params) {
        const auto* ident = static_cast<const IdentNode*>(p.get());
        declare(ident->ident_name, ident->line, ident->col);
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
