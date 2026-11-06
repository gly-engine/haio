#pragma once

#include "haio_codec.hpp"

namespace Haio::Codecs {

/**
 * how hard the encoder looks for ground that is all one colour before it sends any
 * pixels. a level rather than a switch because the levels build on each other: the
 * flat ground found at one is what the next has to paint over.
 *
 *   0  every cut travels whole, which is what zcis did before any of this
 *   1  flat ground becomes a fill command and only the part that varies travels
 *
 * it is off by default because a decoder that does not know the fill command cannot
 * read what it produces, and the one in the televisions does not yet.
 */
inline constexpr Stages::Option zcisOptimization{
    .spellings = {"zcis:optimization"}, .takes = "integer",
    .help = "how hard to look for flat ground before sending pixels",
    .shape = Stages::Shape::Integer, .least = 0, .most = 1, .fallback = "0",
};

/**
 * how much lighter to make the picture, as the share of its drawing that goes away.
 *
 * it names the saving rather than the means. a television draws a run of one colour
 * and a run of a hundred for the same price, so what a picture costs to put up is
 * the number of runs in it, and that is what this counts: minification fifty asks
 * for a file that costs half the draws the plain picture would.
 *
 *   0   the picture to the bit
 *   25  a quarter of the drawing gone
 *   50  half
 *
 * the encoder reaches the number by spending what is cheap before what is dear. the
 * flat ground goes first, which removes draws without moving a single pixel, and
 * only when that falls short does it start letting a pixel that is nearly the colour
 * of the run it sits in be drawn as it. so the same number costs less colour as the
 * encoder learns to save more, and a file asked for fifty today and fifty next year
 * is the same weight on the same television.
 *
 * it is not a grid: nothing is rounded to fixed values, so a gradient does not band
 * along fixed lines, it merely holds still a little longer.
 *
 * a picture can run out of room before the number does -- a photograph of leaves has
 * no runs to join -- and then the encoder gives what it could reach rather than
 * ruining it, which is why the ceiling exists and why ninety nine is a wish and not
 * a promise.
 */
inline constexpr Stages::Option zcisMinification{
    .spellings = {"zcis:minification"}, .takes = "number",
    .help = "the share of the drawing to remove, nought to ninety nine",
    .shape = Stages::Shape::Integer, .least = 0, .most = 99, .fallback = "0",
};

inline constexpr Stages::Option zcisEncodes[] = {zcisOptimization, zcisMinification};

template <> inline constexpr Reads reads<Format::ZCIS>{.encode = zcisEncodes};

}
