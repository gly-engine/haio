#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

namespace Haio {

/** the two dimensional codes qr: paints; qr code unless -format says data matrix */
enum class Matrix {
    QrCode,
    DataMatrix,
};

/** how much of a qr code can be lost and still read, from about 7% to about 30% */
enum class QrLevel {
    L,
    M,
    Q,
    H,
};

namespace Codecs {

inline constexpr Stages::Option matrixFormat{
    .spellings = {"format"}, .takes = "name", .help = "which two dimensional code",
    .shape = Stages::Shape::Name, .names = Stages::namesOf<Matrix>(), .fallback = "qrcode",
    .called = "matrix format",
};

/** zint's own choice when nobody says is the lowest level the data fits at */
inline constexpr Stages::Option qrLevel{
    .spellings = {"qr:level"}, .takes = "name", .help = "how much of it can be lost, qr code only",
    .shape = Stages::Shape::Name, .names = Stages::namesOf<QrLevel>(), .called = "qr error correction level",
};

/** room left in the middle, for a logo: level h unless qr:level says, and never past what still reads */
inline constexpr Stages::Option qrHole{
    .spellings = {"hole"}, .takes = "size", .help = "a hole in the middle, in pixels, for a logo; qr code only",
    .shape = Stages::Shape::Size,
};

/**
 * the quiet zone, in modules, in the background colour. the standards ask for four
 * around a qr code and one around a data matrix; one is what most readers need, and
 * zero is the code alone.
 */
inline constexpr Stages::Option qrMargin{
    .spellings = {"margin"}, .takes = "integer", .help = "the quiet zone around it, in modules",
    .shape = Stages::Shape::Integer, .least = 0, .most = 64, .fallback = "1",
};

inline constexpr Stages::Option qrOptions[] = {Canvas::moduleSize, matrixFormat, Canvas::fill, Canvas::background,
                                                qrLevel, qrHole, qrMargin};

/** qr:"http://pudim.com.br", the words after the colon being what the code says */
template <> inline constexpr Draws draws<Brush::Qr>{.options = qrOptions, .takes = "text"};

}

}
