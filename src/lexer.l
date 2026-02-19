%{
/*
 * lexer.l – Flex scanner for the D language (C++23)
 *
 * Returns yy::parser::symbol_type objects via Bison's api.token.constructor.
 * The YY_DECL macro is injected by parser.tab.hpp (%code provides).
 */
#include "ast.hpp"
#include "parser.tab.hpp"

#include <cstdlib>
#include <cstring>
#include <print>
#include <string>

// Strip surrounding quotes and handle basic escape sequences.
static std::string strip_quotes(const char* s) {
    std::string result;
    const std::size_t len = std::strlen(s);
    result.reserve(len - 2);
    for (std::size_t i = 1; i < len - 1; ++i) {
        if (s[i] == '\\' && i + 1 < len - 1) {
            switch (s[++i]) {
                case 'n':  result += '\n'; break;
                case 't':  result += '\t'; break;
                case '"':  result += '"';  break;
                case '\'': result += '\''; break;
                case '\\': result += '\\'; break;
                default:   result += s[i]; break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}
%}

%option noyywrap yylineno nounput noinput

/* ── Helper patterns ─────────────────────────────────────────────────────────── */
DIGIT      [0-9]
ALPHA      [a-zA-Z_]
ALNUM      [a-zA-Z_0-9]
INTEGER    {DIGIT}+
REAL       {DIGIT}+\.{DIGIT}+
IDENT      {ALPHA}{ALNUM}*
STR_DQ     \"([^\"\\\n]|\\.)*\"
STR_SQ     \'([^\'\\\n]|\\.)*\'
WS         [ \t\r]+
COMMENT    \/\/[^\n]*

%%

{WS}        { /* skip whitespace */ }
{COMMENT}   { /* skip line comment */ }
\n          { /* newline acts as optional statement separator */ }

 /* ── Keywords ───────────────────────────────────────────────────────────────── */
"var"       { return yy::parser::make_TOK_VAR();    }
"if"        { return yy::parser::make_TOK_IF();     }
"then"      { return yy::parser::make_TOK_THEN();   }
"else"      { return yy::parser::make_TOK_ELSE();   }
"end"       { return yy::parser::make_TOK_END();    }
"while"     { return yy::parser::make_TOK_WHILE();  }
"for"       { return yy::parser::make_TOK_FOR();    }
"in"        { return yy::parser::make_TOK_IN();     }
"loop"      { return yy::parser::make_TOK_LOOP();   }
"exit"      { return yy::parser::make_TOK_EXIT();   }
"return"    { return yy::parser::make_TOK_RETURN(); }
"print"     { return yy::parser::make_TOK_PRINT();  }
"true"      { return yy::parser::make_TOK_TRUE(1LL);  }
"false"     { return yy::parser::make_TOK_FALSE(0LL); }
"none"      { return yy::parser::make_TOK_NONE();   }
"is"        { return yy::parser::make_TOK_IS();     }
"func"      { return yy::parser::make_TOK_FUNC();   }
"not"       { return yy::parser::make_TOK_NOT();    }
"and"       { return yy::parser::make_TOK_AND();    }
"or"        { return yy::parser::make_TOK_OR();     }
"xor"       { return yy::parser::make_TOK_XOR();    }

 /* ── Type indicator keywords ─────────────────────────────────────────────────── */
"int"       { return yy::parser::make_TOK_TYPE_INT();    }
"real"      { return yy::parser::make_TOK_TYPE_REAL();   }
"bool"      { return yy::parser::make_TOK_TYPE_BOOL();   }
"string"    { return yy::parser::make_TOK_TYPE_STRING(); }

 /* ── Multi-char operators ────────────────────────────────────────────────────── */
":="        { return yy::parser::make_TOK_ASSIGN();  }
"<="        { return yy::parser::make_TOK_LE();      }
">="        { return yy::parser::make_TOK_GE();      }
"/="        { return yy::parser::make_TOK_NEQ();     }
"=>"        { return yy::parser::make_TOK_ARROW();   }
".."        { return yy::parser::make_TOK_DOTDOT();  }

 /* ── Single-char tokens ──────────────────────────────────────────────────────── */
"<"         { return yy::parser::make_TOK_LT();       }
">"         { return yy::parser::make_TOK_GT();       }
"="         { return yy::parser::make_TOK_EQ();       }
"+"         { return yy::parser::make_TOK_PLUS();     }
"-"         { return yy::parser::make_TOK_MINUS();    }
"*"         { return yy::parser::make_TOK_STAR();     }
"/"         { return yy::parser::make_TOK_SLASH();    }
"("         { return yy::parser::make_TOK_LPAREN();   }
")"         { return yy::parser::make_TOK_RPAREN();   }
"["         { return yy::parser::make_TOK_LBRACKET(); }
"]"         { return yy::parser::make_TOK_RBRACKET(); }
"{"         { return yy::parser::make_TOK_LBRACE();   }
"}"         { return yy::parser::make_TOK_RBRACE();   }
"."         { return yy::parser::make_TOK_DOT();      }
","         { return yy::parser::make_TOK_COMMA();    }
";"         { return yy::parser::make_TOK_SEMI();     }

 /* ── Literals ────────────────────────────────────────────────────────────────── */
{REAL}      { return yy::parser::make_TOK_REAL(std::stod(yytext)); }
{INTEGER}   { return yy::parser::make_TOK_INTEGER(std::stoll(yytext)); }
{STR_DQ}    { return yy::parser::make_TOK_STRING(strip_quotes(yytext)); }
{STR_SQ}    { return yy::parser::make_TOK_STRING(strip_quotes(yytext)); }
{IDENT}     { return yy::parser::make_TOK_IDENT(std::string(yytext)); }

<<EOF>>     { return yy::parser::make_YYEOF(); }

.           {
    std::println(stderr, "Unexpected character '{}' at line {}",
                 yytext[0], yylineno);
}

%%
