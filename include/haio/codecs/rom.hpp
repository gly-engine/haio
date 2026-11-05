#pragma once

#include "haio_codec.hpp"

namespace Haio::Codecs {

inline constexpr Stages::Option tic80Read{
    .spellings = {"tic80:read"},
    .takes = "sector-name",
    .help = "which chunk of a tic-80 cartridge to draw",
};

inline constexpr Stages::Option romDecodes[] = {tic80Read};

template <> inline constexpr Reads reads<Format::ROM>{.decode = romDecodes};

}
