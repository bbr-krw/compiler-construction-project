#pragma once

#include <string>

// Tokenize `input` and return a multiline string with one token per line.
// Format:
//   KEYWORD/OPERATOR tokens: "TOK_NAME\n"
//   Valued tokens:           "TOK_NAME(value)\n"
//   End-of-input:            "YYEOF\n"
std::string dump_tokens(const std::string& input);
