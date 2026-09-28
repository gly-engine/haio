#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

namespace Haio {

/**
 * the barcodes code: paints, named on the command line the way people write them:
 * "Code 128", "CODE-128" and "code128" are all one, since case, spaces and dashes
 * do not matter to a name.
 */
enum class Barcode {
    Code128,
    Code39,
    Codabar,
    Ean13,
    Ean8,
    UpcA,
};

namespace Codecs {

inline constexpr Stages::Option barcodeFormat{
    .spellings = {"format"}, .takes = "name", .help = "which barcode",
    .shape = Stages::Shape::Name, .names = Stages::namesOf<Barcode>(), .fallback = "code128",
    .called = "barcode format",
};

inline constexpr Stages::Option codeOptions[] = {Canvas::moduleSize, barcodeFormat, Canvas::fill, Canvas::background};

/** code:12345, the words after the colon being what the bars say */
template <> inline constexpr Draws draws<Brush::Code>{.options = codeOptions, .takes = "text"};

}

}
