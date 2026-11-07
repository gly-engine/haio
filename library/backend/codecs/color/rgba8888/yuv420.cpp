#include <haio/internal/codecs/yuv.hpp>
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
    const auto planes = Yuv420::planesOf(size);
    const auto width = static_cast<int>(size.width);
    const auto height = static_cast<int>(size.height);
    const auto pixels = static_cast<size_t>(width) * height;

    if (src.size() < pixels * 4 || dst.size() < planes.bytes) {
        HAIO_FAIL(InvalidInput, "rgba8888 to yuv420 got a buffer that is too small");
    }

    auto* y = dst.data();
    auto* u = y + planes.luma;
    auto* v = u + planes.chroma;

    const auto result = libyuv::ABGRToJ420(
        src.data(),
        width * 4,
        y,
        planes.lumaWidth,
        u,
        planes.chromaWidth,
        v,
        planes.chromaWidth,
        width,
        height
    );

    if (result != 0) {
        HAIO_FAIL(InvalidInput, "libyuv failed to convert rgba8888 to yuv420");
    }

    // libyuv writes the real pixels; the spare luma column and row are ours to fill
    Yuv420::repeatEdges(dst, size, planes);
    return {};
}

template <>
Result<Image<Color::YUV420>> Convert<Color::RGBA8888, Color::YUV420>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::YUV420>(std::move(src));
}

}
