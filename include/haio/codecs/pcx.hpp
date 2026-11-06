#pragma once

#include "haio_codec.hpp"

namespace Haio {

/**
 * the two ways a pcx holds its rows.
 *
 * the run length is the one everything reads, and it is what every writer since
 * 1985 has emitted -- pillow refuses a file without it. plain rows exist in the
 * format and are legal, and they are worth having for the one case the run length
 * loses at: noise, where a byte above 0xbf costs two and nothing repeats, so the
 * "compressed" file comes out bigger than the pixels it holds.
 */
enum class PcxCompression {
    Rle,
    None,
};

namespace Codecs {

inline constexpr Stages::Option pcxCompression{
    .spellings = {"pcx:compression"}, .takes = "name",
    .help = "run length rows, or rows as they are",
    .shape = Stages::Shape::Name, .names = Stages::namesOf<PcxCompression>(),
    .fallback = "rle", .called = "pcx compression",
};

inline constexpr Stages::Option pcxEncodes[] = {pcxCompression};

template <> inline constexpr Reads reads<Format::PCX>{.encode = pcxEncodes};

}

}
