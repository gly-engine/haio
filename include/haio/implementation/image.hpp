#pragma once

#include "haio/domain.hpp"
#include "haio/implementation/geometry.hpp"
#include "haio/util/color.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Haio::Implementation {

using Bytes = std::span<const uint8_t>;

/** a setting a codec reads, by the name it declared, with the value still as written */
struct Setting {
    std::string name;
    std::string value;
};

using Settings = std::vector<Setting>;

/** pixels in memory. it has a colour and no container, because it is not a file yet */
template <Color P>
struct Image {
    static constexpr Color color = P;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;
};

/** a transform can index these; having no stride is exactly what rules the others out */
template <Color P> concept Addressable = Util::strideOf(P) != 0;

/** and these can have a corner rounded away, because there is an alpha to clear */
template <Color P> concept Maskable = Addressable<P> && Util::alphaOffsetOf(P) >= 0;

/**
 * the one colour that is not self describing: a byte per pixel means nothing without
 * the colours it points at, so the palette travels with the picture.
 *
 * it is a specialisation rather than a member on every Image, because every other
 * colour would carry an empty vector around for nothing. the cost is that Move, which
 * moves spans of bytes, has no way to carry the entries: palette conversions go
 * through Convert directly and never through convertVia.
 */
template <>
struct Image<Color::PALETTE> {
    static constexpr Color color = Color::PALETTE;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;

    /** 0xAARRGGBB, at most 256 of them, and an index past the end is an error */
    std::vector<uint32_t> entries;
};

/** bytes of a file. it has a container and the colour it was found to hold */
struct Blob {
    Format format = Format::RAW;
    Color color = Color::RGBA8888;
    std::string contentType = "application/octet-stream";
    std::string path;
    std::vector<uint8_t> data;

    /** RAW only: how big the pixels are, since bare pixels have nowhere to say */
    Size size;
};

}
