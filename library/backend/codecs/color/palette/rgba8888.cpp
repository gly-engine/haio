#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * indices become the colours they point at.
 *
 * this one direction is always possible and never loses anything, which is why it
 * needs no filter while the way back does.
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::PALETTE, Color::RGBA8888>(Image<Color::PALETTE> src) {
    const auto pixels = static_cast<size_t>(src.width) * static_cast<size_t>(src.height);
    if (src.data.size() != pixels) HAIO_FAIL(InvalidInput, "invalid palette image: one index per pixel");
    if (src.entries.empty()) {
        HAIO_FAIL(InvalidInput, "this picture is indices with no palette; give it one with -palete");
    }

    std::vector<uint8_t> out(pixels * 4);
    for (size_t at = 0; at < pixels; at++) {
        const auto index = src.data[at];
        if (index >= src.entries.size()) {
            HAIO_FAIL(InvalidInput, "the picture points at colour " + std::to_string(index)
                                        + " and the palette has " + std::to_string(src.entries.size()));
        }
        const auto colour = src.entries[index];
        out[at * 4 + 0] = static_cast<uint8_t>((colour >> 16) & 0xFF);
        out[at * 4 + 1] = static_cast<uint8_t>((colour >> 8) & 0xFF);
        out[at * 4 + 2] = static_cast<uint8_t>(colour & 0xFF);
        out[at * 4 + 3] = 0xFF;
    }
    return Image<Color::RGBA8888>{src.width, src.height, std::move(out)};
}

}
