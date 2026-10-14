#include <haio/util/tga.hpp>

#include <haio/generated/codecs/classes.hpp>
#include <haio/implementation/codecs.hpp>

namespace {

/** the header has to parse and then has to be describing the colour being asked about */
bool holds(Haio::Implementation::Bytes data, Haio::Color wanted) {
    const auto header = Haio::Util::getTgaHeader(data);
    if (!header) return false;
    const auto colour = Haio::Util::getTgaColor(*header);
    return colour && *colour == wanted;
}

}

namespace Haio::Implementation::WIP {

template <> struct DefaultColor<Format::TGA> { static constexpr Color value = Color::BGRA8888; };

}

namespace Haio::Implementation::Codecs {

/**
 * a truecolor tga with an alpha, which is what a tga is when nobody says which kind.
 * it is also the only one of the six that keeps everything a picture arrives with,
 * so it is what haio writes when the command line names the container and not the
 * colour inside it.
 */
/** thirty two bits a pixel, blue first, with the last byte an alpha */
bool Detect<Format::TGA, Color::BGRA8888>::operator()(Bytes data) const {
    return holds(data, Color::BGRA8888);
}

/** twenty four bits a pixel, blue first */
bool Detect<Format::TGA, Color::BGR888>::operator()(Bytes data) const {
    return holds(data, Color::BGR888);
}

/**
 * sixteen bits a pixel whose top bit the file says is an alpha. the bytes are the
 * same as the pair below; the attribute bits in the descriptor are the difference,
 * and they are a claim the writer made rather than anything visible in the pixels.
 */
bool Detect<Format::TGA, Color::RGBA5551>::operator()(Bytes data) const {
    return holds(data, Color::RGBA5551);
}

/** the same sixteen bits with nobody claiming the top one */
bool Detect<Format::TGA, Color::RGB555>::operator()(Bytes data) const {
    return holds(data, Color::RGB555);
}

/** an index a pixel and the colours it points at, which is the format's own palette */
bool Detect<Format::TGA, Color::PALETTE>::operator()(Bytes data) const {
    return holds(data, Color::PALETTE);
}

/** one byte a pixel and no colour at all */
bool Detect<Format::TGA, Color::GRAY8>::operator()(Bytes data) const {
    return holds(data, Color::GRAY8);
}
}
