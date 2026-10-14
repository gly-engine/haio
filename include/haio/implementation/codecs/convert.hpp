#pragma once

#include "haio/implementation/codecs.hpp"

namespace Haio::Implementation {

/**
 * how many bytes one image of this colour and size takes. block formats round up to
 * whole blocks, so this is not always width times height times something.
 */
Result<size_t> sizeOf(Color color, Size size);

/**
 * the shape every Convert specialisation has: check, allocate, hand the work to Move.
 * it lives here so a new colour pair is one Move and one line instead of ten.
 */
template <Color From, Color To>
    requires Codecs::Movable<From, To>
Result<Image<To>> convertVia(Image<From> src) {
    const Size size{src.width, src.height};
    HAIO_TRY(want, sizeOf(From, size));
    if (src.data.size() != want) {
        HAIO_FAIL(InvalidInput, "image data does not match its own size");
    }

    HAIO_TRY(bytes, sizeOf(To, size));
    Image<To> out{src.width, src.height, std::vector<uint8_t>(bytes)};
    if (auto moved = Codecs::Move<From, To>{}(src.data, out.data, size); !moved) {
        return std::unexpected(moved.error());
    }
    return out;
}

}
