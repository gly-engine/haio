#pragma once

#include "haio/implementation/codecs.hpp"
#include "haio/util/canvas.hpp"

namespace Haio::Implementation::Codecs {

inline constexpr Option nullOptions[] = {Util::canvasSize};

/** nothing at all, which is a transparent canvas of whatever -size says */
template <> inline constexpr Draws draws<Brush::Null>{.options = nullOptions};

}
