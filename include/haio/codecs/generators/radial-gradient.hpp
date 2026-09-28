#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

namespace Haio {

/** how far a radial gradient reaches, as imagemagick's gradient:extent names it */
enum class GradientExtent {
    Circle,     /**< the larger half of the picture, which is also Maximum */
    Diagonal,   /**< out to the corners */
    Ellipse,    /**< half the width one way and half the height the other */
    Maximum,
    Minimum,    /**< the smaller half */
};

namespace Codecs {

inline constexpr Stages::Option gradientExtent{
    .spellings = {"gradient:extent"}, .takes = "name", .help = "how far from the centre it reaches",
    .shape = Stages::Shape::Name, .names = Stages::namesOf<GradientExtent>(), .fallback = "circle",
    .called = "gradient extent",
};

inline constexpr Stages::Option gradientCenter{
    .spellings = {"gradient:center"}, .takes = "point", .help = "where the middle is, as x,y",
};

inline constexpr Stages::Option gradientRadii{
    .spellings = {"gradient:radii"}, .takes = "point", .help = "how far it reaches each way, as rx,ry",
};

inline constexpr Stages::Option radialOptions[] = {Canvas::size, gradientExtent, gradientCenter, gradientRadii};

template <> inline constexpr Draws draws<Brush::RadialGradient>{.options = radialOptions, .takes = "colour-pair"};

}

}
