#pragma once

#include "haio/stage.hpp"

namespace Haio::Stages {

/** read so that a line written for imagemagick parses, and refused when it runs */
inline constexpr Stage fx{
    .spellings = {"fx"},
    .rule = "fx",
    .takes = "expression",
    .help = "an expression, read and not yet run",
};

template <> Built build<&fx>(std::string_view value, const Given& given);

}
