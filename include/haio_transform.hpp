#pragma once

#include "haio_codec.hpp"

#include <algorithm>
#include <stdexcept>

/**
 * geometry on addressable pixels. the colour no longer has to be rgba8888: the only
 * thing these ever needed from it was how wide a pixel is, which PixelStride answers,
 * so a crop on rgb565 stops going through a conversion it never used.
 */
namespace Haio {

/**
 * a transformed picture keeps everything about the original except its pixels.
 *
 * building a fresh Image would drop whatever else the colour carries, which for a
 * palette is the palette: a cropped picture would come back as indices into nothing.
 */
template <Color P>
Image<P> withPixels(const Image<P>& from, int width, int height, std::vector<uint8_t> pixels) {
    Image<P> out = from;
    out.width = width;
    out.height = height;
    out.data = std::move(pixels);
    return out;
}

template <Color P>
    requires Addressable<P>
Image<P> cropImage(const Image<P>& image, Rect rect) {
    constexpr auto stride = strideOf(P);

    const int x0 = std::clamp(rect.x, 0, image.width);
    const int y0 = std::clamp(rect.y, 0, image.height);
    const int x1 = std::clamp(rect.x + rect.width, x0, image.width);
    const int y1 = std::clamp(rect.y + rect.height, y0, image.height);
    const int width = x1 - x0;
    const int height = y1 - y0;

    std::vector<uint8_t> out(static_cast<size_t>(width) * static_cast<size_t>(height) * stride);
    for (int y = 0; y < height; y++) {
        const auto src = (static_cast<size_t>(y0 + y) * static_cast<size_t>(image.width) + static_cast<size_t>(x0)) * stride;
        const auto dst = static_cast<size_t>(y) * static_cast<size_t>(width) * stride;
        std::copy_n(image.data.data() + src, static_cast<size_t>(width) * stride, out.data() + dst);
    }
    return withPixels(image, width, height, std::move(out));
}

/** nearest neighbour: one side may be left at zero and follows the other */
template <Color P>
    requires Addressable<P>
Image<P> resizeImage(const Image<P>& image, Size size) {
    constexpr auto stride = strideOf(P);

    if (size.width <= 0 && size.height <= 0) throw std::runtime_error("resize expects a positive size");

    int width = size.width;
    int height = size.height;
    if (width <= 0) width = std::max(1, image.width * height / image.height);
    if (height <= 0) height = std::max(1, image.height * width / image.width);

    std::vector<uint8_t> out(static_cast<size_t>(width) * static_cast<size_t>(height) * stride);
    for (int y = 0; y < height; y++) {
        const int sy = std::min(image.height - 1, y * image.height / height);
        for (int x = 0; x < width; x++) {
            const int sx = std::min(image.width - 1, x * image.width / width);
            const auto src = (static_cast<size_t>(sy) * static_cast<size_t>(image.width) + static_cast<size_t>(sx)) * stride;
            const auto dst = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * stride;
            std::copy_n(image.data.data() + src, stride, out.data() + dst);
        }
    }
    return withPixels(image, width, height, std::move(out));
}

/**
 * clears the alpha outside a rounded corner, so this one asks for more than the other
 * two: a colour with no alpha has no way to say "not here", and asking for square
 * corners back would be a worse answer than refusing to compile.
 */
template <Color P>
    requires Maskable<P>
Image<P> roundImageCorners(const Image<P>& image, int radius) {
    constexpr auto stride = strideOf(P);
    constexpr auto alpha = static_cast<size_t>(alphaOffsetOf(P));

    if (radius <= 0) return image;

    Image<P> out = image;
    const int r = std::min(radius, std::min(image.width, image.height) / 2);
    const int r2 = r * r;

    auto maskCorner = [&](int x, int y, int cx, int cy) {
        const int dx = x - cx;
        const int dy = y - cy;
        if (dx * dx + dy * dy > r2) {
            const auto off = (static_cast<size_t>(y) * static_cast<size_t>(out.width) + static_cast<size_t>(x)) * stride + alpha;
            out.data[off] = 0;
        }
    };

    for (int y = 0; y < r; y++) {
        for (int x = 0; x < r; x++) {
            maskCorner(x, y, r - 1, r - 1);
            maskCorner(out.width - 1 - x, y, out.width - r, r - 1);
            maskCorner(x, out.height - 1 - y, r - 1, out.height - r);
            maskCorner(out.width - 1 - x, out.height - 1 - y, out.width - r, out.height - r);
        }
    }

    return out;
}

}
