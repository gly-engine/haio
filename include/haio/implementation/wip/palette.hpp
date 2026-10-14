#pragma once

#include "haio/implementation/error.hpp"
#include "haio/implementation/image.hpp"
#include "haio/implementation/wip/dither.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include <span>
#include <string_view>

namespace Haio::Implementation::WIP {

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

/** the spelling used on the command line, or nothing when it is not one of them */
std::optional<WIP::Dither> ditherNamed(std::string_view name);

/**
 * fits a true colour picture into a palette.
 *
 * the palette is given rather than derived: haio does not pick colours for anybody,
 * it puts a picture into the colours it was handed.
 */
Result<Image<Color::PALETTE>> toPalette(const Image<Color::RGBA8888>& image,
                                        std::vector<uint32_t> entries, WIP::Dither how);

/** "sort" or "spread", or nothing when it is neither */
std::optional<WIP::Limit> limitNamed(std::string_view name);

Result<std::vector<uint32_t>> limitPalette(const Image<Color::RGBA8888>& image,
                                           std::vector<uint32_t> entries, size_t most, WIP::Limit how);

}
