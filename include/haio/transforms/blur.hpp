#pragma once

#include "haio_transform.hpp"
#include "haio/transforms/blur/box.hpp"

namespace Haio::Transforms {

/**
 * one per colour it runs on, in `library/backend/transforms/blur/COLOUR.cpp`.
 *
 * the radius is in pixels, and is what imagemagick calls the sigma: three box passes
 * of radius r land close to a gaussian of sigma r, so -blur 0x8 is a radius of 8.
 */
template <Color P> Result<Image<P>> Blur(Image<P> image, int radius) = delete;

template <Color P>
concept Blurrable = requires (Image<P> i, int r) { { Blur<P>(std::move(i), r) } -> std::same_as<Result<Image<P>>>; };

}
