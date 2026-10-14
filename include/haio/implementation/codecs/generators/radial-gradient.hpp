#pragma once

#include "haio/implementation/codecs.hpp"
#include "haio/util/canvas.hpp"

namespace Haio::Implementation::Codecs {

inline constexpr Option gradientExtent{
    .spellings = {"gradient:extent"}, .takes = "name", .help = "how far from the centre it reaches",
    .shape = OptionShape::Name, .names = Util::namesOf<Generate<>::GradientExtent>(), .fallback = "circle",
    .called = "gradient extent",
};

inline constexpr Option gradientCenter{
    .spellings = {"gradient:center"}, .takes = "point", .help = "where the middle is, as x,y",
};

inline constexpr Option gradientRadii{
    .spellings = {"gradient:radii"}, .takes = "point", .help = "how far it reaches each way, as rx,ry",
};

inline constexpr Option radialOptions[] = {Util::canvasSize, gradientExtent, gradientCenter, gradientRadii};

template <> inline constexpr Draws draws<Brush::RadialGradient>{.options = radialOptions, .takes = "colour-pair"};

}
