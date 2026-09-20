#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** one sample becomes three, opaque. grey is grey in every channel */
template <>
Result<void> Move<Color::GRAY8, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "gray8 to rgba8888 got a buffer that is too small");
    }
    for (size_t at = 0, to = 0; at < pixels; at++, to += 4) {
        const auto level = src[at];
        dst[to + 0] = level;
        dst[to + 1] = level;
        dst[to + 2] = level;
        dst[to + 3] = 0xFF;
    }
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::RGBA8888>> Convert<Color::GRAY8, Color::RGBA8888>(Image<Color::GRAY8> src) {
    return convertVia<Color::GRAY8, Color::RGBA8888>(std::move(src));
}
/** @} */

}
