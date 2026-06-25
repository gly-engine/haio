#include <haio.hpp>

#include <cstdint>

namespace {

static inline uint8_t expand5(uint16_t x) {
    return static_cast<uint8_t>((x * 255 + 15) / 31);
}

static inline uint8_t expand6(uint16_t x) {
    return static_cast<uint8_t>((x * 255 + 31) / 63);
}

}

namespace Haio::Buffer {

template <>
void Copy<Format::RGB565, Format::RGBA8888>(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    output.resize((input.size() / 2) * 4);
    for (size_t src = 0, dst = 0; src < input.size(); src += 2, dst += 4) {
        const auto px = static_cast<uint16_t>(input[src + 0] | (input[src + 1] << 8));
        output[dst + 0] = expand5((px >> 11) & 0x1f);
        output[dst + 1] = expand6((px >> 5) & 0x3f);
        output[dst + 2] = expand5(px & 0x1f);
        output[dst + 3] = 255;
    }
}

}
