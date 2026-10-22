#pragma once

#include "haio_codec.hpp"
#include "haio/internal/codecs/canvas.hpp"

namespace Haio::Codecs {

/** without -size the picture is the text and nothing around it */
inline constexpr Stages::Option textSize{
    .spellings = {"size"}, .takes = "size",
    .help = "the size it is drawn at, the text centred in it; just the text when nobody says",
    .shape = Stages::Shape::Size,
};

inline constexpr Stages::Option fontSize{
    .spellings = {"font-size", "pointsize"}, .takes = "integer", .help = "how tall the text is, in pixels",
    .shape = Stages::Shape::Integer, .least = 1, .most = 4096, .fallback = "12",
};

/**
 * a path to a font file, or a family looked up in the system's font directories.
 * noto sans is built in, and drawn from there when the system has none of its own.
 */
inline constexpr Stages::Option fontName{
    .spellings = {"font-name", "font"}, .takes = "font", .help = "a font family or a path to a font file",
    .fallback = "Noto Sans",
};

/**
 * "bold italic", "ItalicBold", "bold-italic": the words in any order and any case.
 * a style the font has no face for is made up from the one it has.
 */
inline constexpr Stages::Option fontStyle{
    .spellings = {"font-style"}, .takes = "style", .help = "regular, bold, italic, or bold and italic in any order",
    .fallback = "regular",
};

inline constexpr Stages::Option textOptions[] = {textSize, fontSize, fontName, fontStyle};

/** text:"ola mundo", the words after the colon being what it says */
template <> inline constexpr Draws draws<Brush::Text>{.options = textOptions, .takes = "text", .aliases = {"label"}, .composite4 = true};

}
