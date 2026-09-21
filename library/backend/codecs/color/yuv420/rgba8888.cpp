#include <haio_convert.hpp>

namespace Haio::Codecs {

/**
 * jfif colour, which is full range bt.601 and not the studio range video uses. a
 * decoder written against the video coefficients produces a picture that is visibly
 * washed out rather than obviously wrong, which is the sort of mistake that ships.
 *
 * the two chroma planes are half size in both directions, so each of their samples
 * covers a two by two block of luma.
 */
template <>
Result<void> Move<Color::YUV420, Color::RGBA8888>(Bytes src, std::span<uint8_t> dst, Size size) {
    if (size.width % 2 != 0 || size.height % 2 != 0) {
        HAIO_FAIL(InvalidInput, "yuv420 needs even width and height");
    }

    const auto width = static_cast<size_t>(size.width);
    const auto height = static_cast<size_t>(size.height);
    const auto pixels = width * height;
    const auto half = width / 2;
    const auto chroma = half * (height / 2);

    if (src.size() < pixels + chroma * 2 || dst.size() < pixels * 4) {
        HAIO_FAIL(InvalidInput, "yuv420 to rgba8888 got a buffer that is too small");
    }

    const auto* luma = src.data();
    const auto* blue = luma + pixels;
    const auto* red = blue + chroma;

    const auto clamp = [](int value) {
        return static_cast<uint8_t>(value < 0 ? 0 : value > 255 ? 255 : value);
    };

    for (size_t y = 0; y < height; y++) {
        const auto chromaRow = (y / 2) * half;
        for (size_t x = 0; x < width; x++) {
            const int Y = luma[y * width + x];
            const int Cb = blue[chromaRow + x / 2] - 128;
            const int Cr = red[chromaRow + x / 2] - 128;

            // the coefficients are the jfif ones, scaled by 1024 to stay in integers
            const auto at = (y * width + x) * 4;
            dst[at + 0] = clamp(Y + ((1436 * Cr) >> 10));
            dst[at + 1] = clamp(Y - ((352 * Cb + 731 * Cr) >> 10));
            dst[at + 2] = clamp(Y + ((1815 * Cb) >> 10));
            dst[at + 3] = 0xFF;
        }
    }
    return {};
}

template <>
Result<Image<Color::RGBA8888>> Convert<Color::YUV420, Color::RGBA8888>(Image<Color::YUV420> src) {
    return convertVia<Color::YUV420, Color::RGBA8888>(std::move(src));
}

}
