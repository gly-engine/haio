#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** red and blue trade places and the alpha stays where it was */
template <>
Result<void> Move<Color::BGRA8888, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "bgra8888 to rgba8888 got a buffer that is too small");
    }

    for (size_t at = 0; at < pixels * 4; at += 4) {
        dst[at + 0] = src[at + 2];
        dst[at + 1] = src[at + 1];
        dst[at + 2] = src[at + 0];
        dst[at + 3] = src[at + 3];
    }
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::BGRA8888, Color::RGBA8888>(Image<Color::BGRA8888> src) {
    return convertVia<Color::BGRA8888, Color::RGBA8888>(std::move(src));
}
/** @} */

}
