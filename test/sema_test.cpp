#include "ast.hpp"
#include "lexer.hpp"
#include "parser.tab.hpp"
#include "semantic_analyzer.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>

static std::unique_ptr<ASTNode> parse(const std::string& src) {
    std::unique_ptr<ASTNode> root;
    std::istringstream stream(src);
    Lexer lexer(stream);
    yy::parser parser{root, lexer};
    const int rc = parser.parse();
    if (rc != 0 || !root)
        return nullptr;
    return root;
}

struct SemaResult {
    bool ok;
    std::vector<SemanticError> errors;
};

static SemaResult analyze(const std::string& src) {
    auto root = parse(src);
    EXPECT_NE(root, nullptr) << "parse failed for: " << src;
    if (!root)
        return {false, {}};
    SemanticAnalyzer sa;
    sa.analyze(*root);
    return {sa.ok(), sa.errors()};
}

static bool has_error(const SemaResult& r, const std::string& substr) {
    for (const auto& e : r.errors)
        if (e.message.find(substr) != std::string::npos)
            return true;
    return false;
}

TEST(SemaValid, SimpleVarDeclAndUse) {
    auto r = analyze("var x := 42\nprint x");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, MultipleVarDeclOnOneLine) {
    auto r = analyze("var x, y := 1\nprint x\nprint y");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, NestedScopeShadowing) {
    auto r = analyze(R"(
var x := 1
if x = 1 then
    var x := 2
    print x
end
print x
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, WhileLoopWithExit) {
    auto r = analyze(R"(
var i := 0
while i < 10 loop
    i := i + 1
    if i = 5 => exit
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, InfiniteLoopWithExit) {
    auto r = analyze(R"(
var i := 0
loop
    i := i + 1
    if i = 3 => exit
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, ForRangeNoIterator) {
    auto r = analyze(R"(
for 1..3 loop
    print "hello"
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, ForRangeWithIterator) {
    auto r = analyze(R"(
var sum := 0
for i in 1..5 loop
    sum := sum + i
end
print sum
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, ForIterOverArray) {
    auto r = analyze(R"(
var arr := [10, 20, 30]
var total := 0
for item in arr loop
    total := total + item
end
print total
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, ForIterNoIteratorVariable) {
    auto r = analyze(R"(
var arr := [1, 2, 3]
for arr loop
    print "x"
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, NestedForLoopsExitIsValid) {
    auto r = analyze(R"(
for i in 1..3 loop
    for j in 1..3 loop
        if j = 2 => exit
    end
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, FunctionWithReturn) {
    auto r = analyze(R"(
var f := func(n) is
    return n * 2
end
print f(5)
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, FunctionParamsInScope) {
    auto r = analyze(R"(
var add := func(a, b) is
    var result := a + b
    return result
end
print add(3, 4)
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, FunctionExpressionBody) {
    auto r = analyze(R"(
var f := func(x) => x + 1
print f(10)
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, FunctionNoParams) {
    auto r = analyze(R"(
var f := func => 42
print f()
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, NestedFunctionsInnerReturn) {
    auto r = analyze(R"(
var outer := func(x) is
    var inner := func(y) is
        return y + 1
    end
    return inner(x)
end
print outer(5)
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, ReturnWithoutValue) {
    auto r = analyze(R"(
var f := func is
    var x := 1
    return
end
f()
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, IsTypeCheck) {
    auto r = analyze(R"(
var x := 42
print x is int
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, IsTypeCheckAllTypes) {
    auto r = analyze(R"(
var v := 0
print v is int
print v is real
print v is bool
print v is string
print v is none
print v is []
print v is {}
print v is func
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, IfThenElse) {
    auto r = analyze(R"(
var x := 10
if x > 5 then
    var msg := "big"
    print msg
else
    var msg := "small"
    print msg
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, AssignmentToArrayElement) {
    auto r = analyze(R"(
var t := []
t[10] := 25
print t[10]
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, TupleFieldAccess) {
    auto r = analyze(R"(
var t := {a := 1, b := 2}
print t.a
print t.b
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, TupleIntAccess) {
    auto r = analyze(R"(
var t := {a := 1, b := 2}
print t.1
print t.2
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, RecursiveFunction) {
    auto r = analyze(R"(
var fact := func(n) is
    if n <= 1 then
        return 1
    else
        return n * fact(n - 1)
    end
end
print fact(5)
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, PrintMultipleExpressions) {
    auto r = analyze(R"(
var a := 1
var b := 2
var c := 3
print a, b, c
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, VariableDeclaredInLoopBodyNotLeaking) {
    auto r = analyze(R"(
for i in 1..3 loop
    var tmp := i * 2
    print tmp
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaError, UndeclaredVariableInPrint) {
    auto r = analyze("print z");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'z'"));
}

TEST(SemaError, UndeclaredVariableInExpression) {
    auto r = analyze("var x := y + 1");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'y'"));
}

TEST(SemaError, UndeclaredVariableInAssignRhs) {
    auto r = analyze("var x := 0\nx := unknown");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'unknown'"));
}

TEST(SemaError, UndeclaredInInitializer) {
    auto r = analyze("var x := x");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'x'"));
}

TEST(SemaError, ForRangeIteratorNotVisibleAfterLoop) {
    auto r = analyze(R"(
for i in 1..5 loop
    print i
end
print i
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'i'"));
}

TEST(SemaError, ForIterIteratorNotVisibleAfterLoop) {

    auto r = analyze(R"(
var arr := [1, 2, 3]
for item in arr loop
    print item
end
print item
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'item'"));
}

TEST(SemaError, VariableDeclaredInsideBodyNotVisibleOutside) {
    auto r = analyze(R"(
var x := 1
if x = 1 then
    var inner := 99
end
print inner
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'inner'"));
}

TEST(SemaError, FunctionParamNotVisibleOutsideFunction) {

    auto r = analyze(R"(
var f := func(x) => x + 1
print x
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'x'"));
}

TEST(SemaError, FunctionLocalVarNotVisibleOutside) {
    auto r = analyze(R"(
var f := func is
    var secret := 42
end
print secret
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'secret'"));
}

TEST(SemaError, DuplicateDeclInGlobalScope) {
    auto r = analyze("var x := 1\nvar x := 2");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "already declared"));
    EXPECT_TRUE(has_error(r, "'x'"));
}

TEST(SemaError, DuplicateDeclInFunctionScope) {
    auto r = analyze(R"(
var f := func is
    var y := 1
    var y := 2
end
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "already declared"));
    EXPECT_TRUE(has_error(r, "'y'"));
}

TEST(SemaError, DuplicateDeclInLoopBody) {
    auto r = analyze(R"(
loop
    var z := 1
    var z := 2
    exit
end
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "already declared"));
    EXPECT_TRUE(has_error(r, "'z'"));
}

TEST(SemaError, DuplicateFunctionParams) {
    auto r = analyze(R"(
var f := func(a, a) => a + 1
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "already declared"));
    EXPECT_TRUE(has_error(r, "'a'"));
}

TEST(SemaError, DuplicateDeclInSameVarStatement) {
    auto r = analyze("var n := 1, n := 2");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "already declared"));
}

TEST(SemaError, ExitOutsideAnyLoop) {
    auto r = analyze("exit");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "'exit' used outside of a loop"));
}

TEST(SemaError, ExitInFunctionOutsideLoop) {
    auto r = analyze(R"(
var f := func is
    exit
end
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "'exit' used outside of a loop"));
}

TEST(SemaError, ExitInsideIfOutsideLoop) {
    auto r = analyze(R"(
var x := 1
if x = 1 then
    exit
end
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "'exit' used outside of a loop"));
}

TEST(SemaError, ReturnOutsideAnyFunction) {
    auto r = analyze("return 42");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "'return' used outside of a function"));
}

TEST(SemaError, ReturnAtTopLevelNoValue) {
    auto r = analyze("return");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "'return' used outside of a function"));
}

TEST(SemaError, ReturnInsideLoopNotInsideFunction) {
    auto r = analyze(R"(
loop
    return 1
end
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "'return' used outside of a function"));
}

TEST(SemaValid, ExitInsideWhileInsideIf) {
    auto r = analyze(R"(
var x := 0
while x < 10 loop
    x := x + 1
    if x > 5 then
        exit
    end
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, ReturnInsideNestedIfInsideFunction) {
    auto r = analyze(R"(
var f := func(x) is
    if x > 0 then
        if x > 10 then
            return x
        else
            return 0
        end
    end
    return -1
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, ExitInsideLoopInsideFunction) {
    auto r = analyze(R"(
var f := func is
    loop
        exit
    end
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaError, MultipleErrorsReported) {
    auto r = analyze(R"(
print undeclared_var
exit
)");
    EXPECT_FALSE(r.ok);
    EXPECT_GE(r.errors.size(), 2u);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'exit' used outside of a loop"));
}

TEST(SemaError, MultipleUndeclaredSymbols) {
    auto r = analyze("print a, b, c");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.errors.size(), 3u);
}

TEST(SemaError, ErrorLineIsCorrect) {
    auto r = analyze("var x := 1\nprint z");
    ASSERT_FALSE(r.ok);
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0].loc.line, 2);
}

TEST(SemaError, DuplicateDeclLineIsCorrect) {
    auto r = analyze("var x := 1\nvar x := 2");
    ASSERT_FALSE(r.ok);
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0].loc.line, 2);
    EXPECT_TRUE(has_error(r, "line 1"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
