#pragma once

#include "haio/implementation/codecs.hpp"
#include "haio/util/canvas.hpp"

namespace Haio::Implementation::Codecs {

/** degrees clockwise from north, which is what a direction is a name for; south is the default */
inline constexpr Option gradientAngle{
    .spellings = {"gradient:angle"}, .takes = "number", .help = "which way it runs, in degrees clockwise from north",
    .fallback = "180",
};

inline constexpr Option gradientDirection{
    .spellings = {"gradient:direction"}, .takes = "name", .help = "which way it runs, as a compass point",
    .shape = OptionShape::Name, .names = Util::namesOf<Generate<>::GradientDirection>(), .fallback = "south",
    .called = "gradient direction",
};

inline constexpr Option gradientOptions[] = {Util::canvasSize, gradientAngle, gradientDirection};

template <> inline constexpr Draws draws<Brush::Gradient>{.options = gradientOptions, .takes = "colour-pair"};

}
