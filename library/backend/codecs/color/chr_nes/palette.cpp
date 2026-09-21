#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * unpacks nes pattern data into one index per byte.
 *
 * a tile is sixteen bytes: eight rows of the low bit, then eight rows of the high
 * bit. so a pixel's two bits sit eight bytes apart, and the leftmost pixel is the
 * most significant bit rather than the least.
 *
 * nothing here needs a palette, which is why it fits Move at all: the indices are
 * 0 to 3 whatever colours are eventually hung on them.
 */
template <>
Result<void> Move<Color::CHR_NES, Color::PALETTE>(Bytes src, std::span<uint8_t> dst, Size size) {
    if (size.width % 8 != 0 || size.height % 8 != 0) {
        HAIO_FAIL(InvalidInput, "nes chr is made of 8x8 tiles, so both sides must be multiples of 8");
    }

    const auto width = static_cast<size_t>(size.width);
    const auto height = static_cast<size_t>(size.height);
    const auto columns = width / 8;
    const auto tiles = columns * (height / 8);

    if (src.size() < tiles * 16 || dst.size() < width * height) {
        HAIO_FAIL(InvalidInput, "chr_nes to palette got a buffer that is too small");
    }

    for (size_t tile = 0; tile < tiles; tile++) {
        const auto at = tile * 16;
        const auto originX = (tile % columns) * 8;
        const auto originY = (tile / columns) * 8;

        for (size_t y = 0; y < 8; y++) {
            const auto low = src[at + y];
            const auto high = src[at + y + 8];

            for (size_t x = 0; x < 8; x++) {
                const auto bit = 7 - x;
                const auto index = ((low >> bit) & 1) | (((high >> bit) & 1) << 1);
                dst[(originY + y) * width + originX + x] = static_cast<uint8_t>(index);
            }
        }
    }
    return {};
}

/**
 * the indices arrive without colours. what to hang on them is a separate question,
 * answered by whoever asked for the conversion, because a chr file does not carry a
 * palette and never did: the console held it somewhere else entirely.
 */
template <>
Result<Image<Color::PALETTE>> Convert<Color::CHR_NES, Color::PALETTE>(Image<Color::CHR_NES> src) {
    return convertVia<Color::CHR_NES, Color::PALETTE>(std::move(src));
}

}
