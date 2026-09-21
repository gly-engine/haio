#include <haio.hpp>

#include <fstream>
#include <iostream>

auto main(int argc, char* argv[]) -> int {
    if (argc != 3) {
        std::cerr << "Usage: convert_png_2_etc1_ppm <image1> <image2>\n";
        return 1;
    }

    // a lossy round trip: compress to etc1, bring it back, then write something viewable
    const auto pipe = Haio::Pipe::Decode
                    | Haio::Pipe::Convert<Haio::Color::ETC1>
                    | Haio::Pipe::Convert<Haio::Color::RGBA8888>
                    | Haio::Pipe::Encode<Haio::Format::PPM>;

    auto input = std::ifstream(argv[1], std::ios::binary);
    auto output = std::ofstream(argv[2], std::ios::binary);

    if (const auto result = input >> pipe >> output; !result) {
        std::cerr << result.error().message << '\n';
        return 1;
    }
    return 0;
}
