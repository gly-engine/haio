#include <iostream>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <vector>
#include <string>

std::string basename(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? path : path.substr(pos + 1);
    auto dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

int main(int argc, char** argv) {
    std::ifstream file(argv[1], std::ios::binary);

    if (!file) return 1;

    std::vector<unsigned char> buffer(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    constexpr int perline = 12;
    std::string varname = basename(argv[1]);

    std::cout << "const unsigned char " << varname << "[] = {\n";

    for (size_t i = 0; i < buffer.size(); ++i) {
        std::cout << "0x"
                  << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(buffer[i]);

        if (i + 1 < buffer.size()) std::cout << ", ";
        if ((i + 1) % perline == 0) std::cout << "\n";
    }

    std::cout << "\n};\n";
    std::cout << "const unsigned int " << varname << "_len = " << buffer.size() << ";\n";
    return 0;
}
