#pragma once

#include "haio_transform.hpp"
#include "haio/stage.hpp"

#include <algorithm>

namespace Haio {

/** the rectangle, clamped to the picture; addressable is all it needs to know */
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

namespace Transforms {

/** one per colour it runs on, in `library/backend/transforms/crop/COLOUR.cpp` */
template <Color P> Result<Image<P>> Crop(Image<P> image, Rect rect) = delete;

template <Color P>
concept Croppable = requires (Image<P> i, Rect r) { { Crop<P>(std::move(i), r) } -> std::same_as<Result<Image<P>>>; };

}

namespace Stages {

inline constexpr Stage crop{
    .spellings = {"crop"},
    .rule = "crop",
    .takes = "crop-geometry",
    .help = "crop, as WxH+X+Y the way imagemagick writes it, or as X,Y,W,H",
};

template <> Built build<&crop>(std::string_view value, const Given& given);

}

}
