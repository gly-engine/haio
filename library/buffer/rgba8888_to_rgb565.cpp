#include <haio.hpp>

#include <cstdint>

namespace Haio::Buffer {

template <>
void Copy<Format::RGBA8888, Format::RGB565>(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    output.resize((input.size() / 4) * 2);
    for (size_t src = 0, dst = 0; src < input.size(); src += 4, dst += 2) {
        const auto r = static_cast<uint16_t>((input[src + 0] * 31 + 127) / 255);
        const auto g = static_cast<uint16_t>((input[src + 1] * 63 + 127) / 255);
        const auto b = static_cast<uint16_t>((input[src + 2] * 31 + 127) / 255);
        const auto px = static_cast<uint16_t>((r << 11) | (g << 5) | b);
        output[dst + 0] = static_cast<uint8_t>(px);
        output[dst + 1] = static_cast<uint8_t>(px >> 8);
    }
}

}
