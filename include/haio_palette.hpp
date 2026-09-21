#pragma once

#include "haio_codec.hpp"

#include <span>
#include <string_view>

namespace Haio {

/**
 * the palettes haio knows by name, and the way to ask for part of one.
 *
 * a name on its own is the whole palette, and a colon takes a slice of one:
 *
 *   cga                every colour it has
 *   cga:2              the second bank, where a bank is as wide as the picture needs
 *   cga:1..3           those colours, counted from one
 *   cga:1,4,7          the same, spelled out
 *   cga:1..3,7         and the two together
 *
 * a colon rather than brackets so that a shell leaves it alone: "<" and ">" are
 * redirections, and a palette nobody can type without quoting is a palette nobody
 * types.
 *
 * the bank form is why the picture has to be known before the palette is: a two bit
 * image wants four colours, so its second bank is 5 to 8, while a three colour one
 * has 4 to 6 there. saying it as a bank rather than a list is what lets the same
 * spelling follow a picture that changes depth.
 */
Result<std::vector<uint32_t>> paletteNamed(std::string_view spec, size_t wanted);

/** every name paletteNamed answers to, for the help text and the error message */
std::span<const std::string_view> paletteNames();

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

/** the spelling used on the command line, or nothing when it is not one of them */
std::optional<Dither> ditherNamed(std::string_view name);

/**
 * fits a true colour picture into a palette.
 *
 * the palette is given rather than derived: haio does not pick colours for anybody,
 * it puts a picture into the colours it was handed.
 */
Result<Image<Color::PALETTE>> toPalette(const Image<Color::RGBA8888>& image,
                                        std::vector<uint32_t> entries, Dither how);

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

/** "sort" or "spread", or nothing when it is neither */
std::optional<Limit> limitNamed(std::string_view name);

Result<std::vector<uint32_t>> limitPalette(const Image<Color::RGBA8888>& image,
                                           std::vector<uint32_t> entries, size_t most, Limit how);

}
