#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/**
 * five bits a channel, rounded rather than truncated, and the alpha is dropped: this
 * colour has nowhere to put it and saying so here is better than leaving a caller to
 * wonder why a transparent picture came back solid.
 */
template <>
Result<void> Move<Color::RGBA8888, Color::RGB555>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels * 2) {
        HAIO_FAIL(InvalidInput, "rgba8888 to rgb555 got a buffer that is too small");
    }

    for (size_t at = 0, to = 0; at < pixels * 4; at += 4, to += 2) {
        const auto r = static_cast<uint16_t>((src[at + 0] * 31 + 127) / 255);
        const auto g = static_cast<uint16_t>((src[at + 1] * 31 + 127) / 255);
        const auto b = static_cast<uint16_t>((src[at + 2] * 31 + 127) / 255);
        const auto packed = static_cast<uint16_t>((r << 10) | (g << 5) | b);
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
Result<Image<Color::RGB555>> Convert<Color::RGBA8888, Color::RGB555>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::RGB555>(std::move(src));
}
/** @} */

}
