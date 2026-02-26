#include "token_dump.hpp"

#include "lexer.hpp"
#include "parser.tab.hpp"

#include <format>
#include <sstream>

// Map symbol_kind_type → human-readable name.
// symbol_name() is only available under YYDEBUG, so we provide our own.
static const char* kind_name(yy::parser::symbol_kind_type k) {
    using K = yy::parser::symbol_kind_type;
    switch (k) {
    case K::S_YYEOF:
        return "YYEOF";
    case K::S_TOK_VAR:
        return "TOK_VAR";
    case K::S_TOK_IF:
        return "TOK_IF";
    case K::S_TOK_THEN:
        return "TOK_THEN";
    case K::S_TOK_ELSE:
        return "TOK_ELSE";
    case K::S_TOK_END:
        return "TOK_END";
    case K::S_TOK_WHILE:
        return "TOK_WHILE";
    case K::S_TOK_FOR:
        return "TOK_FOR";
    case K::S_TOK_IN:
        return "TOK_IN";
    case K::S_TOK_LOOP:
        return "TOK_LOOP";
    case K::S_TOK_EXIT:
        return "TOK_EXIT";
    case K::S_TOK_RETURN:
        return "TOK_RETURN";
    case K::S_TOK_PRINT:
        return "TOK_PRINT";
    case K::S_TOK_FUNC:
        return "TOK_FUNC";
    case K::S_TOK_IS:
        return "TOK_IS";
    case K::S_TOK_NOT:
        return "TOK_NOT";
    case K::S_TOK_AND:
        return "TOK_AND";
    case K::S_TOK_OR:
        return "TOK_OR";
    case K::S_TOK_XOR:
        return "TOK_XOR";
    case K::S_TOK_NONE:
        return "TOK_NONE";
    case K::S_TOK_ARROW:
        return "TOK_ARROW";
    case K::S_TOK_DOTDOT:
        return "TOK_DOTDOT";
    case K::S_TOK_ASSIGN:
        return "TOK_ASSIGN";
    case K::S_TOK_LT:
        return "TOK_LT";
    case K::S_TOK_LE:
        return "TOK_LE";
    case K::S_TOK_GT:
        return "TOK_GT";
    case K::S_TOK_GE:
        return "TOK_GE";
    case K::S_TOK_EQ:
        return "TOK_EQ";
    case K::S_TOK_NEQ:
        return "TOK_NEQ";
    case K::S_TOK_PLUS:
        return "TOK_PLUS";
    case K::S_TOK_MINUS:
        return "TOK_MINUS";
    case K::S_TOK_STAR:
        return "TOK_STAR";
    case K::S_TOK_SLASH:
        return "TOK_SLASH";
    case K::S_TOK_LPAREN:
        return "TOK_LPAREN";
    case K::S_TOK_RPAREN:
        return "TOK_RPAREN";
    case K::S_TOK_LBRACKET:
        return "TOK_LBRACKET";
    case K::S_TOK_RBRACKET:
        return "TOK_RBRACKET";
    case K::S_TOK_LBRACE:
        return "TOK_LBRACE";
    case K::S_TOK_RBRACE:
        return "TOK_RBRACE";
    case K::S_TOK_DOT:
        return "TOK_DOT";
    case K::S_TOK_COMMA:
        return "TOK_COMMA";
    case K::S_TOK_SEMI:
        return "TOK_SEMI";
    case K::S_TOK_TYPE_INT:
        return "TOK_TYPE_INT";
    case K::S_TOK_TYPE_REAL:
        return "TOK_TYPE_REAL";
    case K::S_TOK_TYPE_BOOL:
        return "TOK_TYPE_BOOL";
    case K::S_TOK_TYPE_STRING:
        return "TOK_TYPE_STRING";
    case K::S_TOK_INTEGER:
        return "TOK_INTEGER";
    case K::S_TOK_TRUE:
        return "TOK_TRUE";
    case K::S_TOK_FALSE:
        return "TOK_FALSE";
    case K::S_TOK_REAL:
        return "TOK_REAL";
    case K::S_TOK_STRING:
        return "TOK_STRING";
    case K::S_TOK_IDENT:
        return "TOK_IDENT";
    default:
        return "UNKNOWN";
    }
}

std::string dump_tokens(const std::string& input) {
    std::ostringstream out;

    std::istringstream in(input);
    Lexer lexer{in};

    while (true) {
        yy::parser::symbol_type sym = lexer.next();
        const auto kind             = sym.kind();
        const char* name            = kind_name(kind);
        const auto [line, col]      = lexer.location();
        const auto location         = std::format("{:>4}:{:<4}", line, col);

        switch (kind) {
        // ── Valued: long long ──────────────────────────────────────────
        case yy::parser::symbol_kind::S_TOK_INTEGER:
        case yy::parser::symbol_kind::S_TOK_TRUE:
        case yy::parser::symbol_kind::S_TOK_FALSE:
            out << location << name << "(" << sym.value.as<long long>() << ")\n";
            break;

        // ── Valued: double ────────────────────────────────────────────
        case yy::parser::symbol_kind::S_TOK_REAL:
            out << location << name << "(" << std::format("{:g}", sym.value.as<double>()) << ")\n";
            break;

        // ── Valued: string ────────────────────────────────────────────
        case yy::parser::symbol_kind::S_TOK_STRING:
        case yy::parser::symbol_kind::S_TOK_IDENT:
            out << location << name << "(" << sym.value.as<std::string>() << ")\n";
            break;

        // ── End of input ──────────────────────────────────────────────
        case yy::parser::symbol_kind::S_YYEOF:
            out << location << "YYEOF\n";
            return out.str();

        // ── Keywords and operators ────────────────────────────────────
        default:
            out << location << name << "\n";
            break;
        }
    }
}
