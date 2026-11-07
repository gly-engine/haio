#pragma once

#include "haio_transform.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace Haio {

namespace Detail {

/**
 * one pass of the box blur over a run of pixels laid out in rows, along the rows or
 * down the columns. the rows may be wider than the pixels in them, which is how a
 * plane grown to an even side is stored.
 */
template <size_t Stride>
void boxPass(uint8_t* data, int width, int height, size_t rowBytes, int radius, bool across) {
    const int length = across ? width : height;
    const int lines = across ? height : width;
    const int window = radius * 2 + 1;
    const auto at = [&](int line, int i) {
        const int x = across ? i : line;
        const int y = across ? line : i;
        return static_cast<size_t>(y) * rowBytes + static_cast<size_t>(x) * Stride;
    };
    const auto clamped = [&](int i) { return static_cast<size_t>(std::clamp(i, 0, length - 1)) * Stride; };

    std::vector<uint8_t> row(static_cast<size_t>(length) * Stride);
    for (int line = 0; line < lines; line++) {
        for (int i = 0; i < length; i++) std::copy_n(data + at(line, i), Stride, row.data() + clamped(i));

        std::array<int, Stride> sum{};
        for (int k = -radius; k <= radius; k++) {
            for (size_t c = 0; c < Stride; c++) sum[c] += row[clamped(k) + c];
        }
        for (int i = 0; i < length; i++) {
            // rounded rather than cut: six passes of cutting leave the picture darker
            for (size_t c = 0; c < Stride; c++) data[at(line, i) + c] = static_cast<uint8_t>((sum[c] + window / 2) / window);
            const auto in = clamped(i + radius + 1);
            const auto out = clamped(i - radius);
            for (size_t c = 0; c < Stride; c++) sum[c] += row[in + c] - row[out + c];
        }
    }
}

}

/**
 * three passes of a box blur each way, which comes close enough to a gaussian of the
 * same radius and costs the same whatever the radius. past the border the edge pixel
 * is repeated, so the edge neither darkens nor fades into transparency.
 *
 * it averages every byte of a pixel on its own, so it is only for pixels that are a
 * byte a channel: on a packed or an indexed colour the average would mean nothing.
 */
template <size_t Stride>
void boxBlur(uint8_t* data, int width, int height, size_t rowBytes, int radius) {
    if (radius <= 0 || width <= 0 || height <= 0) return;
    for (int pass = 0; pass < 3; pass++) {
        Detail::boxPass<Stride>(data, width, height, rowBytes, radius, true);
        Detail::boxPass<Stride>(data, width, height, rowBytes, radius, false);
    }
}

/** the same blur over a whole picture whose pixels sit side by side */
template <Color P>
    requires Addressable<P>
Image<P> boxBlur(Image<P> image, int radius) {
    constexpr auto stride = strideOf(P);
    boxBlur<stride>(image.data.data(), image.width, image.height, static_cast<size_t>(image.width) * stride, radius);
    return image;
}

}
