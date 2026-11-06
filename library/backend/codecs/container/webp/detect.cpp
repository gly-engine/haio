#include <haio_codec.hpp>
#include <haio_util.hpp>

namespace {

constexpr size_t chunkTag = 12;
constexpr size_t chunkBody = 20;

enum class Holds { Nothing, Rgba, Yuv };

/** lossy vp8 is yuv 4:2:0 like a jpeg; everything else is argb inside */
Holds holdsOf(Haio::Bytes data) {
    using namespace Haio::Util;

    if (!matches(data, "RIFF") || !hasBytes(data, chunkTag, 4) || !matches(data.subspan(8, 4), "WEBP")) {
        return Holds::Nothing;
    }

    const auto chunk = data.subspan(chunkTag, 4);
    if (matches(chunk, "VP8X")) return hasBytes(data, chunkBody, 10) ? Holds::Rgba : Holds::Nothing;
    if (matches(chunk, "VP8L")) return hasBytes(data, chunkBody, 5) ? Holds::Rgba : Holds::Nothing;
    if (!matches(chunk, "VP8 ")) return Holds::Nothing;

    // the keyframe header: three bytes of frame tag, the sync code, then fourteen
    // bits of width and fourteen of height with the scale above them
    constexpr size_t syncCode = chunkBody + 3;
    constexpr size_t dimensions = chunkBody + 6;
    if (!hasBytes(data, dimensions, 4)) return Holds::Nothing;
    if (data[syncCode] != 0x9d || data[syncCode + 1] != 0x01 || data[syncCode + 2] != 0x2a) {
        return Holds::Nothing;
    }

    const int width = (data[dimensions] | (data[dimensions + 1] << 8)) & 0x3fff;
    const int height = (data[dimensions + 2] | (data[dimensions + 3] << 8)) & 0x3fff;

    // an odd side comes out rgba: the planes would hold it now, but libwebp fills only
    // the real columns and rgba is the lossless answer rather than a padded one
    return (width % 2 == 0 && height % 2 == 0) ? Holds::Yuv : Holds::Rgba;
}

}

namespace Haio::Codecs {

/** rgba and not the yuv a lossy one stores: a webp can keep an alpha, and yuv cannot */
template <> struct DefaultColor<Format::WEBP> { static constexpr Color value = Color::RGBA8888; };

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::WEBP, Color::RGBA8888>(Bytes data) {
    return holdsOf(data) == Holds::Rgba;
}
/** @} */

/**
 * @addtogroup detect
 * @{
 */
template <>
bool Detect<Format::WEBP, Color::YUV420>(Bytes data) {
    return holdsOf(data) == Holds::Yuv;
}
/** @} */

}
