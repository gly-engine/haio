#pragma once

#include "haio/implementation/codecs.hpp"

namespace Haio::Implementation::Codecs {

inline constexpr Option jpegQuality{
    .spellings = {"quality"}, .takes = "integer", .help = "how much detail to keep",
    .shape = OptionShape::Integer, .least = 1, .most = 100, .fallback = "92",
};

inline constexpr Option jpegEncodes[] = {jpegQuality};

template <> inline constexpr Reads reads<Format::JPEG>{.encode = jpegEncodes};

}
