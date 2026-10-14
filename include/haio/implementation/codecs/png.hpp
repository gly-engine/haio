#pragma once

#include "haio/implementation/codecs.hpp"

namespace Haio::Implementation::Codecs {

/** spelled the way imagemagick spells it, so the same -define works in both */
inline constexpr Option pngCompressionLevel{
    .spellings = {"png:compression-level"}, .takes = "integer", .help = "zlib's level",
    .shape = OptionShape::Integer, .least = 0, .most = 9, .fallback = "6",
};

inline constexpr Option pngEncodes[] = {pngCompressionLevel};

template <> inline constexpr Reads reads<Format::PNG>{.encode = pngEncodes};

}
