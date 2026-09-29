#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

namespace Haio {

/** which way a linear gradient runs, named the way imagemagick's gradient:direction is */
enum class GradientDirection {
    North,
    NorthEast,
    East,
    SouthEast,
    South,
    SouthWest,
    West,
    NorthWest,
};

namespace Codecs {

/** degrees clockwise from north, which is what a direction is a name for; south is the default */
inline constexpr Stages::Option gradientAngle{
    .spellings = {"gradient:angle"}, .takes = "number", .help = "which way it runs, in degrees clockwise from north",
    .fallback = "180",
};

inline constexpr Stages::Option gradientDirection{
    .spellings = {"gradient:direction"}, .takes = "name", .help = "which way it runs, as a compass point",
    .shape = Stages::Shape::Name, .names = Stages::namesOf<GradientDirection>(), .fallback = "south",
    .called = "gradient direction",
};



inline constexpr Stages::Option gradientOptions[] = {Canvas::size, gradientAngle, gradientDirection};

template <> inline constexpr Draws draws<Brush::Gradient>{.options = gradientOptions, .takes = "colour-pair"};

}

}
