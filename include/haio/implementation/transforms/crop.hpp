#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/transforms.hpp"
#include "haio/util/family.hpp"

#include <algorithm>

namespace Haio::Implementation {

/** the rectangle, clamped to the picture; addressable is all it needs to know */
template <Color P>
    requires Addressable<P>
Image<P> cropImage(const Image<P>& image, Rect rect) {
    constexpr auto stride = Util::strideOf(P);

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

}

namespace Haio::Implementation::Transforms {

/**
 * the rectangle out of a picture: Transform<Operation::Crop, Color::RGBA8888>{}(image, rect), one class per colour
 * it runs on, in `library/implementation/transforms/crop/COLOUR.cpp`.
 */
template <Color P>
concept Croppable = Util::MemberOf<Transform<Operation::Crop, P>, Transform<>>
    && requires (const Transform<Operation::Crop, P>& crop, Image<P> i, Rect r) {
        { crop(std::move(i), r) } -> std::same_as<Result<Image<P>>>;
    };

}
