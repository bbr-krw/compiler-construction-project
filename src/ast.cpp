#include "ast.hpp"

#include <print>

// ── Factory methods ───────────────────────────────────────────────────────────

ASTNode* ASTNode::make(NodeKind k, int ln) {
    return new ASTNode{k, ln};
}

ASTNode* ASTNode::make_int(long long v, int ln) {
    auto* n    = new ASTNode{NodeKind::INT_LIT, ln};
    n->payload = v;
    return n;
}

ASTNode* ASTNode::make_real(double v, int ln) {
    auto* n    = new ASTNode{NodeKind::REAL_LIT, ln};
    n->payload = v;
    return n;
}

ASTNode* ASTNode::make_str(std::string s, int ln) {
    auto* n    = new ASTNode{NodeKind::STR_LIT, ln};
    n->payload = std::move(s);
    return n;
}

ASTNode* ASTNode::make_ident(std::string s, int ln) {
    auto* n    = new ASTNode{NodeKind::IDENT, ln};
    n->payload = std::move(s);
    return n;
}

ASTNode* ASTNode::make_bool(bool v, int ln) {
    auto* n    = new ASTNode{NodeKind::BOOL_LIT, ln};
    n->payload = static_cast<long long>(v ? 1 : 0);
    return n;
}

ASTNode* ASTNode::make_none(int ln) {
    return new ASTNode{NodeKind::NONE_LIT, ln};
}

// ── Kind name ─────────────────────────────────────────────────────────────────

std::string_view ASTNode::kind_name() const noexcept {
    using enum NodeKind;
    switch (kind) {
        case PROGRAM:     return "Program";
        case VAR_DECL:    return "VarDecl";
        case VAR_DEF:     return "VarDef";
        case ASSIGN:      return "Assign";
        case IF:          return "If";
        case IF_SHORT:    return "IfShort";
        case WHILE:       return "While";
        case FOR_RANGE:   return "ForRange";
        case FOR_ITER:    return "ForIter";
        case LOOP_INF:    return "LoopInf";
        case EXIT:        return "Exit";
        case RETURN:      return "Return";
        case PRINT:       return "Print";
        case BODY:        return "Body";
        case OR:          return "Or";
        case AND:         return "And";
        case XOR:         return "Xor";
        case LT:          return "Lt";
        case LE:          return "Le";
        case GT:          return "Gt";
        case GE:          return "Ge";
        case EQ:          return "Eq";
        case NEQ:         return "Neq";
        case ADD:         return "Add";
        case SUB:         return "Sub";
        case MUL:         return "Mul";
        case DIV:         return "Div";
        case UPLUS:       return "UPlus";
        case UMINUS:      return "UMinus";
        case NOT:         return "Not";
        case IS:          return "Is";
        case IDENT:       return "Ident";
        case INDEX:       return "Index";
        case CALL:        return "Call";
        case DOT_FIELD:   return "DotField";
        case DOT_INT:     return "DotInt";
        case INT_LIT:     return "IntLit";
        case REAL_LIT:    return "RealLit";
        case STR_LIT:     return "StrLit";
        case BOOL_LIT:    return "BoolLit";
        case NONE_LIT:    return "NoneLit";
        case ARRAY_LIT:   return "ArrayLit";
        case TUPLE_LIT:   return "TupleLit";
        case TUPLE_ELEM:  return "TupleElem";
        case FUNC_LIT:    return "FuncLit";
        case PARAM_LIST:  return "ParamList";
        case TYPE_INT:    return "TypeInt";
        case TYPE_REAL:   return "TypeReal";
        case TYPE_BOOL:   return "TypeBool";
        case TYPE_STRING: return "TypeString";
        case TYPE_NONE:   return "TypeNone";
        case TYPE_ARRAY:  return "TypeArray";
        case TYPE_TUPLE:  return "TypeTuple";
        case TYPE_FUNC:   return "TypeFunc";
    }
    return "???";
}

// ── Pretty printer ────────────────────────────────────────────────────────────

void ASTNode::print(int indent) const {
    for (int i = 0; i < indent; ++i) std::print("  ");

    std::print("[{}]", kind_name());

    // Inline payload
    std::visit(overloaded{
        [](std::monostate)          {},
        [](long long v)             { std::print(" {}", v); },
        [](double v)                { std::print(" {:g}", v); },
        [](const std::string& s)    { std::print(" {}", s); },
    }, payload);

    // Extra name metadata
    if (!name.empty())
        std::print(" name={}", name);

    // Per-kind overrides for the inline suffix
    if (kind == NodeKind::BOOL_LIT)
        std::println("  ({}) (line {})", get_ival() ? "true" : "false", line);
    else if (kind == NodeKind::DOT_INT)
        std::println("  (.{}) (line {})", get_ival(), line);
    else
        std::println("  (line {})", line);

    for (const auto* child : children)
        if (child) child->print(indent + 1);
}
