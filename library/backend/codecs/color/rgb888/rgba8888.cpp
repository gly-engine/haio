#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/**
 * add an opaque alpha channel.
 *
 * @todo the mirror of the rgba8888 to rgb888 pack, and just as hot, but left scalar:
 * the 3 to 4 unpack needs a load that straddles the shuffle window, so it is a real
 * port rather than the same mask backwards.
 */
template <>
Result<void> Move<Color::RGB888, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 3 || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "rgb888 to rgba8888 got a buffer that is too small");
    }
    for (size_t at = 0, to = 0; to < pixels * 4; at += 3, to += 4) {
        dst[to + 0] = src[at + 0];
        dst[to + 1] = src[at + 1];
        dst[to + 2] = src[at + 2];
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
Result<Image<Color::RGBA8888>> Convert<Color::RGB888, Color::RGBA8888>(Image<Color::RGB888> src) {
    return convertVia<Color::RGB888, Color::RGBA8888>(std::move(src));
}
/** @} */

}
