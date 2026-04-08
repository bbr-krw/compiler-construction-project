#include "ast.hpp"
#include "lexer.hpp"
#include "parser.tab.hpp"
#include "semantic_analyzer.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>

// ── Helpers ───────────────────────────────────────────────────────────────────

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

// Finds the first error whose message contains `substr`.
static bool has_error(const SemaResult& r, const std::string& substr) {
    for (const auto& e : r.errors)
        if (e.message.find(substr) != std::string::npos)
            return true;
    return false;
}

// ── Valid programs ─────────────────────────────────────────────────────────────

TEST(SemaValid, SimpleVarDeclAndUse) {
    // Declaring a variable and printing it is valid.
    auto r = analyze("var x := 42\nprint x");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, MultipleVarDeclOnOneLine) {
    // var x, y := 1 declares two variables; both usable.
    auto r = analyze("var x, y := 1\nprint x\nprint y");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, NestedScopeShadowing) {
    // The spec allows a variable in a nested scope to shadow an outer one.
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
    // exit is valid inside a while loop.
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
    // exit is valid inside an infinite loop construct.
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
    // for 1..3 loop ... end — valid, no iterator variable.
    auto r = analyze(R"(
for 1..3 loop
    print "hello"
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, ForRangeWithIterator) {
    // Iterator variable is accessible inside the loop body.
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
    // for item in arr loop — item is accessible inside the body.
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
    // for arr loop — iterating without naming the element.
    auto r = analyze(R"(
var arr := [1, 2, 3]
for arr loop
    print "x"
end
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, NestedForLoopsExitIsValid) {
    // exit inside a nested for-range loop should not error.
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
    // return inside a function body is valid.
    auto r = analyze(R"(
var f := func(n) is
    return n * 2
end
print f(5)
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, FunctionParamsInScope) {
    // Parameters are accessible throughout the function body.
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
    // func(x) => x + 1 — arrow-body function, no explicit return.
    auto r = analyze(R"(
var f := func(x) => x + 1
print f(10)
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, FunctionNoParams) {
    // func => 42  — zero-parameter function.
    auto r = analyze(R"(
var f := func => 42
print f()
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, NestedFunctionsInnerReturn) {
    // A return inside an inner function is valid (it only returns the inner func).
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
    // return with no expression is valid inside a function.
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
    // `x is int` — valid type-check expression.
    auto r = analyze(R"(
var x := 42
print x is int
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, IsTypeCheckAllTypes) {
    // All eight TypeIndicators used in is-expressions are valid.
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
    // Standard if/then/else/end — variables inside bodies are locally scoped.
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
    // Assigning to arr[k] is valid as long as arr is declared.
    auto r = analyze(R"(
var t := []
t[10] := 25
print t[10]
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, TupleFieldAccess) {
    // Accessing a named tuple field via dot notation.
    auto r = analyze(R"(
var t := {a := 1, b := 2}
print t.a
print t.b
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, TupleIntAccess) {
    // Accessing a tuple element by integer index.
    auto r = analyze(R"(
var t := {a := 1, b := 2}
print t.1
print t.2
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, RecursiveFunction) {
    // A function that references itself by name — the var is declared before
    // the function body executes, so the name must be in scope.
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
    // print e1, e2, e3 — all sub-expressions are validated.
    auto r = analyze(R"(
var a := 1
var b := 2
var c := 3
print a, b, c
)");
    EXPECT_TRUE(r.ok);
}

TEST(SemaValid, VariableDeclaredInLoopBodyNotLeaking) {
    // var declared inside a loop body is not accessible after the loop —
    // but no error is expected on the loop itself; this tests that the
    // inner declaration doesn't pollute the outer scope.
    auto r = analyze(R"(
for i in 1..3 loop
    var tmp := i * 2
    print tmp
end
)");
    EXPECT_TRUE(r.ok);
}

// ── Undeclared variable errors ────────────────────────────────────────────────

TEST(SemaError, UndeclaredVariableInPrint) {
    // Referencing a name that was never declared.
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
    // var x := x  — x is not yet in scope at the point of its own initializer.
    auto r = analyze("var x := x");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'x'"));
}

TEST(SemaError, ForRangeIteratorNotVisibleAfterLoop) {
    // Iterator `i` is scoped to the loop; using it afterwards is an error.
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
    // Same rule for for-iter loops.
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
    // var declared inside an if-body is not visible in the outer scope.
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
    // A function parameter is only in scope inside the function body.
    auto r = analyze(R"(
var f := func(x) => x + 1
print x
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "undeclared"));
    EXPECT_TRUE(has_error(r, "'x'"));
}

TEST(SemaError, FunctionLocalVarNotVisibleOutside) {
    // Locals declared inside a function body are not visible outside.
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

// ── Duplicate declaration errors ──────────────────────────────────────────────

TEST(SemaError, DuplicateDeclInGlobalScope) {
    auto r = analyze("var x := 1\nvar x := 2");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "already declared"));
    EXPECT_TRUE(has_error(r, "'x'"));
}

TEST(SemaError, DuplicateDeclInFunctionScope) {
    // Two vars with the same name inside the same function body.
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
    // Two vars with the same name inside the same loop body.
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
    // Two parameters with the same name in one function.
    auto r = analyze(R"(
var f := func(a, a) => a + 1
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "already declared"));
    EXPECT_TRUE(has_error(r, "'a'"));
}

// Redeclaring in same var statement counts as duplicate in same scope.
TEST(SemaError, DuplicateDeclInSameVarStatement) {
    auto r = analyze("var n := 1, n := 2");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "already declared"));
}

// ── exit / return context errors ──────────────────────────────────────────────

TEST(SemaError, ExitOutsideAnyLoop) {
    auto r = analyze("exit");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "'exit' used outside of a loop"));
}

TEST(SemaError, ExitInFunctionOutsideLoop) {
    // A function body is not a loop — exit inside it without a loop is invalid.
    auto r = analyze(R"(
var f := func is
    exit
end
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "'exit' used outside of a loop"));
}

TEST(SemaError, ExitInsideIfOutsideLoop) {
    // if-body doesn't make it a loop context.
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
    // A loop body is not a function context.
    auto r = analyze(R"(
loop
    return 1
end
)");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(has_error(r, "'return' used outside of a function"));
}

// ── Valid exit / return inside nested constructs ───────────────────────────────

TEST(SemaValid, ExitInsideWhileInsideIf) {
    // The `exit` is ultimately inside a while loop, so it is valid.
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
    // return is valid from any depth inside a function.
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
    // A loop inside a function — exit is valid.
    auto r = analyze(R"(
var f := func is
    loop
        exit
    end
end
)");
    EXPECT_TRUE(r.ok);
}

// ── Multiple errors collected ─────────────────────────────────────────────────

TEST(SemaError, MultipleErrorsReported) {
    // Both undeclared variable and exit-outside-loop should be reported.
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
    // Each undeclared reference should produce its own error.
    auto r = analyze("print a, b, c");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.errors.size(), 3u);
}

// ── Error location accuracy ────────────────────────────────────────────────────

TEST(SemaError, ErrorLineIsCorrect) {
    // The undeclared error should point at line 2 (1-based).
    auto r = analyze("var x := 1\nprint z");
    ASSERT_FALSE(r.ok);
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0].line, 2);
}

TEST(SemaError, DuplicateDeclLineIsCorrect) {
    auto r = analyze("var x := 1\nvar x := 2");
    ASSERT_FALSE(r.ok);
    ASSERT_EQ(r.errors.size(), 1u);
    // Error is reported at the second declaration (line 2).
    EXPECT_EQ(r.errors[0].line, 2);
    // And message mentions original declaration line 1.
    EXPECT_TRUE(has_error(r, "line 1"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
