#pragma once

/**
 * the names the palette's options take, which is still being worked out
 */
namespace Haio {

namespace Implementation::WIP {

/**
 * how a colour that is not in the palette becomes one that is.
 *
 * there is no default on purpose. every one of these changes the picture in a way
 * somebody has to want: nearest flattens a gradient into bands, the dithers trade
 * that for a pattern that is wrong at every single pixel, and strict refuses a
 * picture that was not already in the palette. choosing for the caller would mean
 * choosing what their art looks like.
 */
enum class Dither {
    Nearest,   /**< the closest colour there is */
    Bayer,     /**< ordered, a fixed 4x4 threshold: no state, same answer every time */
    Floyd,     /**< error diffusion: best on photographs, and serial by nature */
    Strict,    /**< every colour must already be in the palette, or it is an error */
};

/**
 * which colours to keep when a palette has more than was asked for.
 *
 * taking the first N is never right: a master palette is ordered by how the hardware
 * addresses it, so the first sixteen of the nes are sixteen dark blues. counting
 * which colours a picture would use is better, but counting alone has a bias of its
 * own, and the two answers differ enough to be worth choosing between.
 */
enum class Limit {
    /**
     * the most used, plainly.
     *
     * right when the picture is the subject: a sprite sheet, a logo, anything where
     * area and importance are the same thing.
     */
    Sort,

    /**
     * the most used, less whatever a chosen colour already covers.
     *
     * a photograph is mostly background, so counting alone spends the whole budget on
     * eighteen shades of sky and leaves the subject sharing one brown. this keeps the
     * first colour by count and then, each time, takes the one whose count times its
     * distance from everything already kept is largest. a colour that is rare but
     * unlike anything chosen beats a common one that is nearly a duplicate.
     */
    Spread,
};

}

}
