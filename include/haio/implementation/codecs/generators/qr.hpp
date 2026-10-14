#pragma once

#include "haio/implementation/codecs.hpp"
#include "haio/util/canvas.hpp"

namespace Haio::Implementation::Codecs {

inline constexpr Option matrixFormat{
    .spellings = {"format"}, .takes = "name", .help = "which two dimensional code",
    .shape = OptionShape::Name, .names = Util::namesOf<Generate<>::Matrix>(), .fallback = "qrcode",
    .called = "matrix format",
};

/** zint's own choice when nobody says is the lowest level the data fits at */
inline constexpr Option qrLevel{
    .spellings = {"qr:level"}, .takes = "name", .help = "how much of it can be lost, qr code only",
    .shape = OptionShape::Name, .names = Util::namesOf<Generate<>::QrLevel>(), .called = "qr error correction level",
};

/** room left in the middle, for a logo: level h unless qr:level says, and never past what still reads */
inline constexpr Option qrHole{
    .spellings = {"hole"}, .takes = "size", .help = "a hole in the middle, in pixels, for a logo; qr code only",
    .shape = OptionShape::Size,
};

/**
 * the quiet zone, in modules, in white. the standards ask for four
 * around a qr code and one around a data matrix; one is what most readers need, and
 * zero is the code alone.
 */
inline constexpr Option qrMargin{
    .spellings = {"margin"}, .takes = "integer", .help = "the quiet zone around it, in modules",
    .shape = OptionShape::Integer, .least = 0, .most = 64, .fallback = "1",
};

inline constexpr Option qrOptions[] = {Util::canvasModuleSize, matrixFormat, qrLevel, qrHole, qrMargin};

/** qr:"http://pudim.com.br", the words after the colon being what the code says */
template <> inline constexpr Draws draws<Brush::Qr>{.options = qrOptions, .takes = "text", .composite4 = true};

}
