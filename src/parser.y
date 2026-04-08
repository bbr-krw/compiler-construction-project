%skeleton "lalr1.cc"
%require  "3.2"
%define   api.token.constructor
%define   api.value.type variant
%define   parse.assert
%locations


%parse-param { std::unique_ptr<ASTNode>& parse_result }
%parse-param { Lexer& lexer }
%lex-param { Lexer& lexer }


%code requires {
    #include <memory>
    #include <string>
    #include "ast.hpp"
    class Lexer;
}


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


%type <std::unique_ptr<ASTNode>> program
%type <std::unique_ptr<BodyNode>> stmt_list body
%type <std::unique_ptr<ASTNode>> stmt
%type <std::unique_ptr<ASTNode>> decl var_def
%type <std::vector<std::unique_ptr<ASTNode>>> var_def_list
%type <std::unique_ptr<ASTNode>> assign
%type <std::unique_ptr<ASTNode>> if_stmt if_short_stmt
%type <std::unique_ptr<ASTNode>> loop_stmt while_stmt for_stmt
%type <std::unique_ptr<ASTNode>> exit_stmt return_stmt print_stmt
%type <std::unique_ptr<ASTNode>> expr and_expr relation factor term unary primary postfix
%type <std::unique_ptr<ASTNode>> func_literal
%type <std::unique_ptr<ParamListNode>> param_list
%type <std::vector<std::unique_ptr<ASTNode>>> expr_list opt_expr_list
%type <std::unique_ptr<ASTNode>> literal array_literal tuple_literal
%type <std::vector<std::unique_ptr<ASTNode>>> tuple_elem_list
%type <std::unique_ptr<ASTNode>> tuple_elem
%type <std::unique_ptr<ASTNode>> type_indicator

%%

program
    : stmt_list
        {
            auto n = std::make_unique<ProgramNode>(Location{1});
            n->stmts = std::move($1->stmts);
            parse_result = std::move(n);
            $$ = {};
        }
    ;

stmt_list
    : %empty
        { $$ = std::make_unique<BodyNode>(Location{@$.begin.line, @$.begin.column}); }
    | stmt_list stmt
        { if ($2) $1->stmts.push_back(std::move($2)); $$ = std::move($1); }
    | stmt_list TOK_SEMI stmt
        { if ($3) $1->stmts.push_back(std::move($3)); $$ = std::move($1); }
    | stmt_list TOK_SEMI
        { $$ = std::move($1); }
    ;

body : stmt_list { $$ = std::move($1); } ;

stmt
    : decl          { $$ = std::move($1); }
    | assign        { $$ = std::move($1); }
    | postfix       { $$ = std::move($1); }
    | if_stmt       { $$ = std::move($1); }
    | if_short_stmt { $$ = std::move($1); }
    | loop_stmt     { $$ = std::move($1); }
    | while_stmt    { $$ = std::move($1); }
    | for_stmt      { $$ = std::move($1); }
    | exit_stmt     { $$ = std::move($1); }
    | return_stmt   { $$ = std::move($1); }
    | print_stmt    { $$ = std::move($1); }
    ;

decl
    : TOK_VAR var_def_list
        {
            auto n = std::make_unique<VarDeclNode>(Location{@1.begin.line, @1.begin.column});
            n->defs = std::move($2);
            $$ = std::move(n);
        }
    ;

var_def_list
    : var_def
        {
            std::vector<std::unique_ptr<ASTNode>> lst;
            lst.push_back(std::move($1));
            $$ = std::move(lst);
        }
    | var_def_list TOK_COMMA var_def
        { $1.push_back(std::move($3)); $$ = std::move($1); }
    ;

var_def
    : TOK_IDENT
        {
            auto n = std::make_unique<VarDefNode>(Location{@1.begin.line, @1.begin.column});
            n->varname = std::move($1);
            $$ = std::move(n);
        }
    | TOK_IDENT TOK_ASSIGN expr
        {
            auto n = std::make_unique<VarDefNode>(Location{@1.begin.line, @1.begin.column});
            n->varname = std::move($1);
            n->init = std::move($3);
            $$ = std::move(n);
        }
    ;

