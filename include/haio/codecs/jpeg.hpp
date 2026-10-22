#pragma once

#include "haio_codec.hpp"

namespace Haio::Codecs {

inline constexpr Stages::Option jpegQuality{
    .spellings = {"quality"}, .takes = "integer", .help = "how much detail to keep",
    .shape = Stages::Shape::Integer, .least = 1, .most = 100, .fallback = "92",
};

inline constexpr Stages::Option jpegEncodes[] = {jpegQuality};

template <> inline constexpr Reads reads<Format::JPEG>{.encode = jpegEncodes};

}
