#pragma once

#include <haio_codec.hpp>

#include <algorithm>
#include <span>

/**
 * where the three planes of a 4:2:0 picture sit.
 *
 * chroma is stored for whole two by two blocks, so a picture with an odd side has a
 * column or a row of luma with no block of its own. the plane is grown to the next
 * even number and that spare column carries a copy of its neighbour, which is the
 * layout libjpeg-turbo defines and writes into: tj3YUVBufSize(159, 1, 159, TJSAMP_420)
 * is 38400, a luma of 160x160 and two chroma of 80x80, not the 38081 a plane of
 * exactly 159x159 would be.
 *
 * that is why this is the layout haio keeps as well. the alternative is the one
 * ffmpeg and libyuv use -- luma at the true width, chroma rounded up -- and choosing
 * it would mean repacking every jpeg on the way in and on the way out, for a picture
 * that is already in the right shape when turbojpeg hands it over.
 *
 * the picture's real size never moves: an Image<YUV420> of a 159x159 jpeg still says
 * 159x159, and the padding is reachable only through here.
 */
namespace Haio::Codecs::Yuv420 {

/** one side grown to the next even number, which is the least a 4:2:0 plane can hold */
constexpr int padded(int side) { return (side + 1) & ~1; }

struct Planes {
    int lumaWidth;
    int lumaHeight;
    int chromaWidth;
    int chromaHeight;
    /** bytes in the luma plane, which is also where the first chroma plane starts */
    size_t luma;
    /** bytes in one chroma plane */
    size_t chroma;
    size_t bytes;
};

constexpr Planes planesOf(Size size) {
    const int width = padded(size.width);
    const int height = padded(size.height);
    const auto luma = static_cast<size_t>(width) * static_cast<size_t>(height);
    const auto chroma = static_cast<size_t>(width / 2) * static_cast<size_t>(height / 2);
    return Planes{width, height, width / 2, height / 2, luma, chroma, luma + chroma * 2};
}

/**
 * the spare luma column and row filled from their neighbours.
 *
 * libyuv writes the real pixels and nothing else, and the chroma planes come out
 * whole -- their width is already the rounded up half. only luma is left with a gap,
 * and leaving it zeroed would put a black column inside the edge block, which comes
 * back as a dark fringe once the chroma is upsampled. repeating the edge is what
 * libjpeg does when it builds the same block itself.
 */
inline void repeatEdges(std::span<uint8_t> dst, Size size, const Planes& planes) {
    const auto stride = static_cast<size_t>(planes.lumaWidth);

    if (planes.lumaWidth > size.width) {
        for (int y = 0; y < size.height; y++) {
            const auto row = static_cast<size_t>(y) * stride;
            dst[row + static_cast<size_t>(size.width)] = dst[row + static_cast<size_t>(size.width) - 1];
        }
    }

    // after the column, so the row it copies already carries its own spare pixel
    if (planes.lumaHeight > size.height) {
        const auto last = static_cast<size_t>(size.height - 1) * stride;
        std::copy_n(dst.data() + last, stride, dst.data() + last + stride);
    }
}

}
