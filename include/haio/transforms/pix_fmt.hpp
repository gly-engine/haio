#pragma once

#include "haio/stage.hpp"

namespace Haio::Stages {

/**
 * the picture on top moved into another colour, where it stays until a transform
 * needs rgba8888. written right before the output it is also the colour the output
 * keeps, which is all ffmpeg's -pix_fmt ever meant.
 */
inline constexpr Stage pixFmt{
    .spellings = {"pix_fmt", "pix_format"},
    .rule = "pix_fmt",
    .takes = "colour-name",
    .help = "the colour the picture is kept in, as ffmpeg names it",
};

template <> Built build<&pixFmt>(std::string_view value, const Given& given);

}
