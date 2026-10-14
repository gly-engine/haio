#pragma once

#include "haio/implementation/language/stage.hpp"
#include "haio/implementation/wip/palette.hpp"

/**
 * the palette itself -- the named ones, the dithers, the limits -- is in
 * include/haio/implementation/wip/palette.hpp and library/implementation/wip/palette/,
 * which is older than this and is not
 * only a transform. what lives here is how the command line asks for it.
 */
namespace Haio::Implementation::LanguageStage {

inline constexpr Option paletteOptions[] = {
    {.spellings = {"filter"}, .takes = "name", .required = true,
     .help = "how a colour that is not in the palette becomes one that is",
     .shape = OptionShape::Name, .names = Util::namesOf<WIP::Dither>(), .called = "dither method"},
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
