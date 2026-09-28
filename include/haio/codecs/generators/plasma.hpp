#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

#include <limits>

namespace Haio::Codecs {

/**
 * the same seed draws the same plasma, the way imagemagick's -seed makes it; without
 * one every picture is new. the noise is haio's own, so a seed does not give back
 * imagemagick's picture, only the same one haio gave last time.
 */
inline constexpr Stages::Option plasmaSeed{
    .spellings = {"seed"}, .takes = "integer", .help = "the seed the noise is drawn from",
    .shape = Stages::Shape::Integer, .least = 0, .most = std::numeric_limits<int>::max(),
};

inline constexpr Stages::Option plasmaOptions[] = {Canvas::size, plasmaSeed};

/** fractal: is plasma: under another name, as it is for imagemagick */
template <> inline constexpr Draws draws<Brush::Plasma>{.options = plasmaOptions, .takes = "colour-pair | \"fractal\"", .aliases = {"fractal"}};

}
