#pragma once

#include "haio_codec.hpp"

namespace Haio::Codecs {

/**
 * which part of a cartridge to draw.
 *
 * a rom is not one picture. it is a file with several things in it and only some of
 * them are pictures at all, so the one wanted is named. the name is free text rather
 * than a list, because a cartridge names its own parts and a wad will name different
 * ones: what the names are is read out of the file, not written down here.
 *
 * spelled for the console rather than for the format, since a cartridge of one is
 * nothing like a cartridge of another, and "rom:read" would read as if it were.
 */
inline constexpr Stages::Option tic80Read{
    .spellings = {"tic80:read"},
    .takes = "sector-name",
    .help = "which chunk of a tic-80 cartridge to draw",
};

inline constexpr Stages::Option romDecodes[] = {tic80Read};

template <> inline constexpr Reads reads<Format::ROM>{.decode = romDecodes};

}
