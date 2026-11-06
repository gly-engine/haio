#pragma once

#include <haio_codec.hpp>

#include <optional>

/**
 * the hundred and twenty eight bytes a pcx starts with, read once for the three
 * files that need them. a header beside the sources rather than a fourth .cpp
 * because the build scans .cpp files for capabilities, and a helper living in one
 * would be shared by accident or copied on purpose -- the same reasoning as the
 * targa header next door.
 *
 * pcx says how deep a pixel is in two numbers that multiply: bits is the depth of
 * one plane and planes is how many there are, so sixteen colours arrive either as
 * four one bit planes (what ega hardware wanted) or as one four bit plane. the pair
 * is what names the colour, never one of them alone.
 */
namespace Haio::Codecs::Pcx {

inline constexpr size_t headerBytes = 128;
inline constexpr uint8_t magic = 0x0A;

/** the marker before the 256 colour palette glued to the end of a version 5 file */
inline constexpr uint8_t vgaMarker = 0x0C;
inline constexpr size_t vgaTrailerBytes = 1 + 256 * 3;

/** the sixteen colours in the header itself, three bytes each */
inline constexpr size_t egaOffset = 16;
inline constexpr size_t egaEntries = 16;

/** the same ceiling the netpbm and targa decoders use, for the same reason */
inline constexpr size_t maxPixels = size_t{1} << 28;

struct Header {
    uint8_t version = 0;
    uint8_t encoding = 0;
    uint8_t bits = 0;      /**< bits per pixel within one plane: 1, 2, 4 or 8 */
    uint16_t xMin = 0;
    uint16_t yMin = 0;
    uint16_t xMax = 0;
    uint16_t yMax = 0;
    uint8_t planes = 0;
    uint16_t bytesPerLine = 0;
    uint16_t paletteInfo = 0;

    int width = 0;
    int height = 0;

    /** every plane of one row, which is the unit the rle fills */
    size_t rowBytes() const { return static_cast<size_t>(bytesPerLine) * planes; }

    /** how many colours the indices can name, for the depths that index */
    size_t colors() const { return size_t{1} << (static_cast<size_t>(bits) * planes); }
};

inline uint16_t little16(Bytes data, size_t at) {
    return static_cast<uint16_t>(data[at] | (data[at + 1] << 8));
}

/**
 * reads the header and refuses anything it cannot describe, so that everything
 * downstream can trust the numbers. the checks are the file disagreeing with
 * itself: a row too short to hold the pixels it claims, a depth no pcx ever had,
 * a picture with no area.
 */
inline std::optional<Header> readHeader(Bytes data) {
    if (data.size() < headerBytes || data[0] != magic) return std::nullopt;

    Header header;
    header.version = data[1];
    header.encoding = data[2];
    header.bits = data[3];
    header.xMin = little16(data, 4);
    header.yMin = little16(data, 6);
    header.xMax = little16(data, 8);
    header.yMax = little16(data, 10);
    header.planes = data[65];
    header.bytesPerLine = little16(data, 66);
    header.paletteInfo = little16(data, 68);

    if (header.version > 5) return std::nullopt;
    if (header.encoding > 1) return std::nullopt;
    if (header.bits != 1 && header.bits != 2 && header.bits != 4 && header.bits != 8) return std::nullopt;
    if (header.planes < 1 || header.planes > 4) return std::nullopt;
    if (header.xMax < header.xMin || header.yMax < header.yMin) return std::nullopt;

    header.width = header.xMax - header.xMin + 1;
    header.height = header.yMax - header.yMin + 1;

    const auto pixels = static_cast<size_t>(header.width) * static_cast<size_t>(header.height);
    if (pixels == 0 || pixels > maxPixels) return std::nullopt;

    // a row has to reach across the picture, and the format pads it to an even byte
    const auto needed = (static_cast<size_t>(header.width) * header.bits + 7) / 8;
    if (header.bytesPerLine < needed) return std::nullopt;

    return header;
}

/**
 * which colour this file holds.
 *
 * everything that indexes is a palette, whatever depth it indexes at: the entries
 * come from the trailer for 256 colours and from the header for the smaller ones,
 * and the decoder hands back one index per pixel either way. three planes of eight
 * bits is the one pcx that carries its colour in the pixels.
 */
inline std::optional<Color> colorOf(const Header& header) {
    if (header.bits == 8 && header.planes == 3) return Color::RGB888;
    if (header.bits == 8 && header.planes == 1) return Color::PALETTE;
    if (header.bits <= 4 && header.planes <= 4) {
        return header.bits * header.planes <= 8 ? std::optional{Color::PALETTE} : std::nullopt;
    }
    return std::nullopt;
}

/** the 256 colour palette sits at the very end, behind its marker */
inline bool hasVgaTrailer(Bytes data, const Header& header) {
    if (header.bits != 8 || header.planes != 1 || header.version < 5) return false;
    if (data.size() < headerBytes + vgaTrailerBytes) return false;
    return data[data.size() - vgaTrailerBytes] == vgaMarker;
}

}
