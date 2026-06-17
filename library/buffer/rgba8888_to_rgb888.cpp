#include <haio.hpp>

namespace Haio::Buffer {

template <>
void Copy<Format::RGBA8888, Format::RGB888>(const std::vector<uint8_t>& input, std::vector<uint8_t>& output) {
    const auto outOffset = output.size();
    const auto pixelCount = input.size() / 4;
    output.resize(outOffset + pixelCount * 3);

    for (size_t src = 0, dst = outOffset; src < pixelCount * 4; src += 4, dst += 3) {
        output[dst + 0] = input[src + 0];
        output[dst + 1] = input[src + 1];
        output[dst + 2] = input[src + 2];
    }
}

}
