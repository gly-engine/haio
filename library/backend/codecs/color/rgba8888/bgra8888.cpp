#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/**
 * the same swap as the way back, written out again rather than shared.
 *
 * it is its own file because the pair is what the build scans for: a move that
 * appeared in the other direction's file would be declared whenever that one was
 * compiled, which is not the same thing as this one existing.
 */
template <>
Result<void> Move<Color::RGBA8888, Color::BGRA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "rgba8888 to bgra8888 got a buffer that is too small");
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
Result<Image<Color::BGRA8888>> Convert<Color::RGBA8888, Color::BGRA8888>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::BGRA8888>(std::move(src));
}
/** @} */

}
