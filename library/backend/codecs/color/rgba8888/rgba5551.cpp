#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/**
 * five bits a channel and one of alpha.
 *
 * the alpha rounds like everything else does, at half: a pixel that is more there
 * than not stays, and the rest goes. there is no third answer to give, which is the
 * whole reason a one bit alpha is worth asking for rather than something to apologise
 * for -- a sprite sheet for hardware that only ever tested one bit wants exactly this.
 */
template <>
Result<void> Move<Color::RGBA8888, Color::RGBA5551>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels * 2) {
        HAIO_FAIL(InvalidInput, "rgba8888 to rgba5551 got a buffer that is too small");
    }

    for (size_t at = 0, to = 0; at < pixels * 4; at += 4, to += 2) {
        const auto r = static_cast<uint16_t>((src[at + 0] * 31 + 127) / 255);
        const auto g = static_cast<uint16_t>((src[at + 1] * 31 + 127) / 255);
        const auto b = static_cast<uint16_t>((src[at + 2] * 31 + 127) / 255);
        const auto a = static_cast<uint16_t>(src[at + 3] >= 128 ? 1 : 0);
        const auto packed = static_cast<uint16_t>((r << 11) | (g << 6) | (b << 1) | a);
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
Result<Image<Color::RGBA5551>> Convert<Color::RGBA8888, Color::RGBA5551>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::RGBA5551>(std::move(src));
}
/** @} */

}
