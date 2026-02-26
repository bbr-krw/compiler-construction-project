#include "lexer.hpp"

#include "parser.tab.hpp"

#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>
#include <istream>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string, yy::parser::symbol_type (*)()> make_keyword = {
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
    char c = _input.get();
    if (c == '\n') {
        next_line();
    } else {
        next_col();
    }
    return c;
}

void Lexer::ungetch(char c) {
    if (c == '\n') {
        prev_line();
    } else {
        prev_col();
    }

    if (c != EOF) {
        _input.putback(c);
    }
}

void Lexer::next_line() {
    _location.line++;
    _location.col = 1;

    if (_line_size.size() <= _location.line) {
        assert(_line_size.size() == _location.line);

        _line_size.push_back(1);
    }
}

void Lexer::next_col() {
    assert(_location.line < _line_size.size());
    _location.col++;

    int& line_size = _line_size[_location.line];
    line_size      = std::max(line_size, _location.col);
}

void Lexer::prev_line() {
    assert(_location.line > 1 && _location.line < _line_size.size());

    _location.line--;
    _location.col = _line_size[_location.line];
}

void Lexer::prev_col() {
    assert(_location.col > 0);
    _location.col--;
}

Lexer::Location Lexer::location() const {
    return _begin_location;
}

yy::parser::symbol_type Lexer::next() {
    _begin_location = _location;

    int c;

    /* Skip whitespace and commentaries */
    while ((c = getch()) != EOF) {
        if (c == '/') {
            if ((c = getch()) == '/') {
                // find comment, move cursor to next line or EOF
                do {
                    c = getch();
                } while (c != '\n' && c != EOF);
            } else {
                // rollback, assume '/' is a part of token
                ungetch(c);
                c = '/';
            }
        }
        if (!isspace(c)) {
            break;
        }
        _begin_location = _location;
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

        auto it = make_keyword.find(text);
        if (it != make_keyword.end()) {
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
        if (n == '/') {
            while ((c = getch()) != '\n');
        }
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

    const auto msg = std::format("syntax error: invalid token at {}:{}", 
        _begin_location.line, _begin_location.col);
    throw yy::parser::syntax_error(msg);
}
