#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** five bits of red, six of green, five of blue, rounded rather than truncated */
template <>
Result<void> Move<Color::RGBA8888, Color::RGB565>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels * 2) {
        HAIO_FAIL(InvalidInput, "rgba8888 to rgb565 got a buffer that is too small");
    }

    for (size_t at = 0, to = 0; at < pixels * 4; at += 4, to += 2) {
        const auto r = static_cast<uint16_t>((src[at + 0] * 31 + 127) / 255);
        const auto g = static_cast<uint16_t>((src[at + 1] * 63 + 127) / 255);
        const auto b = static_cast<uint16_t>((src[at + 2] * 31 + 127) / 255);
        const auto packed = static_cast<uint16_t>((r << 11) | (g << 5) | b);
        dst[to + 0] = static_cast<uint8_t>(packed);
        dst[to + 1] = static_cast<uint8_t>(packed >> 8);
    }
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGB565>> Convert<Color::RGBA8888, Color::RGB565>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::RGB565>(std::move(src));
}
/** @} */

}
