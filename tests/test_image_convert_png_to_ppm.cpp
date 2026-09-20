#include <haio.hpp>

#include <fstream>
#include <iostream>

auto main(int argc, char* argv[]) -> int {
    if (argc != 3) {
        std::cerr << "Usage: convert_png_2_ppm <image1> <image2>\n";
        return 1;
    }

    // built once, runs on anything the bytes turn out to be
    const auto pipe = Haio::Pipe::Decode | Haio::Pipe::Encode<Haio::Format::PPM>;

    auto input = std::ifstream(argv[1], std::ios::binary);
    auto output = std::ofstream(argv[2], std::ios::binary);

    if (const auto result = input >> pipe >> output; !result) {
        std::cerr << result.error().message << '\n';
        return 1;
    }
    return 0;
}
