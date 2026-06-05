#include <haio.hpp>

auto main() -> int {
    auto pipe = Haio::Decode<Haio::Format::PNG>() | Haio::Encode<Haio::Format::PPM>();
    auto input = std::ifstream("assets/logo8x8.png",  std::ios::binary);
    auto output = std::ofstream("build/logo8x8.ppm",  std::ios::binary);

    input >> pipe >> output;
    return 0;
}
