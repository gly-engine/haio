#pragma once

#include "haio/implementation/wip/dither.hpp"
#include "haio/implementation/language/stage.hpp"

namespace Haio::Implementation::LanguageStage {

inline constexpr Stage negate{
    .spellings = {"negate"},
    .rule = "negate",
    .help = "black becomes white and white black",
};

template <> Built build<&negate>(std::string_view value, const Given& given);

}
