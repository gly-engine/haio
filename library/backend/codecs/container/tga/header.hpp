#pragma once

#include <haio_codec.hpp>

#include <optional>
#include <string_view>

/**
 * the eighteen bytes every tga starts with, read once for the three files that need
 * them. it is a header beside the sources rather than a third .cpp because nothing
 * outside this directory has any use for a targa header, and the build scans .cpp
 * files for capabilities: a helper that lived in one of them would be shared by
 * accident or copied on purpose.
 */
namespace Haio::Codecs::Tga {

inline constexpr size_t headerBytes = 18;

/**
 * an image this large would not fit anyway, and a header claims it in four bytes.
 *
 * the same number the netpbm decoder uses, for the same reason: the dimensions are
 * read before there is any picture to measure them against, and a run length payload
 * is not bounded by the size of the file the way a plain one is.
 */
inline constexpr size_t maxPixels = size_t{1} << 28;

/**
 * what a tga 2.0 writer leaves at the end of the file: two offsets nobody here uses
 * and a signature that is the only thing about the format anybody can recognise
 * outright. haio writes one so its own files are named by that rather than by the
 * guesswork below.
 */
inline constexpr std::string_view footerSignature = "TRUEVISION-XFILE.";
inline constexpr size_t footerBytes = 26;

struct Header {
    uint8_t idLength = 0;
    uint8_t mapType = 0;
    uint8_t imageType = 0;
    uint16_t mapFirst = 0;
    uint16_t mapCount = 0;
    uint8_t mapBits = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t depth = 0;
    uint8_t descriptor = 0;

    size_t mapOffset = 0;    /**< the colour map, when there is one */
    size_t dataOffset = 0;   /**< the pixels, whatever came before them */

    /** mapped, truecolor or grey, with the run length flavours folded onto their own */
    int kind() const { return imageType > 8 ? imageType - 8 : imageType; }
    bool rle() const { return imageType > 8; }

    /** one stored unit: a colour for a truecolor picture, an index for a mapped one */
    size_t unit() const { return (static_cast<size_t>(depth) + 7) / 8; }
    size_t pixels() const { return static_cast<size_t>(width) * static_cast<size_t>(height); }

    /**
     * how many of the sixteen bits are alpha, which for this format is a claim rather
     * than a layout: the bit is in the same place either way, and this says whether
     * anybody meant anything by it.
     */
    int attributeBits() const { return descriptor & 0x0f; }