assign
    : postfix TOK_ASSIGN expr
        {
            auto n = std::make_unique<AssignNode>(Location{@1.begin.line, @1.begin.column});
            n->lhs = std::move($1);
            n->rhs = std::move($3);
            $$ = std::move(n);
        }
    ;

if_stmt
    : TOK_IF expr TOK_THEN body TOK_END
        {
            auto n = std::make_unique<IfNode>(Location{@1.begin.line, @1.begin.column});
            n->cond = std::move($2); n->then_body = std::move($4);
            $$ = std::move(n);
        }
    | TOK_IF expr TOK_THEN body TOK_ELSE body TOK_END
        {
            auto n = std::make_unique<IfNode>(Location{@1.begin.line, @1.begin.column});
            n->cond = std::move($2); n->then_body = std::move($4); n->else_body = std::move($6);
            $$ = std::move(n);
        }
    ;


if_short_stmt
    : TOK_IF expr TOK_ARROW stmt
        {
            auto n = std::make_unique<IfShortNode>(Location{@1.begin.line, @1.begin.column});
            n->cond = std::move($2); n->stmt = std::move($4);
            $$ = std::move(n);
        }
    ;


loop_stmt
    : TOK_LOOP body TOK_END
        {
            auto n = std::make_unique<LoopInfNode>(Location{@1.begin.line, @1.begin.column});
            n->body = std::move($2);
            $$ = std::move(n);
        }
    ;


while_stmt
    : TOK_WHILE expr TOK_LOOP body TOK_END
        {
            auto n = std::make_unique<WhileNode>(Location{@1.begin.line, @1.begin.column});
            n->cond = std::move($2); n->body = std::move($4);
            $$ = std::move(n);
        }
    ;


for_stmt
    
    : TOK_FOR expr TOK_DOTDOT expr TOK_LOOP body TOK_END
        {
            auto n = std::make_unique<ForRangeNode>(Location{@1.begin.line, @1.begin.column});
            n->from = std::move($2); n->to = std::move($4); n->body = std::move($6);
            $$ = std::move(n);
        }
    
    | TOK_FOR TOK_IDENT TOK_IN expr TOK_DOTDOT expr TOK_LOOP body TOK_END
        {
            auto n = std::make_unique<ForRangeNode>(Location{@1.begin.line, @1.begin.column});
            n->iter = std::move($2);
            n->from = std::move($4); n->to = std::move($6); n->body = std::move($8);
            $$ = std::move(n);
        }
    
    | TOK_FOR expr TOK_LOOP body TOK_END
        {
            auto n = std::make_unique<ForIterNode>(Location{@1.begin.line, @1.begin.column});
            n->iterable = std::move($2); n->body = std::move($4);
            $$ = std::move(n);
        }
    
    | TOK_FOR TOK_IDENT TOK_IN expr TOK_LOOP body TOK_END
        {
            auto n = std::make_unique<ForIterNode>(Location{@1.begin.line, @1.begin.column});
            n->iter = std::move($2);
            n->iterable = std::move($4); n->body = std::move($6);
            $$ = std::move(n);
        }
    ;


exit_stmt
    : TOK_EXIT  { $$ = std::make_unique<ExitNode>(Location{@1.begin.line, @1.begin.column}); }
    ;

return_stmt
    : TOK_RETURN
        { $$ = std::make_unique<ReturnNode>(Location{@1.begin.line, @1.begin.column}); }
    | TOK_RETURN expr
        {
            auto n = std::make_unique<ReturnNode>(Location{@1.begin.line, @1.begin.column});
            n->value = std::move($2);
            $$ = std::move(n);
        }
    ;

print_stmt
    : TOK_PRINT expr_list
        {
            auto n = std::make_unique<PrintNode>(Location{@1.begin.line, @1.begin.column});
            n->exprs = std::move($2);
            $$ = std::move(n);
        }
    ;

