%{
/*
 * parser.y – Bison grammar for the D language (C++23)
 *
 * Uses the modern lalr1.cc C++ skeleton with:
 *   - api.token.constructor  → typed token constructors in the lexer
 *   - api.value.type variant → std::variant for all semantic values
 *   - %parse-param           → result passed by reference
 */
%}

%skeleton "lalr1.cc"
%require  "3.2"
%define   api.token.constructor
%define   api.value.type variant
%define   parse.assert
%locations

/* The parsed AST root is returned through this reference. */
%parse-param { ASTNode*& parse_result }
%parse-param { Lexer& lexer }
%lex-param { Lexer& lexer }

/* ── Code injected into the generated header (seen by lexer.l) ──────────────── */
%code requires {
    #include <string>
    struct ASTNode;   /* forward-declare so the header compiles stand-alone */
    class Lexer;
}

/* ── Code injected into parser.tab.cpp ─────────────────────────────────────── */
%code {
    #include "ast.hpp"
    #include "lexer.hpp"
    #include <print>

    void yy::parser::error(const location_type& loc, const std::string& msg) {
        std::println(stderr, "Parse error at line {}:{}: {}", loc.begin.line, loc.begin.column, msg);
    }

    static yy::parser::symbol_type yylex(Lexer& lexer) {
        return lexer.next();
    }
}

/* ── Token declarations ─────────────────────────────────────────────────────── */
%token TOK_VAR TOK_IF TOK_THEN TOK_ELSE TOK_END
%token TOK_WHILE TOK_FOR TOK_IN TOK_LOOP
%token TOK_EXIT TOK_RETURN TOK_PRINT
%token TOK_FUNC TOK_IS TOK_NOT TOK_AND TOK_OR TOK_XOR
%token TOK_NONE TOK_ARROW TOK_DOTDOT TOK_ASSIGN
%token TOK_LT TOK_LE TOK_GT TOK_GE TOK_EQ TOK_NEQ
%token TOK_PLUS TOK_MINUS TOK_STAR TOK_SLASH
%token TOK_LPAREN TOK_RPAREN
%token TOK_LBRACKET TOK_RBRACKET
%token TOK_LBRACE  TOK_RBRACE
%token TOK_DOT TOK_COMMA TOK_SEMI
%token TOK_TYPE_INT TOK_TYPE_REAL TOK_TYPE_BOOL TOK_TYPE_STRING

%token <long long>   TOK_INTEGER
%token <long long>   TOK_TRUE TOK_FALSE
%token <double>      TOK_REAL
%token <std::string> TOK_STRING TOK_IDENT

/* ── Non-terminal types ─────────────────────────────────────────────────────── */
%type <ASTNode*> program
%type <ASTNode*> stmt_list body stmt
%type <ASTNode*> decl var_def_list var_def
%type <ASTNode*> assign
%type <ASTNode*> if_stmt if_short_stmt
%type <ASTNode*> loop_stmt while_stmt for_stmt
%type <ASTNode*> exit_stmt return_stmt print_stmt
%type <ASTNode*> expr relation factor term unary primary postfix
%type <ASTNode*> func_literal param_list
%type <ASTNode*> expr_list opt_expr_list
%type <ASTNode*> literal array_literal tuple_literal
%type <ASTNode*> tuple_elem_list tuple_elem
%type <ASTNode*> type_indicator

%%

/* ═══════════════════════════════════════════════════════════════════════════════
   Program
   ═══════════════════════════════════════════════════════════════════════════════ */

program
    : stmt_list
        {
            auto* n = ASTNode::make(NodeKind::PROGRAM, 1);
            for (auto* c : $1->children) n->add_child(c);
            $1->children.clear();
            delete $1;
            parse_result = n;
            $$ = n;
        }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Statement list / body
   ═══════════════════════════════════════════════════════════════════════════════ */

