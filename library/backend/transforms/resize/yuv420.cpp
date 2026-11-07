#include <haio/internal/codecs/yuv.hpp>
#include <haio/transforms/resize.hpp>

#include <algorithm>

namespace Haio::Transforms {

/**
 * nearest neighbour a plane at a time, so a jpeg is resized without going through
 * rgba and back.
 *
 * a chroma sample stands for the two by two block whose top left luma pixel is at
 * twice its place, so it is picked from wherever that pixel was picked from: luma and
 * chroma come from the same spot of the source and the colour never slides off the
 * brightness it belongs to.
 */
template <>
Result<Image<Color::YUV420>> Resize<Color::YUV420>(Image<Color::YUV420> image, Size size, ResizeFilter filter) {
    if (filter != ResizeFilter::Point) HAIO_FAIL(InvalidInput, "this resize filter does not run on yuv420");
    if (size.width <= 0 && size.height <= 0) HAIO_FAIL(InvalidInput, "resize expects a positive size");
    if (image.width <= 0 || image.height <= 0) HAIO_FAIL(InvalidInput, "resize got an empty yuv420 picture");

    const Size from{image.width, image.height};
    const auto source = Codecs::Yuv420::planesOf(from);
    if (image.data.size() < source.bytes) HAIO_FAIL(InvalidInput, "yuv420 picture is smaller than its planes");

    Size to = size;
    if (to.width <= 0) to.width = std::max(1, from.width * to.height / from.height);
    if (to.height <= 0) to.height = std::max(1, from.height * to.width / from.width);
    const auto target = Codecs::Yuv420::planesOf(to);

    const auto sx = [&](int x) { return std::min(from.width - 1, x * from.width / to.width); };
    const auto sy = [&](int y) { return std::min(from.height - 1, y * from.height / to.height); };

    std::vector<uint8_t> out(target.bytes);
    const auto* luma = image.data.data();
    for (int y = 0; y < to.height; y++) {
        const auto* row = luma + static_cast<size_t>(sy(y)) * source.lumaWidth;
        auto* dst = out.data() + static_cast<size_t>(y) * target.lumaWidth;
        for (int x = 0; x < to.width; x++) dst[x] = row[sx(x)];
    }
    Codecs::Yuv420::repeatEdges(out, to, target);

    for (size_t plane = 0; plane < 2; plane++) {
        const auto* src = luma + source.luma + plane * source.chroma;
        auto* dst = out.data() + target.luma + plane * target.chroma;
        for (int y = 0; y < target.chromaHeight; y++) {
            const auto* row = src + static_cast<size_t>(sy(std::min(2 * y, to.height - 1)) / 2) * source.chromaWidth;
            for (int x = 0; x < target.chromaWidth; x++) {
                dst[static_cast<size_t>(y) * target.chromaWidth + x] = row[sx(std::min(2 * x, to.width - 1)) / 2];
            }
        }
    }
    return Image<Color::YUV420>{to.width, to.height, std::move(out)};
}

}
