#pragma once

#include "haio_codec.hpp"

namespace Haio {

/** the two ways the layers of an rgb picture can share its rows */
enum class ZcisLazy {
    Whole,
    Half,
};

namespace Codecs {

/**
 * which commands the file is allowed to carry.
 *
 * it is a level rather than a set of switches because the levels build on each
 * other: the flat ground found at one is what the mask at two lets show through, so
 * there is no asking for the second without the first.
 *
 *   0  every cut travels whole, which is what zcis did before any of this
 *   1  flat ground becomes a fill command and only the part that varies travels
 *   2  a mask on top of it, so that a pixel the ground already shows is not drawn
 *
 * it is not a measure of how hard the encoder tries -- that is minification -- but
 * of what the reader on the other end has to understand. it is off by default
 * because a decoder that does not know a command cannot read what it produces.
 *
 * the two pull opposite ways and that is on purpose. one spends draws to save
 * bytes: the ground stops travelling, and the few draws it cost are gone with it.
 * two spends bytes to save draws: the mask is a member of its own and buys nothing
 * but calls to the television that stop being made.
 */
inline constexpr Stages::Option zcisOptimization{
    .spellings = {"zcis:optimization"}, .takes = "integer",
    .help = "which commands the file may carry: ground, and the mask over it",
    .shape = Stages::Shape::Integer, .least = 0, .most = 2, .fallback = "0",
};

/**
 * how much of the picture the first of the two layers bothers to put up.
 *
 * a picture of rgb travels in two layers, and how they share the rows is a choice.
 *
 *   whole  the first layer holds the odd rows and is stretched over both rows of
 *          every pair, so it is the whole picture at half the height; the second
 *          then sharpens the even rows
 *   half   neither is stretched: the first holds the even rows and the second the
 *          odd ones, a pixel further down, so until the second lands every other
 *          row is still the ground
 *
 * it is not an optimization, which is why it is not a level of one. it changes what
 * somebody sees while the page is loading, and that is a matter of taste before it
 * is a matter of speed.
 *
 * it does change the speed, though, and by more than anything else here. a stretched
 * layer paints every row and the sharp one then paints half of them over again, so
 * one row in three is drawn twice for nothing; two interlaced layers draw each row
 * once. measured over twelve photographs that is a third of all the drawing, with
 * the picture coming out the same to the bit.
 *
 * a 4:2:0 picture ignores it. that one travels as a single frame with no second
 * layer, so there is nothing to interlace and nothing being painted twice.
 */
inline constexpr Stages::Option zcisLazy{
    .spellings = {"zcis:lazy"}, .takes = "name",
    .help = "how much the first layer puts up: the whole picture, or every other row",
    .shape = Stages::Shape::Name, .names = Stages::namesOf<ZcisLazy>(),
    .fallback = "whole", .called = "zcis lazy",
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
 * flat ground of a cut goes first, which removes draws without moving a single pixel
 * and without asking anything of the reader that the fill command did not already
 * ask; only when that falls short does it start letting a pixel that is nearly the
 * colour of the run it sits in be drawn as it.
 *
 * what the second level of optimization finds is not spent on the number, it is
 * added to it. the mask costs no colour but it does cost bytes, and it is asked for
 * by name -- so counting it towards the number would mean that turning it on made
 * the file no faster, only gentler, which is not what someone who turned it on was
 * after. fifty at either level gives up the same colour; at the higher one it simply
 * draws less.
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

inline constexpr Stages::Option zcisEncodes[] = {zcisOptimization, zcisMinification, zcisLazy};

template <> inline constexpr Reads reads<Format::ZCIS>{.encode = zcisEncodes};

}

}
