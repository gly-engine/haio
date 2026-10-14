#pragma once

#include "haio/implementation/codecs.hpp"
#include "haio/util/canvas.hpp"

namespace Haio::Implementation::Codecs {

/** hald:8 is the level; there is nothing else to say about an identity */
template <> inline constexpr Draws draws<Brush::Hald>{.takes = "integer"};

}
