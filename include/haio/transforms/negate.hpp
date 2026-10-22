#pragma once

#include "haio_transform.hpp"
#include "haio/stage.hpp"

namespace Haio {

/** every channel turned upside down but the alpha, as imagemagick's -negate leaves it */
template <Color P>
    requires Addressable<P>
Image<P> negateImage(Image<P> image) {
    constexpr auto stride = strideOf(P);
    constexpr int alpha = alphaOffsetOf(P);
    for (size_t at = 0; at < image.data.size(); at++) {
        if (static_cast<int>(at % stride) != alpha) image.data[at] = static_cast<uint8_t>(255 - image.data[at]);
    }
    return image;
}

namespace Transforms {

/** one per colour it runs on, in `library/backend/transforms/negate/COLOUR.cpp` */
template <Color P> Result<Image<P>> Negate(Image<P> image) = delete;

template <Color P>
concept Negatable = requires (Image<P> i) { { Negate<P>(std::move(i)) } -> std::same_as<Result<Image<P>>>; };

}

namespace Stages {

inline constexpr Stage negate{
    .spellings = {"negate"},
    .rule = "negate",
    .help = "black becomes white and white black",
};

template <> Built build<&negate>(std::string_view value, const Given& given);

}

}