    /** the origin bits, which say where the first stored pixel belongs on screen */
    bool flipX() const { return (descriptor & 0x10) != 0; }
    bool flipY() const { return (descriptor & 0x20) == 0; }
};

/** the signature sits eight bytes into the footer, after the two offsets */
inline bool hasFooter(Bytes data) {
    if (data.size() < footerBytes) return false;
    const auto at = data.size() - footerBytes + 8;
    for (size_t i = 0; i < footerSignature.size(); i++) {
        if (data[at + i] != static_cast<uint8_t>(footerSignature[i])) return false;
    }
    return true;
}

/**
 * the header, made to prove itself.
 *
 * every other container haio reads begins with bytes that mean nothing else. a tga
 * begins with its own dimensions, so there is nothing to compare against and the only
 * question that can be asked is whether the eighteen bytes are consistent: a type it
 * names, a depth that type can have, a colour map exactly when one is called for, and
 * a file long enough to hold what all of that promises. anything less strict would
 * make tga the answer for every unrecognised file.
 */
inline Result<Header> readHeader(Bytes data) {
    if (data.size() < headerBytes) {
        return std::unexpected(Error{ErrorCode::InvalidInput, "this file is too short to be a tga"});
    }

    const auto u16 = [data](size_t at) {
        return static_cast<uint16_t>(data[at] | (data[at + 1] << 8));
    };

    Header header;
    header.idLength = data[0];
    header.mapType = data[1];
    header.imageType = data[2];
    header.mapFirst = u16(3);
    header.mapCount = u16(5);
    header.mapBits = data[7];
    // 8 to 11 are where the picture sits on a screen, which is not part of the picture
    header.width = u16(12);
    header.height = u16(14);
    header.depth = data[16];
    header.descriptor = data[17];

    const auto fail = [](const char* why) {
        return std::unexpected(Error{ErrorCode::InvalidInput, why});
    };

    if (header.mapType > 1) return fail("this tga names a colour map type nobody defined");
    if (header.kind() < 1 || header.kind() > 3) return fail("this tga names an image type nobody defined");
    // the top two descriptor bits were interleaving in the first version and have to
    // be zero in this one, which makes them a cheap way to tell noise from a header
    if ((header.descriptor & 0xc0) != 0) return fail("this tga sets descriptor bits that 2.0 reserved");
    if (header.width == 0 || header.height == 0) return fail("this tga has no area");

    if (header.kind() == 1) {
        if (header.mapType != 1 || header.mapCount == 0) return fail("this tga indexes a colour map it does not carry");
        if (header.mapBits != 15 && header.mapBits != 16 && header.mapBits != 24 && header.mapBits != 32) {
            return fail("this tga names a colour map entry size nobody defined");
        }
        if (header.depth != 8 && header.depth != 16) return fail("a mapped tga indexes one or two bytes at a time");
    } else if (header.mapType == 0 && (header.mapFirst != 0 || header.mapCount != 0 || header.mapBits != 0)) {
        return fail("this tga describes a colour map and then says it has none");
    }

    if (header.kind() == 2 && header.depth != 15 && header.depth != 16 && header.depth != 24 && header.depth != 32) {
        return fail("this tga names a truecolor depth nobody defined");
    }
    if (header.kind() == 3 && header.depth != 8) return fail("a grey tga is one byte a pixel");

    header.mapOffset = headerBytes + static_cast<size_t>(header.idLength);
    header.dataOffset = header.mapOffset;
    if (header.mapType == 1) {
        header.dataOffset += static_cast<size_t>(header.mapCount) * ((static_cast<size_t>(header.mapBits) + 7) / 8);
    }

    if (header.pixels() > maxPixels) return fail("this tga is larger than haio will decode");

    if (header.dataOffset >= data.size()) return fail("this tga header promises pixels the file does not reach");

    const auto available = data.size() - header.dataOffset;
    if (!header.rle()) {
        if (available < header.pixels() * header.unit()) {
            return fail("this tga holds fewer pixels than its header claims");
        }
    } else if (header.pixels() / 128 > available / (1 + header.unit())) {
        /**
         * a compressed picture is as long as it turns out to be, so it cannot be
         * measured the way a plain one can. what can be said is the most it could
         * possibly be: a packet is a count byte and at least one unit, and it covers
         * at most 128 pixels. a file that does not reach even that is either truncated
         * or claiming a size it means to be expanded into.
         */
        return fail("this tga claims more picture than its runs could possibly hold");
    }

    return header;
}

/**
 * which of haio's colours these bytes already are.
 *
 * a tga stores what haio calls bgr888, bgra8888 and the two fives-and-ones outright,
 * so the answer is a layout rather than a conversion: that is the whole reason those
 * colours exist, and why decoding one is a copy and not a loop.
 */
inline std::optional<Color> colorOf(const Header& header) {
    switch (header.kind()) {
        case 1: return Color::PALETTE;
        case 3: return Color::GRAY8;
        case 2:
            if (header.depth == 24) return Color::BGR888;
            if (header.depth == 32) return Color::BGRA8888;
            // the same two bytes either way: the attribute bits are what decides
            // whether the top one is an alpha or nothing at all
            return header.attributeBits() != 0 ? Color::RGBA5551 : Color::RGB555;
        default: break;
    }
    return std::nullopt;
}

}
