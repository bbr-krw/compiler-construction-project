#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

enum class NodeKind {
    PROGRAM,
    VAR_DECL, // var VarDef, VarDef …    children = VarDef nodes
    VAR_DEF,  // name in .name, optional init in children[0]
    ASSIGN,    // children[0]=lhs(postfix), children[1]=rhs(expr)
    IF,        // children[0]=cond, [1]=then-body, [2]=else-body (optional)
    IF_SHORT,  // children[0]=cond, [1]=single stmt
    WHILE,     // children[0]=cond, [1]=body
    FOR_RANGE, // .name=iterator(may be ""), children[0]=from,[1]=to,[2]=body
    FOR_ITER,  // .name=iterator(may be ""), children[0]=expr, [1]=body
    LOOP_INF,  // children[0]=body
    EXIT,
    RETURN, // children[0]=expr (optional)
    PRINT,  // children = expr list
    BODY,   // statement list (used as body of loops/if etc.)
    OR,
    AND,
    XOR,
    LT,
    LE,
    GT,
    GE,
    EQ,
    NEQ,
    ADD,
    SUB,
    MUL,
    DIV,
    UPLUS,
    UMINUS,
    NOT,
    IS, // children[0]=operand, [1]=type-indicator node
    IDENT,     // payload = string (identifier name)
    INDEX,     // children[0]=base, [1]=index-expr
    CALL,      // children[0]=callee, rest=arguments
    DOT_FIELD, // .name=field, children[0]=base
    DOT_INT,   // payload=long long (1-based index), children[0]=base
    INT_LIT,  // payload = long long
    REAL_LIT, // payload = double
    STR_LIT,  // payload = string
    BOOL_LIT, // payload = long long  (1 = true, 0 = false)
    NONE_LIT,
    ARRAY_LIT,  // children = element exprs
    TUPLE_LIT,  // children = TUPLE_ELEM nodes
    TUPLE_ELEM, // .name = element name (empty = unnamed), children[0]=expr
    FUNC_LIT,   // children[0]=PARAM_LIST, [1]=BODY
    PARAM_LIST, // children = IDENT nodes (parameter names)
    TYPE_INT,
    TYPE_REAL,
    TYPE_BOOL,
    TYPE_STRING,
    TYPE_NONE,
    TYPE_ARRAY,
    TYPE_TUPLE,
    TYPE_FUNC,
};

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};

struct ASTNode {
    NodeKind kind;
    int line{0};
    int col{0};

    std::vector<std::unique_ptr<ASTNode>> children;

    using Payload = std::variant<std::monostate, long long, double, std::string>;
    Payload payload{};

    std::string name;

    explicit ASTNode(NodeKind k, int ln = 0, int col = 0) : kind{k}, line{ln}, col{col} {}

    ~ASTNode()                         = default;
    ASTNode(const ASTNode&)            = delete;
    ASTNode& operator=(const ASTNode&) = delete;
    ASTNode(ASTNode&&)                 = default;
    ASTNode& operator=(ASTNode&&)      = default;

    void add_child(std::unique_ptr<ASTNode> child) { children.push_back(std::move(child)); }
    void prepend_child(std::unique_ptr<ASTNode> child) {
        children.insert(children.begin(), std::move(child));
    }

    long long get_ival() const { return std::get<long long>(payload); }
    double get_rval() const { return std::get<double>(payload); }
    const std::string& get_sval() const { return std::get<std::string>(payload); }

    static std::unique_ptr<ASTNode> make(NodeKind k, int ln = 0, int col = 0);
    static std::unique_ptr<ASTNode> make_int(long long v, int ln = 0, int col = 0);
    static std::unique_ptr<ASTNode> make_real(double v, int ln = 0, int col = 0);
    static std::unique_ptr<ASTNode> make_str(std::string s, int ln = 0, int col = 0);   // STR_LIT
    static std::unique_ptr<ASTNode> make_ident(std::string s, int ln = 0, int col = 0); // IDENT
    static std::unique_ptr<ASTNode> make_bool(bool v, int ln = 0, int col = 0);
    static std::unique_ptr<ASTNode> make_none(int ln = 0, int col = 0);

    std::string_view kind_name() const noexcept;
    void print(int indent, std::ostream& os) const;
    void print(int indent = 0) const;
};
