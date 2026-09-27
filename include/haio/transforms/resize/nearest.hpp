#pragma once

#include "haio_transform.hpp"

#include <algorithm>
#include <stdexcept>

namespace Haio {

/**
 * nearest neighbour: one side may be left at zero and follows the other.
 *
 * it is a template over the colour because it never looks inside a pixel, so every
 * addressable colour gets it for nothing; a filter that has to mix pixels is written
 * per colour instead, beside this one.
 */
template <Color P>
    requires Addressable<P>
Image<P> resizeNearest(const Image<P>& image, Size size) {
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

}
