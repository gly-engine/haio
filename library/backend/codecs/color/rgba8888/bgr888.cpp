#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** blue first and the alpha dropped, which is a tga or a bmp scanline */
template <>
Result<void> Move<Color::RGBA8888, Color::BGR888>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels * 3) {
        HAIO_FAIL(InvalidInput, "rgba8888 to bgr888 got a buffer that is too small");
    }

    for (size_t at = 0, to = 0; at < pixels * 4; at += 4, to += 3) {
        dst[to + 0] = src[at + 2];
        dst[to + 1] = src[at + 1];
        dst[to + 2] = src[at + 0];
    }
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::BGR888>> Convert<Color::RGBA8888, Color::BGR888>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::BGR888>(std::move(src));
}
/** @} */

}
