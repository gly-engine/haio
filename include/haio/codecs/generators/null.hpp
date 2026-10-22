#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

namespace Haio::Codecs {

inline constexpr Stages::Option nullOptions[] = {Canvas::size};

/** nothing at all, which is a transparent canvas of whatever -size says */
template <> inline constexpr Draws draws<Brush::Null>{.options = nullOptions};

}
