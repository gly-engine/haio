#pragma once

#include "haio_transform.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace Haio {

namespace Detail {

/** one pass of the box blur, along the rows or down the columns */
template <Color P>
void boxPass(Image<P>& image, int radius, bool across) {
    constexpr auto stride = strideOf(P);
    const int length = across ? image.width : image.height;
    const int lines = across ? image.height : image.width;
    const int window = radius * 2 + 1;
    const auto at = [&](int line, int i) {
        const int x = across ? i : line;
        const int y = across ? line : i;
        return (static_cast<size_t>(y) * image.width + x) * stride;
    };
    const auto clamped = [&](int i) { return static_cast<size_t>(std::clamp(i, 0, length - 1)) * stride; };

    std::vector<uint8_t> row(static_cast<size_t>(length) * stride);
    for (int line = 0; line < lines; line++) {
        for (int i = 0; i < length; i++) std::copy_n(image.data.data() + at(line, i), stride, row.data() + clamped(i));

        std::array<int, stride> sum{};
        for (int k = -radius; k <= radius; k++) {
            for (size_t c = 0; c < stride; c++) sum[c] += row[clamped(k) + c];
        }
        for (int i = 0; i < length; i++) {
            for (size_t c = 0; c < stride; c++) image.data[at(line, i) + c] = static_cast<uint8_t>(sum[c] / window);
            const auto in = clamped(i + radius + 1);
            const auto out = clamped(i - radius);
            for (size_t c = 0; c < stride; c++) sum[c] += row[in + c] - row[out + c];
        }
    }
}

}

/**
 * three passes of a box blur each way, which comes close enough to a gaussian of the
 * same radius and costs the same whatever the radius. past the border the edge pixel
 * is repeated, so the edge neither darkens nor fades into transparency.
 *
 * it averages every byte of a pixel on its own, so it is only for colours that are a
 * byte a channel: on a packed or an indexed colour the average would mean nothing.
 */
template <Color P>
    requires Addressable<P>
Image<P> boxBlur(Image<P> image, int radius) {
    if (radius <= 0 || image.width <= 0 || image.height <= 0) return image;
    for (int pass = 0; pass < 3; pass++) {
        Detail::boxPass(image, radius, true);
        Detail::boxPass(image, radius, false);
    }
    return image;
}

}
