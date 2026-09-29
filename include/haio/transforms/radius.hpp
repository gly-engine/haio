#pragma once

#include "haio_transform.hpp"
#include "haio/stage.hpp"

#include <algorithm>

namespace Haio {

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

namespace Transforms {

/** one per colour it runs on, in `library/backend/transforms/radius/COLOUR.cpp` */
template <Color P> Result<Image<P>> Radius(Image<P> image, int radius) = delete;

template <Color P>
concept Roundable = requires (Image<P> i, int r) { { Radius<P>(std::move(i), r) } -> std::same_as<Result<Image<P>>>; };

}

namespace Stages {

inline constexpr Stage radius{
    .spellings = {"radius"},
    .rule = "radius",
    .takes = "integer",
    .help = "round the corners away",
};

template <> Built build<&radius>(std::string_view value, const Given& given);

}

}
