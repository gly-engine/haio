#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** rec.709, as imagemagick weighs it; the alpha goes, as it does for rgb888 */
template <>
Result<void> Move<Color::RGBA8888, Color::GRAY8>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels) {
        HAIO_FAIL(InvalidInput, "rgba8888 to gray8 got a buffer that is too small");
    }
    for (size_t at = 0, to = 0; to < pixels; at += 4, to++) {
        const double grey = 0.212656 * src[at] + 0.715158 * src[at + 1] + 0.072186 * src[at + 2];
        dst[to] = static_cast<uint8_t>(grey + 0.5);
    }
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::GRAY8>> Convert<Color::RGBA8888, Color::GRAY8>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::GRAY8>(std::move(src));
}
/** @} */

}
