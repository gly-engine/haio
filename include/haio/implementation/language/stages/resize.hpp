#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/transforms.hpp"
#include "haio/implementation/language/stage.hpp"

namespace Haio::Implementation::LanguageStage {

inline constexpr Option resizeOptions[] = {
    {.spellings = {"filter"}, .takes = "name", .help = "how the pixels are picked, as imagemagick names it",
     .shape = OptionShape::Name, .names = Util::namesOf<Transforms::Transform<>::ResizeFilter>(), .fallback = "point", .called = "image filter"},
};

inline constexpr Stage resize{
    .spellings = {"resize"},
    .rule = "resize",
    .takes = "size",
    .options = resizeOptions,
    .help = "resize, to WxH or to a share such as 30% or 30pct",
};

template <> Built build<&resize>(std::string_view value, const Given& given);

}
