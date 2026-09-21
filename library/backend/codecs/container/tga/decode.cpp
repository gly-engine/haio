#include "header.hpp"

#include <haio_codec.hpp>
#include <haio_codecs.hpp>
#include <haio_convert.hpp>
#include <haio_util.hpp>

#include <algorithm>

namespace {

using namespace Haio;
using Haio::Codecs::Tga::Header;

/** a five bit channel widens by repeating its high bits, so 31 reaches 255 exactly */
constexpr uint8_t expand5(unsigned value) { return static_cast<uint8_t>((value << 3) | (value >> 2)); }

/**
 * the pixels in file order, with any runs written out.
 *
 * a run length packet is a count and then either one unit to repeat or that many to
 * copy, and the spec says a packet stops at the end of a scanline while half the
 * encoders in the world carry on across it. reading the stream as one long sequence
 * is what both kinds of file have in common.
 */
Result<std::vector<uint8_t>> unpack(Bytes data, const Header& header) {
    const auto unit = header.unit();
    std::vector<uint8_t> out(header.pixels() * unit);

    if (!header.rle()) {
        // readHeader already measured this one, so the copy cannot run off the end
        std::copy_n(data.begin() + static_cast<ptrdiff_t>(header.dataOffset), out.size(), out.begin());
        return out;
    }

    size_t at = header.dataOffset;
    size_t to = 0;
    while (to < out.size()) {
        if (at >= data.size()) {
            return std::unexpected(Error{ErrorCode::InvalidInput, "this tga ends before its picture does"});
        }

        const uint8_t packet = data[at++];
        const size_t run = (packet & 0x7fu) + 1u;
        const size_t bytes = run * unit;
        if (to + bytes > out.size()) {
            return std::unexpected(Error{ErrorCode::InvalidInput, "a run in this tga carries past the end of the picture"});
        }

        if ((packet & 0x80u) != 0) {
            if (!Util::hasBytes(data, at, unit)) {
                return std::unexpected(Error{ErrorCode::InvalidInput, "this tga ends in the middle of a run"});
            }
            for (size_t k = 0; k < run; k++, to += unit) {
                std::copy_n(data.begin() + static_cast<ptrdiff_t>(at), unit, out.begin() + static_cast<ptrdiff_t>(to));
            }
            at += unit;
        } else {
            if (!Util::hasBytes(data, at, bytes)) {
                return std::unexpected(Error{ErrorCode::InvalidInput, "this tga ends in the middle of a packet"});
            }
            std::copy_n(data.begin() + static_cast<ptrdiff_t>(at), bytes, out.begin() + static_cast<ptrdiff_t>(to));
            at += bytes;
            to += bytes;
        }
    }
    return out;
}

/**
 * the origin bits, honoured rather than passed on.
 *
 * a tga names which corner it started from, and the default is the bottom left
 * because the format was written for a frame buffer that was addressed that way.
 * everything downstream of a decoder here reads top left first, so this is where the
 * two agree.
 */
void orient(std::vector<uint8_t>& pixels, const Header& header) {
    const auto unit = header.unit();
    const auto width = static_cast<size_t>(header.width);
    const auto height = static_cast<size_t>(header.height);
    const auto row = width * unit;

    if (header.flipX()) {
        for (size_t y = 0; y < height; y++) {
            auto* line = pixels.data() + y * row;
            for (size_t x = 0; x < width / 2; x++) {
                std::swap_ranges(line + x * unit, line + (x + 1) * unit, line + (width - 1 - x) * unit);
            }
        }
    }

    if (header.flipY()) {
        for (size_t y = 0; y < height / 2; y++) {
            std::swap_ranges(pixels.data() + y * row, pixels.data() + (y + 1) * row,
                             pixels.data() + (height - 1 - y) * row);
        }
    }
}

/**
 * a sixteen bit tga pixel is A1R5G5B5 and haio's rgba5551 is the same five bit
 * channels with the alpha at the other end, so the whole conversion is a rotate: one
 * bit left, the alpha falling off the top and landing at the bottom.
 */
void rotateToRgba5551(std::vector<uint8_t>& pixels) {
    for (size_t at = 0; at + 1 < pixels.size(); at += 2) {
        const auto packed = static_cast<uint16_t>(pixels[at] | (pixels[at + 1] << 8));
        const auto rotated = static_cast<uint16_t>((packed << 1) | (packed >> 15));
        pixels[at] = static_cast<uint8_t>(rotated);
        pixels[at + 1] = static_cast<uint8_t>(rotated >> 8);
    }
}

/**
 * the colour map, as the colours an index reaches rather than as the bytes it was
 * stored in.
 *
 * the first entry field says the map starts part way up, so the entries before it are
 * filled in as black: an index is an index, and a picture that points below the map
 * is pointing at something the file never gave it.
 */
Result<std::vector<uint32_t>> readMap(Bytes data, const Header& header) {
    const auto entries = static_cast<size_t>(header.mapFirst) + static_cast<size_t>(header.mapCount);
    if (entries > 256) {
        return std::unexpected(Error{ErrorCode::InvalidInput,
                                     "this tga names colour " + std::to_string(entries - 1)
                                         + " and an index of one byte reaches 255"});
    }

    const auto width = (static_cast<size_t>(header.mapBits) + 7) / 8;
    if (!Util::hasBytes(data, header.mapOffset, static_cast<size_t>(header.mapCount) * width)) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "this tga ends inside its colour map"});
    }

    std::vector<uint32_t> out(entries, 0xff000000u);
    for (size_t i = 0; i < header.mapCount; i++) {
        const auto at = header.mapOffset + i * width;
        uint32_t colour = 0xff000000u;
        if (width == 2) {
            const auto packed = static_cast<unsigned>(data[at] | (data[at + 1] << 8));
            colour |= static_cast<uint32_t>(expand5((packed >> 10) & 0x1f)) << 16;
            colour |= static_cast<uint32_t>(expand5((packed >> 5) & 0x1f)) << 8;
            colour |= expand5(packed & 0x1f);
        } else {
            colour |= static_cast<uint32_t>(data[at + 2]) << 16;
            colour |= static_cast<uint32_t>(data[at + 1]) << 8;
            colour |= data[at + 0];
            // the fourth byte is an alpha only where the descriptor says somebody
            // meant it to be; a map written by a tool that zeroed the field would
            // otherwise come back as a palette of invisible colours
            if (width == 4 && header.attributeBits() != 0) {
                colour = (colour & 0x00ffffffu) | (static_cast<uint32_t>(data[at + 3]) << 24);
            }
        }
        out[static_cast<size_t>(header.mapFirst) + i] = colour;
    }
    return out;
}

