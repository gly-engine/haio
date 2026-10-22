#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

namespace Haio::Codecs {

/** hald:8 is the level; there is nothing else to say about an identity */
template <> inline constexpr Draws draws<Brush::Hald>{.takes = "integer"};

}