stmt_list
    : %empty
        { $$ = ASTNode::make(NodeKind::BODY, @$.begin.line, @$.begin.column); }
    | stmt_list stmt
        { if ($2) $1->add_child($2); $$ = $1; }
    | stmt_list TOK_SEMI stmt
        { if ($3) $1->add_child($3); $$ = $1; }
    | stmt_list TOK_SEMI
        { $$ = $1; }
    ;

body : stmt_list { $$ = $1; } ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Single statement
   ═══════════════════════════════════════════════════════════════════════════════ */

stmt
    : decl          { $$ = $1; }
    | assign        { $$ = $1; }
    | postfix       { $$ = $1; }
    | if_stmt       { $$ = $1; }
    | if_short_stmt { $$ = $1; }
    | loop_stmt     { $$ = $1; }
    | while_stmt    { $$ = $1; }
    | for_stmt      { $$ = $1; }
    | exit_stmt     { $$ = $1; }
    | return_stmt   { $$ = $1; }
    | print_stmt    { $$ = $1; }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Declaration:  var x, y := 2, z
   ═══════════════════════════════════════════════════════════════════════════════ */

decl
    : TOK_VAR var_def_list
        {
            auto* n = ASTNode::make(NodeKind::VAR_DECL, @1.begin.line, @1.begin.column);
            for (auto* c : $2->children) n->add_child(c);
            $2->children.clear();
            delete $2;
            $$ = n;
        }
    ;

var_def_list
    : var_def
        {
            auto* lst = ASTNode::make(NodeKind::BODY, @1.begin.line, @1.begin.column);
            lst->add_child($1);
            $$ = lst;
        }
    | var_def_list TOK_COMMA var_def
        { $1->add_child($3); $$ = $1; }
    ;

var_def
    : TOK_IDENT
        {
            auto* n = ASTNode::make(NodeKind::VAR_DEF, @1.begin.line, @1.begin.column);
            n->name = std::move($1);
            $$ = n;
        }
    | TOK_IDENT TOK_ASSIGN expr
        {
            auto* n = ASTNode::make(NodeKind::VAR_DEF, @1.begin.line, @1.begin.column);
            n->name = std::move($1);
            n->add_child($3);
            $$ = n;
        }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Assignment:  postfix := expr
   ═══════════════════════════════════════════════════════════════════════════════ */

assign
    : postfix TOK_ASSIGN expr
        {
            auto* n = ASTNode::make(NodeKind::ASSIGN, @1.begin.line, @1.begin.column);
            n->add_child($1);
            n->add_child($3);
            $$ = n;
        }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Conditional statements
   ═══════════════════════════════════════════════════════════════════════════════ */

if_stmt
    : TOK_IF expr TOK_THEN body TOK_END
        {
            auto* n = ASTNode::make(NodeKind::IF, @1.begin.line, @1.begin.column);
            n->add_child($2); n->add_child($4);
            $$ = n;
        }
    | TOK_IF expr TOK_THEN body TOK_ELSE body TOK_END
        {
            auto* n = ASTNode::make(NodeKind::IF, @1.begin.line, @1.begin.column);
            n->add_child($2); n->add_child($4); n->add_child($6);
            $$ = n;
        }
    ;

/* IfShort: single-statement consequent — no 'end' terminator */
if_short_stmt
    : TOK_IF expr TOK_ARROW stmt
        {
            auto* n = ASTNode::make(NodeKind::IF_SHORT, @1.begin.line, @1.begin.column);
            n->add_child($2); n->add_child($4);
            $$ = n;
        }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Loops
   ═══════════════════════════════════════════════════════════════════════════════ */

/* Infinite loop:  loop body end */
loop_stmt
    : TOK_LOOP body TOK_END
        {
            auto* n = ASTNode::make(NodeKind::LOOP_INF, @1.begin.line, @1.begin.column);
            n->add_child($2);
            $$ = n;
        }
    ;