/**
 * one body for every colour the container can hold; the caller names which.
 *
 * the colours were chosen so that this is a copy: a tga already stores bgr888,
 * bgra8888, grey and indices exactly as haio keeps them, so unpacking runs and
 * turning the picture the right way up is the entire decoder.
 */
template <Color P>
Result<Image<P>> decodeTga(const Blob& blob) {
    HAIO_TRY(header, Codecs::Tga::readHeader(blob.data));

    const auto colour = Codecs::Tga::colorOf(header);
    if (!colour || *colour != P) {
        // only reachable when a command line named the pair itself, since detection
        // hands every file to the decoder its own header asked for
        static constexpr const char* kinds[] = {"", "mapped", "truecolor", "grey"};
        HAIO_FAIL(InvalidInput, std::string("this tga is ") + kinds[header.kind()] + " at "
                                    + std::to_string(header.depth) + " bits a pixel, which is not the colour it was asked to be");
    }
    if constexpr (P == Color::PALETTE) {
        if (header.depth != 8) {
            HAIO_FAIL(UnsupportedFormat, "this tga indexes its colours two bytes at a time, and a palette picture here is one index per pixel");
        }
    }

    HAIO_TRY(pixels, unpack(blob.data, header));
    orient(pixels, header);

    if constexpr (P == Color::RGBA5551) rotateToRgba5551(pixels);

    Image<P> out{header.width, header.height, std::move(pixels)};
    if constexpr (P == Color::PALETTE) {
        HAIO_TRY(entries, readMap(blob.data, header));
        out.entries = std::move(entries);
    }
    return out;
}

}

namespace Haio::Codecs {

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::BGRA8888>> Decode<Format::TGA, Color::BGRA8888>(const Blob& blob) {
    return decodeTga<Color::BGRA8888>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::BGR888>> Decode<Format::TGA, Color::BGR888>(const Blob& blob) {
    return decodeTga<Color::BGR888>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGBA5551>> Decode<Format::TGA, Color::RGBA5551>(const Blob& blob) {
    return decodeTga<Color::RGBA5551>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::RGB555>> Decode<Format::TGA, Color::RGB555>(const Blob& blob) {
    return decodeTga<Color::RGB555>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
/** the indices and the colours they point at, which is what a palette image is */
template <>
Result<Image<Color::PALETTE>> Decode<Format::TGA, Color::PALETTE>(const Blob& blob) {
    return decodeTga<Color::PALETTE>(blob);
}
/** @} */

/**
 * @addtogroup decode
 * @{
 */
template <>
Result<Image<Color::GRAY8>> Decode<Format::TGA, Color::GRAY8>(const Blob& blob) {
    return decodeTga<Color::GRAY8>(blob);
}
/** @} */

}

// touched

// touched twice
