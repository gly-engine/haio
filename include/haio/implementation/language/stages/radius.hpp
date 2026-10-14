#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/language/stage.hpp"

namespace Haio::Implementation::LanguageStage {

inline constexpr Stage radius{
    .spellings = {"radius"},
    .rule = "radius",
    .takes = "integer",
    .help = "round the corners away",
};

template <> Built build<&radius>(std::string_view value, const Given& given);

}
