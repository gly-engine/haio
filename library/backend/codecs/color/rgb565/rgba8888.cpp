#include <haio_convert.hpp>

namespace {

/** five and six bit channels widen by repeating their high bits, not by shifting */
constexpr uint8_t expand5(uint16_t value) { return static_cast<uint8_t>((value << 3) | (value >> 2)); }
constexpr uint8_t expand6(uint16_t value) { return static_cast<uint8_t>((value << 2) | (value >> 4)); }

}

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
template <>
Result<void> Move<Color::RGB565, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 2 || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "rgb565 to rgba8888 got a buffer that is too small");
    }

    for (size_t at = 0, to = 0; at < pixels * 2; at += 2, to += 4) {
        const auto packed = static_cast<uint16_t>(src[at + 0] | (src[at + 1] << 8));
        dst[to + 0] = expand5((packed >> 11) & 0x1f);
        dst[to + 1] = expand6((packed >> 5) & 0x3f);
        dst[to + 2] = expand5(packed & 0x1f);
        dst[to + 3] = 255;
    }
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::RGB565, Color::RGBA8888>(Image<Color::RGB565> src) {
    return convertVia<Color::RGB565, Color::RGBA8888>(std::move(src));
}
/** @} */

}
