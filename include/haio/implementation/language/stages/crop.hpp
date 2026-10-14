#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/language/stage.hpp"

namespace Haio::Implementation::LanguageStage {

inline constexpr Stage crop{
    .spellings = {"crop"},
    .rule = "crop",
    .takes = "crop-geometry",
    .help = "crop, as WxH+X+Y the way imagemagick writes it, or as X,Y,W,H",
};

template <> Built build<&crop>(std::string_view value, const Given& given);

}
