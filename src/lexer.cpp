#include "lexer.hpp"

#include "parser.tab.hpp"

#include <cctype>
#include <cstring>
#include <iostream>
#include <istream>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string, yy::parser::symbol_type (*)()> keywords = {
    {"var", yy::parser::make_TOK_VAR},
    {"if", yy::parser::make_TOK_IF},
    {"then", yy::parser::make_TOK_THEN},
    {"else", yy::parser::make_TOK_ELSE},
    {"end", yy::parser::make_TOK_END},
    {"while", yy::parser::make_TOK_WHILE},
    {"for", yy::parser::make_TOK_FOR},
    {"in", yy::parser::make_TOK_IN},
    {"loop", yy::parser::make_TOK_LOOP},
    {"exit", yy::parser::make_TOK_EXIT},
    {"return", yy::parser::make_TOK_RETURN},
    {"print", yy::parser::make_TOK_PRINT},
    {"func", yy::parser::make_TOK_FUNC},
    {"is", yy::parser::make_TOK_IS},
    {"not", yy::parser::make_TOK_NOT},
    {"and", yy::parser::make_TOK_AND},
    {"or", yy::parser::make_TOK_OR},
    {"xor", yy::parser::make_TOK_XOR},
    {"none", yy::parser::make_TOK_NONE},
    {"int", yy::parser::make_TOK_TYPE_INT},
    {"real", yy::parser::make_TOK_TYPE_REAL},
    {"bool", yy::parser::make_TOK_TYPE_BOOL},
    {"string", yy::parser::make_TOK_TYPE_STRING}};

Lexer::Lexer(std::istream& in) : _input(in) {}

char Lexer::getch() {
    _location.col++;
    return _input.get();
}

void Lexer::ungetch(char c) {
    _location.col--;
    if (c != EOF)
        _input.putback(c);
}

Lexer::Location Lexer::location() const {
    return _location;
}

yy::parser::symbol_type Lexer::next() {
    int c;

    /* Skip whitespace */
    while ((c = getch()) != EOF) {
        if (c == '\n') {
            _location.line++;
            _location.col = 0;
        } else if (!isspace(c)) {
            break;
        }
    }

    if (c == EOF)
        return 0;

    /* ---------- IDENTIFIERS / KEYWORDS ---------- */
    if (isalpha(c) || c == '_') {
        std::string text;
        text += (char)c;

        while (true) {
            c = getch();
            if (isalnum(c) || c == '_')
                text += (char)c;
            else {
                ungetch(c);
                break;
            }
        }

        if (text == "true") {
            return yy::parser::make_TOK_TRUE(1);
        }
        if (text == "false") {
            return yy::parser::make_TOK_FALSE(0);
        }

        auto it = keywords.find(text);
        if (it != keywords.end()) {
            return it->second();
        }

        return yy::parser::make_TOK_IDENT(text);
    }

    /* ---------- NUMBERS ---------- */
    if (isdigit(c)) {
        std::string num;
        bool isReal = false;

        num += (char)c;

        while (true) {
            c = getch();

            if (isdigit(c)) {
                num += (char)c;
            } else if (c == '.') {
                /* Could be real or ".." */
                int next = getch();
                if (next == '.') {
                    /* integer followed by ".." */
                    ungetch(next);
                    ungetch('.');
                    break;
                } else {
                    isReal = true;
                    num += '.';
                    ungetch(next);
                }
            } else {
                ungetch(c);
                break;
            }
        }

        if (isReal) {
            return yy::parser::make_TOK_REAL(std::stod(num));
        } else {
            return yy::parser::make_TOK_INTEGER(std::stoll(num));
        }
    }

    /* ---------- STRING ---------- */
    if (c == '"' || c == '\'') {
        char quote = c;
        std::string str;

        while (true) {
            c = getch();
            if (c == EOF || c == quote)
                break;

            if (c == '\\') {
                int next = getch();
                switch (next) {
                case 'n':
                    str += '\n';
                    break;
                case 't':
                    str += '\t';
                    break;
                case '"':
                    str += '"';
                    break;
                case '\'':
                    str += '\'';
                    break;
                case '\\':
                    str += '\\';
                    break;
                default:
                    str += next;
                    break;
                }
            } else {
                str += (char)c;
            }
        }

        return yy::parser::make_TOK_STRING(str);
    }

    /* ---------- OPERATORS / PUNCTUATION ---------- */

    switch (c) {
    case '+':
        return yy::parser::make_TOK_PLUS();
    case '-':
        return yy::parser::make_TOK_MINUS();

    case '*':
        return yy::parser::make_TOK_STAR();
    case '/': {
        int n = getch();
        if (n == '=')
            return yy::parser::make_TOK_NEQ();
        ungetch(n);
        return yy::parser::make_TOK_SLASH();
    }

    case '(':
        return yy::parser::make_TOK_LPAREN();
    case ')':
        return yy::parser::make_TOK_RPAREN();

    case '[':
        return yy::parser::make_TOK_LBRACKET();
    case ']':
        return yy::parser::make_TOK_RBRACKET();

    case '{':
        return yy::parser::make_TOK_LBRACE();
    case '}':
        return yy::parser::make_TOK_RBRACE();

    case ',':
        return yy::parser::make_TOK_COMMA();
    case ';':
        return yy::parser::make_TOK_SEMI();
    case '.': {
        int n = getch();
        if (n == '.')
            return yy::parser::make_TOK_DOTDOT();
        ungetch(n);
        return yy::parser::make_TOK_DOT();
    }

    case ':': {
        int n = getch();
        if (n == '=')
            return yy::parser::make_TOK_ASSIGN();
        ungetch(n);
        break;
    }

    case '=': {
        int n = getch();
        if (n == '>')
            return yy::parser::make_TOK_ARROW();
        ungetch(n);
        return yy::parser::make_TOK_EQ();
    }

    case '<': {
        int n = getch();
        if (n == '=')
            return yy::parser::make_TOK_LE();
        ungetch(n);
        return yy::parser::make_TOK_LT();
    }

    case '>': {
        int n = getch();
        if (n == '=')
            return yy::parser::make_TOK_GE();
        ungetch(n);
        return yy::parser::make_TOK_GT();
    }
    }

    return c;
}
