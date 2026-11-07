#pragma once

#include "haio_transform.hpp"
#include "haio/stage.hpp"
#include "haio/transforms/blur/box.hpp"

namespace Haio {

namespace Transforms {

/**
 * one per colour it runs on, in `library/backend/transforms/blur/COLOUR.cpp`.
 *
 * the radius is the one of the box passes, in pixels; the sigma imagemagick is asked
 * for is turned into it when the command is read.
 */
template <Color P> Result<Image<P>> Blur(Image<P> image, int radius) = delete;

template <Color P>
concept Blurrable = requires (Image<P> i, int r) { { Blur<P>(std::move(i), r) } -> std::same_as<Result<Image<P>>>; };

}

namespace Stages {

/**
 * -blur RADIUSxSIGMA, as imagemagick reads it. the sigma is what decides how soft the
 * picture gets; imagemagick's radius only cuts its kernel short, and a box blur has
 * no kernel to cut, so it is read and left alone. a bare number is a radius with the
 * sigma imagemagick gives it then, which is one.
 */
inline constexpr Stage blur{
    .spellings = {"blur"},
    .rule = "blur",
    .takes = "blur-geometry",
    .help = "blur, as imagemagick: 0xSIGMA, the sigma in pixels",
};

template <> Built build<&blur>(std::string_view value, const Given& given);

}

}
