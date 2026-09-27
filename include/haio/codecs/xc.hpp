#pragma once

#include "haio_codec.hpp"

namespace Haio::Codecs {

/** -size is optional, the way imagemagick has it: without one the canvas is 1x1 */
inline constexpr Stages::Option xcSize{
    .spellings = {"size"}, .takes = "size", .help = "the size it is drawn at",
    .shape = Stages::Shape::Size, .fallback = "1x1",
};

inline constexpr Stages::Option xcDecodes[] = {xcSize};

template <> inline constexpr Reads reads<Format::XC>{.decode = xcDecodes, .draws = true};

}
