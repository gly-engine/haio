#pragma once

#include "haio_codec.hpp"

namespace Haio::Codecs {

/** spelled the way imagemagick spells it, so the same -define works in both */
inline constexpr Stages::Option pngCompressionLevel{
    .spellings = {"png:compression-level"}, .takes = "integer", .help = "zlib's level",
    .shape = Stages::Shape::Integer, .least = 0, .most = 9, .fallback = "6",
};

inline constexpr Stages::Option pngEncodes[] = {pngCompressionLevel};

template <> inline constexpr Reads reads<Format::PNG>{.encode = pngEncodes};

}
