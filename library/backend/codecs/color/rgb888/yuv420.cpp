#include <haio_convert.hpp>

#include <utility>

#include <libyuv/convert.h>

namespace Haio::Codecs {

/**
 * @addtogroup move
 * @{
 */
/**
 * full range bt.601, the same as the rgba8888 path, so a jpeg of a png comes out the
 * same whether or not the png had an alpha channel to drop on the way.
 *
 * libyuv names formats by the order of a little endian word, not of the bytes: r, g
 * and b in memory is what it calls RAW, and its RGB24 is the one stored as b, g, r.
 */
template <>
Result<void> Move<Color::RGB888, Color::YUV420>(Bytes src, std::span<uint8_t> dst, Size size) {
    if (size.width % 2 != 0 || size.height % 2 != 0) {
        HAIO_FAIL(InvalidInput, "yuv420 needs even width and height");
    }

    const auto width = static_cast<int>(size.width);
    const auto height = static_cast<int>(size.height);
    const auto pixels = static_cast<size_t>(width) * height;
    const auto chroma = static_cast<size_t>(width / 2) * (height / 2);

    if (src.size() < pixels * 3 || dst.size() < pixels + chroma * 2) {
        HAIO_FAIL(InvalidInput, "rgb888 to yuv420 got a buffer that is too small");
    }

    auto* y = dst.data();
    auto* u = y + pixels;
    auto* v = u + chroma;

    const auto result = libyuv::RAWToJ420(
        src.data(),
        width * 3,
        y,
        width,
        u,
        width / 2,
        v,
        width / 2,
        width,
        height
    );

    if (result != 0) {
        HAIO_FAIL(InvalidInput, "libyuv failed to convert rgb888 to yuv420");
    }

    return {};
}
/** @} */

/**
 * @addtogroup convert
 * @{
 */
template <>
Result<Image<Color::YUV420>> Convert<Color::RGB888, Color::YUV420>(Image<Color::RGB888> src) {
    return convertVia<Color::RGB888, Color::YUV420>(std::move(src));
}
/** @} */

}
