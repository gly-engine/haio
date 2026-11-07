#include <haio/internal/codecs/pcx.hpp>

#include <haio_codec.hpp>

namespace {

/** the header has to parse and then has to be describing the colour being asked about */
bool holds(Haio::Bytes data, Haio::Color wanted) {
    const auto header = Haio::Codecs::Pcx::readHeader(data);
    if (!header) return false;
    const auto colour = Haio::Codecs::Pcx::colorOf(*header);
    return colour && *colour == wanted;
}

}

namespace Haio::Codecs {

/**
 * a pcx with a palette, which is what a pcx is when nobody says which kind: the
 * format was made for indexed pictures and spent most of its life holding them.
 */
template <> struct DefaultColor<Format::PCX> { static constexpr Color value = Color::PALETTE; };

/**
 * @addtogroup detect
 * @{
 */
/**
 * anything that indexes: 256 colours in one eight bit plane, sixteen across four
 * one bit planes, and the smaller packings in between. they all come back as one
 * index a pixel, so they all answer here.
 */
template <>
bool Detect<Format::PCX, Color::PALETTE>(Bytes data) {
    return holds(data, Color::PALETTE);
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
/** three planes of eight bits: red, green and blue, a plane at a time */
template <>
bool Detect<Format::PCX, Color::RGB888>(Bytes data) {
    return holds(data, Color::RGB888);
}
/** @} */

}
