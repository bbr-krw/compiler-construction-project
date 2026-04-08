#include "ast.hpp"
#include "interpreter.hpp"
#include "lexer.hpp"
#include "parser.tab.hpp"
#include "semantic_analyzer.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

static const std::string SUITE_DIR{TEST_SUITE_DIR};

class InterpSuiteTest : public ::testing::TestWithParam<int> {};

TEST_P(InterpSuiteTest, RunAndCompareGolden) {
    int n                  = GetParam();
    std::string input_path = SUITE_DIR + "/test" + std::to_string(n) + ".dl";
    std::string gold_path  = SUITE_DIR + "/test" + std::to_string(n) + ".gold";

    if (!fs::exists(input_path) || !fs::exists(gold_path))
        GTEST_SKIP() << "files missing for test" << n;

    std::string src      = read_file(input_path);
    std::string expected = read_file(gold_path);

    std::unique_ptr<ASTNode> root;
    std::istringstream stream(src);
    Lexer lexer(stream);
    yy::parser parser{root, lexer};
    ASSERT_EQ(parser.parse(), 0) << "parse failed for test" << n;
    ASSERT_NE(root, nullptr);

    SemanticAnalyzer sema;
    sema.analyze(*root);
    ASSERT_TRUE(sema.ok()) << "sema error for test" << n;

    std::ostringstream out;
    Interpreter interp(out);
    ASSERT_NO_THROW(interp.run(*root)) << "runtime error for test" << n;

    EXPECT_EQ(out.str(), expected) << "output mismatch for test" << n;
}

INSTANTIATE_TEST_SUITE_P(Suite, InterpSuiteTest, ::testing::Range(1, 152),
                         [](const ::testing::TestParamInfo<int>& i) {
                             return "test" + std::to_string(i.param);
                         });

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}