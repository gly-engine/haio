#include <haio_convert.hpp>

namespace {

constexpr uint8_t expand5(uint16_t value) { return static_cast<uint8_t>((value << 3) | (value >> 2)); }

}

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** the bottom bit is the whole alpha: a pixel is either there or it is not */
template <>
Result<void> Move<Color::RGBA5551, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 2 || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "rgba5551 to rgba8888 got a buffer that is too small");
    }

    for (size_t at = 0, to = 0; at < pixels * 2; at += 2, to += 4) {
        const auto packed = static_cast<uint16_t>(src[at + 0] | (src[at + 1] << 8));
        dst[to + 0] = expand5((packed >> 11) & 0x1f);
        dst[to + 1] = expand5((packed >> 6) & 0x1f);
        dst[to + 2] = expand5((packed >> 1) & 0x1f);
        dst[to + 3] = (packed & 1) ? 255 : 0;
    }
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::RGBA5551, Color::RGBA8888>(Image<Color::RGBA5551> src) {
    return convertVia<Color::RGBA5551, Color::RGBA8888>(std::move(src));
}
/** @} */

}
