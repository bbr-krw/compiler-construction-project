#include "token_dump.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("Cannot open: " + path);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static const std::string SUITE_DIR{TEST_SUITE_DIR};

class LexerSuiteTest : public ::testing::TestWithParam<int> {
protected:
    std::string input_path(int n) const {
        return SUITE_DIR + "/test" + std::to_string(n) + ".d";
    }
    std::string gold_path(int n) const {
        return SUITE_DIR + "/test" + std::to_string(n) + ".lgold";
    }
};

TEST_P(LexerSuiteTest, TokensMatchGolden) {
    const int n = GetParam();
    const std::string inp  = input_path(n);
    const std::string gold = gold_path(n);

    if (!fs::exists(inp) || !fs::exists(gold)) {
        GTEST_SKIP() << "Files not found for test" << n;
    }

    const std::string actual   = dump_tokens(read_file(inp));
    const std::string expected = read_file(gold);

    EXPECT_EQ(actual, expected) << "Token mismatch for test" << n;
}

INSTANTIATE_TEST_SUITE_P(
    Suite, LexerSuiteTest,
    ::testing::Range(1, 151),
    [](const ::testing::TestParamInfo<int>& info) {
        return "test" + std::to_string(info.param);
    });

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
