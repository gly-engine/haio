#include <haio.hpp>

auto main(int argc, char* argv[]) -> int {
    if (argc != 3) {
        std::cerr << "Usage: convert_png_2_ppm <image1> <image2>\n";
        return 1;
    }

    auto pipe = Haio::Decode<Haio::Format::PNG>() | Haio::Encode<Haio::Format::PPM>();
    auto input = std::ifstream(argv[1],  std::ios::binary);
    auto output = std::ofstream(argv[2],  std::ios::binary);

    input >> pipe >> output;
    return 0;
}
