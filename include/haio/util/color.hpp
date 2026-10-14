#pragma once

#include "haio/domain.hpp"

#include <cstddef>

/** what a colour's layout says about one pixel of it */
namespace Haio::Util {

/**
 * how wide one pixel is, or zero when the colour has no such number: etc1 packs four
 * by four blocks and yuv420 splits into planes, so neither can be measured a pixel at
 * a time. sizeOf reads this too, so the widths are written down once.
 */
constexpr size_t strideOf(Color color) {
    switch (color) {
        case Color::RGBA8888: return 4;
        case Color::RGB888:   return 3;
        case Color::RGB565:   return 2;
        case Color::RGB555:   return 2;
        case Color::RGBA5551: return 2;
        case Color::BGR888:   return 3;
        case Color::BGRA8888: return 4;
        case Color::GRAY8:    return 1;
        case Color::GRAYALPHA88: return 2;
        // one byte per pixel, so a crop or a resize can index it like any other
        case Color::PALETTE:  return 1;
        // tiled and planar, so no pixel has an address of its own
        case Color::CHR_NES:  break;
        case Color::ETC1:     break;
        case Color::YUV420:   break;
        // no pixels at all
        case Color::NIL:      break;
    }
    return 0;
}

/**
 * which byte of a pixel holds its alpha, or a negative when the colour has none.
 * the two eight bit colours answer the same, because where the alpha sits is all a
 * masking loop ever wanted to know and neither of them moved it. written as a rule so
 * that the day another arrives, rgba4444 in a ktx or a palette that keeps a
 * transparent entry, it is one line here rather than a second copy of the loop.
 */
constexpr int alphaOffsetOf(Color color) {
    switch (color) {
        case Color::RGBA8888: return 3;
        case Color::BGRA8888: return 3;
        case Color::RGB888:   break;
        case Color::RGB565:   break;
        case Color::BGR888:   break;
        case Color::RGB555:   break;
        // it has an alpha, but it is one bit sharing a byte with the blue, so there
        // is nothing here to clear on its own
        case Color::RGBA5551: break;
        case Color::GRAY8:    break;
        case Color::GRAYALPHA88: return 1;
        // tiled and planar, so no pixel has an address of its own
        case Color::CHR_NES:  break;
        case Color::ETC1:     break;
        case Color::YUV420:   break;
        case Color::NIL:      break;
    }
    return -1;
}

}
