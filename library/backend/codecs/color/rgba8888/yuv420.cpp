#include <haio_convert.hpp>

#include <utility>

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

    const auto width = static_cast<size_t>(size.width);
    const auto height = static_cast<size_t>(size.height);
    const auto pixels = width * height;
    const auto half = width / 2;
    const auto chroma = half * (height / 2);

    if (src.size() < pixels * 4 || dst.size() < pixels + chroma * 2) {
        HAIO_FAIL(InvalidInput, "rgba8888 to yuv420 got a buffer that is too small");
    }

    auto* luma = dst.data();
    auto* blue = luma + pixels;
    auto* red = blue + chroma;

    const auto clamp = [](int value) {
        return static_cast<uint8_t>(value < 0 ? 0 : value > 255 ? 255 : value);
    };

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            const auto at = (y * width + x) * 4;
            const int r = src[at + 0];
            const int g = src[at + 1];
            const int b = src[at + 2];
            luma[y * width + x] = clamp((306 * r + 601 * g + 117 * b) >> 10);
        }
    }

    for (size_t y = 0; y < height; y += 2) {
        for (size_t x = 0; x < width; x += 2) {
            int r = 0;
            int g = 0;
            int b = 0;
            for (const auto corner : {std::pair{y, x}, std::pair{y, x + 1},
                                      std::pair{y + 1, x}, std::pair{y + 1, x + 1}}) {
                const auto at = (corner.first * width + corner.second) * 4;
                r += src[at + 0];
                g += src[at + 1];
                b += src[at + 2];
            }
            r /= 4;
            g /= 4;
            b /= 4;

            const auto at = (y / 2) * half + (x / 2);
            blue[at] = clamp(128 + ((-173 * r - 339 * g + 512 * b) >> 10));
            red[at] = clamp(128 + ((512 * r - 429 * g - 83 * b) >> 10));
        }
    }
    return {};
}

template <>
Result<Image<Color::YUV420>> Convert<Color::RGBA8888, Color::YUV420>(Image<Color::RGBA8888> src) {
    return convertVia<Color::RGBA8888, Color::YUV420>(std::move(src));
}

}
