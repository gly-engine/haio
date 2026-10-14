#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/transforms.hpp"
#include "haio/util/family.hpp"

namespace Haio::Implementation {

/** every channel turned upside down but the alpha, as imagemagick's -negate leaves it */
template <Color P>
    requires Addressable<P>
Image<P> negateImage(Image<P> image) {
    constexpr auto stride = Util::strideOf(P);
    constexpr int alpha = Util::alphaOffsetOf(P);
    for (size_t at = 0; at < image.data.size(); at++) {
        if (static_cast<int>(at % stride) != alpha) image.data[at] = static_cast<uint8_t>(255 - image.data[at]);
    }
    return image;
}

}

namespace Haio::Implementation::Transforms {

/**
 * every channel turned upside down: Transform<Operation::Negate, Color::GRAY8>{}(image), one class per colour it
 * runs on, in `library/implementation/transforms/negate/COLOUR.cpp`.
 */
template <Color P>
concept Negatable = Util::MemberOf<Transform<Operation::Negate, P>, Transform<>>
    && requires (const Transform<Operation::Negate, P>& negate, Image<P> i) {
        { negate(std::move(i)) } -> std::same_as<Result<Image<P>>>;
    };

}
