#include "header.hpp"

#include <haio_codec.hpp>

namespace {

/** the header has to parse and then has to be describing the colour being asked about */
bool holds(Haio::Bytes data, Haio::Color wanted) {
    const auto header = Haio::Codecs::Tga::readHeader(data);
    if (!header) return false;
    const auto colour = Haio::Codecs::Tga::colorOf(*header);
    return colour && *colour == wanted;
}

}

namespace Haio::Codecs {

/**
 * a truecolor tga with an alpha, which is what a tga is when nobody says which kind.
 * it is also the only one of the six that keeps everything a picture arrives with,
 * so it is what haio writes when the command line names the container and not the
 * colour inside it.
 */
template <> struct DefaultColor<Format::TGA> { static constexpr Color value = Color::BGRA8888; };

/**
 * @addtogroup detect
 * @{
 */
/** thirty two bits a pixel, blue first, with the last byte an alpha */
template <>
bool Detect<Format::TGA, Color::BGRA8888>(Bytes data) {
    return holds(data, Color::BGRA8888);
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
/** twenty four bits a pixel, blue first */
template <>
bool Detect<Format::TGA, Color::BGR888>(Bytes data) {
    return holds(data, Color::BGR888);
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
/**
 * sixteen bits a pixel whose top bit the file says is an alpha. the bytes are the
 * same as the pair below; the attribute bits in the descriptor are the difference,
 * and they are a claim the writer made rather than anything visible in the pixels.
 */
template <>
bool Detect<Format::TGA, Color::RGBA5551>(Bytes data) {
    return holds(data, Color::RGBA5551);
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
/** the same sixteen bits with nobody claiming the top one */
template <>
bool Detect<Format::TGA, Color::RGB555>(Bytes data) {
    return holds(data, Color::RGB555);
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
/** an index a pixel and the colours it points at, which is the format's own palette */
template <>
bool Detect<Format::TGA, Color::PALETTE>(Bytes data) {
    return holds(data, Color::PALETTE);
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
/** one byte a pixel and no colour at all */
template <>
bool Detect<Format::TGA, Color::GRAY8>(Bytes data) {
    return holds(data, Color::GRAY8);
}
/** @} */

}
