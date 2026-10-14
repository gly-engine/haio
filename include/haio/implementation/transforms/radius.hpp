#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/transforms.hpp"
#include "haio/util/family.hpp"

#include <algorithm>

namespace Haio::Implementation {

/**
 * clears the alpha outside a rounded corner, so this one asks for more than the other
 * two: a colour with no alpha has no way to say "not here", and asking for square
 * corners back would be a worse answer than refusing to compile.
 */
template <Color P>
    requires Maskable<P>
Image<P> roundImageCorners(const Image<P>& image, int radius) {
    constexpr auto stride = Util::strideOf(P);
    constexpr auto alpha = static_cast<size_t>(Util::alphaOffsetOf(P));

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

namespace Haio::Implementation::Transforms {

/**
 * the corners rounded away: Transform<Operation::Radius, Color::RGBA8888>{}(image, radius), one class per colour
 * it runs on, in `library/implementation/transforms/radius/COLOUR.cpp`.
 */
template <Color P>
concept Roundable = Util::MemberOf<Transform<Operation::Radius, P>, Transform<>>
    && requires (const Transform<Operation::Radius, P>& round, Image<P> i, int r) {
        { round(std::move(i), r) } -> std::same_as<Result<Image<P>>>;
    };

}
