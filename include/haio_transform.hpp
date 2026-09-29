#pragma once

#include "haio_codec.hpp"

/**
 * geometry on addressable pixels. the colour no longer has to be rgba8888: the only
 * thing these ever needed from it was how wide a pixel is, which PixelStride answers,
 * so a crop on rgb565 stops going through a conversion it never used.
 *
 * the transforms themselves are in include/haio/transforms/, one header each, and the
 * colours each one runs on are the sources in `library/backend/transforms/NAME/`.
 */
namespace Haio {

/**
 * a transformed picture keeps everything about the original except its pixels.
 *
 * building a fresh Image would drop whatever else the colour carries, which for a
 * palette is the palette: a cropped picture would come back as indices into nothing.
 */
template <Color P>
Image<P> withPixels(const Image<P>& from, int width, int height, std::vector<uint8_t> pixels) {
    Image<P> out = from;
    out.width = width;
    out.height = height;
    out.data = std::move(pixels);
    return out;
}

}
