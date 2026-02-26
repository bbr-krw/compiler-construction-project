#include "token_dump.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char* argv[]) {
    std::string input;

    if (argc >= 2) {
        std::ifstream file(argv[1]);
        if (!file.is_open()) {
            std::cerr << "Cannot open file: " << argv[1] << "\n";
            return 1;
        }
        input = std::string(std::istreambuf_iterator<char>(file),
                            std::istreambuf_iterator<char>());
    } else {
        input = std::string(std::istreambuf_iterator<char>(std::cin),
                            std::istreambuf_iterator<char>());
    }

    std::cout << dump_tokens(input);
    return 0;
}