// or/xor bind less tightly than and
expr
    : and_expr                      { $$ = std::move($1); }
    | expr TOK_OR  and_expr         { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::OR,  $1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    | expr TOK_XOR and_expr         { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::XOR, $1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    ;

// and binds more tightly than or/xor
and_expr
    : relation                      { $$ = std::move($1); }
    | and_expr TOK_AND relation     { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::AND, $1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    ;

relation
    : factor                        { $$ = std::move($1); }
    | factor TOK_LT  factor   { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::LT, $1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    | factor TOK_LE  factor   { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::LE, $1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    | factor TOK_GT  factor   { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::GT, $1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    | factor TOK_GE  factor   { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::GE, $1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    | factor TOK_EQ  factor   { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::EQ, $1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    | factor TOK_NEQ factor   { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::NEQ,$1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    ;

factor
    : term                          { $$ = std::move($1); }
    | factor TOK_PLUS  term   { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::ADD,$1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    | factor TOK_MINUS term   { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::SUB,$1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    ;

term
    : unary                         { $$ = std::move($1); }
    | term TOK_STAR  unary    { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::MUL,$1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    | term TOK_SLASH unary    { auto n=std::make_unique<BinOpNode>(BinOpNode::Op::DIV,$1->loc); n->left=std::move($1); n->right=std::move($3); $$=std::move(n); }
    ;


unary
    : postfix                               { $$ = std::move($1); }
    | postfix TOK_IS type_indicator
        { auto n=std::make_unique<IsNode>(Location{@1.begin.line, @1.begin.column}); n->operand=std::move($1); n->type_node=std::move($3); $$=std::move(n); }
    | primary                               { $$ = std::move($1); }
    | primary TOK_IS type_indicator
        { auto n=std::make_unique<IsNode>(Location{@1.begin.line, @1.begin.column}); n->operand=std::move($1); n->type_node=std::move($3); $$=std::move(n); }
    | TOK_PLUS  postfix                     { auto n=std::make_unique<UnaryOpNode>(UnaryOpNode::Op::UPLUS,  Location{@1.begin.line, @1.begin.column}); n->operand=std::move($2); $$=std::move(n); }
    | TOK_MINUS postfix                     { auto n=std::make_unique<UnaryOpNode>(UnaryOpNode::Op::UMINUS, Location{@1.begin.line, @1.begin.column}); n->operand=std::move($2); $$=std::move(n); }
    | TOK_NOT   postfix                     { auto n=std::make_unique<UnaryOpNode>(UnaryOpNode::Op::NOT,    Location{@1.begin.line, @1.begin.column}); n->operand=std::move($2); $$=std::move(n); }
    | TOK_PLUS  primary                     { auto n=std::make_unique<UnaryOpNode>(UnaryOpNode::Op::UPLUS,  Location{@1.begin.line, @1.begin.column}); n->operand=std::move($2); $$=std::move(n); }
    | TOK_MINUS primary                     { auto n=std::make_unique<UnaryOpNode>(UnaryOpNode::Op::UMINUS, Location{@1.begin.line, @1.begin.column}); n->operand=std::move($2); $$=std::move(n); }
    | TOK_NOT   primary                     { auto n=std::make_unique<UnaryOpNode>(UnaryOpNode::Op::NOT,    Location{@1.begin.line, @1.begin.column}); n->operand=std::move($2); $$=std::move(n); }
    ;

primary
    : literal                                   { $$ = std::move($1); }
    | func_literal                              { $$ = std::move($1); }
    | TOK_LPAREN expr TOK_RPAREN                { $$ = std::move($2); }
    ;

postfix
    : TOK_IDENT
        { $$ = ASTNode::make_ident(std::move($1), Location{@1.begin.line, @1.begin.column}); }

    
    | postfix TOK_LBRACKET expr TOK_RBRACKET
        {
            auto n = std::make_unique<IndexNode>(Location{@1.begin.line, @1.begin.column});
            n->base = std::move($1); n->index_expr = std::move($3);
            $$ = std::move(n);
        }

    
    | postfix TOK_LPAREN opt_expr_list TOK_RPAREN
        {
            auto n = std::make_unique<CallNode>(Location{@1.begin.line, @1.begin.column});
            n->callee = std::move($1);
            n->args   = std::move($3);
            $$ = std::move(n);
        }

    
    | postfix TOK_DOT TOK_IDENT
        {
            auto n = std::make_unique<DotFieldNode>(Location{@1.begin.line, @1.begin.column});
            n->field = std::move($3);
            n->base  = std::move($1);
            $$ = std::move(n);
        }

    
    | postfix TOK_DOT TOK_INTEGER
        {
            auto n = std::make_unique<DotIntNode>($3, Location{@1.begin.line, @1.begin.column});
            n->base = std::move($1);
            $$ = std::move(n);
        }
    ;

func_literal
    
    : TOK_FUNC TOK_IS body TOK_END
        {
            auto n  = std::make_unique<FuncLitNode>(Location{@1.begin.line, @1.begin.column});
            auto pl = std::make_unique<ParamListNode>(Location{@1.begin.line, @1.begin.column});
            n->params = std::move(pl); n->body = std::move($3);
            $$ = std::move(n);
        }

    
    | TOK_FUNC TOK_ARROW expr
        {
            auto n   = std::make_unique<FuncLitNode>(Location{@1.begin.line, @1.begin.column});
            auto pl  = std::make_unique<ParamListNode>(Location{@1.begin.line, @1.begin.column});
            auto ret = std::make_unique<ReturnNode>(Location{@1.begin.line, @1.begin.column});
            ret->value = std::move($3);
            auto b   = std::make_unique<BodyNode>(Location{@1.begin.line, @1.begin.column});
            b->stmts.push_back(std::move(ret));
            n->params = std::move(pl); n->body = std::move(b);
            $$ = std::move(n);
        }

    
    | TOK_FUNC TOK_LPAREN param_list TOK_RPAREN TOK_IS body TOK_END
        {
            auto n = std::make_unique<FuncLitNode>(Location{@1.begin.line, @1.begin.column});
            n->params = std::move($3); n->body = std::move($6);
            $$ = std::move(n);
        }

    
    | TOK_FUNC TOK_LPAREN param_list TOK_RPAREN TOK_ARROW expr
        {
            auto n   = std::make_unique<FuncLitNode>(Location{@1.begin.line, @1.begin.column});
            auto ret = std::make_unique<ReturnNode>(Location{@1.begin.line, @1.begin.column});
            ret->value = std::move($6);
            auto b   = std::make_unique<BodyNode>(Location{@1.begin.line, @1.begin.column});
            b->stmts.push_back(std::move(ret));
            n->params = std::move($3); n->body = std::move(b);
            $$ = std::move(n);
        }
    ;

param_list
    : TOK_IDENT
        {
            auto pl = std::make_unique<ParamListNode>(Location{@1.begin.line, @1.begin.column});
            pl->params.push_back(ASTNode::make_ident(std::move($1), Location{@1.begin.line, @1.begin.column}));
            $$ = std::move(pl);
        }
    | param_list TOK_COMMA TOK_IDENT
        {
            $1->params.push_back(ASTNode::make_ident(std::move($3), Location{@3.begin.line, @3.begin.column}));
            $$ = std::move($1);
        }
    ;

literal
    : TOK_INTEGER       { $$ = ASTNode::make_int ($1,          Location{@1.begin.line, @1.begin.column}); }
    | TOK_REAL          { $$ = ASTNode::make_real($1,          Location{@1.begin.line, @1.begin.column}); }
    | TOK_STRING        { $$ = ASTNode::make_str (std::move($1), Location{@1.begin.line, @1.begin.column}); }
    | TOK_TRUE          { $$ = ASTNode::make_bool(true,        Location{@1.begin.line, @1.begin.column}); }
    | TOK_FALSE         { $$ = ASTNode::make_bool(false,       Location{@1.begin.line, @1.begin.column}); }
    | TOK_NONE          { $$ = ASTNode::make_none(             Location{@1.begin.line, @1.begin.column}); }
    | array_literal     { $$ = std::move($1); }
    | tuple_literal     { $$ = std::move($1); }
    ;


array_literal
    : TOK_LBRACKET TOK_RBRACKET
        { $$ = std::make_unique<ArrayLitNode>(Location{@1.begin.line, @1.begin.column}); }

tuple_literal
    : TOK_LBRACE TOK_RBRACE
        { $$ = std::make_unique<TupleLitNode>(Location{@1.begin.line, @1.begin.column}); }
    | TOK_LBRACKET expr_list TOK_RBRACKET
        {
            auto n = std::make_unique<ArrayLitNode>(Location{@1.begin.line, @1.begin.column});
            n->elems = std::move($2);
            $$ = std::move(n);
        }
    ;


tuple_literal
    : TOK_LBRACE tuple_elem_list TOK_RBRACE
        {
            auto n = std::make_unique<TupleLitNode>(Location{@1.begin.line, @1.begin.column});
            n->elems = std::move($2);
            $$ = std::move(n);
        }
    ;

tuple_elem_list
    : tuple_elem
        {
            std::vector<std::unique_ptr<ASTNode>> lst;
            lst.push_back(std::move($1));
            $$ = std::move(lst);
        }
    | tuple_elem_list TOK_COMMA tuple_elem
        { $1.push_back(std::move($3)); $$ = std::move($1); }
    ;

tuple_elem
    
    : TOK_IDENT TOK_ASSIGN expr
        {
            auto n = std::make_unique<TupleElemNode>(Location{@1.begin.line, @1.begin.column});
            n->elem_name = std::move($1);
            n->expr      = std::move($3);
            $$ = std::move(n);
        }
    
    | expr
        {
            auto n = std::make_unique<TupleElemNode>(Location{@1.begin.line, @1.begin.column});
            n->expr = std::move($1);
            $$ = std::move(n);
        }
    ;

type_indicator
    : TOK_TYPE_INT              { $$ = std::make_unique<TypeNode>(TypeNode::Type::INT,    Location{@1.begin.line, @1.begin.column}); }
    | TOK_TYPE_REAL             { $$ = std::make_unique<TypeNode>(TypeNode::Type::REAL,   Location{@1.begin.line, @1.begin.column}); }
    | TOK_TYPE_BOOL             { $$ = std::make_unique<TypeNode>(TypeNode::Type::BOOL,   Location{@1.begin.line, @1.begin.column}); }
    | TOK_TYPE_STRING           { $$ = std::make_unique<TypeNode>(TypeNode::Type::STRING, Location{@1.begin.line, @1.begin.column}); }
    | TOK_NONE                  { $$ = std::make_unique<TypeNode>(TypeNode::Type::NONE,   Location{@1.begin.line, @1.begin.column}); }
    | TOK_LBRACKET TOK_RBRACKET { $$ = std::make_unique<TypeNode>(TypeNode::Type::ARRAY,  Location{@1.begin.line, @1.begin.column}); }
    | TOK_LBRACE  TOK_RBRACE    { $$ = std::make_unique<TypeNode>(TypeNode::Type::TUPLE,  Location{@1.begin.line, @1.begin.column}); }
    | TOK_FUNC                  { $$ = std::make_unique<TypeNode>(TypeNode::Type::FUNC,   Location{@1.begin.line, @1.begin.column}); }
    ;

expr_list
    : expr
        {
            std::vector<std::unique_ptr<ASTNode>> lst;
            lst.push_back(std::move($1));
            $$ = std::move(lst);
        }
    | expr_list TOK_COMMA expr
        { $1.push_back(std::move($3)); $$ = std::move($1); }
    ;

opt_expr_list
    : %empty      { $$ = std::vector<std::unique_ptr<ASTNode>>{}; }
    | expr_list   { $$ = std::move($1); }
    ;

%%
