#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/** rec.709, as imagemagick weighs it */
template <>
Result<void> Move<Color::RGBA8888, Color::GRAYALPHA88>(Bytes src, std::span<uint8_t> dst, Size size) {
    const auto pixels = static_cast<size_t>(size.width) * static_cast<size_t>(size.height);
    if (src.size() < pixels * 4 || dst.size() < pixels * 2) {
        HAIO_FAIL(InvalidInput, "rgba8888 to grayalpha88 got a buffer that is too small");
    }
    for (size_t at = 0, to = 0; to < pixels * 2; at += 4, to += 2) {
        const double grey = 0.212656 * src[at] + 0.715158 * src[at + 1] + 0.072186 * src[at + 2];
        dst[to + 0] = static_cast<uint8_t>(grey + 0.5);
        dst[to + 1] = src[at + 3];
    }
    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::GRAYALPHA88>> Convert<Color::RGBA8888, Color::GRAYALPHA88>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::GRAYALPHA88>(std::move(src));
}
/** @} */

}
