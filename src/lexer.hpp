#pragma once

#include <string>
#include <istream>
#include "parser.tab.hpp"

class Lexer {
public:
    Lexer(std::istream& input);

    yy::parser::symbol_type next();

    struct Location {
        int line;
        int col;
    };

    Location location() const;

private:
    std::istream& _input;
    Location _location = {
        .line = 1,
        .col = 1
    };

    char getch();
    void ungetch(char c);
};
