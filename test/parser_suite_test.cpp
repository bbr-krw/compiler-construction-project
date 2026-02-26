#include "parser.tab.hpp"
#include "lexer.yy.hpp"
#include "ast.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// Helper to read file contents
std::string read_file(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + path);
  }
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

// Helper to capture AST output
std::string capture_ast_output(ASTNode* root) {
  std::stringstream ss;
  if (root) {
    root->print(0);  // This prints to stdout, we need to redirect
  }
  return ss.str();
}

// Helper to run parser on input string
ASTNode* parse_input(const std::string& input) {
  ASTNode* parse_result = nullptr;
  yylineno = 1;
  yy_scan_string(input.c_str());
  yy::parser parser{parse_result};
  int result = parser.parse();
  
  if (result != 0) {
    return nullptr;  // Parse failed
  }
  return parse_result;
}

class SuiteTest : public ::testing::TestWithParam<int> {
protected:
  std::string suite_dir = "/home/kirill/itmo/compiler-construction-project/test/suite";
  
  std::string get_test_input_path(int test_num) {
    return suite_dir + "/test" + std::to_string(test_num) + ".d";
  }
  
  std::string get_test_gold_path(int test_num) {
    return suite_dir + "/test" + std::to_string(test_num) + ".pgold";
  }
};

// Parameterized test: parse each .d file and compare AST output
TEST_P(SuiteTest, ParseAndCompareGolden) {
  int test_num = GetParam();
  std::string input_path = get_test_input_path(test_num);
  std::string gold_path = get_test_gold_path(test_num);
  
  // Skip if files don't exist
  if (!fs::exists(input_path) || !fs::exists(gold_path)) {
    GTEST_SKIP() << "Test files not found for test" << test_num;
  }
  
  std::string input = read_file(input_path);
  std::string expected_gold = read_file(gold_path);
  
  // Parse the input
  ASTNode* root = parse_input(input);
  
  // If gold file indicates parse failure, root should be null
  if (expected_gold.find("Parse error") != std::string::npos) {
    EXPECT_EQ(root, nullptr) << "Expected parse error for test" << test_num;
    return;
  }
  
  ASSERT_NE(root, nullptr) << "Parse failed for test" << test_num;
  
  // Capture AST output and compare with golden
  std::stringstream captured;
  root->print(0, captured);
  std::string actual_output = captured.str();
  
  EXPECT_EQ(actual_output, expected_gold)
    << "AST output mismatch for test" << test_num;
  
  delete root;
}

// Generate parameterized tests for tests 1-150
INSTANTIATE_TEST_SUITE_P(SuiteTests, SuiteTest,
  ::testing::Range(1, 151),
  [](const ::testing::TestParamInfo<int>& info) {
    return "test" + std::to_string(info.param);
  });

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}