/* While loop:  while expr loop body end */
while_stmt
    : TOK_WHILE expr TOK_LOOP body TOK_END
        {
            auto* n = ASTNode::make(NodeKind::WHILE, @1.begin.line, @1.begin.column);
            n->add_child($2); n->add_child($4);
            $$ = n;
        }
    ;

/* For loop variants */
for_stmt
    /* for expr .. expr loop body end  (range, no iterator variable) */
    : TOK_FOR expr TOK_DOTDOT expr TOK_LOOP body TOK_END
        {
            auto* n = ASTNode::make(NodeKind::FOR_RANGE, @1.begin.line, @1.begin.column);
            n->add_child($2); n->add_child($4); n->add_child($6);
            $$ = n;
        }
    /* for id in expr .. expr loop body end  (range, named iterator) */
    | TOK_FOR TOK_IDENT TOK_IN expr TOK_DOTDOT expr TOK_LOOP body TOK_END
        {
            auto* n = ASTNode::make(NodeKind::FOR_RANGE, @1.begin.line, @1.begin.column);
            n->name = std::move($2);
            n->add_child($4); n->add_child($6); n->add_child($8);
            $$ = n;
        }
    /* for expr loop body end  (iterate over array/tuple, no iterator var) */
    | TOK_FOR expr TOK_LOOP body TOK_END
        {
            auto* n = ASTNode::make(NodeKind::FOR_ITER, @1.begin.line, @1.begin.column);
            n->add_child($2); n->add_child($4);
            $$ = n;
        }
    /* for id in expr loop body end  (iterate over array/tuple, named iterator) */
    | TOK_FOR TOK_IDENT TOK_IN expr TOK_LOOP body TOK_END
        {
            auto* n = ASTNode::make(NodeKind::FOR_ITER, @1.begin.line, @1.begin.column);
            n->name = std::move($2);
            n->add_child($4); n->add_child($6);
            $$ = n;
        }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Exit / Return / Print
   ═══════════════════════════════════════════════════════════════════════════════ */

exit_stmt
    : TOK_EXIT  { $$ = ASTNode::make(NodeKind::EXIT,   @1.begin.line, @1.begin.column); }
    ;

return_stmt
    : TOK_RETURN
        { $$ = ASTNode::make(NodeKind::RETURN, @1.begin.line, @1.begin.column); }
    | TOK_RETURN expr
        {
            auto* n = ASTNode::make(NodeKind::RETURN, @1.begin.line, @1.begin.column);
            n->add_child($2);
            $$ = n;
        }
    ;

print_stmt
    : TOK_PRINT expr_list
        {
            auto* n = ASTNode::make(NodeKind::PRINT, @1.begin.line, @1.begin.column);
            for (auto* c : $2->children) n->add_child(c);
            $2->children.clear();
            delete $2;
            $$ = n;
        }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Expression hierarchy
   ═══════════════════════════════════════════════════════════════════════════════ */

