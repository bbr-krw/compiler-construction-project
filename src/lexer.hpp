#pragma once

#include "parser.tab.hpp"

#include <cstdint>
#include <istream>
#include <string>

class Lexer {
public:
    Lexer(std::istream& input);

    yy::parser::symbol_type next();

    struct Location {
        int line;
        int col;
    };

    /**
     * Returns `Location` where last returned token starts
     */
    Location begin_location() const;

    /**
     * Returns `Location` where last returned token ends
     */
    Location end_location() const;

private:
    std::istream& _input;

    std::vector<int> _line_size = {INT32_MIN, 0};
    Location _begin_location  = {.line = 1, .col = 1};
    Location _end_location          = {.line = 1, .col = 1};

    void next_line();
    void next_col();
    void prev_line();
    void prev_col();

    char getch();
    void ungetch(char c);
};
