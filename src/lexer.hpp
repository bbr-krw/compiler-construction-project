#pragma once

#include "parser.tab.hpp"

#include <cstdint>
#include <istream>
#include <string>

class Lexer {
public:
    Lexer(std::istream& input);

    yy::parser::symbol_type next();

    /**
     * Returns `yy::position` where last returned token starts
     */
    yy::position begin_location() const;

    /**
     * Returns `yy::position` where last returned token ends
     */
    yy::position end_location() const;

    /**
     * Returns the location (begin..end) of the last returned token
     */
    yy::parser::location_type token_location() const;

private:
    std::istream& _input;

    std::vector<int> _line_size  = {INT32_MIN, 0};
    yy::position _begin_location = yy::position(nullptr, 1, 1);
    yy::position _end_location   = yy::position(nullptr, 1, 1);
    yy::parser::location_type _token_location;

    void next_line();
    void next_col();
    void prev_line();
    void prev_col();

    /* Snapshot current begin/end into _token_location and return it. */
    yy::parser::location_type seal();

    char getch();
    void ungetch(char c);
};
