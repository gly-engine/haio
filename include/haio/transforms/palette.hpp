#pragma once

#include "haio/stage.hpp"
#include "haio_palette.hpp"

/**
 * the palette itself -- the named ones, the dithers, the limits -- is in
 * haio_palette.hpp and library/backend/palette/, which is older than this and is not
 * only a transform. what lives here is how the command line asks for it.
 */
namespace Haio::Stages {

inline constexpr Option paletteOptions[] = {
    {.spellings = {"filter"}, .takes = "name", .required = true,
     .help = "how a colour that is not in the palette becomes one that is",
     .shape = Shape::Name, .names = namesOf<Dither>(), .called = "dither method"},
    {.spellings = {"limit"}, .takes = "limit-spec",
     .help = "keep only some of the palette's colours, as sort:N or spread:N"},
};

inline constexpr Stage palette{
    .spellings = {"palette", "palete"},
    .rule = "palette",
    .takes = "palette-spec",
    .options = paletteOptions,
    .help = "fit the picture into a named palette",
};

template <> Built build<&palette>(std::string_view value, const Given& given);

}
