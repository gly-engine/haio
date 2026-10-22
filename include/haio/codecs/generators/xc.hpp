#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

namespace Haio::Codecs {

inline constexpr Stages::Option xcOptions[] = {Canvas::size};

template <> inline constexpr Draws draws<Brush::Xc>{.options = xcOptions, .takes = "colour", .aliases = {"canvas"}};

}
