#include <haio_convert.hpp>

#include <utility>

#include <libyuv/convert_from_argb.h>

namespace Haio::Codecs {

/**
 * the way back: full range bt.601 again, and the chroma of a two by two block is the
 * average of its four pixels rather than the top left one.
 *
 * taking one pixel and calling it the block would be cheaper and is what a naive
 * encoder does; it shows up as ragged colour on edges, because the sample that
 * happened to be first decides for three others.
 */
template <>
Result<void> Move<Color::RGBA8888, Color::YUV420>(Bytes src, std::span<uint8_t> dst, Size size) {
    if (size.width % 2 != 0 || size.height % 2 != 0) {
        HAIO_FAIL(InvalidInput, "yuv420 needs even width and height");
    }

    const auto width = static_cast<int>(size.width);
    const auto height = static_cast<int>(size.height);
    const auto pixels = static_cast<size_t>(width) * height;
    const auto chroma = static_cast<size_t>(width / 2) * (height / 2);

    if (src.size() < pixels * 4 || dst.size() < pixels + chroma * 2) {
        HAIO_FAIL(InvalidInput, "rgba8888 to yuv420 got a buffer that is too small");
    }

    auto* y = dst.data();
    auto* u = y + pixels;
    auto* v = u + chroma;

    const auto result = libyuv::ABGRToJ420(
        src.data(),
        width * 4,
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
        HAIO_FAIL(InvalidInput, "libyuv failed to convert rgba8888 to yuv420");
    }

    return {};
}

template <>
Result<Image<Color::YUV420>> Convert<Color::RGBA8888, Color::YUV420>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::YUV420>(std::move(src));
}

}
