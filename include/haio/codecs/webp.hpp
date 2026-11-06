#pragma once

#include "haio_codec.hpp"

namespace Haio {

/** the two bitstreams a webp can hold: vp8 and vp8l, which are different codecs */
enum class WebpCompression {
    Lossy,
    Lossless,
};

namespace Codecs {

/** vp8 only; a lossless webp keeps every pixel whatever this says */
inline constexpr Stages::Option webpQuality{
    .spellings = {"quality"}, .takes = "integer", .help = "how much detail to keep",
    .shape = Stages::Shape::Integer, .least = 0, .most = 100, .fallback = "75",
};

inline constexpr Stages::Option webpCompression{
    .spellings = {"webp:compression"}, .takes = "name", .help = "vp8 or vp8l",
    .shape = Stages::Shape::Name, .names = Stages::namesOf<WebpCompression>(),
    .fallback = "lossy", .called = "webp compression",
};

inline constexpr Stages::Option webpMethod{
    .spellings = {"webp:method"}, .takes = "integer", .help = "how hard to try, fastest to smallest",
    .shape = Stages::Shape::Integer, .least = 0, .most = 6, .fallback = "4",
};

inline constexpr Stages::Option webpEncodes[] = {webpQuality, webpCompression, webpMethod};

template <> inline constexpr Reads reads<Format::WEBP>{.encode = webpEncodes};

}

}
