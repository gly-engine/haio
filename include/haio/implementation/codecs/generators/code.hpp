#pragma once

#include "haio/implementation/codecs.hpp"
#include "haio/util/canvas.hpp"

namespace Haio::Implementation::Codecs {

inline constexpr Option barcodeFormat{
    .spellings = {"format"}, .takes = "name", .help = "which barcode",
    .shape = OptionShape::Name, .names = Util::namesOf<Generate<>::Barcode>(), .fallback = "code128",
    .called = "barcode format",
};

inline constexpr Option codeOptions[] = {Util::canvasModuleSize, barcodeFormat};

/** code:12345, the words after the colon being what the bars say */
template <> inline constexpr Draws draws<Brush::Code>{.options = codeOptions, .takes = "text"};

}
