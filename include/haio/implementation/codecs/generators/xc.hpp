#pragma once

#include "haio/implementation/codecs.hpp"
#include "haio/util/canvas.hpp"

namespace Haio::Implementation::Codecs {

inline constexpr Option xcOptions[] = {Util::canvasSize};

template <> inline constexpr Draws draws<Brush::Xc>{.options = xcOptions, .takes = "colour", .aliases = {"canvas"}};

}
