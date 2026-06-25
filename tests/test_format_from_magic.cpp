#include <haio.hpp>

#include <array>
#include <cassert>

int main() {
    constexpr std::array<uint8_t, 8> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    constexpr std::array<uint8_t, 3> ppm = {'P', '6', '\n'};

    assert(Haio::formatFromMagic(png) == Haio::Format::PNG);
    assert(Haio::formatFromMagic(ppm) == Haio::Format::RAW);
    assert(Haio::formatFromMagic({}) == Haio::Format::RAW);

    return 0;
}