expr
    : relation                      { $$ = $1; }
    | expr TOK_OR  relation         { auto* n=ASTNode::make(NodeKind::OR,  $1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    | expr TOK_AND relation         { auto* n=ASTNode::make(NodeKind::AND, $1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    | expr TOK_XOR relation         { auto* n=ASTNode::make(NodeKind::XOR, $1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    ;

relation
    : factor                        { $$ = $1; }
    | factor TOK_LT  factor   { auto* n=ASTNode::make(NodeKind::LT, $1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    | factor TOK_LE  factor   { auto* n=ASTNode::make(NodeKind::LE, $1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    | factor TOK_GT  factor   { auto* n=ASTNode::make(NodeKind::GT, $1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    | factor TOK_GE  factor   { auto* n=ASTNode::make(NodeKind::GE, $1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    | factor TOK_EQ  factor   { auto* n=ASTNode::make(NodeKind::EQ, $1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    | factor TOK_NEQ factor   { auto* n=ASTNode::make(NodeKind::NEQ,$1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    ;

factor
    : term                          { $$ = $1; }
    | factor TOK_PLUS  term   { auto* n=ASTNode::make(NodeKind::ADD,$1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    | factor TOK_MINUS term   { auto* n=ASTNode::make(NodeKind::SUB,$1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    ;

term
    : unary                         { $$ = $1; }
    | term TOK_STAR  unary    { auto* n=ASTNode::make(NodeKind::MUL,$1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    | term TOK_SLASH unary    { auto* n=ASTNode::make(NodeKind::DIV,$1->line, $1->col); n->add_child($1); n->add_child($3); $$=n; }
    ;

/*
 * Unary rule — the critical fix vs the C version:
 *   • postfix (was "reference") covers IDENT + all postfix ops
 *   • primary is added as a direct alternative so literals / (expr) work
 *     without a leading +/- prefix
 *   • reference is NOT allowed to start with '(' (removed that production)
 */
unary
    : postfix                               { $$ = $1; }
    | postfix TOK_IS type_indicator
        { auto* n=ASTNode::make(NodeKind::IS,    @1.begin.line, @1.begin.column); n->add_child($1); n->add_child($3); $$=n; }
    | primary                               { $$ = $1; }
    | primary TOK_IS type_indicator
        { auto* n=ASTNode::make(NodeKind::IS,    @1.begin.line, @1.begin.column); n->add_child($1); n->add_child($3); $$=n; }
    | TOK_PLUS  postfix                     { auto* n=ASTNode::make(NodeKind::UPLUS, @1.begin.line, @1.begin.column); n->add_child($2); $$=n; }
    | TOK_MINUS postfix                     { auto* n=ASTNode::make(NodeKind::UMINUS,@1.begin.line, @1.begin.column); n->add_child($2); $$=n; }
    | TOK_NOT   postfix                     { auto* n=ASTNode::make(NodeKind::NOT,   @1.begin.line, @1.begin.column); n->add_child($2); $$=n; }
    | TOK_PLUS  primary                     { auto* n=ASTNode::make(NodeKind::UPLUS, @1.begin.line, @1.begin.column); n->add_child($2); $$=n; }
    | TOK_MINUS primary                     { auto* n=ASTNode::make(NodeKind::UMINUS,@1.begin.line, @1.begin.column); n->add_child($2); $$=n; }
    | TOK_NOT   primary                     { auto* n=ASTNode::make(NodeKind::NOT,   @1.begin.line, @1.begin.column); n->add_child($2); $$=n; }
    ;

/*
 * Primary — non-reference operands: literals, func literals, parenthesised expr.
 * Per spec, primary does NOT start with an identifier; that is postfix.
 */
primary
    : literal                                   { $$ = $1; }
    | func_literal                              { $$ = $1; }
    | TOK_LPAREN expr TOK_RPAREN                { $$ = $2; }
    ;

/*
 * Postfix (= Reference in spec) — always starts with an identifier.
 * Left-recursive to handle chained indexing, calls, and field access.
 */
postfix
    : TOK_IDENT
        { $$ = ASTNode::make_ident(std::move($1), @1.begin.line, @1.begin.column); }

    /* ref [ expr ] */
    | postfix TOK_LBRACKET expr TOK_RBRACKET
        {
            auto* n = ASTNode::make(NodeKind::INDEX, @1.begin.line, @1.begin.column);
            n->add_child($1); n->add_child($3);
            $$ = n;
        }

    /* ref ( opt_expr_list ) */
    | postfix TOK_LPAREN opt_expr_list TOK_RPAREN
        {
            auto* n = ASTNode::make(NodeKind::CALL, @1.begin.line, @1.begin.column);
            n->add_child($1);
            for (auto* c : $3->children) n->add_child(c);
            $3->children.clear();
            delete $3;
            $$ = n;
        }

    /* ref . IDENT */
    | postfix TOK_DOT TOK_IDENT
        {
            auto* n = ASTNode::make(NodeKind::DOT_FIELD, @1.begin.line, @1.begin.column);
            n->name = std::move($3);
            n->add_child($1);
            $$ = n;
        }

    /* ref . INTEGER */
    | postfix TOK_DOT TOK_INTEGER
        {
            auto* n = ASTNode::make(NodeKind::DOT_INT, @1.begin.line, @1.begin.column);
            n->payload = $3;
            n->add_child($1);
            $$ = n;
        }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Function literal
   ═══════════════════════════════════════════════════════════════════════════════ */

func_literal
    /* func is body end  (no params) */
    : TOK_FUNC TOK_IS body TOK_END
        {
            auto* n  = ASTNode::make(NodeKind::FUNC_LIT, @1.begin.line, @1.begin.column);
            auto* pl = ASTNode::make(NodeKind::PARAM_LIST, @1.begin.line, @1.begin.column);
            n->add_child(pl); n->add_child($3);
            $$ = n;
        }

    /* func => expr  (no params, expression body) */
    | TOK_FUNC TOK_ARROW expr
        {
            auto* n   = ASTNode::make(NodeKind::FUNC_LIT, @1.begin.line, @1.begin.column);
            auto* pl  = ASTNode::make(NodeKind::PARAM_LIST, @1.begin.line, @1.begin.column);
            auto* ret = ASTNode::make(NodeKind::RETURN, @1.begin.line, @1.begin.column);
            ret->add_child($3);
            auto* b   = ASTNode::make(NodeKind::BODY, @1.begin.line, @1.begin.column);
            b->add_child(ret);
            n->add_child(pl); n->add_child(b);
            $$ = n;
        }

    /* func ( params ) is body end */
    | TOK_FUNC TOK_LPAREN param_list TOK_RPAREN TOK_IS body TOK_END
        {
            auto* n = ASTNode::make(NodeKind::FUNC_LIT, @1.begin.line, @1.begin.column);
            n->add_child($3); n->add_child($6);
            $$ = n;
        }

    /* func ( params ) => expr */
    | TOK_FUNC TOK_LPAREN param_list TOK_RPAREN TOK_ARROW expr
        {
            auto* n   = ASTNode::make(NodeKind::FUNC_LIT, @1.begin.line, @1.begin.column);
            auto* ret = ASTNode::make(NodeKind::RETURN, @1.begin.line, @1.begin.column);
            ret->add_child($6);
            auto* b   = ASTNode::make(NodeKind::BODY, @1.begin.line, @1.begin.column);
            b->add_child(ret);
            n->add_child($3); n->add_child(b);
            $$ = n;
        }
    ;

param_list
    : TOK_IDENT
        {
            auto* pl = ASTNode::make(NodeKind::PARAM_LIST, @1.begin.line, @1.begin.column);
            pl->add_child(ASTNode::make_ident(std::move($1), @1.begin.line, @1.begin.column));
            $$ = pl;
        }
    | param_list TOK_COMMA TOK_IDENT
        {
            $1->add_child(ASTNode::make_ident(std::move($3), @3.begin.line, @3.begin.column));
            $$ = $1;
        }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Literals
   ═══════════════════════════════════════════════════════════════════════════════ */

literal
    : TOK_INTEGER       { $$ = ASTNode::make_int ($1,          @1.begin.line, @1.begin.column); }
    | TOK_REAL          { $$ = ASTNode::make_real($1,          @1.begin.line, @1.begin.column); }
    | TOK_STRING        { $$ = ASTNode::make_str (std::move($1), @1.begin.line, @1.begin.column); }
    | TOK_TRUE          { $$ = ASTNode::make_bool(true,        @1.begin.line, @1.begin.column); }
    | TOK_FALSE         { $$ = ASTNode::make_bool(false,       @1.begin.line, @1.begin.column); }
    | TOK_NONE          { $$ = ASTNode::make_none(             @1.begin.line, @1.begin.column); }
    | array_literal     { $$ = $1; }
    | tuple_literal     { $$ = $1; }
    ;

/* Array literal:  []  or  [ expr, expr, ... ] */
array_literal
    : TOK_LBRACKET TOK_RBRACKET
        { $$ = ASTNode::make(NodeKind::ARRAY_LIT, @1.begin.line, @1.begin.column); }
    | TOK_LBRACKET expr_list TOK_RBRACKET
        {
            auto* n = ASTNode::make(NodeKind::ARRAY_LIT, @1.begin.line, @1.begin.column);
            for (auto* c : $2->children) n->add_child(c);
            $2->children.clear();
            delete $2;
            $$ = n;
        }
    ;

/* Tuple literal:  { elem, ... }  (must have at least one element) */
tuple_literal
    : TOK_LBRACE tuple_elem_list TOK_RBRACE
        {
            auto* n = ASTNode::make(NodeKind::TUPLE_LIT, @1.begin.line, @1.begin.column);
            for (auto* c : $2->children) n->add_child(c);
            $2->children.clear();
            delete $2;
            $$ = n;
        }
    ;

tuple_elem_list
    : tuple_elem
        {
            auto* lst = ASTNode::make(NodeKind::BODY, @1.begin.line, @1.begin.column);
            lst->add_child($1);
            $$ = lst;
        }
    | tuple_elem_list TOK_COMMA tuple_elem
        { $1->add_child($3); $$ = $1; }
    ;

tuple_elem
    /* named:  IDENT := expr */
    : TOK_IDENT TOK_ASSIGN expr
        {
            auto* n = ASTNode::make(NodeKind::TUPLE_ELEM, @1.begin.line, @1.begin.column);
            n->name = std::move($1);
            n->add_child($3);
            $$ = n;
        }
    /* unnamed:  expr */
    | expr
        {
            auto* n = ASTNode::make(NodeKind::TUPLE_ELEM, @1.begin.line, @1.begin.column);
            n->add_child($1);
            $$ = n;
        }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Type indicators  (right-hand operand of `is`)
   ═══════════════════════════════════════════════════════════════════════════════ */

type_indicator
    : TOK_TYPE_INT              { $$ = ASTNode::make(NodeKind::TYPE_INT,    @1.begin.line, @1.begin.column); }
    | TOK_TYPE_REAL             { $$ = ASTNode::make(NodeKind::TYPE_REAL,   @1.begin.line, @1.begin.column); }
    | TOK_TYPE_BOOL             { $$ = ASTNode::make(NodeKind::TYPE_BOOL,   @1.begin.line, @1.begin.column); }
    | TOK_TYPE_STRING           { $$ = ASTNode::make(NodeKind::TYPE_STRING, @1.begin.line, @1.begin.column); }
    | TOK_NONE                  { $$ = ASTNode::make(NodeKind::TYPE_NONE,   @1.begin.line, @1.begin.column); }
    | TOK_LBRACKET TOK_RBRACKET { $$ = ASTNode::make(NodeKind::TYPE_ARRAY,  @1.begin.line, @1.begin.column); }
    | TOK_LBRACE  TOK_RBRACE    { $$ = ASTNode::make(NodeKind::TYPE_TUPLE,  @1.begin.line, @1.begin.column); }
    | TOK_FUNC                  { $$ = ASTNode::make(NodeKind::TYPE_FUNC,   @1.begin.line, @1.begin.column); }
    ;

/* ═══════════════════════════════════════════════════════════════════════════════
   Comma-separated expression lists
   ═══════════════════════════════════════════════════════════════════════════════ */

expr_list
    : expr
        {
            auto* lst = ASTNode::make(NodeKind::BODY, @1.begin.line, @1.begin.column);
            lst->add_child($1);
            $$ = lst;
        }
    | expr_list TOK_COMMA expr
        { $1->add_child($3); $$ = $1; }
    ;

opt_expr_list
    : %empty      { $$ = ASTNode::make(NodeKind::BODY, @$.begin.line, @$.begin.column); }
    | expr_list   { $$ = $1; }
    ;

%%
