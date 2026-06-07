#include <fstream>
#include <iostream>
#include <regex>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: replace <file> <regex> <replace>\n";
        return 1;
    }

    std::string path = argv[1];
    std::regex  pattern(argv[2]);
    std::string replacement = argv[3];

    std::ifstream in(path);
    if (!in) {
        std::cerr << "Cannot open: " << path << "\n";
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    std::string result = std::regex_replace(content, pattern, replacement);

    std::ofstream out(path);
    if (!out) {
        std::cerr << "Cannot write: " << path << "\n";
        return 1;
    }
    out << result;

    return 0;
}